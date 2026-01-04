#include "upload_spec_api.h"
#include <curl/curl.h>
#include <cstdio>
#include <mutex>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {
size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    const size_t n = size * nmemb;
    static_cast<std::string*>(userdata)->append(ptr, n);
    return n;
}
void curl_global_once() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

// Encode 8-bit gray to PNG in-memory using stb
// Returns empty vector on failure.
std::vector<uint8_t> encode_png_gray8(const uint8_t* gray, int w, int h) {
    if (!gray || w <= 0 || h <= 0) return {};
    int out_len = 0;
    // comp=1 (gray), stride = w bytes
    unsigned char* out = stbi_write_png_to_mem(gray, w, w, h, 1, &out_len);
    if (!out || out_len <= 0) return {};
    std::vector<uint8_t> png(out, out + out_len);
    STBIW_FREE(out);
    return png;
}

}

SpecApi::Response SpecApi::uploadFile(const std::string &url,
                                              const std::string &file_path,
                                              const std::string &user,
                                              long connect_timeout_ms,
                                              long total_timeout_ms) {

    curl_global_once();
    Response out;

    // quick local check to fail fast if the path is bad
    {
        FILE* f = std::fopen(file_path.c_str(), "rb");
        if (!f) {
            out.error = "File not found: " + file_path;
            return out;
        }
        std::fclose(f);
    }

    CURL* curl = curl_easy_init();
    if (!curl) { out.error = "curl_easy_init failed"; return out; }

    char errbuf[CURL_ERROR_SIZE] = {0};
    std::string resp;

    // Avoid 100-continue extra RTT
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Expect:");

    // Build multipart body
    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_filedata(part, file_path.c_str()); // libcurl sets filename & type

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "user");
    curl_mime_data(part, user.c_str(), CURL_ZERO_TERMINATED);

    // Basic options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    // Reasonable LAN timeouts; NOSIGNAL avoids SIGALRM in threads
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,        total_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    // Perform
    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.http_code);

    if (rc != CURLE_OK) {
        out.error = *errbuf ? std::string(errbuf) : curl_easy_strerror(rc);
    } else {
        out.body = resp;
        if (!(out.http_code >= 200 && out.http_code < 300)) {
            out.error = "HTTP " + std::to_string(out.http_code);
        }
    }

    // Cleanup
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return out;

}

SpecApi::Response SpecApi::uploadMemory(const std::string &url,
                                                const void *data, size_t size, const std::string &filename,
                                                const std::string &content_type, const std::string &user, long connect_timeout_ms, long total_timeout_ms) {

    curl_global_once();
    Response out;
    if (!data || size == 0) { out.error = "empty buffer"; return out; }

    CURL* curl = curl_easy_init();
    if (!curl) { out.error = "curl_easy_init failed"; return out; }

    char errbuf[CURL_ERROR_SIZE] = {0};
    std::string resp;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Expect:");

    curl_mime* mime = curl_mime_init(curl);
    // file part from memory
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_data(part, static_cast<const char*>(data), size);
    curl_mime_filename(part, filename.c_str());
    if (!content_type.empty()) curl_mime_type(part, content_type.c_str());

    // user part
    part = curl_mime_addpart(mime);
    curl_mime_name(part, "user");
    curl_mime_data(part, user.c_str(), CURL_ZERO_TERMINATED);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,        total_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.http_code);
    if (rc != CURLE_OK) {
        // std::cout << curl_easy_strerror(rc) << std::endl;
        out.error = *errbuf ? errbuf : curl_easy_strerror(rc);
    } else {
        out.body = resp;
        if (!(out.http_code >= 200 && out.http_code < 300)) {
            // std::string err = "HTTP " + std::to_string(out.http_code);
            out.error = "HTTP " + std::to_string(out.http_code);
        }
    }

    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return out;

}


SpecApi::Response SpecApi::uploadSpectrogramPNG(const std::string &url, const std::vector<uint8_t> &data, const int &width, const int &height, const std::string &user, const std::string &filename, long connect_timeout_ms, long total_timeout_ms) {
    Response out;
    if (width <= 0 || height <= 0) { out.error = "invalid image size"; return out; }
    const size_t need = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (data.size() != need) {
        out.error = "data size mismatch"; return out;
    }

    std::vector<uint8_t> png = encode_png_gray8(data.data(), width, height);
    if (png.empty()) { out.error = "PNG encode failed"; return out; }

    return uploadMemory(url, png.data(), png.size(), filename, "image/png",
                        user, connect_timeout_ms, total_timeout_ms);
}
