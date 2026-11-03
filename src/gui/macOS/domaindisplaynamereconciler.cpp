/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "domaindisplaynamereconciler.h"

#include <QPointer>
#include <QThread>
#include <utility>

namespace OCC::Mac
{
using namespace Qt::StringLiterals;

DomainDisplayNameReconciler::DomainDisplayNameReconciler(QString reservedIdentifier, Query query, CurrentTargets targets, QObject *parent, int timeoutMs)
    : QObject(parent)
    , _reservedIdentifier(std::move(reservedIdentifier))
    , _query(std::move(query))
    , _targets(std::move(targets))
    , _timeoutMs(qMax(1, timeoutMs))
    , _dispatcher(new QObject, [](QObject *context) {
        context->deleteLater();
    })
{
    _timer.setSingleShot(true);
    connect(&_timer, &QTimer::timeout, this, [this] {
        if (_mutationInFlight) {
            _mutationTimedOut = true;
            const auto error = u"Domain name update timed out; further updates wait for its completion."_s;
            reportFailure(error);
            Q_EMIT finished(false, error);
            return;
        }
        complete(false, u"Domain name query timed out."_s);
    });
}

void DomainDisplayNameReconciler::requestReconciliation()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++_generation;
    if (_active) {
        return;
    }
    _active = true;
    queryDomains();
}

void DomainDisplayNameReconciler::armTimeout()
{
    _timer.start(_timeoutMs);
}

void DomainDisplayNameReconciler::queryDomains()
{
    _domains.clear();
    _mutationIdentifier.clear();
    _snapshot = currentTargets();
    _queryGeneration = _generation;
    const auto token = ++_token;
    armTimeout();
    const auto guard = QPointer<DomainDisplayNameReconciler>(this);
    _query([guard, dispatcher = _dispatcher, token](bool success, Domains domains, QString error) {
        QMetaObject::invokeMethod(
            dispatcher.get(),
            [guard, token, success, domains = std::move(domains), error = std::move(error)]() mutable {
                if (guard) {
                    guard->receivedDomains(token, success, std::move(domains), error);
                }
            },
            Qt::QueuedConnection);
    });
}

void DomainDisplayNameReconciler::receivedDomains(quint64 token, bool success, Domains domains, const QString &error)
{
    if (!_active || token != _token || _mutationInFlight) {
        return;
    }
    _timer.stop();
    if (!success) {
        complete(false, error.isEmpty() ? u"Cannot query File Provider domain names."_s : error);
        return;
    }
    _domains = std::move(domains);
    updateNext();
}

void DomainDisplayNameReconciler::updateNext()
{
    if (_queryGeneration != _generation || currentTargets() != _snapshot) {
        queryDomains();
        return;
    }
    while (!_domains.isEmpty()) {
        const auto entry = _domains.begin();
        const auto identifier = entry.key();
        const auto domain = entry.value();
        _domains.erase(entry);
        const auto target = _snapshot.constFind(identifier);
        if (target == _snapshot.cend() || target->displayName.isEmpty() || target->displayName == domain.displayName || !domain.update) {
            continue;
        }
        _mutationInFlight = true;
        _mutationIdentifier = identifier;
        _mutationTimedOut = false;
        const auto token = ++_token;
        armTimeout();
        const auto guard = QPointer<DomainDisplayNameReconciler>(this);
        domain.update(target->displayName, [guard, dispatcher = _dispatcher, token](bool success, QString error) {
            QMetaObject::invokeMethod(
                dispatcher.get(),
                [guard, token, success, error = std::move(error)] {
                    if (guard) {
                        guard->receivedMutation(token, success, error);
                    }
                },
                Qt::QueuedConnection);
        });
        return;
    }
    complete(true);
}

void DomainDisplayNameReconciler::receivedMutation(quint64 token, bool success, const QString &error)
{
    if (!_active || !_mutationInFlight || token != _token) {
        return;
    }
    _timer.stop();
    _mutationInFlight = false;
    if (std::exchange(_mutationTimedOut, false)) {
        // The uncancellable update may have changed registration after the timeout.
        queryDomains();
    } else if (_queryGeneration != _generation || currentTargets() != _snapshot) {
        queryDomains();
    } else if (!success) {
        complete(false, error.isEmpty() ? u"Cannot update File Provider domain name."_s : error);
    } else {
        _mutationIdentifier.clear();
        updateNext();
    }
}

DomainDisplayNameReconciler::Targets DomainDisplayNameReconciler::currentTargets() const
{
    auto targets = _targets();
    targets.remove(_reservedIdentifier);
    targets.remove({});
    return targets;
}

void DomainDisplayNameReconciler::complete(bool success, const QString &error)
{
    _timer.stop();
    ++_token;
    _domains.clear();
    _active = false;
    if (!success) {
        reportFailure(error);
    }
    Q_EMIT finished(success, error);
}

void DomainDisplayNameReconciler::reportFailure(const QString &error)
{
    if (!_mutationIdentifier.isEmpty()) {
        Q_EMIT failed(_mutationIdentifier, error);
        return;
    }
    for (auto target = _snapshot.cbegin(); target != _snapshot.cend(); ++target) {
        Q_EMIT failed(target.key(), error);
    }
}

}
