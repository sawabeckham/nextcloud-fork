/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "common/forkdomaindisplayname.h"
#include <cstdlib>

int main()
{
    using namespace Qt::StringLiterals;
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
        forkDomainDisplayName(u"files.example.org"_s, 8443, fallback, false, u"  Рабочие файлы  "_s, true) == u"Рабочие файлы"_s,
        forkDomainDisplayName(u"files.example.org"_s, -1, fallback, false, u"Documents.app"_s, false) == u"files.example.org"_s,
        forkDomainDisplayName(u"cloud.example.app"_s, -1, fallback, true, u"Documents.app"_s, false) == fallback,
        forkDomainDisplayName(u"files.example.org"_s, -1, fallback, false, u"  "_s, true) == u"files.example.org"_s,
        forkDomainDisplayName(u"cloud.example.app"_s, -1, fallback, true, u"Documents"_s, true) == u"Documents"_s,
    };
    for (const auto passed : checks) {
        if (!passed) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
