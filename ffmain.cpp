

#include <iostream>
namespace raylib {
    #include <raylib.h>
};
#include "ffmpeg.h"




#define WIDTH 800
#define HEIGHT 600
#define FPS 60
#define DURATION 10

int main(void)
{
    FFMPEG* ffmpeg = ffmpeg_start_rendering(WIDTH, HEIGHT, FPS);

    raylib::InitWindow(WIDTH, HEIGHT, "FFmpeg");
    raylib::SetTargetFPS(FPS);
    raylib::RenderTexture2D screen = raylib::LoadRenderTexture(WIDTH, HEIGHT);

    float x = WIDTH/2;
    float y = HEIGHT/2;
    float r = HEIGHT/8;
    float dx = 200;
    float dy = 200;
    float dt = 1.0f/FPS;
    for (size_t i = 0; i < FPS*DURATION && !raylib::WindowShouldClose(); ++i) {
        float nx = x + dx*dt;
        if (0 < nx - r && nx + r < WIDTH) {
            x = nx;
        } else {
            dx = -dx;
        }

        float ny = y + dy*dt;
        if (0 < ny - r && ny + r < HEIGHT) {
            y = ny;
        } else {
            dy = -dy;
        }

        raylib::BeginDrawing();
            raylib::BeginTextureMode(screen);
                raylib::ClearBackground(raylib::GetColor(0x181818FF));
                raylib::DrawCircle(x, y, r, raylib::GetColor(0xFF0000FF));
            raylib::EndTextureMode();

            raylib::ClearBackground(raylib::GetColor(0x181818FF));
            raylib::DrawTexture(screen.texture, 0, 0, raylib::WHITE);
        raylib::EndDrawing();

        raylib::Image image = raylib::LoadImageFromTexture(screen.texture);
        ffmpeg_send_frame_flipped(ffmpeg, image.data, WIDTH, HEIGHT);
        raylib::UnloadImage(image);
    }
    raylib::UnloadRenderTexture(screen);
    raylib::CloseWindow();

    ffmpeg_end_rendering(ffmpeg);

    printf("Done rendering the video!\n");

    return 0;
}