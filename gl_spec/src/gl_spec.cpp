#include "ws_client.h"
#include "gl_spec.h"
#include <QSurfaceFormat>

#include <QtUiTools/QUiLoader>
#include <QThread>
#include <QFile>

#include <QTimer>

#include <QDebug>

#include <algorithm>
#include <climits>
#include <cmath>

#include <string>
#include <QString>

#include "upload_spec_api.h"
#include "tools.h"
#include <nlohmann/json.hpp>

// Only if you're stuck with C++11
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

GlSpecViewport::GlSpecViewport(QWidget* parent) : QFrame(parent) {
    setFrameShape(QFrame::NoFrame);
    setLineWidth(0);
    setMidLineWidth(0);

    ffmpeg_reader = make_unique<FFMpegReader>();
    ffmpeg_reader_dl = make_unique<FFMpegReader>();

    // 1) Make a root layout for this widget, zero margins
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->setSpacing(0);

    // 2) Load the .ui and put it inside the outer layout
    QUiLoader loader;
    QFile f("resources/layout/gl_spec_parent.ui");
    if (!f.open(QFile::ReadOnly)) { qWarning("cannot open gl_spec_parent.ui"); return; }
    QWidget *panel = loader.load(&f, this);
    f.close();

    outer->addWidget(panel);               // <-- this removes the outer gap


    // 3) Zero margins/spacing on the inner layout that will hold the GL view
    if (auto *parent_of_gl = panel->findChild<QVBoxLayout*>("parent_of_gl")) {

        parent_of_gl->setContentsMargins(0,0,0,0);
        parent_of_gl->setSpacing(0);
        gl_frame = new GlSpecViewFrame(panel);
        gl_frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        parent_of_gl->addWidget(gl_frame, 1);

        labelsView = new LabelTrackView(panel);
        labelsView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        labelsView->setMaximumHeight(80);
        parent_of_gl->addWidget(labelsView, 1);
        labelsView->setStartSec(job_start_, viewport_sec_);

        if (auto *frame = this->findChild<QFrame*>("frame")) {
            scroll_bar = frame->findChild<QScrollBar*>("scroll_for_gl");
            auto* filter = new TimeWheelFilter(scroll_bar);
            scroll_bar->installEventFilter(filter);

            setupTimeScrollbar(scroll_bar, 10.0, 5.0, 0.5);

            connect(scroll_bar, &QScrollBar::valueChanged, this, &GlSpecViewport::setScrollX);

            // audio data > visual worker
            connect(this, &GlSpecViewport::glUiKick, this, &GlSpecViewport::on_new_audio,
                    Qt::QueuedConnection);
            worker_ = std::thread(&GlSpecViewport::worker_loop, this);

            // audio data label > label worker
            connect(this, &GlSpecViewport::labelsUiKick, this, &GlSpecViewport::on_receive_audio_label,
                    Qt::QueuedConnection);
            worker_lbl_ = std::thread(&GlSpecViewport::worker_audio_lbl_loop, this);

            // audio data label > deep learning worker
            worker_dl_ = std::thread(&GlSpecViewport::worker_audio_dl_loop, this);
        }
    }
}

GlSpecViewport::~GlSpecViewport() {
    quit_ = true;
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();

    quit_lbl_ = true;
    cv_lbl_.notify_all();
    if (worker_lbl_.joinable()) worker_lbl_.join();

    quit_dl_ = true;
    cv_dl_.notify_all();
    if (worker_dl_.joinable()) worker_dl_.join();
}

void GlSpecViewport::set_ws_client(WsClient* client) {
    this->client = client;
}

void GlSpecViewport::on_receive_media_audio(MediaObj::Audio audio) {
    this->audio_obj = audio;
    this->is_video = false;
    /*
    list_label.clear();
    list_label.push_back({0.0, 1.5, "Silent"});
    list_label.push_back({2.0, 2.5, "Snoring"});
    list_label.push_back({2.5, 2.8, "Silent"});
    list_label.push_back({5.5, 6.9, "Silent"});
    list_label.push_back({5.0, 5.4, "Silent"});
    list_label.push_back({7.0, 8.4, "Silents"});
    */

    this->list_view_port = raiden::audio::build_global_segments(
        this->audio_obj.num_sample(), 22050, 0.5, 0.5, n_fft, n_hop
        );

    duration_sec_ = this->audio_obj.num_sample() / this->audio_obj.sample_rate;

    QMetaObject::invokeMethod(this, [this] {
            // now on GUI thread
            qDebug() << "Done read file spec: " << this->audio_obj.path.c_str() << " " << std::to_string(this->audio_obj.sample_rate).c_str() << " ~ "
                     << std::to_string(duration_sec_).c_str();
            setupTimeScrollbar(scroll_bar, duration_sec_, 5.0, 0.5);
            request_window(0.0);

            {
                std::lock_guard<std::mutex> lk(mtx_dl_);
                job_start_dl_ = 0;
                has_job_lbl_dl_ = true;
            }
            cv_dl_.notify_one();

        }, Qt::QueuedConnection);
}

