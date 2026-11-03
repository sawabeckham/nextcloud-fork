#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Exercise the production preinstall shell with isolated process/session commands."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class InstallerQuitTests(unittest.TestCase):
    def run_preinstall(self, scenario, executable=True, volume="/"):
        with tempfile.TemporaryDirectory(prefix="fork-preinstall-") as temporary:
            work = Path(temporary)
            app = work / "Nextcloud Fork"
            if executable:
                app.write_text("#!/bin/sh\nexit 0\n")
                app.chmod(0o755)
            mock = work / "mock"
            mock.write_text(f"#!{sys.executable}\n" + '''
import os, pathlib, sys, time
root = pathlib.Path(__file__).parent
command = sys.argv[1]
scenario = os.environ['QUIT_SCENARIO']
with (root/'calls').open('a') as f:
    f.write(command + ' ' + ' '.join(sys.argv[2:]) + '\\n')
if command == 'stat':
    print('loginwindow' if scenario == 'no-session' else 'owner')
elif command == 'id':
    print('0' if scenario == 'no-session' else '501')
elif command == 'launchctl':
    if scenario == 'request-fails':
        sys.exit(1)
    (root/'requested').touch()
elif command == 'sleep':
    time.sleep(0.02)
elif command == 'pgrep':
    if scenario == 'not-running':
        sys.exit(1)
    if (root/'requested').exists() and scenario == 'delayed-exit':
        counter = root/'polls'
        count = int(counter.read_text()) if counter.exists() else 0
        counter.write_text(str(count+1))
        sys.exit(0 if count < 2 else 1)
else:
    raise AssertionError(command)
''')
            mock.chmod(0o755)
            script = (ROOT / 'admin/fork/installer/scripts/preinstall').read_text()
            script = script.replace('/Applications/RomanenkoApp/Nextcloud Fork.app/Contents/MacOS/Nextcloud Fork', str(app))
            for command in ['pgrep', 'stat', 'id', 'launchctl', 'sleep']:
                prefix = '/bin/' if command in ['launchctl', 'sleep'] else '/usr/bin/'
                script = script.replace(prefix + command, f"'{mock}' {command}")
            target = work / 'preinstall'
            target.write_text(script)
            result = subprocess.run(['/bin/sh', str(target), '', '', volume],
                                    env={**os.environ, 'QUIT_SCENARIO': scenario},
                                    capture_output=True, text=True, timeout=10)
            calls = (work/'calls').read_text() if (work/'calls').exists() else ''
            return result, calls

    def test_no_client_requires_no_session_or_quit(self):
        result, calls = self.run_preinstall('not-running')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('launchctl', calls)
        self.assertNotIn('stat ', calls)

    def test_running_client_receives_quit_as_console_user_and_waits(self):
        result, calls = self.run_preinstall('delayed-exit')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('launchctl asuser 501 /usr/bin/sudo -u owner ', calls)
        self.assertIn('Nextcloud Fork --quit', calls)
        self.assertGreaterEqual(calls.count('sleep 1'), 2)
        self.assertNotIn('kill', calls)

    def test_stuck_or_failed_request_blocks_replacement(self):
        for scenario in ['stuck', 'request-fails']:
            with self.subTest(scenario=scenario):
                result, calls = self.run_preinstall(scenario)
                self.assertEqual(result.returncode, 1)
                self.assertIn('did not quit within 30 seconds', result.stderr)
                self.assertEqual(calls.count('sleep 1'), 30)

    def test_missing_executable_or_console_session_fails_safely(self):
        for scenario, executable in [('no-session', True), ('stuck', False)]:
            with self.subTest(scenario=scenario):
                result, calls = self.run_preinstall(scenario, executable=executable)
                self.assertEqual(result.returncode, 1)
                self.assertIn('Cannot request a safe exit', result.stderr)
                self.assertNotIn('launchctl', calls)

    def test_wrong_volume_rejected_before_any_process_action(self):
        result, calls = self.run_preinstall('delayed-exit', volume='/Volumes/Other')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(calls, '')


if __name__ == '__main__':
    unittest.main()
