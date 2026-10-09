#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include "punch-settings.hpp"

#ifdef ENABLE_QT
#include "punch-settings-dialog.hpp"
#endif
#include <util/bmem.h>
#include <util/config-file.h>
#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <graphics/matrix4.h>
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define PUNCH_SOURCE_NAME "Punch Streamer - Punch"

static obs_hotkey_id japish_hotkey = OBS_INVALID_HOTKEY_ID;
static cv::Ptr<cv::FaceDetectorYN> face_detector;

bool punch_face_detector_ready()
{
        return !face_detector.empty();
}

static std::string camera_source_name;
static void release_camera_tap(void);
static void release_camera_distortion(void);

static PunchSettings runtime_settings =
        punch_settings_recommended();

static std::mutex runtime_settings_mutex;

static std::atomic<float> runtime_distortion_left{1.0f};
static std::atomic<float> runtime_distortion_right{1.0f};
static std::atomic<float> runtime_distortion_up{1.0f};
static std::atomic<float> runtime_distortion_down{1.0f};

PunchSettings get_runtime_settings()
{
        std::lock_guard<std::mutex> lock(
                runtime_settings_mutex);

        return runtime_settings;
}

void apply_runtime_settings(
        const PunchSettings &input)
{
        PunchSettings next = input;

        punch_settings_clamp(next);
        next.preset =
                punch_settings_detect_preset(next);

        std::lock_guard<std::mutex> lock(
                runtime_settings_mutex);

        const bool camera_changed =
                camera_source_name != next.camera_source;

        runtime_settings = next;
        camera_source_name = next.camera_source;

        if (camera_changed) {
                release_camera_tap();
                release_camera_distortion();
        }

        runtime_distortion_left.store(
                next.distortion_left);

        runtime_distortion_right.store(
                next.distortion_right);

        runtime_distortion_up.store(
                next.distortion_up);

        runtime_distortion_down.store(
                next.distortion_down);
}

static bool load_yunet(void)
{
        char *model_path = obs_module_file("assets/face_detection_yunet.onnx");

        if (!model_path) {
                obs_log(LOG_ERROR, "Could not resolve YuNet model path");
                return false;
        }

        try {
                face_detector = cv::FaceDetectorYN::create(
                        model_path,
                        "",
                        cv::Size(320, 320),
                        get_runtime_settings().detection_confidence,
                        0.3f,
                        5000);

                bfree(model_path);

                if (face_detector.empty()) {
                        obs_log(LOG_ERROR, "YuNet detector creation failed");
                        return false;
                }

                obs_log(LOG_INFO, "YuNet loaded successfully");
                return true;
        }
        catch (const cv::Exception &e) {
                obs_log(LOG_ERROR, "YuNet load failed: %s", e.what());
                bfree(model_path);
                return false;
        }
}


static void load_saved_hotkey(void)
{
        config_t *config = obs_frontend_get_profile_config();
        if (!config)
                return;

        const char *json = config_get_string(config, "Hotkeys", "punch_streamer_japish");
        if (!json || !*json)
                return;

        obs_data_t *data = obs_data_create_from_json(json);
        if (!data)
                return;

        obs_data_array_t *bindings = obs_data_get_array(data, "bindings");
        if (bindings) {
                obs_hotkey_load(japish_hotkey, bindings);
                obs_data_array_release(bindings);
                obs_log(LOG_INFO, "JAPISH hotkey restored");
        }

        obs_data_release(data);
}

static bool load_runtime_settings_from_profile(void)
{
        config_t *config =
                obs_frontend_get_profile_config();

        if (!config)
                return false;

        PunchSettings loaded;

        if (!punch_settings_load(config, loaded))
                return false;

        apply_runtime_settings(loaded);
        camera_source_name = loaded.camera_source;

        if (camera_source_name.empty()) {
                obs_log(
                        LOG_WARNING,
                        "No camera source configured");

                return false;
        }

        obs_log(
                LOG_INFO,
                "Camera source configured: %s",
                camera_source_name.c_str());

        return true;
}
static obs_sceneitem_t *active_punch_item = NULL;
static float punch_elapsed = 0.0f;

static struct vec2 punch_impact_pos;
static struct vec2 punch_rest_pos;

#define PUNCH_IN_DURATION   0.11f
#define PUNCH_HOLD_DURATION 0.02f
#define PUNCH_OUT_DURATION  0.10f
#define PUNCH_TRAVEL_X      620.0f

