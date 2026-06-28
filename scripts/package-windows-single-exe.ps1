<#
ProtoTools Windows single-file packaging script

What this script does:
1. Configure and build ProtoTools Release with MSVC + Qt.
2. Package the ZLG CAN bridge Python script into zlg_can_bridge.exe, so CAN does
   not depend on a user-installed Python runtime.
3. Create a temporary portable application directory with windeployqt.
4. Copy required runtime DLLs into that directory:
   - Qt runtime and QML plugins: deployed by windeployqt
   - MSVC 2022 runtime: copied from Visual Studio redist directory
   - MSVC 2013 runtime: required by ZLG zlgcan.dll
   - ZLG CAN DLL and sibling x64 kerneldlls: required for CAN/CAN FD link
5. Wrap the portable directory into one self-extracting ProtoTools.exe with
   PyInstaller.
6. Delete temporary build/deploy directories and leave only dist/ProtoTools.exe.

Required tools:
- Windows x64
- Visual Studio 2022 Build Tools with MSVC
- CMake and Ninja
- Qt 6 MSVC x64, default: D:\software\Qt6.11\6.11.1\msvc2022_64
- Python 3 on PATH, only used on the build machine to run PyInstaller
- Network access on first run so pip can install PyInstaller into a local venv
- winget, only if the build machine does not already have VC++ 2013 runtime

Hardware/runtime note:
- This script packages application DLL dependencies. It cannot package kernel
  drivers for USB/CAN hardware. ZLG hardware drivers still need to be installed
  on the target PC if the device requires them.
#>

[CmdletBinding()]
param(
    [string]$QtRoot = "D:\software\Qt6.11\6.11.1\msvc2022_64",
    [string]$VsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
    [string]$BuildDir = "",
    [string]$DistDir = "",
    [string]$ZlgDll = "",
    [switch]$SkipZlgCan,
    [switch]$RunLaunchTest,
    [switch]$KeepIntermediate
)

$ErrorActionPreference = "Stop"

function Resolve-ScriptRoot {
    $scriptPath = $PSCommandPath
    if (-not $scriptPath) {
        $scriptPath = $MyInvocation.MyCommand.Path
    }
    return (Resolve-Path (Join-Path (Split-Path -Parent $scriptPath) "..")).Path
}

$RepoRoot = Resolve-ScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $RepoRoot "build-package-windows" }
if (-not $DistDir) { $DistDir = Join-Path $RepoRoot "dist" }

function Assert-InRepo([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetFullPath($RepoRoot)
    if (-not $full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to operate outside repository: $full"
    }
    return $full
}

function Remove-Tree([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        $safePath = Assert-InRepo $Path
        for ($i = 1; $i -le 5; ++$i) {
            try {
                Remove-Item -LiteralPath $safePath -Recurse -Force
                return
            } catch {
                if ($i -eq 5) {
                    throw
                }
                Start-Sleep -Milliseconds (300 * $i)
            }
        }
    }
}

function New-CleanDirectory([string]$Path) {
    $safePath = Assert-InRepo $Path
    if (Test-Path -LiteralPath $safePath) {
        try {
            Remove-Tree $safePath
        } catch {
            if (-not (Test-Path -LiteralPath $safePath -PathType Container)) {
                throw
            }
            Write-Warning "Could not remove directory itself, clearing its contents instead: $safePath"
            Get-ChildItem -LiteralPath $safePath -Force | ForEach-Object {
                if ($_.PSIsContainer) {
                    Remove-Item -LiteralPath $_.FullName -Recurse -Force
                } else {
                    Remove-Item -LiteralPath $_.FullName -Force
                }
            }
            return
        }
    }
    New-Item -ItemType Directory -Path $safePath | Out-Null
}

function Require-File([string]$Path, [string]$Message) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Message`nMissing: $Path"
    }
}

function Require-Directory([string]$Path, [string]$Message) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Message`nMissing: $Path"
    }
}

function Invoke-VsDevCommand([string]$CommandLine, [string]$LogPath = "") {
    Require-File $VsDevCmd "Visual Studio developer command script was not found."
    $cmd = "call `"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul && $CommandLine"
    if ($LogPath) {
        $cmd = "$cmd > `"$LogPath`" 2>&1"
    }
    & cmd.exe /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        if ($LogPath -and (Test-Path -LiteralPath $LogPath)) {
            Get-Content -LiteralPath $LogPath -Tail 80 | Out-Host
        }
        throw "Command failed with exit code $LASTEXITCODE`: $CommandLine"
    }
}

