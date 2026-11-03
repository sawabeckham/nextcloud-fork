/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "account.h"
#include "connectionnameeditor.h"
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QtTest>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestConnectionNameEditor : public QObject
{
    Q_OBJECT
    static AccountPtr createAccount()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(u"https://cloud.example.invalid"_s));
        return account;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }
    void testConnectionNameEditorApplyResetAndReopen()
    {
        const auto account = createAccount();
        const auto originalUrl = account->url();
        const auto originalIdentity = account->userIdAtHostWithPort();
        {
            ConnectionNameEditor editor(account);
            const auto name = editor.findChild<QLineEdit *>(u"connectionNameEdit"_s);
            const auto save = editor.findChild<QPushButton *>(u"saveConnectionNameButton"_s);
            const auto reset = editor.findChild<QPushButton *>(u"resetConnectionNameButton"_s);
            QVERIFY(name);
            QVERIFY(save);
            QVERIFY(reset);
            QVERIFY(!save->isEnabled());
            name->setText(u"  Рабочий сервер  "_s);
            QVERIFY(account->connectionName().isEmpty());
            QVERIFY(save->isEnabled());
            save->click();
            QCOMPARE(account->connectionName(), u"Рабочий сервер"_s);
            QCOMPARE(name->text(), account->connectionName());
            QVERIFY(!save->isEnabled());
            reset->click();
            QCOMPARE(account->connectionName(), u"Рабочий сервер"_s);
            QVERIFY(name->text().isEmpty());
            save->click();
            QVERIFY(account->connectionName().isEmpty());
            QCOMPARE(account->url(), originalUrl);
            QCOMPARE(account->userIdAtHostWithPort(), originalIdentity);
        }
        ConnectionNameEditor reopened(account);
        const auto name = reopened.findChild<QLineEdit *>(u"connectionNameEdit"_s);
        QVERIFY(name);
        QVERIFY(name->text().isEmpty());
    }

    void testConnectionNameEditorRejectsInvalidInputAndDiscardsDraft()
    {
        const auto account = createAccount();
        QVERIFY(account->setConnectionName(u"Home"_s));
        {
            QDialog dialog;
            const auto editor = new ConnectionNameEditor(account, &dialog);
            const auto name = editor->findChild<QLineEdit *>(u"connectionNameEdit"_s);
            const auto save = editor->findChild<QPushButton *>(u"saveConnectionNameButton"_s);
            QVERIFY(name);
            QVERIFY(save);
            name->setText(u"folder/name"_s);
            QVERIFY(!save->isEnabled());
            save->click();
            QCOMPARE(account->connectionName(), u"Home"_s);
            name->setText(u"Unsaved draft"_s);
            QVERIFY(save->isEnabled());
            dialog.reject();
        }
        ConnectionNameEditor reopened(account);
        QCOMPARE(reopened.findChild<QLineEdit *>(u"connectionNameEdit"_s)->text(), u"Home"_s);
    }

    void finderFailureIsShownAsPlainText()
    {
        ConnectionNameEditor editor(createAccount());
        editor.showFinderUpdateError(u"Finder could not confirm this name <retry>"_s);
        const auto label = editor.findChild<QLabel *>(u"connectionNameValidation"_s);
        QVERIFY(label);
        QCOMPARE(label->textFormat(), Qt::PlainText);
        QVERIFY(!label->isHidden());
        QVERIFY(label->text().contains(u"<retry>"));
    }

#if defined(Q_OS_MACOS) && defined(BUILD_FILE_PROVIDER_MODULE)
    void packageExtensionCannotBeSaved()
    {
        const auto account = createAccount();
        ConnectionNameEditor editor(account);
        const auto name = editor.findChild<QLineEdit *>(u"connectionNameEdit"_s);
        const auto save = editor.findChild<QPushButton *>(u"saveConnectionNameButton"_s);
        QVERIFY(name);
        QVERIFY(save);
        name->setText(u"Home.app"_s);
        QVERIFY(!save->isEnabled());
        save->click();
        QVERIFY(account->connectionName().isEmpty());
    }
#endif
};
QTEST_MAIN(TestConnectionNameEditor)
#include "testconnectionnameeditor.moc"
