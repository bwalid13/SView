#include "BatchConverterDialog.h"

#include "../model/FolderModel.h"
#include "../model/ImageLoader.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>

namespace sview {

BatchConverterDialog::BatchConverterDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Batch Converter"));
    resize(500, 200);

    auto* layout = new QGridLayout(this);

    layout->addWidget(new QLabel(tr("Input Folder:")), 0, 0);
    m_inputEdit = new QLineEdit(this);
    layout->addWidget(m_inputEdit, 0, 1);
    auto* btnInput = new QPushButton(tr("Browse..."), this);
    layout->addWidget(btnInput, 0, 2);

    layout->addWidget(new QLabel(tr("Output Folder:")), 1, 0);
    m_outputEdit = new QLineEdit(this);
    layout->addWidget(m_outputEdit, 1, 1);
    auto* btnOutput = new QPushButton(tr("Browse..."), this);
    layout->addWidget(btnOutput, 1, 2);

    m_sameFolderCheck = new QCheckBox(tr("Use Input Folder as Output"), this);
    layout->addWidget(m_sameFolderCheck, 2, 1, 1, 2);

    layout->addWidget(new QLabel(tr("Export Format:")), 3, 0);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems({"exr", "png", "jpg", "tif", "tga"});
    layout->addWidget(m_formatCombo, 3, 1, 1, 2);

    m_halfResCheck = new QCheckBox(tr("Proxy Export (50% Half-Res)"), this);
    layout->addWidget(m_halfResCheck, 4, 1, 1, 2);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* btnCancel = new QPushButton(tr("Cancel"), this);
    auto* btnStart = new QPushButton(tr("Start Conversion"), this);
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnStart);
    layout->addLayout(btnLayout, 5, 0, 1, 3);

    connect(btnInput, &QPushButton::clicked, this, &BatchConverterDialog::onBrowseInput);
    connect(btnOutput, &QPushButton::clicked, this, &BatchConverterDialog::onBrowseOutput);
    connect(m_sameFolderCheck, &QCheckBox::toggled, this, &BatchConverterDialog::onSameFolderToggled);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnStart, &QPushButton::clicked, this, &BatchConverterDialog::onStartConversion);
}

bool BatchConverterDialog::isHalfRes() const
{
    return m_halfResCheck ? m_halfResCheck->isChecked() : false;
}

void BatchConverterDialog::setHalfRes(bool enabled)
{
    if (m_halfResCheck) {
        m_halfResCheck->setChecked(enabled);
    }
}

void BatchConverterDialog::onBrowseInput()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Input Folder"));
    if (!dir.isEmpty()) {
        m_inputEdit->setText(dir);
        if (m_sameFolderCheck->isChecked())
            m_outputEdit->setText(dir);
    }
}

void BatchConverterDialog::onBrowseOutput()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
    if (!dir.isEmpty()) {
        m_outputEdit->setText(dir);
    }
}

void BatchConverterDialog::onSameFolderToggled(bool checked)
{
    m_outputEdit->setEnabled(!checked);
    if (checked) {
        m_outputEdit->setText(m_inputEdit->text());
    }
}

void BatchConverterDialog::onStartConversion()
{
    const QString inDir = m_inputEdit->text();
    const QString outDir = m_outputEdit->text();
    const QString ext = m_formatCombo->currentText();

    if (inDir.isEmpty() || outDir.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please specify input and output folders."));
        return;
    }

    QDir dir(inDir);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("Error"), tr("Input folder does not exist."));
        return;
    }

    QDir out(outDir);
    if (!out.exists()) {
        out.mkpath(".");
    }

    QStringList nameFilters;
    for (const QString& e : FolderModel::supportedExtensions()) {
        nameFilters << QStringLiteral("*.") + e;
    }
    const QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files | QDir::NoSymLinks);

    if (files.isEmpty()) {
        QMessageBox::information(this, tr("Done"), tr("No supported images found in input folder."));
        return;
    }

    QProgressDialog progress(tr("Converting..."), tr("Cancel"), 0, files.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    int success = 0;
    for (int i = 0; i < files.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled())
            break;

        const QFileInfo& fi = files[i];
        const QString outPath = out.absoluteFilePath(fi.completeBaseName() + "." + ext);
        if (saveImage(fi.absoluteFilePath(), outPath, isHalfRes())) {
            success++;
        }
    }
    progress.setValue(files.size());

    QMessageBox::information(this, tr("Conversion Complete"),
                             tr("Successfully converted %1 / %2 files.").arg(success).arg(files.size()));
    accept();
}

}  // namespace sview
