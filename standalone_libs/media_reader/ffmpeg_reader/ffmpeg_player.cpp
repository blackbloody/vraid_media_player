#include "ffmpeg_reader.h"
#include <algorithm>
#include <iostream>

static SwsContext* create_sws_rgba(const AVCodecContext* vCtx) {
    return sws_getContext(
        vCtx->width, vCtx->height, vCtx->pix_fmt,
        vCtx->width, vCtx->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr, nullptr, nullptr
        );
}

static void reset_swr(SwrContext* swr)
{
    if (!swr) return;

    // Works on older FFmpeg: clears internal delay/buffer
    swr_close(swr);

    int ret = swr_init(swr); // re-init with the same options already set
    if (ret < 0) {
        // If this fails, your SwrContext wasn't configured correctly.
        // In that case, free+recreate the SwrContext.
    }
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

    std::vector<uint8_t> buf(w*h*4);
    out.pixels = std::make_shared<std::vector<uint8_t>>();

    uint8_t* dstData[4]     = { buf.data(), nullptr, nullptr, nullptr };
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

    out.pixels->swap(buf);

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

///////////////////////////////////////////////////////////////////////////////////////////////
/// \brief FFMpegReader::request_chunk
/// \param start_sec
/// \param duration_sec
///

static long long chunk_id(double t0) {
    return (long long) llround(t0 * 1000000.0);
}

void FFMpegReader::request_chunk(const double& req_sec) {
    long long id = chunk_id(req_sec);

    if (req_sec >= end_req) {
        if (decoded_callback)
            decoded_callback(req_sec, "REQ_IGN_end_req");
        else
            thumnail_callback->onFFMpegReaderThumbnail("REQ_IGN_end_req: " + std::to_string(req_sec) + " ~ " + std::to_string(end_req), {});
        return;
    }

    std::lock_guard<std::mutex> lk(req_mtx_);
    if (!req_ids_.insert(id).second) {
        if (decoded_callback)
            decoded_callback(req_sec, "REQ_IGN_dup");
        else
            thumnail_callback->onFFMpegReaderThumbnail("REQ_IGN_dup", {});
        return;
    }

    req_times_.push_back(req_sec);
    if (decoded_callback)
        decoded_callback((double)req_times_.size(), "REQ_OK_size");
    else
        thumnail_callback->onFFMpegReaderThumbnail("REQ_OK_size", {});
    req_cv_.notify_all();
}

void FFMpegReader::decode_chunk() {

    double start_req_sec = ori_request;
    int target_sr = g_aCtx->sample_rate;

    avcodec_flush_buffers(g_vCtx);
    avcodec_flush_buffers(g_aCtx);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) {
        if (pkt) av_packet_free(&pkt);
        if (frame) av_frame_free(&frame);
        return;
    }

    AVStream* vSt = g_fmt->streams[g_vIdx];
    AVStream* aSt = g_fmt->streams[g_aIdx];

    const int64_t want_frames = (int64_t) llround(chunk_len_ * target_sr);
    const int64_t want_samples = want_frames * 1;  // int16_t count (interleaved)
    // decoded_callback(0, "start");

    int ret;
    while (!quit_decode.load(std::memory_order_acquire)) {
        int size_req = 0;
        double t0 = 0.0;
        // decoded_callback(0, "begin");
        // std::cout << "aaa\n";
        // thumnail_callback->onFFMpegReaderThumbnail("aaa", {});
        {
            std::unique_lock<std::mutex> lk(req_mtx_);
            // thumnail_callback->onFFMpegReaderThumbnail("aaa1" + std::to_string(req_times_.size()), {});
            req_cv_.wait(lk, [&]{ return quit_decode.load(std::memory_order_acquire) || !req_times_.empty(); });
            if (quit_decode.load(std::memory_order_acquire)) break;
            start_req_sec = ori_request;
            t0 = req_times_.front();
            req_times_.pop_front();
            size_req = req_times_.size();

            // lk.unlock();
        }
        // thumnail_callback->onFFMpegReaderThumbnail("bbb", {});
        // decoded_callback(size_req, "process");

        std::vector<VideoFrameRGBA> outFrame;
        AudioBufferU8 outAudio;
        bool video_done = false;
        bool audio_done = false;
        double end_sec = t0 + chunk_len_;
        double duration_sec = chunk_len_;

        // decoded_callback(end_sec, std::to_string(t0));

        int64_t seek_ts = av_rescale_q(
            (int64_t)(t0 * AV_TIME_BASE),
            AV_TIME_BASE_Q,
            vSt->time_base);

        int ret_seek = av_seek_frame(g_fmt, g_vIdx, seek_ts, AVSEEK_FLAG_BACKWARD);
        if (ret_seek < 0) {
            if (decoded_callback)
                decoded_callback(t0, "seek_fail");
            continue; // go to next request
        }

        avformat_flush(g_fmt);
        avcodec_flush_buffers(g_vCtx);
        avcodec_flush_buffers(g_aCtx);
        reset_swr(g_swrMonoS16);

        std::vector<int16_t> audioSamples;
        while ((!video_done || !audio_done) && !quit_decode.load(std::memory_order_acquire)) {
            ret = av_read_frame(g_fmt, pkt);
            if (ret < 0) {
                video_done = true;
                break;
            }

            if (pkt->stream_index == g_vIdx && !video_done) {
                if (avcodec_send_packet(g_vCtx, pkt) < 0) {
                    av_packet_unref(pkt);
                    video_done = true;
                    break;
                }

                while (true) {
                    ret = avcodec_receive_frame(g_vCtx, frame);
                    if (ret == AVERROR(EAGAIN))
                        break;
                    if (ret == AVERROR_EOF) {
                        video_done = true;
                        break;
                    }
                    if (ret < 0) {
                        video_done = true;
                        break;
                    }

                    double pts_sec =
                        frame->best_effort_timestamp * av_q2d(vSt->time_base);
                    // decoded_callback(pts_sec, "got video pkt");

                    if (pts_sec + 1e-4 < t0)
                        continue;
                    if (pts_sec >= end_sec) {
                        video_done = true;
                        break;
                    }

                    VideoFrameRGBA vf;
                    if (frame_to_rgba(frame, g_swsRGBA, pts_sec, vf)) {
                        outFrame.push_back(std::move(vf));
                    }
                }
            } else if (pkt->stream_index == g_aIdx && !audio_done) {
                if (avcodec_send_packet(g_aCtx, pkt) < 0) {
                    av_packet_unref(pkt);
                    audio_done = true;
                    break;
                }

                while (true) {
                    ret = avcodec_receive_frame(g_aCtx, frame);
                    if (ret == AVERROR(EAGAIN))
                        break;
                    if (ret == AVERROR_EOF) {
                        audio_done = true;
                        break;
                    }
                    if (ret < 0) {
                        audio_done = true;
                        break;
                    }

                    double pts_sec =
                        frame->best_effort_timestamp * av_q2d(aSt->time_base);
                    double frame_dur_sec =
                        frame->nb_samples / double(g_aCtx->sample_rate);
                    // decoded_callback(pts_sec, "got audio pkt");

                    if (pts_sec + frame_dur_sec < t0 - 1e-4)
                        continue;
                    if (pts_sec >= end_sec) {
                        audio_done = true;
                        continue;
                    }

                    const AVSampleFormat out_fmt = AV_SAMPLE_FMT_S16;
                    const int out_bps = av_get_bytes_per_sample(out_fmt);
                    int in_samples = frame->nb_samples;
                    int in_sr  = g_aCtx->sample_rate;
                    int out_sr = target_sr;

                    int64_t delay = swr_get_delay(g_swrMonoS16, in_sr);
                    int out_max_samples = (int)av_rescale_rnd(delay + in_samples, out_sr, in_sr, AV_ROUND_UP);

                    // bytes for 1 channel s16
                    int out_buf_bytes = av_samples_get_buffer_size(nullptr, 1, out_max_samples, out_fmt, 1);

                    // allocate int16 count, not bytes
                    // std::vector<int16_t> tmp(out_buf_bytes / out_bps);
                    std::vector<int16_t> tmp(out_max_samples);

                    uint8_t* outData[1] = { (uint8_t*)tmp.data() };

                    int got = swr_convert(
                        g_swrMonoS16,
                        outData,
                        out_max_samples,
                        (const uint8_t**)frame->extended_data,
                        in_samples
                        );

                    if (got > 0) {
                        /*
                        audioSamples.insert(audioSamples.end(), tmp.begin(), tmp.begin() + got);
                        if ((int64_t)audioSamples.size() >= want_samples)
                            audio_done = true;
                        */
                        // drop samples that occur before t0 (so chunk starts exactly at t0)
                        int drop = 0;
                        if (pts_sec < t0) {
                            drop = (int)llround((t0 - pts_sec) * out_sr);
                            if (drop < 0) drop = 0;
                            if (drop > got) drop = got;
                        }

                        int keep = got - drop;
                        if (keep > 0) {
                            int64_t remaining = want_samples - (int64_t)audioSamples.size();
                            if (remaining > 0) {
                                int take = (int)std::min<int64_t>(remaining, keep);

                                audioSamples.insert(audioSamples.end(),
                                                    tmp.begin() + drop,
                                                    tmp.begin() + drop + take);
                            }
                        }

                        if ((int64_t)audioSamples.size() >= want_samples)
                            audio_done = true;
                    }
                }
            }

            av_packet_unref(pkt);   // <-- here, every loop
        }
        std::sort(outFrame.begin(), outFrame.end(),
                  [](const VideoFrameRGBA& a, const VideoFrameRGBA& b){
                      return a.pts_sec < b.pts_sec;
                  });

        AVChunk chunk;
        chunk.t0_sec  = t0;
        chunk.len_sec = chunk_len_;
        if (t0 == start_req_sec) {
            thumnail_callback->onFFMpegReaderThumbnail("decode 1st load", *outFrame.at(0).pixels);
        }

        chunk.video   = std::move(outFrame);

        if (!audioSamples.empty()) {
            if ((int64_t)audioSamples.size() > want_samples) {
                audioSamples.resize((size_t)want_samples);
            } else if ((int64_t)audioSamples.size() < want_samples) {
                audioSamples.insert(audioSamples.end(),
                                    (size_t)(want_samples - (int64_t)audioSamples.size()),
                                    0);
            }

            outAudio.t0_sec      = t0;
            outAudio.sample_rate = target_sr;
            outAudio.channels    = 1;

            outAudio.data.resize(audioSamples.size() * sizeof(int16_t));
            memcpy(outAudio.data.data(), audioSamples.data(), outAudio.data.size());

            chunk.audio = std::move(outAudio);
        }
        chunk.valid   = !chunk.video.empty() && !chunk.audio.data.empty();

        if (decoded_callback) {
            decoded_callback(chunk.audio.data.size(), "got audio chunk");
            decoded_callback(chunk.video.size(), "got video chunk");
        }

        if (chunk.valid) {
            {
                std::unique_lock<std::mutex> lk(q_mtx_);
                chunks_.push_back(std::move(chunk));
            }
            q_cv_.notify_all();

            {
                std::lock_guard<std::mutex> lk(req_mtx_);
                req_ids_.erase(chunk_id(t0));
            }

        } else {
            if (decoded_callback)
                decoded_callback(t0, "fail");
        }
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);

    if (decoded_callback)
        decoded_callback(0, "end decode");
}

