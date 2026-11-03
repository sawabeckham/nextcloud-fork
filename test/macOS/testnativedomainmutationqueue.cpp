/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "gui/macOS/nativedomainmutationqueue.h"

#include <QtTest>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <utility>

using namespace Qt::StringLiterals;
using OCC::Mac::NativeDomainMutationQueue;

class TestNativeDomainMutationQueue : public QObject
{
    Q_OBJECT
private:
    using Queue = NativeDomainMutationQueue;
    QList<Queue::Finished> scheduled;

    std::unique_ptr<Queue> make()
    {
        return std::make_unique<Queue>([this](auto operation) {
            scheduled.append(std::move(operation));
        });
    }

    template<typename Result>
    static auto await(std::future<Result> &result) -> Result
    {
        if (result.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
            qFatal("Native mutation wait did not finish within the test watchdog.");
        }
        return result.get();
    }

private Q_SLOTS:
    void init()
    {
        scheduled.clear();
    }

    void serializesUntilActualCompletion()
    {
        auto queue = make();
        auto order = QStringList{};
        auto firstFinished = Queue::Finished{};
        queue->enqueue([&](auto finished) {
            order.append(u"first"_s);
            firstFinished = std::move(finished);
        });
        queue->enqueue([&](auto finished) {
            order.append(u"second"_s);
            finished();
        });
        QCOMPARE(scheduled.size(), 1);
        scheduled.takeFirst()();
        QCOMPARE(order, QStringList{u"first"_s});
        QVERIFY(scheduled.isEmpty());
        // A caller timeout or a destroyed GUI owner cannot release an uncancellable native mutation.
        QCoreApplication::processEvents();
        QVERIFY(scheduled.isEmpty());
        firstFinished();
        QCOMPARE(scheduled.size(), 1);
        firstFinished();
        QCOMPARE(scheduled.size(), 1);
        scheduled.takeFirst()();
        QCOMPARE(order, (QStringList{u"first"_s, u"second"_s}));
        QVERIFY(scheduled.isEmpty());
    }

    void removalBeforeRenameCannotResurrectDomain()
    {
        auto queue = make();
        auto registered = true;
        auto renamed = false;
        auto result = true;
        auto removalFinished = Queue::Finished{};
        queue->enqueue([&](auto finished) {
            removalFinished = std::move(finished);
        });
        queue->updateExisting(
            [&](auto queried) {
                QVERIFY(!registered);
                queried(true,
                        registered ? Queue::Mutation{[&](auto completed) {
                            renamed = true;
                            completed(true, {});
                        }}
                                   : Queue::Mutation{},
                        {});
            },
            [&](bool success, const QString &) {
                result = success;
            });
        scheduled.takeFirst()();
        QVERIFY(scheduled.isEmpty());
        registered = false;
        removalFinished();
        QCOMPARE(scheduled.size(), 1);
        scheduled.takeFirst()();
        QVERIFY(!renamed);
        QVERIFY(!registered);
        QVERIFY(!result);
    }

    void renameBeforeRemovalFinishesFirst()
    {
        auto queue = make();
        auto registered = true;
        auto metadata = u"old metadata"_s;
        auto copiedMetadata = QString{};
        auto renamed = false;
        auto renameFinished = Queue::Completion{};
        queue->updateExisting(
            [&](auto queried) {
                // The metadata observed before queuing may already be stale.
                copiedMetadata = metadata;
                queried(true,
                        [&](auto completed) {
                            renamed = true;
                            renameFinished = std::move(completed);
                        },
                        {});
            },
            [](bool, const QString &) {});
        metadata = u"latest metadata"_s;
        queue->enqueue([&](auto finished) {
            registered = false;
            finished();
        });
        scheduled.takeFirst()();
        QCOMPARE(copiedMetadata, u"latest metadata"_s);
        QVERIFY(renamed);
        QVERIFY(registered);
        QVERIFY(scheduled.isEmpty());
        renameFinished(true, {});
        QCOMPARE(scheduled.size(), 1);
        scheduled.takeFirst()();
        QVERIFY(!registered);
        QVERIFY(scheduled.isEmpty());
    }

    void queryFailureReleasesQueueWithoutMutation()
    {
        auto queue = make();
        auto changed = false;
        auto nextRan = false;
        auto result = true;
        queue->updateExisting(
            [&](auto queried) {
                queried(
                    false,
                    [&](auto completed) {
                        changed = true;
                        completed(true, {});
                    },
                    u"query failed"_s);
            },
            [&](bool success, const QString &error) {
                result = success;
                QCOMPARE(error, u"query failed"_s);
            });
        queue->enqueue([&](auto finished) {
            nextRan = true;
            finished();
        });
        scheduled.takeFirst()();
        QVERIFY(!changed);
        QVERIFY(!result);
        QCOMPARE(scheduled.size(), 1);
        scheduled.takeFirst()();
        QVERIFY(nextRan);
    }

