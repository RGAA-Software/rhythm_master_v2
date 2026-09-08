"""Create the first-party articulated petal GLB used by Crystal Choir.

The model is authored here; glTF encoding follows the project's validated
cgltf/Godot-derived import profile. No external model or texture is embedded.
"""

import json
import math
import struct


def surface(u, v, target=-1):
    width = 0.025 + 0.43 * max(0, math.sin(math.pi * u)) ** 0.8
    x, y, z = 0.25 + 2.1 * u, v * width, 0.16 * math.sin(math.pi * u) + 0.12 * v * v
    z += 0.035 * math.cos(v * math.pi * 4) * math.sin(math.pi * u)
    if target == 0:
        y *= 1.7
    elif target == 1:
        z += 0.42 * math.sin(math.pi * u) * (1 - v * v)
    elif target == 2:
        z += 0.35 * u * v
    elif target == 3:
        x += 0.45 * u
    return x, y, z


def normal(u, v, target=-1):
    eps = 0.0001
    lo, hi = max(0, u - eps), min(1, u + eps)
    a, b = surface(lo, v, target), surface(hi, v, target)
    c, d = surface(u, v - eps, target), surface(u, v + eps, target)
    du, dv = [y - x for x, y in zip(a, b)], [y - x for x, y in zip(c, d)]
    cross = [du[1] * dv[2] - du[2] * dv[1], du[2] * dv[0] - du[0] * dv[2],
             du[0] * dv[1] - du[1] * dv[0]]
    length = math.sqrt(sum(x * x for x in cross))
    return tuple(x / length for x in cross)


def build():
    document = dict(asset=dict(version="2.0", generator="Rhythm Master Crystal Choir author"),
                    buffers=[], bufferViews=[], accessors=[])
    payload = bytearray()

    def accessor(rows, kind, component=5126):
        while len(payload) % 4:
            payload.append(0)
        offset = len(payload)
        code = {5126: 'f', 5123: 'H', 5121: 'B'}[component]
        for row in rows:
            payload.extend(struct.pack('<' + code * len(row), *row))
        view = len(document['bufferViews'])
        document['bufferViews'].append(dict(buffer=0, byteOffset=offset, byteLength=len(payload)-offset))
        result = len(document['accessors'])
        record = dict(bufferView=view, componentType=component, count=len(rows), type=kind)
        if kind in ('VEC3', 'SCALAR'):
            record.update(min=[min(row[i] for row in rows) for i in range(len(rows[0]))],
                          max=[max(row[i] for row in rows) for i in range(len(rows[0]))])
        document['accessors'].append(record)
        return result

    coordinates = [(i / 48, j / 12 * 2 - 1) for i in range(49) for j in range(13)]
    positions = [surface(u, v) for u, v in coordinates]
    normals = [normal(u, v) for u, v in coordinates]
    joints, weights = [], []
    for u, _ in coordinates:
        joint = min(1, int(u * 2))
        amount = u * 2 - joint
        joints.append((joint, joint + 1, 0, 0))
        weights.append((1 - amount, amount, 0, 0))
    indices = []
    for i in range(48):
        for j in range(12):
            a = i * 13 + j
            indices.extend((a, a + 13, a + 1, a + 1, a + 13, a + 14))
    attributes = dict(POSITION=accessor(positions, 'VEC3'), NORMAL=accessor(normals, 'VEC3'),
                      TEXCOORD_0=accessor([(u, v * 0.5 + 0.5) for u, v in coordinates], 'VEC2'),
                      JOINTS_0=accessor(joints, 'VEC4', 5121), WEIGHTS_0=accessor(weights, 'VEC4'))
    targets = []
    for target in range(4):
        points = [surface(u, v, target) for u, v in coordinates]
        directions = [normal(u, v, target) for u, v in coordinates]
        targets.append(dict(
            POSITION=accessor([tuple(b-a for a, b in zip(base, point))
                               for base, point in zip(positions, points)], 'VEC3'),
            NORMAL=accessor([tuple(b-a for a, b in zip(base, direction))
                             for base, direction in zip(normals, directions)], 'VEC3')))
    primitive = dict(attributes=attributes, indices=accessor([(i,) for i in indices], 'SCALAR', 5123),
                     material=0, targets=targets)
    document['meshes'] = [dict(name='Articulated glass petal', weights=[0, 0.15, 0, 0],
                               primitives=[primitive])]
    document['materials'] = [dict(name='Petal', doubleSided=True,
                                  pbrMetallicRoughness=dict(baseColorFactor=[0.08, 0.7, 0.8, 1],
                                                           metallicFactor=0.65, roughnessFactor=0.28))]
    document['nodes'] = [dict(name='Petal mesh', mesh=0, skin=0),
                          dict(name='Root joint', translation=[0.25, 0, 0], children=[2]),
                          dict(name='Middle joint', translation=[1.05, 0, 0], children=[3]),
                          dict(name='Tip joint', translation=[1.05, 0, 0])]
    inverse_bind = []
    for x in (0.25, 1.30, 2.35):
        inverse_bind.append((1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, 0, 0, 1))
    document['skins'] = [dict(joints=[1, 2, 3], inverseBindMatrices=accessor(inverse_bind, 'MAT4'))]
    document['animations'] = []
    times = accessor([(i / 8,) for i in range(33)], 'SCALAR')
    for clip, name in enumerate(('Slow unfurl', 'Rhythmic crest')):
        animation = dict(name=name, samplers=[], channels=[])
        for joint in range(3):
            rotations = []
            for i in range(33):
                angle = math.sin(i / 32 * math.tau * (clip + 1) - joint * 0.55) * (0.16 + clip * 0.26)
                rotations.append((0, math.sin(angle / 2), 0, math.cos(angle / 2)))
            animation['samplers'].append(dict(input=times, output=accessor(rotations, 'VEC4')))
            animation['channels'].append(dict(sampler=joint, target=dict(node=joint + 1, path='rotation')))
        document['animations'].append(animation)
    document['scenes'] = [dict(nodes=[0, 1])]
    document['scene'] = 0
    document['buffers'] = [dict(byteLength=len(payload))]
    encoded = json.dumps(document, separators=(',', ':')).encode('utf-8')
    encoded += b' ' * (-len(encoded) % 4)
    payload += b'\0' * (-len(payload) % 4)
    return (struct.pack('<III', 0x46546c67, 2, 28 + len(encoded) + len(payload)) +
            struct.pack('<II', len(encoded), 0x4e4f534a) + encoded +
            struct.pack('<II', len(payload), 0x004e4942) + payload)
