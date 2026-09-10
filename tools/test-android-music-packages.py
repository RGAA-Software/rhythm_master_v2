"""Verify native Android music package contracts and exact decoded PCM, not APK UI."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--adb', default='D:/android/sdk/platform-tools/adb.exe')
    parser.add_argument('--build', type=Path, default=ROOT / 'out/android-arm64-release')
    parser.add_argument('--small-music', type=Path, default=ROOT / 'out/windows-release/audio-decoder-tests/tone.flac')
    parser.add_argument('--large-music', type=Path, default=ROOT / 'out/windows-release/large-music-fixtures/resonance_demo.wav')
    args = parser.parse_args()
    run_id = uuid.uuid4().hex
    output = ROOT / 'out/android-music-packages' / run_id
    output.mkdir(parents=True)
    remote = '/data/local/tmp/rhythm-music-packages-' + run_id
    binaries = {
        'contracts': args.build / 'src/project_io/soundtrack_contract_tests',
        'archive': args.build / 'src/project_io/file_archive_tests',
        'player': args.build / 'src/player_core/soundtrack_player_tests',
    }
    sources = {'small': args.small_music, 'large': args.large_music}
    inputs = {**binaries, **sources}
    identity = {'serial': args.serial, 'remote': remote,
                'sha256': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in inputs.items()},
                'bytes': {name: path.stat().st_size for name, path in sources.items()},
                'status': 'running', 'apk_ui': False}

    def adb(*arguments):
        return subprocess.run([args.adb, '-s', args.serial, *map(str, arguments)],
                              capture_output=True, timeout=180)

    def checked(*arguments):
        result = adb(*arguments)
        if result.returncode:
            (output / 'adb-failure.log').write_bytes(result.stdout + result.stderr)
            result.check_returncode()

    print('Native music package evidence:', output, flush=True)
    try:
        checked('shell', 'mkdir', '-p', remote)
        for name, path in inputs.items():
            checked('push', path, remote + '/' + name)
            if name in binaries:
                checked('shell', 'chmod', '700', remote + '/' + name)
        checks = {
            'contracts': ['contracts', remote + '/contract-results'],
            'archive': ['archive', remote + '/archive-results'],
            'small': ['player', remote + '/small', remote + '/small-results'],
            'large': ['player', remote + '/large', remote + '/large-results'],
        }
        for name, command in checks.items():
            result = adb('shell', 'LD_LIBRARY_PATH=/data/local/tmp', remote + '/' + command[0], *command[1:])
            (output / (name + '.log')).write_bytes(result.stdout + result.stderr)
            result.check_returncode()
            print(name + ': passed', flush=True)
        identity['status'] = 'passed'
    except Exception as error:
        identity.update(status='failed', error=str(error))
        raise
    finally:
        (output / 'identity.json').write_text(json.dumps(identity, indent=4) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
