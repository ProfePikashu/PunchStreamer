$ErrorActionPreference = "Stop"

$cmake  = Get-Content ".\CMakeLists.txt" -Raw
$header = if (Test-Path ".\src\punch-settings-dialog.hpp") {
    Get-Content ".\src\punch-settings-dialog.hpp" -Raw
} else { "" }

$cpp = if (Test-Path ".\src\punch-settings-dialog.cpp") {
    Get-Content ".\src\punch-settings-dialog.cpp" -Raw
} else { "" }

$all = $header + "`n" + $cpp

$required = @(
    "PunchSettingsDialog",
    "obs_frontend_add_tools_menu",
    "Recommended",
    "Subtle",
    "Cartoon",
    "Custom",
    "Camera",
    "Test JAPISH",
    "Punch Size",
    "Impact X",
    "Impact Y",
    "Punch Speed",
    "Detection Confidence",
    "Distortion Force",
    "Distortion X",
    "Distortion Y",
    "Left Reach",
    "Right Reach",
    "Up Reach",
    "Down Reach",
    "Volume",
    "Save",
    "Cancel",
    "Restore Recommended"
)

$missing = @()

if ($cmake -notmatch 'ENABLE_QT') {
    $missing += "cmake:ENABLE_QT"
}

foreach ($token in $required) {
    if (-not $all.Contains($token)) {
        $missing += "ui:$token"
    }
}

if ($missing.Count -gt 0) {
    Write-Host "TASK 4 RED confirmado."
    Write-Host "Faltan:"
    $missing | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host "TASK 4 UI contract: PASS"
exit 0