    void duplicateQueryAndMutationCallbacksAreIgnored()
    {
        auto queue = make();
        auto queries = Queue::QueryCompletion{};
        auto mutation = Queue::Completion{};
        auto mutations = 0;
        auto results = 0;
        queue->updateExisting(
            [&](auto queried) {
                queries = std::move(queried);
            },
            [&](bool, const QString &) {
                ++results;
            });
        scheduled.takeFirst()();
        auto update = [&](auto completed) {
            ++mutations;
            mutation = std::move(completed);
        };
        queries(true, update, {});
        queries(true, update, {});
        QCOMPARE(mutations, 1);
        mutation(false, u"update failed"_s);
        mutation(true, {});
        QCOMPARE(results, 1);
        QVERIFY(scheduled.isEmpty());
    }

    void workerCompletionReleasesFenceWithoutQtEventLoop()
    {
        auto queue = std::make_unique<Queue>([](auto operation) {
            operation();
        });
        auto completion = Queue::Finished{};
        auto nextRan = false;
        auto workerThread = std::thread::id{};
        queue->enqueue([&](auto finished) {
            completion = std::move(finished);
        });
        queue->enqueue([&](auto finished) {
            workerThread = std::this_thread::get_id();
            nextRan = true;
            finished();
        });
        // Simulate a synchronous owner waiting for the native callback, with no Qt event processing.
        auto worker = std::thread([completion] {
            completion();
        });
        const auto expectedThread = worker.get_id();
        worker.join();
        QVERIFY(nextRan);
        QVERIFY(workerThread == expectedThread);
    }

    void delayedCreateWaitsBeforePersistingItsIdentifier_data()
    {
        QTest::addColumn<bool>("nativeSuccess");
        QTest::newRow("success") << true;
        QTest::newRow("error") << false;
    }

    void delayedCreateWaitsBeforePersistingItsIdentifier()
    {
        QFETCH(bool, nativeSuccess);
        auto queue = Queue([](auto operation) {
            operation();
        });
        auto started = std::promise<Queue::Finished>{};
        auto nativeCompletion = started.get_future();
        auto createSucceeded = false;
        const auto identifier = u"new-domain-identifier"_s;
        auto mapping = std::async(std::launch::async, [&] {
            queue.enqueueAndWait([&](auto finished) {
                started.set_value(std::move(finished));
            });
            return createSucceeded ? identifier : QString{};
        });
        auto finished = await(nativeCompletion);
        const auto returnedEarly = mapping.wait_for(std::chrono::milliseconds(20)) == std::future_status::ready;
        createSucceeded = nativeSuccess;
        finished();
        // A duplicate native callback must not throw or release another operation.
        finished();
        const auto persistedIdentifier = await(mapping);
        QVERIFY(!returnedEarly);
        QCOMPARE(persistedIdentifier, nativeSuccess ? identifier : QString{});
    }

    void queuedRemovalWaitsBehindRenameAndReturnsPreservedData()
    {
        auto queue = Queue([](auto operation) {
            operation();
        });
        auto renameFinished = Queue::Finished{};
        queue.enqueue([&](auto finished) {
            renameFinished = std::move(finished);
        });
        auto entered = std::promise<void>{};
        auto enteredWait = entered.get_future();
        auto removeStarted = std::atomic_bool(false);
        auto removed = false;
        auto preservedData = QString{};
        const auto recoveryPath = u"/test/preserved-dirty-data"_s;
        auto removal = std::async(std::launch::async, [&] {
            entered.set_value();
            queue.enqueueAndWait([&](auto finished) {
                removeStarted = true;
                removed = true;
                preservedData = recoveryPath;
                finished();
            });
            return preservedData;
        });
        await(enteredWait);
        const auto returnedEarly = removal.wait_for(std::chrono::milliseconds(20)) == std::future_status::ready;
        const auto startedEarly = removeStarted.load();
        renameFinished();
        const auto returnedPath = await(removal);
        QVERIFY(!returnedEarly);
        QVERIFY(!startedEarly);
        QVERIFY(removed);
        QCOMPARE(returnedPath, recoveryPath);
    }

    void pendingOperationsOutliveQueueOwner()
    {
        auto queue = make();
        auto completion = Queue::Finished{};
        auto nextRan = false;
        queue->enqueue([&](auto finished) {
            completion = std::move(finished);
        });
        queue->enqueue([&](auto finished) {
            nextRan = true;
            finished();
        });
        scheduled.takeFirst()();
        queue.reset();
        completion();
        QCOMPARE(scheduled.size(), 1);
        scheduled.takeFirst()();
        QVERIFY(nextRan);
    }
};

QTEST_GUILESS_MAIN(TestNativeDomainMutationQueue)
#include "testnativedomainmutationqueue.moc"
