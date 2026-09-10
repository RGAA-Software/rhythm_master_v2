"""First-party concept geometry; focused reuse of author_petal_model's GLB writer."""

import json
import math
import random
import struct


def encode_mesh(positions, normals, coordinates, indices, label):
    document = dict(asset=dict(version='2.0', generator='Rhythm Master concept mesh author'),
                    buffers=[], bufferViews=[], accessors=[])
    payload = bytearray()

    def accessor(rows, kind, component=5126):
        payload.extend(b'\0' * (-len(payload) % 4))
        offset = len(payload)
        code = 'f' if component == 5126 else 'I'
        for row in rows:
            payload.extend(struct.pack('<' + code * len(row), *row))
        view = len(document['bufferViews'])
        document['bufferViews'].append(dict(buffer=0, byteOffset=offset, byteLength=len(payload) - offset))
        result = len(document['accessors'])
        record = dict(bufferView=view, componentType=component, count=len(rows), type=kind)
        if kind == 'VEC3':
            record.update(min=[min(row[i] for row in rows) for i in range(3)],
                          max=[max(row[i] for row in rows) for i in range(3)])
        document['accessors'].append(record)
        return result

    attributes = dict(POSITION=accessor(positions, 'VEC3'), NORMAL=accessor(normals, 'VEC3'),
                      TEXCOORD_0=accessor(coordinates, 'VEC2'))
    document['meshes'] = [dict(name=label, primitives=[dict(attributes=attributes,
        indices=accessor([(i,) for i in indices], 'SCALAR', 5125))])]
    document['nodes'] = [dict(mesh=0)]
    document['scenes'] = [dict(nodes=[0])]
    document['scene'] = 0
    document['buffers'] = [dict(byteLength=len(payload))]
    encoded = json.dumps(document, separators=(',', ':')).encode('utf-8')
    encoded += b' ' * (-len(encoded) % 4)
    payload += b'\0' * (-len(payload) % 4)
    return (struct.pack('<III', 0x46546c67, 2, 28 + len(encoded) + len(payload)) +
            struct.pack('<II', len(encoded), 0x4e4f534a) + encoded +
            struct.pack('<II', len(payload), 0x004e4942) + payload)


def ceramic_shell():
    def surface(u, v):
        radius = .48 + 2.45 * u
        angle = -1.65 * u + v * (.025 + .22 * math.sin(math.pi * u) ** .65)
        return (radius * math.cos(angle), radius * math.sin(angle),
                .15 + 1.4 * math.sin(u * math.pi * .9) + v * .8 * math.sin(math.pi * u)
                + v * v * .25 * math.sin(math.pi * u))

    positions, normals, uv, indices = [], [], [], []
    for i in range(97):
        for j in range(25):
            u, v = i / 96, j / 12 - 1
            positions.append(surface(u, v))
            uv.append((u, (v + 1) / 2))
            a, b = surface(max(0, u - .0001), v), surface(min(1, u + .0001), v)
            c, d = surface(u, max(-1, v - .0001)), surface(u, min(1, v + .0001))
            du, dv = [y - x for x, y in zip(a, b)], [y - x for x, y in zip(c, d)]
            cross = (du[1] * dv[2] - du[2] * dv[1], du[2] * dv[0] - du[0] * dv[2],
                     du[0] * dv[1] - du[1] * dv[0])
            length = max(1e-9, math.sqrt(sum(x * x for x in cross)))
            normals.append(tuple(x / length for x in cross))
    for i in range(96):
        for j in range(24):
            a = i * 25 + j
            indices.extend((a, a + 25, a + 1, a + 1, a + 25, a + 26))
    # Close the thin shell so oblique views reveal a real edge and back face.
    front_count = len(positions)
    front_indices = list(indices)
    positions.extend(tuple(p - n * .04 for p, n in zip(position, normal))
                     for position, normal in zip(positions[:front_count], normals[:front_count]))
    normals.extend(tuple(-n for n in normal) for normal in normals[:front_count])
    uv.extend(uv[:front_count])
    for i in range(0, len(front_indices), 3):
        a, b, c = front_indices[i:i + 3]
        indices.extend((a + front_count, c + front_count, b + front_count))
    boundary = ([j for j in range(25)] + [i * 25 + 24 for i in range(1, 97)] +
                [96 * 25 + j for j in range(23, -1, -1)] + [i * 25 for i in range(95, 0, -1)])
    for a, b in zip(boundary, boundary[1:] + boundary[:1]):
        start = len(positions)
        corners = (positions[a], positions[b], positions[a + front_count], positions[b + front_count])
        edge = [y - x for x, y in zip(corners[0], corners[1])]
        down = [y - x for x, y in zip(corners[0], corners[2])]
        cross = (edge[1] * down[2] - edge[2] * down[1], edge[2] * down[0] - edge[0] * down[2],
                 edge[0] * down[1] - edge[1] * down[0])
        length = max(1e-9, math.sqrt(sum(x * x for x in cross)))
        positions.extend(corners)
        normals.extend([tuple(x / length for x in cross)] * 4)
        uv.extend((uv[a], uv[b], uv[a], uv[b]))
        indices.extend((start, start + 1, start + 2, start + 2, start + 1, start + 3))
    return encode_mesh(positions, normals, uv, indices, 'Swept ceramic shell')


