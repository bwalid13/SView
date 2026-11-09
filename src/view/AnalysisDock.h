#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace sview {

class ViewerViewModel;

// -----------------------------------------------------------------------------
// AnalysisDock: Panneau de contrôle dockable pour l'exposition, le gamma, les AOVs,
// la comparaison A/B wipe et le damier alpha d'arrière-plan.
// -----------------------------------------------------------------------------
class AnalysisDock : public QWidget {
    Q_OBJECT
public:
    explicit AnalysisDock(ViewerViewModel* vm, QWidget* parent = nullptr);
    ~AnalysisDock() override = default;

    // Getters publics pour l'introspection et les tests automatisés
    QSlider* exposureSlider() const { return m_exposureSlider; }
    QDoubleSpinBox* exposureSpinBox() const { return m_exposureSpin; }
    QPushButton* exposureResetButton() const { return m_exposureResetBtn; }

    QSlider* gammaSlider() const { return m_gammaSlider; }
    QDoubleSpinBox* gammaSpinBox() const { return m_gammaSpin; }
    QPushButton* gammaResetButton() const { return m_gammaResetBtn; }

    QComboBox* aovComboBox() const { return m_aovCombo; }
    QComboBox* ocioComboBox() const { return m_ocioCombo; }

    QPushButton* wipeToggleButton() const { return m_wipeToggleBtn; }
    QComboBox* wipeModeComboBox() const { return m_wipeModeCombo; }
    QSlider* wipePosSlider() const { return m_wipePosSlider; }
    QLabel* wipePosLabel() const { return m_wipePosLabel; }
    QPushButton* loadBButton() const { return m_loadBBtn; }
    QLabel* imageBLabel() const { return m_imageBLabel; }

    QCheckBox* checkerboardCheckBox() const { return m_checkerboardCheck; }

    // Contrôles 360° & Lookdev
    QCheckBox* mode360CheckBox() const { return m_mode360Check; }
    QCheckBox* spheresCheckBox() const { return m_spheresCheck; }
    QDoubleSpinBox* yawSpinBox() const { return m_yawSpin; }
    QDoubleSpinBox* pitchSpinBox() const { return m_pitchSpin; }
    QDoubleSpinBox* fovSpinBox() const { return m_fovSpin; }
    QSlider* yawSlider() const { return m_yawSlider; }
    QSlider* pitchSlider() const { return m_pitchSlider; }
    QSlider* fovSlider() const { return m_fovSlider; }
    QLabel* yawValueLabel() const { return m_yawValLabel; }
    QLabel* pitchValueLabel() const { return m_pitchValLabel; }
    QLabel* fovValueLabel() const { return m_fovValLabel; }
    QPushButton* resetCamera360Button() const { return m_resetCamera360Btn; }

    // Contrôles du rig lookdev
    QComboBox* rigCornerComboBox() const { return m_rigCornerCombo; }
    QCheckBox* colorChartCheckBox() const { return m_chartCheck; }

    ViewerViewModel* viewModel() const { return m_vm; }

    // Helper de création pour QtAdvancedDockingSystem
    static ads::CDockWidget* createDockWidget(ads::CDockManager* dockManager,
                                              ViewerViewModel* vm,
                                              QWidget* parent = nullptr);

signals:
    void requestLoadImageB();

public slots:
    void refreshFromViewModel();

private slots:
    // Slots UI -> ViewModel
    void onExposureSliderChanged(int intValue);
    void onExposureSpinBoxChanged(double value);
    void onExposureResetClicked();

    void onGammaSliderChanged(int intValue);
    void onGammaSpinBoxChanged(double value);
    void onGammaResetClicked();

    void onAovComboActivated(int index);
    void onOcioComboActivated(int index);

    void onWipeToggled(bool checked);
    void onWipeModeChanged(int index);
    void onWipePosSliderChanged(int intValue);
    void onLoadBClicked();

    void onCheckerboardToggled(bool checked);
    void onFlipHToggled(bool checked);
    void onFlipVToggled(bool checked);

    void onMode360Toggled(bool checked);
    void onSpheresToggled(bool checked);
    void onYawSliderChanged(int value);
    void onPitchSliderChanged(int value);
    void onFovSliderChanged(int value);
    void onYawSpinChanged(double value);
    void onPitchSpinChanged(double value);
    void onFovSpinChanged(double value);
    void onResetCamera360Clicked();

    // Lookdev rig
    void onRigCornerChanged(int index);
    void onColorChartToggled(bool checked);

    // Slots ViewModel -> UI
    void onDisplayChanged();
    void onImageChanged();
    void onImageBChanged();
    void onMode360Changed(bool enabled);
    void onSpheresChanged(bool enabled);
    void onCamera360Changed(float yaw, float pitch, float fov);

private:
    void setupUi();
    void setupConnections();
    void populateAovCombo();

    ViewerViewModel* m_vm = nullptr;

    // Contrôles : Exposition
    QSlider* m_exposureSlider = nullptr;
    QDoubleSpinBox* m_exposureSpin = nullptr;
    QPushButton* m_exposureResetBtn = nullptr;

    // Contrôles : Gamma
    QSlider* m_gammaSlider = nullptr;
    QDoubleSpinBox* m_gammaSpin = nullptr;
    QPushButton* m_gammaResetBtn = nullptr;

    // Contrôles : Canaux & AOVs
    QComboBox* m_aovCombo = nullptr;
    QComboBox* m_ocioCombo = nullptr;

    // Contrôles : A/B Wipe
    QPushButton* m_wipeToggleBtn = nullptr;
    QComboBox* m_wipeModeCombo = nullptr;
    QSlider* m_wipePosSlider = nullptr;
    QLabel* m_wipePosLabel = nullptr;
    QPushButton* m_loadBBtn = nullptr;
    QLabel* m_imageBLabel = nullptr;

    // Contrôles : Damier & Flip
    QCheckBox* m_checkerboardCheck = nullptr;
    QPushButton* m_flipHBtn = nullptr;
    QPushButton* m_flipVBtn = nullptr;

    // Contrôles : 360° & Lookdev
    QCheckBox* m_mode360Check = nullptr;
    QCheckBox* m_spheresCheck = nullptr;
    QSlider* m_yawSlider = nullptr;
    QSlider* m_pitchSlider = nullptr;
    QSlider* m_fovSlider = nullptr;
    QLabel* m_yawValLabel = nullptr;
    QLabel* m_pitchValLabel = nullptr;
    QLabel* m_fovValLabel = nullptr;
    QDoubleSpinBox* m_yawSpin = nullptr;
    QDoubleSpinBox* m_pitchSpin = nullptr;
    QDoubleSpinBox* m_fovSpin = nullptr;
    QPushButton* m_resetCamera360Btn = nullptr;

    // Contrôles : Lookdev rig
    QComboBox* m_rigCornerCombo = nullptr;
    QCheckBox* m_chartCheck = nullptr;

    // Gardes anti-boucle de rétroaction
    bool m_updatingExposure = false;
    bool m_updatingGamma = false;
    bool m_updatingAov = false;
    bool m_updatingOcio = false;
    bool m_updatingWipe = false;
    bool m_updatingCamera360 = false;
};

}  // namespace sview
