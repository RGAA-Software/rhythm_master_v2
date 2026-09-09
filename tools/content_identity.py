"""Tie generated runtime content to its exact authored inputs and package bytes."""

import hashlib
import json


def digest_files(directory, paths):
    digest = hashlib.sha256()
    for path in sorted(paths):
        name = path.relative_to(directory).as_posix().encode('utf-8')
        data = path.read_bytes()
        digest.update(len(name).to_bytes(8, 'little'))
        digest.update(name)
        digest.update(len(data).to_bytes(8, 'little'))
        digest.update(data)
    return digest.hexdigest()


def authoring_digest(source):
    names = ['graph.textproto', 'manifest.json', 'editor.json']
    names += [name for name in ('presets.json', 'thumbnail.rgba', 'thumbnail.json')
              if (source / name).is_file()]
    return digest_files(source, [source / name for name in names] +
                        [path for path in (source / 'assets').rglob('*') if path.is_file()])


def compiled_digest(directory):
    paths = [directory / name for name in ('graph.pb', 'manifest.json', 'editor.json')]
    if (directory / 'presets.json').is_file():
        paths.append(directory / 'presets.json')
    return digest_files(directory, paths)


def write_compiled_identity(source, destination):
    record = {'version': 1, 'source_sha256': authoring_digest(source),
              'compiled_sha256': compiled_digest(destination)}
    (destination / 'build_identity.json').write_text(
        json.dumps(record, indent=4) + '\n', encoding='utf-8')


def write_package_identity(template, package):
    record = json.loads((template / 'build_identity.json').read_text(encoding='utf-8'))
    if record.get('version') != 1 or record['compiled_sha256'] != compiled_digest(template):
        raise ValueError(f'Compiled content identity mismatch: {template}')
    record['package_sha256'] = hashlib.sha256(package.read_bytes()).hexdigest()
    package.with_suffix('.source.json').write_text(json.dumps(record, indent=4) + '\n', encoding='utf-8')


def verify_package_source(source, package):
    record = json.loads(package.with_suffix('.source.json').read_text(encoding='utf-8'))
    if (record.get('version') != 1 or record.get('source_sha256') != authoring_digest(source) or
            record.get('package_sha256') != hashlib.sha256(package.read_bytes()).hexdigest()):
        raise ValueError(f'Stale runtime content; rebuild runtime_builtin_content: {package}')
    return record['source_sha256']
