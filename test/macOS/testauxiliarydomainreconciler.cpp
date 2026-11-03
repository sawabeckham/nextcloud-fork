/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "gui/macOS/auxiliarydomainreconciler.h"

#include <QSignalSpy>
#include <QtTest>
#include <thread>

using namespace Qt::StringLiterals;
using OCC::Mac::AuxiliaryDomainReconciler;

class TestAuxiliaryDomainReconciler : public QObject
{
    Q_OBJECT
private:
    const QString auxiliary = u"site.manro.nextcloudfork.tests.auxiliary"_s;
    const QString first = u"site.manro.nextcloudfork.tests.first"_s;
    const QString second = u"site.manro.nextcloudfork.tests.second"_s;
    QStringList mapping;
    QList<AuxiliaryDomainReconciler::QueryCompletion> queries;
    QList<AuxiliaryDomainReconciler::Completion> additions;
    QList<AuxiliaryDomainReconciler::Completion> removals;

    std::unique_ptr<AuxiliaryDomainReconciler> make(int timeoutMs = 1000, int retries = 0)
    {
        return std::make_unique<AuxiliaryDomainReconciler>(
            auxiliary,
            [this](auto callback) {
                queries.append(std::move(callback));
            },
            [this] {
                return mapping;
            },
            [this](auto callback) {
                additions.append(std::move(callback));
            },
            [this](auto callback) {
                removals.append(std::move(callback));
            },
            nullptr,
            timeoutMs,
            0,
            retries);
    }

private Q_SLOTS:
    void init()
    {
        mapping.clear();
        queries.clear();
        additions.clear();
        removals.clear();
    }

    void policy_data()
    {
        QTest::addColumn<int>("realCount");
        QTest::addColumn<int>("auxiliaryState"); // -1 absent, 0 visible, 1 hidden.
        QTest::addColumn<int>("action"); // -1 remove, 0 unchanged, 1 ensure hidden.
        QTest::newRow("zero-absent") << 0 << -1 << 0;
        QTest::newRow("zero-hidden") << 0 << 1 << -1;
        QTest::newRow("one-absent") << 1 << -1 << 1;
        QTest::newRow("one-visible") << 1 << 0 << 1;
        QTest::newRow("one-hidden") << 1 << 1 << 0;
        QTest::newRow("two-hidden") << 2 << 1 << -1;
        QTest::newRow("two-absent") << 2 << -1 << 0;
    }

