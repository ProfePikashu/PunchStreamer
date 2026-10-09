#include "punch-settings.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

static void fail(const char *message)
{
        std::cerr << message << "\n";
        std::exit(1);
}

static void expect_close(const char *name, float actual, float expected)
{
        if (std::fabs(actual - expected) > 0.0001f) {
                std::cerr
                        << name
                        << ": expected "
                        << expected
                        << ", got "
                        << actual
                        << "\n";
                std::exit(1);
        }
}

static void test_recommended()
{
        const PunchSettings s = punch_settings_recommended();

        if (s.preset != PunchPreset::Recommended)
                fail("Recommended preset mismatch");

        expect_close("punch_scale", s.punch_scale, 4.25f);
        expect_close("impact_offset_x", s.impact_offset_x, 120.0f);
        expect_close("impact_offset_y", s.impact_offset_y, -12.0f);
        expect_close("punch_speed", s.punch_speed, 1.0f);
        expect_close("detection_confidence", s.detection_confidence, 0.75f);
        expect_close("distortion_force", s.distortion_force, 0.40f);
        expect_close("distortion_offset_x", s.distortion_offset_x, 0.000f);
        expect_close("distortion_offset_y", s.distortion_offset_y, 0.008f);
        expect_close("distortion_left", s.distortion_left, 1.0f);
        expect_close("distortion_right", s.distortion_right, 1.0f);
        expect_close("distortion_up", s.distortion_up, 1.0f);
        expect_close("distortion_down", s.distortion_down, 1.0f);
        expect_close("impact_volume", s.impact_volume, 1.0f);
}

static void test_presets()
{
        const PunchSettings subtle = punch_settings_subtle();

        expect_close("subtle punch", subtle.punch_scale, 3.60f);
        expect_close("subtle force", subtle.distortion_force, 0.24f);
        expect_close("subtle left", subtle.distortion_left, 0.75f);
        expect_close("subtle right", subtle.distortion_right, 0.75f);
        expect_close("subtle up", subtle.distortion_up, 0.75f);
        expect_close("subtle down", subtle.distortion_down, 0.75f);
        expect_close("subtle volume", subtle.impact_volume, 0.85f);

        const PunchSettings cartoon = punch_settings_cartoon();

        expect_close("cartoon punch", cartoon.punch_scale, 5.00f);
        expect_close("cartoon speed", cartoon.punch_speed, 1.10f);
        expect_close("cartoon force", cartoon.distortion_force, 0.60f);
        expect_close("cartoon left", cartoon.distortion_left, 0.80f);
        expect_close("cartoon right", cartoon.distortion_right, 1.35f);
        expect_close("cartoon up", cartoon.distortion_up, 0.85f);
        expect_close("cartoon down", cartoon.distortion_down, 1.15f);
        expect_close("cartoon volume", cartoon.impact_volume, 1.25f);
}

static void test_clamping()
{
        PunchSettings s = punch_settings_recommended();

        s.punch_scale = 100.0f;
        s.impact_offset_x = -900.0f;
        s.impact_offset_y = 900.0f;
        s.punch_speed = 0.1f;
        s.detection_confidence = 1.5f;
        s.distortion_force = -2.0f;
        s.distortion_offset_x = 1.0f;
        s.distortion_offset_y = -1.0f;
        s.distortion_left = 0.01f;
        s.distortion_right = 9.0f;
        s.distortion_up = 9.0f;
        s.distortion_down = 0.01f;
        s.impact_volume = 8.0f;

        punch_settings_clamp(s);

        expect_close("clamp punch", s.punch_scale, 8.0f);
        expect_close("clamp x", s.impact_offset_x, -500.0f);
        expect_close("clamp y", s.impact_offset_y, 500.0f);
        expect_close("clamp speed", s.punch_speed, 0.50f);
        expect_close("clamp confidence", s.detection_confidence, 0.95f);
        expect_close("clamp force", s.distortion_force, 0.0f);
        expect_close("clamp offset x", s.distortion_offset_x, 0.20f);
        expect_close("clamp offset y", s.distortion_offset_y, -0.20f);
        expect_close("clamp left", s.distortion_left, 0.25f);
        expect_close("clamp right", s.distortion_right, 2.50f);
        expect_close("clamp up", s.distortion_up, 2.50f);
        expect_close("clamp down", s.distortion_down, 0.25f);
        expect_close("clamp volume", s.impact_volume, 2.0f);
}

