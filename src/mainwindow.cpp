#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QImageCapture>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QPermission>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QtMath>

#include <algorithm>

namespace {

QString outputPath(const QString &prefix, const QString &ext)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    return QDir(dir).filePath(
        QStringLiteral("%1_%2.%3")
            .arg(prefix, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"), ext));
}

QString formatLabel(const QCameraFormat &f)
{
    const QSize r = f.resolution();
    const int minFps = qRound(f.minFrameRate());
    const int maxFps = qRound(f.maxFrameRate());
    const QString fps = (minFps == maxFps) ? QString::number(maxFps)
                                           : QStringLiteral("%1-%2").arg(minFps).arg(maxFps);
    return QStringLiteral("%1x%2 @ %3 fps  [%4]")
        .arg(r.width())
        .arg(r.height())
        .arg(fps, QVideoFrameFormat::pixelFormatToString(f.pixelFormat()));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("USB 視訊"));

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    auto *bar = new QHBoxLayout;
    m_deviceCombo = new QComboBox;
    m_deviceCombo->setMinimumWidth(220);
    m_formatCombo = new QComboBox;
    m_formatCombo->setMinimumWidth(260);
    m_startBtn = new QPushButton(tr("開始"));
    m_snapBtn = new QPushButton(tr("拍照"));
    m_recBtn = new QPushButton(tr("錄影"));
    bar->addWidget(new QLabel(tr("裝置:")));
    bar->addWidget(m_deviceCombo);
    bar->addWidget(new QLabel(tr("格式:")));
    bar->addWidget(m_formatCombo);
    bar->addStretch();
    bar->addWidget(m_startBtn);
    bar->addWidget(m_snapBtn);
    bar->addWidget(m_recBtn);
    root->addLayout(bar);

    m_video = new QVideoWidget;
    m_video->setMinimumSize(640, 360);
    m_video->setAspectRatioMode(Qt::KeepAspectRatio);
    root->addWidget(m_video, 1);

    m_status = new QLabel;
    statusBar()->addWidget(m_status, 1);

    setCentralWidget(central);
    resize(kDefaultWidth + 40, kDefaultHeight + 120);

    m_session = new QMediaCaptureSession(this);
    m_imageCapture = new QImageCapture(this);
    m_recorder = new QMediaRecorder(this);
    m_session->setVideoOutput(m_video);
    m_session->setImageCapture(m_imageCapture);
    m_session->setRecorder(m_recorder);

    connect(&m_mediaDevices, &QMediaDevices::videoInputsChanged, this, &MainWindow::refreshDevices);
    connect(m_deviceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onDeviceChanged);
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onFormatChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::toggleCamera);
    connect(m_snapBtn, &QPushButton::clicked, this, &MainWindow::takeSnapshot);
    connect(m_recBtn, &QPushButton::clicked, this, &MainWindow::toggleRecording);

    connect(m_imageCapture, &QImageCapture::imageSaved, this, [this](int, const QString &file) {
        statusBar()->showMessage(tr("已儲存照片: %1").arg(file), 5000);
    });
    connect(m_imageCapture, &QImageCapture::errorOccurred, this,
            [this](int, QImageCapture::Error, const QString &msg) {
                statusBar()->showMessage(tr("拍照失敗: %1").arg(msg), 5000);
            });
    connect(m_recorder, &QMediaRecorder::recorderStateChanged, this, [this] {
        m_recBtn->setText(m_recorder->recorderState() == QMediaRecorder::RecordingState
                              ? tr("停止錄影")
                              : tr("錄影"));
        updateButtons();
    });
    connect(m_recorder, &QMediaRecorder::errorOccurred, this,
            [this](QMediaRecorder::Error, const QString &msg) {
                statusBar()->showMessage(tr("錄影錯誤: %1").arg(msg), 5000);
            });

    refreshDevices();
    updateButtons();
    startCameraWithPermission();
}

MainWindow::~MainWindow()
{
    if (m_recorder && m_recorder->recorderState() == QMediaRecorder::RecordingState)
        m_recorder->stop();
    if (m_camera)
        m_camera->stop();
}

