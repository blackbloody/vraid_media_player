#include "tools.h"
#include "define.h"
#include "internal_tools.h"

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace raiden {

SpectrogramByte tools::flatMatrixToByteImg(const std::vector<float>& flat, int height, int width, const std::string &file_name, bool is_db) {

    Eigen::MatrixXf magnitude = internal_tools::fromFlatToMatrix(flat, height, width);

    // std::cout << "raw min/max: "
    //           << magnitude.minCoeff() << " / "
    //           << magnitude.maxCoeff() << std::endl;

    if (is_db) {
        const float DB_FLOOR = -80.0f;
        // map [-80,0] -> [0,1]
        magnitude = (magnitude.array() - DB_FLOOR) / -DB_FLOOR;
        magnitude = magnitude.cwiseMax(0.0f).cwiseMin(1.0f);
    }

    // Wrap Eigen matrix data into OpenCV Mat without deep copy
    cv::Mat matFloat(static_cast<int>(magnitude.rows()), static_cast<int>(magnitude.cols()), CV_32F);
    for (int r = 0; r < matFloat.rows; ++r)
        for (int c = 0; c < matFloat.cols; ++c)
            matFloat.at<float>(r, c) = magnitude(r, c);

    // Skip scaling if not needed
    int scaleX = 1;  // time axis
    int scaleY = 1;  // frequency axis

    cv::Mat resized;
    if (scaleX != 1 || scaleY != 1) {
        cv::resize(matFloat, resized, cv::Size(), scaleX, scaleY, cv::INTER_AREA);
    } else {
        resized = matFloat; // no scaling needed
    }

    // Convert float [0,1] → byte [0,255]
    cv::Mat byteImg;
    resized.convertTo(byteImg, CV_8U, 255.0);

    // Flip vertically if required
    cv::Mat flipped;
    cv::flip(byteImg, flipped, 0);  // can skip if not needed in memory

    // Optional: save test image (only during testing)
    if (!file_name.empty()) {
        cv::imwrite("/media/virus/Goblin/img" + file_name + ".png", flipped);
    }

    // Copy to std::vector<uint8_t>
    std::vector<uint8_t> data(flipped.data, flipped.data + flipped.total());

    return {flipped.cols, flipped.rows, data};
}

std::vector<float> tools::extractSpectrogramSlice(const std::vector<float> &full_flat, int full_width, int height, int start_frame, int frame_size) {
    std::vector<float> slice_flat(height * frame_size, 0.0f); // pre-filled with 0 for padding
    // std::vector<float> slice_flat; // pre-filled with 0 for padding

    for (int row = 0; row < height; ++row) {
        int src_row_offset = row * full_width;
        int dst_row_offset = row * frame_size;

        std::memcpy(
            &slice_flat[dst_row_offset],
            &full_flat[src_row_offset + start_frame],
            sizeof(float) * frame_size
            );
        // remaining values already zero (due to pre-fill)
    }

    return slice_flat;
}

}
