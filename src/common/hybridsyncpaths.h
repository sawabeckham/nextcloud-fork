/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>

namespace OCC::Utility
{
// Conservative ASCII case folding also protects the fixed Library/CloudStorage components.
[[nodiscard]] inline auto hybridPathComponent(std::filesystem::path component) -> std::string
{
    auto value = component.string();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : static_cast<char>(ch);
    });
    return value;
}

[[nodiscard]] inline auto hybridPathsDisjoint(const std::filesystem::path &local, const std::filesystem::path &managed) -> bool
{
    if (!local.is_absolute() || !managed.is_absolute()) {
        return false;
    }
    auto error = std::error_code{};
    const auto localPath = std::filesystem::weakly_canonical(local, error).lexically_normal();
    if (error) {
        return false;
    }
    const auto managedPath = std::filesystem::weakly_canonical(managed, error).lexically_normal();
    if (error) {
        return false;
    }
    auto a = localPath.begin();
    auto b = managedPath.begin();
    for (; a != localPath.end() && b != managedPath.end(); ++a, ++b) {
        if (hybridPathComponent(*a) != hybridPathComponent(*b)) {
            return true;
        }
    }
    return false;
}
} // namespace OCC::Utility
