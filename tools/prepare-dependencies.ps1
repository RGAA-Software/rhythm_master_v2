param([string]$ReferenceRoot = 'C:/source/shark_dynamics_wallpaper')
$ErrorActionPreference = 'Stop'
$project_root = Split-Path $PSScriptRoot -Parent
$destination_root = Join-Path $project_root 'third_party/sources'
New-Item -ItemType Directory -Force $destination_root | Out-Null
$packages = @(
    @{name='sdl'; url='https://github.com/libsdl-org/SDL.git'; revision='96292a5b464258a2b926e0a3d72f8b98c2a81aa6'},
    @{name='imgui'; url='https://github.com/ocornut/imgui.git'; revision='4806a1924ff6181180bf5e4b8b79ab4394118875'},
    @{name='node_editor'; url='https://github.com/thedmd/imgui-node-editor.git'; revision='021aa0ea4da13fed864bafb2a92d4c5205076866'},
    @{name='picosha2'; url='https://github.com/okdshin/PicoSHA2.git'; revision='161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29'}
)
foreach ($package in $packages) {
    $destination = Join-Path $destination_root $package.name
    if (-not (Test-Path $destination)) {
        git init -q $destination
        if ($LASTEXITCODE) { throw "Init failed: $destination" }
        git -C $destination remote add origin $package.url
        if ($LASTEXITCODE) { throw "Remote setup failed: $destination" }
    }
    $actual_revision = git -C $destination rev-parse --verify HEAD 2>$null
    if ($LASTEXITCODE) {
        git -C $destination fetch --depth 1 origin $package.revision
        if ($LASTEXITCODE) { throw "Fetch failed: $($package.name)" }
        git -C $destination checkout -q --detach FETCH_HEAD
        if ($LASTEXITCODE) { throw "Checkout failed: $destination" }
    }
    $actual_revision = git -C $destination rev-parse HEAD
    if ($actual_revision -ne $package.revision) { throw "Unexpected revision: $destination" }
    $local_changes = git -C $destination status --porcelain --untracked-files=no
    if ($LASTEXITCODE -or $local_changes) { throw "Modified dependency: $destination" }
}
# The reference repositories are read only. This copies selected upstream files,
# never their build directories, project configuration, or application resources.
$selections = @{
    bgfx=@('include','src','LICENSE','3rdparty/directx-headers','3rdparty/khronos','3rdparty/renderdoc','examples/common/imgui/vs_ocornut_imgui.bin.h','examples/common/imgui/fs_ocornut_imgui.bin.h','examples/common/imgui/vs_ocornut_imgui.sc','examples/common/imgui/fs_ocornut_imgui.sc','examples/common/imgui/varying.def.sc','examples/common/common.sh');
    bx=@('include','src','3rdparty','LICENSE');
    bimg=@('include','src','LICENSE')
}
$inventory = @()
foreach ($name in ($selections.Keys | Sort-Object)) {
    foreach ($relative in $selections[$name]) {
        $source = Join-Path $ReferenceRoot "3rd/$name/$relative"
        $destination = Join-Path $destination_root "$name/$relative"
        $source_files = if (Test-Path $source -PathType Container) { Get-ChildItem $source -Recurse -File } else { Get-Item $source }
        foreach ($source_file in $source_files) {
            $relative_file = [IO.Path]::GetRelativePath((Join-Path $ReferenceRoot '3rd'), $source_file.FullName).Replace('\','/')
            $copied_file = Join-Path $destination_root $relative_file
            if (-not (Test-Path -LiteralPath $copied_file)) {
                New-Item -ItemType Directory -Force (Split-Path $copied_file -Parent) | Out-Null
                Copy-Item -LiteralPath $source_file.FullName -Destination $copied_file
            }
            $hash = (Get-FileHash -LiteralPath $source_file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $copied_hash = (Get-FileHash -LiteralPath (Join-Path $destination_root $relative_file) -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($hash -ne $copied_hash) { throw "Source mismatch: $relative_file" }
            $inventory += @{file=$relative_file; sha256=$hash}
        }
    }
}
$record = @{reference_revision=(git -C $ReferenceRoot rev-parse HEAD); packages=$packages; files=$inventory}
$record | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 (Join-Path $project_root 'third_party/source_inventory.json')
