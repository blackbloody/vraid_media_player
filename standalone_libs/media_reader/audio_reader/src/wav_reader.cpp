#include "wav_reader.h"
#include <chrono>

// for memory mapping
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>

namespace wav_reader {
    WavReader::WavReader() {
    }

    WavReader::~WavReader() {}

    void WavReader::readFile(const std::string& file_path, const std::function<void(MediaObj::Audio)>& wav_callback) {

        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        const char* file_str = file_path.c_str();
        int fd = -1;
        struct stat fileInfo{};
        uint8_t* data;

        fd = open(file_str, O_RDONLY);
        posix_fadvise(fd, 0, 0, 1);
        fstat(fd, &fileInfo);
        data = static_cast<uint8_t*>(mmap(nullptr, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0));
        size_t length = fileInfo.st_size;
        size_t offset = 0;

        MediaObj::Audio obj = {};
        if (length > 0) {
            obj.path = file_str;
            // ------ WAV Structure
            obj.chunk_id = convertToASCII(getHexOnBufferByte(data, offset, 4, 4, false));
            obj.chunk_size = convertToDecimal(getHexOnBufferByte(data, offset, offset + 4, 4, true));
            obj.chunk_format = convertToASCII(getHexOnBufferByte(data, offset, offset + 4, 4, false));
            // ------ Format SubChunk
            obj.sub_chunk_id = convertToASCII(getHexOnBufferByte(data, offset, offset + 4, 4, false));
            obj.sub_chunk_size = convertToDecimal(getHexOnBufferByte(data, offset, offset + 4, 4, true));
            // obj.audio_format = std::to_string(convertToDecimal(getHexOnBufferByte(data, offset, offset + 2, 2, true)));

            obj.audio_format = static_cast<int>(convertToDecimal(getHexOnBufferByte(data, offset, offset + 2, 2, true)));
            // 1 → PCM (integer)
            // 3 → IEEE Float
            // 65534 → WAVE_FORMAT_EXTENSIBLE (more complex)

            obj.channel = static_cast<int>(convertToDecimal(getHexOnBufferByte(data, offset, offset + 2, 2, true)));
            obj.sample_rate = static_cast<int>(convertToDecimal(getHexOnBufferByte(data, offset, offset + 4, 4, true)));
            obj.byte_rate = static_cast<int>(convertToDecimal(getHexOnBufferByte(data, offset, offset + 4, 4, true)));
            obj.block_align = static_cast<int>(convertToDecimal(getHexOnBufferByte(data, offset, offset + 2, 2, true)));
            obj.bit_per_sample = static_cast<int>(convertToDecimal(getHexOnBufferByte(data, offset, offset + 2, 2, true)));
            // -------- Find offset Data
            offset = findOffsetDataHeader(data, offset, 0);
            std::string dataChunkID = convertToASCII(getHexOnBufferByte(data, offset, offset + 4, 4, false));
            obj.data_chunk_size = static_cast<int>(convertToDecimal(getHexOnBufferByte(data, offset, offset + 4, 4, true)));
            obj.offset_first_sample = offset;
        }

        // Clean up: Unmap the memory and close the file
        if (munmap(data, fileInfo.st_size) == -1)
            munmap(data, fileInfo.st_size);
        close(fd);

        auto end = std::chrono::steady_clock::now();
        long millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::string msg = "Read duration: " + std::to_string(static_cast<float>(millis) / 1000.0f) +
                          " seconds (" + std::to_string(millis) + "ms)";
        std::cout << msg << std::endl;

        this->audio_obj = obj;
        wav_callback(obj);


    }
}
