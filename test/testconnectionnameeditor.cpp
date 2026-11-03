/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "account.h"
#include "connectionnameeditor.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
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

    void previewEditor()
    {
        if (qEnvironmentVariableIntValue("NEXTCLOUD_FORK_CONNECTION_NAME_PREVIEW") != 1) {
            QSKIP("Set NEXTCLOUD_FORK_CONNECTION_NAME_PREVIEW=1 for the isolated visual preview.");
        }

        const auto account = createAccount();
        const auto originalUrl = account->url();
        const auto originalIdentity = account->userIdAtHostWithPort();
        QDialog dialog;
        dialog.setWindowTitle(u"Connection name — isolated test preview"_s);
        const auto layout = new QVBoxLayout(&dialog);
        layout->addWidget(new ConnectionNameEditor(account, &dialog));
        const auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        constexpr auto previewTimeoutMs = 60'000;
        QTimer::singleShot(previewTimeoutMs, &dialog, &QDialog::reject);
        dialog.exec();
        QCOMPARE(account->url(), originalUrl);
        QCOMPARE(account->userIdAtHostWithPort(), originalIdentity);
    }

    void renderEditorStates()
    {
        const auto outputPath = qEnvironmentVariable("NEXTCLOUD_FORK_CONNECTION_NAME_RENDER_DIR");
        if (outputPath.isEmpty()) {
            QSKIP("Set NEXTCLOUD_FORK_CONNECTION_NAME_RENDER_DIR for isolated widget render artifacts.");
        }
        const auto outputDirectory = QDir(outputPath);
        QVERIFY(QDir().mkpath(outputDirectory.absolutePath()));
        const auto account = createAccount();
        const auto originalUrl = account->url();
        const auto originalIdentity = account->userIdAtHostWithPort();
        QDialog dialog;
        dialog.setWindowTitle(u"Connection name — isolated render"_s);
        const auto layout = new QVBoxLayout(&dialog);
        const auto editor = new ConnectionNameEditor(account, &dialog);
        layout->addWidget(editor);
        const auto name = editor->findChild<QLineEdit *>(u"connectionNameEdit"_s);
        const auto save = editor->findChild<QPushButton *>(u"saveConnectionNameButton"_s);
        const auto reset = editor->findChild<QPushButton *>(u"resetConnectionNameButton"_s);
        const auto validation = editor->findChild<QLabel *>(u"connectionNameValidation"_s);
        QVERIFY(name);
        QVERIFY(save);
        QVERIFY(reset);
        QVERIFY(validation);
        constexpr auto renderWidth = 560;
        const auto capture = [&](const QString &fileName) {
            dialog.resize(renderWidth, dialog.sizeHint().height());
            dialog.show();
            QCoreApplication::processEvents();
            return dialog.grab().save(outputDirectory.filePath(fileName));
        };

        QVERIFY(account->connectionName().isEmpty());
        QVERIFY(name->text().isEmpty());
        QVERIFY(!save->isEnabled());
        QVERIFY(!reset->isEnabled());
        QVERIFY(capture(u"01-default.png"_s));

        name->setText(u"Рабочий сервер — документы"_s);
        QVERIFY(account->connectionName().isEmpty());
        QVERIFY(save->isEnabled());
        QVERIFY(capture(u"02-cyrillic-draft.png"_s));
        save->click();
        QCOMPARE(account->connectionName(), u"Рабочий сервер — документы"_s);
        QVERIFY(!save->isEnabled());
        QVERIFY(capture(u"03-cyrillic-saved.png"_s));

#if defined(Q_OS_MACOS) && defined(BUILD_FILE_PROVIDER_MODULE)
        name->setText(u"Home.app"_s);
#else
        name->setText(u"folder/name"_s);
#endif
        QVERIFY(!save->isEnabled());
        QVERIFY(!validation->isHidden());
        save->click();
        QCOMPARE(account->connectionName(), u"Рабочий сервер — документы"_s);
        QVERIFY(capture(u"04-invalid-name.png"_s));

        reset->click();
        QVERIFY(name->text().isEmpty());
        QVERIFY(save->isEnabled());
        QVERIFY(validation->isHidden());
        QVERIFY(capture(u"05-reset-draft.png"_s));
        save->click();
        QVERIFY(account->connectionName().isEmpty());
        QVERIFY(!save->isEnabled());
        QVERIFY(capture(u"06-reset-saved.png"_s));

        const auto finderError = u"Finder could not confirm this name. Please try again."_s;
        editor->showFinderUpdateError(finderError);
        QCOMPARE(validation->text(), finderError);
        QCOMPARE(validation->textFormat(), Qt::PlainText);
        QVERIFY(!validation->isHidden());
        QVERIFY(capture(u"07-finder-error.png"_s));
        QCOMPARE(account->url(), originalUrl);
        QCOMPARE(account->userIdAtHostWithPort(), originalIdentity);
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
