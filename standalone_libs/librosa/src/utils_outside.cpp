#include "librosa.h"
#include <cmath>
#include <algorithm>

namespace raiden {
std::vector<SpecViewPort> audio::build_global_segments(int64_t total_samples, int sr, double window_sec,
                                                    double overlap_frac, int n_fft, int n_hop, bool center) {


    std::vector<SpecViewPort> out;
    if (total_samples <= 0 || sr <= 0) return out;

    const int64_t win = (int64_t)std::llround(window_sec * sr);
    const int64_t step = (int64_t)std::llround(win * (1.0 - overlap_frac));
    if (win <= 0 || step <= 0) return out;

    int64_t num = 1;
    if (total_samples > win) {
        num = (int64_t)std::ceil(double(total_samples - win) / double(step)) + 1;
    }
    out.reserve((size_t)num);

    const int64_t offs = center ? (n_fft / 2) : 0;

    for (int64_t i = 0; i < num; ++i) {
        const int64_t s = i * step;
        if (s >= total_samples) break;
        int64_t e = s + win;
        if (e > total_samples) e = total_samples;
        if (e <= s) break;

        const int64_t seg_len = e - s;

        // frame index aligning with your STFT (fully contained frames)
        int frame_start = int((s + offs) / n_hop);
        int frame_size  = (seg_len >= n_fft) ? (1 + int((seg_len - n_fft) / n_hop)) : 0;

        out.push_back({
            float(s) / sr,
            float(e) / sr,
            int(s), int(e),
            frame_start, frame_size
        });
    }
    return out;

}

std::vector<SpecViewPortNDC> audio::project_visible_segments_to_ndc(const std::vector<SpecViewPort> &all, double view_start_sec, double view_span_sec) {
    std::vector<SpecViewPortNDC> out;
    if (all.empty() || view_span_sec <= 0.0) return out;

    const double view_end = view_start_sec + view_span_sec;

    // Find first overlapping index (binary search on end_sec > view_start_sec)
    auto it = std::lower_bound(all.begin(), all.end(), view_start_sec,
                               [](const SpecViewPort& w, double t) { return double(w.end_sec) <= t; });

    for (; it != all.end(); ++it) {
        if (double(it->start_sec) >= view_end) break; // past the view

        float x0 = to_ndc(it->start_sec, view_start_sec, view_span_sec);
        float x1 = to_ndc(it->end_sec,   view_start_sec, view_span_sec);

        // clamp (optional safety)
        x0 = std::max(-1.f, std::min(1.f, x0));
        x1 = std::max(-1.f, std::min(1.f, x1));

        out.push_back({ x0, x1, *it });
    }
    return out;                               // -1..1
}

float audio::to_ndc(float t, double view_start_sec, double view_span_sec) {
    if (view_span_sec <= 0.0) return -1.f;
    float u = float((t - view_start_sec) / view_span_sec); // 0..1 across viewport
    return -1.f + 2.f * u;                                 // -1..1
}

SignalAdaptGl audio::project_visible_adapt_wave(const std::vector<float> &audio_wave, int pixel_w, const float& global_peak) {
    std::vector<float> adapt_audio_wave;
    int gl_draw_count = 0;

    adapt_audio_wave.clear();
    if (audio_wave.empty() || pixel_w <= 0) { return {{}, gl_draw_count}; }

    const int N = (int)audio_wave.size();
    const float gain = (global_peak > 0.f) ? (1.0f / global_peak) : 1.0f;

    if (N <= pixel_w) {
        // High-resolution mode: one vertex per sample (LINE_STRIP)
        adapt_audio_wave.reserve(N * 2);
        for (int i = 0; i < N; ++i) {
            float t = (N > 1) ? (float)i / (float)(N - 1) : 0.f;
            float x_ndc = t * 2.f - 1.f;
            float y = std::max(-1.f, std::min(1.f, audio_wave[i] * gain));
            adapt_audio_wave.push_back(x_ndc); adapt_audio_wave.push_back(y);
        }
        gl_draw_count = N; // vertices (pairs)
        // render path will use LINE_STRIP
        return { adapt_audio_wave, gl_draw_count };
    } else {
        // Envelope mode: per-pixel min/max (LINES)
        const int spp = std::max(1, N / pixel_w);
        adapt_audio_wave.reserve(pixel_w * 4);
        for (int px = 0; px < pixel_w; ++px) {
            int s0 = px * spp;
            int s1 = (px == pixel_w - 1) ? N : std::min(N, s0 + spp);
            if (s0 >= N) break;

            float vmin =  1e9f, vmax = -1e9f;
            for (int j = s0; j < s1; ++j) {
                float v = audio_wave[j] * gain;
                if (v < vmin) vmin = v;
                if (v > vmax) vmax = v;
            }
            vmin = std::max(-1.f, std::min(1.f, vmin));
            vmax = std::max(-1.f, std::min(1.f, vmax));

            float x_ndc = (pixel_w == 1) ? 0.f : ((float)px / (float)(pixel_w - 1) * 2.f - 1.f);
            // two points: (x, vmin) and (x, vmax)
            adapt_audio_wave.push_back(x_ndc); adapt_audio_wave.push_back(vmin);
            adapt_audio_wave.push_back(x_ndc); adapt_audio_wave.push_back(vmax);
        }
        gl_draw_count = (int)adapt_audio_wave.size() / 2;
        return { adapt_audio_wave, gl_draw_count };
    }
}

}
