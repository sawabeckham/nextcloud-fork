/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "common/hybridsyncpaths.h"
#include "common/utility_mac_sandbox.h"
#include <QDir>
#include <QString>

namespace OCC::Utility
{
[[nodiscard]] inline auto hybridSyncPathAllowed(const QString &path) -> bool
{
    const auto realHome = getRealHomeDirectory();
    if (realHome.isEmpty()) {
        return false;
    }
    const auto home = std::filesystem::path(realHome.toStdString());
    const auto local = std::filesystem::path(path.toStdString());
    // Reserve Library (including File Provider storage and app containers) even before
    // a domain is created. Ancestor folders, including the home itself, are rejected.
    return hybridPathsDisjoint(local, home / "Library") && hybridPathsDisjoint(local, home / "Library" / "CloudStorage")
        && hybridPathsDisjoint(local, std::filesystem::path(QDir::homePath().toStdString()) / "Library");
}
} // namespace OCC::Utility
