#include "internal_tools.h"
#include <iostream>

namespace raiden {

Matrixcf internal_tools::stft(Vectorf &x, int n_fft, int n_hop, const std::string &win, bool center, const std::string &mode) {

    Vectorf window;
    if (win == "hann" || win == "hanning") {
        window = 0.5f * (1.f - (Vectorf::LinSpaced(n_fft, 0.f, static_cast<float>(n_fft - 1)) * 2.f * M_PI / n_fft).array().cos());
        // window = 0.5f * (1.f - (Vectorf::LinSpaced(n_fft, 0.f, static_cast<float>(n_fft - 1))
        //     * (2.f * static_cast<float>(M_PI) / static_cast<float>(n_fft))).array().cos());
    } else if (win == "hamming") {
        window = 0.54f - 0.46f * (Vectorf::LinSpaced(n_fft, 0.f, static_cast<float>(n_fft - 1)).array().cos());
    }

    // Normalize like librosa (preserve relative energy)
    window /= window.sum();

    // Optional center-padding (like librosa)
    int pad_len = center ? n_fft / 2 : 0;
    Vectorf x_paded = pad(x, pad_len, pad_len, mode, 0.f);

    // Calculate number of frames
    int n_frames = static_cast<int>(std::floor((x_paded.size() - n_fft) / n_hop)) + 1;

    // Create STFT result container (transpose-friendly format)
    Matrixcf X(n_frames, n_fft / 2 + 1);
    Eigen::FFT<float> fft;

    for (int i = 0; i < n_frames; ++i) {
        // Windowed frame
        int start = i * n_hop;
        Vectorf frame = window.array() * x_paded.segment(start, n_fft).array();

        // Convert Eigen vector to std::vector<float>
        std::vector<float> frame_std(frame.data(), frame.data() + frame.size());

        // Perform FFT
        std::vector<std::complex<float>> fft_result;
        fft.fwd(fft_result, frame_std);

        // Copy only positive frequencies
        for (int k = 0; k < n_fft / 2 + 1; ++k) {
            X(i, k) = fft_result[k];
        }
    }

    Matrixcf X_Response = X.transpose().eval();
    return X_Response;
}

Matrixf internal_tools::stftMagnitude(const std::vector<float> &data, int n_fft, int n_hop, const std::string &win, bool center, const std::string &mode) {
    // To Eigen
    Vectorf eigenVector = toEigen(data);
    const Matrixcf stft = internal_tools::stft(eigenVector, n_fft, n_hop, win, center, mode);
    Matrixf magnitudes = stft.array().abs().square().matrix();
    return magnitudes;
}

Matrixf internal_tools::combineSpectrogramChunks(const std::vector<Matrixf> &chunks, int hop_frames) {

    if (chunks.empty()) return Matrixf();

    const int bins = chunks[0].rows();
    const int frames_per_chunk = chunks[0].cols();
    const int total_frames = hop_frames * (chunks.size() - 1) + frames_per_chunk;

    Eigen::MatrixXf accumulator = Eigen::MatrixXf::Zero(bins, total_frames);
    Eigen::MatrixXi counter = Eigen::MatrixXi::Zero(bins, total_frames);

    for (size_t i = 0; i < chunks.size(); ++i) {
        int start_col = static_cast<int>(i) * hop_frames;

        for (int f = 0; f < frames_per_chunk; ++f) {
            int global_col = start_col + f;
            if (global_col >= total_frames) break;

            for (int b = 0; b < bins; ++b) {
                accumulator(b, global_col) += chunks[i](b, f);
                counter(b, global_col) += 1;
            }
        }
    }

    // Normalize overlapping regions
    for (int b = 0; b < bins; ++b) {
        for (int f = 0; f < total_frames; ++f) {
            if (counter(b, f) > 0)
                accumulator(b, f) /= static_cast<float>(counter(b, f));
        }
    }

    return accumulator;

}

Spectrogram internal_tools::buildTile(const Eigen::MatrixXf &data) {
    const int magnifyX = 10; // pixels per column (e.g., frame width)
    const int magnifyY = 10; // pixels per row (e.g., bin height)

    const int tileWidth = data.cols() * magnifyX;
    const int tileHeight = data.rows() * magnifyY;

    std::vector<float> tileData;
    tileData.reserve(tileWidth * tileHeight);

    for (int row = 0; row < data.rows(); ++row) {
        for (int y = 0; y < magnifyY; ++y) { // magnify vertically
            for (int col = 0; col < data.cols(); ++col) {
                float value = data(row, col);
                for (int x = 0; x < magnifyX; ++x) { // magnify horizontally
                    tileData.push_back(value);
                }
            }
        }
    }

    return { tileWidth, tileHeight, tileData };
}

Matrixf internal_tools::create_mel_filterbank(int sr, int n_fft, int n_mels, float fmin, float fmax, bool slaney) {
    int n_fft_bins = n_fft / 2 + 1;
    if (fmax <= 0.0f) fmax = sr / 2.0f;  // Nyquist frequency

    // Step 1: Compute mel scale points
    float mel_min = hzToMel(fmin);
    float mel_max = hzToMel(fmax);

    std::vector<float> mel_points(n_mels + 2);  // includes edges
    for (int i = 0; i < n_mels + 2; ++i) {
        mel_points[i] = melToHz(mel_min + (mel_max - mel_min) * i / (n_mels + 1));
    }

    // Step 2: Convert mel points to FFT bin indices
    std::vector<int> bin_points(n_mels + 2);
    for (int i = 0; i < mel_points.size(); ++i) {
        // bin_points[i] = static_cast<int>(std::floor((n_fft + 1) * mel_points[i] / sr));
        bin_points[i] = static_cast<int>(std::floor(n_fft_bins * mel_points[i] / (sr / 2.0f)));
    }

    // Step 3: Create filterbank
    Matrixf filterbank = Matrixf::Zero(n_mels, n_fft_bins);

    for (int i = 0; i < n_mels; ++i) {
        int left = bin_points[i];
        int center = bin_points[i + 1];
        int right = bin_points[i + 2];

        for (int j = left; j < center; ++j) {
            if (j >= 0 && j < n_fft_bins)
                filterbank(i, j) = (j - left) / float(center - left);
        }
        for (int j = center; j < right; ++j) {
            if (j >= 0 && j < n_fft_bins)
                filterbank(i, j) = (right - j) / float(right - center);
        }

        if (slaney) {
            float bw_hz = mel_points[i + 2] - mel_points[i]; // bandwidth in Hz
            if (bw_hz > 0.0f) {
                float scale = 2.0f / bw_hz;
                filterbank.row(i) *= scale;
            }
        }
    }

    return filterbank;
}

Vectorf internal_tools::toEigen(const std::vector<float> &data) {
    Vectorf eigenVector(1, data.size());
    for (int i = 0; i < data.size(); ++i) {
        eigenVector(0, i) = data[i];
    }
    return eigenVector;
}

Vectorf internal_tools::pad(Vectorf &x, int left, int right, const std::string &mode, float value) {
    Vectorf x_paded = Vectorf::Constant(left+x.size()+right, value);
    x_paded.segment(left, x.size()) = x;
    if (mode == "reflect") {
        for (int i = 0; i < left; ++i) {
            x_paded[i] = x[left-i];
        }
        for (int i = left; i < left+right; ++i){
            x_paded[i+x.size()] = x[x.size()-2-i+left];
        }
    }

    if (mode == "symmetric") {
        for (int i = 0; i < left; ++i) {
            x_paded[i] = x[left-i-1];
        }
        for (int i = left; i < left+right; ++i) {
            x_paded[i+x.size()] = x[x.size()-1-i+left];
        }
    }

    if (mode == "edge") {
        for (int i = 0; i < left; ++i) {
            x_paded[i] = x[0];
        }
        for (int i = left; i < left+right; ++i) {
            x_paded[i+x.size()] = x[x.size()-1];
        }
    }
    return x_paded;
}

std::vector<float> internal_tools::createEvenlySpacedVector(float end, int numElements) {
    std::vector<float> result;
    if (numElements < 1) {
        return result;
    }

    float step = end / ((float)numElements - 1);
    for (int i = 0; i < numElements - 1; ++i) {
        result.push_back(step * static_cast<float>(i));
    }

    // Add the last value
    result.push_back(end);

    return result;
}


Eigen::MatrixXf internal_tools::power_to_db(const Eigen::MatrixXf& S_power,
                                            bool  ref_is_max,
                                            float ref_value,
                                            float amin,
                                            float top_db) {
    Eigen::ArrayXXf S = S_power.array().max(amin);

    // Reference level
    float ref = ref_is_max ? S.maxCoeff() : std::max(ref_value, amin);

    // dB: 20 * log10(S / ref)
    Eigen::ArrayXXf log_spec = 10.0f * (S / ref).log10();

    // top_db limiting (clip dynamic range to [max_db - top_db, max_db])
    if (top_db >= 0.0f) {
        float max_db = log_spec.maxCoeff();
        float lower  = max_db - top_db;
        log_spec = log_spec.max(lower);
    }
    return log_spec.matrix();
}

Eigen::MatrixXf internal_tools::amplitude_to_db(const Eigen::MatrixXf& S_mag,
                                                bool  ref_is_max,
                                                float ref_value,
                                                float amin,
                                                float top_db) {
    Eigen::ArrayXXf S = S_mag.array().abs().max(amin);

    float ref = ref_is_max ? S.maxCoeff() : std::max(ref_value, amin);

    Eigen::ArrayXXf log_spec = 20.0f * (S / ref).log10();

    if (top_db >= 0.0f) {
        float max_db = log_spec.maxCoeff();
        float lower  = max_db - top_db;
        log_spec = log_spec.max(lower);
    }
    return log_spec.matrix();
}

Eigen::MatrixXf internal_tools::db_to_unit(const Eigen::MatrixXf &db, float min_db, float max_db) {
    Eigen::ArrayXXf x = db.array().min(max_db).max(min_db);
    return ((x - min_db) / (max_db - min_db)).matrix(); // [-80,0] → [0,1]
}

}
