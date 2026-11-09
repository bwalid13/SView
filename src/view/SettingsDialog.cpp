#include "SettingsDialog.h"
#include "../model/SettingsModel.h"
#include "../model/WindowsRegistryManager.h"
#include "../util/IconConverter.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace sview {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences & Settings"));
    resize(560, 560);
    setupUi();
    updateRegistrationStatus();
    updateIconPreviews();
}

void SettingsDialog::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(12);

    // 1. Performance & RAM Cache Group
    auto* cacheGroup = new QGroupBox(tr("Performance & Memory Cache"), this);
    auto* cacheForm = new QFormLayout(cacheGroup);
    cacheForm->setContentsMargins(10, 12, 10, 10);
    cacheForm->setSpacing(8);

    m_cacheBudgetSpin = new QSpinBox(cacheGroup);
    m_cacheBudgetSpin->setRange(512, 262144);
    m_cacheBudgetSpin->setSingleStep(1024);
    m_cacheBudgetSpin->setSuffix(QStringLiteral(" MB"));
    m_cacheBudgetSpin->setValue(static_cast<int>(SettingsModel::instance().cacheBudgetMB()));
    cacheForm->addRow(tr("RAM Cache Budget:"), m_cacheBudgetSpin);

    m_floatingDockCheck = new QCheckBox(tr("Enable Floating HUD Transport Dock"), cacheGroup);
    m_floatingDockCheck->setChecked(SettingsModel::instance().isFloatingDockEnabled());
    cacheForm->addRow(QString(), m_floatingDockCheck);

    rootLayout->addWidget(cacheGroup);

    // 2. Application & File Association Icon (Milestone 4 / Requirement R5)
    auto* iconGroup = new QGroupBox(tr("Application & File Association Icon"), this);
    auto* iconLayout = new QVBoxLayout(iconGroup);
    iconLayout->setContentsMargins(10, 12, 10, 10);
    iconLayout->setSpacing(8);

    auto* iconPrompt = new QLabel(tr("Select the active icon for Sview window and Windows Explorer file associations:"), iconGroup);
    iconPrompt->setWordWrap(true);
    iconLayout->addWidget(iconPrompt);

    // Preset cards layout
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(12);

    m_iconButtonGroup = new QButtonGroup(this);

    auto createCard = [this, iconGroup, cardsLayout](const QString& title, QLabel*& previewLabel, QRadioButton*& radio, const QString& objName) {
        auto* cardFrame = new QFrame(iconGroup);
        cardFrame->setFrameShape(QFrame::StyledPanel);
        cardFrame->setStyleSheet(QStringLiteral("QFrame { background: rgba(255,255,255,0.04); border-radius: 6px; padding: 4px; }"));
        auto* cardLayout = new QVBoxLayout(cardFrame);
        cardLayout->setAlignment(Qt::AlignCenter);
        cardLayout->setSpacing(4);

        previewLabel = new QLabel(cardFrame);
        previewLabel->setFixedSize(48, 48);
        previewLabel->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(previewLabel, 0, Qt::AlignCenter);

        radio = new QRadioButton(title, cardFrame);
        radio->setObjectName(objName);
        cardLayout->addWidget(radio, 0, Qt::AlignCenter);

        m_iconButtonGroup->addButton(radio);
        cardsLayout->addWidget(cardFrame);
    };

    createCard(tr("Default"), m_defaultPreviewLabel, m_defaultIconRadio, QStringLiteral("defaultIconRadio"));
    createCard(tr("Cyber Alt 1"), m_alt1PreviewLabel, m_alt1IconRadio, QStringLiteral("alt1IconRadio"));
    createCard(tr("Dark Slate Alt 2"), m_alt2PreviewLabel, m_alt2IconRadio, QStringLiteral("alt2IconRadio"));
    createCard(tr("Custom"), m_customPreviewLabel, m_customIconRadio, QStringLiteral("customIconRadio"));

    iconLayout->addLayout(cardsLayout);

    // Custom Path input row
    auto* customRow = new QHBoxLayout();
    customRow->setSpacing(6);
    auto* customLabel = new QLabel(tr("Custom Image:"), iconGroup);
    m_customPathEdit = new QLineEdit(iconGroup);
    m_customPathEdit->setObjectName(QStringLiteral("customIconPathEdit"));
    m_customPathEdit->setPlaceholderText(tr("Select image or ICO file..."));
    m_customPathEdit->setText(SettingsModel::instance().customIconPath());

    m_browseCustomBtn = new QPushButton(tr("Browse..."), iconGroup);
    m_browseCustomBtn->setObjectName(QStringLiteral("browseCustomIconButton"));

    customRow->addWidget(customLabel);
    customRow->addWidget(m_customPathEdit, 1);
    customRow->addWidget(m_browseCustomBtn);
    iconLayout->addLayout(customRow);

    // Apply Icon Button & Status Row
    auto* applyRow = new QHBoxLayout();
    m_applyIconBtn = new QPushButton(tr("Apply Icon"), iconGroup);
    m_applyIconBtn->setObjectName(QStringLiteral("applyIconButton"));
    m_iconStatusLabel = new QLabel(iconGroup);
    m_iconStatusLabel->setObjectName(QStringLiteral("iconStatusLabel"));
    m_iconStatusLabel->setStyleSheet(QStringLiteral("color: #a0a0a0;"));

    applyRow->addWidget(m_applyIconBtn);
    applyRow->addWidget(m_iconStatusLabel, 1);
    iconLayout->addLayout(applyRow);

    rootLayout->addWidget(iconGroup);

    // Initialize radio selection from SettingsModel
    const QString currentPreset = SettingsModel::instance().appIconPreset();
    if (currentPreset == QStringLiteral("alt1")) {
        m_alt1IconRadio->setChecked(true);
    } else if (currentPreset == QStringLiteral("alt2")) {
        m_alt2IconRadio->setChecked(true);
    } else if (currentPreset == QStringLiteral("custom")) {
        m_customIconRadio->setChecked(true);
    } else {
        m_defaultIconRadio->setChecked(true);
    }

    // 3. Windows System Associations Group (Milestone 3)
    auto* regGroup = new QGroupBox(tr("Windows File Associations"), this);
    auto* regLayout = new QVBoxLayout(regGroup);
    regLayout->setContentsMargins(10, 12, 10, 10);
    regLayout->setSpacing(8);

    auto* regInfo = new QLabel(tr("Associate Sview as default viewer for all supported image formats (.exr, .hdr, .tx, .tex, .rat, .png, .jpg, etc.) in Windows Explorer:"), regGroup);
    regInfo->setWordWrap(true);
    regLayout->addWidget(regInfo);

    auto* btnRow = new QHBoxLayout();
    m_registerDefaultAppBtn = new QPushButton(tr("Register as Default App (All Supported Formats)"), regGroup);
    m_registerDefaultAppBtn->setObjectName(QStringLiteral("settingsRegisterDefaultAppButton"));
    btnRow->addWidget(m_registerDefaultAppBtn);
    btnRow->addStretch();
    regLayout->addLayout(btnRow);

    m_regStatusLabel = new QLabel(regGroup);
    m_regStatusLabel->setObjectName(QStringLiteral("settingsRegStatusLabel"));
    regLayout->addWidget(m_regStatusLabel);

    rootLayout->addWidget(regGroup);
    rootLayout->addStretch(1);

    // 4. Bottom Dialog Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    auto* saveBtn = new QPushButton(tr("Save"), this);
    saveBtn->setDefault(true);
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(saveBtn);
    rootLayout->addLayout(buttonLayout);

    connect(m_registerDefaultAppBtn, &QPushButton::clicked, this, &SettingsDialog::onRegisterDefaultAppClicked);
    connect(m_browseCustomBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseCustomClicked);
    connect(m_applyIconBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyIconClicked);
    connect(m_defaultIconRadio, &QRadioButton::toggled, this, &SettingsDialog::onIconRadioToggled);
    connect(m_alt1IconRadio, &QRadioButton::toggled, this, &SettingsDialog::onIconRadioToggled);
    connect(m_alt2IconRadio, &QRadioButton::toggled, this, &SettingsDialog::onIconRadioToggled);
    connect(m_customIconRadio, &QRadioButton::toggled, this, &SettingsDialog::onIconRadioToggled);
    connect(m_customPathEdit, &QLineEdit::textChanged, this, &SettingsDialog::updateIconPreviews);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
}

