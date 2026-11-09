#include "storage.hpp"

#include <QAction>
#include <QApplication>
#include <QCamera>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QImage>
#include <QLabel>
#include <QLockFile>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QMenu>
#include <QMessageBox>
#include <QPermissions>
#include <QPushButton>
#include <QPixmap>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
std::filesystem::path filePath(const QString &path)
{
#ifdef Q_OS_WIN
    return path.toStdWString();
#else
    return QFile::encodeName(path).toStdString();
#endif
}

class Window final : public QWidget {
public:
    explicit Window(const QString &folder) : folder(folder)
    {
        setWindowTitle(QStringLiteral("CameraGuard"));
        qApp->installEventFilter(this);
        auto *layout = new QFormLayout(this);
        status.setWordWrap(true);
        status.setMinimumWidth(350);
        layout->addRow(&status);
        for (const auto &device : QMediaDevices::videoInputs()) {
            devices.addItem(device.description(), device.id());
            if (device.isDefault())
                devices.setCurrentIndex(devices.count() - 1);
        }
        connect(&mediaDevices, &QMediaDevices::videoInputsChanged, this, [this] {
            const auto selected = devices.currentData();
            devices.clear();
            for (const auto &device : QMediaDevices::videoInputs())
                devices.addItem(device.description(), device.id());
            const int index = devices.findData(selected);
            if (index >= 0)
                devices.setCurrentIndex(index);
        });
        QSettings settings;
        days.setRange(1, 365);
        days.setValue(settings.value(QStringLiteral("days"), 7).toInt());
        limit.setRange(64, 102400);
        limit.setValue(settings.value(QStringLiteral("limit"), 2048).toInt());
        sensitivity.addItems({QStringLiteral("低"), QStringLiteral("中"), QStringLiteral("高")});
        sensitivity.setCurrentIndex(std::clamp(settings.value(QStringLiteral("sensitivity"), 1).toInt(), 0, 2));
        continuous.setChecked(settings.value(QStringLiteral("continuous"), false).toBool());
        preview.setChecked(true);
        layout->addRow(QStringLiteral("カメラ"), &devices);
        layout->addRow(QStringLiteral("保存日数"), &days);
        layout->addRow(QStringLiteral("容量上限 (MiB)"), &limit);
        layout->addRow(QStringLiteral("動体検知の感度"), &sensitivity);
        layout->addRow(&continuous, &preview);
        image.setFixedSize(400, 300);
        image.setAlignment(Qt::AlignCenter);
        image.setStyleSheet(QStringLiteral("background:#192630;color:white"));
        image.setText(QStringLiteral("カメラ停止中"));
        layout->addRow(&image);
        layout->addRow(&toggle, &browse);
        layout->addRow(new QLabel(QStringLiteral("×でバックグラウンドへ移動します。"), this));

        session.setCamera(&camera);
        session.setRecorder(&recorder);
        session.setVideoSink(&sink);
        connect(&sink, &QVideoSink::videoFrameChanged, this, &Window::frame);
        connect(&camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &error) {
            if (monitoring)
                stop(QStringLiteral("カメラエラー: ") + error);
        });
        connect(&recorder, &QMediaRecorder::errorOccurred, this,
                [this](QMediaRecorder::Error, const QString &error) {
            stop(QStringLiteral("録画エラー: ") + error);
        }, Qt::QueuedConnection);
        connect(&recorder, &QMediaRecorder::recorderStateChanged, this,
                [this](QMediaRecorder::RecorderState state) {
            if (state == QMediaRecorder::StoppedState)
                finishClip();
        }, Qt::QueuedConnection);

