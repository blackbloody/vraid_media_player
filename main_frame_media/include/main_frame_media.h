#ifndef MAIN_FRAME_MEDIA_H
#define MAIN_FRAME_MEDIA_H
#pragma once

#include "audio_player.h"
#include "ffmpeg_reader.h"

#include <QPointer>
#include <QFrame>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>

#include "frame_media_gl.h"
#include "i_media_sink.h"
#include "wav_reader.h"

#include <thread>
#include <string>

class MainFrameMedia : public QFrame, public IFFMpegReaderCallback {
    Q_OBJECT
public:
    explicit MainFrameMedia(QWidget* parent=nullptr);
    ~MainFrameMedia() override;
    IMediaCallback& getMediaCallback() {
        return *media_callback;
    }
    void setMediaAnalyzerPath(const std::string& media_path);
private slots:
    void onPlayPauseMedia();
private:
    // GlFrameMedia* glFrameMedia = nullptr;
    QPointer<MainFrameMedia> self;
    QPointer<GlFrameMedia> glFrameMedia;
    QPointer<QSlider> mediaSlider;
    std::unique_ptr<IMediaCallback>    media_callback;

    std::mutex frame_mtx_;
    int pendingFrameW_ = 0, pendingFrameH_ = 0;
    std::shared_ptr<const std::vector<uint8_t>> pendingFramePix_;
    std::atomic<int> play_ms_{0};
    std::atomic_bool hasFramePending_{false};

    bool userScrubbing_ = false;
    bool isPlaying_ = false;
    std::string media_path;
    std::shared_ptr<FFMpegReader> ffmpeg_reader_player;
    audio_player::AudioThroughAccessPlayer audioPlayer;

    double play_sec = 0;
    MediaObj::Vid vid_obj;
    MediaObj::Audio audio_obj;
    std::thread* fileThread = nullptr;
    std::thread* mediaThread = nullptr;
    std::thread* decodeThread = nullptr;
    wav_reader::WavReader wav_reader = wav_reader::WavReader();
private:
    QPushButton *btnPlayPause;

private:
    static bool endsWith(const std::string& fullString, const std::string& ending) {
        // Check if the ending string is longer than the full string
        if (ending.size() > fullString.size()) {
            return false;
        }

        // Compare the ending of the full string with the target ending
        return fullString.compare(fullString.size() - ending.size(), ending.size(), ending) == 0;
    }
    void onFFMpegReaderThumbnail(std::string message, std::vector<uint8_t> thumbnail_frame) override;
};

#endif // MAIN_FRAME_MEDIA_H