void GlSpecViewport::on_receive_media(MediaObj::Vid vid, MediaObj::Audio audio) {

    this->is_video = true;
    this->audio_obj = audio;
    int target_sr_;
    {
        std::unique_lock<std::mutex> lk(mtx_);
        target_sr_ = this->target_sr_;
    }
    ffmpeg_reader->media_init(vid.path, target_sr_);
    ffmpeg_reader_dl->media_init(vid.path, target_sr_);

    // qDebug() << std::to_string(this->audio_obj.num_sample()).c_str();
    // qDebug() << std::to_string(this->audio_obj.sample_rate).c_str();
    duration_sec_ = this->audio_obj.num_sample() / this->audio_obj.sample_rate;

    ///*
    QMetaObject::invokeMethod(this, [this] {
            // now on GUI thread
            qDebug() << "Done read file spec: " << this->audio_obj.path.c_str() << " " << std::to_string(this->audio_obj.sample_rate).c_str() << " ~ "
                     << std::to_string(duration_sec_).c_str();
            setupTimeScrollbar(scroll_bar, duration_sec_, 5.0, 0.5);
            request_window(0.0);

            ///*
            {
                std::lock_guard<std::mutex> lk(mtx_dl_);
                job_start_dl_ = 0;
                has_job_lbl_dl_ = true;
            }
            cv_dl_.notify_one();
            //*/

        }, Qt::QueuedConnection);
    //*/

}

void GlSpecViewport::on_played_sec(double sec) {
    // qDebug() << "asss:" << std::to_string(sec).c_str();
    ///*
    double current_sec = 0;
    double viewport_sec_ = 0;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        current_sec = job_start_;
        viewport_sec_ = this->viewport_sec_;
    }
    double end = current_sec + viewport_sec_;
    if (sec < current_sec || sec > end) {

        int px_sec = static_cast<int>(sec);
        qDebug() << "Rec: " << std::to_string(sec).c_str() << " ~ " << std::to_string(px_sec).c_str();

        QMetaObject::invokeMethod(this, [this, sec, px_sec, viewport_sec_] {
                // now on GUI thread
                scroll_bar->setValue(px_sec);
                request_window(px_sec);
                labelsView->setStartSec(px_sec, viewport_sec_);

            }, Qt::QueuedConnection);
    }


}

void GlSpecViewport::on_ws_message(const std::string &m) {

    std::vector<audio_label> list_current_label;
    {
        std::unique_lock<std::mutex> lk(mtx_ws_dl_);
        list_current_label = this->list_label;
    }

    // qDebug() << "GlSpecViewport:" << m.c_str();
    auto j = nlohmann::json::parse(m);
    if (j.is_discarded()) {
        return;
    }
    if (j.contains("op") && j["op"].is_string() && j["op"] == "job_done") {
        auto list_j = j["result"];
        for (std::size_t i = 0; i < list_j.size(); ++i) {
            const auto& obj = list_j[i];
            double start = obj.value("start", 0.0);
            double end   = obj.value("end",   0.0);
            double num   = obj.value("num",   0.0);
            std::string value = obj.value("value", "");

            // qDebug() << "dl: " << std::to_string(i).c_str() << ": " << std::to_string(start).c_str() << " = " << value.c_str();

            list_current_label.push_back({ start, end, value });
        }
        {
            std::unique_lock<std::mutex> lk(mtx_ws_dl_);
            this->list_label = list_current_label;
        }
        {
            std::lock_guard<std::mutex> lk(mtx_);
            has_job_lbl_ = true;
        }
        cv_lbl_.notify_one();


        ////
        /// restart dl
        // std::cout << std::to_string(job_start_dl_).c_str();
        double start = 0.0;
        double duration_sec = 0;
        {
            std::lock_guard<std::mutex> lk(mtx_dl_);
            start = job_start_dl_;
            duration_sec = this->duration_sec_;
            if (start < duration_sec) {
                // std::cout << "v dl:" << std::to_string(job_start_dl_).c_str();
                has_job_lbl_dl_ = true;
            }
        }
        if (start < duration_sec) {
            // std::cout << "v dl:" << std::to_string(job_start_dl_).c_str();
            cv_dl_.notify_one();
        }
    }

}

