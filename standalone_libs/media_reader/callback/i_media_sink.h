#ifndef I_MEDIA_SINK_H
#define I_MEDIA_SINK_H
#pragma once
#include "media.h"

class IMediaCallback;

struct IMediaSinkCallback {
    virtual ~IMediaSinkCallback() = default;
    virtual void on_receive_media_audio(MediaObj::Audio audio) = 0;
    virtual void on_receive_media(MediaObj::Vid vid, MediaObj::Audio audio) = 0;
    virtual void on_played_sec(double sec) = 0;
    // virtual void on_receive_media_vid(MediaObj::Video video);
    // virtual void on_receive_media(MediaObj::Video video, MediaObj::Audio audio);
};

#endif // I_MEDIA_SINK_H
