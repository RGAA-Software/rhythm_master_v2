"""Verify actual APK content identity, then run packaged PCM/GPU checks on USB Android."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import uuid
import zipfile

import content_identity
from verify_windows import run_logged, verification_lease

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--effect', default='aureate_vortex')
    parser.add_argument('--expected-nodes', type=int, required=True)
    parser.add_argument('--adb', type=Path, default=Path('D:/android/sdk/platform-tools/adb.exe'))
    parser.add_argument('--build', type=Path, default=ROOT / 'out/android-arm64-release')
    parser.add_argument('--host-build', type=Path, default=ROOT / 'out/windows-release')
    args = parser.parse_args()
    if not args.effect.replace('_', '').isalnum() or args.expected_nodes <= 0:
        parser.error('Invalid effect or expected node count')
    evidence = ROOT / 'out/android-music' / uuid.uuid4().hex
    evidence.mkdir(parents=True)
    # APK builds do not link standalone test targets. Rebuild the checker before
    # interpreting its result; an old registry would reject newly added operators.
    cache = (args.build / 'CMakeCache.txt').read_text(encoding='utf-8')
    configuration = next(line.split('=', 1)[1] for line in cache.splitlines()
                         if line.startswith('CMAKE_BUILD_TYPE:STRING='))
    run_logged([sys.executable, str(ROOT / 'tools/build-android.py'),
                '--build', str(args.build), '--configuration', configuration,
                '--target', 'android_gpu_contract_tests'], evidence / 'native-build.log')
    apk_path = args.build / 'apk/rhythm-player-release.apk'
    package = args.host_build / 'content/packages' / (args.effect + '.rhythmpack')
    source = ROOT / 'content/templates' / args.effect
    with verification_lease():
        source_identity = content_identity.verify_package_source(source, package)
        with zipfile.ZipFile(apk_path) as apk:
            data = apk.read('assets/effects/' + args.effect + '.rhythmpack')
        digest = hashlib.sha256(data).hexdigest()
        if digest != hashlib.sha256(package.read_bytes()).hexdigest():
            raise ValueError('Final APK contains a different runtime content revision')
        (evidence / 'effect.rhythmpack').write_bytes(data)
        (evidence / 'identity.json').write_text(json.dumps({
            'source_sha256': source_identity, 'package_sha256': digest,
            'apk_sha256': hashlib.sha256(apk_path.read_bytes()).hexdigest(),
            'expected_nodes': args.expected_nodes, 'serial': args.serial
        }, indent=4) + '\n', encoding='utf-8')
    adb = [str(args.adb), '-s', args.serial]
    remote = '/data/local/tmp/rhythm-music-' + evidence.name
    commands = [adb + ['shell', 'am', 'force-stop', 'org.rhythmmaster.player'],
                adb + ['push', str(args.build / 'src/android_player/android_gpu_contract_tests'), remote],
                adb + ['push', str(evidence / 'effect.rhythmpack'), remote + '.rhythmpack'],
                adb + ['shell', 'chmod', '700', remote]]
    for command in commands:
        subprocess.run(command, check=True)
    completed = False
    try:
        run_logged(adb + ['shell', 'LD_LIBRARY_PATH=/data/local/tmp', remote, remote + '.rhythmpack',
                          '--arrangement', str(args.expected_nodes)], evidence / 'device.log')
        completed = True
    finally:
        # Failed pixel comparisons are precisely when the images are needed.
        # Preserve the original test failure if an earlier failure produced no image.
        missing = []
        for seconds in (2, 6, 10, 14):
            for mode in ('music', 'silence'):
                name = f'{seconds}.{mode}.ppm'
                result = subprocess.run(adb + ['pull', remote + '.rhythmpack.' + name,
                                               str(evidence / name)], capture_output=True)
                if result.returncode:
                    missing.append(name)
                    (evidence / (name + '.pull-error.txt')).write_bytes(result.stdout + result.stderr)
        if completed and missing:
            raise RuntimeError('GPU passed but evidence images are missing: ' + repr(missing))
    print('Android APK music evidence:', evidence)


if __name__ == '__main__':
    main()
