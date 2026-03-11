#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>

#include "gui/SystemFacade.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshUi();
    void applyTemperatureLimits();

private:
    void setupUi();
    void connectSignals();
    void updateCameraView();
    void updateTemperatureView();

private:
    SystemFacade system_;

    QLabel* cameraLabel_{nullptr};

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

    QTimer* refreshTimer_{nullptr};
};