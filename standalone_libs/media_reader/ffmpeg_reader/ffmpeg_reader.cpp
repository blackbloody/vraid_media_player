#include "ffmpeg_reader.h"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavutil/display.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <chrono>
#include <random>
#include <cmath>

void FFMpegReader::read_media(const std::string& path, const std::function<void(const MediaObj::Vid& vid_obj, const MediaObj::Audio& audio_obj)>& media_callback) {

    AVFormatContext* fmt = nullptr;

    // Reduce FFmpeg logging noise
    av_log_set_level(AV_LOG_ERROR);

    // --- Open file ---
    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "FFMpegReader: cannot open file: " << path << "\n";
        return;
    }

    // --- Read stream metadata ---
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        std::cerr << "FFMpegReader: cannot read stream info\n";
        avformat_close_input(&fmt);
        return;
    }

    ///////////////////////////////////////////////////////////
    /// \brief vid_stream_index
    ///
    MediaObj::Vid vid_obj = {};

    int vid_stream_index = av_find_best_stream(
        fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream* st_vid = fmt->streams[vid_stream_index];
    AVCodecParameters* cp_vid = st_vid->codecpar;
    AVRational fr = av_guess_frame_rate(fmt, st_vid, nullptr);

    vid_obj.path = path;
    vid_obj.width = cp_vid->width;
    vid_obj.height = cp_vid->height;
    vid_obj.codec_type = cp_vid->codec_type;
    vid_obj.codec_name = avcodec_get_name(cp_vid->codec_id);

    // Frame rates
    AVRational fr_guess = av_guess_frame_rate(fmt, st_vid, nullptr); // effective playback
    AVRational fr_avg   = st_vid->avg_frame_rate;                     // encoded rate

    double fps_guess = rational_to_double(fr_guess);
    double fps_avg   = rational_to_double(fr_avg);

    vid_obj.playback_frame_rate = fps_guess > 0.0 ? fps_guess : fps_avg;
    vid_obj.frame_rate          = fps_avg   > 0.0 ? fps_avg   : fps_guess;
    vid_obj.fps                 = vid_obj.playback_frame_rate;
    vid_obj.preferred_rate      = static_cast<int>(std::lround(vid_obj.fps));

    // Bitrate (bits per second)
    if (cp_vid->bit_rate > 0)
        vid_obj.avg_bitrate = static_cast<double>(cp_vid->bit_rate);
    else if (fmt->bit_rate > 0)
        vid_obj.avg_bitrate = static_cast<double>(fmt->bit_rate);
    else
        vid_obj.avg_bitrate = 0.0;

    // Bit depth from pixel format
    vid_obj.bit_depth = 0;
    if (cp_vid->format != AV_PIX_FMT_NONE) {
        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(cp_vid->format));
        if (desc) {
            vid_obj.bit_depth = desc->comp[0].depth; // typical enough
        }
    }

    // Megapixels
    vid_obj.megapixels = 0.0;
    if (vid_obj.width > 0 && vid_obj.height > 0) {
        vid_obj.megapixels = (vid_obj.width * vid_obj.height) / 1000000.0;
    }

    // Encoder string from metadata
    vid_obj.encoder.clear();
    AVDictionaryEntry* enc_tag = av_dict_get(fmt->metadata, "encoder", nullptr, 0);
    if (!enc_tag)
        enc_tag = av_dict_get(st_vid->metadata, "encoder", nullptr, 0);
    if (enc_tag && enc_tag->value)
        vid_obj.encoder = enc_tag->value;

    // Rotation: prefer metadata "rotate", fallback to display matrix
    vid_obj.rotation = 0;

    AVDictionaryEntry* rotate_tag = av_dict_get(st_vid->metadata, "rotate", nullptr, 0);
    if (rotate_tag && rotate_tag->value) {
        vid_obj.rotation = std::atoi(rotate_tag->value);
    } else {
        int side_data_size = 0;
        const uint8_t* display_matrix = av_stream_get_side_data(
            st_vid, AV_PKT_DATA_DISPLAYMATRIX, &side_data_size);
        if (display_matrix && side_data_size >= 9 * 4) {
            double rot = av_display_rotation_get(
                reinterpret_cast<const int32_t*>(display_matrix));
            if (!std::isnan(rot)) {
                int r = static_cast<int>(std::lround(rot));
                // Normalize to multiples of 90 if close
                int mod = ((r % 360) + 360) % 360; // [0, 359]
                int snaps[] = {0, 90, 180, 270};
                int best = mod;
                int best_diff = 361;
                for (int s : snaps) {
                    int diff = std::abs(mod - s);
                    if (diff < best_diff) {
                        best_diff = diff;
                        best = s;
                    }
                }
                vid_obj.rotation = best;
            }
        }
    }

    // helper for metadata -> double
    auto get_meta_double = [](AVDictionary* dict, const char* key) -> double {
        if (!dict) return 0.0;
        AVDictionaryEntry* e = av_dict_get(dict, key, nullptr, 0);
        if (!e || !e->value) return 0.0;
        char* end = nullptr;
        double v = std::strtod(e->value, &end);
        if (end == e->value) return 0.0;
        return v;
    };

    // Preview / poster times (if container provides them; often 0)
    vid_obj.preview_time     = 0.0;
    vid_obj.preview_duration = 0.0;
    vid_obj.poster_time      = 0.0;

    vid_obj.preview_time     = get_meta_double(fmt->metadata, "preview_time");
    vid_obj.preview_duration = get_meta_double(fmt->metadata, "preview_duration");
    vid_obj.poster_time      = get_meta_double(fmt->metadata, "poster_time");

    // If not found at container level, you could also check st->metadata
    if (vid_obj.preview_time == 0.0)
        vid_obj.preview_time = get_meta_double(st_vid->metadata, "preview_time");
    if (vid_obj.preview_duration == 0.0)
        vid_obj.preview_duration = get_meta_double(st_vid->metadata, "preview_duration");
    if (vid_obj.poster_time == 0.0)
        vid_obj.poster_time = get_meta_double(st_vid->metadata, "poster_time");

    ///////////////////////////////////////////////////////////
    /// \brief audio_stream_index
    ///
    MediaObj::Audio audio_obj = {};

    int audio_stream_index = av_find_best_stream(
        fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index < 0) {
        std::cerr << "FFMpegReader: no audio stream in: " << path << "\n";
        avformat_close_input(&fmt);
        return;
    }

    AVStream* st = fmt->streams[audio_stream_index];
    AVCodecParameters* cp = st->codecpar;

    ////////////////////////
    /// \brief media_callback

    audio_obj.path = path;

    // Channels / sample rate
    audio_obj.channel = cp->channels;
    audio_obj.sample_rate = cp->sample_rate;

    // Bits per sample: FFmpeg sometimes leaves these as 0.
    int bits = 0;
    if (cp->bits_per_raw_sample > 0) {
        bits = cp->bits_per_raw_sample;
    } else if (cp->bits_per_coded_sample > 0) {
        bits = cp->bits_per_coded_sample;
    } else {
        // Fallback – for MP3/AAC this is fuzzy anyway.
        // If you only care about decoded PCM later, you can just choose 16 or 32.
        bits = 16;
    }
    audio_obj.bit_per_sample = static_cast<size_t>(bits);

    // Block align = bytes per sample frame (all channels)
    if (audio_obj.channel > 0 && bits > 0) {
        audio_obj.block_align = audio_obj.channel * (bits / 8);
    } else {
        audio_obj.block_align = 0;
    }

    // Byte rate = sample_rate * block_align
    if (audio_obj.sample_rate > 0 && audio_obj.block_align > 0) {
        audio_obj.byte_rate = static_cast<size_t>(audio_obj.sample_rate * audio_obj.block_align);
    } else {
        audio_obj.byte_rate = 0;
    }

    // Estimate data_chunk_size from duration
    audio_obj.data_chunk_size = 0;
    if (st->duration != AV_NOPTS_VALUE &&
        audio_obj.sample_rate > 0 &&
        audio_obj.block_align > 0)
    {
        double dur_sec = st->duration * av_q2d(st->time_base);
        double samples_per_chan = dur_sec * audio_obj.sample_rate;
        long samples_per_chan_round = static_cast<long>(samples_per_chan + 0.5);

        // samples_per_chan * block_align gives you approximate PCM bytes
        audio_obj.data_chunk_size = static_cast<int>(
            samples_per_chan_round * audio_obj.block_align
            );
    }

    // WAV-specific header fields: you *cannot* reliably fill these
    // for MP3/AAC/MP4 via FFmpeg alone. For generic audio, leave them as defaults.
    audio_obj.chunk_id        = "";       // e.g. "RIFF" if you parse WAV yourself
    audio_obj.chunk_size      = 0;
    audio_obj.chunk_format    = "";       // e.g. "WAVE"
    audio_obj.sub_chunk_id    = "";       // e.g. "fmt "
    audio_obj.sub_chunk_size  = 0;
    audio_obj.audio_format    = 3;        // 3 = IEEE float in your existing code (only meaningful for WAV)
    audio_obj.offset_first_sample = 0;    // 44 for plain WAV; unknown for compressed

    // The sample vectors (data, amplitude_time_series, etc.) are not filled here.
    // This function is just a "probe".
    audio_obj.data.clear();
    audio_obj.amplitude_time_series.clear();
    audio_obj.amplitude_envelope.clear();
    audio_obj.root_mean_square.clear();
    audio_obj.zero_crossing_rate.clear();

    avformat_close_input(&fmt);

    std::vector<uint8_t> out_rgba;
    int w_vid;
    int h_vid;
    if (ffmpeg_get_random_frame_rgba(path, out_rgba, w_vid, h_vid)) {
        vid_obj.thumnail = out_rgba;
    }

    media_callback(vid_obj, audio_obj);
}

