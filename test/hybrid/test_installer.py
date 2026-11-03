#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import importlib.util
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("installer", ROOT / "admin/fork/make-installer.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="fork-installer-test-")
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)
        self.app = self.work / installer.APP_NAME
        self.bundle(self.app, installer.BUNDLE_ID, "Nextcloud Fork")
        for name in ("FinderSyncExt", "FileProviderExt", "FileProviderUIExt"):
            self.bundle(self.app / "Contents/PlugIns" / f"{name}.appex", f"{installer.BUNDLE_ID}.{name}", name)
        subprocess.run(["/usr/bin/codesign", "--force", "--deep", "--sign", "-", str(self.app)], check=True, capture_output=True)

    def bundle(self, path, identifier, executable):
        macos = path / "Contents/MacOS"
        macos.mkdir(parents=True)
        target = macos / executable
        target.write_text("#!/bin/sh\nexit 0\n")
        target.chmod(0o755)
        (path / "Contents/Info.plist").write_bytes(plistlib.dumps({
            "CFBundleIdentifier": identifier,
            "CFBundleExecutable": executable,
            "CFBundleVersion": "35.0.0.60",
            "CFBundleShortVersionString": f"35.0.0fork{installer.fork_version()}",
            "CFBundlePackageType": "APPL" if path.suffix == ".app" else "XPC!",
        }))

    def change_info(self, **updates):
        path = self.app / "Contents/Info.plist"
        info = plistlib.loads(path.read_bytes())
        info.update(updates)
        path.write_bytes(plistlib.dumps(info))

    def test_wrong_app_and_old_version_are_rejected(self):
        self.change_info(CFBundleIdentifier="com.nextcloud.desktopclient")
        with self.assertRaisesRegex(ValueError, "different application"):
            installer.validate_bundle(self.app, installer.fork_version())
        self.change_info(CFBundleIdentifier=installer.BUNDLE_ID, CFBundleShortVersionString="35.0.0daily")
        with self.assertRaisesRegex(ValueError, "Rebuild"):
            installer.validate_bundle(self.app, installer.fork_version())

    def test_missing_extension_is_rejected(self):
        extension_info = self.app / "Contents/PlugIns/FileProviderExt.appex/Contents/Info.plist"
        extension_info.unlink()
        with self.assertRaises(FileNotFoundError):
            installer.validate_bundle(self.app, installer.fork_version())

    def test_tampered_signature_is_rejected(self):
        (self.app / "Contents/MacOS/Nextcloud Fork").write_text("#!/bin/sh\nexit 1\n")
        with self.assertRaises(subprocess.CalledProcessError):
            installer.validate_bundle(self.app, installer.fork_version())

    def test_real_package_destination_and_payload(self):
        output = self.work / "folder with spaces/Nextcloud Fork.pkg"
        installer.build_installer(self.app, output)
        expanded = self.work / "expanded"
        subprocess.run(["/usr/sbin/pkgutil", "--expand-full", str(output), str(expanded)], check=True)
        distribution = ET.parse(expanded / "Distribution").getroot()
        self.assertEqual(distribution.find("domains").get("enable_localSystem"), "true")
        self.assertEqual(distribution.find("options").get("hostArchitectures"), "arm64")
        component = expanded / "client.pkg"
        package_info = ET.parse(component / "PackageInfo").getroot()
        self.assertEqual(package_info.get("identifier"), installer.PACKAGE_ID)
        self.assertEqual(package_info.get("version"), installer.fork_version())
        self.assertEqual(package_info.get("install-location"), installer.INSTALL_LOCATION)
        self.assertEqual(package_info.get("relocatable"), "false")
        self.assertEqual(list(package_info.find("relocate")), [])
        self.assertEqual(package_info.find("upgrade-bundle/bundle").get("id"), installer.BUNDLE_ID)
        payload_app = component / "Payload" / installer.APP_NAME
        subprocess.run(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(payload_app)], check=True)
        self.assertEqual((self.app / "Contents/Info.plist").read_bytes(), (payload_app / "Contents/Info.plist").read_bytes())
        self.assertTrue((component / "Scripts/preinstall").stat().st_mode & 0o111)
        self.assertTrue((component / "Scripts/postinstall").stat().st_mode & 0o111)
        with self.assertRaises(FileExistsError):
            installer.build_installer(self.app, output)

    def test_output_inside_signed_app_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside the signed app"):
            installer.build_installer(self.app, self.app / "installer.pkg")

    def test_symlink_bundle_is_rejected(self):
        other = self.work / "alias" / installer.APP_NAME
        other.parent.mkdir()
        other.symlink_to(self.app, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "real"):
            installer.validate_bundle(other, installer.fork_version())

    def test_nonstartup_volume_is_rejected_before_installing(self):
        result = subprocess.run([
            str(ROOT / "admin/fork/installer/scripts/preinstall"), "", "", "/Volumes/Fork-test",
        ], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("startup volume", result.stderr)

    def test_version_components_follow_fork_rule(self):
        version_file = self.work / "FORK_VERSION.cmake"
        with patch.object(installer, "ROOT", self.work):
            version_file.write_text('set(NEXTCLOUD_FORK_VERSION "1.7.9")\n')
            self.assertEqual(installer.fork_version(), "1.7.9")
            version_file.write_text('set(NEXTCLOUD_FORK_VERSION "1.7.10")\n')
            with self.assertRaisesRegex(ValueError, "single-digit"):
                installer.fork_version()


if __name__ == "__main__":
    unittest.main()