bool FFMpegReader::media_pre_load_chunk(double start_sec) {

    if (!dec_thread.joinable())
        dec_thread = std::thread(&FFMpegReader::decode_chunk, this);


    quit_decode.store(false, std::memory_order_release);
    ori_request = start_sec;
    chunks_.clear();
    req_times_.clear();

    end_req = start_sec + (chunk_len_ * 5);

    // thumnail_callback->onFFMpegReaderThumbnail("test blank", {});
    // request_chunk(start_sec);
    next_req = start_sec;
    for (int i = 0; i < 4; i++) {
        request_chunk(next_req);
        next_req += chunk_len_;
    }



    AVChunk out;
    if (peek_chunk(0, out)) {
        std::vector<uint8_t> thumbnail_frame = std::move(*out.video.at(0).pixels);
        thumnail_callback->onFFMpegReaderThumbnail("pre load", thumbnail_frame);
        return true;
    } else {
        return false;
    }
}

void FFMpegReader::media_load_chunk_playback(const PlayerCallback& frame_callback, const DecodedCallback& decoded_callback,
                                             double fps, double start_sec, double duration_sec) {

    if (g_playing.load(std::memory_order_acquire)) return;

    g_playing.store(true, std::memory_order_release);
    if (quit_decode.load(std::memory_order_acquire))
        quit_decode.store(false, std::memory_order_release);

    bool need_clear_request = true;
    if (start_sec == ori_request) {
        AVChunk out;
        if (peek_chunk(0, out) && out.t0_sec == start_sec) {
            need_clear_request = false;
        }
    } else {
        ori_request = start_sec;
    }
    if (need_clear_request) {
        chunks_.clear();
        req_times_.clear();

        this->decoded_callback = decoded_callback;
        next_req = start_sec;
    }
    end_req = start_sec + duration_sec;

    if (this->decoded_callback) {
        decoded_callback(end_req, "end play");
    }

    // request_chunk(next_req);
    is_continue_play = false;
    fill_backlog(next_req);
    play_thread = std::thread(&FFMpegReader::play_loop, this, start_sec, fps , frame_callback);
}

