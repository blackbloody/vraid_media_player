#include "i_media_callback.h"
#include "ffmpeg_reader.h"
#include "main_frame_media.h"

#include <QtUiTools/QUiLoader>
#include <QFile>

#include <QDebug>
#include <QPushButton>
#include <QStyle>

// Only if you're stuck with C++11
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

MainFrameMedia::MainFrameMedia(QWidget* parent) : QFrame(parent), media_callback(new IMediaCallback()) {
    setFrameShape(QFrame::NoFrame);
    setLineWidth(0);
    setMidLineWidth(0);

    ffmpeg_reader_player = make_unique<FFMpegReader>(this);

    // 1) Make a root layout for this widget, zero margins
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->setSpacing(0);

    QUiLoader loader;
    QFile f("resources/layout/main_frame_media.ui");
    if (!f.open(QFile::ReadOnly)) { qWarning("cannot open main_frame_media.ui"); return; }
    QWidget *panel = loader.load(&f, this);
    f.close();

    outer->addWidget(panel);

    mediaSlider = panel->findChild<QSlider*>("mediaSlider");
    btnPlayPause = panel->findChild<QPushButton*>("btn_play_pause");
    btnPlayPause->setCheckable(true);
    btnPlayPause->setText(QString());
    btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    btnPlayPause->setIconSize(QSize(24, 24));

    mediaSlider->setTracking(true);
    connect(mediaSlider, &QSlider::sliderPressed, this, [this] {
        userScrubbing_ = true;   // bool member
    });
    connect(mediaSlider, &QSlider::sliderMoved, this, [this](int v) {
        // v is in ms if you set it that way
        double previewSec = v / 1000.0;
        qDebug() << std::to_string(previewSec).c_str();
        // update preview label / tooltip here (GUI thread)
        // ui->lblPreview->setText(formatTime(previewSec));
    });
    connect(mediaSlider, &QSlider::sliderReleased, this, [this] {
        userScrubbing_ = false;
        if (!mediaSlider) return;

        double targetSec = mediaSlider->value() / 1000.0;
        // qDebug() << std::to_string(targetSec).c_str();

        if (ffmpeg_reader_player) {
            play_sec = targetSec;
            ffmpeg_reader_player->media_pre_load_chunk(targetSec);
        }
    });

    connect(btnPlayPause, &QPushButton::clicked, this, &MainFrameMedia::onPlayPauseMedia);

    if (auto *parent_of_gl = panel->findChild<QVBoxLayout*>("parent_of_gl")) {
        parent_of_gl->setContentsMargins(0,0,0,0);
        parent_of_gl->setSpacing(0);
        self = this;
        glFrameMedia = new GlFrameMedia(panel);
        glFrameMedia->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        parent_of_gl->addWidget(glFrameMedia, 1);
    }
}

MainFrameMedia::~MainFrameMedia() {

    if (mediaThread) {
        if (mediaThread->joinable()) mediaThread->join();
        delete mediaThread;
        mediaThread = nullptr;
    }

    if (fileThread) {
        if (fileThread->joinable()) fileThread->join();
        delete fileThread;
        fileThread = nullptr;
    }

    if (decodeThread) {
        if (decodeThread->joinable())
            decodeThread->join();
        delete decodeThread;
        decodeThread = nullptr;
    }

    if (ffmpeg_reader_player)
        ffmpeg_reader_player->stop_playback();
    else
        qDebug() << "not detroy";

}

void MainFrameMedia::onFFMpegReaderThumbnail(std::string message, std::vector<uint8_t> thumbnail_frame) {
    QMetaObject::invokeMethod(this, [this, message, thumbnail_frame] {
            // now on GUI thread
            qDebug() << message.c_str();
            if (!thumbnail_frame.empty())
                glFrameMedia->updateText(vid_obj.width, vid_obj.height, thumbnail_frame);
        }, Qt::QueuedConnection);
}

void MainFrameMedia::setMediaAnalyzerPath(const std::string& media_path) {

    if (fileThread) {
        if (fileThread->joinable()) fileThread->join();
        delete fileThread;
        fileThread = nullptr;
    }

    fileThread = new std::thread([this, media_path] {

        if (endsWith(media_path, ".wav")) {
            wav_reader.readFile(media_path, [this, media_path](const MediaObj::Audio& audio_obj){

                this->media_path = media_path;
                this->audio_obj = audio_obj;
                QMetaObject::invokeMethod(this, [this, audio_obj] {
                        // now on GUI thread
                        if (fileThread->joinable()) {
                            fileThread->join();

                            delete fileThread;
                            fileThread = nullptr;
                        }
                        media_callback->publish_audio(audio_obj);

                    }, Qt::QueuedConnection);

            });
        } else {
            FFMpegReader::read_media(media_path, [this, media_path](const MediaObj::Vid& vid_obj, const MediaObj::Audio& audio_obj) {
                // qDebug() << "Receive: " << std::to_string(audio_obj.sample_rate).c_str();
                // qDebug() << "Receive: " << (vid_obj.codec_name).c_str();
                this->media_path = media_path;
                this->vid_obj = vid_obj;
                this->audio_obj = audio_obj;

                double duration_sec_ = this->audio_obj.num_sample() / this->audio_obj.sample_rate;
                const qint64 durMs64 = qRound64(duration_sec_ * 1000.0);

                const int durMs = (durMs64 > std::numeric_limits<int>::max())
                                      ? std::numeric_limits<int>::max()
                                      : int(durMs64);

                ffmpeg_reader_player->media_init(this->media_path, 0);
                media_callback->publish_media(vid_obj, audio_obj);
                //ffmpeg_reader_player->media_pre_load_chunk(play_sec);
                QMetaObject::invokeMethod(this, [this, vid_obj, audio_obj, durMs] {
                        // now on GUI thread
                        qDebug() << "Done Media: " << std::to_string(vid_obj.thumnail.size()).c_str();
                        ffmpeg_reader_player->media_pre_load_chunk(play_sec);
                        ///*
                        if (fileThread->joinable()) {
                            fileThread->join();

                            delete fileThread;
                            fileThread = nullptr;
                        }
                        //*/
                        glFrameMedia->updateText(vid_obj.width, vid_obj.height, vid_obj.thumnail);
                        if (mediaSlider) {
                            mediaSlider->setRange(0, durMs);
                            mediaSlider->setSingleStep(100);   // 0.1s arrow keys
                            mediaSlider->setPageStep(5000);    // 5s PgUp/PgDn
                        }
                        /*
                        decodeThread = new std::thread([this] {
                           ffmpeg_reader_player->media_pre_load_chunk(play_sec);
                        });
                        */

                    }, Qt::QueuedConnection);
            });
        }
    });
}

