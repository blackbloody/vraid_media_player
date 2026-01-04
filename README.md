# Media Spectrogram Player (Qt + OpenGL)

A desktop media player + audio analysis viewer.  
It plays video while showing an aligned **mel spectrogram** timeline so you can **scrub**, **inspect**, and (eventually) **label** audio events with precision.

<img width="1553" height="894" alt="image" src="https://github.com/user-attachments/assets/c437e15f-e490-4676-816d-993d157f298c" />

## What it does (current UI)
- **Video preview panel** (top) rendered via OpenGL texture update
- **Playback controls**: play/pause button + timeline slider
- **Audio visualization panel** (bottom): **mel spectrogram** with time ticks (e.g. 95.5s → 99.5s)
- **View mode selector** (e.g. “Mel Spectrogram” dropdown)

## Why this exists
Most media players don’t help you *see* audio structure. This tool is meant for:
- audio inspection (speech/music/noise patterns)
- dataset review
- QA/debugging audio decode/sync
- future: labeling/annotation and ML-assisted event detection

## Tech stack
- **Qt 5** (Widgets + `.ui` via Qt Designer)
- **OpenGL** rendering (video frame texture upload)
- Media decode: **FFmpeg** (video/audio)
- Audio output (Linux): **ALSA** (or your backend of choice)
- Spectrogram: STFT → mel filterbank → dB scale (implementation may vary)

> Note: exact dependencies may differ depending on your build setup—adjust the list below to match your repo.

## Build (Linux)
### Dependencies (typical)
- Qt5: `qtbase5-dev`, `qttools5-dev`, `qttools5-dev-tools`
- Build tools: `cmake`, `g++`, `pkg-config`
- FFmpeg dev: `libavformat-dev`, `libavcodec-dev`, `libswscale-dev`, `libswresample-dev`, `libavutil-dev`
- ALSA dev: `libasound2-dev`
- OpenGL loader (optional): `glad` / `glew` depending on your project
