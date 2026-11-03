/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QString>

namespace OCC::Utility
{
/**
 * @brief The Finder name: a supported local connection name, or the server address, `host[:port]`.
 *
 * Falls back to `fallback` (the `<server> - <user>` name of the official client) when the host is empty
 * or ends in an extension that LaunchServices treats as a bundle, such as ".app": the domain root
 * folder would then be shown as an application (see issues #7979 / #9684).
 */
[[nodiscard]] inline auto forkDomainDisplayName(const QString &host,
                                                int port,
                                                const QString &fallback,
                                                bool hostEndsInPackageExtension,
                                                const QString &connectionName = {},
                                                bool connectionNameSupported = false) -> QString
{
    if (connectionNameSupported && !connectionName.trimmed().isEmpty()) {
        return connectionName.trimmed();
    }
    if (host.isEmpty() || hostEndsInPackageExtension) {
        return fallback;
    }
    if (port > 0 && port != 80 && port != 443) {
        return host + QLatin1Char(':') + QString::number(port);
    }
    return host;
}
} // namespace OCC::Utility
