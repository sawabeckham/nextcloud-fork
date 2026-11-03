/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../../../shell_integration/MacOSX/NextcloudFileProviderKit/Sources/NextcloudFileProviderXPC/include/ForkFileProviderDomain.h"
#include "config.h"
#include "gui/macOS/fileproviderdomainmanager.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QtTest>

#include <atomic>
#include <memory>

#import <FileProvider/FileProvider.h>
#import <Foundation/Foundation.h>

using namespace Qt::StringLiterals;

class TestFileProviderIsolation : public QObject
{
    Q_OBJECT

    static constexpr auto pendingDomainCount = qsizetype{-1};
    static constexpr auto domainQueryTimeoutMs = 10000;

    static auto bundleIdentifier() -> QString
    {
        return QString::fromNSString(NSBundle.mainBundle.bundleIdentifier);
    }

    static auto expectedIdentifier() -> QString
    {
        return QString::fromLatin1(APPLICATION_REV_DOMAIN) + u".tests.FileProviderIsolationTest"_s;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void bundleIdentityIsSeparateFromTheClient()
    {
        QCOMPARE(bundleIdentifier(), expectedIdentifier());
    }

    void bundleExecutableMatchesTheTestProcess()
    {
        const auto executable = QString::fromNSString([NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleExecutable"]);
        QCOMPARE(executable, QFileInfo(QCoreApplication::applicationFilePath()).fileName());
    }

    void auxiliaryIdentityCannotMatchARealDomain()
    {
        using OCC::Mac::FileProviderDomainManager;
        const auto auxiliaryIdentifier = QString::fromLatin1(NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER);
#ifdef NEXTCLOUD_FORK
        QVERIFY(FileProviderDomainManager::isAuxiliaryDomainIdentifier(auxiliaryIdentifier));
#else
        QVERIFY(!FileProviderDomainManager::isAuxiliaryDomainIdentifier(auxiliaryIdentifier));
#endif
        QVERIFY(!FileProviderDomainManager::isAuxiliaryDomainIdentifier({}));
        QVERIFY(!FileProviderDomainManager::isAuxiliaryDomainIdentifier(u"00000000-0000-4000-8000-000000000001"_s));
        QVERIFY(!FileProviderDomainManager::isAuxiliaryDomainIdentifier(auxiliaryIdentifier + u".other"_s));
    }

    void nativeDomainQueryCannotDiscoverClientDomains()
    {
        // Fail before calling the native API if this executable still impersonates the client.
        QCOMPARE(bundleIdentifier(), expectedIdentifier());
        const auto count = std::make_shared<std::atomic<qsizetype>>(pendingDomainCount);
        [NSFileProviderManager
            getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *domains, NSError *) { count->store(static_cast<qsizetype>(domains.count)); }];
        QTRY_VERIFY_WITH_TIMEOUT(count->load() != pendingDomainCount, domainQueryTimeoutMs);
        QCOMPARE(count->load(), qsizetype{0});
    }
};

QTEST_GUILESS_MAIN(TestFileProviderIsolation)
#include "testfileproviderisolation.moc"
