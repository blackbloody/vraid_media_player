#include "tools.h"
#include "define.h"
#include "internal_tools.h"

namespace raiden {

SpectrogramTile tools::loadStft(const std::vector<float> &data, const int &sr, int n_fft, int n_hop) {

    std::string win = "hann";
    bool center = true; // Center the signal
    std::string mode = "reflect";

    // To Eigen
    Vectorf eigenVector = internal_tools::toEigen(data);
    // To Matrixcf(stft)
    Matrixcf stft = internal_tools::stft(eigenVector, n_fft, n_hop, win, center, mode);

    // Magnitudes
    Matrixf magnitudes = internal_tools::spectrogram(stft, 1.0f);

    float epsilon = 1e-5f;
    Eigen::MatrixXf logMagnitudes = 10.0f * (magnitudes.array() + epsilon).log10();

    // Clip to [-80, 0] dB and normalize
    float min_db = -60; // e.g., -60.0f
    // Clamp to [min_db, 0] dB
    logMagnitudes = logMagnitudes.cwiseMax(min_db).cwiseMin(0.0f);
    // Normalize to [0, 1]
    Matrixf normalizedMagnitudes = (logMagnitudes.array() - min_db) / std::abs(min_db);

    int width = static_cast<int>(normalizedMagnitudes.cols());
    int height = static_cast<int>(normalizedMagnitudes.rows());
    std::vector<float> normalizedData;
    for (int row = 0; row < normalizedMagnitudes.rows(); ++row) {
        for (int col = 0; col < normalizedMagnitudes.cols(); ++col) {
            float value = normalizedMagnitudes(row, col);
            normalizedData.push_back(value);
        }
    }
    Spectrogram spec = { width, height, normalizedData };
    Spectrogram tile_spec = internal_tools::buildTile(normalizedMagnitudes);

    SpectrogramTile tile = {
        Spectrogram{spec.width, spec.height, spec.data},
        tile_spec.width,
        tile_spec.height,
        tile_spec.data
    };
    return tile;
}

SpectrogramTile tools::loadMelSpec(const std::vector<float> &data, const int &sr, int n_fft, int n_hop, int n_mels, float fmin, float fmax) {

    std::string win = "hamming";
    win = "hann";
    bool center = true; // Center the signal
    std::string mode = "reflect";

    // To Eigen
    Vectorf eigenVector = internal_tools::toEigen(data);
    // To Matrixcf(stft)
    Matrixcf stft = internal_tools::stft(eigenVector, n_fft, n_hop, win, center, mode);
    Matrixf magnitudes = internal_tools::spectrogram(stft, 1.0f);

    Matrixf mel_filterbank = internal_tools::create_mel_filterbank(sr, n_fft, n_mels);
    Matrixf mel_spectrogram = mel_filterbank * magnitudes;

    float epsilon = 1e-5f;
    Matrixf log_mel_spectrogram = 10.0f * (mel_spectrogram.array() + epsilon).log10();

    // Clip to [-80, 0] dB and normalize
    float min_db = -80; // e.g., -60.0f
    // Clamp to [min_db, 0] dB
    log_mel_spectrogram = log_mel_spectrogram.cwiseMax(min_db).cwiseMin(fmin);
    // Normalize to [0, 1]
    Matrixf normalizedMagnitudes = (log_mel_spectrogram.array() - min_db) / std::abs(min_db);
    int width = static_cast<int>(normalizedMagnitudes.cols());
    int height = static_cast<int>(normalizedMagnitudes.rows());
    std::vector<float> normalizedData;
    for (int row = 0; row < normalizedMagnitudes.rows(); ++row) {
        for (int col = 0; col < normalizedMagnitudes.cols(); ++col) {
            float value = normalizedMagnitudes(row, col);
            normalizedData.push_back(value);
        }
    }
    Spectrogram spec = { width, height, normalizedData };
    Spectrogram tile_spec = internal_tools::buildTile(normalizedMagnitudes);

    SpectrogramTile tile = {
        Spectrogram{spec.width, spec.height, spec.data},
        tile_spec.width,
        tile_spec.height,
        tile_spec.data
    };

    return tile;
}

//////////////////////////////

SpectrogramTileOverlap tools::loadStftOverlap(const std::vector<float>& data,
                                              const int& sr,
                                              int n_fft, int n_hop) {
    // ---- Parameters (debug-tunable) ----
    const float segment_sec   = 0.5f;   // seconds
    const float overlap_ratio = 0.5f;   // 50%

    // ---- Basic guards ----
    if (sr <= 0 || n_fft <= 0 || n_hop <= 0) {
        //g_warning("loadStftOverlap: invalid params sr=%d n_fft=%d n_hop=%d", sr, n_fft, n_hop);
        return {};
    }

    const int total_samples   = static_cast<int>(data.size());
    int segment_samples       = static_cast<int>(std::floor(sr * segment_sec));
    if (segment_samples < n_fft) {
        // You cannot compute even a single STFT frame
        //g_warning("loadStftOverlap: segment_samples=%d < n_fft=%d (sr=%d sec=%.3f).", segment_samples, n_fft, sr, segment_sec);
        return {};
    }

    int hop_samples = static_cast<int>(std::round(segment_samples * (1.0f - overlap_ratio)));
    if (hop_samples <= 0) {
        hop_samples = 1; // avoid infinite loop
        //g_warning("loadStftOverlap: hop_samples rounded to 0, forcing to 1.");
    }

    // ---- Align segment to full STFT frames to avoid shape drift between chunks ----
    // frames = 1 + floor((segment_samples - n_fft)/n_hop)
    int frames_per_chunk = 1 + static_cast<int>(std::floor((segment_samples - n_fft) / static_cast<double>(n_hop)));
    if (frames_per_chunk <= 0) {
        //g_warning("loadStftOverlap: frames_per_chunk computed as %d; params may be incompatible.", frames_per_chunk);
        return {};
    }
    // effective samples that exactly fit frames_per_chunk:
    int effective_segment_samples = (frames_per_chunk - 1) * n_hop + n_fft;

    // Advance in STFT-frames for chunk stitching.
    // Use a rounded value to reflect the *spectrogram* hop (not sample hop truncation).
    const double frames_advance_f = hop_samples / static_cast<double>(n_hop);
    int hop_frames = static_cast<int>(std::round(frames_advance_f));
    if (hop_frames <= 0) {
        hop_frames = 1; // minimum 1 frame of advance
        //g_warning("loadStftOverlap: hop_frames rounded to 0, forcing to 1.");
    }

    //g_message("[STFT] segSamples=%d (aligned=%d) hopSamples=%d  frames/chunk=%d  hopFrames=%d  totalSamples=%d",
    //            segment_samples, effective_segment_samples, hop_samples, frames_per_chunk, hop_frames, total_samples);

    // ---- Chunk loop ----
    std::vector<Matrixf> chunks;
    chunks.reserve(std::max(1, total_samples / std::max(1, hop_samples)));

    for (int start = 0; start + effective_segment_samples <= total_samples; start += hop_samples) {
        const int end = start + effective_segment_samples;
        // Slice without extra allocations if you have a view type; otherwise copy:
        std::vector<float> segment_data(data.begin() + start, data.begin() + end);

        Matrixf stftMagnitude = internal_tools::stftMagnitude(
            segment_data, n_fft, n_hop, "hann", false, "reflect"
            );

        // Basic shape sanity: expect rows=bins, cols=frames_per_chunk (or close if padding differs)
        if (stftMagnitude.cols() != frames_per_chunk) {
            //g_warning("Chunk shape mismatch: cols=%d expected=%d (start=%d end=%d)",
            //          (int)stftMagnitude.cols(), frames_per_chunk, start, end);
        }

        // dB pipeline with clamping to avoid NaNs
        stftMagnitude = internal_tools::power_to_db(stftMagnitude);    // may yield -inf
        // Clamp dB floor e.g. [-80, 0]
        const float DB_FLOOR = -80.0f;
        for (int r = 0; r < stftMagnitude.rows(); ++r)
            for (int c = 0; c < stftMagnitude.cols(); ++c)
                stftMagnitude(r, c) = std::max(DB_FLOOR, stftMagnitude(r, c));

        stftMagnitude = internal_tools::db_to_unit(stftMagnitude);

        // Replace NaN/Inf with 0
        for (int r = 0; r < stftMagnitude.rows(); ++r) {
            for (int c = 0; c < stftMagnitude.cols(); ++c) {
                float v = stftMagnitude(r, c);
                if (!(v == v) || !std::isfinite(v)) stftMagnitude(r, c) = 0.0f;
            }
        }

        // Record
        if (!chunks.empty() && chunks.back().rows() != stftMagnitude.rows()) {
            //g_error("Inconsistent bin count across chunks: prev=%d this=%d",
            //        (int)chunks.back().rows(), (int)stftMagnitude.rows());
        }
        chunks.push_back(std::move(stftMagnitude));
    }

    if (chunks.empty()) {
        //g_warning("loadStftOverlap: produced 0 chunks (audio too short?).");
        return {};
    }

    // ---- Combine ----
    Matrixf magnitudes = internal_tools::combineSpectrogramChunks(chunks, hop_frames);
    // Sanity check combined matrix
    if (magnitudes.rows() <= 0 || magnitudes.cols() <= 0) {
        //g_error("combineSpectrogramChunks returned empty matrix.");
        return {};
    }

    // ---- Flatten (row-major) ----
    const int height = static_cast<int>(magnitudes.rows());
    const int width  = static_cast<int>(magnitudes.cols());
    std::vector<float> normalizedData;
    normalizedData.reserve(width * height);
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c)
            normalizedData.push_back(magnitudes(r, c));

