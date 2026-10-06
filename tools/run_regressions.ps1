param(
    [string]$HeaderRoot = (Join-Path $PSScriptRoot '../overlay/soh/soh/Enhancements/NativeSkating'),
    [string]$ImGuiRoot = '',
    [string]$RuntimeDll = '',
    [string]$AssetsRoot = ''
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$outputRoot = Join-Path $root 'build/tests'
$fontFile = Join-Path $root 'resources/fonts/Nunito-Sans/NunitoSans-VF.ttf'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

$testNames = @(
    'catalog', 'score', 'vert', 'jump', 'font-data', 'physics',
    'jump-render', 'menu-registration', 'legacy', 'shield-model'
)
foreach ($testName in $testNames) {
    $sourceFile = Join-Path $root "tests/$testName-test.cpp"
    & cl.exe /nologo /EHsc /O2 /W4 /std:c++17 "/I$HeaderRoot" $sourceFile `
        "/Fe:$outputRoot/$testName.exe" "/Fo:$outputRoot/$testName.obj"
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed: $testName"
    }
    & "$outputRoot/$testName.exe" $fontFile
    if ($LASTEXITCODE -ne 0) {
        throw "Test failed: $testName"
    }
}

& cl.exe /nologo /EHsc /O2 /W4 /std:c++17 `
    "/I$HeaderRoot/../SkateHarkinian" (Join-Path $root 'tests/vhs-health-test.cpp') `
    "/Fe:$outputRoot/vhs-health.exe" "/Fo:$outputRoot/vhs-health.obj"
if ($LASTEXITCODE -ne 0) {
    throw 'VHS health fixture compile failed'
}
& "$outputRoot/vhs-health.exe"
if ($LASTEXITCODE -ne 0) {
    throw 'VHS health fixture failed'
}

if ($ImGuiRoot) {
    & cl.exe /nologo /EHsc /O2 /std:c++17 `
        /DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS /DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS `
        "/I$ImGuiRoot" (Join-Path $root 'tests/font-ui-test.cpp') `
        "$ImGuiRoot/imgui.cpp" "$ImGuiRoot/imgui_draw.cpp" `
        "$ImGuiRoot/imgui_widgets.cpp" "$ImGuiRoot/imgui_tables.cpp" `
        "/Fe:$outputRoot/font-ui.exe" "/Fo:$outputRoot/"
    if ($LASTEXITCODE -ne 0) {
        throw 'Font fixture compile failed'
    }
    & "$outputRoot/font-ui.exe" $fontFile
    if ($LASTEXITCODE -ne 0) {
        throw 'Font fixture failed'
    }
}

if ($RuntimeDll -and $AssetsRoot) {
    & cl.exe /nologo /EHsc /O2 /std:c++17 "/I$HeaderRoot" `
        (Join-Path $root 'tests/mini-native-test.cpp') `
        "/Fe:$outputRoot/mini-native.exe" "/Fo:$outputRoot/mini-native.obj"
    if ($LASTEXITCODE -ne 0) {
        throw 'Mini-ramp compile failed'
    }
    & "$outputRoot/mini-native.exe" $RuntimeDll $AssetsRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Mini-ramp failed'
    }
}

Write-Output 'PASS requested public regression suite; game was not launched'