static PunchTiming active_punch_timing{
        0.11f,
        0.02f,
        0.10f
};

static float active_distortion_force = 0.40f;

static float clamp01(float value)
{
        if (value < 0.0f)
                return 0.0f;
        if (value > 1.0f)
                return 1.0f;
        return value;
}

static float ease_out_cubic(float t)
{
        float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
}

static float ease_in_cubic(float t)
{
        return t * t * t;
}

static void set_punch_x(float x)
{
        struct vec2 pos = punch_impact_pos;
        pos.x = x;
        obs_sceneitem_set_pos(active_punch_item, &pos);
}

static struct vec2 tracked_scene_target = {0.0f, 0.0f};
static bool tracked_scene_target_ready = false;
static float tracked_face_scene_width = 180.0f;

#define PUNCH_FACE_WIDTH_FACTOR 4.25f

#define IMPACT_OFFSET_X 120.0f
#define IMPACT_OFFSET_Y -12.0f

#define JAPISH_FORCE 0.40f
#define JAPISH_RADIUS 0.80f
#define JAPISH_CENTER_OFFSET_X 0.000f
#define JAPISH_CENTER_OFFSET_Y 0.008f

static std::atomic<float> distortion_strength{0.0f};
static std::atomic<float> distortion_center_x{0.5f};
static std::atomic<float> distortion_center_y{0.5f};


static obs_source_t *japish_audio_source = NULL;
static bool japish_audio_fired = false;

static void ensure_japish_audio(void)
{
        if (japish_audio_source)
                return;

        char *audio_path =
                obs_module_file("assets/japish.mp3");

        if (!audio_path) {
                obs_log(LOG_ERROR,
                        "Could not resolve japish.mp3 path");
                return;
        }

        obs_data_t *settings = obs_data_create();

        obs_data_set_bool(settings,
                          "is_local_file",
                          true);

        obs_data_set_string(settings,
                            "local_file",
                            audio_path);

        obs_data_set_bool(settings,
                          "looping",
                          false);

        obs_data_set_bool(settings,
                          "restart_on_activate",
                          true);

        obs_data_set_bool(settings,
                          "close_when_inactive",
                          false);

        obs_data_set_bool(settings,
                          "clear_on_media_end",
                          true);

        japish_audio_source =
                obs_source_create_private(
                        "ffmpeg_source",
                        "Punch Streamer - JAPISH Audio",
                        settings);

        obs_data_release(settings);
        bfree(audio_path);

        if (!japish_audio_source) {
                obs_log(LOG_ERROR,
                        "Could not create JAPISH audio source");
                return;
        }

        /*
         * Primero active y despuÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â©s showing:
         * evita que se reproduzca durante la preparaciÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â³n.
         */
        obs_source_inc_active(japish_audio_source);
        obs_source_inc_showing(japish_audio_source);

        obs_source_set_monitoring_type(
                japish_audio_source,
                OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);

        obs_source_set_volume(
                japish_audio_source,
                get_runtime_settings().impact_volume);

        obs_source_media_stop(japish_audio_source);

        obs_log(LOG_INFO,
                "JAPISH audio ready");
}

static void play_japish_audio(void)
{
        if (!japish_audio_source)
                return;

        obs_source_set_volume(
                japish_audio_source,
                get_runtime_settings().impact_volume);

        obs_source_media_restart(japish_audio_source);

        obs_log(LOG_INFO,
                "JAPISH impact audio");
}

