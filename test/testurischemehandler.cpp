/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include <QtTest>

#include <QScopeGuard>
#include <QStandardPaths>
#include <QUrl>

#include "config.h"
#include "configfile.h"
#include "managedsettingstestutils.h"
#include "theme.h"
#include "urischemehandler.h"

using namespace OCC;
using namespace Qt::StringLiterals;

Q_DECLARE_METATYPE(OCC::UriSchemeHandler::Action)

class TestUriSchemeHandler : public QObject
{
    Q_OBJECT

    static auto clientUrl(const QString &path) -> QUrl
    {
        return QUrl{QString::fromLatin1(APPLICATION_URI_HANDLER_SCHEME) + u"://"_s + path};
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // Keeps tests off the device policy of the machine running them.
    void init()
    {
        ConfigFile::setDeviceSourcesFactory([] {
            return std::vector<std::unique_ptr<SettingSource>>{};
        });
    }

    void cleanup()
    {
        ConfigFile::setDeviceSourcesFactory({});
    }

    void parseUri_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<UriSchemeHandler::Action>("expectedAction");
        QTest::addColumn<QUrl>("expectedServerUrl");
        QTest::addColumn<bool>("expectError");

        auto foreignUrl = clientUrl(u"login/server:https://cloud.example.com"_s);
        foreignUrl.setScheme(foreignUrl.scheme() == u"nc"_s ? u"nextcloudfork"_s : u"nc"_s);
        QTest::newRow("foreign client scheme") << foreignUrl << UriSchemeHandler::Action::Invalid << QUrl{} << true;

        QTest::newRow("login") << clientUrl(u"login/server:https://cloud.example.com"_s) << UriSchemeHandler::Action::Login
                               << QUrl(QStringLiteral("https://cloud.example.com")) << false;

        QTest::newRow("login encoded server") << clientUrl(u"login/server:https%3A%2F%2Fcloud.example.com"_s) << UriSchemeHandler::Action::Login
                                              << QUrl(QStringLiteral("https://cloud.example.com")) << false;

        QTest::newRow("login uppercase host") << clientUrl(u"LOGIN/server:https://cloud.example.com"_s) << UriSchemeHandler::Action::Login
                                              << QUrl(QStringLiteral("https://cloud.example.com")) << false;

        QTest::newRow("login missing server prefix") << clientUrl(u"login/https://cloud.example.com"_s) << UriSchemeHandler::Action::Invalid << QUrl{} << true;

        QTest::newRow("login missing server url") << clientUrl(u"login/server:"_s) << UriSchemeHandler::Action::Invalid << QUrl{} << true;

        QTest::newRow("login relative server url") << clientUrl(u"login/server:cloud.example.com"_s) << UriSchemeHandler::Action::Invalid << QUrl{} << true;

        QTest::newRow("login hostless server url") << clientUrl(u"login/server:https://"_s) << UriSchemeHandler::Action::Invalid << QUrl{} << true;

        QTest::newRow("login unsupported server url scheme")
            << clientUrl(u"login/server:ftp://cloud.example.com"_s) << UriSchemeHandler::Action::Invalid << QUrl{} << true;

        QTest::newRow("old add account") << clientUrl(u"addAccount/?serverUrl=https%3A%2F%2Fcloud.example.com"_s) << UriSchemeHandler::Action::Invalid << QUrl{}
                                         << true;

        QTest::newRow("local edit") << clientUrl(u"open/admin@cloud.example.com/Documents/file.txt?token=secret"_s) << UriSchemeHandler::Action::OpenLocalEdit
                                    << QUrl{} << false;

        QTest::newRow("unknown action") << clientUrl(u"unknown/"_s) << UriSchemeHandler::Action::Invalid << QUrl{} << true;
    }

    void parseUri()
    {
        QFETCH(QUrl, url);
        QFETCH(UriSchemeHandler::Action, expectedAction);
        QFETCH(QUrl, expectedServerUrl);
        QFETCH(bool, expectError);

        auto theme = Theme::instance();
        theme->setOverrideServerUrl(QString{});
        theme->setForceOverrideServerUrl(false);
        ConfigFile{}.setOverrideServerUrl(QString{});

        const auto result = UriSchemeHandler::parseUri(url);

        QCOMPARE(result.action, expectedAction);
        QCOMPARE(result.serverUrl, expectedServerUrl);
        QCOMPARE(result.error.isEmpty(), !expectError);
    }

