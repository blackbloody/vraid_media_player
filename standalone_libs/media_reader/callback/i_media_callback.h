#ifndef IMEDIACALLBACK_H
#define IMEDIACALLBACK_H
#pragma once

#include <sigc++/sigc++.h>
#include <unordered_set>
#include "i_media_sink.h"
#include "media.h"

#include <iostream>

class IMediaCallback {
private:
    std::unordered_set<IMediaSinkCallback*> sinks;
public:
    void register_sink(IMediaSinkCallback* sink) {
        sinks.insert(sink);
    }
    void unregister_sink(IMediaSinkCallback* sink) {
        sinks.erase(sink);
    }

    //////////////////

    void publish_audio(MediaObj::Audio audio) {
        for (auto sink : sinks) {
            sink->on_receive_media_audio(audio);
        }
    }

    void publish_media(MediaObj::Vid vid, MediaObj::Audio audio) {
        //std::cout << "Sink V: " << std::to_string(audio.sample_rate).c_str();
        for (auto sink : sinks) {
            sink->on_receive_media(vid, audio);
        }
    }

    void update_played_audio(double sec) {
        for (auto sink : sinks) {
            sink->on_played_sec(sec);
        }
    }

};

#endif // IMEDIACALLBACK_H
