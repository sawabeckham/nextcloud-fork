# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check fork-only Finder naming through the configured plist build pipeline."""

import json
import pathlib
import plistlib
import re
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
INTEGRATION = ROOT / "shell_integration/MacOSX/NextcloudIntegration"
SETTING = "OC_FILE_PROVIDER_HIDE_EXTENSION_NAME"
KEY = "NSFileProviderHideExtensionName"


class FileProviderLocationProfileTests(unittest.TestCase):
    def test_fork_and_official_processed_plists(self):
        project = json.loads(subprocess.check_output([
            "plutil", "-convert", "json", "-o", "-",
            str(INTEGRATION / "NextcloudIntegration.xcodeproj/project.pbxproj"),
        ]))
        configurations = [
            entry["buildSettings"] for entry in project["objects"].values()
            if entry.get("isa") == "XCBuildConfiguration"
            and entry.get("buildSettings", {}).get("INFOPLIST_FILE") == "FileProviderExt/Info.plist"
        ]
        self.assertEqual(len(configurations), 2, "Cover Debug and Release")
        with tempfile.TemporaryDirectory(prefix="fork-location-profile-") as directory:
            source = pathlib.Path(directory) / "source"
            source.mkdir()
            # Evaluate the real MacOSX CMake file without compiling or signing targets.
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.16)\n'
                'project(LocationProfile NONE)\n'
                'set(APPLE TRUE)\n'
                'add_custom_target(nextcloud)\n'
                f'include("{ROOT}/shell_integration/MacOSX/CMakeLists.txt")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/command.txt" "${NEXTCLOUD_XCODEBUILD}")\n'
            )
            for fork in (False, True):
                with self.subTest(fork=fork):
                    build = pathlib.Path(directory) / str(fork)
                    subprocess.run([
                        "cmake", "-S", str(source), "-B", str(build),
                        f'-DNEXTCLOUD_FORK={"ON" if fork else "OFF"}',
                    ], check=True, capture_output=True)
                    arguments = (build / "command.txt").read_text().split(";")
                    settings = dict(argument.split("=", 1) for argument in arguments if "=" in argument)
                    self.assertEqual(settings.get(SETTING), "1" if fork else None)
                    for configuration in configurations:
                        self.assertEqual(configuration["INFOPLIST_PREPROCESS"], "YES")
                        definitions = configuration["INFOPLIST_PREPROCESSOR_DEFINITIONS"]
                        definitions = definitions.replace("$(inherited)", "")
                        definitions = re.sub(
                            r"\$\(([A-Z_]+):default=([0-9]+)\)",
                            lambda match: settings.get(match[1], match[2]), definitions,
                        )
                        self.assertNotIn("$(", definitions)
                        processed = subprocess.check_output([
                            "clang", "-E", "-P", "-x", "c", *[
                                "-D" + definition for definition in definitions.split()
                            ], str(INTEGRATION / "FileProviderExt/Info.plist"),
                        ])
                        info = plistlib.loads(processed)
                        self.assertNotIn(KEY, info)
                        extension = info["NSExtension"]
                        if fork:
                            self.assertIs(extension[KEY], True, "Must be a plist Boolean")
                        else:
                            self.assertNotIn(KEY, extension, "Preserve official default")
                        self.assertEqual(info["CFBundleDisplayName"], "$(OC_APPLICATION_NAME) File Provider")
                        self.assertEqual(extension["NSExtensionPointIdentifier"], "com.apple.fileprovider-nonui")


if __name__ == "__main__":
    unittest.main()
