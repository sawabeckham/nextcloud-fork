# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Tests for safe local publication of versioned fork installer packages."""

import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY / "admin/fork/publish-installer.py"
SPEC = importlib.util.spec_from_file_location("publish_installer", SCRIPT)
PUBLISHER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = PUBLISHER
SPEC.loader.exec_module(PUBLISHER)


class PublishInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.source_dir = self.root / "build output"
        self.source_dir.mkdir()
        self.destination_dir = self.root / "pkg"

    def tearDown(self):
        self.temporary.cleanup()

    def source(self, version="0.7.0", data=b"signed package bytes"):
        source = self.source_dir / f"Nextcloud-Fork-{version}-arm64.pkg"
        source.write_bytes(data)
        return source

    def test_copies_exact_bytes_and_reports_sha256(self):
        payload = b"PKG bytes\x00with binary data" * 23
        source = self.source(data=payload)

        result = PUBLISHER.publish(source, self.destination_dir)

        destination = self.destination_dir / source.name
        self.assertEqual(destination.read_bytes(), payload)
        self.assertEqual(result["destination"], str(destination))
        self.assertEqual(result["sha256"], hashlib.sha256(payload).hexdigest())
        self.assertEqual(result["removed"], [])

    def test_same_version_and_same_bytes_is_idempotent(self):
        source = self.source()
        first = PUBLISHER.publish(source, self.destination_dir)
        second = PUBLISHER.publish(source, self.destination_dir)

        self.assertEqual(second, first)
        self.assertEqual(sorted(path.name for path in self.destination_dir.iterdir()), [source.name])

    def test_same_version_with_different_bytes_is_conflict(self):
        source = self.source()
        PUBLISHER.publish(source, self.destination_dir)
        source.write_bytes(b"different bytes")

        with self.assertRaisesRegex(PUBLISHER.PublishError, "immutable"):
            PUBLISHER.publish(source, self.destination_dir)
        self.assertEqual((self.destination_dir / source.name).read_bytes(), b"signed package bytes")

    def test_rollover_retains_current_and_previous_version(self):
        for version in ("0.6.8", "0.6.9"):
            path = self.destination_dir / f"Nextcloud-Fork-{version}-arm64.pkg"
            self.destination_dir.mkdir(exist_ok=True)
            path.write_bytes(version.encode())
        source = self.source("0.7.0")

        result = PUBLISHER.publish(source, self.destination_dir)

        self.assertEqual(set(result["removed"]), {"Nextcloud-Fork-0.6.8-arm64.pkg"})
        self.assertEqual(
            {path.name for path in self.destination_dir.iterdir()},
            {"Nextcloud-Fork-0.6.9-arm64.pkg", "Nextcloud-Fork-0.7.0-arm64.pkg"},
        )

    def test_refuses_older_version_without_removing_newer_packages(self):
        self.destination_dir.mkdir()
        newest = self.destination_dir / "Nextcloud-Fork-0.7.0-arm64.pkg"
        newest.write_bytes(b"newer")
        source = self.source("0.6.9")

        with self.assertRaisesRegex(PUBLISHER.PublishError, "older"):
            PUBLISHER.publish(source, self.destination_dir)
        self.assertEqual(newest.read_bytes(), b"newer")
        self.assertEqual(list(self.destination_dir.iterdir()), [newest])

    def test_preserves_unrelated_files_and_symlink_package_candidates(self):
        self.destination_dir.mkdir()
        unrelated = self.destination_dir / "notes.txt"
        unrelated.write_text("keep")
        target = self.root / "keep-target"
        target.write_text("symlink target")
        link = self.destination_dir / "Nextcloud-Fork-0.6.8-arm64.pkg"
        link.symlink_to(target)
        self.source("0.6.9")

        PUBLISHER.publish(self.source("0.7.0"), self.destination_dir)

        self.assertEqual(unrelated.read_text(), "keep")
        self.assertTrue(link.is_symlink())
        self.assertEqual(target.read_text(), "symlink target")

    def test_rejects_invalid_or_empty_source(self):
        invalid_name = self.source("0.10.0")
        with self.assertRaisesRegex(PUBLISHER.PublishError, "source name"):
            PUBLISHER.publish(invalid_name, self.destination_dir)

        empty = self.source("0.7.1", data=b"")
        with self.assertRaisesRegex(PUBLISHER.PublishError, "empty"):
            PUBLISHER.publish(empty, self.destination_dir)

    def test_rejects_symlink_source_destination_directory_and_destination_file(self):
        real_source = self.source()
        symlink_source = self.source_dir / "Nextcloud-Fork-0.7.1-arm64.pkg"
        symlink_source.symlink_to(real_source)
        with self.assertRaisesRegex(PUBLISHER.PublishError, "regular"):
            PUBLISHER.publish(symlink_source, self.destination_dir)

        real_directory = self.root / "real-pkg"
        real_directory.mkdir()
        symlink_directory = self.root / "pkg-link"
        symlink_directory.symlink_to(real_directory, target_is_directory=True)
        with self.assertRaisesRegex(PUBLISHER.PublishError, "directory"):
            PUBLISHER.publish(real_source, symlink_directory)

        self.destination_dir.mkdir()
        (self.destination_dir / real_source.name).symlink_to(real_source)
        with self.assertRaisesRegex(PUBLISHER.PublishError, "not a regular file"):
            PUBLISHER.publish(real_source, self.destination_dir)

    def test_cli_emits_json_and_cleans_temporary_file(self):
        source = self.source()
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), "--source", str(source), "--directory", str(self.destination_dir)],
            check=True,
            capture_output=True,
            text=True,
        )

        result = json.loads(completed.stdout)
        self.assertEqual(result["destination"], str(self.destination_dir / source.name))
        self.assertEqual(result["sha256"], hashlib.sha256(source.read_bytes()).hexdigest())
        self.assertFalse(any(path.name.startswith(".nextcloud-fork-pkg-") for path in self.destination_dir.iterdir()))


if __name__ == "__main__":
    unittest.main()
