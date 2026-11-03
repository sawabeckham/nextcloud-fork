/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "accountfwd.h"
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace OCC
{
/** Edits a local connection label without changing the server URL or credentials. */
class ConnectionNameEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionNameEditor(const AccountPtr &account, QWidget *parent = nullptr);
    void showFinderUpdateError(const QString &message);

private:
    [[nodiscard]] static QString localizedText(const char *source, const char *russian);
    void updateActions();
    void saveName();
    [[nodiscard]] bool isNameSupported() const;

    AccountPtr _account;
    QLineEdit *_nameEdit;
    QLabel *_validationLabel;
    QPushButton *_saveButton;
    QPushButton *_resetButton;
};
}
