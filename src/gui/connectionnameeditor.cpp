/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "connectionnameeditor.h"
#include "account.h"
#include "config.h"

#if defined(Q_OS_MACOS) && defined(BUILD_FILE_PROVIDER_MODULE)
#include "macOS/fileproviderdomainmanager.h"
#endif

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace OCC
{
ConnectionNameEditor::ConnectionNameEditor(const AccountPtr &account, QWidget *parent)
    : QWidget(parent)
    , _account(account)
    , _nameEdit(new QLineEdit(account->connectionName(), this))
    , _validationLabel(new QLabel(this))
    , _saveButton(new QPushButton(localizedText("Save", "Сохранить"), this))
    , _resetButton(new QPushButton(localizedText("Use server address", "Использовать адрес сервера"), this))
{
    setObjectName("connectionNameEditor"_L1);
    _nameEdit->setObjectName("connectionNameEdit"_L1);
    _nameEdit->setPlaceholderText(account->url().host());
    _validationLabel->setObjectName("connectionNameValidation"_L1);
    _validationLabel->setTextFormat(Qt::PlainText);
    _validationLabel->setWordWrap(true);
    _saveButton->setObjectName("saveConnectionNameButton"_L1);
    _resetButton->setObjectName("resetConnectionNameButton"_L1);
    _saveButton->setAutoDefault(false);
    _resetButton->setAutoDefault(false);

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto title = new QLabel(localizedText("Name in Finder", "Название в Finder"), this);
    title->setBuddy(_nameEdit);
    layout->addWidget(title);
    const auto inputRow = new QHBoxLayout;
    _nameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _nameEdit->setAccessibleName(title->text());
    inputRow->addWidget(_nameEdit, 1);
    inputRow->addWidget(_saveButton);
    layout->addLayout(inputRow);
    const auto help = new QLabel(localizedText("Enter a name, then save it. Leave the field empty to show the server address.",
                                               "Введите название и нажмите «Сохранить». Пустое поле — адрес сервера."),
                                 this);
    help->setWordWrap(true);
    layout->addWidget(help);
    layout->addWidget(_validationLabel);
    const auto actions = new QHBoxLayout;
    actions->addWidget(_resetButton);
    actions->addStretch();
    layout->addLayout(actions);

    connect(_nameEdit, &QLineEdit::textChanged, this, &ConnectionNameEditor::updateActions);
    connect(_saveButton, &QPushButton::clicked, this, &ConnectionNameEditor::saveName);
    connect(_resetButton, &QPushButton::clicked, _nameEdit, &QLineEdit::clear);
    connect(_account.data(), &Account::connectionNameChanged, this, [this] {
        if (!_nameEdit->isModified()) {
            _nameEdit->setText(_account->connectionName());
        }
        updateActions();
    });
    updateActions();
}

QString ConnectionNameEditor::localizedText(const char *source, const char *russian)
{
    // Use the language selected by Application, including an explicit user override.
    const auto language = qApp->property("ui_lang").toString();
    if (!language.isEmpty() && QLocale(language).language() == QLocale::Russian) {
        return QString::fromUtf8(russian);
    }
    return tr(source);
}

bool ConnectionNameEditor::isNameSupported() const
{
    if (!Account::isValidConnectionName(_nameEdit->text())) {
        return false;
    }
#if defined(Q_OS_MACOS) && defined(BUILD_FILE_PROVIDER_MODULE)
    return Mac::FileProviderDomainManager::isConnectionNameSupported(_nameEdit->text().trimmed());
#else
    return true;
#endif
}

void ConnectionNameEditor::updateActions()
{
    const auto supported = isNameSupported();
    _validationLabel->setText(supported
                                  ? QString{}
                                  : localizedText("Choose a shorter name without file separators, line breaks, or an application or document extension.",
                                                  "Выберите короткое название без разделителей пути, переносов строк и расширений приложений или документов."));
    _validationLabel->setVisible(!supported);
    _saveButton->setEnabled(supported && _nameEdit->text().trimmed() != _account->connectionName());
    _resetButton->setEnabled(!_nameEdit->text().isEmpty());
}

void ConnectionNameEditor::saveName()
{
    if (!isNameSupported() || !_account->setConnectionName(_nameEdit->text())) {
        updateActions();
        return;
    }
    _nameEdit->setText(_account->connectionName());
    _nameEdit->setModified(false);
    updateActions();
}

void ConnectionNameEditor::showFinderUpdateError(const QString &message)
{
    _validationLabel->setText(message);
    _validationLabel->show();
}
}