bool FFMpegReader::stop_playback(double restart_load) {

    // std::cout << "destroy\n";
    if (is_continue_play) {
        is_continue_play = false;
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(req_mtx_);
        quit_decode = true;
    }
    req_cv_.notify_all();
    if (dec_thread.joinable()) dec_thread.join();
    req_times_.clear();

    {
        std::lock_guard<std::mutex> lk(play_mtx_);
        g_playing = false;
    }
    q_cv_.notify_all();
    play_cv_.notify_all();
    if (play_thread.joinable()) play_thread.join();
    chunks_.clear();

    if (g_pcm) snd_pcm_drop(g_pcm);

    if (restart_load >= 0) {
        is_continue_play = true;
        media_pre_load_chunk(restart_load);
    }

    return true;
}

static double audio_played_sec(snd_pcm_t* pcm,
                               double audio_t0,
                               int sr,
                               int64_t total_written_frames)
{
    if (!pcm || sr <= 0) return audio_t0;

    snd_pcm_sframes_t delay = 0;
    int err = snd_pcm_delay(pcm, &delay);
    if (err < 0) {
        err = snd_pcm_recover(pcm, err, 1);
        if (err < 0) return audio_t0;
        if (snd_pcm_delay(pcm, &delay) < 0) return audio_t0;
    }

    if (delay < 0) delay = 0; // can happen on some setups
    int64_t played_frames = total_written_frames - (int64_t)delay;

    if (played_frames < 0) played_frames = 0;
    if (played_frames > total_written_frames) played_frames = total_written_frames;

    return audio_t0 + double(played_frames) / double(sr);
}

