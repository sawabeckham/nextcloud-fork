/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>
#include <memory>

namespace OCC::Mac
{

// Reconciles only the reserved auxiliary domain. Backends never receive a real domain ID.
class AuxiliaryDomainReconciler : public QObject
{
    Q_OBJECT
public:
    using Completion = std::function<void(bool, QString)>;
    using QueryCompletion = std::function<void(bool, QHash<QString, bool>, QString)>;
    using Query = std::function<void(QueryCompletion)>;
    using MappedDomainIdentifiers = std::function<QStringList()>;
    using Mutation = std::function<void(Completion)>;
    static constexpr auto DefaultTimeoutMs = 10000;
    static constexpr auto DefaultRetryDelayMs = 250;
    static constexpr auto DefaultMaxRetries = 2;

    AuxiliaryDomainReconciler(QString auxiliaryIdentifier,
                              Query query,
                              MappedDomainIdentifiers mappedDomainIdentifiers,
                              Mutation ensureHidden,
                              Mutation removeAuxiliary,
                              QObject *parent = nullptr,
                              int timeoutMs = DefaultTimeoutMs,
                              int retryDelayMs = DefaultRetryDelayMs,
                              int maxRetries = DefaultMaxRetries);

    // Called on the object's thread. Multiple requests share one serialized reconciliation.
    void requestReconciliation();

Q_SIGNALS:
    void finished(bool success, const QString &error);

private:
    void queryDomains();
    void receivedDomains(quint64 token, bool success, const QHash<QString, bool> &domains, const QString &error);
    void mutate(const Mutation &mutation);
    void receivedMutation(quint64 token, bool success, const QString &error);
    void failed(const QString &error);
    void complete(bool success, const QString &error = {});
    void armTimeout();

    const QString _auxiliaryIdentifier;
    const Query _query;
    const MappedDomainIdentifiers _mappedDomainIdentifiers;
    const Mutation _ensureHidden;
    const Mutation _removeAuxiliary;
    const int _timeoutMs;
    const int _retryDelayMs;
    const int _maxRetries;
    // Callback-held ownership keeps this dispatch context alive until background completions finish.
    const std::shared_ptr<QObject> _dispatcher;
    QTimer _timer;
    QStringList _mappingSnapshot;
    quint64 _token = 0;
    quint64 _generation = 0;
    quint64 _queryGeneration = 0;
    int _retries = 0;
    bool _active = false;
    // Native mutations cannot be cancelled. A timeout reports failure but retains this fence.
    bool _mutationInFlight = false;
    bool _mutationTimedOut = false;
};

}
