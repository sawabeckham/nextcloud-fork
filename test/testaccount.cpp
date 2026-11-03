/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <qglobal.h>
#include <QTemporaryDir>
#include <QtTest>

#include "account.h"
#include "accountstate.h"
#include "logger.h"
#include "syncenginetestutils.h"

using namespace OCC;
using namespace Qt::StringLiterals;

class TestAccount: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testAccountDavPath_unitialized_noCrash()
    {
        AccountPtr account = Account::create();
        [[maybe_unused]] const auto davPath = account->davPath();
    }

    void testAccount_isPublicShareLink_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("expectedResult");
        QTest::addColumn<QString>("expectedDavUser");

        QTest::newRow("plain URL") << "https://example.com" << false << "";
        QTest::newRow("plain URL, trailing slash") << "https://example.com/" << false << "";
        QTest::newRow("share link") << "https://example.com/s/rPZLaTKfWct37Nd" << true << "rPZLaTKfWct37Nd";
        QTest::newRow("share link, trailing slash") << "https://example.com/s/rPZLaTKfWct37Nd/" << false << "";
        QTest::newRow("subpath containing /s/ (looks like share link)") << "https://example.com/s/nextcloud" << true << "nextcloud";
        QTest::newRow("subpath containing /s/, trailing slash") << "https://example.com/s/nextcloud/" << false << "";
        QTest::newRow("subpath containing /s/, share link") << "https://example.com/s/nextcloud/s/rPZLaTKfWct37Nd" << true << "rPZLaTKfWct37Nd";
        QTest::newRow("subpath containing /s/, share link, trailing slash") << "https://example.com/s/nextcloud/s/rPZLaTKfWct37Nd/" << false << "";
    }

    void testAccount_isPublicShareLink()
    {
        AccountPtr account = Account::create();

        QFETCH(QString, url);
        QFETCH(bool, expectedResult);
        QFETCH(QString, expectedDavUser);

        account->setUrl(QUrl{url});
        QCOMPARE(account->isPublicShareLink(), expectedResult);
        QCOMPARE(account->davUser(), expectedDavUser);
    }

