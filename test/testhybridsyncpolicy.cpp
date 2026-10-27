/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "common/hybridsyncpolicy.h"

int main()
{
    using namespace OCC::Utility;
    static_assert(classicSyncAllowed(false));
    static_assert(classicSyncAllowed(true) == hybridSyncEnabled);
    static_assert(!classicSyncConflict(false, false));
    static_assert(!classicSyncConflict(false, true));
    static_assert(!classicSyncConflict(true, false));
    static_assert(classicSyncConflict(true, true) == !hybridSyncEnabled);
}
