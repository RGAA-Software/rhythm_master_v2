param(
    [string]$Ndk = 'D:/android/sdk/ndk/29.0.14206865',
    [string]$TargetSdk = 'C:/source/vcpkg/installed/arm64-android',
    [string]$HostProtoc = 'C:/source/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe',
    [string]$ShaderCompiler = ''
)
$ErrorActionPreference = 'Stop'
$project_root = Split-Path $PSScriptRoot -Parent
Push-Location $project_root
try {
    $extra_options = @()
    if ($ShaderCompiler) { $extra_options += "-DRHYTHM_SHADERC=$ShaderCompiler" }
    & cmake -S . -B out/android-arm64 -G Ninja `
        "-DCMAKE_TOOLCHAIN_FILE=$Ndk/build/cmake/android.toolchain.cmake" `
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Debug `
        -DRHYTHM_BUILD_PROJECT_IO=ON "-DRHYTHM_IO_SDK=$TargetSdk" `
        "-DRHYTHM_PROTOC=$HostProtoc" "-DCMAKE_FIND_ROOT_PATH=$TargetSdk" @extra_options
    if ($LASTEXITCODE) { throw 'Android shared-target configuration failed.' }
    & cmake --build out/android-arm64 --parallel 20
    if ($LASTEXITCODE) { throw 'Android shared-target incremental build failed.' }
} finally {
    Pop-Location
}
