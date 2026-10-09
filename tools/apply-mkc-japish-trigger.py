from pathlib import Path

PLUGIN_PATH = Path(__file__).resolve().parent.parent / "src" / "plugin-main.cpp"
EVENT_NAME = r"Local\AndyAzhTEC.PunchStreamer.TriggerJapish.v1"


def require_once(data: bytes, needle: bytes, name: str) -> int:
    count = data.count(needle)
    if count != 1:
        raise RuntimeError(f"JAPISH patch stopped: '{name}' matched {count} times")
    return data.index(needle)


def newline_after(data: bytes, index: int) -> bytes:
    crlf = data.find(b"\r\n", index)
    lf = data.find(b"\n", index)
    if crlf != -1 and crlf == lf - 1:
        return b"\r\n"
    if lf != -1:
        return b"\n"
    return b"\n"


def insert_after(data: bytes, needle: bytes, payload: bytes, name: str) -> bytes:
    index = require_once(data, needle, name)
    end = index + len(needle)
    print(f"OK  {name}")
    return data[:end] + payload + data[end:]


def insert_before(data: bytes, needle: bytes, payload: bytes, name: str) -> bytes:
    index = require_once(data, needle, name)
    print(f"OK  {name}")
    return data[:index] + payload + data[index:]


def main() -> None:
    original = PLUGIN_PATH.read_bytes()
    if b"AndyAzhTEC.PunchStreamer.TriggerJapish.v1" in original:
        raise RuntimeError("JAPISH patch stopped: Named Event bridge already present")

    data = original

    include_needle = b"#include <cstring>"
    include_index = require_once(data, include_needle, "Windows include")
    nl = newline_after(data, include_index)
    include_payload = (
        nl
        + b"#ifdef _WIN32" + nl
        + b"#include <windows.h>" + nl
        + b"#endif"
    )
    data = insert_after(data, include_needle, include_payload, "Windows include")

    bridge_needle = b"static void release_camera_distortion(void);"
    bridge_index = require_once(data, bridge_needle, "Named Event bridge")
    nl = newline_after(data, bridge_index)
    bridge_lines = [
        b"",
        b"bool trigger_japish();",
        b"",
        b"#ifdef _WIN32",
        b"static HANDLE japish_remote_event = NULL;",
        b"static constexpr wchar_t JAPISH_REMOTE_EVENT_NAME[] =",
        b'        L"Local\\\\AndyAzhTEC.PunchStreamer.TriggerJapish.v1";',
        b"#endif",
        b"",
        b"static bool ensure_japish_remote_event(void)",
        b"{",
        b"#ifdef _WIN32",
        b"        if (japish_remote_event)",
        b"                return true;",
        b"",
        b"        japish_remote_event = CreateEventW(",
        b"                NULL,",
        b"                FALSE,",
        b"                FALSE,",
        b"                JAPISH_REMOTE_EVENT_NAME);",
        b"",
        b"        if (!japish_remote_event) {",
        b"                obs_log(",
        b"                        LOG_ERROR,",
        b'                        "Could not create MKC JAPISH event (error=%lu)",',
        b"                        (unsigned long)GetLastError());",
        b"                return false;",
        b"        }",
        b"",
        b'        obs_log(LOG_INFO, "MKC JAPISH event ready");',
        b"        return true;",
        b"#else",
        b"        return false;",
        b"#endif",
        b"}",
        b"",
        b"static void poll_japish_remote_event(void)",
        b"{",
        b"#ifdef _WIN32",
        b"        if (!japish_remote_event)",
        b"                return;",
        b"",
        b"        const DWORD state =",
        b"                WaitForSingleObject(japish_remote_event, 0);",
        b"",
        b"        if (state == WAIT_OBJECT_0) {",
        b'                obs_log(LOG_INFO, "JAPISH remote trigger from MKC");',
        b"                trigger_japish();",
        b"        }",
        b"#endif",
        b"}",
        b"",
        b"static void release_japish_remote_event(void)",
        b"{",
        b"#ifdef _WIN32",
        b"        if (!japish_remote_event)",
        b"                return;",
        b"",
        b"        CloseHandle(japish_remote_event);",
        b"        japish_remote_event = NULL;",
        b"#endif",
        b"}",
    ]
    bridge_payload = nl + nl.join(bridge_lines)
    data = insert_after(data, bridge_needle, bridge_payload, "Named Event bridge")

    poll_needle = b"        process_capture_probe();"
    poll_index = require_once(data, poll_needle, "poll event on OBS tick")
    nl = newline_after(data, poll_index)
    data = insert_before(
        data,
        poll_needle,
        b"        poll_japish_remote_event();" + nl,
        "poll event on OBS tick",
    )

    load_needle = b"        load_runtime_settings_from_profile();"
    load_index = require_once(data, load_needle, "create event on module load")
    nl = newline_after(data, load_index)
    data = insert_before(
        data,
        load_needle,
        b"        ensure_japish_remote_event();" + nl,
        "create event on module load",
    )

    unload_needle = b"        release_japish_audio();"
    unload_index = require_once(data, unload_needle, "close event on module unload")
    nl = newline_after(data, unload_index)
    data = insert_after(
        data,
        unload_needle,
        nl + b"        release_japish_remote_event();",
        "close event on module unload",
    )

    if data == original:
        raise RuntimeError("JAPISH patch stopped: no changes generated")

    PLUGIN_PATH.write_bytes(data)
    print("MKC JAPISH byte-preserving patch applied successfully.")


if __name__ == "__main__":
    main()
