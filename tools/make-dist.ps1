<#
.SYNOPSIS
    builds gbemu-sdl (+ flappy/crossy roms) and assembles a portable dist folder.
.DESCRIPTION
    windows powershell 5.1 compatible packaging script. configures/builds with
    ninja + mingw, then copies the exe, sdl2.dll, roms, and launcher .cmd files
    into a dist folder, plus optional per-user start menu shortcuts.
#>
param(
    [string]$Sdl2Prefix = $(if ($env:SDL2_PREFIX) { $env:SDL2_PREFIX } else { "C:/Users/CJ/opt/SDL2-2.32.10/x86_64-w64-mingw32" }),
    [string]$GbdkHome = $(if ($env:GBDK_HOME) { $env:GBDK_HOME } else { "C:/Users/CJ/opt/gbdk" }),
    [string]$BuildDir = "build-rel",
    [string]$DistDir = "dist",
    [switch]$NoShortcuts,
    [switch]$NoTetris
)

$ErrorActionPreference = "Stop"

function Write-Step($msg) {
    Write-Host "==> $msg" -ForegroundColor Cyan
}

function Fail($msg) {
    Write-Host "error: $msg" -ForegroundColor Red
    exit 1
}

# repo root is the parent of this script's directory (tools/..)
$RepoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $RepoRoot
try {
    if (-not (Test-Path $Sdl2Prefix)) {
        Fail "sdl2 prefix not found: $Sdl2Prefix (pass -Sdl2Prefix or set SDL2_PREFIX)"
    }
    # gbdk is optional; without it we fall back to the vendored roms
    $HaveGbdk = Test-Path $GbdkHome
    if (-not $HaveGbdk) {
        Write-Host "warning: gbdk not found; using the vendored roms" -ForegroundColor Yellow
    }

    # --- configure ---
    $cacheFile = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cacheFile) {
        Write-Step "reusing existing configure in $BuildDir"
    } else {
        Write-Step "configuring $BuildDir (release, ninja, mingw)"
        if ($HaveGbdk) {
            cmake -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release `
                "-DCMAKE_C_COMPILER=gcc" "-DCMAKE_CXX_COMPILER=g++" `
                "-DCMAKE_PREFIX_PATH=$Sdl2Prefix" "-DGBDK_HOME=$GbdkHome"
        } else {
            cmake -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release `
                "-DCMAKE_C_COMPILER=gcc" "-DCMAKE_CXX_COMPILER=g++" `
                "-DCMAKE_PREFIX_PATH=$Sdl2Prefix"
        }
        if ($LASTEXITCODE -ne 0) { Fail "cmake configure failed" }
    }

    # --- build ---
    if ($HaveGbdk) {
        Write-Step "building gbemu-sdl, flappy, crossy"
        cmake --build $BuildDir --target gbemu-sdl flappy crossy -j
        if ($LASTEXITCODE -ne 0) { Fail "build failed" }
    } else {
        Write-Step "building gbemu-sdl"
        cmake --build $BuildDir --target gbemu-sdl -j
        if ($LASTEXITCODE -ne 0) { Fail "build failed" }
    }

    $exeSrc = Join-Path $BuildDir "gbemu-sdl.exe"
    if (-not (Test-Path $exeSrc)) { Fail "expected build output missing: $exeSrc" }
    if ($HaveGbdk) {
        $flappySrc = Join-Path $BuildDir "flappy.gb"
        $crossySrc = Join-Path $BuildDir "crossy.gb"
        if (-not (Test-Path $flappySrc)) { Fail "expected build output missing: $flappySrc" }
        if (-not (Test-Path $crossySrc)) { Fail "expected build output missing: $crossySrc" }
    } else {
        # no gbdk build; play from the committed roms instead
        $flappySrc = Join-Path $RepoRoot "assets\roms\flappy.gb"
        $crossySrc = Join-Path $RepoRoot "assets\roms\crossy.gb"
        if (-not (Test-Path $flappySrc)) { Fail "expected vendored rom missing: $flappySrc" }
        if (-not (Test-Path $crossySrc)) { Fail "expected vendored rom missing: $crossySrc" }
    }

    # --- dist dir ---
    if (-not (Test-Path $DistDir)) {
        Write-Step "creating $DistDir"
        New-Item -ItemType Directory -Path $DistDir | Out-Null
    }

    # --- rotate old exe, never clobber saves ---
    $exeDest = Join-Path $DistDir "gbemu-sdl.exe"
    if (Test-Path $exeDest) {
        $n = 1
        $oldName = Join-Path $DistDir "gbemu-sdl.old.exe"
        while (Test-Path $oldName) {
            $n++
            $oldName = Join-Path $DistDir "gbemu-sdl.old$n.exe"
        }
        Write-Step "keeping previous exe as $(Split-Path -Leaf $oldName)"
        Move-Item -Path $exeDest -Destination $oldName
    }

    Write-Step "copying exe, sdl2.dll, roms, icon"
    Copy-Item -Path $exeSrc -Destination $exeDest -Force

    $dllSrc = Join-Path $Sdl2Prefix "bin\SDL2.dll"
    if (-not (Test-Path $dllSrc)) { Fail "SDL2.dll not found at $dllSrc" }
    # a running emulator locks the dll; the dll never changes between deploys, so keep going
    try {
        Copy-Item -Path $dllSrc -Destination (Join-Path $DistDir "SDL2.dll") -Force -ErrorAction Stop
    } catch {
        Write-Host "warning: SDL2.dll is in use (emulator running?), keeping the existing copy" -ForegroundColor Yellow
    }

    Copy-Item -Path $flappySrc -Destination (Join-Path $DistDir "flappy.gb") -Force
    Copy-Item -Path $crossySrc -Destination (Join-Path $DistDir "crossy.gb") -Force

    # gbdk built fresh roms; keep the committed copies in sync with the sources
    if ($HaveGbdk) {
        Write-Step "refreshing vendored roms in assets/roms from this build"
        Copy-Item -Path $flappySrc -Destination (Join-Path $RepoRoot "assets\roms\flappy.gb") -Force
        Copy-Item -Path $crossySrc -Destination (Join-Path $RepoRoot "assets\roms\crossy.gb") -Force
    }

    $iconSrc = Join-Path $RepoRoot "assets\icons\gbemu.bmp"
    if (Test-Path $iconSrc) {
        Copy-Item -Path $iconSrc -Destination (Join-Path $DistDir "gbemu.bmp") -Force
    } else {
        Write-Host "warning: $iconSrc not found, skipping window icon" -ForegroundColor Yellow
    }

    # --- launcher .cmd files ---
    Write-Step "writing launcher .cmd files"
    $launchers = @{
        "tetris.cmd" = "tetris.gb"
        "flappy.cmd" = "flappy.gb"
        "crossy.cmd" = "crossy.gb"
    }
    foreach ($name in $launchers.Keys) {
        $rom = $launchers[$name]
        $body = "@echo off`r`nstart `"`" `"%~dp0gbemu-sdl.exe`" `"%~dp0$rom`"`r`n"
        [System.IO.File]::WriteAllText((Join-Path $DistDir $name), $body)
    }

    # --- tetris.gb ---
    if (-not $NoTetris) {
        $tetrisDest = Join-Path $DistDir "tetris.gb"
        if (Test-Path $tetrisDest) {
            Write-Step "tetris.gb already present in dist, leaving it alone"
        } else {
            Write-Step "copying assets/roms/tetris.gb into dist"
            Copy-Item -Path (Join-Path $RepoRoot "assets\roms\tetris.gb") -Destination $tetrisDest -Force
        }
    } else {
        Write-Step "skipping tetris.gb (-NoTetris)"
    }

    # --- start menu shortcuts ---
    if (-not $NoShortcuts) {
        Write-Step "creating start menu shortcuts"
        $distFull = (Resolve-Path $DistDir).Path
        $startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
        $shell = New-Object -ComObject WScript.Shell
        $shortcuts = @{
            "tetris" = "tetris.cmd"
            "flappy" = "flappy.cmd"
            "crossy" = "crossy.cmd"
        }
        foreach ($name in $shortcuts.Keys) {
            $cmdFile = $shortcuts[$name]
            $lnkPath = Join-Path $startMenu "$name.lnk"
            $shortcut = $shell.CreateShortcut($lnkPath)
            $shortcut.TargetPath = Join-Path $distFull $cmdFile
            $shortcut.WorkingDirectory = $distFull
            $shortcut.IconLocation = Join-Path $distFull "gbemu-sdl.exe"
            $shortcut.Save()
        }
        Write-Host "shortcuts created: tetris, flappy, crossy (type the name in windows search)" -ForegroundColor Green
    } else {
        Write-Step "skipping start menu shortcuts (-NoShortcuts)"
    }

    Write-Host ""
    Write-Host "done. dist folder: $((Resolve-Path $DistDir).Path)" -ForegroundColor Green
} finally {
    Pop-Location
}