static void release_japish_audio(void)
{
        if (!japish_audio_source)
                return;

        obs_source_media_stop(japish_audio_source);

        obs_source_dec_showing(japish_audio_source);
        obs_source_dec_active(japish_audio_source);

        obs_source_release(japish_audio_source);
        japish_audio_source = NULL;
}
static void arm_punch_animation(obs_sceneitem_t *item)
{
        const PunchSettings punch_settings =
                get_runtime_settings();

        active_punch_timing =
                punch_timing_for_speed(
                        punch_settings.punch_speed);

        active_distortion_force =
                punch_settings.distortion_force;

        japish_audio_fired = false;
        if (active_punch_item) {
                obs_sceneitem_set_pos(active_punch_item, &punch_impact_pos);
                obs_sceneitem_set_visible(active_punch_item, false);
                obs_sceneitem_release(active_punch_item);
        }

        active_punch_item = item;
        obs_sceneitem_addref(active_punch_item);

        obs_sceneitem_set_alignment(active_punch_item, OBS_ALIGN_RIGHT);

        obs_source_t *punch_source =
                obs_sceneitem_get_source(active_punch_item);

        uint32_t punch_source_width =
                obs_source_get_width(punch_source);

        if (punch_source_width > 0 &&
            tracked_face_scene_width > 0.0f) {
                float desired_width =
                        tracked_face_scene_width *
                        punch_settings.punch_scale;

                float punch_scale =
                        desired_width /
                        (float)punch_source_width;

                struct vec2 scale;
                vec2_set(&scale, punch_scale, punch_scale);
                obs_sceneitem_set_scale(active_punch_item, &scale);
        }

        if (tracked_scene_target_ready)
                punch_impact_pos = tracked_scene_target;
        else
                obs_sceneitem_get_pos(active_punch_item, &punch_impact_pos);

        punch_rest_pos = punch_impact_pos;
        punch_rest_pos.x -= PUNCH_TRAVEL_X;

        punch_elapsed = 0.0f;

        obs_sceneitem_set_pos(active_punch_item, &punch_rest_pos);
        obs_sceneitem_set_visible(active_punch_item, true);
}

static bool map_detector_point_to_scene(float detector_x,
                                        float detector_y,
                                        struct vec2 *result)
{
        obs_source_t *scene_source = obs_frontend_get_current_scene();
        if (!scene_source)
                return false;

        obs_scene_t *scene = obs_scene_from_source(scene_source);
        if (!scene) {
                obs_source_release(scene_source);
                return false;
        }

        obs_sceneitem_t *camera_item =
                obs_scene_find_source_recursive(scene, camera_source_name.c_str());

        if (!camera_item) {
                obs_log(LOG_WARNING, "Could not find configured camera source in current scene");
                obs_source_release(scene_source);
                return false;
        }

        obs_source_t *camera_source = obs_sceneitem_get_source(camera_item);

        const float source_w = (float)obs_source_get_width(camera_source);
        const float source_h = (float)obs_source_get_height(camera_source);

        if (source_w <= 0.0f || source_h <= 0.0f) {
                obs_source_release(scene_source);
                return false;
        }

        struct obs_sceneitem_crop crop;
        obs_sceneitem_get_crop(camera_item, &crop);

        const float visible_w =
                source_w - (float)crop.left - (float)crop.right;
        const float visible_h =
                source_h - (float)crop.top - (float)crop.bottom;

        if (visible_w <= 0.0f || visible_h <= 0.0f) {
                obs_source_release(scene_source);
                return false;
        }

        const float source_x = detector_x * source_w / 640.0f;
        const float source_y = detector_y * source_h / 360.0f;

        const float nx =
                (source_x - (float)crop.left) / visible_w;
        const float ny =
                (source_y - (float)crop.top) / visible_h;

        struct matrix4 box;
        obs_sceneitem_get_box_transform(camera_item, &box);

        struct vec3 point;
        vec3_set(&point, nx, ny, 0.0f);
        vec3_transform(&point, &point, &box);

        const PunchSettings punch_settings =
                get_runtime_settings();

        result->x =
                point.x + punch_settings.impact_offset_x;

        result->y =
                point.y + punch_settings.impact_offset_y;

        obs_log(LOG_INFO,
                "Mapped nose: detector=(%.1f, %.1f) scene=(%.1f, %.1f)",
                detector_x, detector_y,
                result->x, result->y);

        obs_source_release(scene_source);
        return true;
}
static void process_capture_probe(void);