void MainFrameMedia::onPlayPauseMedia() {
    if (media_path.empty())  {
        btnPlayPause->toggled(false);
        return;
    }

    if (endsWith(media_path, ".wav")) {
        if (!isPlaying_) {
            isPlaying_ = true;
            btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));

            mediaThread = new std::thread([this]{

                double duration = this->audio_obj.num_sample() / this->audio_obj.sample_rate;

                audioPlayer.onPlayWavForTimeStamp(
                    audio_obj, 0, duration, [this](const int &lengthSample, const float& sec_played) {
                        if (sec_played == 0.0) {
                            QMetaObject::invokeMethod(this, [this] {
                                    // now on GUI thread
                                    if (mediaThread->joinable()) {
                                        mediaThread->join();

                                        delete mediaThread;
                                        mediaThread = nullptr;
                                    }
                                    isPlaying_ = false;
                                    btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

                                }, Qt::QueuedConnection);
                        }
                        else { // still playing
                            media_callback->update_played_audio(sec_played);
                            //qDebug() << "Seek: " << std::to_string(sec_played).c_str();
                        }
                    }
                    );
            });

        } else {
            isPlaying_ = false;
            btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
            audioPlayer.onPause();
        }
    } else {

        if (!isPlaying_) {
            isPlaying_ = true;
            btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));

            mediaThread = new std::thread([this] {

                double duration = this->audio_obj.num_sample() / this->audio_obj.sample_rate;
                ffmpeg_reader_player->media_load_chunk_playback(
                    [this](int w, int h, std::shared_ptr<const std::vector<uint8_t>> pixels, int channel,
                           const double& play_sec, bool done){

                        if (done) {
                            qDebug() << "End";
                            QMetaObject::invokeMethod(this, [this] {
                                    // now on GUI thread
                                    if (mediaThread->joinable()) {
                                        mediaThread->join();

                                        delete mediaThread;
                                        mediaThread = nullptr;
                                    }
                                    isPlaying_ = false;
                                    ffmpeg_reader_player->stop_playback();
                                    btnPlayPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

                                }, Qt::QueuedConnection);
                            return;
                        }
                        //media_callback->update_played_audio(play_sec);

                        {
                            std::lock_guard<std::mutex> lk(frame_mtx_);
                            pendingFrameW_ = w;
                            pendingFrameH_ = h;
                            play_ms_.store(int(play_sec * 1000.0), std::memory_order_relaxed);
                            pendingFramePix_ = std::move(pixels);
                        }

                        if (!hasFramePending_.exchange(true)) {
                            // start update
                            media_callback->update_played_audio(play_sec);
                            // qDebug() << "Update: " << std::to_string(play_sec).c_str();
                            int w, h;
                            std::shared_ptr<const std::vector<uint8_t>> pix;
                            {
                                std::lock_guard<std::mutex> lk(frame_mtx_);
                                w = pendingFrameW_;
                                h = pendingFrameH_;
                                pix = pendingFramePix_;
                            }

                            if (pix) {
                                if (!glFrameMedia) return;

                                QMetaObject::invokeMethod(this, [this, w, h, pix] {
                                        glFrameMedia->submitFrame(w, h, std::move(pix));

                                        if (!mediaSlider || mediaSlider->isSliderDown()) return;
                                        QSignalBlocker b(*mediaSlider);

                                        this->play_sec = play_ms_.load(std::memory_order_relaxed) / 1000.0;
                                        mediaSlider->setValue(play_ms_.load(std::memory_order_relaxed));
                                    }, Qt::QueuedConnection);
                            }
                            hasFramePending_.store(false);
                        }

                }, [this](const double& start_sec, const std::string& message) {
                        /*
                        if (message.length() > 0) {
                            qDebug() << message.c_str() << ": " << std::to_string(start_sec).c_str();
                        } else {
                            qDebug() << "Decoded time: " << std::to_string(start_sec).c_str();
                        }
                        */
                },vid_obj.preferred_rate, play_sec, duration);
                // },vid_obj.preferred_rate, play_sec, 30*60);

            });

        } else {
            qDebug() << std::to_string(this->play_sec).c_str();
            // play_sec = play_ms_.load(std::memory_order_relaxed) / 1000.0;
            ffmpeg_reader_player->stop_playback(this->play_sec);
            // ffmpeg_reader_player->media_pre_load_chunk(this->play_sec);
        }

        //btnPlayPause->toggled(false);
    }
}
