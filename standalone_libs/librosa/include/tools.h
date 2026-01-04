#ifndef TOOLS_H
#define TOOLS_H
#pragma once

#include "obj_audio.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

namespace raiden {

class tools {
public:
    static SpectrogramTile loadStft(const std::vector<float>& data, const int &sr=22050, int n_fft = 1024, int n_hop = 256);
    static SpectrogramTile loadMelSpec(const std::vector<float>& data, const int &sr=22050, int n_fft = 1024, int n_hop = 256, int n_mels = 512,
                                       float fmin = 0.0f, float fmax = -1.0f);

    static SpectrogramTileOverlap loadStftOverlap(const std::vector<float>& data, const int &sr=22050, int n_fft = 1024, int n_hop = 256);
    static SpectrogramTileOverlap loadMelOverlap(const std::vector<float>& data, const int &sr=22050, int n_fft = 1024, int n_hop = 256, int n_mels = 512,
                                                 float fmin = 0.0f, float fmax = -1.0f, float segment_sec = 0.5f, float overlap_ratio = 0.5f, bool to_unit = true);

    static SpectrogramByte flatMatrixToByteImg(const std::vector<float>& flat, int height, int width, const std::string &file_name, bool is_db = false);
    static std::vector<float> extractSpectrogramSlice(
        const std::vector<float>& full_flat,
        int full_width,
        int height,
        int start_frame,
        int frame_size);


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static inline int choose_hop_for_width(int sr, double T_sec, int n_fft, int target_W, bool center)
    {
        if (target_W <= 1) return std::max(1, n_fft); // degenerate
        const double L = T_sec * sr;
        const double numer = center ? L : (L - n_fft);
        int hop = static_cast<int>(std::floor(numer / (target_W - 1)));
        return std::max(1, hop);
    }
};

}

#endif // TOOLS_H