static void punch_tick(void *data, float seconds)
{
        (void)data;

        process_capture_probe();

        if (!active_punch_item) {
                distortion_strength.store(0.0f);
                return;
        }

        punch_elapsed += seconds;

        if (!japish_audio_fired &&
            punch_elapsed >= active_punch_timing.entry) {
                play_japish_audio();
                japish_audio_fired = true;
        }

        if (punch_elapsed < active_punch_timing.entry) {
                distortion_strength.store(0.0f);

                float t = clamp01(punch_elapsed / active_punch_timing.entry);
                t = ease_out_cubic(t);

                float x = punch_rest_pos.x +
                        (punch_impact_pos.x - punch_rest_pos.x) * t;

                set_punch_x(x);
                return;
        }

        if (punch_elapsed < active_punch_timing.entry + active_punch_timing.hold) {
                distortion_strength.store(active_distortion_force);
                obs_sceneitem_set_pos(active_punch_item, &punch_impact_pos);
                return;
        }

        float out_start = active_punch_timing.entry + active_punch_timing.hold;

        if (punch_elapsed <
            out_start + active_punch_timing.exit) {
                float t = clamp01(
                        (punch_elapsed - out_start) / active_punch_timing.exit);

                distortion_strength.store(
                        active_distortion_force * (1.0f - t));

                t = ease_in_cubic(t);

                float x = punch_impact_pos.x +
                        (punch_rest_pos.x - punch_impact_pos.x) * t;

                set_punch_x(x);
                return;
        }

        distortion_strength.store(0.0f);

        obs_sceneitem_set_pos(active_punch_item, &punch_impact_pos);
        obs_sceneitem_set_visible(active_punch_item, false);

        obs_sceneitem_release(active_punch_item);
        active_punch_item = NULL;
        punch_elapsed = 0.0f;
}
static void show_punch(void)
{
        obs_source_t *scene_source = obs_frontend_get_current_scene();

        if (!scene_source) {
                obs_log(LOG_WARNING, "No current scene");
                return;
        }

        obs_scene_t *scene = obs_scene_from_source(scene_source);

        if (!scene) {
                obs_source_release(scene_source);
                return;
        }

        obs_sceneitem_t *item = obs_scene_find_source(scene, PUNCH_SOURCE_NAME);

        if (item) {
                arm_punch_animation(item);
                obs_log(LOG_INFO, "Punch shown");
                obs_source_release(scene_source);
                return;
        }

        char *path = obs_module_file("assets/punch.png");

        obs_data_t *settings = obs_data_create();
        obs_data_set_string(settings, "file", path);

        obs_source_t *punch = obs_source_create(
                "image_source",
                PUNCH_SOURCE_NAME,
                settings,
                NULL);

        obs_data_release(settings);
        bfree(path);

        if (!punch) {
                obs_log(LOG_ERROR, "Could not create punch image source");
                obs_source_release(scene_source);
                return;
        }

        obs_sceneitem_t *new_item = obs_scene_add(scene, punch);
        if (new_item)
                arm_punch_animation(new_item);

        obs_log(LOG_INFO, "Punch source created");

        obs_source_release(punch);
        obs_source_release(scene_source);
}

#define FRAME_TAP_ID "punch_streamer_frame_tap"

struct frame_tap_data {
        bool logged;
};

static obs_source_t *frame_tap_source = NULL;
static obs_weak_source_t *frame_tap_camera_weak = NULL;
static std::string frame_tap_camera_name;
static std::atomic<bool> capture_requested{false};
static std::atomic<bool> capture_ready{false};
static std::mutex capture_mutex;

static std::vector<uint8_t> captured_y;
static std::vector<uint8_t> captured_u;
static std::vector<uint8_t> captured_v;

static uint32_t captured_width = 0;
static uint32_t captured_height = 0;
static uint64_t captured_timestamp = 0;
static bool build_bgr_from_capture(cv::Mat &bgr)
{
        std::lock_guard<std::mutex> lock(capture_mutex);

        if (!capture_ready.load() || captured_width == 0 || captured_height == 0)
                return false;

        capture_ready.store(false);

        cv::Mat y((int)captured_height, (int)captured_width, CV_8UC1, captured_y.data());
        cv::Mat u_half((int)captured_height, (int)(captured_width / 2), CV_8UC1, captured_u.data());
        cv::Mat v_half((int)captured_height, (int)(captured_width / 2), CV_8UC1, captured_v.data());

        cv::Mat u_full;
        cv::Mat v_full;
        cv::resize(u_half, u_full, cv::Size((int)captured_width, (int)captured_height), 0.0, 0.0, cv::INTER_LINEAR);
        cv::resize(v_half, v_full, cv::Size((int)captured_width, (int)captured_height), 0.0, 0.0, cv::INTER_LINEAR);

        std::vector<cv::Mat> ycrcb;
        ycrcb.push_back(y);
        ycrcb.push_back(v_full);
        ycrcb.push_back(u_full);

        cv::Mat merged;
        cv::merge(ycrcb, merged);
        cv::cvtColor(merged, bgr, cv::COLOR_YCrCb2BGR);

        return !bgr.empty();
}

