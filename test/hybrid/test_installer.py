#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import importlib.util
from pathlib import Path
import plistlib
import shutil
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
    @classmethod
    def setUpClass(cls):
        cls.binaries = tempfile.TemporaryDirectory(prefix="fork-installer-architectures-")
        cls.addClassCleanup(cls.binaries.cleanup)
        cls.binary_dir = Path(cls.binaries.name)
        source = cls.binary_dir / "fixture.c"
        source.write_text("int installer_architecture_fixture(void) { return 0; }\n")
        for architecture in ("arm64", "x86_64", "arm64e"):
            subprocess.run([
                "/usr/bin/clang", "-arch", architecture, "-dynamiclib", str(source),
                "-o", str(cls.binary_dir / architecture),
            ], check=True, capture_output=True)
        subprocess.run([
            "/usr/bin/lipo", "-create", str(cls.binary_dir / "arm64"),
            str(cls.binary_dir / "x86_64"), "-output", str(cls.binary_dir / "universal"),
        ], check=True, capture_output=True)

    def install_binary(self, architecture, relative_path):
        destination = self.app / relative_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(self.binary_dir / architecture, destination)
        for parent in destination.parents:
            if parent.suffix == ".framework":
                resources = parent / "Versions/A/Resources"
                resources.mkdir(parents=True, exist_ok=True)
                (resources / "Info.plist").write_bytes(plistlib.dumps({
                    "CFBundleIdentifier": "site.manro.nextcloudfork.tests.fixture",
                    "CFBundleExecutable": "Fixture",
                    "CFBundlePackageType": "FMWK",
                    "CFBundleVersion": "1",
                }))
                (parent / "Versions/Current").symlink_to("A", target_is_directory=True)
                (parent / "Fixture").symlink_to("Versions/Current/Fixture")
                (parent / "Resources").symlink_to("Versions/Current/Resources", target_is_directory=True)
                break
        subprocess.run(["/usr/bin/codesign", "--force", "--deep", "--sign", "-", str(self.app)],
                       check=True, capture_output=True)
        return destination

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

    def test_non_arm64_macho_is_rejected_recursively(self):
        for architecture, relative_path in (
            ("x86_64", "Contents/Frameworks/Fixture.framework/Versions/A/Fixture"),
            ("universal", "Contents/PlugIns/platforms/libfixture.dylib"),
            ("arm64e", "Contents/PlugIns/FileProviderExt.appex/Contents/Frameworks/libfixture.dylib"),
        ):
            with self.subTest(architecture=architecture):
                binary = self.install_binary(architecture, relative_path)
                with self.assertRaisesRegex(ValueError, "arm64"):
                    installer.validate_bundle(self.app, installer.fork_version())
                shutil.copyfile(self.binary_dir / "arm64", binary)

    def test_arm64_framework_aliases_and_non_macho_resources_are_accepted(self):
        self.install_binary("arm64", "Contents/Frameworks/Fixture.framework/Versions/A/Fixture")
        resources = self.app / "Contents/Resources"
        resources.mkdir()
        (resources / "plain.txt").write_text("A resource, not an executable.\n")
        subprocess.run(["/usr/bin/codesign", "--force", "--deep", "--sign", "-", str(self.app)],
                       check=True, capture_output=True)
        installer.validate_bundle(self.app, installer.fork_version())

    def test_external_and_dangling_symlinks_are_rejected(self):
        external = self.work / "external.dylib"
        shutil.copyfile(self.binary_dir / "x86_64", external)
        alias = self.app / "Contents/external.dylib"
        for target in (external, self.binary_dir, self.work / "missing.dylib"):
            with self.subTest(target=target.name):
                alias.symlink_to(target)
                with self.assertRaisesRegex(ValueError, "symlink"):
                    installer.validate_bundle(self.app, installer.fork_version())
                alias.unlink()

    def test_macho_inspection_failure_is_rejected(self):
        self.install_binary("arm64", "Contents/Frameworks/libfixture.dylib")
        for error in (subprocess.CalledProcessError(1, "/usr/bin/lipo"), OSError("lipo unavailable")):
            with self.subTest(error=type(error).__name__):
                with patch.object(installer.subprocess, "run", side_effect=error) as run:
                    with self.assertRaisesRegex(ValueError, "inspect Mach-O"):
                        installer.validate_bundle(self.app, installer.fork_version())
                    self.assertEqual(run.call_args.args[0][:2], ["/usr/bin/lipo", "-archs"])

    def test_unreadable_directory_is_rejected(self):
        directory = self.app / "Contents/unreadable"
        directory.mkdir()
        directory.chmod(0)
        try:
            with self.assertRaises(PermissionError):
                installer.validate_bundle(self.app, installer.fork_version())
        finally:
            directory.chmod(0o700)

    def test_malformed_macho_is_rejected(self):
        (self.app / "Contents/malformed.dylib").write_bytes(bytes.fromhex("cffaedfe"))
        with self.assertRaisesRegex(ValueError, "inspect Mach-O"):
            installer.validate_bundle(self.app, installer.fork_version())

    def test_real_package_destination_and_payload(self):
        self.install_binary("arm64", "Contents/Frameworks/libfixture.dylib")
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
        payload_binary = payload_app / "Contents/Frameworks/libfixture.dylib"
        self.assertEqual(subprocess.check_output(["/usr/bin/lipo", "-archs", str(payload_binary)], text=True).split(), ["arm64"])
        self.assertEqual(payload_binary.read_bytes(), (self.app / "Contents/Frameworks/libfixture.dylib").read_bytes())
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
