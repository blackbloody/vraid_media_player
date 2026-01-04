#include "librosa.h"

#include <vector>

#include <sndfile.h>
#include <samplerate.h>

#include <cmath>
#include <cstring>
#include <numeric>

#include <algorithm>
#include <iomanip>
#include <iostream>

namespace raiden {

Signal audio::load(const std::string& path, const int& sr, const bool& mono, const float& offset, const float& duration) {
    SF_INFO info{};
    int target_sr = sr;
    float duration_sec = duration;
    float offset_sec = offset;

    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
    if (!f) {
        // failed to open
        return {{}, 0};
    }

    // Optional: enforce container if you really want WAV only
    // if ((info.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) { sf_close(f); return {{}, 0}; }

    const int src_sr = info.samplerate;
    const int ch     = info.channels;
    const sf_count_t total = info.frames;

    if (offset_sec < 0.f) offset_sec = 0.f;
    if (duration_sec < 0.f) {
        duration_sec = (total > 0) ? float(total) / float(src_sr) : 0.f;
    }

    const sf_count_t start = std::min<sf_count_t>(
        total, static_cast<sf_count_t>(llround(offset_sec * src_sr)));
    const sf_count_t want  = static_cast<sf_count_t>(llround(duration_sec * src_sr));
    const sf_count_t end   = std::min<sf_count_t>(total, start + want);
    sf_count_t need        = (end > start) ? (end - start) : 0;

    if (need <= 0) { sf_close(f); return {{}, src_sr}; }

    if (sf_seek(f, start, SEEK_SET) < 0) { sf_close(f); return {{}, 0}; }

    std::vector<float> interleaved(static_cast<size_t>(need) * static_cast<size_t>(ch));
    sf_count_t got = sf_readf_float(f, interleaved.data(), need);
    sf_close(f);

    if (got <= 0) { return {{}, src_sr}; }
    interleaved.resize(static_cast<size_t>(got) * static_cast<size_t>(ch));

    // --- make a mono buffer (API is mono-shaped) ---
    std::vector<float> mono_buf(static_cast<size_t>(got));
    if (ch == 1) {
        std::copy_n(interleaved.data(), static_cast<size_t>(got), mono_buf.data());
    } else if (mono) {
        // average all channels
        for (sf_count_t i = 0; i < got; ++i) {
            float sum = 0.f;
            const size_t base = static_cast<size_t>(i) * static_cast<size_t>(ch);
            for (int c = 0; c < ch; ++c) sum += interleaved[base + static_cast<size_t>(c)];
            mono_buf[static_cast<size_t>(i)] = sum / float(ch);
        }
    } else {
        // keep channel 0 only (still mono shape; avoids breaking callers)
        for (sf_count_t i = 0; i < got; ++i) {
            mono_buf[static_cast<size_t>(i)] = interleaved[static_cast<size_t>(i) * static_cast<size_t>(ch)];
        }
    }

    const int out_sr = (target_sr > 0) ? target_sr : src_sr;
    if (out_sr != src_sr) {
        const double ratio = double(out_sr) / double(src_sr);
        const sf_count_t cap = static_cast<sf_count_t>(std::ceil(double(got) * ratio)) + 8;

        std::vector<float> out(static_cast<size_t>(cap));

        SRC_DATA sd{};
        sd.data_in       = mono_buf.data();
        sd.input_frames  = static_cast<long>(got);
        sd.data_out      = out.data();
        sd.output_frames = static_cast<long>(cap);
        sd.src_ratio     = ratio;
        sd.end_of_input  = 1;

        if (src_simple(&sd, SRC_SINC_BEST_QUALITY, 1) != 0) {
            return {{}, 0};
        }
        out.resize(static_cast<size_t>(sd.output_frames_gen));
        return { std::move(out), out_sr };
    }

    return { std::move(mono_buf), out_sr };
}

static inline int16_t read_s16_be(const uint8_t* p)
{
    // p[0] = high byte, p[1] = low byte (big-endian)
    uint16_t u = (uint16_t(p[0]) << 8) | uint16_t(p[1]);
    return static_cast<int16_t>(u);
}

static inline int16_t read_s16_le(const uint8_t* p)
{
    // p[0] = low byte, p[1] = high byte (little-endian)
    uint16_t u = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    return static_cast<int16_t>(u);
}

Signal audio::loadBufferToWaveMono(std::vector<uint8_t> data, int sample_rate) {


    Signal out;
    out.sr = sample_rate;

    const size_t n_samples = data.size() / sizeof(int16_t);
    out.data.resize(n_samples);

    const int16_t* s = reinterpret_cast<const int16_t*>(data.data());
    const float scale = 1.0f / 32768.0f;

    for (size_t i = 0; i < n_samples; ++i)
        out.data[i] = float(s[i]) * scale;

    return out;
    /*
    Signal out;

    int ch = 1;
    const size_t bytes_per_sample = 2; // S16
    const size_t frame_size_bytes = ch * bytes_per_sample;
    const size_t n_frames = data.size() / frame_size_bytes;

    out.sr = sample_rate;
    out.data.resize(n_frames);

    const float scale = 1.0f / 32768.0f; // S16 -> [-1,1)

    for (size_t i = 0; i < n_frames; ++i) {
        float accum = 0.0f;

        for (int c = 0; c < ch; ++c) {
            int16_t s16 = 0;
            const size_t byte_idx = i * frame_size_bytes + c * bytes_per_sample;
            s16 = read_s16_le(&data[byte_idx]);

            accum += static_cast<float>(s16) * scale;
        }

        out.data[i] = accum / static_cast<float>(ch); // downmix to mono
    }

    return out;
    */

}

}
