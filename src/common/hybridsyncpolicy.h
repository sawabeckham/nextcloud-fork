/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

namespace OCC::Utility
{
#ifdef NEXTCLOUD_EXPERIMENTAL_HYBRID_SYNC
inline constexpr auto hybridSyncEnabled = true;
#else
inline constexpr auto hybridSyncEnabled = false;
#endif

[[nodiscard]] constexpr auto classicSyncAllowed(bool fileProviderEnabled) -> bool
{
    return !fileProviderEnabled || hybridSyncEnabled;
}

[[nodiscard]] constexpr auto classicSyncConflict(bool fileProviderEnabled, bool hasClassicFolders) -> bool
{
    return fileProviderEnabled && hasClassicFolders && !hybridSyncEnabled;
}
} // namespace OCC::Utility