bool FFMpegReader::ffmpeg_get_random_frame_rgba(const std::string& path, std::vector<uint8_t>& out_rgba, int& out_w, int& out_h) {

    AVFormatContext* fmt = nullptr;
    bool ok = false;

    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0)
        return false;

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return false;
    }

    int video_stream_index = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        avformat_close_input(&fmt);
        return false;
    }

    AVStream* st = fmt->streams[video_stream_index];

    double dur_sec = 0.0;
    if (st->duration != AV_NOPTS_VALUE)
        dur_sec = st->duration * av_q2d(st->time_base);
    else if (fmt->duration != AV_NOPTS_VALUE)
        dur_sec = fmt->duration / static_cast<double>(AV_TIME_BASE);

    if (dur_sec <= 0.0) dur_sec = 1.0; // fallback

    // random in [0, dur_sec)
    uint64_t seed = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, std::max(0.0, dur_sec - 0.1));

    double t = dist(rng);

    avformat_close_input(&fmt);

    return ffmpeg_get_frame_rgba_at(path, t, out_rgba, out_w, out_h);
}

bool FFMpegReader::ffmpeg_get_frame_rgba_at(
    const std::string& path,
    double time_sec,
    std::vector<uint8_t>& out_rgba,
    int& out_w,
    int& out_h
    ) {

    AVFormatContext* fmt = nullptr;
    AVCodecContext*  dec_ctx = nullptr;
    SwsContext*      sws = nullptr;
    AVFrame*         frame = nullptr;
    AVPacket*        pkt = nullptr;

    bool ok = false;

    av_log_set_level(AV_LOG_ERROR);

    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Cannot open file: " << path << "\n";
        // goto cleanup;
    }

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        std::cerr << "Cannot find stream info: " << path << "\n";
        // goto cleanup;
    }

    // Find best video stream
    int video_stream_index = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        std::cerr << "No video stream in: " << path << "\n";
        // goto cleanup;
    }

    AVStream* st = fmt->streams[video_stream_index];
    AVCodecParameters* cp = st->codecpar;

    // Create decoder
    {
        const AVCodec* dec = avcodec_find_decoder(cp->codec_id);
        if (!dec) {
            std::cerr << "Decoder not found for codec_id=" << cp->codec_id << "\n";
            // goto cleanup;
        }

        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx) {
            std::cerr << "Failed to alloc codec context\n";
            // goto cleanup;
        }

        if (avcodec_parameters_to_context(dec_ctx, cp) < 0) {
            std::cerr << "Failed to copy codec parameters\n";
            // goto cleanup;
        }

        if (avcodec_open2(dec_ctx, dec, nullptr) < 0) {
            std::cerr << "Failed to open codec\n";
            // goto cleanup;
        }
    }

    // Compute seek target (timestamp in stream time_base)
    if (time_sec < 0.0) time_sec = 0.0;
    int64_t target_ts = static_cast<int64_t>(time_sec / av_q2d(st->time_base));

    if (av_seek_frame(fmt, video_stream_index, target_ts, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "Seek failed\n";
        goto cleanup;
    }
    avcodec_flush_buffers(dec_ctx);

    frame = av_frame_alloc();
    pkt   = av_packet_alloc();
    if (!frame || !pkt) {
        std::cerr << "Failed to alloc frame/packet\n";
        goto cleanup;
    }

    out_rgba.clear();
    out_w = 0;
    out_h = 0;

    // Decode loop: read packets until we get one decoded frame
    while (av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index != video_stream_index) {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(dec_ctx, pkt) < 0) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);

        while (true) {
            int ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0) {
                goto cleanup;
            }

            // We got a frame -> convert to RGBA
            out_w = frame->width;
            out_h = frame->height;

            if (out_w <= 0 || out_h <= 0) {
                goto cleanup;
            }

            sws = sws_getContext(
                frame->width, frame->height,
                static_cast<AVPixelFormat>(frame->format),
                frame->width, frame->height,
                AV_PIX_FMT_RGBA,
                SWS_BILINEAR,
                nullptr, nullptr, nullptr
                );
            if (!sws) {
                std::cerr << "Failed to create sws context\n";
                goto cleanup;
            }

            out_rgba.resize(static_cast<size_t>(out_w) * out_h * 4);

            uint8_t* dst_data[4] = { nullptr };
            int dst_linesize[4] = { 0 };

            if (av_image_fill_arrays(
                    dst_data, dst_linesize,
                    out_rgba.data(),
                    AV_PIX_FMT_RGBA,
                    out_w, out_h,
                    1) < 0) {
                std::cerr << "av_image_fill_arrays failed\n";
                goto cleanup;
            }

            sws_scale(
                sws,
                frame->data, frame->linesize,
                0, frame->height,
                dst_data, dst_linesize
                );

            ok = true;
            goto cleanup; // done
        }
    }

cleanup:
    if (pkt)   av_packet_free(&pkt);
    if (frame) av_frame_free(&frame);
    if (sws)   sws_freeContext(sws);
    if (dec_ctx) avcodec_free_context(&dec_ctx);
    if (fmt)   avformat_close_input(&fmt);

    return ok;

}
