"""Check owned portable source and public contracts, without inspecting imports."""

import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN = re.compile(
    r"(?:SDL|imgui|bgfx|bx/|windows\.h|TargetConditionals|android/|jni\.h|"
    r"cgltf|box2d|\bb2[A-Z]|asio|cpr/|Qt|QWidget|QObject|libav|graph\.pb\.h|google/protobuf|openssl|msquic|quiche|EVP_|X509|QUIC_)", re.I
)

# Only these private renderer adapters may include backend APIs. Match full paths
# so a same-named file in a domain module cannot bypass the boundary check.
RENDER_ADAPTERS = {
    "src/rhythm_render/src/" + name for name in (
        "bgfx_backend.h", "bgfx_backend.cpp", "bgfx_handles.h",
        "bgfx_readbacks.h", "bgfx_readbacks.cpp",
        "bgfx_scene.h", "bgfx_scene.cpp",
        "bgfx_scene_instances.h", "bgfx_scene_instances.cpp",
        "bgfx_scene_lights.h", "bgfx_scene_lights.cpp",
        "bgfx_scene_textures.h", "bgfx_scene_textures.cpp",
        "bgfx_scene_shadow.h", "bgfx_scene_shadow.cpp",
        "bgfx_scene_skin.h", "bgfx_scene_skin.cpp",
        "bgfx_scene_morph.h", "bgfx_scene_morph.cpp",
        "bgfx_scene_environment.h", "bgfx_scene_environment.cpp",
        "bgfx_gpu_points.h", "bgfx_gpu_points.cpp",
        "bgfx_texture_programs.h", "bgfx_texture_programs.cpp")
}
RENDER_ADAPTERS.update({"src/rhythm_render/src/bgfx_image_programs.h", "src/rhythm_render/src/bgfx_image_programs.cpp"})

# This isolated R0 probe deliberately validates native buffer bindings; its public
# test entry point exposes only project types and a bounded byte span.
RENDER_ADAPTERS.add("src/rhythm_render/tests/gpu_execution_probe.cpp")
# P6.1 isolated material linkage/pixel probe has the same native boundary;
# its public contract contains only a bounded host byte span and a path.
RENDER_ADAPTERS.add("src/rhythm_render/tests/material_profile_gpu.cpp")


CPP_NON_CODE = re.compile(
    r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    r'|/\*.*?\*/|//[^\n]*', re.S)
RAW_POINTER = re.compile(
    r"\b(?!(?:return|co_return|throw)\b)[A-Za-z_]\w*(?:::\w+)*(?:\s+const)?\s*\*")
MANUAL_OWNERSHIP = re.compile(r"\bnew\b\s*(?:\w|\()|\bdelete\s*\[\s*\]|\bdelete\s+(?![;=])")


def without_comments(text):
    return CPP_NON_CODE.sub(lambda match: " " if match[0].startswith("/") else match[0], text)


def code_tokens(text):
    return CPP_NON_CODE.sub(" ", text)


def main():
    failures = []
    checked = 0
    for module in ("graph", "runtime", "parameters", "performance", "particles", "scene3d", "shader_expression", "shader_artifact", "image_shader", "model_assets", "prepared_assets", "video_playback", "video_sources", "export_core", "audio_analysis", "audio_playback", "cluster", "cluster_auth", "cluster_player", "foundation", "qr", "player_core", "player_audio", "editor_application", "rhythm_render"):
        for path in (ROOT / "src" / module).rglob("*"):
            if path.suffix not in (".h", ".cpp") or path.relative_to(ROOT).as_posix() in RENDER_ADAPTERS:
                continue
            code = without_comments(path.read_text(encoding="utf-8-sig"))
            for included in re.findall(r"^\s*#\s*include\s*[<\"]([^>\"]+)", code, re.M):
                # This is a generated GLSL include inside a C++ raw string, not
                # a C++ backend dependency. Keep the exception file/name exact.
                if path.relative_to(ROOT).as_posix() == "src/image_shader/source.cpp" and included == "bgfx_shader.sh":
                    continue
                if FORBIDDEN.search(included):
                    failures.append(f"{path.relative_to(ROOT)}: forbidden include {included}")
            checked += 1
    for path in (ROOT / "src").glob("*/include/**/*.h"):
        code = without_comments(path.read_text(encoding="utf-8-sig"))
        if FORBIDDEN.search(code) or re.search(r"\bglm(?:::|/)", code):
            failures.append(f"{path.relative_to(ROOT)}: backend type in public contract")
        if RAW_POINTER.search(code_tokens(code)):
            failures.append(f"{path.relative_to(ROOT)}: raw pointer in public contract")
    for path in (ROOT / "src").rglob("*"):
        if path.suffix not in (".h", ".cpp"):
            continue
        text = path.read_text(encoding="utf-8-sig")
        code = code_tokens(text)
        if "\t" in text:
            failures.append(f"{path.relative_to(ROOT)}: tab in owned C++")
        if MANUAL_OWNERSHIP.search(code):
            failures.append(f"{path.relative_to(ROOT)}: manual owning allocation")
    catalogs = [json.loads(path.read_text(encoding="utf-8-sig"))
                for path in sorted((ROOT / "locales").glob("*/studio.json"))]
    if len(catalogs) != 2 or catalogs[0].keys() != catalogs[1].keys():
        failures.append("Studio locale key sets differ")
    for catalog in catalogs:
        for source in (ROOT / "src/graph/src/compiler.cpp", ROOT / "src/graph/src/registry.cpp", ROOT / "src/editor_application/commands.cpp"):
            diagnostics = re.findall(r'"(graph\.[a-z_]+)"', source.read_text(encoding="utf-8-sig"))
            for diagnostic in diagnostics:
                if diagnostic not in catalog:
                    failures.append(f"Missing diagnostic translation: {diagnostic}")
        if any(not isinstance(value, str) or not value or "##" in value
               for value in catalog.values()):
            failures.append("Locale contains empty text or UI identity suffix")
    if failures:
        raise SystemExit("\n".join(failures))
    print(f"Portable boundary checks passed for {checked} files; public APIs and locale parity passed")


if __name__ == "__main__":
    main()
