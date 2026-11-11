#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Package a signed fork bundle without modifying it; uses Apple's packaging tools."""

import argparse
from pathlib import Path
import plistlib
import re
import stat
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
APP_NAME = "Nextcloud Fork.app"
BUNDLE_ID = "site.manro.nextcloudfork"
INSTALL_LOCATION = "/Applications/RomanenkoApp"
PACKAGE_ID = f"{BUNDLE_ID}.installer"
MACHO_MAGICS = frozenset((
    bytes.fromhex("feedface"), bytes.fromhex("cefaedfe"),
    bytes.fromhex("feedfacf"), bytes.fromhex("cffaedfe"),
    bytes.fromhex("cafebabe"), bytes.fromhex("bebafeca"),
    bytes.fromhex("cafebabf"), bytes.fromhex("bfbafeca"),
))


def fork_version():
    content = (ROOT / "FORK_VERSION.cmake").read_text()
    match = re.search(r'set\(NEXTCLOUD_FORK_VERSION "([0-9]\.[0-9]\.[0-9])"\)', content)
    if not match:
        raise ValueError("FORK_VERSION.cmake must contain three single-digit components")
    return match[1]


def validate_arm64_architectures(app):
    """Inspect every physical Mach-O; internal framework aliases share those files."""
    bundle_root = app.resolve(strict=True)
    pending = [app]
    while pending:
        directory = pending.pop()
        for path in sorted(directory.iterdir()):
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode):
                try:
                    target = path.resolve(strict=True)
                except (OSError, RuntimeError) as error:
                    raise ValueError(f"Unresolvable bundle symlink: {path}") from error
                if not target.is_relative_to(bundle_root):
                    raise ValueError(f"Bundle symlink points outside the app: {path}")
                continue
            if stat.S_ISDIR(mode):
                pending.append(path)
                continue
            if not stat.S_ISREG(mode):
                raise ValueError(f"Unsupported bundle file: {path}")
            with path.open("rb") as stream:
                if stream.read(4) not in MACHO_MAGICS:
                    continue
            try:
                result = subprocess.run(["/usr/bin/lipo", "-archs", str(path)],
                                        check=True, capture_output=True, text=True)
            except (OSError, subprocess.CalledProcessError) as error:
                raise ValueError(f"Could not inspect Mach-O architectures: {path}") from error
            architectures = result.stdout.split()
            if architectures != ["arm64"]:
                raise ValueError(f"Expected only arm64 in {path}; found: {' '.join(architectures) or 'unknown'}")


def validate_bundle(app, version):
    if app.name != APP_NAME or app.is_symlink():
        raise ValueError(f"Expected a real {APP_NAME} bundle")
    with (app / "Contents/Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    if info.get("CFBundleIdentifier") != BUNDLE_ID:
        raise ValueError("Refusing to package a different application")
    if f"fork{version}" not in info.get("CFBundleShortVersionString", ""):
        raise ValueError("Rebuild the app: its fork version does not match the installer")
    executable = info.get("CFBundleExecutable")
    if executable != "Nextcloud Fork" or not (app / "Contents/MacOS" / executable).is_file():
        raise ValueError("The app executable is missing or unexpected")
    for extension in ("FinderSyncExt", "FileProviderExt", "FileProviderUIExt"):
        extension_path = app / "Contents/PlugIns" / f"{extension}.appex"
        with (extension_path / "Contents/Info.plist").open("rb") as stream:
            extension_info = plistlib.load(stream)
        if extension_info.get("CFBundleIdentifier") != f"{BUNDLE_ID}.{extension}":
            raise ValueError(f"Unexpected identifier for {extension}")
    validate_arm64_architectures(app)
    subprocess.run(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(app)], check=True)


def build_installer(app, output, signing_identity=None):
    version = fork_version()
    app = Path(app).absolute()
    output = Path(output).absolute()
    if output.resolve().is_relative_to(app.resolve()):
        raise ValueError("The installer output must be outside the signed app")
    validate_bundle(app, version)
    if output.exists():
        raise FileExistsError(f"Output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="nextcloud-fork-installer-", dir=output.parent) as temporary:
        work = Path(temporary)
        payload = work / "payload"
        payload.mkdir()
        subprocess.run(["/usr/bin/ditto", str(app), str(payload / APP_NAME)], check=True)
        components = [{
            "RootRelativeBundlePath": APP_NAME,
            "BundleIsRelocatable": False,
            "BundleIsVersionChecked": True,
            "BundleHasStrictIdentifier": True,
            "BundleOverwriteAction": "upgrade",
        }]
        component_plist = work / "components.plist"
        component_plist.write_bytes(plistlib.dumps(components))
        component = work / "client.pkg"
        subprocess.run([
            "/usr/bin/pkgbuild", "--root", str(payload),
            "--component-plist", str(component_plist), "--identifier", PACKAGE_ID,
            "--version", version, "--install-location", INSTALL_LOCATION,
            "--ownership", "recommended", "--scripts", str(ROOT / "admin/fork/installer/scripts"),
            str(component),
        ], check=True)
        distribution = ET.Element("installer-gui-script", {"minSpecVersion": "2"})
        ET.SubElement(distribution, "title").text = f"Nextcloud Fork {version}"
        ET.SubElement(distribution, "options", {"customize": "never", "require-scripts": "false", "hostArchitectures": "arm64", "rootVolumeOnly": "true"})
        ET.SubElement(distribution, "domains", {"enable_anywhere": "false", "enable_currentUserHome": "false", "enable_localSystem": "true"})
        volume_check = ET.SubElement(distribution, "volume-check")
        ET.SubElement(volume_check, "allowed-os-versions").append(ET.Element("os-version", {"min": "13.0"}))
        ET.SubElement(distribution, "welcome", {"file": "Welcome.html"})
        ET.SubElement(distribution, "conclusion", {"file": "Conclusion.html"})
        choices = ET.SubElement(distribution, "choices-outline")
        ET.SubElement(choices, "line", {"choice": "client"})
        choice = ET.SubElement(distribution, "choice", {"id": "client", "visible": "false"})
        ET.SubElement(choice, "pkg-ref", {"id": PACKAGE_ID})
        ET.SubElement(distribution, "pkg-ref", {"id": PACKAGE_ID, "version": version, "onConclusion": "none"}).text = "client.pkg"
        distribution_path = work / "Distribution.xml"
        ET.ElementTree(distribution).write(distribution_path, encoding="utf-8", xml_declaration=True)
        product = work / "Nextcloud-Fork.pkg"
        command = ["/usr/bin/productbuild", "--distribution", str(distribution_path),
                   "--package-path", str(work), "--resources", str(ROOT / "admin/fork/installer/resources")]
        if signing_identity:
            command.extend(["--sign", signing_identity])
        subprocess.run([*command, str(product)], check=True)
        product.replace(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sign", help="Developer ID Installer identity, if available")
    args = parser.parse_args()
    print(build_installer(args.app, args.output, args.sign))


if __name__ == "__main__":
    main()
