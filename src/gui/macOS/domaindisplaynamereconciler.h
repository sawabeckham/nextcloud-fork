/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <memory>

namespace OCC::Mac
{

// Serializes in-place name updates of registered, currently mapped domains.
class DomainDisplayNameReconciler : public QObject
{
    Q_OBJECT
public:
    using Completion = std::function<void(bool, QString)>;
    struct Target {
        QString displayName;
        QPointer<QObject> owner;
        bool operator==(const Target &) const = default;
    };
    struct Domain {
        QString displayName;
        // Captures the registered domain snapshot, including its native metadata.
        std::function<void(const QString &, Completion)> update;
    };
    using Targets = QMap<QString, Target>;
    using Domains = QMap<QString, Domain>;
    using QueryCompletion = std::function<void(bool, Domains, QString)>;
    using Query = std::function<void(QueryCompletion)>;
    using CurrentTargets = std::function<Targets()>;
    static constexpr auto DefaultTimeoutMs = 10000;

    DomainDisplayNameReconciler(QString reservedIdentifier, Query query, CurrentTargets targets, QObject *parent = nullptr, int timeoutMs = DefaultTimeoutMs);
    void requestReconciliation();

Q_SIGNALS:
    void finished(bool success, const QString &error);
    void failed(const QString &domainIdentifier, const QString &error);

private:
    void queryDomains();
    void receivedDomains(quint64 token, bool success, Domains domains, const QString &error);
    void updateNext();
    void receivedMutation(quint64 token, bool success, const QString &error);
    void armTimeout();
    [[nodiscard]] Targets currentTargets() const;
    void complete(bool success, const QString &error = {});
    void reportFailure(const QString &error);

    const QString _reservedIdentifier;
    const Query _query;
    const CurrentTargets _targets;
    const int _timeoutMs;
    const std::shared_ptr<QObject> _dispatcher;
    QTimer _timer;
    Targets _snapshot;
    Domains _domains;
    QString _mutationIdentifier;
    quint64 _token = 0;
    quint64 _generation = 0;
    quint64 _queryGeneration = 0;
    bool _active = false;
    bool _mutationInFlight = false;
    bool _mutationTimedOut = false;
};

}