static void process_capture_probe(void)
{
        if (!capture_ready.load())
                return;

        try {
                cv::Mat bgr;

                if (!build_bgr_from_capture(bgr)) {
                        obs_log(LOG_WARNING, "BGR conversion failed");
                        return;
                }

                if (face_detector.empty()) {
                        obs_log(LOG_WARNING, "YuNet detector unavailable");
                        return;
                }

                cv::Mat input;
                cv::resize(bgr, input, cv::Size(640, 360));

                face_detector->setInputSize(input.size());

                cv::Mat faces;
                face_detector->setScoreThreshold(
                        get_runtime_settings().detection_confidence);

                face_detector->detect(input, faces);

                if (faces.empty() || faces.rows < 1) {
                        obs_log(LOG_INFO, "YuNet: no face detected");
                        return;
                }

                int best = 0;
                float best_score = faces.at<float>(0, 14);

                for (int i = 1; i < faces.rows; i++) {
                        float score = faces.at<float>(i, 14);

                        if (score > best_score) {
                                best = i;
                                best_score = score;
                        }
                }

                float x = faces.at<float>(best, 0);
                float y = faces.at<float>(best, 1);
                float w = faces.at<float>(best, 2);
                float h = faces.at<float>(best, 3);

                float nose_x = faces.at<float>(best, 8);
                float nose_y = faces.at<float>(best, 9);

                const PunchSettings punch_settings =
                        get_runtime_settings();

                distortion_center_x.store(
                        clamp01(
                                (nose_x / 640.0f) +
                                punch_settings.distortion_offset_x));

                distortion_center_y.store(
                        clamp01(
                                (nose_y / 360.0f) +
                                punch_settings.distortion_offset_y));

                struct vec2 face_left;
                struct vec2 face_right;

                if (map_detector_point_to_scene(
                            nose_x,
                            nose_y,
                            &tracked_scene_target) &&
                    map_detector_point_to_scene(
                            x,
                            nose_y,
                            &face_left) &&
                    map_detector_point_to_scene(
                            x + w,
                            nose_y,
                            &face_right)) {

                        tracked_face_scene_width =
                                face_right.x - face_left.x;

                        if (tracked_face_scene_width < 0.0f)
                                tracked_face_scene_width =
                                        -tracked_face_scene_width;

                        tracked_scene_target_ready = true;
                        show_punch();
                }

                obs_log(LOG_INFO,
                        "YuNet face: bbox=(%.1f, %.1f, %.1f, %.1f) nose=(%.1f, %.1f) score=%.3f",
                        x, y, w, h,
                        nose_x, nose_y,
                        best_score);
        }
        catch (const cv::Exception &e) {
                obs_log(LOG_ERROR, "YuNet detection exception: %s", e.what());
        }
}
static void request_camera_capture(void)
{
        capture_ready.store(false);
        capture_requested.store(true);
        obs_log(LOG_INFO, "Camera capture requested");
}

static void capture_frame_if_requested(struct obs_source_frame *frame)
{
        if (!frame || !capture_requested.exchange(false))
                return;

        if (frame->format != VIDEO_FORMAT_I422 ||
            !frame->data[0] ||
            !frame->data[1] ||
            !frame->data[2]) {
                obs_log(LOG_WARNING,
                        "Unsupported capture format=%d",
                        (int)frame->format);
                return;
        }

        const uint32_t width = frame->width;
        const uint32_t height = frame->height;
        const uint32_t chroma_width = width / 2;

        std::lock_guard<std::mutex> lock(capture_mutex);

        captured_y.resize((size_t)width * height);
        captured_u.resize((size_t)chroma_width * height);
        captured_v.resize((size_t)chroma_width * height);

        for (uint32_t y = 0; y < height; y++) {
                memcpy(captured_y.data() + ((size_t)y * width),
                       frame->data[0] + ((size_t)y * frame->linesize[0]),
                       width);

                memcpy(captured_u.data() + ((size_t)y * chroma_width),
                       frame->data[1] + ((size_t)y * frame->linesize[1]),
                       chroma_width);

                memcpy(captured_v.data() + ((size_t)y * chroma_width),
                       frame->data[2] + ((size_t)y * frame->linesize[2]),
                       chroma_width);
        }

        captured_width = width;
        captured_height = height;
        captured_timestamp = frame->timestamp;

        capture_ready.store(true);

        obs_log(LOG_INFO,
                "Camera capture ready: %ux%u I422 timestamp=%llu",
                width,
                height,
                (unsigned long long)captured_timestamp);
}


static const char *frame_tap_get_name(void *unused)
{
        (void)unused;
        return "Punch Streamer Frame Tap";
}

