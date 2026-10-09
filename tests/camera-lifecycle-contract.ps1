$ErrorActionPreference = "Stop"

$src = Get-Content ".\src\plugin-main.cpp" -Raw

$required = @(
    "frame_tap_camera_name",
    "distortion_camera_name",
    "release_camera_tap",
    "camera_source_name = next.camera_source",
    "frame_tap_camera_name != camera_source_name",
    "distortion_camera_name != camera_source_name",
    "release_camera_tap();"
)

$missing = @()

foreach ($token in $required) {
    if (-not $src.Contains($token)) {
        $missing += $token
    }
}

if ($missing.Count -gt 0) {
    Write-Host "TASK 5 RED confirmado."
    Write-Host "Faltan:"
    $missing | ForEach-Object {
        Write-Host " - $_"
    }
    exit 1
}

Write-Host "TASK 5 lifecycle contract: PASS"
exit 0