void SettingsDialog::updateIconPreviews()
{
    m_defaultPreviewLabel->setPixmap(QIcon(QStringLiteral(":/icons/sview.ico")).pixmap(48, 48));
    m_alt1PreviewLabel->setPixmap(QIcon(QStringLiteral(":/icons/sview_alt1.ico")).pixmap(48, 48));
    m_alt2PreviewLabel->setPixmap(QIcon(QStringLiteral(":/icons/sview_alt2.ico")).pixmap(48, 48));

    const QString customPath = m_customPathEdit->text().trimmed();
    if (!customPath.isEmpty() && QFile::exists(customPath)) {
        QIcon customIco(customPath);
        if (!customIco.isNull()) {
            m_customPreviewLabel->setPixmap(customIco.pixmap(48, 48));
        } else {
            QPixmap pix(customPath);
            if (!pix.isNull()) {
                m_customPreviewLabel->setPixmap(pix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                m_customPreviewLabel->setText(tr("[?]"));
            }
        }
    } else {
        m_customPreviewLabel->setText(tr("[Custom]"));
    }
}

QString SettingsDialog::selectedIconPreset() const
{
    if (m_alt1IconRadio && m_alt1IconRadio->isChecked())
        return QStringLiteral("alt1");
    if (m_alt2IconRadio && m_alt2IconRadio->isChecked())
        return QStringLiteral("alt2");
    if (m_customIconRadio && m_customIconRadio->isChecked())
        return QStringLiteral("custom");
    return QStringLiteral("default");
}

int SettingsDialog::selectedIconIndex() const
{
    if (m_alt1IconRadio && m_alt1IconRadio->isChecked())
        return 1;
    if (m_alt2IconRadio && m_alt2IconRadio->isChecked())
        return 2;
    return 0;
}

void SettingsDialog::onIconRadioToggled()
{
    if (m_customIconRadio->isChecked() && m_customPathEdit->text().isEmpty()) {
        onBrowseCustomClicked();
    }
}

void SettingsDialog::onBrowseCustomClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Custom Icon Image"),
        QString(),
        tr("Images (*.ico *.png *.jpg *.jpeg *.bmp *.webp *.exr *.hdr);;All Files (*.*)"));

    if (!path.isEmpty()) {
        m_customPathEdit->setText(path);
        m_customIconRadio->setChecked(true);
        updateIconPreviews();
    }
}

