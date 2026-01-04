#ifndef MEDIA_OBJ_H
#define MEDIA_OBJ_H

#include <string>
#include <vector>

namespace MediaObj {

    struct Vid {
        std::string path = "";
        int width, height;
        int codec_type;
        std::string codec_name;
        double fps;
        int preferred_rate;
        int playback_frame_rate;
        double preview_time;
        int preview_duration;
        double poster_time;
        int bit_depth;
        double avg_bitrate;
        int frame_rate;
        std::string encoder;
        double megapixels;
        int rotation;

        std::vector<uint8_t> thumnail;
    };

    struct Audio {
        std::string path = "";
        std::string chunk_id = "";
        size_t chunk_size = 0;
        std::string chunk_format = "";

        std::string sub_chunk_id = "";
        size_t sub_chunk_size = 0;
        size_t audio_format = 3;

        int channel = 0;
        int sample_rate = 0;
        size_t byte_rate = 0;
        int block_align = 0;
        size_t bit_per_sample = 0;
        int data_chunk_size = 0;

        size_t offset_first_sample = 0;
        std::vector<float> data;

        std::vector<float> amplitude_time_series;
        std::vector<float> amplitude_envelope;
        std::vector<float> root_mean_square;
        std::vector<float> zero_crossing_rate;

        long num_sample() {
            if (data_chunk_size != 0 && block_align != 0) {
                long rtn = data_chunk_size / block_align;
                return rtn;
            }
            else
                return 0;
        }

        long num_sample_per_second() {
            if (sample_rate != 0 && channel != 0) {
                long rtn = sample_rate * channel;
                return rtn;
            }
            else
                return 0;
        }
    };

    struct MdlAudio {
        int track;
        float start_sec;
        int start;
        int end;
        std::vector<float> data;
    };

}

#endif // MEDIA_OBJ_H