void GlSpecViewport::worker_audio_dl_loop() {
    const double viewport_sec = 5.0;
    while(!quit_dl_) {
        MediaObj::Audio audio_obj;
        double start = 0.0;
        {
            std::unique_lock<std::mutex> lk(mtx_dl_);
            cv_dl_.wait(lk, [&]{ return quit_dl_ || has_job_lbl_dl_; });
            if (quit_dl_) break;

            audio_obj = this->audio_obj;
            start = job_start_dl_;
            // qDebug() << "s:" << std::to_string(start).c_str();

            has_job_lbl_dl_ = false;
        }
        double remaining = duration_sec_ - start;
        double duration_viewport_sec = std::min(viewport_sec, remaining);

        Signal audio;
        if (!is_video) {
            audio = raiden::audio::load(
                audio_obj.path, target_sr_, false, float(start), duration_viewport_sec);
        } else {
            AudioBufferU8 audio_buffer = ffmpeg_reader_dl->media_load_audio_buffer(start, duration_viewport_sec);
            if (audio_buffer.sample_rate > 0) {
                audio = raiden::audio::loadBufferToWaveMono(audio_buffer.data, audio_buffer.sample_rate);
            }
        }
        if (!audio.data.empty()) {
            SpectrogramTileOverlap melSpec = raiden::tools::loadMelOverlap(audio.data, target_sr_, n_fft, n_hop, 128, 0.0f, -1.0f, 0.5f, 0.0, false);
            //qDebug() << "Mel:" << std::to_string(melSpec.height).c_str() << "x" << std::to_string(melSpec.width).c_str();

            if (melSpec.height > 0 && melSpec.width > 0) {
                SpectrogramByte byteImg = raiden::tools::flatMatrixToByteImg(melSpec.spectrogram.data,
                                                                              melSpec.spectrogram.height,
                                                                              melSpec.spectrogram.width,
                                                                              "_current_whole_" + std::to_string(10), true);

                SpecApi::uploadSpectrogramPNG("http://192.168.1.131/raid/api/spec", byteImg.data, byteImg.width,
                                                  byteImg.height, "fakrul_dev", "spec.png");

                if (client) {
                    client->send_with_time(start, viewport_sec);
                }
                start += viewport_sec;
                //qDebug() << "c2 viewprt dl: " << std::to_string(duration_viewport_sec).c_str() << "~" << std::to_string(start).c_str() << "-" <<
                //    std::to_string(remaining).c_str();
            }
        }

        {
            std::lock_guard<std::mutex> lk(mtx_);
            job_start_dl_ = start;
        }
    }
}

void GlSpecViewport::setSignalViewMode(const ViewSignalDataMode& view_mode, const bool& isChange) {

    switch(view_mode) {
    case ViewSignalDataMode::Mel_Spectrogram:
        //viewport_sec_ = 5.0;
        break;
    case ViewSignalDataMode::WaveForm:
        //viewport_sec_ = 10.0;
        break;
    }

    this->view_mode = view_mode;
    if (isChange) {
        request_window(job_start_);
    }
}

void GlSpecViewport::setScrollX(int px) {
    const int scale     = std::max(1, scroll_bar->property("timeScale").toInt());
    const int stepTicks = std::max(1, scroll_bar->property("timeStepTicks").toInt());

    // snap to nearest step boundary
    const int snapped = (px / stepTicks) * stepTicks;
    if (snapped != px) {
        const QSignalBlocker b(*scroll_bar);
        scroll_bar->setValue(snapped);
        //return;
    }
    const double startSec = double(snapped) / double(scale);
    // gl_frame->setViewStartSeconds(startSec);

    labelsView->setStartSec(startSec, viewport_sec_);
    request_window(startSec);
}