int FFMpegReader::queued_chunks() {
    std::lock_guard<std::mutex> lk(q_mtx_);
    return (int)chunks_.size();
}

int FFMpegReader::pending_requests() {
    std::lock_guard<std::mutex> lk(req_mtx_);
    return (int)req_times_.size();
}

void FFMpegReader::fill_backlog(double& next_req_local) {
    // keep about 2 seconds buffered (minimum 6 chunks)
    const int target = std::max(6, (int)std::ceil(2.0 / chunk_len_));

    while (g_playing.load(std::memory_order_acquire)) {
        int backlog = queued_chunks() + pending_requests();
        if (backlog >= target) break;

        request_chunk(next_req_local);
        next_req_local += chunk_len_;

        if (next_req_local >= end_req) break;
    }
}

void FFMpegReader::play_loop(const double& start_sec, const int& fps, const PlayerCallback& frame_callback)
{
    snd_pcm_prepare(g_pcm);

    // const double frameInterval = (fps > 0) ? (1.0 / fps) : (1.0 / 30.0);
    const double lead_sec = 0.006;   // show slightly early
    const int prefetch = 2;

    bool have_audio_base = false;
    double audio_t0 = 0.0;
    int64_t total_written_frames = 0;

    double next_req_local = next_req;
    double end_sec = end_req;

    fill_backlog(next_req_local);
    double current_play = start_sec;

    // int64_t end_ts = sec_to_ts(end_sec, video_stream->time_base);
    snd_pcm_nonblock(g_pcm, 1);              // optional but helps avoid blocking

    while (g_playing.load(std::memory_order_acquire)) {

        fill_backlog(next_req_local);

        if ((current_play+chunk_len_) >= end_sec) {
            break;
        }

        AVChunk ck;
        if (!pop_chunk(ck)) {
            decoded_callback(audio_t0, "need chunk");
            break;
        }

        if (!ck.valid) {
            decoded_callback(audio_t0, "need chunk");
            continue;
        }

        fill_backlog(next_req_local);

        const auto& abuf = ck.audio;
        const int sr = abuf.sample_rate;
        const int ch = abuf.channels;
        const size_t bytesPerFrame = sizeof(int16_t) * (size_t)ch;

        const uint8_t* audioPtr = abuf.data.data();
        const size_t totalFrames = abuf.data.size() / bytesPerFrame;

        if (!have_audio_base) {
            audio_t0 = abuf.t0_sec;
            total_written_frames = 0;          // IMPORTANT: reset relative to base
            have_audio_base = true;
        }

        size_t vid_i = 0;
        while (vid_i < ck.video.size() && ck.video[vid_i].pts_sec < ck.t0_sec - 1e-4)
            ++vid_i;

        double last_presented_pts = -1.0;
        size_t audio_i = 0; // <-- AUDIO frame index (NOT video)

        // before loop (once)
        auto next_tick = std::chrono::steady_clock::now();
        const auto tick = std::chrono::milliseconds(1);

        const double chunk_end_sec  = ck.t0_sec + ck.len_sec;
        const double video_last_pts = ck.video.empty() ? ck.t0_sec : ck.video.back().pts_sec;
        const double target_end_sec = std::max(chunk_end_sec, video_last_pts);

        auto pcm_drained_or_stopped = [&]() -> bool {
            snd_pcm_state_t st = snd_pcm_state(g_pcm);

            // If it’s not actually running, do NOT wait forever.
            // (This covers PREPARED/SETUP/PAUSED/XRUN/etc.)
            if (st != SND_PCM_STATE_RUNNING) {
                // XRUN is "dead" until you prepare; but at chunk tail we just want to exit.
                return true;
            }

            snd_pcm_sframes_t delay = 0;
            if (snd_pcm_delay(g_pcm, &delay) < 0) return true; // can’t query -> don’t hang
            return delay <= 0;
        };

        // progress watchdog (prevents infinite loop if ALSA time doesn't move)
        double last_played_sec = -1.0;
        auto   last_progress   = std::chrono::steady_clock::now();

        while (g_playing.load(std::memory_order_acquire)) {

            double played_sec = audio_played_sec(g_pcm, audio_t0, sr, total_written_frames);

            // watchdog update
            auto now = std::chrono::steady_clock::now();
            if (last_played_sec < 0.0 || played_sec > last_played_sec + 1e-4) {
                last_played_sec = played_sec;
                last_progress   = now;
            }

            // If we've already submitted all audio for this chunk, but ALSA time isn't moving,
            // force-finish the chunk instead of hanging.
            if (audio_i >= totalFrames) {
                if (now - last_progress > std::chrono::milliseconds(250)) {
                    // force flush remaining video frames
                    while (vid_i < ck.video.size()) {
                        const auto& f = ck.video[vid_i++];
                        if (f.pixels) frame_callback(f.width, f.height, f.pixels, 4, played_sec, false);
                    }
                    break;
                }
            }

            double present_until = played_sec + lead_sec;

            // If audio is finished and PCM is drained/stopped, flush video tail
            if (audio_i >= totalFrames && pcm_drained_or_stopped()) {
                present_until = std::max(present_until, video_last_pts + lead_sec);
            }

            // ---- VIDEO ----
            const VideoFrameRGBA* last = nullptr;
            while (vid_i < ck.video.size() && ck.video[vid_i].pts_sec <= present_until) {
                last = &ck.video[vid_i];
                ++vid_i;
            }
            if (last && last->pixels &&
                (last_presented_pts < 0.0 || std::fabs(last->pts_sec - last_presented_pts) > 1e-6) && g_playing.load(std::memory_order_acquire)) {
                frame_callback(last->width, last->height, last->pixels, 4, played_sec, false);
                last_presented_pts = last->pts_sec;
            }
            if (!g_playing.load(std::memory_order_acquire)) break;

            // ---- AUDIO ----
            if (audio_i < totalFrames) {
                snd_pcm_sframes_t avail = snd_pcm_avail_update(g_pcm);
                if (avail < 0) {
                    avail = snd_pcm_recover(g_pcm, (int)avail, 1);
                    if (avail < 0) break;
                }

                const size_t framesLeft = totalFrames - audio_i;
                const snd_pcm_sframes_t cap =
                    (snd_pcm_sframes_t)std::max<int>(1, (int)llround(sr * 0.010)); // ~10ms
                snd_pcm_sframes_t canWrite = std::min<snd_pcm_sframes_t>(avail, cap);

                if (canWrite > 0) {
                    snd_pcm_sframes_t n = snd_pcm_writei(
                        g_pcm,
                        audioPtr + audio_i * bytesPerFrame,
                        (snd_pcm_uframes_t)std::min<size_t>((size_t)canWrite, framesLeft)
                        );

                    if (n == -EAGAIN) {
                        // try later
                    } else if (n == -EPIPE) {
                        snd_pcm_prepare(g_pcm);
                        audio_t0 = ck.t0_sec + (double)audio_i / (double)sr;
                        total_written_frames = 0;
                    } else if (n < 0) {
                        n = snd_pcm_recover(g_pcm, (int)n, 1);
                        if (n < 0) break;
                    } else {
                        audio_i += (size_t)n;
                        total_written_frames += (int64_t)n;
                    }
                } else {
                    // snd_pcm_wait(g_pcm, 3);
                    std::this_thread::sleep_for(std::chrono::milliseconds(3));
                }
            } else {
                // snd_pcm_wait(g_pcm, 3);
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }

            // ---- DONE for this chunk (no hang) ----
            if (audio_i >= totalFrames && vid_i >= ck.video.size()) {
                // Don’t require delay==0 forever. If ALSA is stopped/non-running, exit too.
                if (pcm_drained_or_stopped() || played_sec >= target_end_sec - 1e-3) {
                    break;
                }
            }

            /*
            std::this_thread::sleep_until(next_tick);
            */
            // pacing clamp (keep yours)
            // next_tick += tick;
            //if (next_tick < now) next_tick = now;
            std::unique_lock<std::mutex> lk(q_mtx_);
            q_cv_.wait_until(lk, next_tick, [&]{
                return !g_playing.load(std::memory_order_acquire);
            });
            if (!g_playing.load(std::memory_order_acquire)) break;
        }

        current_play = ck.t0_sec;

    }

    // IMPORTANT: drain blocks. If stopping, drop instead.
    if (!g_playing.load(std::memory_order_acquire)) {
        snd_pcm_drop(g_pcm);   // stop immediately, don't block
        snd_pcm_prepare(g_pcm);
    } else {
        snd_pcm_drain(g_pcm);
    }

    frame_callback(0, 0, {}, 0, 0.0, true);
}

bool FFMpegReader::pop_chunk(AVChunk& out) {
    std::unique_lock<std::mutex> lk(q_mtx_);

    q_cv_.wait(lk, [&] {
        return !g_playing.load(std::memory_order_acquire) || !chunks_.empty();
    });
    if (!g_playing.load(std::memory_order_acquire)) return false;
    if (chunks_.empty()) return false;

    out = std::move(chunks_.front());
    chunks_.pop_front();
    return true;
}

bool FFMpegReader::peek_last_chunk(AVChunk& out) {
    std::unique_lock<std::mutex> lk(q_mtx_);

    // wait until deque is non-empty
    q_cv_.wait(lk, [this] { return !chunks_.empty(); });

    // at this point chunks_ is guaranteed non-empty
    out = chunks_.back();   // copy snapshot
    return true;
}

bool FFMpegReader::peek_chunk(size_t index, AVChunk& out) {
    std::lock_guard<std::mutex> lk(q_mtx_);
    if (index >= chunks_.size())
        return false;
    out = chunks_[index];
    return true;
}