static void *frame_tap_create(obs_data_t *settings, obs_source_t *source)
{
        (void)settings;
        (void)source;

        return bzalloc(sizeof(struct frame_tap_data));
}

static void frame_tap_destroy(void *data)
{
        bfree(data);
}

static struct obs_source_frame *frame_tap_filter_video(
        void *data,
        struct obs_source_frame *frame)
{
        struct frame_tap_data *tap = (struct frame_tap_data *)data;

        if (frame && !tap->logged) {
                obs_log(LOG_INFO,
                        "Frame tap OK: %ux%u format=%d linesize0=%u flip=%d",
                        frame->width,
                        frame->height,
                        (int)frame->format,
                        frame->linesize[0],
                        frame->flip ? 1 : 0);

                tap->logged = true;
        }

        capture_frame_if_requested(frame);

        return frame;
}

#define PUNCH_DISTORTION_FILTER_ID "punch_streamer_distortion"

struct distortion_filter_data {
        obs_source_t *context;
        gs_effect_t *effect;
};

static obs_source_t *distortion_filter_source = NULL;
static obs_weak_source_t *distortion_camera_weak = NULL;
static std::string distortion_camera_name;

static const char *distortion_filter_get_name(void *unused)
{
        (void)unused;
        return "Punch Streamer Distortion";
}

static void *distortion_filter_create(obs_data_t *settings,
                                      obs_source_t *source)
{
        (void)settings;

        struct distortion_filter_data *filter =
                (struct distortion_filter_data *)
                bzalloc(sizeof(struct distortion_filter_data));

        filter->context = source;

        char *effect_path =
                obs_module_file("assets/punch_distortion.effect");

        char *effect_error = NULL;

        obs_enter_graphics();
        filter->effect =
                gs_effect_create_from_file(effect_path, &effect_error);
        obs_leave_graphics();

        bfree(effect_path);

        if (effect_error) {
                obs_log(LOG_ERROR,
                        "Distortion effect error: %s",
                        effect_error);
                bfree(effect_error);
        }

        if (!filter->effect) {
                obs_log(LOG_ERROR,
                        "Could not load punch distortion effect");
                bfree(filter);
                return NULL;
        }

        obs_log(LOG_INFO,
                "Distortion effect loaded successfully");

        return filter;
}

static void distortion_filter_destroy(void *data)
{
        struct distortion_filter_data *filter =
                (struct distortion_filter_data *)data;

        if (!filter)
                return;

        obs_enter_graphics();

        if (filter->effect)
                gs_effect_destroy(filter->effect);

        obs_leave_graphics();

        bfree(filter);
}

static void distortion_filter_render(void *data, gs_effect_t *effect)
{
        (void)effect;

        struct distortion_filter_data *filter =
                (struct distortion_filter_data *)data;

        if (!filter || !filter->effect)
                return;

        if (!obs_source_process_filter_begin(
                    filter->context,
                    GS_RGBA,
                    OBS_ALLOW_DIRECT_RENDERING))
                return;

        gs_eparam_t *param =
                gs_effect_get_param_by_name(
                        filter->effect,
                        "Golpe");

        if (param)
                gs_effect_set_float(
                        param,
                        distortion_strength.load());

        param = gs_effect_get_param_by_name(
                filter->effect,
                "Centro_X");

        if (param)
                gs_effect_set_float(
                        param,
                        distortion_center_x.load());

        param = gs_effect_get_param_by_name(
                filter->effect,
                "Centro_Y");

        if (param)
                gs_effect_set_float(
                        param,
                        distortion_center_y.load());

        param = gs_effect_get_param_by_name(
                filter->effect,
                "Radio");

        if (param)
                gs_effect_set_float(
                        param,
                        JAPISH_RADIUS);

        param = gs_effect_get_param_by_name(
                filter->effect,
                "Reach_Left");

        if (param)
                gs_effect_set_float(
                        param,
                        runtime_distortion_left.load());

        param = gs_effect_get_param_by_name(
                filter->effect,
                "Reach_Right");

        if (param)
                gs_effect_set_float(
                        param,
                        runtime_distortion_right.load());

        param = gs_effect_get_param_by_name(
                filter->effect,
                "Reach_Up");

        if (param)
                gs_effect_set_float(
                        param,
                        runtime_distortion_up.load());

        param = gs_effect_get_param_by_name(
                filter->effect,
                "Reach_Down");

        if (param)
                gs_effect_set_float(
                        param,
                        runtime_distortion_down.load());

        obs_source_process_filter_end(
                filter->context,
                filter->effect,
                0,
                0);
}

