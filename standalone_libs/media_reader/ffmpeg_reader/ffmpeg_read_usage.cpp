#include "ffmpeg_reader.h"

#include <iostream>
#include <cstring>

static SwsContext* create_sws_rgba(const AVCodecContext* vCtx) {
    return sws_getContext(
        vCtx->width, vCtx->height, vCtx->pix_fmt,
        vCtx->width, vCtx->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr, nullptr, nullptr
        );
}

static bool frame_to_rgba(AVFrame* src,
                          SwsContext* sws_rgba,
                          double pts_sec,
                          VideoFrameRGBA& out)
{
    const int w = src->width;
    const int h = src->height;

    out.width   = w;
    out.height  = h;
    out.pts_sec = pts_sec;
    out.pixels->resize(w * h * 4);

    uint8_t* dstData[4]     = { out.pixels->data(), nullptr, nullptr, nullptr };
    int      dstLinesize[4] = { w * 4, 0, 0, 0 };

    int ret = sws_scale(
        sws_rgba,
        src->data,
        src->linesize,
        0,
        h,
        dstData,
        dstLinesize
        );
    return (ret > 0);
}

// last frame with pts <= t_sec
static const VideoFrameRGBA* find_frame_for_time(
    const std::vector<VideoFrameRGBA>& frames,
    double t_sec)
{
    if (frames.empty()) return nullptr;

    if (t_sec <= frames.front().pts_sec) return &frames.front();
    if (t_sec >= frames.back().pts_sec)  return &frames.back();

    const VideoFrameRGBA* best = &frames.front();
    for (const auto& f : frames) {
        if (f.pts_sec <= t_sec)
            best = &f;
        else
            break;
    }
    return best;
}

static SwrContext* create_swr_mono_s16(const AVCodecContext* aCtx, int target_sr) {
    int64_t in_ch_layout = aCtx->channel_layout;
    int out_sr = (target_sr != 0) ? target_sr : aCtx->sample_rate;
    if (!in_ch_layout) {
        in_ch_layout = av_get_default_channel_layout(aCtx->channels);
    }

    SwrContext* swr = swr_alloc_set_opts(
        nullptr,
        AV_CH_LAYOUT_MONO,            // out layout
        AV_SAMPLE_FMT_S16,           // out fmt
        out_sr,           // out rate
        in_ch_layout,                // in layout
        aCtx->sample_fmt,            // in fmt
        aCtx->sample_rate,           // in rate
        0, nullptr
        );
    if (!swr) return nullptr;
    if (swr_init(swr) < 0) {
        swr_free(&swr);
        return nullptr;
    }
    return swr;
}

static void ffmpeg_global_init_once() {
    static std::once_flag once;
    std::call_once(once, []{
        // av_register_all(); // remove (obsolete). Do NOT call per instance.
        // avformat_network_init(); // only if you use network URLs
    });
}