function Get-PeMachine([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        if ($stream.Length -lt 0x40) { return "unknown" }
        $stream.Seek(0x3c, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or ($peOffset + 6) -gt $stream.Length) { return "unknown" }
        $stream.Seek($peOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $signature = $reader.ReadUInt32()
        if ($signature -ne 0x00004550) { return "unknown" }
        $machine = $reader.ReadUInt16()
        switch ($machine) {
            0x8664 { return "x64" }
            0x014c { return "x86" }
            default { return "0x{0:x4}" -f $machine }
        }
    } finally {
        $stream.Dispose()
    }
}

function Resolve-ZlgDllCandidate([string]$Path, [bool]$Explicit) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $machine = Get-PeMachine $resolved
    if ($machine -ne "x64") {
        $message = "Skipping non-x64 zlgcan.dll ($machine): $resolved"
        if ($Explicit) {
            throw "The selected zlgcan.dll must be x64 for this package.`n$message"
        }
        Write-Warning $message
        return ""
    }
    return $resolved
}

function Find-ZlgDll {
    if ($ZlgDll) {
        return Resolve-ZlgDllCandidate $ZlgDll $true
    }

    $candidates = @(
        "D:\software\EricTool_v3.0.1\zlgcan.dll",
        "C:\Program Files\ZCANPRO\zlgcan.dll",
        "C:\Program Files (x86)\ZCANPRO\zlgcan.dll"
    )
    foreach ($candidate in $candidates) {
        $resolved = Resolve-ZlgDllCandidate $candidate $false
        if ($resolved) {
            return $resolved
        }
    }
    return ""
}

function Copy-ZlgRuntime([string]$ZlgDllPath, [string]$TargetDir) {
    Copy-Item -LiteralPath $ZlgDllPath -Destination (Join-Path $TargetDir "zlgcan.dll") -Force

    # Newer ZLG zlgcan.dll versions load device-specific helpers from a sibling
    # kerneldlls directory at OpenDevice time, for example USBCANFD.dll.
    $kernelDlls = Join-Path (Split-Path -Parent $ZlgDllPath) "kerneldlls"
    if (Test-Path -LiteralPath $kernelDlls -PathType Container) {
        $targetKernelDlls = Join-Path $TargetDir "kerneldlls"
        New-Item -ItemType Directory -Path $targetKernelDlls -Force | Out-Null
        Get-ChildItem -LiteralPath $kernelDlls -Force | ForEach-Object {
            $targetPath = Join-Path $targetKernelDlls $_.Name
            if ($_.PSIsContainer) {
                Copy-Item -LiteralPath $_.FullName -Destination $targetPath -Recurse -Force
            } elseif ($_.Extension -ieq ".dll") {
                $machine = Get-PeMachine $_.FullName
                if ($machine -eq "x64") {
                    Copy-Item -LiteralPath $_.FullName -Destination $targetPath -Force
                } else {
                    Write-Warning "Skipping non-x64 ZLG kernel DLL ($machine): $($_.FullName)"
                }
            } else {
                Copy-Item -LiteralPath $_.FullName -Destination $targetPath -Force
            }
        }
    } else {
        Write-Warning "ZLG kerneldlls directory was not found next to $ZlgDllPath. CAN/CAN FD may fail at OpenDevice on target PCs."
    }
}

function Copy-Msvc2022Runtime([string]$TargetDir) {
    $redistRootCandidates = @(
        "D:\software\vs2022\VC\Redist\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC"
    )
    $redistRoot = $redistRootCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Select-Object -First 1
    if (-not $redistRoot) {
        throw "MSVC 2022 redist directory was not found under any known location."
    }

    $crtDir = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName "x64\Microsoft.VC143.CRT" } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
        Select-Object -First 1

    if (-not $crtDir) {
        throw "MSVC 2022 x64 CRT directory was not found under $redistRoot"
    }

    Get-ChildItem -LiteralPath $crtDir -Filter "*.dll" | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $TargetDir -Force
    }
}

