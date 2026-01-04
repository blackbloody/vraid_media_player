#include "wav_reader.h"
#include <sstream>
#include <iomanip>

namespace wav_reader {

    std::string WavReader::convertToASCII(std::string hex) {
        std::string ascii;
        for (size_t i = 0; i < hex.length(); i += 2) {
            //taking two characters from hex string
            std::string part = hex.substr(i, 2);
            //changing it into base 16
            char ch = std::stoul(part, nullptr, 16);
            //putting it into the ASCII string
            ascii += ch;
        }
        return ascii;
    }

    size_t WavReader::convertToDecimal(std::string hex) {
        long result;
        std::stringstream ss(hex);
        ss >> std::hex >> result;
        return (size_t)result;
    }

    std::string WavReader::getHexOnBufferByte(uint8_t *data, size_t &offset, const size_t &limit, const size_t &length, bool isLittleEndian) {
        std::string tempHex;
        for (; offset < limit; offset++) {
            std::stringstream sss;
            sss << std::hex << std::setfill('0');
            sss << std::hex << std::setw(2) << static_cast<long>(data[offset]);
            tempHex += sss.str();
        }
        if (isLittleEndian) {
            std::string b[length];
            std::string tempByteHex;

            size_t indexHex = 1;
            size_t indexStarter = length - 1;
            for (char i : tempHex) {
                tempByteHex += i;
                if (indexHex < 2)
                    indexHex++;
                else {
                    b[indexStarter] = tempByteHex;
                    indexStarter--;
                    tempByteHex = "";
                    indexHex = 1;
                }
            }
            tempHex = "";
            for (size_t i = 0; i < length; i++)
                tempHex += b[i];
        }
        return tempHex;
    }

    size_t WavReader::findOffsetDataHeader(uint8_t *data, size_t &offset, const size_t &indicator_reduce) {
        size_t valBeforeData = offset;
        std::string tempVal;
        while (offset < 1000) {
            tempVal = "";
            offset -= indicator_reduce;
            tempVal = getHexOnBufferByte(data, offset, offset + 4, 4, false);
            if (tempVal == "64617461" || tempVal == "44415441") {
                break;
            }
        }
        offset -= 4;
        std::string dataChunkID = convertToASCII(getHexOnBufferByte(data, offset, offset + 4, 4, false));
        if (dataChunkID == "data" || dataChunkID == "DATA")
            return offset - 4;
        else
            return 0 + findOffsetDataHeader(data, valBeforeData, 3);
    }

    bool WavReader::isBigEndian() {
        uint16_t x = 1;
        return reinterpret_cast<uint8_t*>(&x)[0] == 0;
    }

}
