$ErrorActionPreference = "Stop"

$plugin = Get-Content ".\src\plugin-main.cpp" -Raw
$dialog = Get-Content ".\src\punch-settings-dialog.cpp" -Raw

$checks = @(
    @{
        Name = "frontend event callback"
        Ok = $plugin -match 'punch_frontend_event'
    },
    @{
        Name = "PROFILE_CHANGED handling"
        Ok = $plugin -match 'OBS_FRONTEND_EVENT_PROFILE_CHANGED'
    },
    @{
        Name = "profile settings reload"
        Ok = $plugin -match 'load_runtime_settings_from_profile\(\)'
    },
    @{
        Name = "profile hotkey reload"
        Ok = (
            $plugin -match 'OBS_FRONTEND_EVENT_PROFILE_CHANGED[\s\S]{0,600}load_saved_hotkey\(\)'
        )
    },
    @{
        Name = "frontend callback registration"
        Ok = $plugin -match 'obs_frontend_add_event_callback'
    },
    @{
        Name = "frontend callback cleanup"
        Ok = $plugin -match 'obs_frontend_remove_event_callback'
    },
    @{
        Name = "camera status validates video capability"
        Ok = (
            $dialog -match 'obs_source_get_output_flags' -and
            $dialog -match 'OBS_SOURCE_VIDEO'
        )
    }
)

$missing = @(
    $checks |
    Where-Object { -not $_.Ok } |
    ForEach-Object { $_.Name }
)

if ($missing.Count -gt 0) {
    Write-Host "TASK 6 RED confirmado."
    Write-Host "Falta:"
    $missing | ForEach-Object {
        Write-Host " - $_"
    }
    exit 1
}

Write-Host "TASK 6 profile lifecycle contract: PASS"
exit 0
