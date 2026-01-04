#ifndef SPEC_API_H
#define SPEC_API_H
#pragma once

#include <vector>
#include <string>

class SpecApi {
public:
    struct Response {
        long http_code = 0;
        std::string body;
        std::string error;
        explicit operator bool() const { return http_code >= 200 && http_code < 300; }
    };

    static Response uploadFile(const std::string& url,
                               const std::string& file_path,
                               const std::string& user,
                               long connect_timeout_ms = 5000,
                               long total_timeout_ms = 120000);

    static Response uploadMemory(const std::string& url,
                                 const void* data, size_t size,
                                 const std::string& filename,
                                 const std::string& content_type,
                                 const std::string& user,
                                 long connect_timeout_ms = 5000,
                                 long total_timeout_ms = 120000);

    static Response uploadSpectrogramPNG(const std::string& url,
                                         const std::vector<uint8_t>& data, const int& width, const int& height,
                                         const std::string& user,const std::string& filename, long connect_timeout_ms = 5000,
                                         long total_timeout_ms = 120000);
};

#endif //SPEC_API_H
