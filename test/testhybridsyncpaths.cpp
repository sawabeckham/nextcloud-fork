/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "common/hybridsyncpaths.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace
{
void require(bool condition)
{
    if (!condition) {
        std::abort();
    }
}
}

int main()
{
    namespace fs = std::filesystem;
    using OCC::Utility::hybridPathsDisjoint;
    auto pattern = std::string("/tmp/nextcloud-fork-paths-XXXXXX");
    const auto temp = mkdtemp(pattern.data());
    require(temp != nullptr);
    const auto root = fs::path(temp);
    const auto managed = root / "Library" / "CloudStorage";
    fs::create_directories(managed / "Nextcloud");
    fs::create_directories(root / "Projects");
    require(hybridPathsDisjoint(root / "Projects", managed));
    require(hybridPathsDisjoint(root / "CloudStorage-backup", managed));
    require(hybridPathsDisjoint(root / "Projects" / "new", managed));
    require(!hybridPathsDisjoint(root, managed));
    require(!hybridPathsDisjoint(managed, managed));
    require(!hybridPathsDisjoint(managed / "Nextcloud", managed));
    require(!hybridPathsDisjoint(root / "Library", managed));
    require(!hybridPathsDisjoint(root / "Library" / "CloudStorage" / "Nextcloud" / "..", managed));
    require(!hybridPathsDisjoint("", managed));
    require(!hybridPathsDisjoint("relative", managed));
    require(!hybridPathsDisjoint(root, "relative"));
    fs::create_directory_symlink(managed, root / "alias");
    require(!hybridPathsDisjoint(root / "alias" / "Nextcloud", managed));
    fs::create_directory_symlink(root, root / "parent-alias");
    require(!hybridPathsDisjoint(root / "parent-alias", managed));
    require(!hybridPathsDisjoint(root / "library" / "cloudstorage" / "new", managed));
    fs::create_directory_symlink(root / "loop", root / "loop");
    require(!hybridPathsDisjoint(root / "loop", managed));
    // Managed storage may itself be a symlink to another volume.
    fs::create_directories(root / "ExternalStorage" / "Nextcloud");
    fs::create_directory_symlink(root / "ExternalStorage", root / "moved-storage");
    require(!hybridPathsDisjoint(root / "ExternalStorage" / "Nextcloud", root / "moved-storage"));
    require(hybridPathsDisjoint(root / "Projects", root / "moved-storage"));
    fs::remove_all(root);
    std::puts("Hybrid sync path tests passed");
}