bool FFMpegReader::media_init(const std::string& path, int target_sr) {

    //ffmpeg_global_init_once();
    media_destroy();

    // av_register_all(); // harmless on newer ffmpeg

    if (avformat_open_input(&g_fmt, path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input: " << path << "\n";
        return false;
    }
    if (avformat_find_stream_info(g_fmt, nullptr) < 0) {
        std::cerr << "Failed to find stream info\n";
        return false;
    }

    g_vIdx = av_find_best_stream(g_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    g_aIdx = av_find_best_stream(g_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (g_vIdx < 0 || g_aIdx < 0) {
        std::cerr << "Need both video and audio streams\n";
        return false;
    }

    AVStream* vSt = g_fmt->streams[g_vIdx];
    AVStream* aSt = g_fmt->streams[g_aIdx];

    // video codec
    {
        const AVCodec* vCodec = avcodec_find_decoder(vSt->codecpar->codec_id);
        if (!vCodec) {
            std::cerr << "Video decoder not found\n";
            return false;
        }
        g_vCtx = avcodec_alloc_context3(vCodec);
        avcodec_parameters_to_context(g_vCtx, vSt->codecpar);
        if (avcodec_open2(g_vCtx, vCodec, nullptr) < 0) {
            std::cerr << "Failed to open video codec\n";
            return false;
        }
    }

    // audio codec
    {
        const AVCodec* aCodec = avcodec_find_decoder(aSt->codecpar->codec_id);
        if (!aCodec) {
            std::cerr << "Audio decoder not found\n";
            return false;
        }
        g_aCtx = avcodec_alloc_context3(aCodec);
        avcodec_parameters_to_context(g_aCtx, aSt->codecpar);
        if (avcodec_open2(g_aCtx, aCodec, nullptr) < 0) {
            std::cerr << "Failed to open audio codec\n";
            return false;
        }
    }

    ///*
    g_swsRGBA    = create_sws_rgba(g_vCtx);
    g_swrMonoS16 = create_swr_mono_s16(g_aCtx, target_sr);
    if (!g_swsRGBA || !g_swrMonoS16) {
        std::cerr << "Failed to init sws/swr\n";
        return false;
    }
    //*/

    // ALSA
    const char* device = "default";
    int err = snd_pcm_open(&g_pcm, device, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "snd_pcm_open: " << snd_strerror(err) << "\n";
        return false;
    }

    unsigned int sample_rate = (target_sr != 0) ? target_sr : g_aCtx->sample_rate;
    // unsigned int sample_rate = g_aCtx->sample_rate;
    snd_pcm_format_t fmt = SND_PCM_FORMAT_S16_LE;
    unsigned int channels = 1;
    unsigned int latency_us = 200000; // 200ms

    err = snd_pcm_set_params(
        g_pcm,
        fmt,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        channels,
        sample_rate,
        1,
        latency_us
        );
    if (err < 0) {
        std::cerr << "snd_pcm_set_params: " << snd_strerror(err) << "\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(g_chunkMutex);
        g_chunk = AVChunk{};
    }

    g_playing      = false;
    g_audioTimeSec = 0.0;
    //g_frameCallback = nullptr;
    g_videoFps      = 30.0;

    g_init = true;
    return true;

}

bool FFMpegReader::decode_chunk_local(double start_sec, double duration_sec, std::vector<VideoFrameRGBA>& outVideo,
                                      AudioBufferU8& outAudio, bool audio_only, int target_sr) {

    outVideo.clear();
    outAudio.data.clear();
    outAudio.t0_sec      = start_sec;

    if (target_sr != 0) {
        outAudio.sample_rate = target_sr;
    } else {
        outAudio.sample_rate = g_aCtx->sample_rate;
    }

    outAudio.channels    = 1;

    const double end_sec = start_sec + duration_sec;

    int64_t ts = static_cast<int64_t>(start_sec * AV_TIME_BASE);
    if (av_seek_frame(g_fmt, -1, ts, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "av_seek_frame failed\n";
        return false;
    }
    avcodec_flush_buffers(g_vCtx);
    avcodec_flush_buffers(g_aCtx);

    AVPacket* pkt  = av_packet_alloc();
    AVFrame*  frame = av_frame_alloc();
    if (!pkt || !frame) {
        if (pkt)   av_packet_free(&pkt);
        if (frame) av_frame_free(&frame);
        return false;
    }

    AVStream* vSt = g_fmt->streams[g_vIdx];
    AVStream* aSt = g_fmt->streams[g_aIdx];

    bool video_done = false;
    bool audio_done = false;
    if (audio_only) {
        video_done = true;
    }

    std::vector<int16_t> audioSamples; // temp mono S16

    while (!video_done || !audio_done) {
        int ret = av_read_frame(g_fmt, pkt);
        if (ret < 0) {
            break;
        }
        if (pkt->stream_index == g_vIdx && !video_done) {
            // VIDEO
            if (avcodec_send_packet(g_vCtx, pkt) < 0) {
                av_packet_unref(pkt);
                break;
            }
            av_packet_unref(pkt);

            while (true) {
                ret = avcodec_receive_frame(g_vCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;

                double pts_sec =
                    frame->best_effort_timestamp * av_q2d(vSt->time_base);

                if (pts_sec + 1e-4 < start_sec)
                    continue;
                if (pts_sec >= end_sec) {
                    video_done = true;
                    continue;
                }

                VideoFrameRGBA vf;
                if (frame_to_rgba(frame, g_swsRGBA, pts_sec, vf)) {
                    outVideo.push_back(std::move(vf));
                }
            }
        } else if (pkt->stream_index == g_aIdx && !audio_done) {
            // AUDIO
            if (avcodec_send_packet(g_aCtx, pkt) < 0) {
                av_packet_unref(pkt);
                break;
            }
            av_packet_unref(pkt);

            while (true) {
                ret = avcodec_receive_frame(g_aCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0) goto done;

                double pts_sec =
                    frame->best_effort_timestamp * av_q2d(aSt->time_base);
                double frame_dur_sec =
                    frame->nb_samples / double(g_aCtx->sample_rate);

                if (pts_sec + frame_dur_sec < start_sec - 1e-4)
                    continue;
                if (pts_sec >= end_sec) {
                    audio_done = true;
                    continue;
                }

                const AVSampleFormat out_fmt = AV_SAMPLE_FMT_S16;
                const int out_bps = av_get_bytes_per_sample(out_fmt);
                int in_samples = frame->nb_samples;
                int in_sr  = g_aCtx->sample_rate;
                int out_sr = (target_sr != 0) ? target_sr : in_sr;
                int64_t delay = swr_get_delay(g_swrMonoS16, in_sr);

                int out_max_samples = av_rescale_rnd(
                    delay + in_samples,
                    out_sr,
                    in_sr,
                    AV_ROUND_UP
                    );
                int out_buf_size = av_samples_get_buffer_size(
                    nullptr, 1, out_max_samples, out_fmt, 1
                    );

                std::vector<int16_t> tmp(out_buf_size);
                ///*
                uint8_t* outData[1] = {
                    reinterpret_cast<uint8_t*>(tmp.data())
                };
                //*/
                //uint8_t* outData[1] = { tmp.data() };

                int got = swr_convert(
                    g_swrMonoS16,
                    outData,
                    out_max_samples,
                    const_cast<const uint8_t**>(frame->extended_data),
                    in_samples
                    );
                if (got < 0) goto done;

                size_t got_bytes = size_t(got) * 1 * out_bps;
                audioSamples.insert(audioSamples.end(),
                                    tmp.begin(),
                                    tmp.begin() + got);

                double have_sec = double(audioSamples.size()) / out_sr;
                // double have_sec = double(audioSamples.size()) / double(out_sr * 1 * out_bps);
                if (have_sec >= duration_sec + 0.1) {
                    audio_done = true;
                }
            }
        } else {
            av_packet_unref(pkt);
        }
        if (video_done && audio_done) break;

    }

done:
    av_frame_free(&frame);
    av_packet_free(&pkt);

    if (!audioSamples.empty()) {
        outAudio.data.resize(audioSamples.size() * sizeof(int16_t));
        std::memcpy(outAudio.data.data(), audioSamples.data(), outAudio.data.size());
        // std::memcpy(outAudio.data.data(), audioSamples.data(), audioSamples.size() * sizeof(int16_t));
    }

    // std::cout << "decoded\n";
    return !outVideo.empty() || !outAudio.data.empty();
}

bool FFMpegReader::media_load_chunk(double start_sec, double duration_sec) {
    std::vector<VideoFrameRGBA> localVideo;
    AudioBufferU8               localAudio;

    if (!decode_chunk_local(start_sec, duration_sec,
                            localVideo, localAudio)) {
        return false;
    }

    std::lock_guard<std::mutex> lk(g_chunkMutex);
    g_chunk.t0_sec  = start_sec;
    g_chunk.len_sec = duration_sec;
    g_chunk.video   = std::move(localVideo);
    g_chunk.audio   = std::move(localAudio);
    g_chunk.valid   = true;
    return true;
}

AudioBufferU8 FFMpegReader::media_load_audio_buffer(double start_sec, double duration_sec) {
    std::vector<VideoFrameRGBA> localVideo;
    AudioBufferU8 localAudio;

    if (!decode_chunk_local(start_sec, duration_sec, localVideo, localAudio, true, 22050)) {
        localAudio.sample_rate = 0;
        return localAudio;
    }

    return localAudio;
}

void FFMpegReader::playback_thread_func(const VideoFrameCallback& frame_callback,
                                 double fps) {

    AVChunk chunk = g_chunk;

    const AudioBufferU8& abuf = chunk.audio;
    const int    sampleRate   = abuf.sample_rate;
    const int    channels     = abuf.channels;
    const size_t bytesPerSmp  = sizeof(int16_t) * channels;
    const size_t totalFrames  = abuf.data.size() / bytesPerSmp;
    const uint8_t* audioPtr   = abuf.data.data();

    if (sampleRate <= 0 || totalFrames == 0) {
        g_playing = false;
        return;
    }

    ///*
    snd_pcm_prepare(g_pcm);

    using clock = std::chrono::steady_clock;
    auto wallStart = clock::now();

    const double frameInterval = (fps > 0.0) ? (1.0 / fps) : (1.0 / 30.0);
    double nextFrameTime = chunk.t0_sec;  // when next video frame should be shown

    size_t framesPlayed = 0;

    // play
    ///*
    while (g_playing && framesPlayed < totalFrames) {
        // audio time based on frames already sent
        double audioTime = abuf.t0_sec +
                           double(framesPlayed) / double(sampleRate);
        // g_audioTimeSec.store(audioTime, std::memory_order_relaxed);
        // std::cout << std::to_string(framesPlayed).c_str() << " " << std::to_string(totalFrames).c_str();
        if (frame_callback) {
            while (nextFrameTime <= audioTime + 1e-4 &&
                   nextFrameTime <= chunk.t0_sec + chunk.len_sec + 1e-4)
            {
                // std::cout << std::to_string(framesPlayed).c_str() << " " << std::to_string(totalFrames).c_str();
                const VideoFrameRGBA* f =
                    find_frame_for_time(chunk.video, nextFrameTime);
                if (f) {
                    const int channel = 4;
                    // play_sec = current audio time (what is being played *now*)
                    // frame_callback(f->width, f->height,
                    //                f->pixels->data(),
                    //                channel, false);
                }
                nextFrameTime += frameInterval;
            }
        }

        // write a small block of audio (~20 ms)
        const size_t framesLeft      = totalFrames - framesPlayed;
        const size_t framesPerWrite  = static_cast<size_t>(sampleRate / 50); // 20ms
        const size_t thisFrames      = std::min(framesLeft, framesPerWrite);

        snd_pcm_sframes_t n = snd_pcm_writei(
            g_pcm,
            audioPtr + framesPlayed * bytesPerSmp,
            thisFrames
            );

        if (n == -EPIPE) {
            snd_pcm_prepare(g_pcm);
            continue;
        } else if (n < 0) {
            std::cerr << "snd_pcm_writei error: " << snd_strerror(n) << "\n";
            break;
        } else {
            framesPlayed += static_cast<size_t>(n);
        }

        // optional small sleep to prevent busy loop if ALSA returns instantly
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    //*/
    snd_pcm_drain(g_pcm);
    g_playing = false;
    //*/
    // std::cout << "Done\n";
    frame_callback(0, 0, {}, 0, true/*, audioTime*/);
}

bool FFMpegReader::media_start_play(const VideoFrameCallback& frame_callback, double fps) {
    if (g_playing) return false;

    if (!g_chunk.valid) {
        std::cerr << "No chunk loaded\n";
        return false;
    }

    if (fps <= 0.0) fps = 30.0;
    //g_playThread = std::thread(&FFMpegReader::playback_thread_func, this, g_frameCallback, g_videoFps);
    g_playing = true;
    playback_thread_func(frame_callback, fps);
    //frame_callback(0, 0, {}, 0, true/*, audioTime*/);

    return true;
}

void FFMpegReader::media_destroy() {
    g_playing = false;
    // if (g_playThread.joinable())
    //     g_playThread.join();

    if (g_pcm) {
        snd_pcm_close(g_pcm);
        g_pcm = nullptr;
    }

    if (g_swsRGBA)   { sws_freeContext(g_swsRGBA);  g_swsRGBA = nullptr; }
    if (g_swrMonoS16){ swr_free(&g_swrMonoS16);     g_swrMonoS16 = nullptr; }

    if (g_vCtx) { avcodec_free_context(&g_vCtx); g_vCtx = nullptr; }
    if (g_aCtx) { avcodec_free_context(&g_aCtx); g_aCtx = nullptr; }

    if (g_fmt)  { avformat_close_input(&g_fmt); g_fmt = nullptr; }

    g_vIdx = g_aIdx = -1;

    {
        std::lock_guard<std::mutex> lk(g_chunkMutex);
        g_chunk = AVChunk{};
    }

    g_audioTimeSec = 0.0;
    //g_frameCallback = nullptr;
    g_videoFps      = 30.0;
    g_init = false;
}
