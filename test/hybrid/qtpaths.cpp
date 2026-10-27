/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "gui/macOS/hybridsyncpaths.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdlib>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto home = OCC::Utility::getRealHomeDirectory();
    QTemporaryDir local("/tmp/nextcloud-fork-qt-XXXXXX");
    if (home.isEmpty() || !local.isValid() || !OCC::Utility::hybridSyncPathAllowed(local.path()) || OCC::Utility::hybridSyncPathAllowed(home)
        || OCC::Utility::hybridSyncPathAllowed(home + "/Library") || OCC::Utility::hybridSyncPathAllowed(home + "/Library/CloudStorage")
        || OCC::Utility::hybridSyncPathAllowed("relative")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
