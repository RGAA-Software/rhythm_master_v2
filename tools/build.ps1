param(
    [ValidateSet('core', 'windows', 'render', 'render-standalone')][string]$Preset = 'core',
    [switch]$ConfigureOnly,
    [string]$ShaderCompiler = ''
)
$ErrorActionPreference = 'Stop'
$env:VSLANG = '1033'
$project_root = Split-Path $PSScriptRoot -Parent
$cmake_command = (Get-Command cmake).Source
$ctest_command = (Get-Command ctest).Source
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs_root = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs_root) { throw 'MSVC x64 tools are required.' }
$dev_shell = Join-Path $vs_root 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
Import-Module $dev_shell
Enter-VsDevShell -VsInstallPath $vs_root -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$env:VSLANG = '1033'
Push-Location $project_root
try {
    # CMake 3.28 can misdecode a Chinese-only MSVC installation's include prefix.
    # Measure the compiler output so Ninja records header dependencies correctly.
    $probe_directory = Join-Path $project_root 'out/toolchain'
    New-Item -ItemType Directory -Force $probe_directory | Out-Null
    $probe_source = Join-Path $probe_directory 'include_probe.cpp'
    Set-Content -LiteralPath $probe_source -Value '#include <stddef.h>' -Encoding utf8
    $probe_output = & cl.exe /nologo /utf-8 /showIncludes /EP /TP $probe_source 2>&1
    if ($LASTEXITCODE) { throw 'MSVC include-prefix probe failed.' }
    $prefix_line = $probe_output | Where-Object { $_ -match '^(.+?)[A-Za-z]:[\\/].*stddef\.h' } | Select-Object -First 1
    if (-not $prefix_line) { throw 'Cannot determine MSVC include-prefix.' }
    $include_prefix = [regex]::Match($prefix_line, '^(.+?)[A-Za-z]:[\\/]').Groups[1].Value.TrimEnd()
    if ($Preset -eq 'render-standalone') {
        & $cmake_command -S src/rhythm_render -B out/render-standalone -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DRHYTHM_MSVC_INCLUDE_PREFIX=$include_prefix"
    } else {
        $extra_options = @()
        if ($ShaderCompiler) { $extra_options += "-DRHYTHM_SHADERC=$ShaderCompiler" }
        & $cmake_command --preset $Preset "-DRHYTHM_MSVC_INCLUDE_PREFIX=$include_prefix" @extra_options
    }
    if ($LASTEXITCODE) { throw 'CMake configure failed.' }
    if (-not $ConfigureOnly) {
        if ($Preset -eq 'render-standalone') {
            & $cmake_command --build out/render-standalone --parallel 20
        } else {
            & $cmake_command --build --preset $Preset
        }
        if ($LASTEXITCODE) { throw 'Incremental build failed.' }
        if ($Preset -eq 'render-standalone') {
            & $ctest_command --test-dir out/render-standalone --output-on-failure
        } else {
            & $ctest_command --preset $Preset
        }
        if ($LASTEXITCODE) { throw 'Tests failed.' }
    }
} finally { Pop-Location }