function Ensure-Vc2013RuntimeOnBuildMachine {
    $need = @("C:\Windows\System32\msvcr120.dll", "C:\Windows\System32\msvcp120.dll")
    $missing = $need | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }
    if ($missing.Count -eq 0) {
        return
    }

    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "VC++ 2013 x64 runtime is missing and winget is not available. Install Microsoft.VCRedist.2013.x64 once, then rerun this script."
    }

    $vc2013Dir = Join-Path $BuildDir "vcredist2013"
    New-CleanDirectory $vc2013Dir
    & winget download --id Microsoft.VCRedist.2013.x64 --source winget --download-directory $vc2013Dir --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) {
        throw "winget download Microsoft.VCRedist.2013.x64 failed."
    }

    $installer = Get-ChildItem -LiteralPath $vc2013Dir -Filter "*.exe" | Select-Object -First 1
    if (-not $installer) {
        throw "VC++ 2013 redistributable installer was not downloaded."
    }

    # The runtime is needed only on the build machine so the DLLs can be copied
    # into the final portable payload. Exit code 3010 means success with reboot
    # requested; some already-installed cases can return non-zero, so verify the
    # DLL files after running the installer instead of trusting the code alone.
    $process = Start-Process -FilePath $installer.FullName -ArgumentList "/install", "/quiet", "/norestart" -Wait -PassThru
    if (($process.ExitCode -ne 0) -and ($process.ExitCode -ne 3010)) {
        Write-Warning "VC++ 2013 installer returned $($process.ExitCode); checking whether runtime DLLs are now present."
    }

    $stillMissing = $need | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }
    if ($stillMissing.Count -gt 0) {
        throw "VC++ 2013 x64 runtime DLLs are still missing: $($stillMissing -join ', ')"
    }
}

function Copy-Vc2013Runtime([string]$TargetDir) {
    Ensure-Vc2013RuntimeOnBuildMachine
    Copy-Item -LiteralPath "C:\Windows\System32\msvcr120.dll" -Destination $TargetDir -Force
    Copy-Item -LiteralPath "C:\Windows\System32\msvcp120.dll" -Destination $TargetDir -Force
}

function Ensure-PyInstallerVenv {
    $venvDir = Join-Path $BuildDir "pyinstaller-venv"
    $venvPython = Join-Path $venvDir "Scripts\python.exe"
    if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
        & python -m venv $venvDir | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "python -m venv failed. Ensure Python 3 is available on PATH."
        }
    }

    & $venvPython -m pip install --upgrade pip pyinstaller | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install PyInstaller into local build venv."
    }
    return $venvPython
}

function Invoke-LaunchTest([string]$ExePath) {
    $processes = Get-Process ProtoTools, zlg_can_bridge -ErrorAction SilentlyContinue
    if ($processes) {
        $processes | Stop-Process -Force
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $ExePath
    $psi.WorkingDirectory = Split-Path -Parent $ExePath
    $psi.UseShellExecute = $false
    $psi.EnvironmentVariables["PATH"] = "C:\Windows\System32;C:\Windows"
    $process = [System.Diagnostics.Process]::Start($psi)
    Start-Sleep -Seconds 15
    $process.Refresh()
    if ($process.HasExited) {
        throw "Launch test failed; ProtoTools exited with code $($process.ExitCode)."
    }

    Get-Process ProtoTools -ErrorAction SilentlyContinue | Stop-Process -Force
}

function Stop-ProtoToolsProcesses {
    $processes = Get-Process ProtoTools, zlg_can_bridge -ErrorAction SilentlyContinue
    if ($processes) {
        $processes | Stop-Process -Force
        Start-Sleep -Milliseconds 500
    }
}

Write-Host "== ProtoTools single-file package =="
Write-Host "Repo:  $RepoRoot"
Write-Host "Qt:    $QtRoot"
Write-Host "Build: $BuildDir"
Write-Host "Dist:  $DistDir"

Stop-ProtoToolsProcesses

Require-Directory $QtRoot "Qt MSVC x64 root was not found."
Require-File (Join-Path $QtRoot "bin\windeployqt.exe") "windeployqt was not found under Qt root."
Require-File (Join-Path $RepoRoot "app\tools\zlg_can_bridge.py") "ZLG CAN bridge script was not found."
Require-File (Join-Path $RepoRoot "app\rcc\icons\ProtoTools.ico") "Windows icon was not found."

$BuildDir = Assert-InRepo $BuildDir
$DistDir = Assert-InRepo $DistDir
$PortableDir = Join-Path $DistDir "ProtoTools-portable"
$FinalExe = Join-Path $DistDir "ProtoTools.exe"

New-CleanDirectory $BuildDir
New-CleanDirectory $DistDir
New-Item -ItemType Directory -Path $PortableDir | Out-Null

Write-Host "Step 1/7: Configure CMake"
Invoke-VsDevCommand "cmake -S `"$RepoRoot`" -B `"$BuildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=`"$QtRoot`""

Write-Host "Step 2/7: Build ProtoTools Release"
Invoke-VsDevCommand "cmake --build `"$BuildDir`" --config Release --target ProtoTools" (Join-Path $BuildDir "build.log")

$BuiltExe = Join-Path $BuildDir "app\ProtoTools.exe"
Require-File $BuiltExe "ProtoTools.exe was not built."

Write-Host "Step 3/7: Build zlg_can_bridge.exe"
$venvPython = Ensure-PyInstallerVenv
& $venvPython -m PyInstaller --onefile --clean --name zlg_can_bridge `
    --distpath (Join-Path $BuildDir "bridge-dist") `
    --workpath (Join-Path $BuildDir "bridge-build") `
    --specpath (Join-Path $BuildDir "bridge-spec") `
    (Join-Path $RepoRoot "app\tools\zlg_can_bridge.py")
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller failed to build zlg_can_bridge.exe."
}

