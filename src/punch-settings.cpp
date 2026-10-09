#include "punch-settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

static constexpr const char *SECTION = "PunchStreamer";

static float clamp_value(float value, float minimum, float maximum)
{
        return std::max(minimum, std::min(value, maximum));
}

static bool close_enough(float a, float b)
{
        return std::fabs(a - b) <= 0.0001f;
}

static bool same_tuning(
        const PunchSettings &a,
        const PunchSettings &b)
{
        return
                close_enough(a.punch_scale, b.punch_scale) &&
                close_enough(a.impact_offset_x, b.impact_offset_x) &&
                close_enough(a.impact_offset_y, b.impact_offset_y) &&
                close_enough(a.punch_speed, b.punch_speed) &&
                close_enough(
                        a.detection_confidence,
                        b.detection_confidence) &&
                close_enough(
                        a.distortion_force,
                        b.distortion_force) &&
                close_enough(
                        a.distortion_offset_x,
                        b.distortion_offset_x) &&
                close_enough(
                        a.distortion_offset_y,
                        b.distortion_offset_y) &&
                close_enough(
                        a.distortion_left,
                        b.distortion_left) &&
                close_enough(
                        a.distortion_right,
                        b.distortion_right) &&
                close_enough(
                        a.distortion_up,
                        b.distortion_up) &&
                close_enough(
                        a.distortion_down,
                        b.distortion_down) &&
                close_enough(
                        a.impact_volume,
                        b.impact_volume);
}

static const char *preset_name(PunchPreset preset)
{
        switch (preset) {
        case PunchPreset::Recommended:
                return "Recommended";
        case PunchPreset::Subtle:
                return "Subtle";
        case PunchPreset::Cartoon:
                return "Cartoon";
        case PunchPreset::Custom:
        default:
                return "Custom";
        }
}

PunchSettings punch_settings_recommended()
{
        PunchSettings s{};

        s.camera_source = "";
        s.preset = PunchPreset::Recommended;

        s.punch_scale = 4.25f;
        s.impact_offset_x = 120.0f;
        s.impact_offset_y = -12.0f;
        s.punch_speed = 1.0f;

        s.detection_confidence = 0.75f;

        s.distortion_force = 0.40f;
        s.distortion_offset_x = 0.000f;
        s.distortion_offset_y = 0.008f;

        s.distortion_left = 1.0f;
        s.distortion_right = 1.0f;
        s.distortion_up = 1.0f;
        s.distortion_down = 1.0f;

        s.impact_volume = 1.0f;

        return s;
}

PunchSettings punch_settings_subtle()
{
        PunchSettings s = punch_settings_recommended();

        s.preset = PunchPreset::Subtle;
        s.punch_scale = 3.60f;
        s.distortion_force = 0.24f;

        s.distortion_left = 0.75f;
        s.distortion_right = 0.75f;
        s.distortion_up = 0.75f;
        s.distortion_down = 0.75f;

        s.impact_volume = 0.85f;

        return s;
}

PunchSettings punch_settings_cartoon()
{
        PunchSettings s = punch_settings_recommended();

        s.preset = PunchPreset::Cartoon;
        s.punch_scale = 5.00f;
        s.punch_speed = 1.10f;
        s.distortion_force = 0.60f;

        s.distortion_left = 0.80f;
        s.distortion_right = 1.35f;
        s.distortion_up = 0.85f;
        s.distortion_down = 1.15f;

        s.impact_volume = 1.25f;

        return s;
}

void punch_settings_clamp(PunchSettings &s)
{
        s.punch_scale =
                clamp_value(s.punch_scale, 1.0f, 8.0f);

        s.impact_offset_x =
                clamp_value(s.impact_offset_x, -500.0f, 500.0f);

        s.impact_offset_y =
                clamp_value(s.impact_offset_y, -500.0f, 500.0f);

        s.punch_speed =
                clamp_value(s.punch_speed, 0.50f, 2.00f);

        s.detection_confidence =
                clamp_value(
                        s.detection_confidence,
                        0.50f,
                        0.95f);

        s.distortion_force =
                clamp_value(s.distortion_force, 0.0f, 1.0f);

        s.distortion_offset_x =
                clamp_value(
                        s.distortion_offset_x,
                        -0.20f,
                        0.20f);

        s.distortion_offset_y =
                clamp_value(
                        s.distortion_offset_y,
                        -0.20f,
                        0.20f);

        s.distortion_left =
                clamp_value(s.distortion_left, 0.25f, 2.50f);

        s.distortion_right =
                clamp_value(s.distortion_right, 0.25f, 2.50f);

        s.distortion_up =
                clamp_value(s.distortion_up, 0.25f, 2.50f);

        s.distortion_down =
                clamp_value(s.distortion_down, 0.25f, 2.50f);

        s.impact_volume =
                clamp_value(s.impact_volume, 0.0f, 2.0f);
}