////////////////////////////////////////////////////////////////////
/// START SETUP SCROLL BAR
////////////////////////////////////////////////////////////////////

// Choose ticks-per-second that won’t overflow QScrollBar (int range)
int GlSpecViewport::chooseScale(double duration_sec, int preferred/*ms*/) {
    if (duration_sec <= 0) return preferred;
    const double maxTicks = double(INT_MAX) - 1;
    const double maxScale = maxTicks / duration_sec;
    return int(std::max(1.0, std::min<double>(preferred, maxScale)));
}

// Call this whenever duration/viewport/W changes
void GlSpecViewport::setupTimeScrollbar(QScrollBar* bar,
                                            double duration_sec,
                                            double viewport_sec, double step_sec)
{
    if (!bar) return;
    const int scale     = chooseScale(duration_sec);        // ms ticks
    const int total     = int(std::lround(duration_sec * scale));
    const int viewTicks = int(std::lround(std::min(viewport_sec, duration_sec) * scale));
    const int stepTicks = std::max(1, int(std::lround(std::max(step_sec, 0.001) * scale)));
    const int maxV      = std::max(0, total - std::max(viewTicks, stepTicks));

    const QSignalBlocker b(*bar);
    bar->setMinimum(0);
    bar->setMaximum(maxV);
    bar->setPageStep(std::max(viewTicks, stepTicks));             // page = viewport (or >= step)
    bar->setSingleStep(stepTicks);                                // arrow/wheel = fixed time step
    bar->setValue(std::min(bar->value(), maxV));

    bar->setTracking(true);
    bar->setProperty("timeScale",    scale);
    bar->setProperty("timeStepTicks", stepTicks);
}


////////////////////////////////////////////////////////////////////
/// END SETUP SCROLL BAR
////////////////////////////////////////////////////////////////////
///

void GlSpecViewport::request_window(float start) {

    const double max_start = std::max(0.0, duration_sec_ - viewport_sec_);
    if (start < 0.0) start = 0.0;
    if (start > max_start) start = max_start;

    //qDebug() << "Start: " << std::to_string(start).c_str();

    ///*
    {
        std::lock_guard<std::mutex> lk(mtx_);
        job_start_ = start;
        pWidthGlFrame = gl_frame->viewportWidthPx();
        has_job_ = true; // coalesce rapid changes
    }
    cv_.notify_one();
    //*/

    ///*
    {
        std::unique_lock<std::mutex> lk(mtx_lbl_);
        job_start_lbl_ = start;
        has_job_lbl_ = true;
    }
    cv_lbl_.notify_one();
    //*/
}


//////////////////////////

void GlSpecViewport::worker_audio_lbl_loop() {
    while(!quit_lbl_) {
        std::vector<audio_label> list_label;
        double start = 0.0;
        double viewport_sec_;
        {
            std::unique_lock<std::mutex> lk(mtx_lbl_);
            cv_lbl_.wait(lk, [&]{return quit_lbl_ || has_job_lbl_;});
            if (quit_lbl_) break;

            start = job_start_lbl_;
            viewport_sec_ = this->viewport_sec_;
            list_label = this->list_label;
            has_job_lbl_ = false;
        }

        std::vector<audio_label> list_usage_label_import;
        //list_usage_label_import = list_label;
        const double end = start + viewport_sec_;

        list_usage_label_import.reserve(list_label.size());
        for (const auto& a : list_label) {
            // qDebug() << "lbl: " << std::to_string(a.start).c_str() << " = " << a.label.c_str();
            if (a.end > start && a.start < end) {
                list_usage_label_import.push_back(a);
            }
        }

        {
            std::lock_guard<std::mutex> lk(mtx_);
            this->list_usage_label.swap(list_usage_label_import);
        }
        emit labelsUiKick();
    }
}

void GlSpecViewport::on_receive_audio_label() {
    std::vector<audio_label> list_usage_label;
    {
        std::unique_lock<std::mutex> lk(mtx_lbl_);
        list_usage_label = this->list_usage_label;
    }

    for (auto& l : list_ui_label) {
        delete l.span;
    }
    list_ui_label.clear();

    for (int i = 0; i < list_usage_label.size(); i++) {
        audio_label a = list_usage_label[i];
        list_ui_label.push_back({a, labelsView->addSpan(a.start, a.end, QString::fromStdString(a.label))});
    }
}











