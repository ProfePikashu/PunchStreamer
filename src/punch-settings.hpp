#pragma once

#include <string>
#include <util/config-file.h>

enum class PunchPreset {
        Recommended,
        Subtle,
        Cartoon,
        Custom
};

struct PunchSettings {
        std::string camera_source;
        PunchPreset preset;

        float punch_scale;
        float impact_offset_x;
        float impact_offset_y;
        float punch_speed;

        float detection_confidence;

        float distortion_force;
        float distortion_offset_x;
        float distortion_offset_y;
        float distortion_left;
        float distortion_right;
        float distortion_up;
        float distortion_down;

        float impact_volume;
};

PunchSettings punch_settings_recommended();
PunchSettings punch_settings_subtle();
PunchSettings punch_settings_cartoon();

void punch_settings_clamp(PunchSettings &settings);
PunchPreset punch_settings_detect_preset(const PunchSettings &settings);

bool punch_settings_load(config_t *config, PunchSettings &settings);
bool punch_settings_save(config_t *config, const PunchSettings &settings);

struct PunchTiming {
        float entry;
        float hold;
        float exit;
};

PunchTiming punch_timing_for_speed(float speed);

PunchSettings get_runtime_settings();
void apply_runtime_settings(const PunchSettings &settings);
bool trigger_japish();

bool punch_face_detector_ready();