PunchPreset punch_settings_detect_preset(
        const PunchSettings &settings)
{
        if (same_tuning(
                    settings,
                    punch_settings_recommended()))
                return PunchPreset::Recommended;

        if (same_tuning(
                    settings,
                    punch_settings_subtle()))
                return PunchPreset::Subtle;

        if (same_tuning(
                    settings,
                    punch_settings_cartoon()))
                return PunchPreset::Cartoon;

        return PunchPreset::Custom;
}

static void load_float_if_present(
        config_t *config,
        const char *name,
        float &value)
{
        if (config_has_user_value(
                    config,
                    SECTION,
                    name)) {
                value =
                        (float)config_get_double(
                                config,
                                SECTION,
                                name);
        }
}

bool punch_settings_load(
        config_t *config,
        PunchSettings &settings)
{
        if (!config)
                return false;

        settings = punch_settings_recommended();

        if (config_has_user_value(
                    config,
                    SECTION,
                    "CameraSource")) {
                const char *camera =
                        config_get_string(
                                config,
                                SECTION,
                                "CameraSource");

                settings.camera_source =
                        camera ? camera : "";
        }

        load_float_if_present(
                config,
                "PunchScale",
                settings.punch_scale);

        load_float_if_present(
                config,
                "ImpactOffsetX",
                settings.impact_offset_x);

        load_float_if_present(
                config,
                "ImpactOffsetY",
                settings.impact_offset_y);

        load_float_if_present(
                config,
                "PunchSpeed",
                settings.punch_speed);

        load_float_if_present(
                config,
                "DetectionConfidence",
                settings.detection_confidence);

        load_float_if_present(
                config,
                "DistortionForce",
                settings.distortion_force);

        load_float_if_present(
                config,
                "DistortionOffsetX",
                settings.distortion_offset_x);

        load_float_if_present(
                config,
                "DistortionOffsetY",
                settings.distortion_offset_y);

        load_float_if_present(
                config,
                "DistortionLeft",
                settings.distortion_left);

        load_float_if_present(
                config,
                "DistortionRight",
                settings.distortion_right);

        load_float_if_present(
                config,
                "DistortionUp",
                settings.distortion_up);

        load_float_if_present(
                config,
                "DistortionDown",
                settings.distortion_down);

        load_float_if_present(
                config,
                "ImpactVolume",
                settings.impact_volume);

        punch_settings_clamp(settings);

        settings.preset =
                punch_settings_detect_preset(settings);

        return true;
}

bool punch_settings_save(
        config_t *config,
        const PunchSettings &input)
{
        if (!config)
                return false;

        PunchSettings settings = input;
        punch_settings_clamp(settings);

        settings.preset =
                punch_settings_detect_preset(settings);

        config_set_string(
                config,
                SECTION,
                "CameraSource",
                settings.camera_source.c_str());

        config_set_string(
                config,
                SECTION,
                "Preset",
                preset_name(settings.preset));

        config_set_double(
                config,
                SECTION,
                "PunchScale",
                settings.punch_scale);

        config_set_double(
                config,
                SECTION,
                "ImpactOffsetX",
                settings.impact_offset_x);

        config_set_double(
                config,
                SECTION,
                "ImpactOffsetY",
                settings.impact_offset_y);

        config_set_double(
                config,
                SECTION,
                "PunchSpeed",
                settings.punch_speed);

        config_set_double(
                config,
                SECTION,
                "DetectionConfidence",
                settings.detection_confidence);

        config_set_double(
                config,
                SECTION,
                "DistortionForce",
                settings.distortion_force);

        config_set_double(
                config,
                SECTION,
                "DistortionOffsetX",
                settings.distortion_offset_x);

        config_set_double(
                config,
                SECTION,
                "DistortionOffsetY",
                settings.distortion_offset_y);

        config_set_double(
                config,
                SECTION,
                "DistortionLeft",
                settings.distortion_left);

        config_set_double(
                config,
                SECTION,
                "DistortionRight",
                settings.distortion_right);

        config_set_double(
                config,
                SECTION,
                "DistortionUp",
                settings.distortion_up);

        config_set_double(
                config,
                SECTION,
                "DistortionDown",
                settings.distortion_down);

        config_set_double(
                config,
                SECTION,
                "ImpactVolume",
                settings.impact_volume);

        return true;
}

PunchTiming punch_timing_for_speed(float speed)
{
        speed = clamp_value(speed, 0.50f, 2.00f);

        PunchTiming timing{};

        timing.entry = 0.11f / speed;
        timing.hold = 0.02f / speed;
        timing.exit = 0.10f / speed;

        return timing;
}
