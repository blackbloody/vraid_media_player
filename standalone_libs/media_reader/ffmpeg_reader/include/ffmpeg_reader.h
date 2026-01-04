#ifndef FFMPEG_READER_H
#define FFMPEG_READER_H
#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <deque>
#include <unordered_set>
#include "media.h"

// ALSA
#include <alsa/asoundlib.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

struct VideoFrameRGBA {
    int width  = 0;
    int height = 0;
    double pts_sec = 0.0;              // absolute timestamp in seconds
    //std::vector<uint8_t> pixels;       // size = width * height * 4 (RGBA)
    std::shared_ptr<std::vector<uint8_t>> pixels; // shared <-- shared
};

struct AudioBufferU8 {
    double t0_sec       = 0.0;         // start time of audio buffer
    int    sample_rate  = 0;           // Hz
    int    channels     = 1;           // mono
    std::vector<uint8_t> data;         // raw PCM S16_LE, frames * 2 bytes
};

struct AVChunk {
    double t0_sec  = 0.0;              // chunk start time
    double len_sec = 0.0;              // chunk duration
    std::vector<VideoFrameRGBA> video;
    AudioBufferU8               audio;
    bool valid = false;
};

typedef std::function<void(const int& w,
                           const int& h,
                           const std::vector<uint8_t>& pixels,
                           const int& channel, bool done)> VideoFrameCallback;

typedef std::function<void(const int& w,
                           const int& h,
                           // const std::vector<uint8_t>& pixels,
                           std::shared_ptr<const std::vector<uint8_t>> pixels,
                           const int& channel, const double& play_sec, bool done)> PlayerCallback;

typedef std::function<void(const double& start_sec, const std::string& message)> DecodedCallback;

class IFFMpegReaderCallback {
public:
    virtual void onFFMpegReaderThumbnail(std::string message, std::vector<uint8_t> data) = 0;
};

class FFMpegReader {
public:
    FFMpegReader() = default;
    FFMpegReader(IFFMpegReaderCallback* thumnail_callback) {
        this->thumnail_callback = thumnail_callback;
    }
    ~FFMpegReader() { media_destroy(); }

    FFMpegReader(const FFMpegReader&) = delete;
    FFMpegReader& operator=(const FFMpegReader&) = delete;

    FFMpegReader(FFMpegReader&&) noexcept = default;
    FFMpegReader& operator=(FFMpegReader&&) noexcept = default;
public:
    static void read_media(const std::string& path, const std::function<void(const MediaObj::Vid& vid_obj, const MediaObj::Audio& audio_obj)>& media_callback);
    static double rational_to_double(AVRational r) {
        if (r.num != 0 && r.den != 0) return r.num / static_cast<double>(r.den);
        return 0.0;
    }

private:
    std::atomic<bool> g_init{false};
    IFFMpegReaderCallback* thumnail_callback = nullptr;

    static bool ffmpeg_get_random_frame_rgba(const std::string& path, std::vector<uint8_t>& out_rgba, int& out_w, int& out_h);
    static bool ffmpeg_get_frame_rgba_at(
        const std::string& path,
        double time_sec,
        std::vector<uint8_t>& out_rgba,
        int& out_w,
        int& out_h
        );

    bool decode_chunk_local(double start_sec, double duration_sec, std::vector<VideoFrameRGBA>& outVideo,
                            AudioBufferU8& outAudio, bool audio_only = false, int target_sr = 0);
    void playback_thread_func(const VideoFrameCallback& frame_callback,
                              double fps);

private:
    // ---- Globals ----

    AVFormatContext* g_fmt      = nullptr;
    AVCodecContext*  g_vCtx     = nullptr;
    AVCodecContext*  g_aCtx     = nullptr;
    int              g_vIdx     = -1;
    int              g_aIdx     = -1;
    SwsContext*      g_swsRGBA  = nullptr;
    SwrContext*      g_swrMonoS16 = nullptr;

    snd_pcm_t*       g_pcm      = nullptr;

    AVChunk          g_chunk;
    std::mutex       g_chunkMutex;

    std::thread      g_playThread;
    std::atomic<bool> g_playing{false};

    // optional: expose audio clock if you want it elsewhere
    std::atomic<double> g_audioTimeSec{0.0};

    // playback config
    double             g_videoFps = 30.0;
    VideoFrameCallback g_frameCallback;

    //////////////////////////////
    /// \brief new playback
    // ---- request queue (times to decode)
    std::thread dec_thread;
    std::thread play_thread;

    std::atomic<bool> quit_decode{false};
    std::unordered_set<long long> req_ids_;
    std::deque<double> req_times_;
    std::mutex req_mtx_;
    std::condition_variable req_cv_;

    // ---- chunk queue (decoded data)
    std::mutex q_mtx_;
    std::condition_variable q_cv_;
    std::deque<AVChunk> chunks_;

    std::mutex play_mtx_;
    std::condition_variable play_cv_;

    double ori_request     = 0.0;
    double next_req        = 0.0;
    double play_start_sec_ = 0.0;
    double play_end_sec_   = 0.0;
    double play_fps_       = 0.0; // 0 = use PTS
    double end_req_sec     = 0.0;

    bool is_continue_play = false;
    double end_req = 0;
    DecodedCallback decoded_callback;

public:
    // ---- Public API ----
    inline bool isInit() {
        return g_init;
    }

    // init all global context (FFmpeg + ALSA)
    bool media_init(const std::string& path, int target_sr = 0);

    // decode chunk [start_sec, start_sec + duration_sec)
    bool media_load_chunk(double start_sec, double duration_sec = 3.0);

    AudioBufferU8 media_load_audio_buffer(double start_sec, double duration_sec = 1.0);

    bool media_start_play(const VideoFrameCallback& frame_callback, double fps);

    // stop playback and destroy all globals
    void media_destroy();


    // new playback
    void request_chunk(const double& req_sec);
    void decode_chunk();
    void restart_decode();
    bool media_pre_load_chunk(double start_sec);
    void media_load_chunk_playback(const PlayerCallback& frame_callback, const DecodedCallback& decoded_callback,
                                   double fps, double start_sec, double duration_sec = 1.0);
    bool stop_playback(double restart_load = -1);

private:
    const double chunk_len_ = 0.500;

    int queued_chunks();
    int pending_requests();
    void fill_backlog(double& next_req_local);

    void play_loop(const double& start_sec, const int& fps, const PlayerCallback& frame_callback);

    bool peek_chunk(size_t index, AVChunk& out);
    bool pop_chunk(AVChunk& out);
    bool peek_last_chunk(AVChunk& out);
};

#endif // FFMPEG_READER_H
