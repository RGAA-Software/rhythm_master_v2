"""Verify actual APK content identity, then run packaged PCM/GPU checks on USB Android."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid
import zipfile

import content_identity
from verify_windows import run_logged, verification_lease

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--effect', default='luminous_concerto')
    parser.add_argument('--expected-nodes', type=int, required=True)
    parser.add_argument('--adb', type=Path, default=Path('D:/android/sdk/platform-tools/adb.exe'))
    parser.add_argument('--build', type=Path, default=ROOT / 'out/android-arm64-release')
    parser.add_argument('--host-build', type=Path, default=ROOT / 'out/windows-release')
    args = parser.parse_args()
    if not args.effect.replace('_', '').isalnum() or args.expected_nodes <= 0:
        parser.error('Invalid effect or expected node count')
    evidence = ROOT / 'out/android-music' / uuid.uuid4().hex
    evidence.mkdir(parents=True)
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
    run_logged(adb + ['shell', 'LD_LIBRARY_PATH=/data/local/tmp', remote, remote + '.rhythmpack',
                      '--arrangement', str(args.expected_nodes)], evidence / 'device.log')
    for seconds in (2, 6, 10, 14):
        for mode in ('music', 'silence'):
            name = f'{seconds}.{mode}.ppm'
            subprocess.run(adb + ['pull', remote + '.rhythmpack.' + name, str(evidence / name)], check=True)
    print('Android APK music evidence:', evidence)


if __name__ == '__main__':
    main()