static struct obs_source_info distortion_filter_info = {};

static void register_distortion_filter_source(void)
{
        distortion_filter_info.id = PUNCH_DISTORTION_FILTER_ID;
        distortion_filter_info.type = OBS_SOURCE_TYPE_FILTER;
        distortion_filter_info.output_flags = OBS_SOURCE_VIDEO;
        distortion_filter_info.get_name = distortion_filter_get_name;
        distortion_filter_info.create = distortion_filter_create;
        distortion_filter_info.destroy = distortion_filter_destroy;
        distortion_filter_info.video_render = distortion_filter_render;

        obs_register_source(&distortion_filter_info);
}

static void ensure_camera_distortion(void)
{
        if (distortion_filter_source &&
            distortion_camera_name != camera_source_name)
                release_camera_distortion();

        if (distortion_filter_source)
                return;

        obs_source_t *camera =
                obs_get_source_by_name(camera_source_name.c_str());

        if (!camera) {
                obs_log(LOG_WARNING,
                        "Configured camera source not found for distortion");
                return;
        }

        distortion_filter_source =
                obs_source_create_private(
                        PUNCH_DISTORTION_FILTER_ID,
                        "Punch Streamer Distortion",
                        NULL);

        if (!distortion_filter_source) {
                obs_log(LOG_ERROR,
                        "Could not create distortion filter");
                obs_source_release(camera);
                return;
        }

        obs_source_filter_add(
                camera,
                distortion_filter_source);

        distortion_camera_name = camera_source_name;
        distortion_camera_weak =
                obs_source_get_weak_source(camera);

        obs_source_release(camera);

        obs_log(LOG_INFO,
                "Distortion filter attached to configured camera source");
}

static void release_camera_distortion(void)
{
        if (!distortion_filter_source) {
                if (distortion_camera_weak) {
                        obs_weak_source_release(
                                distortion_camera_weak);
                        distortion_camera_weak = NULL;
                }

                distortion_camera_name.clear();
                return;
        }

        obs_source_t *camera = NULL;

        if (distortion_camera_weak)
                camera = obs_weak_source_get_source(
                        distortion_camera_weak);

        if (camera) {
                obs_source_filter_remove(
                        camera,
                        distortion_filter_source);
                obs_source_release(camera);
        }

        if (distortion_camera_weak) {
                obs_weak_source_release(
                        distortion_camera_weak);
                distortion_camera_weak = NULL;
        }

        obs_source_release(distortion_filter_source);
        distortion_filter_source = NULL;
        distortion_camera_name.clear();
}
static struct obs_source_info frame_tap_info = {};

static void register_frame_tap_source(void)
{
        frame_tap_info.id = FRAME_TAP_ID;
        frame_tap_info.type = OBS_SOURCE_TYPE_FILTER;
        frame_tap_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC;
        frame_tap_info.get_name = frame_tap_get_name;
        frame_tap_info.create = frame_tap_create;
        frame_tap_info.destroy = frame_tap_destroy;
        frame_tap_info.filter_video = frame_tap_filter_video;

        obs_register_source(&frame_tap_info);
}

static void release_camera_tap(void)
{
        if (!frame_tap_source) {
                if (frame_tap_camera_weak) {
                        obs_weak_source_release(
                                frame_tap_camera_weak);
                        frame_tap_camera_weak = NULL;
                }

                frame_tap_camera_name.clear();
                return;
        }

        obs_source_t *camera = NULL;

        if (frame_tap_camera_weak)
                camera = obs_weak_source_get_source(
                        frame_tap_camera_weak);

        if (camera) {
                obs_source_filter_remove(
                        camera,
                        frame_tap_source);
                obs_source_release(camera);
        }

        if (frame_tap_camera_weak) {
                obs_weak_source_release(
                        frame_tap_camera_weak);
                frame_tap_camera_weak = NULL;
        }

        obs_source_release(frame_tap_source);
        frame_tap_source = NULL;
        frame_tap_camera_name.clear();

        capture_requested.store(false);

        {
                std::lock_guard<std::mutex> lock(capture_mutex);
                capture_ready.store(false);
                captured_y.clear();
                captured_u.clear();
                captured_v.clear();
                captured_width = 0;
                captured_height = 0;
                captured_timestamp = 0;
        }
}

