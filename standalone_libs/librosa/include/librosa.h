#ifndef LIBROSA_H
#define LIBROSA_H

#pragma once

#include <obj_audio.h>
#include <string>
#include <vector>

namespace raiden {

class audio {
public:
    static Signal load(const std::string& path, const int& sr=-1, const bool& mono=true, const float& offset=0.f, const float& duration=-1.0f);
    static Signal loadBufferToWaveMono(std::vector<uint8_t> data, int sample_rate);
// outsider util helper func
public:
    static std::vector<SpecViewPort> build_global_segments(int64_t total_samples, int sr, double window_sec,
                          double overlap_frac, int n_fft, int n_hop, bool center=false);
    static std::vector<SpecViewPortNDC> project_visible_segments_to_ndc(const std::vector<SpecViewPort> &all, double view_start_sec, double view_span_sec);
    static SignalAdaptGl project_visible_adapt_wave(const std::vector<float> &audio_wave, int pixel_w, const float& global_peak);
    static float to_ndc(float t, double view_start_sec, double view_span_sec);
};

}

#endif // LIBROSA_H