    // ---- Build tile (guard tile shape) ----
    Spectrogram tile_spec = internal_tools::buildTile(magnitudes);
    if ((int)tile_spec.data.size() != tile_spec.width * tile_spec.height) {
        //g_error("buildTile returned invalid buffer: w=%d h=%d size=%zu", tile_spec.width, tile_spec.height, tile_spec.data.size());
        return {};
    }

    Spectrogram spec = { width, height, normalizedData };

    SpectrogramTileOverlap tile = {
        Spectrogram{spec.width, spec.height, spec.data},
        tile_spec.width,
        tile_spec.height,
        hop_frames,
        (int)chunks[0].cols(),   // frames_per_chunk (not assumed—read actual)
        tile_spec.data
    };

    //g_message("[STFT] OUT: spec=%dx%d  tile=%dx%d  hopFrames=%d  framesPerChunk=%d", spec.width, spec.height, tile_spec.width, tile_spec.height, hop_frames, (int)chunks[0].cols());

    return tile;
}

SpectrogramTileOverlap tools::loadMelOverlap(const std::vector<float>& data,
                                             const int& sr,
                                             int n_fft, int n_hop,
                                             int n_mels, float fmin, float fmax,
                                             float segment_sec, float overlap_ratio, bool to_unit)
{
    // ---------- Basic guards ----------
    if (sr <= 0 || n_fft <= 0 || n_hop <= 0 || n_mels <= 0 || segment_sec <= 0.0f) {
        //g_warning("loadMelOverlap: invalid params sr=%d n_fft=%d n_hop=%d n_mels=%d segment=%.3f",
        //          sr, n_fft, n_hop, n_mels, segment_sec);
        return {};
    }
    if (fmin < 0.0f) fmin = 0.0f;
    const float nyquist = 0.5f * sr;
    if (fmax <= 0.0f || fmax > nyquist) fmax = nyquist;
    if (overlap_ratio < 0.0f) overlap_ratio = 0.0f;
    if (overlap_ratio >= 1.0f) overlap_ratio = 0.99f;

    const int total_samples = static_cast<int>(data.size());
    int segment_samples = static_cast<int>(std::floor(sr * segment_sec));
    if (segment_samples < n_fft) {
        //g_warning("loadMelOverlap: segment_samples=%d < n_fft=%d; no frames possible.", segment_samples, n_fft);
        return {};
    }

    // Advance in samples; make sure it’s >=1 to avoid infinite loop
    // int hop_samples = static_cast<int>(std::round(segment_samples * (1.0f - overlap_ratio)));
    int hop_samples = (int)std::round(segment_samples * (1.0f - overlap_ratio));
    // int hop_samples = std::max(1, (int)std::floor(segment_samples * (1.0f - overlap_ratio)));
    if (hop_samples <= 0) { hop_samples = 1; //g_warning("loadMelOverlap: hop_samples<=0, forcing 1");
    }

    // Precompute frames-per-chunk with center=false math (consistent with STFT call below)
    // int frames_per_chunk = 1 + static_cast<int>(std::floor((segment_samples - n_fft) / static_cast<double>(n_hop)));
    int frames_per_chunk = 1 + static_cast<int>(std::floor(segment_samples / static_cast<double>(n_hop)));
    if (frames_per_chunk <= 0) {
        //g_warning("loadMelOverlap: frames_per_chunk=%d (segment=%d, n_fft=%d, n_hop=%d).",
        //          frames_per_chunk, segment_samples, n_fft, n_hop);
        return {};
    }
    // Align segment length to an integer number of frames (prevents drift)
    // const int effective_segment_samples = (frames_per_chunk - 1) * n_hop + n_fft;
    const int effective_segment_samples = segment_samples;

    // Advance in spectrogram frames (rounded), never 0
    int hop_frames = static_cast<int>(std::lround(hop_samples / static_cast<double>(n_hop)));
    if (hop_frames <= 0) { hop_frames = 1;
        //g_warning("loadMelOverlap: hop_frames<=0, forcing 1");
    }

    //g_message("[MEL] seg=%d (aligned=%d) hop_samples=%d frames/chunk=%d hop_frames=%d total=%d fmin=%.1f fmax=%.1f",
    //          segment_samples, effective_segment_samples, hop_samples, frames_per_chunk, hop_frames,
    //          total_samples, fmin, fmax);

    // ---------- Mel filterbank ----------
    // Expect shape: (n_mels, n_fft/2 + 1)
    Matrixf mel_filterbank = internal_tools::create_mel_filterbank(sr, n_fft, n_mels, fmin, fmax, /*htk=*/false);
    const int mel_rows = static_cast<int>(mel_filterbank.rows());
    const int mel_cols = static_cast<int>(mel_filterbank.cols());
    const int stft_bins_expected = n_fft / 2 + 1;
    if (mel_rows != n_mels || mel_cols != stft_bins_expected) {
        //g_error("Mel FB shape mismatch: got %dx%d expected %dx%d (n_mels x bins).",
        //        mel_rows, mel_cols, n_mels, stft_bins_expected);
        return {};
    }

    // ---------- Chunk loop ----------
    std::vector<Matrixf> chunks;
    std::vector<std::pair<int,int>> spans;
    // build spans with snapped tail
    {
        int start = 0;
        while (start + effective_segment_samples <= total_samples) {
            spans.emplace_back(start, start + effective_segment_samples);
            start += hop_samples;
        }
        const int last_start = total_samples - effective_segment_samples;
        if (spans.empty() || spans.back().first < last_start) {
            spans.emplace_back(last_start, last_start + effective_segment_samples);
        }
    }

    chunks.reserve((int)spans.size());

    int locked_bins = -1;
    int locked_frames = -1;

    for (const auto& se : spans) {
        const int start = se.first;
        const int end   = se.second;

        std::vector<float> segment_data(data.begin() + start, data.begin() + end);

        // keep center=true, it matches frames_per_chunk = 1 + floor(segment/n_hop)
        Matrixf S = internal_tools::stftMagnitude(segment_data, n_fft, n_hop, "hann", /*center=*/true, "reflect");

        if (S.rows() != stft_bins_expected) {
            //g_error("STFT bins mismatch: got rows=%d expected=%d", (int)S.rows(), stft_bins_expected);
            return {};
        }
        if (locked_frames < 0) locked_frames = (int)S.cols();
        if ((int)S.cols() != locked_frames) {
            //g_warning("Chunk frame mismatch: cols=%d expected=%d (start=%d end=%d)",
            //          (int)S.cols(), locked_frames, start, end);
        }

        Matrixf M = mel_filterbank * S;
        M = internal_tools::power_to_db(M);

        const float DB_FLOOR = -80.0f;
        for (int r = 0; r < M.rows(); ++r)
            for (int c = 0; c < M.cols(); ++c) {
                float v = M(r, c);
                if (!std::isfinite(v)) v = DB_FLOOR;
                M(r, c) = std::max(DB_FLOOR, v);
            }
        if (to_unit) {
            M = internal_tools::db_to_unit(M);
        }

        if (locked_bins < 0) locked_bins = (int)M.rows();
        if ((int)M.rows() != locked_bins) {
            //g_error("Mel chunk bins mismatch: got=%d expected=%d", (int)M.rows(), locked_bins);
            return {};
        }

        chunks.push_back(std::move(M));
    }

    // sanity: predicted width using constant stride placement
    int N = (int)spans.size();
    int predicted = locked_frames + (N - 1) * hop_frames;
    //g_message("[MEL] chunks=%d framesPerChunk=%d hopFrames=%d predictedWidth=%d",
    //          N, locked_frames, hop_frames, predicted);

    if (chunks.empty()) {
        //g_warning("loadMelOverlap: produced 0 chunks (audio too short?)");
        return {};
    }

    // ---------- Combine ----------
    Matrixf magnitudes = internal_tools::combineSpectrogramChunks(chunks, hop_frames);
    if (magnitudes.rows() <= 0 || magnitudes.cols() <= 0) {
        //g_error("combineSpectrogramChunks returned empty matrix.");
        return {};
    }

    // ---------- Flatten ----------
    const int height = (int)magnitudes.rows();
    const int width  = (int)magnitudes.cols();
    std::vector<float> normalizedData;
    normalizedData.reserve(width * height);
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c)
            normalizedData.push_back(magnitudes(r, c));

    // ---------- Build tile ----------
    Spectrogram tile_spec = internal_tools::buildTile(magnitudes);
    if ((int)tile_spec.data.size() != tile_spec.width * tile_spec.height) {
        //g_error("buildTile invalid buffer: w=%d h=%d size=%zu",
        //        tile_spec.width, tile_spec.height, tile_spec.data.size());
        return {};
    }

    Spectrogram spec = { width, height, normalizedData };
    const int frames_per_chunk_actual = (int)chunks[0].cols(); // ground truth

    //g_message("[MEL] OUT spec=%dx%d tile=%dx%d hopFrames=%d framesPerChunk=%d",
    //          spec.width, spec.height, tile_spec.width, tile_spec.height,
    //          hop_frames, frames_per_chunk_actual);

    SpectrogramTileOverlap tile = {
        Spectrogram{spec.width, spec.height, spec.data},
        tile_spec.width,
        tile_spec.height,
        hop_frames,
        frames_per_chunk_actual,
        tile_spec.data
    };
    return tile;
}

}