void SettingsDialog::onApplyIconClicked()
{
    const QString preset = selectedIconPreset();
    QString effectivePath;
    int effectiveIndex = 0;

    if (preset == QStringLiteral("alt1")) {
        effectivePath = QStringLiteral(":/icons/sview_alt1.ico");
        effectiveIndex = 1;
        SettingsModel::instance().setAppIconPreset(preset);
        m_iconStatusLabel->setText(tr("✔ Cyber Neon (Alt 1) icon applied"));
    } else if (preset == QStringLiteral("alt2")) {
        effectivePath = QStringLiteral(":/icons/sview_alt2.ico");
        effectiveIndex = 2;
        SettingsModel::instance().setAppIconPreset(preset);
        m_iconStatusLabel->setText(tr("✔ Dark Slate (Alt 2) icon applied"));
    } else if (preset == QStringLiteral("custom")) {
        const QString customSource = m_customPathEdit->text().trimmed();
        if (customSource.isEmpty() || !QFile::exists(customSource)) {
            m_iconStatusLabel->setText(tr("⚠ Custom image file does not exist"));
            m_iconStatusLabel->setStyleSheet(QStringLiteral("color: #f44336;"));
            return;
        }

        // Auto-convert non-ico image to .ico under AppData
        if (customSource.endsWith(QStringLiteral(".ico"), Qt::CaseInsensitive)) {
            effectivePath = customSource;
        } else {
            QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            if (appData.isEmpty()) {
                appData = QDir::tempPath() + QStringLiteral("/SView");
            }
            QDir().mkpath(appData);
            const QString targetIco = appData + QStringLiteral("/custom_app_icon.ico");
            if (IconConverter::convertImageToIco(customSource, targetIco)) {
                effectivePath = targetIco;
            } else {
                effectivePath = customSource;
            }
        }
        effectiveIndex = 0;
        SettingsModel::instance().setCustomIconPath(effectivePath);
        SettingsModel::instance().setAppIconPreset(preset);
        m_iconStatusLabel->setText(tr("✔ Custom icon converted & applied"));
    } else {
        effectivePath = QStringLiteral(":/icons/sview.ico");
        effectiveIndex = 0;
        SettingsModel::instance().setAppIconPreset(QStringLiteral("default"));
        m_iconStatusLabel->setText(tr("✔ Default icon applied"));
    }

    // Update Windows Registry and Desktop Shell
    WindowsRegistryManager::setAppIcon(effectivePath, effectiveIndex);

    // Apply live icon to current Qt Application
    const QIcon appliedIcon(effectivePath);
    if (!appliedIcon.isNull()) {
        QApplication::setWindowIcon(appliedIcon);
    }

    m_iconStatusLabel->setStyleSheet(QStringLiteral("color: #4caf50; font-weight: bold;"));
}

void SettingsDialog::updateRegistrationStatus()
{
    bool allReg = true;
    const QStringList allExts = WindowsRegistryManager::allSupportedExtensions();
    for (const QString& ext : allExts) {
        if (!WindowsRegistryManager::isDefaultAppRegistered(ext)) {
            allReg = false;
            break;
        }
    }

    m_isRegistered = allReg;

    if (m_isRegistered) {
        m_regStatusLabel->setText(tr("✔ Sview is Registered for all image formats"));
        m_regStatusLabel->setStyleSheet(QStringLiteral("color: #4caf50; font-weight: bold;"));
    } else {
        m_regStatusLabel->setText(tr("Status: Not registered as default viewer"));
        m_regStatusLabel->setStyleSheet(QStringLiteral("color: #a0a0a0;"));
    }
}

void SettingsDialog::onRegisterDefaultAppClicked()
{
    const bool ok = WindowsRegistryManager::registerAllFileAssociations();
    if (ok) {
        updateRegistrationStatus();
    }
}

void SettingsDialog::onSaveClicked()
{
    onApplyIconClicked();
    SettingsModel::instance().setCacheBudgetMB(static_cast<std::size_t>(m_cacheBudgetSpin->value()));
    SettingsModel::instance().setFloatingDockEnabled(m_floatingDockCheck->isChecked());
    accept();
}

}  // namespace sview
