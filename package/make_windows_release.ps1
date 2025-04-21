# Windows builds of SimCoupe to generate ZIP and EXE installer packages.

param (
    [switch]$Clean = $false,
    [switch]$CodeSign = $false
)

$ErrorActionPreference = 'Stop'
$prev_pwd = $PWD

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs_dir = &$vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module (Join-Path $vs_dir "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")

$vcpkg_dirs = $env:VCPKG_ROOT, $env:VCPKG_INSTALLATION_ROOT, 'C:\vcpkg'
$vcpkg_dir = $vcpkg_dirs | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
$vcpkg_toolchain = Join-Path $vcpkg_dir 'scripts\buildsystems\vcpkg.cmake'

function Build-Zip {
    param (
        [string] $Arch = $env:PROCESSOR_ARCHITECTURE
    )

    $old_path = $env:Path
    Enter-VsDevShell -Arch $Arch -VsInstallPath $vs_dir -SkipAutomaticLocation

    $target_arch = if ($Arch -eq 'x86') {'x86'} else {'x64'}
    $build_dir = "build-$target_arch"
    if ($Clean) {
        Remove-Item $build_dir -Recurse -Force -ErrorAction SilentlyContinue
    }

    mkdir $build_dir -ErrorAction SilentlyContinue | Out-Null
    Push-Location $build_dir

    $cmake_args = @(
        "-Wno-dev"
        "-G Ninja"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_VERSION=6.1"
    )

    if (Test-Path $vcpkg_toolchain -PathType Leaf) {
        $cmake_args += "-DCMAKE_TOOLCHAIN_FILE=`"$vcpkg_toolchain`""
    }

    &cmake .. @cmake_args
    &cmake --build .

    if ($CodeSign) {
        sign.bat SimCoupe.exe
    }

    if (Get-Command 7z -ErrorAction SilentlyContinue) {
        $version = (Get-Item SimCoupe.exe).VersionInfo.FileVersion
        $version = $version.Substring(0, $version.LastIndexOf('.'))
        $zipfile = Join-Path .. "SimCoupe-${version}-win_${target_arch}.zip"

        Remove-Item $zipfile -ErrorAction SilentlyContinue
        &7z a $zipfile -bsp0 -bso0 SimCoupe.exe *.dll ..\ReadMe.md ..\Manual.md ..\License.* ..\Resource\* '-x!..\Resource\SimCoupe.bmp'
        &7z l $zipfile
    } else {
        Write-Warning '7zip is required for ZIP creation'
    }

    Pop-Location
    $env:Path = $old_path
}

try {
    Build-Zip -Arch x86
    Build-Zip -Arch amd64

    $iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\iscc.exe'
    if (Test-Path $iscc) {
        $iscc_args = @('/DCUSTOM_BUILD')
        if ($CodeSign) {
            $iscc_args += '/DSIGN_BUILD'
        }

        Push-Location package
        &$iscc @iscc_args install.iss
        Pop-Location
    } else {
        Write-Warning 'Inno Setup 6.x is required for EXE installer'
    }

    Get-ChildItem -File SimCoupe-* | Sort-Object LastWriteTime | Select-Object -Last 3
}
finally {
    Set-Location $prev_pwd
}