static void ensure_camera_tap(void)
{
        if (frame_tap_source &&
            frame_tap_camera_name != camera_source_name)
                release_camera_tap();

        if (frame_tap_source)
                return;

        obs_source_t *camera = obs_get_source_by_name(camera_source_name.c_str());

        if (!camera) {
                obs_log(LOG_WARNING, "Configured camera source not found");
                return;
        }

        frame_tap_source = obs_source_create_private(
                FRAME_TAP_ID,
                "Punch Streamer Frame Tap",
                NULL);

        if (!frame_tap_source) {
                obs_log(LOG_ERROR, "Could not create frame tap");
                obs_source_release(camera);
                return;
        }

        obs_source_filter_add(camera, frame_tap_source);
        frame_tap_camera_name = camera_source_name;
        frame_tap_camera_weak =
                obs_source_get_weak_source(camera);
        obs_source_release(camera);

        obs_log(LOG_INFO, "Frame tap attached to configured camera source");
}
static void probe_camera_frame(void)
{
        obs_source_t *camera = obs_get_source_by_name(camera_source_name.c_str());

        if (!camera) {
                obs_log(LOG_WARNING, "Configured camera source not found");
                return;
        }

        struct obs_source_frame *frame = obs_source_get_frame(camera);

        if (!frame) {
                obs_log(LOG_WARNING, "Camera frame not available");
                obs_source_release(camera);
                return;
        }

        obs_log(LOG_INFO,
                "Camera frame OK: %ux%u format=%d linesize0=%u flip=%d timestamp=%llu",
                frame->width,
                frame->height,
                (int)frame->format,
                frame->linesize[0],
                frame->flip ? 1 : 0,
                (unsigned long long)frame->timestamp);

        obs_source_release_frame(camera, frame);
        obs_source_release(camera);
}
bool trigger_japish()
{
        if (camera_source_name.empty()) {
                obs_log(
                        LOG_WARNING,
                        "JAPISH ignored: no camera source configured");

                return false;
        }

        obs_source_t *camera =
                obs_get_source_by_name(
                        camera_source_name.c_str());

        if (!camera) {
                obs_log(
                        LOG_WARNING,
                        "JAPISH ignored: configured camera source not found");

                return false;
        }

        obs_source_release(camera);

        obs_log(LOG_INFO, "JAPISH triggered");

        ensure_japish_audio();
        ensure_camera_tap();
        ensure_camera_distortion();
        request_camera_capture();

        return true;
}

static void japish_hotkey_callback(void *data, obs_hotkey_id id,
                                   obs_hotkey_t *hotkey, bool pressed)
{
        (void)data;
        (void)id;
        (void)hotkey;

        if (!pressed)
                return;

        trigger_japish();
}

static void punch_frontend_event(
        enum obs_frontend_event event,
        void *private_data)
{
        (void)private_data;

        if (event != OBS_FRONTEND_EVENT_PROFILE_CHANGED)
                return;

        obs_log(
                LOG_INFO,
                "OBS profile changed; reloading Punch Streamer settings");

        load_runtime_settings_from_profile();
        load_saved_hotkey();
}

bool obs_module_load(void)
{
        obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
        obs_log(LOG_INFO, "OpenCV runtime: %s", cv::getVersionString().c_str());

        load_runtime_settings_from_profile();
        load_yunet();
        register_frame_tap_source();
        register_distortion_filter_source();

        japish_hotkey = obs_hotkey_register_frontend(
                "punch_streamer_japish",
                obs_module_text("Hotkey.Japish"),
                japish_hotkey_callback,
                NULL);

        load_saved_hotkey();

        obs_frontend_add_event_callback(
                punch_frontend_event,
                NULL);

        obs_add_tick_callback(punch_tick, NULL);

#ifdef ENABLE_QT
        register_punch_settings_tools_menu();
#endif

        return true;
}

void obs_module_unload(void)
{
        obs_remove_tick_callback(punch_tick, NULL);

        obs_frontend_remove_event_callback(
                punch_frontend_event,
                NULL);
        face_detector.release();
        release_camera_tap();
        release_camera_distortion();
        release_japish_audio();

        if (active_punch_item) {
                obs_sceneitem_release(active_punch_item);
                active_punch_item = NULL;
        }

        if (japish_hotkey != OBS_INVALID_HOTKEY_ID)
                obs_hotkey_unregister(japish_hotkey);

        obs_log(LOG_INFO, "plugin unloaded");
}
































