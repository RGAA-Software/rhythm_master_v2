"""Verify and retain the focused glsl-fxaa source and its actual notices."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
INTEGRITY = 'DocLGbKW+YFIynx80X65GENCfLg8skTETH56flJEPEH6ZPRs//hytRmIVU5Q2c4nPkhoz1rZGmkia+ckU6g7YQ=='
FILES = ('LICENSE.md', 'README.md', 'fxaa.glsl', 'index.glsl', 'package.json', 'texcoords.glsl')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, default=ROOT / 'third_party/source_archives/glsl-fxaa-3.0.0.tgz')
    args = parser.parse_args()
    data = args.archive.read_bytes()
    if hashlib.sha512(data).digest() != base64.b64decode(INTEGRITY):
        raise ValueError('glsl-fxaa archive integrity mismatch')
    archive_path = ROOT / 'third_party/source_archives/glsl-fxaa-3.0.0.tgz'
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    archive_path.write_bytes(data)
    destination = ROOT / 'third_party/sources/glsl-fxaa'
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    with tarfile.open(archive_path) as archive:
        if {entry.name for entry in archive.getmembers()} != {'package/' + name for name in FILES}:
            raise ValueError('Unexpected glsl-fxaa archive entries')
        for name in FILES:
            member = archive.getmember('package/' + name)
            if not member.isfile():
                raise ValueError('Expected plain source files')
            with archive.extractfile(member) as stream:
                content = stream.read()
            (destination / name).write_bytes(content)
            records.append(dict(path=name, sha256=hashlib.sha256(content).hexdigest()))
    notices = ROOT / 'third_party/notices/glsl-fxaa'
    notices.mkdir(parents=True, exist_ok=True)
    (notices / 'LICENSE-MIT.txt').write_bytes((destination / 'LICENSE.md').read_bytes())
    source = (destination / 'fxaa.glsl').read_text(encoding='utf-8')
    notice = source[source.index('/**'):source.index('*/') + 2]
    (notices / 'LICENSE-BSD.txt').write_text(notice + '\n', encoding='utf-8')
    record = dict(source_url='https://github.com/mattdesl/glsl-fxaa', version='3.0.0',
                  revision='5028eff0bc801aab51b884b27b47c313defa6a0c',
                  archive_url='https://registry.npmjs.org/glsl-fxaa/-/glsl-fxaa-3.0.0.tgz',
                  archive_sha256=hashlib.sha256(data).hexdigest(), archive_integrity='sha512-' + INTEGRITY,
                  license='MIT AND BSD-3-Clause', exceptions=[], files=records,
                  notices='third_party/notices/glsl-fxaa',
                  inspected_reference=dict(repository='https://github.com/cables-gl/cables',
                      revision='75d9960b75545cbd40037d4b48ccf52ac8c75b63',
                      files=['src/ops/base/Ops.Gl.ImageCompose.FXAA/att_fxaa.frag',
                             'src/ops/base/Ops.Gl.ImageCompose.FXAA/Ops.Gl.ImageCompose.FXAA.json']),
                  dependency_decision='No FXAA/SMAA port in the configured vcpkg checkout or shader in the installed bgfx source snapshots. Focused shader extraction only; no npm runtime or JavaScript dependencies.',
                  target='src/rhythm_render/shaders/texture_fxaa.sc',
                  modifications=['Adapt sampler and uniforms to bgfx and compute neighbor UVs from actual source extent.',
                                 'Filter premultiplied RGBA together; include coverage edges when source alpha varies.',
                                 'Keep opaque RGB luminance behavior, bounded span/reduction controls and optional strength.',
                                 'Remove glslify and host WebGL interfaces; retain the fixed nine-tap algorithm.'],
                  artifacts=['Windows Studio', 'Windows Player', 'Android Player'])
    (ROOT / 'provenance/fxaa.json').write_text(json.dumps(record, indent=4) + '\n', encoding='utf-8')
    print('glsl-fxaa 3.0.0 sources, MIT and BSD-3-Clause notices verified')


if __name__ == '__main__':
    main()
