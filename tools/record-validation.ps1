param([string]$OutputDirectory = 'out/validation')
$ErrorActionPreference = 'Stop'
$project_root = Split-Path $PSScriptRoot -Parent
$output_root = Join-Path $project_root $OutputDirectory
New-Item -ItemType Directory -Force $output_root | Out-Null
$environment = @{
    recorded_utc = [DateTime]::UtcNow.ToString('o')
    configuration = 'Debug; MSVC x64 and Android arm64; not a performance acceptance run'
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
    os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,TotalVisibleMemorySize
    video = @(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,CurrentHorizontalResolution,CurrentVerticalResolution,CurrentRefreshRate)
    cmake = (& cmake --version | Select-Object -First 1)
    ninja = (& ninja --version)
    python = (& python --version)
    ndk = Get-Content 'D:/android/sdk/ndk/29.0.14206865/source.properties'
    gpu = (& nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader)
}
$environment | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 (Join-Path $output_root 'environment.json')
$source_files = @()
foreach ($directory in @('src', 'cmake', 'tools', 'content', 'locales')) {
    $source_files += Get-ChildItem (Join-Path $project_root $directory) -Recurse -File | Where-Object Extension -NE '.pyc'
}
foreach ($name in @('AGENTS.md', 'CMakeLists.txt', 'CMakePresets.json', '.clang-format', '.editorconfig',
                    'third_party/source_inventory.json', 'third_party/sdk_inventory.json')) {
    $source_files += Get-Item (Join-Path $project_root $name)
}
$snapshot = @($source_files | Sort-Object FullName | ForEach-Object {
    @{
        file = [IO.Path]::GetRelativePath($project_root, $_.FullName).Replace('\', '/')
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
})
$snapshot | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $output_root 'source_snapshot.json')
Write-Output "Recorded environment and $($snapshot.Count) owned-source/configuration hashes."
