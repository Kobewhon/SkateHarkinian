param([switch]$PackageOnly)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$failures = 0
$manifest = Get-Content -LiteralPath (Join-Path $root 'RUNTIME-MANIFEST.json') -Raw | ConvertFrom-Json

foreach ($item in $manifest.files) {
    if ($item.path -match '(^/|^[A-Za-z]:|(^|/)\.\.(/|$)|\\)') {
        throw 'Unsafe package manifest path'
    }
    $file = Join-Path $root $item.path
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) {
        Write-Output ('MISSING package file: ' + $item.path)
        $failures++
        continue
    }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256) {
        Write-Output ('CHANGED package file: ' + $item.path)
        $failures++
    }
}

if (!$PackageOnly) {
    if (!(Test-Path -LiteralPath (Join-Path $root 'oot.o2r') -PathType Leaf)) {
        Write-Output 'MISSING user OoT data: supply your own oot.o2r beside soh.exe'
        $failures++
    }
    $assets = Join-Path $root 'skate-data/assets'
    $required = @(
        'private/game.json',
        'private/stock/physics-skeletons.json',
        'private/stock/skater-collections.json',
        'private/stock/data/config/input.cfg',
        'private/stock/data/anim/OnBoard.abin',
        'private/stock/data/anim/OffBoard.abin',
        'private/stock/data/script/camera/Default_cameragraph.stategraph'
    )
    foreach ($name in $required) {
        $file = Join-Path $assets $name
        if (!(Test-Path -LiteralPath $file -PathType Leaf) -or
            (Get-Item -LiteralPath $file -ErrorAction SilentlyContinue).Length -eq 0) {
            Write-Output ('MISSING/EMPTY prepared data: ' + $name)
            $failures++
        }
    }
    $game = Join-Path $assets 'private/game.json'
    if (Test-Path -LiteralPath $game -PathType Leaf) {
        try {
            $preparedManifest = Get-Content -LiteralPath $game -Raw | ConvertFrom-Json
            if ($preparedManifest.version -ne 1) {
                throw 'Unsupported game.json version; expected 1'
            }
            foreach ($key in @('character_scene', 'action_graph', 'motion_graph')) {
                $name = $preparedManifest.$key
                if (!$name -or $name -match '(^/|^[A-Za-z]:|(^|/)\.\.(/|$)|\\)') {
                    throw ('Unsafe/non-relative field: ' + $key)
                }
                $file = Join-Path $assets $name
                if (!(Test-Path -LiteralPath $file -PathType Leaf) -or
                    (Get-Item -LiteralPath $file -ErrorAction SilentlyContinue).Length -eq 0) {
                    throw ('Missing/empty manifest asset: ' + $name)
                }
            }
        } catch {
            Write-Output ('INVALID prepared manifest: ' + $_.Exception.Message)
            $failures++
        }
    }
}

if ($failures) {
    Write-Output 'Verification failed. See INSTALLATION.md and docs/SKATE3-DATA-SETUP.md. No game launched.'
    exit 1
}
Write-Output ('PASS ' + $manifest.buildId + '; package files verified. No game launched. ' +
    'Runtime compatibility still requires human testing.')