#ifdef NEXTCLOUD_FORK
    void connectionNameValidation_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("valid");
        QTest::newRow("automatic") << QString{} << true;
        QTest::newRow("spaces-reset") << u"   "_s << true;
        QTest::newRow("trimmed-cyrillic") << u"  Мой сервер  "_s << true;
        QTest::newRow("utf16-boundary") << QString(100, u'a') << true;
        QTest::newRow("utf16-overflow") << QString(101, u'a') << false;
        QTest::newRow("utf8-boundary") << QString(100, QChar(0x044F)) << true;
        QTest::newRow("utf8-overflow") << QString(67, QChar(0x4E00)) << false;
        QTest::newRow("emoji-boundary") << u"😀"_s.repeated(50) << true;
        QTest::newRow("emoji-overflow") << u"😀"_s.repeated(51) << false;
        QTest::newRow("slash") << u"my/server"_s << false;
        QTest::newRow("colon") << u"my:server"_s << false;
        QTest::newRow("backslash") << u"my\\server"_s << false;
        QTest::newRow("dot") << u"."_s << false;
        QTest::newRow("dotdot") << u".."_s << false;
        QTest::newRow("trailing-dot") << u"server. "_s << false;
        QTest::newRow("newline") << u"server\n"_s << false;
        QTest::newRow("tab") << u"\tserver"_s << false;
        QTest::newRow("null") << QString(1, QChar(0)) << false;
        QTest::newRow("unicode-control") << QString(1, QChar(0x0085)) << false;
        QTest::newRow("unicode-line-separator") << u"Home\u2028Work"_s << false;
        QTest::newRow("unicode-paragraph-separator") << u"Home\u2029Work"_s << false;
    }

    void connectionNameValidation()
    {
        QFETCH(QString, name);
        QFETCH(bool, valid);
        QCOMPARE(Account::isValidConnectionName(name), valid);
        const auto account = Account::create();
        QVERIFY(account->setConnectionName(u"Original"_s));
        QSignalSpy changed(account.data(), &Account::connectionNameChanged);
        QSignalSpy saved(account.data(), &Account::wantsAccountSaved);
        QCOMPARE(account->setConnectionName(name), valid);
        QCOMPARE(account->connectionName(), valid ? name.trimmed() : u"Original"_s);
        QCOMPARE(changed.count(), valid ? 1 : 0);
        QCOMPARE(saved.count(), valid ? 1 : 0);
    }

    void connectionNameChangesOnlyLocalLabel()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(u"https://cloud.example.invalid:8443"_s));
        account->setDavUser(u"alice"_s);
        account->setDavDisplayName(u"Alice"_s);
        const auto displayName = account->displayName();
        const auto prettyName = account->prettyName();
        const auto userId = account->userIdAtHostWithPort();
        const auto url = account->url();
        const auto id = account->id();
        QSignalSpy changed(account.data(), &Account::connectionNameChanged);
        QSignalSpy saved(account.data(), &Account::wantsAccountSaved);
        QSignalSpy davNameChanged(account.data(), &Account::accountChangedDisplayName);
        QSignalSpy prettyNameChanged(account.data(), &Account::prettyNameChanged);
        QVERIFY(account->setConnectionName(u"  Мой сервер  "_s));
        QCOMPARE(account->connectionName(), u"Мой сервер"_s);
        QVERIFY(account->setConnectionName(u"Мой сервер"_s));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(saved.count(), 1);
        QCOMPARE(qvariant_cast<AccountPtr>(saved.first().first()), account);
        QVERIFY(account->setConnectionName(u"   "_s));
        QVERIFY(account->connectionName().isEmpty());
        QVERIFY(account->setConnectionName({}));
        QCOMPARE(changed.count(), 2);
        QCOMPARE(saved.count(), 2);
        QCOMPARE(davNameChanged.count(), 0);
        QCOMPARE(prettyNameChanged.count(), 0);
        QCOMPARE(account->displayName(), displayName);
        QCOMPARE(account->prettyName(), prettyName);
        QCOMPARE(account->davDisplayName(), u"Alice"_s);
        QCOMPARE(account->davUser(), u"alice"_s);
        QCOMPARE(account->userIdAtHostWithPort(), userId);
        QCOMPARE(account->url(), url);
        QCOMPARE(account->id(), id);
    }
