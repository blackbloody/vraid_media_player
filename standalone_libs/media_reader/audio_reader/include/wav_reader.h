
#include "media.h"
#include <string>
#include <functional>

namespace wav_reader {
class WavReader {
public:
    WavReader();
    ~WavReader();
    void readFile(const std::string& file_path, const std::function<void(MediaObj::Audio)>& wav_callback);
    inline MediaObj::Audio getWavObj() const { return audio_obj; }
private:
    MediaObj::Audio audio_obj{};

    static std::string convertToASCII(std::string hex);
    static size_t convertToDecimal(std::string hex);
    static std::string getHexOnBufferByte(uint8_t *data, size_t &offset, const size_t &limit, const size_t &length,
                                   bool isLittleEndian);
    static size_t findOffsetDataHeader(uint8_t *data, size_t &offset, const size_t &indicator_reduce);
    static bool isBigEndian();
};
}
