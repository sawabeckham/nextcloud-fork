# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Copy a verified fork installer into a local release directory safely."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import tempfile


_PKG_NAME = re.compile(r"^Nextcloud-Fork-(\d)\.(\d)\.(\d)-arm64\.pkg$")


class PublishError(Exception):
    """A validation or publication error suitable for a CLI message."""


def _own_packages(directory: Path) -> list[tuple[tuple[int, int, int], Path]]:
    packages = []
    for entry in directory.iterdir():
        match = _PKG_NAME.fullmatch(entry.name)
        if not match:
            continue
        try:
            mode = entry.lstat().st_mode
        except FileNotFoundError:
            continue
        if stat.S_ISREG(mode):
            packages.append((tuple(int(part) for part in match.groups()), entry))
    return packages


def _hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def publish(source: Path, directory: Path) -> dict[str, object]:
    """Safely publish *source* and retain the two newest own installer files."""
    try:
        source_mode = source.lstat().st_mode
    except OSError as error:
        raise PublishError(f"cannot inspect source: {error}") from error
    if not stat.S_ISREG(source_mode):
        raise PublishError("source must be a regular, non-symlink file")
    if source.stat().st_size == 0:
        raise PublishError("source must not be empty")

    match = _PKG_NAME.fullmatch(source.name)
    if not match:
        raise PublishError("source name must be Nextcloud-Fork-X.Y.Z-arm64.pkg with single-digit components")
    version = tuple(int(part) for part in match.groups())

    try:
        directory.mkdir(parents=True, exist_ok=True)
        directory_mode = directory.lstat().st_mode
    except OSError as error:
        raise PublishError(f"cannot create or inspect destination directory: {error}") from error
    if not stat.S_ISDIR(directory_mode):
        raise PublishError("destination directory must be a real directory, not a symlink")

    destination = directory / source.name
    try:
        dest_mode = destination.lstat().st_mode
    except FileNotFoundError:
        dest_mode = None
    except OSError as error:
        raise PublishError(f"cannot inspect destination file: {error}") from error
    if dest_mode is not None and not stat.S_ISREG(dest_mode):
        raise PublishError("destination path exists and is not a regular file")

    packages = _own_packages(directory)
    latest = max((candidate_version for candidate_version, _ in packages), default=None)
    if latest is not None and version < latest:
        raise PublishError("refusing to publish a version older than the newest existing installer")

    source_hash = _hash_file(source)
    if destination.exists():
        if _hash_file(destination) != source_hash:
            raise PublishError("destination already exists with different bytes; published packages are immutable")
        removed = _retain_two(directory)
        return {"destination": str(destination), "sha256": source_hash, "removed": removed}

    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(prefix=".nextcloud-fork-pkg-", dir=directory, delete=False) as temporary:
            temporary_path = Path(temporary.name)
            os.fchmod(temporary.fileno(), stat.S_IMODE(source_mode))
            with source.open("rb") as input_file:
                shutil.copyfileobj(input_file, temporary)
            temporary.flush()
            os.fsync(temporary.fileno())

        copied_hash = _hash_file(temporary_path)
        if copied_hash != source_hash:
            raise PublishError("copied bytes failed SHA-256 verification")

        # A same-directory hard link installs the complete file atomically and
        # fails if another publisher has already created this immutable name.
        os.link(temporary_path, destination)
    except FileExistsError as error:
        raise PublishError("destination appeared during publication; refusing to overwrite it") from error
    except OSError as error:
        raise PublishError(f"could not publish installer atomically: {error}") from error
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink()
            except FileNotFoundError:
                pass

    removed = _retain_two(directory)
    return {"destination": str(destination), "sha256": copied_hash, "removed": removed}


def _retain_two(directory: Path) -> list[str]:
    packages = _own_packages(directory)
    packages.sort(key=lambda item: item[0], reverse=True)
    retained = {path for _, path in packages[:2]}
    removed = []
    for _, path in packages:
        if path in retained:
            continue
        # lstat and unlink never follow a symlink if the entry changed since scan.
        try:
            if stat.S_ISREG(path.lstat().st_mode):
                path.unlink()
                removed.append(path.name)
        except FileNotFoundError:
            continue
    return removed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="verified PKG to publish")
    parser.add_argument("--directory", required=True, type=Path, help="local installer directory")
    arguments = parser.parse_args()
    try:
        result = publish(arguments.source, arguments.directory)
    except PublishError as error:
        parser.error(str(error))
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
