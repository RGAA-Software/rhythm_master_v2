param([string]$BuildDirectory = 'out/windows', [string]$Output = 'out/studio-window.png')
$ErrorActionPreference = 'Stop'
$project_root = Split-Path $PSScriptRoot -Parent
$app_path = (Resolve-Path (Join-Path $project_root "$BuildDirectory/src/windows_spike/rhythm_master.exe")).Path
$sdk = Join-Path $project_root '../vcpkg/installed/x64-windows'
$previous_path = $env:PATH
$env:PATH = "$sdk/debug/bin;$sdk/bin;" + $env:PATH
Add-Type -AssemblyName System.Drawing
if (-not ('RhythmWindowCapture' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class RhythmWindowCapture {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
}
'@
}
$app_process = Start-Process -FilePath $app_path -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $project_root 'out/ui-run.log') `
    -RedirectStandardError (Join-Path $project_root 'out/ui-run-error.log')
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 200
        $app_process.Refresh()
    } while (-not $app_process.MainWindowHandle -and -not $app_process.HasExited -and [DateTime]::UtcNow -lt $deadline)
    if ($app_process.HasExited) { throw 'Studio exited before capture.' }
    Start-Sleep -Milliseconds 1500
    $rect = [RhythmWindowCapture+Rect]::new()
    if (-not [RhythmWindowCapture]::GetWindowRect($app_process.MainWindowHandle,[ref]$rect)) { throw 'No app window.' }
    $bitmap = [Drawing.Bitmap]::new($rect.Right-$rect.Left,$rect.Bottom-$rect.Top)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $dc = $graphics.GetHdc()
        try { $captured = [RhythmWindowCapture]::PrintWindow($app_process.MainWindowHandle,$dc,2) }
        finally { $graphics.ReleaseHdc($dc) }
        if (-not $captured) { throw 'PrintWindow failed.' }
        $bitmap.Save((Join-Path $project_root $Output),[Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
    $modules = $app_process.Modules | Select-Object -ExpandProperty ModuleName
    if ($modules | Where-Object { $_ -match '^Qt[56]' }) { throw 'Unexpected Qt module loaded.' }
    $modules | Sort-Object | Set-Content (Join-Path $project_root 'out/studio-loaded-modules.txt')
    "Captured Studio window: $Output"
} finally {
    if (-not $app_process.HasExited) {
        $null = $app_process.CloseMainWindow()
        if (-not $app_process.WaitForExit(5000) -and $app_process.Path -eq $app_path) { Stop-Process -Id $app_process.Id }
    }
    $app_process.Dispose()
    $env:PATH = $previous_path
}
