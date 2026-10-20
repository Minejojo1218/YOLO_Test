#pragma once

#include <QDialog>
#include <QPair>
#include <QStringList>

#include <vector>

#include <opencv2/core.hpp>

#include "Annotation.h"

class ProjectManager;
class QLabel;
class QLineEdit;
class QListWidget;
class QComboBox;
class QDoubleSpinBox;

// 图像预处理：处理步骤链（镜像/旋转/亮度/对比度/反色/灰度/模糊/锐化）+ 预览 + 批量应用
// 批量应用输出到独立目录（默认 <项目>/preprocessed），镜像/旋转会同步转换标签坐标。
class PreprocessDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreprocessDialog(ProjectManager *project, QWidget *parent = nullptr);

private slots:
    void onImageSelected(int row);
    void onAddStep();
    void onDeleteStep();
    void onUpStep();
    void onDownStep();
    void onStepSelectionChanged(int row);
    void onPreview();
    void onReset();
    void onApply();
    void onBrowseOutput();

private:
    struct Step
    {
        int type = 0;
        double value = 0.0;
    };

    static QString stepName(int type);
    static QPair<double, double> valueRange(int type);   // (最小, 最大)
    static double defaultValue(int type);
    static bool isGeometryStep(int type);
    static cv::Mat applySteps(const cv::Mat &img, const std::vector<Step> &steps);
    Annotation transformLabels(const Annotation &ann) const;

    void refreshStepList();
    void updateParamsForType(int type);
    void loadImage(int row);
    void showMat(QLabel *label, const cv::Mat &m);

    ProjectManager *m_project = nullptr;
    QStringList m_files;
    int m_currentRow = -1;
    cv::Mat m_original;
    std::vector<Step> m_steps;

    QListWidget *m_imageList = nullptr;
    QLabel *m_origLabel = nullptr;
    QLabel *m_procLabel = nullptr;
    QListWidget *m_stepList = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QDoubleSpinBox *m_valueSpin = nullptr;
    QLineEdit *m_outDir = nullptr;
};
