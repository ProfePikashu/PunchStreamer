#include "punch-settings-dialog.hpp"
#include "punch-settings.hpp"

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

class PunchSettingsDialog : public QDialog
{
public:
    explicit PunchSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Punch Streamer");
        setMinimumWidth(520);

        config_t *config =
            obs_frontend_get_profile_config();

        persisted_ = get_runtime_settings();

        if (config)
            punch_settings_load(config, persisted_);

        buildUi();
        populateCameras();
        loadControls(persisted_);
        refreshStatus();
        connectSignals();
    }

    void reject() override
    {
        apply_runtime_settings(persisted_);
        QDialog::reject();
    }

private:
    PunchSettings persisted_{};
    bool loading_ = false;

    QComboBox *preset_ = nullptr;
    QComboBox *camera_ = nullptr;

    QLabel *cameraStatus_ = nullptr;
    QLabel *faceStatus_ = nullptr;

    QDoubleSpinBox *punchSize_ = nullptr;
    QDoubleSpinBox *impactX_ = nullptr;
    QDoubleSpinBox *impactY_ = nullptr;
    QDoubleSpinBox *punchSpeed_ = nullptr;
    QDoubleSpinBox *confidence_ = nullptr;
    QDoubleSpinBox *force_ = nullptr;
    QDoubleSpinBox *distortionX_ = nullptr;
    QDoubleSpinBox *distortionY_ = nullptr;

    QDoubleSpinBox *leftReach_ = nullptr;
    QDoubleSpinBox *rightReach_ = nullptr;
    QDoubleSpinBox *upReach_ = nullptr;
    QDoubleSpinBox *downReach_ = nullptr;

    QDoubleSpinBox *volume_ = nullptr;

    QPushButton *testButton_ = nullptr;
    QPushButton *restoreButton_ = nullptr;
    QPushButton *saveButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;

    static QDoubleSpinBox *makeSpin(
        double min,
        double max,
        double step,
        int decimals)
    {
        auto *spin = new QDoubleSpinBox();

        spin->setRange(min, max);
        spin->setSingleStep(step);
        spin->setDecimals(decimals);
        spin->setKeyboardTracking(false);

        return spin;
    }

    void buildUi()
    {
        auto *root = new QVBoxLayout(this);

        auto *generalBox =
            new QGroupBox("Punch Streamer");

        auto *general =
            new QFormLayout(generalBox);

        preset_ = new QComboBox();

        preset_->addItem(
            "Recommended",
            static_cast<int>(
                PunchPreset::Recommended));

        preset_->addItem(
            "Subtle",
            static_cast<int>(
                PunchPreset::Subtle));

        preset_->addItem(
            "Cartoon",
            static_cast<int>(
                PunchPreset::Cartoon));

        preset_->addItem(
            "Custom",
            static_cast<int>(
                PunchPreset::Custom));

        camera_ = new QComboBox();

        cameraStatus_ = new QLabel("Unknown");
        faceStatus_ = new QLabel("Unknown");

        general->addRow("Preset", preset_);
        general->addRow("Camera", camera_);
        general->addRow("Camera Status", cameraStatus_);
        general->addRow(
            "Face Detection Status",
            faceStatus_);

        root->addWidget(generalBox);

        auto *impactBox =
            new QGroupBox("Punch");

        auto *impact =
            new QFormLayout(impactBox);

        punchSize_ =
            makeSpin(1.0, 8.0, 0.05, 2);

        impactX_ =
            makeSpin(-500.0, 500.0, 1.0, 0);

        impactY_ =
            makeSpin(-500.0, 500.0, 1.0, 0);

        punchSpeed_ =
            makeSpin(0.50, 2.00, 0.05, 2);

        confidence_ =
            makeSpin(0.50, 0.95, 0.01, 2);

        impact->addRow("Punch Size", punchSize_);
        impact->addRow("Impact X", impactX_);
        impact->addRow("Impact Y", impactY_);
        impact->addRow("Punch Speed", punchSpeed_);
        impact->addRow(
            "Detection Confidence",
            confidence_);

        root->addWidget(impactBox);

        auto *distortionBox =
            new QGroupBox("Distortion");

        auto *distortion =
            new QFormLayout(distortionBox);

        force_ =
            makeSpin(0.0, 1.0, 0.01, 2);

        distortionX_ =
            makeSpin(-0.20, 0.20, 0.001, 3);

        distortionY_ =
            makeSpin(-0.20, 0.20, 0.001, 3);

        leftReach_ =
            makeSpin(0.25, 2.50, 0.05, 2);

        rightReach_ =
            makeSpin(0.25, 2.50, 0.05, 2);

        upReach_ =
            makeSpin(0.25, 2.50, 0.05, 2);

        downReach_ =
            makeSpin(0.25, 2.50, 0.05, 2);

        distortion->addRow(
            "Distortion Force",
            force_);

        distortion->addRow(
            "Distortion X",
            distortionX_);

        distortion->addRow(
            "Distortion Y",
            distortionY_);

        distortion->addRow(
            "Left Reach",
            leftReach_);

        distortion->addRow(
            "Right Reach",
            rightReach_);

        distortion->addRow(
            "Up Reach",
            upReach_);

        distortion->addRow(
            "Down Reach",
            downReach_);

        root->addWidget(distortionBox);

        auto *audioBox =
            new QGroupBox("Audio");

        auto *audio =
            new QFormLayout(audioBox);

        volume_ =
            makeSpin(0.0, 200.0, 5.0, 0);

        volume_->setSuffix("%");

        audio->addRow("Volume", volume_);

        root->addWidget(audioBox);

        testButton_ =
            new QPushButton("Test JAPISH");

        restoreButton_ =
            new QPushButton(
                "Restore Recommended");

        saveButton_ =
            new QPushButton("Save");

        cancelButton_ =
            new QPushButton("Cancel");

        auto *testRow =
            new QHBoxLayout();

        testRow->addWidget(testButton_);
        testRow->addWidget(restoreButton_);
        testRow->addStretch();

        root->addLayout(testRow);

        auto *buttons =
            new QHBoxLayout();

        buttons->addStretch();
        buttons->addWidget(saveButton_);
        buttons->addWidget(cancelButton_);

        root->addLayout(buttons);
    }

    static bool enumVideoSource(
        void *data,
        obs_source_t *source)
    {
        auto *names =
            static_cast<std::vector<QString> *>(
                data);

        if (!source)
            return true;

        const uint32_t flags =
            obs_source_get_output_flags(source);

        if ((flags & OBS_SOURCE_VIDEO) == 0)
            return true;

        const char *name =
            obs_source_get_name(source);

        if (name && *name)
            names->push_back(
                QString::fromUtf8(name));

        return true;
    }

    void populateCameras()
    {
        std::vector<QString> names;

        obs_enum_sources(
            enumVideoSource,
            &names);

        camera_->clear();
        camera_->addItem("(Not configured)");

        for (const QString &name : names)
            camera_->addItem(name);
    }

    QString selectedCamera() const
    {
        if (camera_->currentIndex() <= 0)
            return {};

        return camera_->currentText();
    }

    PunchSettings collectSettings() const
    {
        PunchSettings settings =
            get_runtime_settings();

        settings.camera_source =
            selectedCamera()
                .toUtf8()
                .constData();

        settings.punch_scale =
            static_cast<float>(
                punchSize_->value());

        settings.impact_offset_x =
            static_cast<float>(
                impactX_->value());

        settings.impact_offset_y =
            static_cast<float>(
                impactY_->value());

        settings.punch_speed =
            static_cast<float>(
                punchSpeed_->value());

        settings.detection_confidence =
            static_cast<float>(
                confidence_->value());

        settings.distortion_force =
            static_cast<float>(
                force_->value());

        settings.distortion_offset_x =
            static_cast<float>(
                distortionX_->value());

        settings.distortion_offset_y =
            static_cast<float>(
                distortionY_->value());

        settings.distortion_left =
            static_cast<float>(
                leftReach_->value());

        settings.distortion_right =
            static_cast<float>(
                rightReach_->value());

        settings.distortion_up =
            static_cast<float>(
                upReach_->value());

        settings.distortion_down =
            static_cast<float>(
                downReach_->value());

        settings.impact_volume =
            static_cast<float>(
                volume_->value() / 100.0);

        settings.preset =
            punch_settings_detect_preset(
                settings);

        return settings;
    }

    void loadControls(
        const PunchSettings &settings)
    {
        loading_ = true;

        int cameraIndex = 0;

        if (!settings.camera_source.empty()) {
            const QString name =
                QString::fromUtf8(
                    settings.camera_source.c_str());

            cameraIndex =
                camera_->findText(name);

            if (cameraIndex < 0) {
                camera_->addItem(name);
                cameraIndex =
                    camera_->findText(name);
            }
        }

        camera_->setCurrentIndex(
            cameraIndex >= 0
                ? cameraIndex
                : 0);

        punchSize_->setValue(
            settings.punch_scale);

        impactX_->setValue(
            settings.impact_offset_x);

        impactY_->setValue(
            settings.impact_offset_y);

        punchSpeed_->setValue(
            settings.punch_speed);

        confidence_->setValue(
            settings.detection_confidence);

        force_->setValue(
            settings.distortion_force);

        distortionX_->setValue(
            settings.distortion_offset_x);

        distortionY_->setValue(
            settings.distortion_offset_y);

        leftReach_->setValue(
            settings.distortion_left);

        rightReach_->setValue(
            settings.distortion_right);

        upReach_->setValue(
            settings.distortion_up);

        downReach_->setValue(
            settings.distortion_down);

        volume_->setValue(
            settings.impact_volume * 100.0);

        const int presetIndex =
            preset_->findData(
                static_cast<int>(
                    punch_settings_detect_preset(
                        settings)));

        preset_->setCurrentIndex(
            presetIndex >= 0
                ? presetIndex
                : 3);

        loading_ = false;
    }

    void updatePresetIndicator(
        const PunchSettings &settings)
    {
        const PunchPreset detected =
            punch_settings_detect_preset(
                settings);

        const int index =
            preset_->findData(
                static_cast<int>(detected));

        QSignalBlocker blocker(preset_);

        preset_->setCurrentIndex(
            index >= 0
                ? index
                : preset_->findText("Custom"));
    }

    void refreshStatus()
    {
        const QString cameraName =
            selectedCamera();

        if (cameraName.isEmpty()) {
            cameraStatus_->setText(
                "Not configured");
        } else {
            obs_source_t *camera =
                obs_get_source_by_name(
                    cameraName
                        .toUtf8()
                        .constData());

                        if (camera) {
                const uint32_t flags =
                    obs_source_get_output_flags(camera);

                if ((flags & OBS_SOURCE_VIDEO) != 0) {
                    cameraStatus_->setText(
                        "Ready");
                } else {
                    cameraStatus_->setText(
                        "Not a video source");
                }

                obs_source_release(camera);
            } else {
                cameraStatus_->setText(
                    "Not found");
            }
        }

        faceStatus_->setText(
            punch_face_detector_ready()
                ? "Ready"
                : "Unavailable");
    }

    void applyLive()
    {
        if (loading_)
            return;

        PunchSettings settings =
            collectSettings();

        apply_runtime_settings(settings);
        updatePresetIndicator(settings);
        refreshStatus();
    }

    void selectPreset(
        PunchPreset preset)
    {
        if (preset == PunchPreset::Custom)
            return;

        const std::string camera =
            selectedCamera()
                .toUtf8()
                .constData();

        PunchSettings settings;

        switch (preset) {
        case PunchPreset::Subtle:
            settings =
                punch_settings_subtle();
            break;

        case PunchPreset::Cartoon:
            settings =
                punch_settings_cartoon();
            break;

        case PunchPreset::Recommended:
        default:
            settings =
                punch_settings_recommended();
            break;
        }

        settings.camera_source = camera;

        loadControls(settings);
        apply_runtime_settings(settings);
        refreshStatus();
    }

    void saveAndClose()
    {
        PunchSettings settings =
            collectSettings();

        apply_runtime_settings(settings);

        config_t *config =
            obs_frontend_get_profile_config();

        if (config) {
            punch_settings_save(
                config,
                settings);

            config_save_safe(
                config,
                "tmp",
                nullptr);
        }

        persisted_ = settings;

        accept();
    }

    void restoreRecommended()
    {
        const std::string camera =
            selectedCamera()
                .toUtf8()
                .constData();

        PunchSettings settings =
            punch_settings_recommended();

        settings.camera_source = camera;

        loadControls(settings);
        apply_runtime_settings(settings);
        refreshStatus();
    }

    void connectSignals()
    {
        connect(
            preset_,
            &QComboBox::currentIndexChanged,
            this,
            [this](int) {
                if (loading_)
                    return;

                const PunchPreset preset =
                    static_cast<PunchPreset>(
                        preset_
                            ->currentData()
                            .toInt());

                selectPreset(preset);
            });

        connect(
            camera_,
            &QComboBox::currentIndexChanged,
            this,
            [this](int) {
                applyLive();
            });

        const std::vector<QDoubleSpinBox *> spins = {
            punchSize_,
            impactX_,
            impactY_,
            punchSpeed_,
            confidence_,
            force_,
            distortionX_,
            distortionY_,
            leftReach_,
            rightReach_,
            upReach_,
            downReach_,
            volume_
        };

        for (QDoubleSpinBox *spin : spins) {
            connect(
                spin,
                &QDoubleSpinBox::valueChanged,
                this,
                [this](double) {
                    applyLive();
                });
        }

        connect(
            testButton_,
            &QPushButton::clicked,
            this,
            [this]() {
                applyLive();
                trigger_japish();
                refreshStatus();
            });

        connect(
            restoreButton_,
            &QPushButton::clicked,
            this,
            [this]() {
                restoreRecommended();
            });

        connect(
            saveButton_,
            &QPushButton::clicked,
            this,
            [this]() {
                saveAndClose();
            });

        connect(
            cancelButton_,
            &QPushButton::clicked,
            this,
            [this]() {
                reject();
            });
    }
};

static void openPunchSettings(void *)
{
    QWidget *parent =
        static_cast<QWidget *>(
            obs_frontend_get_main_window());

    PunchSettingsDialog dialog(parent);
    dialog.exec();
}

void register_punch_settings_tools_menu()
{
    obs_frontend_add_tools_menu_item(
        "Punch Streamer",
        openPunchSettings,
        nullptr);
}