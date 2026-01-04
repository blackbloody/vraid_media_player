#include "audio_player.h"

#include <cmath>
#include <alsa/asoundlib.h>
#define PCM_DEVICE "default"

namespace audio_player {

void AudioThroughAccessPlayer::onPause() {
    {
        std::unique_lock<std::mutex> lk(mtx_player_);
        playing_ = false;
    }
}

void AudioThroughAccessPlayer::onPlayWavForTimeStamp(MediaObj::Audio audio_obj, const float &start_sec, const float &end_sec,
                                                     const std::function<void(const int &, const float &)> &play_callback) {

    {
        std::unique_lock<std::mutex> lk(mtx_player_);
        playing_ = true;
    }

    snd_pcm_t* handler = nullptr;
    int pcm = snd_pcm_open(&handler, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (pcm < 0) {
        // g_warning("snd_pcm_open failed: %s", snd_strerror(pcm));
        play_callback(0, 0.0);
        return;
    }

    snd_pcm_format_t format = SND_PCM_FORMAT_S16;
    switch (audio_obj.bit_per_sample) {
    case 32:
        if (audio_obj.audio_format == 1)
            format = SND_PCM_FORMAT_S32_LE;
        else if (audio_obj.audio_format == 3)
            format = isBigEndian() ? SND_PCM_FORMAT_FLOAT_BE : SND_PCM_FORMAT_FLOAT_LE;
        break;
    case 16:
        format = isBigEndian() ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_S16_LE;
        break;
    case 8:
        format = SND_PCM_FORMAT_S8;
        break;
    }

    int latency = 10000;
    pcm = snd_pcm_set_params(handler, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                             audio_obj.channel, audio_obj.sample_rate, 1, latency);
    if (pcm < 0) {
        // g_warning("snd_pcm_set_params failed: %s", snd_strerror(pcm));
        snd_pcm_close(handler);
        play_callback(0, 0.0);
        return;
    }

    // Calculate time range in frames
    float actual_end_sec = (end_sec - start_sec != 0.0f)
                               ? end_sec
                               : static_cast<float>(audio_obj.num_sample()) / static_cast<float>(audio_obj.num_sample_per_second());

    int start_sample = static_cast<int>(start_sec * static_cast<float>(audio_obj.sample_rate));
    int total_samples = static_cast<int>((actual_end_sec - start_sec) * static_cast<float>(audio_obj.sample_rate));

    // Use extractRawBytes to load only the target segment
    size_t out_size = 0;
    uint8_t* segment = extractWavRawBytes(
        audio_obj.path,
        audio_obj.offset_first_sample,
        audio_obj.bit_per_sample,
        audio_obj.channel,
        start_sample,
        total_samples,
        out_size
        );

    if (!segment || out_size == 0) {
        // g_warning("Failed to extract audio bytes.");
        snd_pcm_close(handler);
        play_callback(0, 0.0);
        return;
    }

    // Playback loop setup
    const float incrementTime = 0.0116f;
    const size_t bytes_per_sample = audio_obj.bit_per_sample / 8;
    const size_t samples_per_buffer = std::ceil(audio_obj.sample_rate * incrementTime * audio_obj.channel);
    const size_t frameSize = samples_per_buffer * bytes_per_sample;

    int currentPosSample = 0;
    float start_time_stamp = 0.0f;
    size_t segment_offset = 0;

    while (segment_offset < out_size) {

        size_t remainingBytes = out_size - segment_offset;
        size_t chunkSize = std::min(frameSize, remainingBytes);

        std::vector<uint8_t> sample(chunkSize);
        memcpy(sample.data(), segment + segment_offset, chunkSize);

        if (snd_pcm_state(handler) == SND_PCM_STATE_XRUN) {
            snd_pcm_prepare(handler);
        }

        size_t bytesPerFrame = bytes_per_sample * audio_obj.channel;
        snd_pcm_uframes_t actualFrames = chunkSize / bytesPerFrame;

        snd_pcm_uframes_t frames_played = snd_pcm_writei(handler, sample.data(), actualFrames);
        if (frames_played < 0) {
            // g_warning("snd_pcm_writei failed: %s", snd_strerror(frames_played));
        }

        snd_pcm_sframes_t delay;
        snd_pcm_delay(handler, &delay);

        play_callback(static_cast<int>(samples_per_buffer), start_time_stamp += incrementTime);
        currentPosSample += static_cast<int>(samples_per_buffer);

        segment_offset += chunkSize;

        {
            std::unique_lock<std::mutex> lk(mtx_player_);
            if (!playing_) {
                break;
            }
        }
    }

    snd_pcm_drain(handler);
    snd_pcm_pause(handler, 1);
    snd_pcm_close(handler);
    snd_config_update_free_global();

    delete[] segment;
    play_callback(0, 0.0);
}

uint8_t* AudioThroughAccessPlayer::extractWavRawBytes(const std::string& filePath,
                                   size_t offset_first_sample,
                                   int bit_per_sample,
                                   int num_channels,
                                   int start_sample,
                                   int num_samples,
                                   size_t& out_size) {
    size_t bytes_per_sample = bit_per_sample / 8;
    size_t bytes_per_frame = bytes_per_sample * num_channels;

    size_t byte_start = offset_first_sample + start_sample * bytes_per_frame;
    size_t byte_length = num_samples * bytes_per_frame;

    // Allocate memory to hold the extracted bytes
    uint8_t* buffer = new uint8_t[byte_length];

    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        delete[] buffer;
        out_size = 0;
        return nullptr;
    }

    lseek(fd, byte_start, SEEK_SET);
    ssize_t bytes_read = read(fd, buffer, byte_length);
    close(fd);

    if (bytes_read < 0 || static_cast<size_t>(bytes_read) != byte_length) {
        delete[] buffer;
        out_size = 0;
        return nullptr;
    }

    out_size = byte_length;
    return buffer;
}

}
