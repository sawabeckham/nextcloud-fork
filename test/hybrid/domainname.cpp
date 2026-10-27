/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "common/forkdomaindisplayname.h"
#include <cstdlib>

int main()
{
    using OCC::Utility::forkDomainDisplayName;
    const auto fallback = QStringLiteral("files.example.org - user");
    const auto checks = {
        forkDomainDisplayName(QStringLiteral("files.example.org"), -1, fallback, false) == QStringLiteral("files.example.org"),
        forkDomainDisplayName(QStringLiteral("files.example.org"), 443, fallback, false) == QStringLiteral("files.example.org"),
        forkDomainDisplayName(QStringLiteral("files.example.org"), 80, fallback, false) == QStringLiteral("files.example.org"),
        forkDomainDisplayName(QStringLiteral("files.example.org"), 8443, fallback, false) == QStringLiteral("files.example.org:8443"),
        // A host ending in ".app" or another package extension keeps the `<server> - <user>` name.
        forkDomainDisplayName(QStringLiteral("cloud.example.app"), -1, fallback, true) == fallback,
        forkDomainDisplayName(QString(), -1, fallback, false) == fallback,
    };
    for (const auto passed : checks) {
        if (!passed) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