        connect(&toggle, &QPushButton::clicked, this, [this] {
            if (monitoring)
                stop();
            else
                requestStart();
        });
        connect(&browse, &QPushButton::clicked, this, &Window::openFolder);
        connect(&preview, &QCheckBox::toggled, this, [this](bool checked) {
            if (!checked)
                image.setText(QStringLiteral("プレビュー無効"));
        });
        statusAction = menu.addAction(QStringLiteral("停止中"));
        statusAction->setEnabled(false);
        menu.addAction(QStringLiteral("画面を開く"), this, &Window::reopen);
        stopAction = menu.addAction(QStringLiteral("監視を停止"), this, [this] { stop(); });
        stopAction->setEnabled(false);
        menu.addAction(QStringLiteral("録画フォルダーを開く"), this, &Window::openFolder);
        menu.addSeparator();
        menu.addAction(QStringLiteral("CameraGuardを終了"), this, &Window::quit);
        tray.setIcon(style()->standardIcon(QStyle::SP_DriveHDIcon));
        tray.setContextMenu(&menu);
        connect(&tray, &QSystemTrayIcon::activated, this, [this](auto reason) {
            if (reason == QSystemTrayIcon::DoubleClick)
                reopen();
        });
        tray.show();
        setStatus(QStringLiteral("停止中"));

        connect(&housekeeping, &QTimer::timeout, this, [this] {
            const auto now = clock.elapsed();
            if (now - lastFrame > 8000) {
                stop(QStringLiteral("カメラ映像が途切れました。接続と権限を確認してください。"));
                return;
            }
            if (now - lastCleanup >= 10000 && !finalizing) {
                lastCleanup = now;
                if (!activePath.isEmpty()
                    && QFileInfo(activePath).size() >= qint64(guard::MiB * limit.value()))
                    stopClip();
                else
                    clean();
            }
        });
        housekeeping.setInterval(1000);
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == qApp && event->type() == QEvent::Quit && !readyToQuit) {
            quit();
            return true;
        }
        return QWidget::eventFilter(object, event);
    }

    void closeEvent(QCloseEvent *event) override
    {
        event->ignore();
        if (QSystemTrayIcon::isSystemTrayAvailable())
            hide();
        else
            quit();
    }

