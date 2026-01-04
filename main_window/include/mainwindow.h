#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ws_sink.h"

#include <QMainWindow>
#include <QScreen>

#include "main_frame_media.h"
#include "gl_spec.h"

#include <string>
#include <thread>

#include <memory>
#include "i_media_sink.h"

template <class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public WsSink, public IMediaSinkCallback
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    std::unique_ptr<WsClient> client;
    std::unique_ptr<WsHub>    hub;

    void on_ws_message(const std::string &m) override;
    // WsHub*   hub{nullptr};

private:
    Ui::MainWindow *ui;
    GlSpecViewport* glSpecView = nullptr;
    MainFrameMedia* mainFrameMedia = nullptr;
    void createMenu();
    void applySixtyPercent(QWidget* w, QScreen* s, bool resizeAndCenter);

private slots:
    void onComboBoxSignalChange(int index);

private:
    std::thread* fileThread = nullptr;
    void on_receive_media_audio(MediaObj::Audio audio) override;
    void on_receive_media(MediaObj::Vid vid, MediaObj::Audio audio) override {}
    void on_played_sec(double sec) override {};

private slots:
    void onOpenFile();
};
#endif // MAINWINDOW_H
