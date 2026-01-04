#ifndef INTERNAL_TOOLS_H
#define INTERNAL_TOOLS_H

#include "define.h"
#include "obj_audio.h"

namespace raiden {

class internal_tools {
public:
    static Matrixcf stft(Vectorf &x, int n_fft, int n_hop, const std::string &win, bool center,
                         const std::string &mode);
    static Matrixf stftMagnitude(const std::vector<float> &data, int n_fft, int n_hop, const std::string &win, bool center,
                                 const std::string &mode);

    static Matrixf combineSpectrogramChunks(const std::vector<Matrixf>& chunks, int hop_frames);

    static  Matrixf create_mel_filterbank(int sr, int n_fft, int n_mels = 512,
                                         float fmin = 0.0f, float fmax = -1.0f, bool slaney = false);

    static Vectorf toEigen(const std::vector<float>& data);
    static Matrixf spectrogram(const Matrixcf &X, float power = 1.f) {
        return X.array().abs().pow(power).matrix();
    }

    // Convert frequency in Hz to mel
    static float hzToMel(float hz) {
        return 2595.0f * std::log10(1.0f + hz / 700.0f);
    }
    // Convert mel to frequency in Hz
    static float melToHz(float mel) {
        return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
    }

    static Vectorf pad(Vectorf &x, int left, int right, const std::string &mode, float value);
    static std::vector<float> createEvenlySpacedVector(float end, int numElements);

    static Eigen::MatrixXf fromFlatToMatrix(const std::vector<float>& flat, int height, int width) {
        Eigen::Map<const Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> matrix(flat.data(), height, width);
        return matrix;
    }

    static Eigen::MatrixXf power_to_db(const Eigen::MatrixXf& S_power,
                                       bool  ref_is_max   = true,
                                       float ref_value    = 1.0f,
                                       float amin         = 1e-10f,
                                       float top_db       = 80.0f);

    static Eigen::MatrixXf amplitude_to_db(const Eigen::MatrixXf& S_mag,
                                           bool  ref_is_max   = true,
                                           float ref_value    = 1.0f,
                                           float amin         = 1e-10f,
                                           float top_db       = 80.0f);

    static Eigen::MatrixXf db_to_unit(const Eigen::MatrixXf& db,
                                      float min_db = -80.0f, float max_db = 0.0f);

    //////////////////////
    static Spectrogram buildTile(const Eigen::MatrixXf &data);
};

}

#endif // INTERNAL_TOOLS_H
