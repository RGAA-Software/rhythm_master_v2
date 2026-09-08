"""Author an editable audio/video performance with original procedural media.

Reuses the project's Graph writer and PCM synthesis conventions. NumPy is an
authoring tool only; encoding uses the installed vcpkg FFmpeg, never a build.
"""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("gate", ROOT / "tools/author-resonance-gate.py")
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


def media_sources(work, ffmpeg):
    work.mkdir(parents=True, exist_ok=True)
    rate = 48000
    time = np.arange(rate * 8, dtype=np.float64) / rate
    beat = time % 0.5
    kick = 0.40 * np.exp(-beat * 18) * np.sin(np.pi * 2 * (52 * beat + 4 * (1 - np.exp(-beat * 28))))
    notes = np.array([55, 65.4064, 82.4069, 73.4162])[np.floor(time / 2).astype(int) % 4]
    bass = 0.20 * np.exp(-beat * 5) * np.sin(2 * np.pi * notes * time)
    hat = 0.045 * np.exp(-(time % 0.125) * 80) * (np.sin(2 * np.pi * 7200 * time) + np.sin(2 * np.pi * 9317 * time))
    melody = np.array([220, 329.6276, 440, 523.2511, 659.2551, 440, 329.6276, 293.6648])[np.floor(time / 0.25).astype(int) % 8]
    chime = 0.22 * np.exp(-(time % 0.25) * 9) * (np.sin(2 * np.pi * melody * time) + 0.25 * np.sin(2 * np.pi * melody * 2 * time))
    fade = np.minimum(1, np.minimum(time * 100, (8 - time) * 100))
    result = []
    for name, signal in [("pulse", kick + bass + hat), ("chimes", chime)]:
        pcm = np.clip(signal * fade, -1, 1)
        stereo = np.column_stack((pcm, pcm))
        source = work / (name + ".wav")
        with wave.open(str(source), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(rate)
            output.writeframes(np.rint(stereo * 32767).astype("<i2").tobytes())
        target = work / (name + ".flac")
        subprocess.run([str(ffmpeg), "-v", "error", "-y", "-i", str(source), str(target)], check=True)
        result.append((target, "audio/flac"))
    y, x = np.mgrid[-1:1:144j, -1.777:1.777:256j]
    for variant in range(2):
        target = work / f"curtain-{variant}.mp4"
        command = [str(ffmpeg), "-v", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgb24",
                   "-video_size", "256x144", "-framerate", "24", "-i", "pipe:0", "-an",
                   "-c:v", "mpeg4", "-q:v", "3", "-pix_fmt", "yuv420p", str(target)]
        frames = []
        for frame in range(96):
            phase = frame / 96 * 2 * np.pi
            field = np.zeros_like(x)
            for strand in range(10):
                center = (strand - 4.5) * 0.09 + 0.22 * np.sin(x * 2.5 + phase + strand * 0.3)
                center += 0.16 * np.sin(x * 4 - phase * (variant * 2 - 1) + strand * 0.25)
                field += np.exp(-((y - center) / 0.014) ** 2) * (0.35 + 0.22 * np.cos(x * 2 + strand + phase))
                field += 0.055 * np.exp(-((y - center) / 0.08) ** 2)
            palette = np.array([0.04, 0.65, 1.0] if variant == 0 else [1.0, 0.16, 0.38])
            image = np.clip(field[..., None] * palette, 0, 1)
            frames.append(np.rint(image * 255).astype(np.uint8).tobytes())
        subprocess.run(command, input=b"".join(frames), check=True, timeout=30)
        result.append((target, "video/mp4"))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ffmpeg", type=Path, default=Path("C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe"))
    args = parser.parse_args()
    destination = ROOT / "content/templates/luminous_concerto"
    records = []
    for path, mime in media_sources(ROOT / "out/luminous-concerto-authoring", args.ffmpeg):
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        blob = destination / "assets/sha256" / digest[:2] / digest
        blob.parent.mkdir(parents=True, exist_ok=True)
        blob.write_bytes(data)
        records.append(dict(sha256=digest, bytes=len(data), media_type=mime))
    graph = GATE.Graph()
    node = graph.node
    response = node("control.scalar", -500, 0, value=1, control_minimum=0, control_maximum=3)
    motion = node("control.scalar", -500, 300, value=1, control_minimum=0, control_maximum=3)
    glow = node("control.scalar", -500, 600, value=0.35, control_minimum=0, control_maximum=1)
    time = node("core.time", 0, 0)
    rotation = node("scalar.expression", 320, 0, dict(time=time, a=motion), expression="time * a * 5")
    back = node("texture.gradient", 1300, 0, color_a=(0.002, 0.004, 0.016, 1), color_b=(0.02, 0.002, 0.025, 1))
    for index in range(2):
        y = 450 + index * 550
        clip = node("texture.video_clip", 0, y, clip_start=index * 6, clip_duration=10,
                    source_in=0.5, source_out=3.5, clip_end=2, fade_in=2, fade_out=2)
        graph.nodes[-1] = graph.nodes[-1][:-1] + f'    properties {{ key: "asset" value {{ asset_sha256: "{records[index + 2]["sha256"]}" }} }}\n}}'
        folded = node("texture.mapping", 350, y, dict(source=clip, rotation=rotation), sectors=6 + index * 2,
                      scale=0.9, twist=0.15 * (index * 2 - 1))
        back = node("texture.composite", 1650 + index * 350, 0, dict(a=back, b=folded), composite_mode=1, amount=0.6)
    mesh = node("geometry.torus", 0, 1700, radius=2.1, tube_ratio=0.01, radial_segments=96, tube_segments=8)
    scenes = []
    for index in range(8):
        y = 2050 + index * 340
        band = node("audio.band", 0, y, audio_band=6 + index * 7)
        level = node("scalar.expression", 320, y, dict(a=band, b=response), expression="a * b")
        scale = node("scalar.map", 640, y, dict(value=level), input_max=0.3, output_min=0.75 + index * 0.05, output_max=1.0 + index * 0.05)
        energy = node("scalar.map", 970, y, dict(value=level), input_max=0.3, output_min=0.35, output_max=2)
        color = (0.04, 0.65, 0.9, 1) if index % 2 == 0 else (0.95, 0.12, 0.35, 1)
        material = node("material.pbr", 1300, y, dict(emission=energy), color_a=color, color_b=color, metallic=0.2, roughness=0.3)
        instance = node("scene.instance", 1630, y, dict(geometry=mesh, material=material))
        scenes.append(node("scene.transform", 1960, y, dict(scene=instance, scale=scale), rotation_x=20 + index * 15, rotation_y=index * 22.5))
    scene = scenes[0]
    for index, part in enumerate(scenes[1:]):
        scene = node("scene.merge", 2300 + index * 320, 2050, dict(a=scene, b=part))
    scene = node("scene.transform", 4600, 2050, dict(scene=scene, rotation_z=rotation))
    camera = node("scene.camera", 4600, 2600, eye_z=9, field_of_view=45, near_plane=0.1, far_plane=30)
    rendered = node("scene.render", 4900, 2050, dict(scene=scene, camera=camera))
    halo = node("texture.blur", 5200, 2450, dict(source=rendered), blur_radius=10)
    scene_image = node("texture.composite", 5500, 2050, dict(a=rendered, b=halo, amount=glow), composite_mode=1)
    display = node("texture.composite", 5800, 0, dict(a=back, b=scene_image), composite_mode=1)
    spectrum = node("texture.spectrum", 5500, 500, spectrum_layout=1, spectrum_radius=0.38, spectrum_gain=1.5,
                    bar_count=96, bar_gap=0.7, color_a=(0.05, 0.8, 1, 0.6), color_b=(1, 0.2, 0.4, 0.7))
    display = node("texture.composite", 6100, 0, dict(a=display, b=spectrum), composite_mode=1, amount=0.7)
    final = node("output.texture", 6400, 0, dict(source=display))
    controls = ['controls {']
    for identity, title in [(response, 'Music response'), (motion, 'Orbit speed'), (glow, 'Bloom')]:
        controls.append(f' titles {{ key: {identity} value: "{title}" }}')
    for identity, title, values in [(1, 'Opening', (0.5, 0.4, 0.2)), (2, 'Chorus', (1.5, 1.2, 0.5)), (3, 'Release', (0.4, 0.3, 0.15))]:
        controls.append(f' snapshots {{ id: {identity} title: "{title}"')
        for control, value in zip((response, motion, glow), values):
            controls.append(f'  values {{ key: {control} value: {value} }}')
        controls.append(' }')
    for identity, seconds, snapshot, fade in [(1, 0, 1, 0), (2, 4, 2, 2), (3, 12, 3, 3)]:
        controls.append(f' cues {{ id: {identity} title: "Part {identity}" seconds: {seconds} snapshot: {snapshot} fade: {fade} smooth: true }}')
    controls.append('}')
    (destination / 'graph.textproto').write_text(f'schema_version: 5\nid: "official-luminous-concerto"\noutput: {final}\ncanvas {{ width: 1280 height: 720 }}\n' + '\n'.join(graph.nodes + graph.edges + controls) + '\n', encoding='utf-8')
    (destination / 'editor.json').write_text(json.dumps(dict(version=2, positions=graph.positions), indent=4) + '\n', encoding='utf-8')
    clips = []
    for identity, title, source, start, duration, source_in, source_out, gain, pan in [
        (1, 'Pulse foundation', 0, 0, 16, 0, 8, 0.85, 0),
        (2, 'Chimes entrance', 1, 4, 8, 0, 8, 0.75, -0.25),
        (3, 'Chimes release', 1, 12, 4, 0, 4, 0.6, 0.25),
        (4, 'Answering chimes', 1, 8, 8, 2, 6, 0.35, 0.6),
    ]:
        clips.append(dict(id=identity, title=title, sha256=records[source]['sha256'], start=start, duration=duration,
                          source_in=source_in, source_out=source_out, loop=True, fade_in=0.5, fade_out=1,
                          smooth=True, gain=gain, pan=pan, muted=False))
    manifest = json.loads((ROOT / 'content/templates/crystal_choir/manifest.json').read_text(encoding='utf-8'))
    manifest.update(manifest_version=3, content_id='official.templates.luminous_concerto', project_id='official-luminous-concerto',
                    title='光幕协奏 / Luminous Concerto', titles={'zh-CN': '光幕协奏', 'en-US': 'Luminous Concerto'},
                    assets=records, tier='example', soundtrack=dict(sha256=records[0]['sha256'], title='Luminous Concerto', gain=0.75, loop=True, clips=clips),
                    descriptions={'zh-CN': '两段原创流光视频、四条音频片段与三段 Cue 组成 16 秒音画演出。低中高频驱动八组立体光环，视频在中段交叠，配乐可逐片段编辑入出点、音量、左右平衡与淡入淡出。',
                                  'en-US': 'A 16-second performance with two original ribbon videos, four audio clips and three cues. Eight 3D rings respond to bass, mids and treble while the video layers overlap. Edit trim, placement, gain, balance and fades directly in the timeline.'})
    (destination / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')
    print(f'Luminous Concerto: {len(graph.nodes)} nodes, {len(graph.edges)} edges; {sum(record["bytes"] for record in records)} asset bytes')


if __name__ == '__main__':
    main()
