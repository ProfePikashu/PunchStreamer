$ErrorActionPreference = 'Stop'

$pluginPath = Join-Path $PSScriptRoot '..\src\plugin-main.cpp'
$pluginPath = [System.IO.Path]::GetFullPath($pluginPath)
$text = [System.IO.File]::ReadAllText($pluginPath)

function Replace-Required([string]$name, [string]$old, [string]$new) {
    if (-not $script:text.Contains($old)) {
        throw "JAPISH plugin patch stopped: expected block not found: $name"
    }
    $script:text = $script:text.Replace($old, $new)
    Write-Host "OK  $name"
}

Replace-Required 'Windows include' `
    "#include <cstring>`r`n" `
    "#include <cstring>`r`n`r`n#ifdef _WIN32`r`n#include <windows.h>`r`n#endif`r`n"

$eventBlock = @'
static void release_camera_distortion(void);

bool trigger_japish();

#ifdef _WIN32
static HANDLE japish_remote_event = NULL;
static constexpr wchar_t JAPISH_REMOTE_EVENT_NAME[] =
        L"Local\\AndyAzhTEC.PunchStreamer.TriggerJapish.v1";
#endif

static bool ensure_japish_remote_event(void)
{
#ifdef _WIN32
        if (japish_remote_event)
                return true;

        japish_remote_event = CreateEventW(
                NULL,
                FALSE,
                FALSE,
                JAPISH_REMOTE_EVENT_NAME);

        if (!japish_remote_event) {
                obs_log(
                        LOG_ERROR,
                        "Could not create MKC JAPISH event (error=%lu)",
                        (unsigned long)GetLastError());
                return false;
        }

        obs_log(LOG_INFO, "MKC JAPISH event ready");
        return true;
#else
        return false;
#endif
}

static void poll_japish_remote_event(void)
{
#ifdef _WIN32
        if (!japish_remote_event)
                return;

        const DWORD state =
                WaitForSingleObject(japish_remote_event, 0);

        if (state == WAIT_OBJECT_0) {
                obs_log(LOG_INFO, "JAPISH remote trigger from MKC");
                trigger_japish();
        }
#endif
}

static void release_japish_remote_event(void)
{
#ifdef _WIN32
        if (!japish_remote_event)
                return;

        CloseHandle(japish_remote_event);
        japish_remote_event = NULL;
#endif
}
'@
$eventBlock = $eventBlock -replace "`n", "`r`n"

Replace-Required 'Named Event bridge' `
    "static void release_camera_distortion(void);`r`n" `
    ($eventBlock + "`r`n")

Replace-Required 'poll event on OBS tick' `
    "        (void)data;`r`n`r`n        process_capture_probe();" `
    "        (void)data;`r`n`r`n        poll_japish_remote_event();`r`n        process_capture_probe();"

Replace-Required 'create event on module load' `
    "        obs_log(LOG_INFO, \"OpenCV runtime: %s\", cv::getVersionString().c_str());`r`n`r`n        load_runtime_settings_from_profile();" `
    "        obs_log(LOG_INFO, \"OpenCV runtime: %s\", cv::getVersionString().c_str());`r`n`r`n        ensure_japish_remote_event();`r`n        load_runtime_settings_from_profile();"

Replace-Required 'close event on module unload' `
    "        release_japish_audio();`r`n`r`n        if (active_punch_item)" `
    "        release_japish_audio();`r`n        release_japish_remote_event();`r`n`r`n        if (active_punch_item)"

[System.IO.File]::WriteAllText(
    $pluginPath,
    $text,
    [System.Text.UTF8Encoding]::new($false))

Write-Host 'MKC JAPISH trigger patch applied successfully.'
