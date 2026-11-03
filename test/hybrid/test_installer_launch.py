#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Run the actual postinstall with isolated session/registration/launch commands."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class InstallerLaunchTests(unittest.TestCase):
    def run_postinstall(self, scenario):
        with tempfile.TemporaryDirectory(prefix="fork-postinstall-") as temporary:
            work = Path(temporary)
            app = work / "Applications/RomanenkoApp/Nextcloud Fork.app"
            for extension in ['FinderSyncExt', 'FileProviderExt', 'FileProviderUIExt']:
                (app / 'Contents/PlugIns' / f'{extension}.appex').mkdir(parents=True)
            mock = work / 'mock'
            mock.write_text(f'#!{sys.executable}\n' + '''
import json, os, pathlib, sys
root = pathlib.Path(__file__).parent
command = sys.argv[1]
scenario = os.environ['LAUNCH_SCENARIO']
with (root/'calls').open('a') as stream:
    stream.write(json.dumps(sys.argv[1:]) + '\\n')
if command == 'stat':
    print('loginwindow' if scenario == 'loginwindow' else 'owner')
elif command == 'id':
    if scenario == 'unknown-user':
        sys.exit(1)
    print('0' if scenario == 'root' else '501')
elif command == 'launchctl':
    assert sys.argv[2:7] == ['asuser', '501', '/usr/bin/sudo', '-u', 'owner']
    if scenario == 'registration-fails' and sys.argv[7] != '/usr/bin/open':
        sys.exit(1)
    if scenario == 'launch-fails' and sys.argv[7] == '/usr/bin/open':
        sys.exit(1)
else:
    raise AssertionError(command)
''')
            mock.chmod(0o755)
            source = (ROOT / 'admin/fork/installer/scripts/postinstall').read_text()
            source = source.replace('/Applications/RomanenkoApp/Nextcloud Fork.app', str(app))
            for command in ['stat', 'id', 'launchctl']:
                prefix = '/bin/' if command == 'launchctl' else '/usr/bin/'
                source = source.replace(prefix + command, f"'{mock}' {command}")
            script = work / 'postinstall'
            script.write_text(source)
            result = subprocess.run(['/bin/sh', str(script)],
                                    env={**os.environ, 'LAUNCH_SCENARIO': scenario},
                                    capture_output=True, text=True, timeout=10)
            calls_file = work / 'calls'
            calls = [json.loads(line) for line in calls_file.read_text().splitlines()]
            return result, calls, str(app)

    def test_launches_only_installed_bundle_as_console_user_after_registration(self):
        result, calls, app = self.run_postinstall('success')
        self.assertEqual(result.returncode, 0, result.stderr)
        operations = [call for call in calls if call[0] == 'launchctl']
        self.assertEqual(len(operations), 5)
        for call in operations:
            self.assertEqual(call[1:6], ['asuser', '501', '/usr/bin/sudo', '-u', 'owner'])
        self.assertTrue(operations[0][6].endswith('/lsregister'))
        self.assertEqual(operations[0][7:], ['-f', app])
        self.assertEqual({Path(call[-1]).name for call in operations[1:4]},
                         {'FinderSyncExt.appex', 'FileProviderExt.appex', 'FileProviderUIExt.appex'})
        for call in operations[1:4]:
            self.assertEqual(call[6:8], ['/usr/bin/pluginkit', '-a'])
        self.assertEqual(operations[-1][6:], ['/usr/bin/open', '-a', app])

    def test_no_user_session_never_launches_or_registers_as_root(self):
        for scenario in ['root', 'loginwindow', 'unknown-user']:
            with self.subTest(scenario=scenario):
                result, calls, _ = self.run_postinstall(scenario)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertFalse(any(call[0] == 'launchctl' for call in calls))
                self.assertIn('no console user', result.stdout)

    def test_registration_failure_still_attempts_launch_once(self):
        result, calls, app = self.run_postinstall('registration-fails')
        self.assertEqual(result.returncode, 0, result.stderr)
        launches = [call for call in calls if '/usr/bin/open' in call]
        self.assertEqual(len(launches), 1)
        self.assertEqual(launches[0][-3:], ['/usr/bin/open', '-a', app])
        self.assertIn('registration deferred', result.stdout)

    def test_launch_failure_reports_manual_recovery_without_failing_completed_install(self):
        result, calls, _ = self.run_postinstall('launch-fails')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(sum('/usr/bin/open' in call for call in calls), 1)
        self.assertIn('was installed, but could not be opened', result.stdout)
        self.assertIn('Applications > RomanenkoApp', result.stdout)


if __name__ == '__main__':
    unittest.main()
