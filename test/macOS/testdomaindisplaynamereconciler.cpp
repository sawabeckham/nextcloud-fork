/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "gui/macOS/domaindisplaynamereconciler.h"
#include "gui/macOS/fileproviderdomainmanager.h"

#include <QSignalSpy>
#include <QtTest>
#include <thread>
#include <utility>

using namespace Qt::StringLiterals;
using OCC::Mac::DomainDisplayNameReconciler;

class TestDomainDisplayNameReconciler : public QObject
{
    Q_OBJECT
private:
    using Reconciler = DomainDisplayNameReconciler;
    const QString first = u"site.manro.nextcloudfork.tests.first"_s;
    const QString second = u"site.manro.nextcloudfork.tests.second"_s;
    const QString reserved = u"site.manro.nextcloudfork.tests.reserved"_s;
    QObject firstOwner;
    QObject secondOwner;
    Reconciler::Targets targets;
    QList<Reconciler::QueryCompletion> queries;
    QList<Reconciler::Completion> updates;
    QStringList updatedIdentifiers;
    QStringList updatedNames;

    std::unique_ptr<Reconciler> make(int timeout = 1000)
    {
        return std::make_unique<Reconciler>(
            reserved,
            [this](auto completion) {
                queries.append(std::move(completion));
            },
            [this] {
                return targets;
            },
            nullptr,
            timeout);
    }

    Reconciler::Domain domain(const QString &identifier, const QString &name)
    {
        return {name, [this, identifier](const QString &desired, auto completion) {
                    updatedIdentifiers.append(identifier);
                    updatedNames.append(desired);
                    updates.append(std::move(completion));
                }};
    }

private Q_SLOTS:
    void init()
    {
        targets = {{first, {u"New name"_s, &firstOwner}}};
        queries.clear();
        updates.clear();
        updatedIdentifiers.clear();
        updatedNames.clear();
    }

