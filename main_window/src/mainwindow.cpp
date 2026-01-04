#include "ws_hub.h"
#include "ws_client.h"
#include "i_media_callback.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFile>
#include <QTimer>

#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QStandardPaths>
#include <QFileInfo>

#include "media.h"
#include <QComboBox>
//media_callback(new IMediaCallback()),
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) , ui(new Ui::MainWindow),
    hub(new WsHub()),
    client(make_unique<WsClient>(/*hub, */"ws://127.0.0.1:8100/ws?user_id=fakrul_dev"))
{
    ui->setupUi(this);
    client->setHub(*hub.get());
    hub->register_sink(this);

    // media_callback->register_sink(this);

    createMenu();
    setStatusBar(nullptr);

    mainFrameMedia = new MainFrameMedia(this);
    mainFrameMedia->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->main_view->addWidget(mainFrameMedia, 1);

    mainFrameMedia->getMediaCallback().register_sink(this);

    int default_signal_view_mode = 1;
    ui->signalViewMode->clear();
    ui->signalViewMode->addItem("Waveform", int(ViewSignalDataMode::WaveForm));
    ui->signalViewMode->addItem("Mel Spectrogram", int(ViewSignalDataMode::Mel_Spectrogram));
    connect(ui->signalViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onComboBoxSignalChange);
    ui->signalViewMode->setCurrentIndex(default_signal_view_mode);

    glSpecView = new GlSpecViewport(this);
    glSpecView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->signal_view->addWidget(glSpecView, 1);
    glSpecView->setSignalViewMode(static_cast<ViewSignalDataMode>(default_signal_view_mode), false);

    mainFrameMedia->getMediaCallback().register_sink(glSpecView);

    applySixtyPercent(this, QGuiApplication::primaryScreen(), true);
    if (auto* w = ui->signal_view->parentWidget()) {
        w->setMinimumHeight(240);
    }

    glSpecView->set_ws_client(client.get());
    hub->register_sink(glSpecView);
    ui->split_main->setChildrenCollapsible(false);
    QTimer::singleShot(0, this, [this]{
        const int h = ui->split_main->height();
        ui->split_main->setSizes({ int(h * 0.6), h - int(h * 0.6) });
    });
    client->connect();

    //readFile("/media/virus/Goblin/NON_SPLIT_DATA/1/validating/mixture/mixture.wav");
    // readFile("/media/virus/Goblin/import-data/vocal_/JVASS/angry/fujitou_angry_001.wav");
    // mainFrameMedia->setMediaAnalyzerPath("/media/virus/Goblin/NON_SPLIT_DATA/1/validating/mixture/mixture.wav");

    mainFrameMedia->setMediaAnalyzerPath("/media/virus/White/pikpakcli/b1/330077.xyz LULU-179.mp4");
    // mainFrameMedia->setMediaAnalyzerPath("/media/virus/Goblin/cawd-676-4k_000.mp4");
    // mainFrameMedia->setMediaAnalyzerPath("/home/virus/Downloads/[Sav1or] Toaru Kagaku no Railgun (A Certain Scientific Railgun) - S02E14 [1080p][AV1][OPUS][Dual Audio].mp4");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_receive_media_audio(MediaObj::Audio audio) {

}

void MainWindow::on_ws_message(const std::string &m) {
    // qDebug() << "MainWindow:" << m.c_str();
}

void MainWindow::onComboBoxSignalChange(int index) {
    //qDebug() << std::to_string(index).c_str();
    if (glSpecView) {
        auto m = static_cast<ViewSignalDataMode>(index);
        glSpecView->setSignalViewMode(m, true);
    }
}

void MainWindow::createMenu() {

    QMenuBar *mb = menuBar();

    // File
    QMenu *file = mb->addMenu("File");
    QAction *open = file->addAction("New");
    open->setShortcuts(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &MainWindow::onOpenFile);

    file->addSeparator();
    file->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

}

void MainWindow::onOpenFile() {

    static QString lastDir = QDir::homePath();

    const QString filter =
        "Media (*.wav *.flac *.mp3 *.ogg *.opus *.m4a *.aac "
        "*.mp4 *.mkv *.avi *.mov *.webm *.mpeg *.mpg);;"
        "Audio (*.wav *.flac *.mp3 *.ogg *.opus *.m4a *.aac);;"
        "Video (*.mp4 *.mkv *.avi *.mov *.webm *.mpeg *.mpg);;"
        "All Files (*)";

    const QString path = QFileDialog::getOpenFileName(this, tr("Open Media"), lastDir, filter);
    if (path.isEmpty()) return;

    lastDir = QFileInfo(path).absolutePath();
    mainFrameMedia->setMediaAnalyzerPath(path.toStdString());
}

void MainWindow::applySixtyPercent(QWidget* w, QScreen* s, bool resizeAndCenter)
{
    if (!s) s = QGuiApplication::primaryScreen();
    const QRect r = s->availableGeometry();     // excludes taskbar/dock
    const double f = 0.80;
    const int minW = int(r.width()  * f);
    const int minH = int(r.height() * f);

    w->setMinimumSize(minW, minH);

    if (resizeAndCenter) {
        w->resize(minW, minH);
        w->move(r.center() - QPoint(minW/2, minH/2));
    } else {
        // if user shrank it below new minimum (e.g., moved to smaller screen)
        if (w->width() < minW || w->height() < minH)
            w->resize(std::max(w->width(),  minW),
                      std::max(w->height(), minH));
    }
}