static void test_preset_detection()
{
        PunchSettings recommended = punch_settings_recommended();

        if (punch_settings_detect_preset(recommended) !=
            PunchPreset::Recommended)
                fail("Recommended detection failed");

        PunchSettings subtle = punch_settings_subtle();

        if (punch_settings_detect_preset(subtle) != PunchPreset::Subtle)
                fail("Subtle detection failed");

        PunchSettings cartoon = punch_settings_cartoon();

        if (punch_settings_detect_preset(cartoon) != PunchPreset::Cartoon)
                fail("Cartoon detection failed");

        recommended.distortion_force = 0.41f;

        if (punch_settings_detect_preset(recommended) != PunchPreset::Custom)
                fail("Custom detection failed");
}

static void test_camera_only_profile_falls_back_to_recommended()
{
        config_t *config = nullptr;

        const char *text =
                "[PunchStreamer]\n"
                "CameraSource=Camara\n";

        if (config_open_string(&config, text) != CONFIG_SUCCESS)
                fail("config_open_string failed");

        PunchSettings s;

        if (!punch_settings_load(config, s))
                fail("punch_settings_load failed");

        if (s.camera_source != "Camara")
                fail("CameraSource was not loaded");

        expect_close("fallback punch", s.punch_scale, 4.25f);
        expect_close("fallback speed", s.punch_speed, 1.0f);
        expect_close("fallback confidence", s.detection_confidence, 0.75f);
        expect_close("fallback force", s.distortion_force, 0.40f);
        expect_close("fallback volume", s.impact_volume, 1.0f);

        if (s.preset != PunchPreset::Recommended)
                fail("Camera-only profile should resolve Recommended");

        config_close(config);
}

static void test_save_roundtrip_values()
{
        config_t *config = nullptr;

        if (config_open_string(&config, "[PunchStreamer]\n") !=
            CONFIG_SUCCESS)
                fail("save config_open_string failed");

        PunchSettings s = punch_settings_cartoon();
        s.camera_source = "Webcam Publica";

        if (!punch_settings_save(config, s))
                fail("punch_settings_save failed");

        const char *camera =
                config_get_string(
                        config,
                        "PunchStreamer",
                        "CameraSource");

        if (!camera || std::string(camera) != "Webcam Publica")
                fail("CameraSource save failed");

        expect_close(
                "saved punch",
                (float)config_get_double(
                        config,
                        "PunchStreamer",
                        "PunchScale"),
                5.0f);

        expect_close(
                "saved volume",
                (float)config_get_double(
                        config,
                        "PunchStreamer",
                        "ImpactVolume"),
                1.25f);

        config_close(config);
}

static void test_punch_timing()
{
        PunchTiming normal = punch_timing_for_speed(1.0f);

        expect_close("timing normal entry", normal.entry, 0.11f);
        expect_close("timing normal hold", normal.hold, 0.02f);
        expect_close("timing normal exit", normal.exit, 0.10f);

        PunchTiming fast = punch_timing_for_speed(2.0f);

        expect_close("timing fast entry", fast.entry, 0.055f);
        expect_close("timing fast hold", fast.hold, 0.010f);
        expect_close("timing fast exit", fast.exit, 0.050f);

        PunchTiming slow = punch_timing_for_speed(0.50f);

        expect_close("timing slow entry", slow.entry, 0.22f);
        expect_close("timing slow hold", slow.hold, 0.04f);
        expect_close("timing slow exit", slow.exit, 0.20f);
}

int main()
{
        test_recommended();
        test_presets();
        test_clamping();
        test_preset_detection();
        test_camera_only_profile_falls_back_to_recommended();
        test_save_roundtrip_values();
        test_punch_timing();

        std::cout << "PunchSettings tests: PASS\n";
        return 0;
}