void MainWindow::startCameraWithPermission()
{
    QCameraPermission perm;
    switch (qApp->checkPermission(perm)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(perm, this, &MainWindow::startCameraWithPermission);
        return;
    case Qt::PermissionStatus::Denied:
        m_status->setText(tr("未取得攝影機權限，請至系統設定允許。"));
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
    if (m_camera)
        m_camera->start();
}

void MainWindow::refreshDevices()
{
    const QCameraDevice previous = m_camera ? m_camera->cameraDevice() : QCameraDevice();

    QSignalBlocker block(m_deviceCombo);
    m_deviceCombo->clear();
    const auto devices = QMediaDevices::videoInputs();
    int select = 0;
    for (int i = 0; i < devices.size(); ++i) {
        m_deviceCombo->addItem(devices[i].description(), QVariant::fromValue(devices[i]));
        if (devices[i] == previous)
            select = i;
        else if (previous.isNull() && devices[i].isDefault())
            select = i;
    }
    if (devices.isEmpty()) {
        m_status->setText(tr("找不到視訊裝置，請接上 USB 攝影機。"));
        delete m_camera;
        m_camera = nullptr;
        m_formats.clear();
        m_formatCombo->clear();
        updateButtons();
        return;
    }
    m_deviceCombo->setCurrentIndex(select);
    block.unblock();

    // Only rebuild the camera if the selected device actually changed.
    if (!m_camera || m_camera->cameraDevice() != devices[select]) {
        onDeviceChanged();
    }
}

void MainWindow::onDeviceChanged()
{
    const auto device = m_deviceCombo->currentData().value<QCameraDevice>();
    if (device.isNull())
        return;
    const bool wasActive = m_camera && m_camera->isActive();
    setupCamera(device);
    if (wasActive)
        startCameraWithPermission();
}

void MainWindow::setupCamera(const QCameraDevice &device)
{
    if (m_recorder->recorderState() == QMediaRecorder::RecordingState)
        m_recorder->stop();

    m_session->setCamera(nullptr);
    delete m_camera;

    m_camera = new QCamera(device, this);
    m_session->setCamera(m_camera);
    connect(m_camera, &QCamera::activeChanged, this, [this] {
        updateStatus();
        updateButtons();
    });
    connect(m_camera, &QCamera::errorOccurred, this, &MainWindow::onCameraError);

    populateFormats();
    updateStatus();
    updateButtons();
}

void MainWindow::populateFormats()
{
    m_formats = m_camera->cameraDevice().videoFormats();

    // Largest resolution first, then highest frame rate.
    std::sort(m_formats.begin(), m_formats.end(), [](const QCameraFormat &a, const QCameraFormat &b) {
        const qint64 pa = qint64(a.resolution().width()) * a.resolution().height();
        const qint64 pb = qint64(b.resolution().width()) * b.resolution().height();
        if (pa != pb)
            return pa > pb;
        return a.maxFrameRate() > b.maxFrameRate();
    });

    QSignalBlocker block(m_formatCombo);
    m_formatCombo->clear();
    for (const auto &f : std::as_const(m_formats))
        m_formatCombo->addItem(formatLabel(f));

    const QCameraFormat def = pickDefaultFormat(m_formats);
    const int idx = m_formats.indexOf(def);
    if (idx >= 0)
        m_formatCombo->setCurrentIndex(idx);
    if (!def.isNull())
        m_camera->setCameraFormat(def);
}

// Prefer 1280x720 whose frame-rate range contains 30 fps (and is closest to it);
// otherwise any 1280x720; otherwise the closest resolution to 1280x720.
QCameraFormat MainWindow::pickDefaultFormat(const QList<QCameraFormat> &formats) const
{
    QCameraFormat best;
    double bestScore = -1;
    for (const auto &f : formats) {
        const QSize r = f.resolution();
        const double dist = qAbs(r.width() - kDefaultWidth) + qAbs(r.height() - kDefaultHeight);
        double fpsDist = 0;
        if (kDefaultFps < f.minFrameRate())
            fpsDist = f.minFrameRate() - kDefaultFps;
        else if (kDefaultFps > f.maxFrameRate())
            fpsDist = kDefaultFps - f.maxFrameRate();
        // Resolution match dominates; fps match breaks ties; raw formats (e.g. YUYV) are
        // fine, but MJPEG-style compressed ones are not penalised either.
        const double score = 1.0 / (1.0 + dist * 1000.0 + fpsDist);
        if (score > bestScore) {
            bestScore = score;
            best = f;
        }
    }
    return best;
}

void MainWindow::onFormatChanged()
{
    const int i = m_formatCombo->currentIndex();
    if (!m_camera || i < 0 || i >= m_formats.size())
        return;
    const bool wasActive = m_camera->isActive();
    if (wasActive)
        m_camera->stop();
    m_camera->setCameraFormat(m_formats[i]);
    if (wasActive)
        m_camera->start();
    updateStatus();
}

void MainWindow::toggleCamera()
{
    if (!m_camera)
        return;
    if (m_camera->isActive()) {
        if (m_recorder->recorderState() == QMediaRecorder::RecordingState)
            m_recorder->stop();
        m_camera->stop();
    } else {
        startCameraWithPermission();
    }
}

void MainWindow::takeSnapshot()
{
    if (m_camera && m_camera->isActive())
        m_imageCapture->captureToFile(outputPath("snapshot", "jpg"));
}

void MainWindow::toggleRecording()
{
    if (m_recorder->recorderState() == QMediaRecorder::RecordingState) {
        m_recorder->stop();
        statusBar()->showMessage(tr("已儲存影片: %1").arg(m_recorder->actualLocation().toLocalFile()), 5000);
        return;
    }
    if (!m_camera || !m_camera->isActive())
        return;
    m_recorder->setQuality(QMediaRecorder::HighQuality);
    m_recorder->setVideoResolution(m_camera->cameraFormat().resolution());
    m_recorder->setVideoFrameRate(kDefaultFps);
    m_recorder->setOutputLocation(QUrl::fromLocalFile(outputPath("video", "mp4")));
    m_recorder->record();
}

void MainWindow::onCameraError(QCamera::Error, const QString &message)
{
    m_status->setText(tr("攝影機錯誤: %1").arg(message));
}

void MainWindow::updateStatus()
{
    if (!m_camera)
        return;
    const QCameraFormat f = m_camera->cameraFormat();
    const QSize r = f.resolution();
    m_status->setText(tr("%1 — %2x%3 @ %4-%5 fps — %6")
                          .arg(m_camera->cameraDevice().description())
                          .arg(r.width())
                          .arg(r.height())
                          .arg(qRound(f.minFrameRate()))
                          .arg(qRound(f.maxFrameRate()))
                          .arg(m_camera->isActive() ? tr("執行中") : tr("已停止")));
}

void MainWindow::updateButtons()
{
    const bool has = m_camera != nullptr;
    const bool active = has && m_camera->isActive();
    m_startBtn->setEnabled(has);
    m_startBtn->setText(active ? tr("停止") : tr("開始"));
    m_snapBtn->setEnabled(active);
    m_recBtn->setEnabled(active);
    m_formatCombo->setEnabled(has);
}
