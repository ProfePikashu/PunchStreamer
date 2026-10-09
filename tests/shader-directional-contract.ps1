$ErrorActionPreference = "Stop"

$shader = Get-Content ".\data\assets\punch_distortion.effect" -Raw
$plugin = Get-Content ".\src\plugin-main.cpp" -Raw

$requiredShader = @(
    "Reach_Left",
    "Reach_Right",
    "Reach_Up",
    "Reach_Down"
)

$requiredPlugin = @(
    '"Reach_Left"',
    '"Reach_Right"',
    '"Reach_Up"',
    '"Reach_Down"',
    "distortion_left",
    "distortion_right",
    "distortion_up",
    "distortion_down"
)

$missing = @()

foreach ($token in $requiredShader) {
    if (-not $shader.Contains($token)) {
        $missing += "shader:$token"
    }
}

foreach ($token in $requiredPlugin) {
    if (-not $plugin.Contains($token)) {
        $missing += "plugin:$token"
    }
}

if ($missing.Count -gt 0) {
    Write-Host "TASK 3 RED confirmado."
    Write-Host "Faltan:"
    $missing | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host "TASK 3 shader contract: PASS"
exit 0