    void onlyExistingMappedDomainIsUpdated()
    {
        targets.insert(second, {u"Missing registration"_s, &secondOwner});
        targets.insert(reserved, {u"Never rename helper"_s, &secondOwner});
        targets.insert({}, {u"Never rename empty identifier"_s, &secondOwner});
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        reconciler->requestReconciliation();
        queries.takeFirst()(
            true,
            {{first, domain(first, u"Old name"_s)}, {reserved, domain(reserved, u"Internal"_s)}, {u"unmapped"_s, domain(u"unmapped"_s, u"Old"_s)}},
            {});
        QTRY_COMPARE(updates.size(), 1);
        QCOMPARE(updatedIdentifiers, QStringList{first});
        QCOMPARE(updatedNames, QStringList{u"New name"_s});
        updates.takeFirst()(true, {});
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), true);
        QVERIFY(queries.isEmpty());
    }

    void matchingAndMissingDomainsAreNoOps()
    {
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {{first, domain(first, u"New name"_s)}}, {});
        QTRY_COMPARE(finished.size(), 1);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {}, {});
        QTRY_COMPARE(finished.size(), 2);
        QVERIFY(updates.isEmpty());
    }

    void changedNameAndRemovedAccountInvalidateSnapshot()
    {
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        reconciler->requestReconciliation();
        targets[first].displayName = u"Newest name"_s;
        reconciler->requestReconciliation();
        reconciler->requestReconciliation();
        QCOMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(queries.size(), 1);
        QVERIFY(updates.isEmpty());
        targets.clear();
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(updates.isEmpty());
    }

    void changedMappingAndOwnerInvalidateSnapshot()
    {
        auto reconciler = make();
        reconciler->requestReconciliation();
        targets[first].owner = &secondOwner;
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(queries.size(), 1);
        QVERIFY(updates.isEmpty());
        targets = {{second, {u"Second name"_s, &secondOwner}}};
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}, {second, domain(second, u"Old"_s)}}, {});
        QTRY_COMPARE(updates.size(), 1);
        QCOMPARE(updatedIdentifiers, QStringList{second});
        updates.takeFirst()(true, {});
    }

    void triggersDuringMutationWaitForCompletion()
    {
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(updates.size(), 1);
        targets[first].displayName = u"Latest"_s;
        reconciler->requestReconciliation();
        reconciler->requestReconciliation();
        QVERIFY(queries.isEmpty());
        updates.takeFirst()(true, {});
        QTRY_COMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{first, domain(first, u"New name"_s)}}, {});
        QTRY_COMPARE(updates.size(), 1);
        QCOMPARE(updatedNames.last(), u"Latest"_s);
        updates.takeFirst()(true, {});
        QTRY_COMPARE(finished.size(), 1);
    }

    void queryFailureDoesNotMutate()
    {
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        auto failed = QSignalSpy(reconciler.get(), &Reconciler::failed);
        reconciler->requestReconciliation();
        queries.takeFirst()(false, {{first, domain(first, u"Old"_s)}}, u"Cannot query"_s);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), false);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(), first);
        QVERIFY(updates.isEmpty());
        QVERIFY(queries.isEmpty());
    }

    void mutationFailureDoesNotRetry()
    {
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        auto failed = QSignalSpy(reconciler.get(), &Reconciler::failed);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(updates.size(), 1);
        updates.takeFirst()(false, u"Cannot update"_s);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), false);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(), first);
        QVERIFY(queries.isEmpty());
    }

    void queryTimeoutRejectsLateCompletion()
    {
        auto reconciler = make(20);
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        reconciler->requestReconciliation();
        auto late = queries.takeFirst();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), false);
        late(true, {{first, domain(first, u"Old"_s)}}, {});
        QCoreApplication::processEvents();
        QVERIFY(updates.isEmpty());
    }

    void mutationTimeoutFencesLateSideEffects()
    {
        auto reconciler = make(20);
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(updates.size(), 1);
        auto late = updates.takeFirst();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), false);
        targets[first].displayName = u"Latest"_s;
        reconciler->requestReconciliation();
        QTest::qWait(40);
        QVERIFY(queries.isEmpty());
        QVERIFY(updates.isEmpty());
        late(true, {});
        QTRY_COMPARE(queries.size(), 1);
        queries.takeFirst()(true, {{first, domain(first, u"New name"_s)}}, {});
        QTRY_COMPARE(updates.size(), 1);
        QCOMPARE(updatedNames.last(), u"Latest"_s);
        updates.takeFirst()(true, {});
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(finished.last().first().toBool(), true);
    }

    void callbackFromWorkerAndAfterDestruction()
    {
        auto reconciler = make();
        auto finished = QSignalSpy(reconciler.get(), &Reconciler::finished);
        reconciler->requestReconciliation();
        auto callback = queries.takeFirst();
        auto worker = std::thread([callback] {
            callback(true, {}, {});
        });
        worker.join();
        QTRY_COMPARE(finished.size(), 1);
        reconciler->requestReconciliation();
        queries.takeFirst()(true, {{first, domain(first, u"Old"_s)}}, {});
        QTRY_COMPARE(updates.size(), 1);
        auto mutation = updates.takeFirst();
        reconciler.reset();
        worker = std::thread([mutation] {
            mutation(true, {});
        });
        worker.join();
        QCoreApplication::processEvents();
        QVERIFY(queries.isEmpty());
    }

#ifdef NEXTCLOUD_FORK
    void nativePackageValidation()
    {
        using Manager = OCC::Mac::FileProviderDomainManager;
        QVERIFY(Manager::isConnectionNameSupported({}));
        QVERIFY(Manager::isConnectionNameSupported(u"  Рабочие файлы  "_s));
        QVERIFY(Manager::isConnectionNameSupported(u"Reports.txt"_s));
        QVERIFY(!Manager::isConnectionNameSupported(u"Documents.app"_s));
        QVERIFY(!Manager::isConnectionNameSupported(u"Documents.bundle"_s));
        QVERIFY(!Manager::isConnectionNameSupported(u" Documents.app  "_s));
        QVERIFY(!Manager::isConnectionNameSupported(u"Invalid/name"_s));
    }
#endif
};

QTEST_GUILESS_MAIN(TestDomainDisplayNameReconciler)
#include "testdomaindisplaynamereconciler.moc"
