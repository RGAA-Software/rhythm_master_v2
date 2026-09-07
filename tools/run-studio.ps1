param(
    [string]$BuildDirectory = 'out/windows-release'
)
$ErrorActionPreference = 'Stop'
$project_root = Split-Path $PSScriptRoot -Parent
$executable = (Resolve-Path (Join-Path $project_root "$BuildDirectory/src/windows_spike/deploy/rhythm_master.exe")).Path
$process = Start-Process -FilePath $executable -WorkingDirectory (Split-Path $executable -Parent) -WindowStyle Hidden -PassThru
Write-Output "Studio started (PID $($process.Id))."
