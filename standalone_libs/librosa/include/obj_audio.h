#ifndef OBJ_AUDIO_H
#define OBJ_AUDIO_H

#include <cstdint>
#include <vector>

struct Signal {
    std::vector<float> data;
    int sr;
};

struct SignalAdaptGl {
    std::vector<float> adapt_audio_wave;
    int gl_draw_count;
};

struct SpecViewPort {
    float  start_sec, end_sec;
    int    start_sample, end_sample;
    int    frame_start, frame_size;
};

struct SpecViewPortNDC {
    float ndc_start, ndc_end;
    SpecViewPort base;
};

// tools
struct Spectrogram {
    int width;
    int height;
    std::vector<float> data;
};

struct SpectrogramTile {
    Spectrogram spectrogram;
    int width;
    int height;
    std::vector<float> data;
};

struct SpectrogramTileOverlap {
    Spectrogram spectrogram;
    int width;
    int height;
    // int segment_samples;
    int hop_frames;
    int framesPerChunk;
    std::vector<float> data;
};

struct SpectrogramByte {
    int width;
    int height;
    std::vector<uint8_t> data;
};

// View Mode
enum class ViewSignalDataMode {
    WaveForm,
    Mel_Spectrogram,
    STFT
};


#endif // OBJ_AUDIO_H