private:
    void reopen()
    {
        showNormal();
        raise();
        activateWindow();
    }

    void setStatus(const QString &message)
    {
        if (status.text() == message)
            return;
        status.setText(message);
        statusAction->setText(message.left(65));
        tray.setToolTip(QStringLiteral("CameraGuard — ") + message);
    }

    void openFolder()
    {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
            setStatus(QStringLiteral("録画フォルダーを開けません。"));
    }

    void requestStart()
    {
        if (monitoring || finalizing || exiting)
            return;
#ifdef Q_OS_MACOS
        QCameraPermission permission;
        if (qApp->checkPermission(permission) == Qt::PermissionStatus::Undetermined) {
            toggle.setEnabled(false);
            setStatus(QStringLiteral("カメラの使用許可を待っています…"));
            qApp->requestPermission(permission, this, [this](const QPermission &result) {
                if (exiting)
                    return;
                toggle.setEnabled(true);
                if (result.status() == Qt::PermissionStatus::Granted)
                    start();
                else
                    setStatus(QStringLiteral("システム設定でカメラのアクセスを許可してください。"));
            });
            return;
        }
        if (qApp->checkPermission(permission) != Qt::PermissionStatus::Granted) {
            setStatus(QStringLiteral("システム設定でカメラのアクセスを許可してください。"));
            return;
        }
#endif
        start();
    }

    void start()
    {
        const auto selected = devices.currentData().toByteArray();
        const auto inputs = QMediaDevices::videoInputs();
        const auto found = std::find_if(inputs.begin(), inputs.end(), [&selected](const auto &device) {
            return device.id() == selected;
        });
        if (found == inputs.end()) {
            setStatus(QStringLiteral("利用できるカメラを選んでください。"));
            return;
        }

        QMediaFormat format(QMediaFormat::MPEG4);
        if (format.supportedVideoCodecs(QMediaFormat::Encode).contains(QMediaFormat::VideoCodec::H264))
            format.setVideoCodec(QMediaFormat::VideoCodec::H264);
        else if (format.supportedVideoCodecs(QMediaFormat::Encode).contains(QMediaFormat::VideoCodec::MPEG4))
            format.setVideoCodec(QMediaFormat::VideoCodec::MPEG4);
        else {
            setStatus(QStringLiteral("MP4の録画機能を利用できません。"));
            return;
        }
        recorder.setMediaFormat(format);
        recorder.setQuality(QMediaRecorder::NormalQuality);
        recorder.setVideoResolution(640, 480);
        recorder.setVideoFrameRate(12);

        if (!clean())
            return;

        QSettings settings;
        settings.setValue(QStringLiteral("days"), days.value());
        settings.setValue(QStringLiteral("limit"), limit.value());
        settings.setValue(QStringLiteral("sensitivity"), sensitivity.currentIndex());
        settings.setValue(QStringLiteral("continuous"), continuous.isChecked());
        camera.setCameraDevice(*found);
        const auto formats = found->videoFormats();
        const auto distance = [](const QCameraFormat &candidate) {
            const auto size = candidate.resolution();
            return std::abs(size.width() - 640) + std::abs(size.height() - 480);
        };
        if (!formats.isEmpty())
            camera.setCameraFormat(*std::min_element(formats.begin(), formats.end(),
                [&distance](const auto &a, const auto &b) { return distance(a) < distance(b); }));

        monitoring = true;
        stopMessage.clear();
        previous = {};
        clock.restart();
        lastFrame = lastCleanup = 0;
        nextCheck = nextPreview = motionUntil = 0;
        updateControls();
        setStatus(QStringLiteral("カメラを起動中…"));
        housekeeping.start();
        camera.start();
    }

    void updateControls()
    {
        const bool idle = !monitoring && !finalizing && !exiting;
        devices.setEnabled(idle);
        days.setEnabled(idle);
        limit.setEnabled(idle);
        sensitivity.setEnabled(idle);
        continuous.setEnabled(idle);
        toggle.setEnabled(!exiting && (monitoring || !finalizing));
        toggle.setText(monitoring ? QStringLiteral("監視を停止") : QStringLiteral("監視を開始"));
        stopAction->setEnabled(monitoring);
    }

    bool clean()
    {
        try {
            if (guard::prune(filePath(folder), std::chrono::hours(24 * days.value()),
                             guard::MiB * limit.value(), filePath(activePath)))
                return true;
            stop(QStringLiteral("ディスクの空きが256 MiB未満のため停止しました。"));
        } catch (const std::filesystem::filesystem_error &) {
            stop(QStringLiteral("録画フォルダーを整理できません。アクセス権を確認してください。"));
        }
        return false;
    }

    void frame(const QVideoFrame &video)
    {
        if (!monitoring || !video.isValid())
            return;
        const auto now = clock.elapsed();
        lastFrame = now;
        const bool detect = !continuous.isChecked() && now >= nextCheck;
        const bool display = isVisible() && !isMinimized() && preview.isChecked() && now >= nextPreview;
        if (detect || display) {
            const auto captured = video.toImage();
            if (!captured.isNull()) {
                if (display) {
                    image.setPixmap(QPixmap::fromImage(captured.scaled(
                        image.size(), Qt::KeepAspectRatio, Qt::FastTransformation)));
                    nextPreview = now + 250;
                }
                if (detect) {
                    const auto gray = captured.scaled(160, 120, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                        .convertToFormat(QImage::Format_Grayscale8);
                    if (!previous.isNull()) {
                        int changed = 0;
                        for (int y = 0; y < gray.height(); ++y) {
                            const auto *before = previous.constScanLine(y);
                            const auto *after = gray.constScanLine(y);
                            for (int x = 0; x < gray.width(); ++x)
                                changed += std::abs(int(after[x]) - int(before[x])) > 24;
                        }
                        constexpr int thresholds[] = {1056, 422, 154};
                        if (changed >= thresholds[sensitivity.currentIndex()])
                            motionUntil = now + 6000;
                    }
                    previous = gray;
                    nextCheck = now + 125;
                }
            }
        }

        if (finalizing)
            return;
        const bool shouldRecord = continuous.isChecked() || now < motionUntil;
        if (!activePath.isEmpty()) {
            if (!shouldRecord || now - clipStarted >= 60000)
                stopClip();
        } else if (shouldRecord) {
            startClip(now);
        } else {
            setStatus(QStringLiteral("監視中・動きを待機"));
        }
    }

    void startClip(qint64 now)
    {
        if (!clean())
            return;
        const auto date = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const auto id = QUuid::createUuid().toString(QUuid::Id128).left(12);
        activePath = QDir(folder).filePath(QStringLiteral("guard_%1_%2.part.mp4").arg(date, id));
        clipStarted = now;
        recorder.setOutputLocation(QUrl::fromLocalFile(activePath));
        recorder.record();
        if (recorder.recorderState() == QMediaRecorder::RecordingState)
            setStatus(QStringLiteral("● 録画中"));
        else
            stop(QStringLiteral("録画を開始できません。カメラと保存先を確認してください。"));
    }

    void stopClip()
    {
        finalizing = true;
        if (recorder.recorderState() != QMediaRecorder::StoppedState)
            recorder.stop();
        else
            finishClip();
    }

    void finishClip()
    {
        if (activePath.isEmpty())
            return;
        const auto path = std::exchange(activePath, QString());
        finalizing = false;
        const QFileInfo info(path);
        if (info.isFile() && info.size() > 0 && recorder.error() == QMediaRecorder::NoError) {
            auto destination = path;
            destination.chop(9);
            destination += QStringLiteral(".mp4");
            if (!QFile::rename(path, destination)) {
                stop(QStringLiteral("録画ファイルの確定に失敗しました。"));
                return;
            }
        } else if (info.isFile() && info.size() == 0) {
            QFile::remove(path);
        }
        if (!monitoring)
            finishStop();
        else
            clean();
    }

    void stop(const QString &reason = {})
    {
        monitoring = false;
        if (!reason.isEmpty())
            stopMessage = reason;
        housekeeping.stop();
        if (!activePath.isEmpty()) {
            setStatus(QStringLiteral("録画を保存中…"));
            if (!finalizing)
                stopClip();
            updateControls();
        } else {
            finishStop();
        }
    }

    void finishStop()
    {
        camera.stop();
        previous = {};
        image.setText(QStringLiteral("カメラ停止中"));
        setStatus(stopMessage.isEmpty() ? QStringLiteral("停止中") : stopMessage);
        if (!stopMessage.isEmpty() && !exiting && !isVisible())
            reopen();
        updateControls();
        if (exiting) {
            readyToQuit = true;
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
    }

    void quit()
    {
        exiting = true;
        stop();
    }

    const QString folder;
    QLabel status, image;
    QComboBox devices, sensitivity;
    QSpinBox days, limit;
    QCheckBox continuous{QStringLiteral("常時録画")};
    QCheckBox preview{QStringLiteral("プレビュー表示")};
    QPushButton toggle{QStringLiteral("監視を開始")};
    QPushButton browse{QStringLiteral("録画フォルダーを開く")};
    QMenu menu;
    QSystemTrayIcon tray;
    QAction *statusAction = nullptr;
    QAction *stopAction = nullptr;
    QMediaDevices mediaDevices;
    QCamera camera;
    QMediaRecorder recorder;
    QVideoSink sink;
    QMediaCaptureSession session;
    QElapsedTimer clock;
    QTimer housekeeping;
    QImage previous;
    QString activePath, stopMessage;
    qint64 lastFrame = 0, lastCleanup = 0, nextCheck = 0, nextPreview = 0;
    qint64 motionUntil = 0, clipStarted = 0;
    bool monitoring = false, finalizing = false, exiting = false, readyToQuit = false;
};
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--smoke-test") {
            QCoreApplication smoke(argc, argv);
            QMediaFormat format(QMediaFormat::MPEG4);
            const auto codecs = format.supportedVideoCodecs(QMediaFormat::Encode);
            return codecs.contains(QMediaFormat::VideoCodec::H264)
                || codecs.contains(QMediaFormat::VideoCodec::MPEG4) ? 0 : 1;
        }
    }
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Hotakacchi"));
    app.setApplicationName(QStringLiteral("CameraGuard"));
    app.setQuitOnLastWindowClosed(false);

    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const auto folder = QDir(root).filePath(QStringLiteral("recordings"));
    if (root.isEmpty() || !QDir().mkpath(folder)) {
        QMessageBox::critical(nullptr, QStringLiteral("CameraGuard"),
                              QStringLiteral("録画フォルダーを作成できません。"));
        return 1;
    }
    QLockFile lock(QDir(root).filePath(QStringLiteral("instance.lock")));
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        QMessageBox::information(nullptr, QStringLiteral("CameraGuard"),
                                 QStringLiteral("起動済みのCameraGuardを通知領域から開いてください。"));
        return 1;
    }
    Window window(folder);
    window.show();
    return app.exec();
}
