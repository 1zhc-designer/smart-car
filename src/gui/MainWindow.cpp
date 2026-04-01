#include "gui/MainWindow.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QVBoxLayout>
#include <QWidget>

#include <opencv2/opencv.hpp>

namespace {
QImage matToQImage(const cv::Mat& mat) {
    if (mat.empty()) {
        return {};
    }
    if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    }
    return {};
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    connectSignals();
    system_.start();

    lowLimitSpin_->setValue(system_.lowLimit());
    highLimitSpin_->setValue(system_.highLimit());

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::refreshUi);
    refreshTimer_->start(100);

    refreshUi();
}

MainWindow::~MainWindow() {
    system_.stop();
}

void MainWindow::setupUi() {
    setWindowTitle("Smart Car Control");
    resize(1100, 850);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);

    auto* cameraGroup = new QGroupBox("Camera View", this);
    auto* cameraLayout = new QVBoxLayout(cameraGroup);
    cameraLabel_ = new QLabel(this);
    cameraLabel_->setMinimumSize(640, 480);
    cameraLabel_->setStyleSheet("background-color: black; border: 1px solid gray;");
    cameraLabel_->setAlignment(Qt::AlignCenter);
    cameraLayout->addWidget(cameraLabel_);
    mainLayout->addWidget(cameraGroup);

    auto* motionGroup = new QGroupBox("Movement Control", this);
    auto* motionLayout = new QVBoxLayout(motionGroup);
    auto* motionTop = new QHBoxLayout();
    forwardBtn_ = new QPushButton("Forward", this);
    motionTop->addStretch();
    motionTop->addWidget(forwardBtn_);
    motionTop->addStretch();

    auto* motionMid = new QHBoxLayout();
    leftBtn_ = new QPushButton("Left", this);
    stopBtn_ = new QPushButton("Stop", this);
    rightBtn_ = new QPushButton("Right", this);
    motionMid->addWidget(leftBtn_);
    motionMid->addWidget(stopBtn_);
    motionMid->addWidget(rightBtn_);

    auto* motionBottom = new QHBoxLayout();
    backwardBtn_ = new QPushButton("Backward", this);
    motionBottom->addStretch();
    motionBottom->addWidget(backwardBtn_);
    motionBottom->addStretch();

    motionLayout->addLayout(motionTop);
    motionLayout->addLayout(motionMid);
    motionLayout->addLayout(motionBottom);
    mainLayout->addWidget(motionGroup);

    auto* gimbalGroup = new QGroupBox("Servo / Gimbal Control", this);
    auto* gimbalLayout = new QVBoxLayout(gimbalGroup);

    auto* gimbalTop = new QHBoxLayout();
    gimbalUpBtn_ = new QPushButton("Up", this);
    gimbalTop->addStretch();
    gimbalTop->addWidget(gimbalUpBtn_);
    gimbalTop->addStretch();

    auto* gimbalMid = new QHBoxLayout();
    gimbalLeftBtn_ = new QPushButton("Left", this);
    gimbalResetBtn_ = new QPushButton("Reset", this);
    gimbalRightBtn_ = new QPushButton("Right", this);
    gimbalMid->addWidget(gimbalLeftBtn_);
    gimbalMid->addWidget(gimbalResetBtn_);
    gimbalMid->addWidget(gimbalRightBtn_);

    auto* gimbalBottom = new QHBoxLayout();
    gimbalDownBtn_ = new QPushButton("Down", this);
    gimbalBottom->addStretch();
    gimbalBottom->addWidget(gimbalDownBtn_);
    gimbalBottom->addStretch();

    gimbalLayout->addLayout(gimbalTop);
    gimbalLayout->addLayout(gimbalMid);
    gimbalLayout->addLayout(gimbalBottom);
    mainLayout->addWidget(gimbalGroup);

    auto* tempGroup = new QGroupBox("Temperature Monitor", this);
    auto* tempLayout = new QVBoxLayout(tempGroup);
    currentTempLabel_ = new QLabel("Current Temperature: -- | Light: --", this);
    currentStatusLabel_ = new QLabel("Status: --", this);

    auto* limitsLayout = new QHBoxLayout();
    lowLimitSpin_ = new QSpinBox(this);
    highLimitSpin_ = new QSpinBox(this);
    applyLimitsBtn_ = new QPushButton("Apply", this);
    lowLimitSpin_->setRange(-20, 120);
    highLimitSpin_->setRange(-20, 120);

    limitsLayout->addWidget(new QLabel("Lower Limit:", this));
    limitsLayout->addWidget(lowLimitSpin_);
    limitsLayout->addSpacing(20);
    limitsLayout->addWidget(new QLabel("Upper Limit:", this));
    limitsLayout->addWidget(highLimitSpin_);
    limitsLayout->addSpacing(20);
    limitsLayout->addWidget(applyLimitsBtn_);

    tempLayout->addWidget(currentTempLabel_);
    tempLayout->addWidget(currentStatusLabel_);
    tempLayout->addLayout(limitsLayout);
    mainLayout->addWidget(tempGroup);
}

void MainWindow::connectSignals() {
    connect(forwardBtn_, &QPushButton::clicked, this, [this]() { system_.moveForward(); });
    connect(backwardBtn_, &QPushButton::clicked, this, [this]() { system_.moveBackward(); });
    connect(leftBtn_, &QPushButton::clicked, this, [this]() { system_.turnLeft(); });
    connect(rightBtn_, &QPushButton::clicked, this, [this]() { system_.turnRight(); });
    connect(stopBtn_, &QPushButton::clicked, this, [this]() { system_.stopMotion(); });
    connect(gimbalUpBtn_, &QPushButton::clicked, this, [this]() { system_.gimbalUp(); });
    connect(gimbalDownBtn_, &QPushButton::clicked, this, [this]() { system_.gimbalDown(); });
    connect(gimbalLeftBtn_, &QPushButton::clicked, this, [this]() { system_.gimbalLeft(); });
    connect(gimbalRightBtn_, &QPushButton::clicked, this, [this]() { system_.gimbalRight(); });
    connect(gimbalResetBtn_, &QPushButton::clicked, this, [this]() { system_.gimbalReset(); });
    connect(applyLimitsBtn_, &QPushButton::clicked, this, &MainWindow::applyTemperatureLimits);
}

void MainWindow::refreshUi() {
    updateCameraView();
    updateTemperatureView();
}

void MainWindow::applyTemperatureLimits() {
    const int low = lowLimitSpin_->value();
    const int high = highLimitSpin_->value();
    if (low < high) {
        system_.setTemperatureLimits(low, high);
    }
}

void MainWindow::updateCameraView() {
    const cv::Mat frame = system_.latestFrame();
    const QImage image = matToQImage(frame);
    if (!image.isNull()) {
        cameraLabel_->setPixmap(QPixmap::fromImage(image).scaled(cameraLabel_->size(), Qt::KeepAspectRatio,
                                                                 Qt::SmoothTransformation));
    }
}

void MainWindow::updateTemperatureView() {
    currentTempLabel_->setText(
        QString("Current Temperature: %1 °C | Light: %2")
            .arg(system_.currentTemperature(), 0, 'f', 2)
            .arg(system_.currentLightLevel()));
    currentStatusLabel_->setText(QString("Status: %1").arg(QString::fromStdString(system_.currentStatus())));
    if (!lowLimitSpin_->hasFocus()) {
        lowLimitSpin_->setValue(system_.lowLimit());
    }
    if (!highLimitSpin_->hasFocus()) {
        highLimitSpin_->setValue(system_.highLimit());
    }
}