#endif

    void testAccount_setLimitSettings_globalNetworkLimitFallback()
    {
        using LimitSetting = Account::AccountNetworkTransferLimitSetting;
        AccountPtr account = Account::create();

        const auto setLimitSettings = [account](const LimitSetting setting) -> void {
            account->setDownloadLimitSetting(setting);
            account->setUploadLimitSetting(setting);
        };

        const auto verifyLimitSettings = [account](const LimitSetting expectedSetting) -> void {
            QCOMPARE_EQ(expectedSetting, account->downloadLimitSetting());
            QCOMPARE_EQ(expectedSetting, account->uploadLimitSetting());
        };

        // the default setting should be NoLimit
        verifyLimitSettings(LimitSetting::NoLimit);

        // changing it to ManualLimit should succeed
        setLimitSettings(LimitSetting::ManualLimit);
        verifyLimitSettings(LimitSetting::ManualLimit);

        // changing it to AutoLimit should fall back to NoLimit
        setLimitSettings(LimitSetting::AutoLimit);
        verifyLimitSettings(LimitSetting::NoLimit);

        // changing it to LegacyGlobalLimit (-2) should fall back to NoLimit
        setLimitSettings(LimitSetting::LegacyGlobalLimit);
        verifyLimitSettings(LimitSetting::NoLimit);
    }

    void testAccount_listRemoteFolder_data()
    {
        QTest::addColumn<QString>("remotePath");
        QTest::addColumn<QString>("subPath");
        QTest::addColumn<QStringList>("expectedPaths");

        QTest::newRow("root = /; requesting ''") << "" << "" << QStringList{ "A", "B", "C", "S" };
        QTest::newRow("root = /; requesting A/") << "" << "A/" << QStringList{ "A/a1", "A/a2", "A/sub1" };
        QTest::newRow("root = /; requesting A/sub1/") << "" << "A/sub1/" << QStringList{ "A/sub1/sub2" };
        QTest::newRow("root = /; requesting A/sub1/sub2") << "" << "A/sub1/sub2/" << QStringList{};
        QTest::newRow("root = /; requesting A/sub_nodownload/") << "" << "A/sub_nodownload/" << QStringList{};
        QTest::newRow("root = /A; requesting A/") << "/A" << "A/" << QStringList{ "a1", "a2", "sub1" };
        QTest::newRow("root = /A; requesting A/sub1") << "/A" << "A/sub1/" << QStringList{ "sub1/sub2" };
        QTest::newRow("root = /A; requesting A/sub1/sub2") << "/A" << "A/sub1/sub2/" << QStringList{};
        QTest::newRow("root = /A; requesting A/sub_nodownload/") << "/A" << "A/sub_nodownload/" << QStringList{};
        QTest::newRow("root = /shared_nodownload; requesting shared_nodownload/") << "/shared_nodownload" << "shared_nodownload/" << QStringList{};
    }

    void testAccount_listRemoteFolder()
    {
        QFETCH(QString, remotePath);
        QFETCH(QString, subPath);
        QFETCH(QStringList, expectedPaths);

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().mkdir("A/sub1");
        fakeFolder.remoteModifier().mkdir("A/sub1/sub2");
        fakeFolder.remoteModifier().mkdir("A/sub1/sub2");

        // prepare shares that are not allowed to be downloaded
        std::function<void(FileInfo &)> makeSharedNoDownload;
        makeSharedNoDownload = [&makeSharedNoDownload](FileInfo &fileInfo) -> void {
            fileInfo.isShared = true;
            fileInfo.downloadForbidden = true;
            for (auto &subItem : fileInfo.children) {
                makeSharedNoDownload(subItem);
            }
        };
        FileInfo sharedNoDownload { "shared_nodownload"_L1, { { "file1"_L1, 32 }, { "file2"_L1, 32 } } };
        FileInfo sharedNoDownloadSub { "subdir"_L1, { { "file3"_L1, 32 }, { "file4"_L1, 32 } } };
        FileInfo aSubSharedNoDownload { "sub_nodownload"_L1, { { "file4"_L1, 32 }, { "file5"_L1, 32 } } };
        sharedNoDownload.children.insert(sharedNoDownloadSub.name, std::move(sharedNoDownloadSub));
        makeSharedNoDownload(sharedNoDownload);
        makeSharedNoDownload(aSubSharedNoDownload);
        fakeFolder.remoteModifier().children.insert(sharedNoDownload.name, std::move(sharedNoDownload));
        fakeFolder.remoteModifier().find("A")->children.insert(aSubSharedNoDownload.name, std::move(aSubSharedNoDownload));

        const auto account = fakeFolder.account();

        QEventLoop localEventLoop;
        auto lsPropPromise = QPromise<OCC::PlaceholderCreateInfo>{};
        auto lsPropFuture = lsPropPromise.future();
        auto lsPropFutureWatcher = QFutureWatcher<OCC::PlaceholderCreateInfo>{};
        lsPropFutureWatcher.setFuture(lsPropFuture);

        QList<QString> receivedPaths;

        QObject::connect(&lsPropFutureWatcher, &QFutureWatcher<QStringList>::finished,
                            &localEventLoop, [&localEventLoop] () {
                                qInfo() << "ls prop finished";
                                localEventLoop.quit();
                            });

        QObject::connect(&lsPropFutureWatcher, &QFutureWatcher<QStringList>::resultReadyAt,
                            &localEventLoop, [&receivedPaths, &lsPropFutureWatcher] (int resultIndex) {
                                qInfo() << "ls prop result at index" << resultIndex;
                                const auto &newResultEntries = lsPropFutureWatcher.resultAt(resultIndex);
                                receivedPaths.append(newResultEntries.fullPath);
                            });

        fakeFolder.syncJournal().clearFileTable(); // pretend we only need to fetch placeholders

        account->listRemoteFolder(&lsPropPromise, remotePath, subPath, &fakeFolder.syncJournal());
        localEventLoop.exec();

        receivedPaths.sort();
        expectedPaths.sort();
        QCOMPARE_EQ(receivedPaths, expectedPaths);
    }
};

QTEST_GUILESS_MAIN(TestAccount)
#include "testaccount.moc"
