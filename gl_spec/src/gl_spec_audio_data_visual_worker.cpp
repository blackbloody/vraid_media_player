#include "gl_spec.h"
#include<QDebug>
#include <QThread>

#include "tools.h"

void GlSpecViewport::worker_loop() {
    while (!quit_) {
        MediaObj::Audio audio_obj;
        double start = 0.0;
        float viewport_sec_;
        float global_peak;
        int pWidthGlFrame;
        int target_sr_;
        std::vector<SpecViewPort> list_view_port;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&]{ return quit_ || has_job_; });
            if (quit_) break;

            audio_obj = this->audio_obj;
            start = job_start_;
            target_sr_ = this->target_sr_;
            viewport_sec_ = this->viewport_sec_;
            pWidthGlFrame = this->pWidthGlFrame;
            global_peak = global_peak_;
            list_view_port = this->list_view_port;

            has_job_ = false;
        }
        //qDebug() << "Extracting: " << std::to_string(job_start_).c_str();
        auto listSpecViewPortNdc = raiden::audio::project_visible_segments_to_ndc(list_view_port,
                                                                                  start, viewport_sec_);

        Signal audio;
        if (!is_video) {
            audio = raiden::audio::load(
                audio_obj.path, target_sr_, false, float(start), float(viewport_sec_));
        } else {
            AudioBufferU8 audio_buffer = ffmpeg_reader->media_load_audio_buffer(start, viewport_sec_);
            // qDebug() << std::to_string(audio_buffer.sample_rate).c_str();
            if (audio_buffer.sample_rate > 0) {
                audio = raiden::audio::loadBufferToWaveMono(audio_buffer.data, audio_buffer.sample_rate);
            }
        }
        // qWarning() << "empty audio"; continue;
        if (!audio.data.empty()) {
            if (target_sr_ <= 0 || n_fft <= 0 || n_hop <= 0) {
                qWarning() << "bad params sr/fft/hop:" << target_sr_ << n_fft << n_hop;
                continue;
            }
            if (int(audio.data.size()) < n_fft) {
                qWarning() << "clip shorter than n_fft; skipping. N=" << int(audio.data.size());
                continue;
            }
            SignalAdaptGl signAdaptGl;
            SpectrogramTileOverlap melSpec;

            switch (view_mode) {
            case ViewSignalDataMode::Mel_Spectrogram:
                melSpec = raiden::tools::loadMelOverlap(audio.data, target_sr_, n_fft, n_hop, 128);
                break;
            default:
            case ViewSignalDataMode::WaveForm:
                signAdaptGl = raiden::audio::project_visible_adapt_wave(audio.data, pWidthGlFrame, global_peak);
                break;
            }

            {
                std::lock_guard<std::mutex> lk(mtx_);
                switch (view_mode) {
                case ViewSignalDataMode::Mel_Spectrogram:
                    this->melSpec = std::move(melSpec);
                    break;
                default:
                case ViewSignalDataMode::WaveForm:
                    curr_wave_data.swap(audio.data);
                    list_view_port_ndc.swap(listSpecViewPortNdc);
                    currSignAdaptGl = std::move(signAdaptGl);
                    break;
                }
            }
            emit glUiKick();
        } else {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                has_job_ = false;
            }
        }
    }
}

void GlSpecViewport::on_new_audio() {
    ViewSignalDataMode view_mode;
    float start = 0.0;
    float viewport_sec = 0.0;
    std::vector<float> audio;
    SignalAdaptGl signAdaptGl;
    std::vector<SpecViewPortNDC> listSegmentWindowNDC;
    SpectrogramTileOverlap melSpec;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        start = static_cast<float>(job_start_);
        view_mode = this->view_mode;
        viewport_sec = static_cast<float>(viewport_sec_);
        audio = curr_wave_data;
        signAdaptGl = currSignAdaptGl;
        listSegmentWindowNDC = this->list_view_port_ndc;
        melSpec = this->melSpec;
    }

    switch(view_mode) {
    case ViewSignalDataMode::Mel_Spectrogram:
        // qDebug() << "Res: " << std::to_string(melSpec.height).c_str() << "x" << std::to_string(melSpec.width).c_str();
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, [this, start, viewport_sec, melSpec]{
                    gl_frame->setViewSpec(start, viewport_sec, melSpec);
                }, Qt::QueuedConnection);
        } else {
            gl_frame->setViewSpec(start, viewport_sec, melSpec);
        }
        break;
    default:
    case ViewSignalDataMode::WaveForm:
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, [this, start, viewport_sec, signAdaptGl]{
                    gl_frame->setViewWav(start, viewport_sec, signAdaptGl.gl_draw_count, signAdaptGl.adapt_audio_wave);
                }, Qt::QueuedConnection);
        } else {
            gl_frame->setViewWav(start, viewport_sec, signAdaptGl.gl_draw_count, signAdaptGl.adapt_audio_wave);
        }
        break;
    }
}
