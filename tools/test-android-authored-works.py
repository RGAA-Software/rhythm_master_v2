"""Select authored works in the installed APK and preserve the saved program.

Reuses the native touch/dump workflow from test-android-program-ui.py. This is
application UI, pause/resume and presentation evidence, not acoustic capture.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
APP = 'org.rhythmmaster.player'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--effect', action='append',
                        help='Bundled effect name; defaults to both P4 authored works')
    parser.add_argument('--adb', default='D:/android/sdk/platform-tools/adb.exe')
    parser.add_argument('--apk', type=Path, default=ROOT / 'out/android-arm64-release/apk/rhythm-player-release.apk')
    args = parser.parse_args()
    output = ROOT / 'out/android-authored-works' / uuid.uuid4().hex
    output.mkdir(parents=True)
    print('Authored works APK evidence:', output, flush=True)
    sequence = 0

    def adb(*command, timeout=30, check=True):
        result = subprocess.run([args.adb, '-s', args.serial, *map(str, command)],
                                capture_output=True, timeout=timeout)
        if check and result.returncode:
            (output / 'adb-failure.txt').write_bytes(result.stdout + result.stderr)
            raise RuntimeError('ADB failed: ' + repr(command))
        return result.stdout if check else result

    def shot(name):
        data = adb('exec-out', 'screencap', '-p')
        (output / (name + '.png')).write_bytes(data)
        return struct.unpack('>II', data[16:24])

    def hierarchy(name):
        nonlocal sequence
        remote = '/sdcard/rhythm-authored-works.xml'
        result = adb('shell', 'uiautomator', 'dump', '--compressed', remote)
        if b'dumped to:' not in result:
            raise RuntimeError('Fresh UI hierarchy unavailable')
        data = adb('shell', 'cat', remote)
        sequence += 1
        (output / f'{sequence:02d}-{name}.xml').write_bytes(data)
        return ET.fromstring(data)

    def tap(x, y):
        adb('shell', 'input', 'tap', round(x), round(y))
        time.sleep(.25)

    def touch(node):
        if node is None:
            raise RuntimeError('Expected visible control missing')
        x1, y1, x2, y2 = map(int, re.findall(r'\d+', node.get('bounds', '')))
        if x2 <= x1 or y2 - y1 < 30:
            raise RuntimeError('Control has no usable touch area')
        tap((x1 + x2) / 2, (y1 + y2) / 2)

    def saved():
        exists = adb('shell', 'run-as', APP, 'test', '-f', 'files/performance/list.json', check=False)
        return adb('exec-out', 'run-as', APP, 'cat', 'files/performance/list.json') if exists.returncode == 0 else None

    before = saved()
    if before is not None:
        (output / 'saved-program-before.json').write_bytes(before)
    apk_hash = hashlib.sha256(args.apk.read_bytes()).hexdigest()
    package_path = adb('shell', 'pm', 'path', APP).decode('utf-8').strip()
    if not package_path.startswith('package:') or '\n' in package_path:
        raise RuntimeError('Expected one installed acceptance APK')
    installed_hash = adb('shell', 'sha256sum', package_path.removeprefix('package:')).decode('utf-8').split()[0]
    if installed_hash != apk_hash:
        raise RuntimeError('Installed APK differs from the built acceptance APK')
    with zipfile.ZipFile(args.apk) as archive:
        catalog = {item['id']: item for item in json.loads(archive.read('assets/effects/catalog.json'))}
    record = {'apk_sha256': apk_hash, 'installed_apk_sha256': installed_hash,
              'serial': args.serial, 'works': {}, 'acoustic_capture': False, 'status': 'running'}
    try:
        adb('shell', 'am', 'force-stop', APP)
        adb('shell', 'am', 'start', '-n', APP + '/.PlayerActivity')
        time.sleep(5)
        width, height = shot('startup')
        if width <= height:
            raise RuntimeError('This landscape authoring check requires a landscape startup scene')
        queries = {'contour_pulse': 'contour', 'resonant_armillary': 'armillary',
                   'prismatic_title': 'title', 'vector_resonance': 'vector',
                   'spectral_corolla': 'corolla', 'glyph_current': 'glyph'}
        for name in args.effect or ['contour_pulse', 'resonant_armillary']:
            query = queries[name]
            entry = catalog[name]
            tap(width * .675, height * .134)  # Existing landscape Choose effect button.
            picker = hierarchy(name + '-picker')
            field = next((node for node in picker.iter('node')
                          if node.get('resource-id') == 'android:id/search_src_text'), None)
            touch(field)
            time.sleep(1)  # Let the device IME finish taking focus.
            adb('shell', 'input', 'keyevent', 4)  # Close the keyboard, retaining the catalog.
            # Pace hardware-key injection while the native list refreshes. A
            # burst can race the device IME's cursor updates (observed "iTtle").
            for character in query:
                adb('shell', 'input', 'text', character)
                time.sleep(.1)
            picker = hierarchy(name + '-filtered')
            if not any(node.get('text') == query for node in picker.iter('node')):
                raise RuntimeError('Catalog query was not entered unchanged')
            title = entry['titles']['zh-CN']
            matches = [node for node in picker.iter('node')
                       if node.get('text', '').startswith(title + ' · ')]
            if len(matches) != 1:
                raise RuntimeError('Catalog title selection is missing or ambiguous')
            touch(matches[0])
            time.sleep(2)
            current_width, current_height = shot(name + '-playing')
            if current_width <= current_height or entry['canvas']['width'] <= entry['canvas']['height']:
                raise RuntimeError('Accepted authored canvas did not select landscape presentation')
            tap(width * .783, height * .134)  # Pause for a stable title/status observation.
            # Frame statistics keep changing even while paused, so the stock
            # uiautomator dump never reaches idle on this window. Preserve
            # screenshots for explicit review instead of accepting stale XML.
            time.sleep(.7)
            shot(name + '-paused')
            tap(width * .783, height * .134)  # Resume before the next effect selection.
            time.sleep(1)
            shot(name + '-resumed')
            record['works'][name] = {'title': title, 'package_sha256': entry['sha256'],
                                     'canvas': entry['canvas'],
                                     'catalog_selection': 'passed', 'presentation_review': 'pending'}
            print('Selected; review title, output, 16-second duration and pause/resume screenshots:', name, flush=True)
        tap(width * .783, height * .134)
        record['saved_program_unchanged'] = saved() == before
        if not record['saved_program_unchanged']:
            raise RuntimeError('Read-only effect selection changed the saved program')
        record['status'] = 'presentation_review_pending'
    except Exception as error:
        record['status'] = 'failed'
        record['error'] = str(error)
        shot('failure')
        raise
    finally:
        (output / 'results.json').write_text(json.dumps(record, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')
    print('Android authored work UI capture complete; presentation review required:', output, flush=True)


if __name__ == '__main__':
    main()