def vortex_mesh(family, points):
    rng = random.Random(931 + family)
    positions, normals, uv, indices = [], [], [], []

    def center(t, phase, drift, depth):
        radius = .12 * math.exp(t * 3.8)
        angle = phase - t * 5.4 + .12 * math.sin(t * 7 + drift)
        return (radius * math.cos(angle), radius * math.sin(angle) * .79,
                depth * t * t + math.sin(angle * 1.5 + drift) * t * .5)

    def triangle_fan(x, y, z, radius):
        start = len(positions)
        positions.append((x, y, z))
        for j in range(9):
            angle = j / 8 * math.tau
            positions.append((x + radius * math.cos(angle), y + radius * math.sin(angle), z))
        normals.extend([(0, 0, 1)] * 10)
        uv.extend([(.5, .5)] * 10)
        for j in range(8):
            indices.extend((start, start + j + 1, start + j + 2))

    for strand in range(78):
        arm = strand % 3
        phase = arm * math.tau / 3 + family * .38 + rng.gauss(0, .12)
        drift, depth = rng.random() * 6, rng.uniform(-1.8, 3.4)
        end = rng.uniform(.75, 1.08)
        if points:
            for j in range(36):
                t = rng.uniform(.12, end)
                x, y, z = center(t, phase + rng.gauss(0, .012), drift, depth)
                radius = rng.uniform(.003, .011) * (.35 + t)
                if rng.random() < .022:
                    radius *= rng.uniform(2.5, 5.5)
                    z += .5
                triangle_fan(x, y, z, radius)
        else:
            start = len(positions)
            for j in range(145):
                t = j / 144 * end
                x, y, z = center(t, phase, drift, depth)
                width = (.0012 + .0018 * t) * rng.uniform(.85, 1.15)
                ahead = center(t + .0001, phase, drift, depth)
                dx, dy = ahead[0] - x, ahead[1] - y
                length = max(.000001, math.hypot(dx, dy))
                ox, oy = -dy / length * width, dx / length * width
                positions.extend(((x - ox, y - oy, z), (x + ox, y + oy, z)))
                normals.extend(((0, 0, 1), (0, 0, 1)))
                uv.extend(((0, t), (1, t)))
                if j:
                    a = start + (j - 1) * 2
                    indices.extend((a, a + 2, a + 1, a + 1, a + 2, a + 3))
    return encode_mesh(positions, normals, uv, indices, 'Spatial sparks' if points else 'Spatial filaments')
