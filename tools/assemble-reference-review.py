"""Assemble six real Player motion/silence captures with matching packages and editable sources."""

import hashlib
import html
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
NAMES = ("layered_neon", "aurora_clouds", "firefly_garden", "prismatic_lotus", "stellar_currents", "orbital_reliquary")
FFMPEG = Path("C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe")


def capture(name, digest, silent):
    records = list((ROOT / "out" / ("reference-silent-review" if silent else "refined-reference-review") / name).glob("*/preview.json"))
    records.append(ROOT / "out/effects-review" / name / ("silent-preview.json" if silent else "preview.json"))
    for path in sorted((path for path in records if path.is_file()), key=lambda path: path.stat().st_mtime, reverse=True):
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("sha256") == digest:
            return path.parent, "silent-" if path.name == "silent-preview.json" else ""
    result = subprocess.run([sys.executable, str(ROOT / "tools/render-template-preview.py"), "--package",
                             str(ROOT / "out/windows/content/packages" / (name + ".rhythmpack")), "--output",
                             str(ROOT / "out/reference-silent-review" / name if silent else ROOT / "out/refined-reference-review" / name)]
                            + (["--silent"] if silent else []), capture_output=True, check=True, timeout=180)
    return Path(result.stdout.decode("utf-8").strip().splitlines()[-1]), ""


def main():
    output = ROOT / "out/effects-review"
    output.mkdir(parents=True, exist_ok=True)
    articles = []
    for name in NAMES:
        package = ROOT / "out/windows/content/packages" / (name + ".rhythmpack")
        digest = hashlib.sha256(package.read_bytes()).hexdigest()
        destination = output / name
        destination.mkdir(exist_ok=True)
        for silent in (False, True):
            source, prefix = capture(name, digest, silent)
            for suffix in ("mp4", "png", "json"):
                original = source / (prefix + "preview." + suffix)
                target = destination / (("silent-" if silent else "") + "preview." + suffix)
                if original.resolve() != target.resolve():
                    shutil.copy2(original, target)
        shutil.copy2(package, destination / package.name)
        authored = ROOT / "content/templates" / name
        editable = destination / "editable-source"
        editable.mkdir(exist_ok=True)
        for filename in ("graph.textproto", "editor.json", "manifest.json"):
            shutil.copy2(authored / filename, editable / filename)
        subprocess.run([str(FFMPEG), "-hide_banner", "-loglevel", "error", "-nostdin", "-y", "-i",
                        str(destination / "preview.mp4"), "-vf", "fps=1,scale=480:270,tile=3x2", "-frames:v", "1",
                        str(destination / "contact.png")], check=True, timeout=30)
        manifest = json.loads((authored / "manifest.json").read_text(encoding="utf-8"))
        title = html.escape(manifest["titles"]["zh-CN"])
        description = html.escape(manifest.get("descriptions", {}).get("zh-CN", ""))
        tier = "基础" if manifest["tier"] == "basic" else "高端"
        articles.append(f'<article><h2>{title}<small>{tier}候选</small></h2><video controls loop muted preload="metadata" '
                        f'poster="{name}/preview.png" src="{name}/preview.mp4"></video><p>{description}</p>'
                        f'<p><a href="{name}/silent-preview.mp4">静音下的动态效果</a> · '
                        f'<a href="{name}/contact.png">六个时间点</a> · <a href="{name}/{name}.rhythmpack">Player 运行包</a></p>'
                        f'<p class="note">Studio → 从模板新建 → {title}。所有组件可进入内部节点图编辑。</p></article>')
        print(name, flush=True)
    page = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>Rhythm Master · 六个动态效果参考</title><style>
body{background:#0b1120;color:#e9effb;font:16px/1.7 "Microsoft YaHei",system-ui;margin:36px auto;max-width:1280px;padding:0 24px}
h1{font-size:28px}h2{font-size:20px}small{float:right;color:#90b5ca;font-size:13px;font-weight:normal}
main{display:grid;grid-template-columns:repeat(auto-fit,minmax(400px,1fr));gap:24px}
article{background:#151f31;padding:20px;border:1px solid #24344b;border-radius:14px}video{width:100%;border-radius:8px}
a{color:#7ce1e8}.note{color:#a9b7cb;font-size:14px}</style><h1>首批六个动态效果参考</h1>
<p>基础三个，高端三个。视频来自实际 Player 渲染；每段六秒，无配乐，音频响应使用可重复的合成特征。</p>
<p class="note">当前供视觉评审，尚未计入用户已验收数量。Android Adreno 650 的 Release 短时离屏测试已完成；
均衡画质下六个效果 p95 均低于 30 ms，仍需 APK 交互与持续运行验收。目标保持基础 50 个、高端 50 个。</p><main>'''
    (output / "index.html").write_text(page + "".join(articles) + "</main></html>\n", encoding="utf-8")
    print(output / "index.html")


if __name__ == "__main__":
    main()
