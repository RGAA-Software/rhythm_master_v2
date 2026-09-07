param(
    [string]$Adb = 'D:/android/sdk/platform-tools/adb.exe',
    [string]$Serial = '',
    [string]$BuildDirectory = 'out/android-arm64'
)
$ErrorActionPreference = 'Stop'
$project_root = Split-Path $PSScriptRoot -Parent
$build_root = (Resolve-Path (Join-Path $project_root $BuildDirectory)).Path
if (-not $Serial) {
    $connected = @(& $Adb devices | Where-Object { $_ -match '^\S+\s+device$' })
    if ($connected.Count -ne 1) { throw 'Specify -Serial when there is not exactly one authorized device.' }
    $Serial = ($connected[0] -split '\s+')[0]
}
function Invoke-Adb([string[]]$Arguments) {
    & $Adb -s $Serial @Arguments
    if ($LASTEXITCODE) { throw "adb failed: $($Arguments -join ' ')" }
}
$abi = & $Adb -s $Serial shell getprop ro.product.cpu.abi
if ($LASTEXITCODE -or $abi.Trim() -ne 'arm64-v8a') { throw 'This runner requires an arm64-v8a device.' }
Invoke-Adb @('shell', 'getprop', 'ro.build.version.sdk')
# Each run owns a fresh directory; no device files are removed and no APK is installed.
$remote = '/data/local/tmp/rhythm-master-phase-a-' + [DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff')
Invoke-Adb @('shell', 'mkdir', '-p', $remote)
$targets = @{
    render_contract_tests = 'src/rhythm_render';
    graph_contract_tests = 'src/graph';
    binding_tests = 'src/graph';
    component_tests = 'src/graph';
    component_io_tests = 'src/project_io';
    component_command_tests = 'src/editor_application';
    component_edit_tests = 'src/editor_application';
    runtime_contract_tests = 'src/runtime';
    audio_analysis_tests = 'src/audio_analysis';
    particle_simulation_tests = 'src/particles';
    point_tests = 'src/runtime';
    audio_input_tests = 'src/runtime';
    audio_spectrum_tests = 'src/runtime';
    affine_tests = 'src/runtime';
    shapes_tests = 'src/runtime';
    expression_tests = 'src/parameters';
    texture_ops_tests = 'src/runtime';
    editor_contract_tests = 'src/editor_application';
    persistence_contract_tests = 'src/project_io'
    program_contract_tests = 'src/project_io'
    package_contract_tests = 'src/project_io'
    wire_contract_tests = 'src/project_io'
    template_contract_tests = 'src/project_io'
    clock_contract_tests = 'src/cluster'
    external_contract_tests = 'src/runtime'
    input_contract_tests = 'src/cluster'
    datagram_contract_tests = 'src/cluster'
    exchange_contract_tests = 'src/cluster'
    send_queue_contract_tests = 'src/cluster'
    stream_contract_tests = 'src/cluster'
    scene_contract_tests = 'src/cluster'
    async_contract_tests = 'src/foundation'
    qr_contract_tests = 'src/qr'
    player_contract_tests = 'src/player_core'
    package_loader_contract_tests = 'src/player_core'
    package_import_contract_tests = 'src/player_core'
    schedule_contract_tests = 'src/cluster_player'
    scalar_contract_tests = 'src/runtime'
    signal_contract_tests = 'src/runtime'
    curve_contract_tests = 'src/parameters'
    asset_contract_tests = 'src/assets'
    content_contract_tests = 'src/content'
}
if (Test-Path -LiteralPath (Join-Path $build_root 'src/android_player/android_gpu_contract_tests')) {
    $targets['android_gpu_contract_tests'] = 'src/android_player'
}
foreach ($name in ($targets.Keys | Sort-Object)) {
    Invoke-Adb @('push', (Join-Path $build_root "$($targets[$name])/$name"), "$remote/$name")
    Invoke-Adb @('shell', 'chmod', '700', "$remote/$name")
}
Invoke-Adb @('push', (Join-Path $build_root 'content/templates/signal_texture'), "$remote/template")
Invoke-Adb @('push', (Join-Path $build_root 'content/templates'), "$remote/templates")
Invoke-Adb @('push', (Join-Path $project_root 'content/presets/catalog.json'), "$remote/presets.json")
Invoke-Adb @('push', (Join-Path $project_root 'src/project_io/tests/fixtures/legacy-v1.rhythmpack'), "$remote/legacy-v1.rhythmpack")
if ($targets.ContainsKey('android_gpu_contract_tests')) {
    Invoke-Adb @('push', (Join-Path $project_root 'out/windows/content/packages/signal_texture.rhythmpack'), "$remote/fixture.rhythmpack")
}
foreach ($name in ($targets.Keys | Sort-Object)) {
    $arguments = if ($name -eq 'persistence_contract_tests') { ' ./template ./projects' }
        elseif ($name -eq 'content_contract_tests') { ' ./presets.json' }
        elseif ($name -eq 'package_contract_tests') { ' ./packages ./legacy-v1.rhythmpack' }
        elseif ($name -eq 'template_contract_tests') { ' ./templates' }
        elseif ($name -eq 'android_gpu_contract_tests') { ' ./fixture.rhythmpack' } else { '' }
    Write-Output "Running $name on $Serial"
    Invoke-Adb @('shell', "cd '$remote' && ./$name$arguments")
}
Write-Output "$($targets.Count) shared native contracts passed. Device evidence retained at $remote"