    void parseUriHonoursForcedSingleServerOverride()
    {
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreTheme = qScopeGuard([theme, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        theme->setOverrideServerUrl(QStringLiteral("https://cloud.example.com"));
        theme->setForceOverrideServerUrl(true);

        const auto acceptedResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://cloud.example.com/"_s));
        QCOMPARE(acceptedResult.action, UriSchemeHandler::Action::Login);
        QCOMPARE(acceptedResult.serverUrl, QUrl(QStringLiteral("https://cloud.example.com/")));
        QVERIFY(acceptedResult.error.isEmpty());

        const auto rejectedResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://unconfigured.example.com"_s));
        QCOMPARE(rejectedResult.action, UriSchemeHandler::Action::Invalid);
        QVERIFY(!rejectedResult.error.isEmpty());
    }

    void parseUriHonoursConfiguredServerOverride()
    {
        ConfigFile cfg;
        const auto previousConfiguredOverrideServerUrl = cfg.overrideServerUrl();
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreState = qScopeGuard([&cfg, theme, previousConfiguredOverrideServerUrl, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            cfg.setOverrideServerUrl(previousConfiguredOverrideServerUrl);
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        cfg.setOverrideServerUrl(QStringLiteral("https://configured.example.com"));
        theme->setOverrideServerUrl(QString{});
        theme->setForceOverrideServerUrl(false);

        const auto acceptedResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://configured.example.com"_s));
        QCOMPARE(acceptedResult.action, UriSchemeHandler::Action::Login);
        QCOMPARE(acceptedResult.serverUrl, QUrl(QStringLiteral("https://configured.example.com")));
        QVERIFY(acceptedResult.error.isEmpty());

        const auto rejectedResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://unconfigured.example.com"_s));
        QCOMPARE(rejectedResult.action, UriSchemeHandler::Action::Invalid);
        QVERIFY(!rejectedResult.error.isEmpty());
    }

    void parseUriRestrictsOnlyForLockedManagedServerOverride_data()
    {
        QTest::addColumn<bool>("enforced");

        QTest::newRow("device default") << false;
        QTest::newRow("device policy") << true;
    }

    void parseUriRestrictsOnlyForLockedManagedServerOverride()
    {
        QFETCH(bool, enforced);

        ConfigFile cfg;
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreTheme = qScopeGuard([theme, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        cfg.clearOverrideServerUrl();
        theme->setOverrideServerUrl(QString{});
        theme->setForceOverrideServerUrl(false);
        ConfigFile::setDeviceSourcesFactory([enforced] {
            std::vector<std::unique_ptr<SettingSource>> sources;
            sources.push_back(std::make_unique<MapSource>(enforced ? SettingSourceType::PlatformPolicy : SettingSourceType::PlatformDefault,
                                                          enforced ? EnforcementState::Enforced : EnforcementState::NotEnforced,
                                                          enforced ? 200 : 20,
                                                          QVariantMap{{u"overrideServerUrl"_s, u"https://configured.example.com"_s}}));
            return sources;
        });

        const auto configuredResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://configured.example.com"_s));
        QCOMPARE(configuredResult.action, UriSchemeHandler::Action::Login);

        const auto otherResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://unconfigured.example.com"_s));
        QCOMPARE(otherResult.action, enforced ? UriSchemeHandler::Action::Invalid : UriSchemeHandler::Action::Login);
    }

    void parseUriHonoursMultipleServerOverrides()
    {
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreTheme = qScopeGuard([theme, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        theme->setOverrideServerUrl(
            QStringLiteral("["
                           R"({"name": "Primary", "url": "https://primary.example.com"},)"
                           R"({"name": "Secondary", "url": "https://secondary.example.com"})"
                           "]"));
        theme->setForceOverrideServerUrl(true);

        const auto acceptedResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://secondary.example.com"_s));
        QCOMPARE(acceptedResult.action, UriSchemeHandler::Action::Login);
        QCOMPARE(acceptedResult.serverUrl, QUrl(QStringLiteral("https://secondary.example.com")));
        QVERIFY(acceptedResult.error.isEmpty());

        const auto rejectedResult = UriSchemeHandler::parseUri(clientUrl(u"login/server:https://unconfigured.example.com"_s));
        QCOMPARE(rejectedResult.action, UriSchemeHandler::Action::Invalid);
        QVERIFY(!rejectedResult.error.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestUriSchemeHandler)
#include "testurischemehandler.moc"
