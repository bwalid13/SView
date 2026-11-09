#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;

namespace sview {

class BatchConverterDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchConverterDialog(QWidget* parent = nullptr);

    QCheckBox* halfResCheckBox() const { return m_halfResCheck; }
    bool isHalfRes() const;
    void setHalfRes(bool enabled);

private slots:
    void onBrowseInput();
    void onBrowseOutput();
    void onSameFolderToggled(bool checked);
    void onStartConversion();

private:
    QLineEdit* m_inputEdit;
    QLineEdit* m_outputEdit;
    QCheckBox* m_sameFolderCheck;
    QComboBox* m_formatCombo;
    QCheckBox* m_halfResCheck;
};

}  // namespace sview