Write-Host "Step 4/7: Create portable payload"
Copy-Item -LiteralPath $BuiltExe -Destination $PortableDir -Force
Copy-Item -LiteralPath (Join-Path $BuildDir "bridge-dist\zlg_can_bridge.exe") -Destination $PortableDir -Force

if (-not $SkipZlgCan) {
    $resolvedZlgDll = Find-ZlgDll
    if (-not $resolvedZlgDll) {
        throw "zlgcan.dll was not found. Install the ZLG x64 runtime on the build machine, pass -ZlgDll <path>, or use -SkipZlgCan for a package without bundled ZLG runtime."
    }
    Copy-ZlgRuntime $resolvedZlgDll $PortableDir
}

Write-Host "Step 5/7: Deploy Qt and runtime DLLs"
$windeployqt = Join-Path $QtRoot "bin\windeployqt.exe"
Invoke-VsDevCommand "`"$windeployqt`" --release --qmldir `"$RepoRoot\app\qml`" --compiler-runtime --no-translations `"$PortableDir\ProtoTools.exe`"" (Join-Path $BuildDir "windeployqt.log")
Copy-Msvc2022Runtime $PortableDir
if (-not $SkipZlgCan) {
    Copy-Vc2013Runtime $PortableDir
}

# windeployqt may copy installer payloads/logs that are not needed at runtime.
@("vc_redist.x64.exe", "windeployqt.log", "ProtoTools-icon-preview.png", "zlg_can_bridge.py") | ForEach-Object {
    $path = Join-Path $PortableDir $_
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
    }
}

Write-Host "Step 6/7: Wrap portable payload into one ProtoTools.exe"
$launcher = Join-Path $BuildDir "single_file_launcher.py"
$launcherCode = @'
import os
import subprocess
import sys

base_dir = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
app_dir = os.path.join(base_dir, "ProtoTools-portable")
app_exe = os.path.join(app_dir, "ProtoTools.exe")
os.chdir(app_dir)
sys.exit(subprocess.call([app_exe] + sys.argv[1:]))
'@
[System.IO.File]::WriteAllText($launcher, $launcherCode, [System.Text.UTF8Encoding]::new($false))

$addData = "$PortableDir;ProtoTools-portable"
& $venvPython -m PyInstaller --onefile --windowed --clean --name ProtoTools `
    --icon (Join-Path $RepoRoot "app\rcc\icons\ProtoTools.ico") `
    --distpath $DistDir `
    --workpath (Join-Path $BuildDir "single-file-build") `
    --specpath (Join-Path $BuildDir "single-file-spec") `
    "--add-data=$addData" `
    $launcher
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller failed to build single-file ProtoTools.exe."
}
Require-File $FinalExe "Final single-file ProtoTools.exe was not created."

if ($RunLaunchTest) {
    Write-Host "Step 7/7: Launch test"
    Invoke-LaunchTest $FinalExe
} else {
    Write-Host "Step 7/7: Launch test skipped"
}

if (-not $KeepIntermediate) {
    Write-Host "Cleaning intermediate files"
    Remove-Tree $BuildDir
    Get-ChildItem -LiteralPath $DistDir -Force | Where-Object { $_.FullName -ne $FinalExe } | ForEach-Object {
        if ($_.PSIsContainer) {
            Remove-Item -LiteralPath $_.FullName -Recurse -Force
        } else {
            Remove-Item -LiteralPath $_.FullName -Force
        }
    }
}

$final = Get-Item -LiteralPath $FinalExe
Write-Host "Done: $($final.FullName)"
Write-Host "Size: $([Math]::Round($final.Length / 1MB, 2)) MB"
