#pragma once

#include <QDialog>
#include <QString>

class QPushButton;
class QLabel;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QRadioButton;
class QButtonGroup;
class QLineEdit;

namespace sview {

class SettingsModel;

// Settings and Preferences dialog
// Allows configuring RAM Cache limit, playback FPS, floating dock,
// Windows file associations, and dynamic icon management.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    // Existing M3 File Associations Accessors
    QPushButton* registerDefaultAppButton() const { return m_registerDefaultAppBtn; }
    bool isRegistered() const { return m_isRegistered; }
    QLabel* registrationStatusLabel() const { return m_regStatusLabel; }

    // Milestone 4 Dynamic Icon Accessors
    QRadioButton* defaultIconRadio() const { return m_defaultIconRadio; }
    QRadioButton* alt1IconRadio() const { return m_alt1IconRadio; }
    QRadioButton* alt2IconRadio() const { return m_alt2IconRadio; }
    QRadioButton* customIconRadio() const { return m_customIconRadio; }
    QPushButton* browseCustomIconButton() const { return m_browseCustomBtn; }
    QPushButton* applyIconButton() const { return m_applyIconBtn; }
    QLabel* iconStatusLabel() const { return m_iconStatusLabel; }
    QLineEdit* customPathEdit() const { return m_customPathEdit; }

    QString selectedIconPreset() const;
    int selectedIconIndex() const;

public slots:
    void onApplyIconClicked();
    void onBrowseCustomClicked();

private slots:
    void onRegisterDefaultAppClicked();
    void onSaveClicked();
    void onIconRadioToggled();

private:
    void setupUi();
    void updateRegistrationStatus();
    void updateIconPreviews();

    // M3 Controls
    QPushButton* m_registerDefaultAppBtn = nullptr;
    QLabel* m_regStatusLabel = nullptr;
    QSpinBox* m_cacheBudgetSpin = nullptr;
    QCheckBox* m_floatingDockCheck = nullptr;
    bool m_isRegistered = false;

    // M4 Controls
    QRadioButton* m_defaultIconRadio = nullptr;
    QRadioButton* m_alt1IconRadio = nullptr;
    QRadioButton* m_alt2IconRadio = nullptr;
    QRadioButton* m_customIconRadio = nullptr;
    QButtonGroup* m_iconButtonGroup = nullptr;

    QLabel* m_defaultPreviewLabel = nullptr;
    QLabel* m_alt1PreviewLabel = nullptr;
    QLabel* m_alt2PreviewLabel = nullptr;
    QLabel* m_customPreviewLabel = nullptr;

    QLineEdit* m_customPathEdit = nullptr;
    QPushButton* m_browseCustomBtn = nullptr;
    QPushButton* m_applyIconBtn = nullptr;
    QLabel* m_iconStatusLabel = nullptr;
};

}  // namespace sview
