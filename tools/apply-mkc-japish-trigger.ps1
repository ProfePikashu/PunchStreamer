$ErrorActionPreference = 'Stop'

$pluginPath = Join-Path $PSScriptRoot '..\src\plugin-main.cpp'
$pluginPath = [System.IO.Path]::GetFullPath($pluginPath)
$rawText = [System.IO.File]::ReadAllText($pluginPath)
$useCrLf = $rawText.Contains("`r`n")
$text = $rawText.Replace("`r`n", "`n")

function Replace-Required([string]$name, [string]$old, [string]$new) {
    if (-not $script:text.Contains($old)) {
        throw "JAPISH plugin patch stopped: expected block not found: $name"
    }
    $script:text = $script:text.Replace($old, $new)
    Write-Host "OK  $name"
}

Replace-Required 'Windows include' `
    "#include <cstring>`n" `
    "#include <cstring>`n`n#ifdef _WIN32`n#include <windows.h>`n#endif`n"

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

Replace-Required 'Named Event bridge' `
    "static void release_camera_distortion(void);`n" `
    ($eventBlock + "`n")

Replace-Required 'poll event on OBS tick' `
    "        (void)data;`n`n        process_capture_probe();" `
    "        (void)data;`n`n        poll_japish_remote_event();`n        process_capture_probe();"

Replace-Required 'create event on module load' `
    "        obs_log(LOG_INFO, \"OpenCV runtime: %s\", cv::getVersionString().c_str());`n`n        load_runtime_settings_from_profile();" `
    "        obs_log(LOG_INFO, \"OpenCV runtime: %s\", cv::getVersionString().c_str());`n`n        ensure_japish_remote_event();`n        load_runtime_settings_from_profile();"

Replace-Required 'close event on module unload' `
    "        release_japish_audio();`n`n        if (active_punch_item)" `
    "        release_japish_audio();`n        release_japish_remote_event();`n`n        if (active_punch_item)"

$output = if ($useCrLf) { $text.Replace("`n", "`r`n") } else { $text }

[System.IO.File]::WriteAllText(
    $pluginPath,
    $output,
    [System.Text.UTF8Encoding]::new($false))

Write-Host 'MKC JAPISH trigger patch applied successfully.'
