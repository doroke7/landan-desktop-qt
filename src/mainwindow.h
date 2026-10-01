#pragma once

#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QList>
#include <QMainWindow>
#include <QMediaDevices>

class QComboBox;
class QImageCapture;
class QLabel;
class QMediaCaptureSession;
class QMediaRecorder;
class QPushButton;
class QVideoWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshDevices();
    void onDeviceChanged();
    void onFormatChanged();
    void toggleCamera();
    void takeSnapshot();
    void toggleRecording();
    void onCameraError(QCamera::Error error, const QString &message);

private:
    static constexpr int kDefaultWidth = 1280;
    static constexpr int kDefaultHeight = 720;
    static constexpr int kDefaultFps = 30;

    void startCameraWithPermission();
    void setupCamera(const QCameraDevice &device);
    void populateFormats();
    QCameraFormat pickDefaultFormat(const QList<QCameraFormat> &formats) const;
    void updateStatus();
    void updateButtons();

    QComboBox *m_deviceCombo = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_snapBtn = nullptr;
    QPushButton *m_recBtn = nullptr;
    QVideoWidget *m_video = nullptr;
    QLabel *m_status = nullptr;

    QList<QCameraFormat> m_formats;  // parallel to m_formatCombo items

    QMediaDevices m_mediaDevices;
    QMediaCaptureSession *m_session = nullptr;
    QCamera *m_camera = nullptr;
    QImageCapture *m_imageCapture = nullptr;
    QMediaRecorder *m_recorder = nullptr;
};
