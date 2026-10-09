$ErrorActionPreference = "Stop"

$src = Get-Content ".\src\plugin-main.cpp" -Raw

$dead = @(
    "tracked_face_x",
    "tracked_face_y",
    "calibrated_impact_pos",
    "calibrated_impact_ready",
    "BASE_FACE_X",
    "BASE_FACE_Y",
    "PUNCH_FACTOR_X",
    "PUNCH_FACTOR_Y"
)

$remaining = @(
    $dead |
    Where-Object {
        $src -match "\b$([regex]::Escape($_))\b"
    }
)

if ($remaining.Count -gt 0) {
    Write-Host "TASK 7 RED confirmado."
    Write-Host "Codigo prototipo muerto todavia presente:"
    $remaining | ForEach-Object {
        Write-Host " - $_"
    }
    exit 1
}

Write-Host "TASK 7 dead prototype cleanup contract: PASS"
exit 0