    void policy()
    {
        QFETCH(int, realCount);
        QFETCH(int, auxiliaryState);
        QFETCH(int, action);
        // Unregistered mappings and unrelated registrations do not inflate the real count.
        mapping = {first, first, second, u"site.manro.nextcloudfork.tests.unregistered"_s};
        auto domains = QHash<QString, bool>{{u"site.manro.nextcloudfork.tests.unmapped"_s, false}};
        if (realCount > 0) {
            domains.insert(first, false);
        }
        if (realCount > 1) {
            domains.insert(second, false);
        }
        if (auxiliaryState >= 0) {
            domains.insert(auxiliary, auxiliaryState != 0);
        }
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        QCOMPARE(queries.size(), 1);
        queries.takeFirst()(true, domains, {});
        QTRY_COMPARE(additions.size(), action == 1 ? 1 : 0);
        QTRY_COMPARE(removals.size(), action == -1 ? 1 : 0);
        if (action == 1) {
            additions.takeFirst()(true, {});
        }
        if (action == -1) {
            removals.takeFirst()(true, {});
        }
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), true);
        QCOMPARE(mapping.size(), 4);
    }

    void reservedMappingFailsClosed()
    {
        mapping = {auxiliary};
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        QVERIFY(!finished.at(0).at(1).toString().isEmpty());
        QVERIFY(queries.isEmpty());
        QVERIFY(additions.isEmpty());
        QVERIFY(removals.isEmpty());
    }

    void errorsRetryFromFreshQuery_data()
    {
        QTest::addColumn<int>("failureStage");
        QTest::newRow("query") << 0;
        QTest::newRow("ensure") << 1;
        QTest::newRow("remove") << 2;
    }

    void errorsRetryFromFreshQuery()
    {
        QFETCH(int, failureStage);
        mapping = failureStage == 2 ? QStringList{} : QStringList{first};
        auto reconciler = make(1000, 1);
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        auto domains = QHash<QString, bool>{{first, false}, {auxiliary, false}};
        for (auto attempt = 0; attempt < 2; ++attempt) {
            QTRY_COMPARE(queries.size(), 1);
            if (failureStage == 0) {
                queries.takeFirst()(false, {}, u"query failed"_s);
            } else {
                queries.takeFirst()(true, domains, {});
                auto &mutations = failureStage == 1 ? additions : removals;
                QTRY_COMPARE(mutations.size(), 1);
                mutations.takeFirst()(false, u"mutation failed"_s);
            }
        }
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        QTest::qWait(10);
        QVERIFY(queries.isEmpty());
    }

    void staleQueryAndCoalescedTriggers()
    {
        mapping = {first};
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        reconciler->requestReconciliation();
        reconciler->requestReconciliation();
        QCOMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{first, false}}, {});
        QTRY_COMPARE(queries.size(), 1);
        QVERIFY(additions.isEmpty());
        // A mapping change also invalidates a query even without a new trigger.
        mapping = {first, second};
        queries.takeFirst()(true, {{first, false}}, {});
        QTRY_COMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{first, false}, {second, false}}, {});
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(additions.isEmpty());
        QVERIFY(removals.isEmpty());
    }

    void mappingCollisionWhileQueryPending()
    {
        mapping = {first};
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        mapping.append(auxiliary);
        queries.takeFirst()(true, {{first, false}}, {});
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        QVERIFY(additions.isEmpty());
        QVERIFY(removals.isEmpty());
    }

    void triggerDuringMutation()
    {
        mapping = {first};
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {{first, false}}, {});
        QTRY_COMPARE(additions.size(), 1);
        mapping.clear();
        reconciler->requestReconciliation();
        additions.takeFirst()(true, {});
        QTRY_COMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{auxiliary, true}}, {});
        QTRY_COMPARE(removals.size(), 1);
        removals.takeFirst()(true, {});
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), true);
    }

    void timeoutAndLateCallback()
    {
        mapping = {first};
        auto reconciler = make(20, 1);
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        auto stale = queries.takeFirst();
        QTRY_COMPARE(queries.size(), 1);
        stale(true, {{first, false}}, {});
        QCoreApplication::processEvents();
        QVERIFY(additions.isEmpty());
        // The second query also times out: bounded retries finish with a failure.
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        queries.takeFirst()(true, {{first, false}}, {});
        QCoreApplication::processEvents();
        QVERIFY(additions.isEmpty());
        QCOMPARE(finished.size(), 1);
    }

    void mutationTimeout_data()
    {
        QTest::addColumn<bool>("remove");
        QTest::newRow("ensure-timeout") << false;
        QTest::newRow("remove-timeout") << true;
    }

    void lateMutationReconcilesItsActualSideEffect_data()
    {
        QTest::addColumn<bool>("remove");
        QTest::newRow("late-add-after-last-account-removed") << false;
        QTest::newRow("late-remove-after-one-account-added") << true;
    }

    void lateMutationReconcilesItsActualSideEffect()
    {
        QFETCH(bool, remove);
        mapping = remove ? QStringList{} : QStringList{first};
        auto domains = QHash<QString, bool>{{first, false}};
        if (remove) {
            domains.insert(auxiliary, true);
        }
        auto reconciler = make(20, 1);
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, domains, {});
        auto &pending = remove ? removals : additions;
        QTRY_COMPARE(pending.size(), 1);
        auto late = pending.takeFirst();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), false);

        mapping = remove ? QStringList{first} : QStringList{};
        reconciler->requestReconciliation();
        QTest::qWait(50);
        // Native mutation completion is still unknown. No replacement may race it.
        QVERIFY(queries.isEmpty());
        QVERIFY(additions.isEmpty());
        QVERIFY(removals.isEmpty());

        if (remove) {
            domains.remove(auxiliary);
        } else {
            domains.insert(auxiliary, true);
        }
        late(true, {});
        QTRY_COMPARE(queries.size(), 1);
        queries.takeFirst()(true, domains, {});
        auto &correction = remove ? additions : removals;
        QTRY_COMPARE(correction.size(), 1);
        correction.takeFirst()(true, {});
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(finished.at(1).at(0).toBool(), true);
    }

    void mutationTimeout()
    {
        QFETCH(bool, remove);
        mapping = remove ? QStringList{} : QStringList{first};
        auto reconciler = make(20);
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {{first, false}, {auxiliary, false}}, {});
        auto &mutations = remove ? removals : additions;
        QTRY_COMPARE(mutations.size(), 1);
        auto late = mutations.takeFirst();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), false);
        late(true, {});
        QTRY_COMPARE(queries.size(), 1);
        QCOMPARE(finished.size(), 1);
        queries.takeFirst()(true, remove ? QHash<QString, bool>{} : QHash<QString, bool>{{first, false}, {auxiliary, true}}, {});
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(finished.at(1).at(0).toBool(), true);
    }

    void completionFromWorkerAndAfterDestruction()
    {
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &AuxiliaryDomainReconciler::finished);
        reconciler->requestReconciliation();
        auto callback = queries.takeFirst();
        auto worker = std::thread([callback] {
            callback(true, {}, {});
        });
        worker.join();
        QTRY_COMPARE(finished.size(), 1);
        reconciler->requestReconciliation();
        callback = queries.takeFirst();
        reconciler.reset();
        worker = std::thread([callback] {
            callback(true, {}, {});
        });
        worker.join();
        QCoreApplication::processEvents();
        QVERIFY(additions.isEmpty());
        QVERIFY(removals.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAuxiliaryDomainReconciler)
#include "testauxiliarydomainreconciler.moc"
