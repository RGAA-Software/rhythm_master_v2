"""Build a source-bound local comparison page from actual Studio exports."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from content_identity import authoring_digest

ROOT = Path(__file__).resolve().parents[1]
FFMPEG = Path('C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe')
WORKS = {
    'aureate_vortex': ('鎏光流涡', '旋臂持续转动，粒子保持流动，相机绕行；音乐增强光丝、发射与流场。'),
    'porcelain_bloom': ('瓷金绽放', '两层瓷瓣以不同速度转动和舒展，相机绕行；音乐调制展开与釉光。'),
    'stratified_ink': ('层叠墨流', '多尺度地形沿连续噪声域迁移；音乐调制起伏、侵蚀和金边。'),
    'lumen_corridor': ('光门空间', '镜头持续穿行并缓慢横移升降，门段在镜头后回收；音乐调制结构与灯光。'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--music', type=Path, required=True)
    parser.add_argument('--silence', type=Path, required=True)
    parser.add_argument('--motion', type=Path, required=True)
    args = parser.parse_args()
    args.music = args.music.resolve()
    args.silence = args.silence.resolve()
    args.motion = args.motion.resolve()
    output = ROOT / 'docs/design/reviews/concept_works_v3_2026-09-10'
    videos = ROOT / 'out/concept-works-v3-preview'
    output.mkdir(parents=True, exist_ok=True)
    videos.mkdir(parents=True, exist_ok=True)
    records = {}
    page = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>持续运动 · 第三轮</title>
<style>body{max-width:1600px;margin:24px auto;padding:0 24px;background:#10151d;color:#e7eaf1;font:16px/1.7 system-ui}video,img{width:100%;display:block}article{padding:20px;background:#19212e;border-radius:12px;margin:24px 0}.pair{display:grid;grid-template-columns:1fr 1fr;gap:16px}button{background:#1e6175;color:white;border:0;border-radius:6px;padding:12px 20px;cursor:pointer}h3{font-size:16px;color:#a8c4d5}@media(max-width:700px){.pair{display:block}}</style>
<h1>四件作品 · 持续运动与音乐调制</h1>
<p>0.3 实际 Studio 输出。每段 16 秒，左侧保留作品音乐，右侧在渲染前关闭全部音轨输入，静音也保持主运动。可同步播放比较，也可分别拖动查看。工程自评不代替你的验收。</p>
<p>运动硬规则：持续流动、旋转或前进，空间作品建立相机运动；音乐调制已有运动。普通循环不应回跳。手动改变速度目前仍会重映射相位；GPU 粒子保持生命周期，不保证视频首尾像素完全一致。</p>
'''
    for name, (title, description) in WORKS.items():
        digest = authoring_digest(ROOT / 'content/templates' / name)
        record = {'source_sha256': digest, 'visual_acceptance': 'pending', 'exports': {}}
        media = []
        for directory, mode, suffix in ((args.music, 'export', ''), (args.silence, 'silent-export', '-silent')):
            result = json.loads((directory / 'results.json').read_text(encoding='utf-8'))[name]
            if result['status'] != 'passed' or result['source_sha256'] != digest or result['mode'] != mode:
                raise ValueError(f'Stale or failed export evidence: {name}/{mode}')
            candidates = list((directory / name).rglob('*.mp4'))
            if len(candidates) != 1:
                raise ValueError(f'Expected one final MP4: {name}/{mode}')
            destination = videos / (name + suffix + '.mp4')
            shutil.copy2(candidates[0], destination)
            record['exports'][mode] = {'evidence': directory.relative_to(ROOT).as_posix(),
                                      'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}
            media.append(destination.name)
        motion = json.loads((args.motion / 'results.json').read_text(encoding='utf-8'))[name]
        if motion['status'] != 'passed' or motion['source_sha256'] != digest:
            raise ValueError(f'Stale or failed motion evidence: {name}')
        record['motion'] = motion
        record['motion_evidence'] = args.motion.relative_to(ROOT).as_posix()
        shutil.copy2(args.motion / name / 'silence-contact.png', output / (name + '-motion.png'))
        subprocess.run([str(FFMPEG), '-v', 'error', '-y', '-ss', '4', '-i', str(videos / media[0]),
                        '-frames:v', '1', str(output / (name + '.png'))], check=True)
        page += f'<article><h2>{title}</h2><p>{description}</p><button onclick="playPair(this)">从头同步播放这一组</button><p class="status" aria-live="polite"></p><div class="pair">'
        for filename, label in zip(media, ('音乐驱动', '真正静音输入 · 自主运动')):
            page += f'<section><h3>{label}</h3><video controls loop playsinline preload="metadata" poster="{name}.png" src="../../../../out/concept-works-v3-preview/{filename}"></video></section>'
        page += f'</div><details><summary>查看连续 32 秒静音采样（每 4 秒一帧）</summary><img src="{name}-motion.png"></details></article>'
        records[name] = record
    page += '''<script>async function playPair(button){document.querySelectorAll('video').forEach(v=>v.pause());const article=button.closest('article');const videos=article.querySelectorAll('video');try{videos.forEach(v=>v.currentTime=0);await Promise.all([...videos].map(v=>v.play()));article.querySelector('.status').textContent='正在同步播放';}catch(error){article.querySelector('.status').textContent='请使用视频播放按钮：'+error.message;}}</script></html>'''
    (output / 'index.html').write_text(page, encoding='utf-8')
    (output / 'evidence.json').write_text(json.dumps(records, ensure_ascii=False, indent=4) + '\n', encoding='utf-8')
    print(output / 'index.html')


if __name__ == '__main__':
    main()
