# 🎶 Musulizer – Music Visualizer

![raylib](https://img.shields.io/badge/raylib-5.0-blue.svg)
![C++](https://img.shields.io/badge/C++-20-orange.svg)
![FFmpeg](https://img.shields.io/badge/optional-FFmpeg-green.svg)

Musulizer is a **real-time audio visualizer** built with [raylib](https://www.raylib.com/), [miniaudio](https://miniaud.io/), and optionally [FFmpeg](https://ffmpeg.org/).
It supports **drag-and-drop music playback**, **FFT-based visualization**, and **microphone input capture**.
With FFmpeg enabled, you can also **render music visualizations to video**.

---

## ✨ Features

* 🎵 Play music files (drag & drop into the window)
* 📜 Playlist with scrolling and selection
* ⏯️ Play / Pause / Seek using the tracker
* 🔊 Volume control with mute/unmute toggle
* 🎙️ Microphone input visualization (press **C**)
* 🖥️ Fullscreen toggle (**F**)
* 🎥 (Optional) Render visualizer to video using **FFmpeg** (**R**)
* 🔮 Shader-based effects for smoother visuals

---

## 📦 Dependencies

* [raylib 5.0+](https://www.raylib.com/)
* [miniaudio](https://github.com/mackron/miniaudio) (included as `miniaudio.c`)
* C++20 compatible compiler
* [FFmpeg](https://ffmpeg.org/) (optional, required if compiled with `-DFFMPEG_ENABLE`)

---

## ⚙️ Build Instructions

### Windows (MinGW / MSYS2)

```sh
g++ main.cpp -Wall -Wextra -Wpedantic -std=c++20 \
    -Iraylib/include -Lraylib/lib \
    -lraylib -lgdi32 -lwinmm \
    -o musulizer.exe
```

### Linux

```sh
g++ main.cpp -Wall -Wextra -Wpedantic -std=c++20 \
    -Iraylib/include -Lraylib/lib \
    -lraylib -lm -ldl -lpthread -lGL -lX11 \
    -o musulizer
```

### With FFmpeg

```sh
g++ main.cpp -Wall -Wextra -Wpedantic -std=c++20 \
    -DFFMPEG_ENABLE \
    -Iraylib/include -Lraylib/lib \
    -lraylib -lavformat -lavcodec -lavutil -lswscale \
    -o musulizer
```

---

## 🎮 Controls

| Key       | Action                                             |
| --------- | -------------------------------------------------- |
| **Space** | Play / Pause current track                         |
| **F**     | Toggle fullscreen                                  |
| **C**     | Toggle microphone capture                          |
| **R**     | Start video rendering (FFmpeg, if enabled)         |
| **Mouse** | Interact with playlist, tracker, and volume slider |

---

## 📂 Assets

This project expects resources under `resources/`:

```
resources/
 ├── icons/
 │   ├── play.png
 │   ├── pause.png
 │   ├── volumehigh.png
 │   ├── mute.png
 │   └── images.png
 └── shaders/
     └── glsl330/
         ├── circle.fs
         └── circle_web.fs
```

---

## 🚀 Usage

1. Launch the application.
2. Drag & drop audio files into the window.
3. Use the playlist on the left to select tracks.
4. Adjust playback with the tracker and volume slider.
5. (Optional) Press **R** to record visualization to video (requires FFmpeg).


 * NOTE: The idea for this implementation was inspired by tsoding's musializer project:
 * https://github.com/tsoding/musializer.git