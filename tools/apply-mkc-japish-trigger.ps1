$ErrorActionPreference = 'Stop'

$pluginPath = Join-Path $PSScriptRoot '..\src\plugin-main.cpp'
$pluginPath = [System.IO.Path]::GetFullPath($pluginPath)
$rawText = [System.IO.File]::ReadAllText($pluginPath)
$useCrLf = $rawText.Contains("`r`n")
$text = $rawText.Replace("`r`n", "`n")

function Replace-One([string]$name, [string]$inputText, [string]$pattern, [string]$replacement) {
    $regex = [System.Text.RegularExpressions.Regex]::new(
        $pattern,
        [System.Text.RegularExpressions.RegexOptions]::Singleline)

    $matches = $regex.Matches($inputText)
    if ($matches.Count -ne 1) {
        throw "JAPISH plugin patch stopped: '$name' matched $($matches.Count) times"
    }

    Write-Host "OK  $name"
    return $regex.Replace($inputText, $replacement, 1)
}

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

$next = $text

$next = Replace-One `
    'Windows include' `
    $next `
    '(#include\s*<cstring>\s*\n)' `
    ('$1' + "`n#ifdef _WIN32`n#include <windows.h>`n#endif`n")

$next = Replace-One `
    'Named Event bridge' `
    $next `
    'static\s+void\s+release_camera_distortion\s*\(\s*void\s*\)\s*;\s*\n' `
    ($eventBlock + "`n")

$next = Replace-One `
    'poll event on OBS tick' `
    $next `
    '(static\s+void\s+punch_tick\s*\([^)]*\)\s*\{\s*\(void\)data\s*;)' `
    ('$1' + "`n`n        poll_japish_remote_event();")

$next = Replace-One `
    'create event on module load' `
    $next `
    '(bool\s+obs_module_load\s*\(\s*void\s*\)\s*\{)' `
    ('$1' + "`n        ensure_japish_remote_event();")

$next = Replace-One `
    'close event on module unload' `
    $next `
    '(void\s+obs_module_unload\s*\(\s*void\s*\)\s*\{)' `
    ('$1' + "`n        release_japish_remote_event();")

$output = if ($useCrLf) { $next.Replace("`n", "`r`n") } else { $next }

[System.IO.File]::WriteAllText(
    $pluginPath,
    $output,
    [System.Text.UTF8Encoding]::new($false))

Write-Host 'MKC JAPISH trigger patch applied successfully.'
