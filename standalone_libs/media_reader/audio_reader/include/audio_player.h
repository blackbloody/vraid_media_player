#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H
#pragma once
#include "media.h"
#include <functional>
#include <atomic>
#include <mutex>

namespace audio_player {
class AudioThroughAccessPlayer {
public:
    void onPause();
    void onPlayWavForTimeStamp(MediaObj::Audio audio_obj, const float &start_sec, const float &end_sec,
                                   const std::function<void(const int &len_sample_buffer, const float &time_stamp_buffer)> &play_callback);

private:
    std::mutex mtx_player_;
    std::atomic<bool> playing_{false};
    static bool isBigEndian() {
        uint16_t x = 1;
        return reinterpret_cast<uint8_t*>(&x)[0] == 0;
    }

    static uint8_t* extractWavRawBytes(const std::string& filePath,
                                       size_t offset_first_sample,
                                       int bit_per_sample,
                                       int num_channels,
                                       int start_sample,
                                       int num_samples,
                                       size_t& out_size);
};
}

#endif // AUDIO_PLAYER_H
