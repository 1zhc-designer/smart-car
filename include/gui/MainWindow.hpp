#pragma once

#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include "gui/SystemFacade.hpp"

/**
 * @brief Qt GUI for the smart car demo.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void applyTemperatureLimits();
    void applyModeSelection();

private:
    void setupUi();
    void connectSignals();
    void refreshUi();
    void updateCameraView();
    void updateTemperatureView();
    void updateModeView();

    SystemFacade system_;

    QLabel* cameraLabel_{nullptr};

    QComboBox* modeCombo_{nullptr};
    QLabel* modeStatusLabel_{nullptr};

    QPushButton* forwardBtn_{nullptr};
    QPushButton* backwardBtn_{nullptr};
    QPushButton* leftBtn_{nullptr};
    QPushButton* rightBtn_{nullptr};
    QPushButton* stopBtn_{nullptr};

    QPushButton* gimbalUpBtn_{nullptr};
    QPushButton* gimbalDownBtn_{nullptr};
    QPushButton* gimbalLeftBtn_{nullptr};
    QPushButton* gimbalRightBtn_{nullptr};
    QPushButton* gimbalResetBtn_{nullptr};

    QLabel* currentTempLabel_{nullptr};
    QLabel* currentStatusLabel_{nullptr};
    QSpinBox* lowLimitSpin_{nullptr};
    QSpinBox* highLimitSpin_{nullptr};
    QPushButton* applyLimitsBtn_{nullptr};
};