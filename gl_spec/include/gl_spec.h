#ifndef GL_SPEC_H
#define GL_SPEC_H

#pragma once
#include <QFrame>
#include <QVBoxLayout>

#include <QScrollBar>
#include "gl_spec_frame.h"

#include "media.h"
#include "librosa.h"
#include "ffmpeg_reader.h"

#include <thread>
#include <atomic>
#include <condition_variable>

#include "label_track.h"
#include "ws_sink.h"
#include "i_media_sink.h"

struct audio_label {
    double start, end;
    std::string label;
};

struct audio_ui_label {
    audio_label label;
    SpanItem* span;
};

class GlSpecViewport : public QFrame, public WsSink, public IMediaSinkCallback {
    Q_OBJECT
public:
    explicit GlSpecViewport(QWidget* parent=nullptr);
    ~GlSpecViewport() override;
    void setSignalViewMode(const ViewSignalDataMode& view_mode, const bool& isChange);
private:
    QScrollBar* scroll_bar = nullptr;
    GlSpecViewFrame* gl_frame = nullptr;
    LabelTrackView* labelsView = nullptr;
private:
    ViewSignalDataMode view_mode = ViewSignalDataMode::WaveForm;
    std::vector<float> curr_wave_data;
    SignalAdaptGl currSignAdaptGl;
    double duration_sec_ = 0.0;
    double viewport_sec_ = 5.0;
    const int target_sr_ = 22050; // render SR
    float global_peak_ = 1.0f;    // set to 1 if your loader returns [-1,1]
    int pWidthGlFrame;

    int n_hop = 256;
    // int n_fft = 2048;
    int n_fft = 1024;
    std::vector<audio_label> list_label;
    std::vector<audio_label> list_usage_label;
    std::vector<audio_ui_label> list_ui_label;
private:
    void setScrollX(int px);
    int chooseScale(double duration_sec, int preferred = 1000 /*ms*/);
    void setupTimeScrollbar(QScrollBar* bar,
                            double duration_sec,
                            double viewport_sec, double step_sec);

// analyzer
private:
    // audio visualizer data
    bool has_job_ = false;
    double job_start_ = 0.0;
    std::thread worker_;
    std::atomic<bool> quit_{false};
    std::mutex mtx_;
    std::condition_variable cv_;

    // audio label data
    bool has_job_lbl_ = false;
    double job_start_lbl_ = 0.0;
    std::thread worker_lbl_;
    std::atomic<bool> quit_lbl_{false};
    std::mutex mtx_lbl_;
    std::condition_variable cv_lbl_;

    // audio label dl data
    bool has_job_lbl_dl_ = false;
    double job_start_dl_ = 0.0;
    std::thread worker_dl_;
    std::atomic<bool> quit_dl_{false};
    std::mutex mtx_dl_;
    std::condition_variable cv_dl_;

    std::mutex mtx_ws_dl_;

    void request_window(float start=0.0);
    void worker_loop();
    void worker_audio_lbl_loop();
    void worker_audio_dl_loop();
signals:
    void glUiKick();
    void labelsUiKick();
    void dlToMainThreadKick();
private slots:
    void on_new_audio();
    void on_receive_audio_label();

private:
    // FFMpegReader ffmpeg_reader;
    std::shared_ptr<FFMpegReader> ffmpeg_reader;
    std::shared_ptr<FFMpegReader> ffmpeg_reader_dl;
    MediaObj::Audio audio_obj;
    bool is_video = false;
    std::vector<SpecViewPort> list_view_port;
    std::vector<SpecViewPortNDC> list_view_port_ndc;
    SpectrogramTileOverlap melSpec;

    WsClient* client = nullptr;
    void on_ws_message(const std::string &m) override;
    void on_receive_media_audio(MediaObj::Audio audio) override;
    void on_receive_media(MediaObj::Vid vid, MediaObj::Audio audio) override;
    void on_played_sec(double sec) override;
public:
     void set_ws_client(WsClient* client);
};


////////////////////////////////////////////////////////////

#include <QWheelEvent>
class TimeWheelFilter : public QObject {
    Q_OBJECT
public:
    explicit TimeWheelFilter(QScrollBar* bar) : QObject(bar), bar(bar) {}
protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        if (obj == bar && e->type() == QEvent::Wheel) {
            auto* we = static_cast<QWheelEvent*>(e);
            int steps = 0;
            if (!we->pixelDelta().isNull()) {
                steps = we->pixelDelta().y() / 120;  // touchpads
            } else {
                steps = we->angleDelta().y() / 120;  // mouse wheel notches
            }
            const int stepTicks = std::max(1, bar->property("timeStepTicks").toInt());
            bar->setValue(bar->value() + steps * stepTicks);
            return true; // consume, don’t let Qt apply the *3
        }
        return QObject::eventFilter(obj, e);
    }
private:
    QScrollBar* bar;
};

#endif // GL_SPEC_H
