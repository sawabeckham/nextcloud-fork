/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "auxiliarydomainreconciler.h"

#include <QPointer>
#include <QThread>
#include <algorithm>
#include <utility>

namespace OCC::Mac
{
using namespace Qt::StringLiterals;

AuxiliaryDomainReconciler::AuxiliaryDomainReconciler(QString auxiliaryIdentifier,
                                                     Query query,
                                                     MappedDomainIdentifiers mappedDomainIdentifiers,
                                                     Mutation ensureHidden,
                                                     Mutation removeAuxiliary,
                                                     QObject *parent,
                                                     int timeoutMs,
                                                     int retryDelayMs,
                                                     int maxRetries)
    : QObject(parent)
    , _auxiliaryIdentifier(std::move(auxiliaryIdentifier))
    , _query(std::move(query))
    , _mappedDomainIdentifiers(std::move(mappedDomainIdentifiers))
    , _ensureHidden(std::move(ensureHidden))
    , _removeAuxiliary(std::move(removeAuxiliary))
    , _timeoutMs(qMax(1, timeoutMs))
    , _retryDelayMs(qMax(0, retryDelayMs))
    , _maxRetries(qMax(0, maxRetries))
    , _dispatcher(new QObject, [](QObject *context) {
        context->deleteLater();
    })
{
    _timer.setSingleShot(true);
}

void AuxiliaryDomainReconciler::requestReconciliation()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++_generation;
    if (_active) {
        return;
    }
    _active = true;
    _retries = 0;
    queryDomains();
}

void AuxiliaryDomainReconciler::armTimeout()
{
    _timer.disconnect(this);
    connect(&_timer, &QTimer::timeout, this, [this] {
        if (_mutationInFlight) {
            _mutationTimedOut = true;
            Q_EMIT finished(false,
                            u"Auxiliary domain mutation timed out; its outcome is unknown. Further mutations are blocked until its completion arrives."_s);
            return;
        }
        failed(u"Auxiliary domain operation timed out."_s);
    });
    _timer.start(_timeoutMs);
}

void AuxiliaryDomainReconciler::queryDomains()
{
    const auto token = ++_token;
    _queryGeneration = _generation;
    _mappingSnapshot = _mappedDomainIdentifiers();
    _mappingSnapshot.removeDuplicates();
    _mappingSnapshot.sort();
    // A reserved identifier in account settings must never be treated as disposable.
    if (_auxiliaryIdentifier.isEmpty() || _mappingSnapshot.contains(_auxiliaryIdentifier)) {
        complete(false, u"The auxiliary identifier conflicts with an account mapping."_s);
        return;
    }
    armTimeout();
    const auto guard = QPointer<AuxiliaryDomainReconciler>(this);
    _query([guard, dispatcher = _dispatcher, token](bool success, QHash<QString, bool> domains, QString error) {
        QMetaObject::invokeMethod(
            dispatcher.get(),
            [guard, token, success, domains = std::move(domains), error = std::move(error)] {
                if (guard) {
                    guard->receivedDomains(token, success, domains, error);
                }
            },
            Qt::QueuedConnection);
    });
}

void AuxiliaryDomainReconciler::receivedDomains(quint64 token, bool success, const QHash<QString, bool> &domains, const QString &error)
{
    if (!_active || token != _token) {
        return;
    }
    _timer.stop();
    auto mapping = _mappedDomainIdentifiers();
    mapping.removeDuplicates();
    mapping.sort();
    if (_queryGeneration != _generation || mapping != _mappingSnapshot) {
        queryDomains();
        return;
    }
    if (!success) {
        failed(error.isEmpty() ? u"Cannot query File Provider domains."_s : error);
        return;
    }
    const auto realCount = std::count_if(mapping.cbegin(), mapping.cend(), [&domains](const auto &id) {
        return domains.contains(id);
    });
    if (realCount == 1) {
        if (!domains.contains(_auxiliaryIdentifier) || !domains.value(_auxiliaryIdentifier)) {
            mutate(_ensureHidden);
            return;
        }
    } else if (domains.contains(_auxiliaryIdentifier)) {
        mutate(_removeAuxiliary);
        return;
    }
    complete(true);
}

void AuxiliaryDomainReconciler::mutate(const Mutation &mutation)
{
    Q_ASSERT(!_mutationInFlight);
    _mutationInFlight = true;
    _mutationTimedOut = false;
    const auto token = ++_token;
    armTimeout();
    const auto guard = QPointer<AuxiliaryDomainReconciler>(this);
    mutation([guard, dispatcher = _dispatcher, token](bool success, QString error) {
        QMetaObject::invokeMethod(
            dispatcher.get(),
            [guard, token, success, error = std::move(error)] {
                if (guard) {
                    guard->receivedMutation(token, success, error);
                }
            },
            Qt::QueuedConnection);
    });
}

void AuxiliaryDomainReconciler::receivedMutation(quint64 token, bool success, const QString &error)
{
    if (!_active || !_mutationInFlight || token != _token) {
        return;
    }
    _timer.stop();
    _mutationInFlight = false;
    if (std::exchange(_mutationTimedOut, false)) {
        // A late completion can reflect a real side effect. Re-read registrations before
        // deciding anything, including when the caller changed accounts while waiting.
        _retries = 0;
        queryDomains();
        return;
    }
    if (!success) {
        failed(error.isEmpty() ? u"Cannot update the auxiliary File Provider domain."_s : error);
        return;
    }
    auto mapping = _mappedDomainIdentifiers();
    mapping.removeDuplicates();
    mapping.sort();
    if (_queryGeneration != _generation || mapping != _mappingSnapshot) {
        queryDomains();
        return;
    }
    complete(true);
}

void AuxiliaryDomainReconciler::failed(const QString &error)
{
    _timer.stop();
    ++_token; // Reject duplicate or late completions, including those arriving during retry delay.
    if (_retries++ >= _maxRetries) {
        complete(false, error);
        return;
    }
    _timer.disconnect(this);
    connect(&_timer, &QTimer::timeout, this, &AuxiliaryDomainReconciler::queryDomains);
    _timer.start(_retryDelayMs);
}

void AuxiliaryDomainReconciler::complete(bool success, const QString &error)
{
    _timer.stop();
    ++_token;
    _active = false;
    Q_EMIT finished(success, error);
}

}
