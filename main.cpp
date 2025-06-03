#include <iostream>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

namespace raylib {
    #include <raylib.h>
}
#include "ffmpeg.h"
#include "miniaudio.c"
#undef LoadImage
#undef DrawText



#define GetCurrentSample(cs) (assert(0 <= cs && cs < (int)top.samples.size() && "INVALID INDEX"), top.samples[cs])

#define PLAYSTATEPATH ".\\resources\\icons\\play.png"
#define PAUSESTATEPATH ".\\resources\\icons\\pause.png"
#define VOLUMEHIGHICONPATH ".\\resources\\icons\\volumehigh.png"
#define VOLUMEMUTEICONPATH ".\\resources\\icons\\mute.png"
#define APPICONPATH ".\\resources\\icons\\images.png"
#define SHADERCIRCLEPATH ".\\resources\\shaders\\glsl330\\circle.fs"

#define N (1<<13)

#define TRACKER_RADIUS 16
#define GAP 20
#define PLAYLIST_WIDTH 220
#define TRACHER_HEIGHT 130
#define VOLUME_LINE_HEIGHT 6
#define FPS 60
#define ICON_SCALE 0.07f

#define KEY_PAUSE_MUSIC raylib::KEY_SPACE
#define KEY_FULLSCREEN raylib::KEY_F
#define KEY_CAPUTRE_MICROPHONE raylib::KEY_C
#define KEY_RENDER_VIDEO raylib::KEY_R


struct Sample {
    raylib::Music music;
    char* name;
    char* file_name;
    bool paused;
    Sample(raylib::Music music, char* name, char* file_name, bool paused)
    {
        this->music = music;
        this->name = name;
        this->file_name = file_name;
        this->paused = paused;
    }
    Sample(){}
};

struct MyComplex {
    float Real, Imag;
    MyComplex(float Real, float Imag)
    {
        this->Real = Real;
        this->Imag = Imag;
    }
    MyComplex()
    {
        this->Real = 0;
        this->Imag = 0;
    }
};


typedef struct {
    // Tracks
    std::vector<Sample> samples;
    int CurrentSample;

    // Informative Buffers
    float in[N];
    float in2[N];
    MyComplex out[N];

    // Visualized Smooth Buffers
    float in_raw[N];
    float in_win[N];
    MyComplex out_raw[N];
    float out_log[N];
    float out_smooth[N];
    float out_smear[N];
    raylib::Shader shader;
    int shader_radius_location;
    int shader_power_location;


    // Capturing Audio
    ma_device device;
    bool capturing;

    // Volume
    raylib::Vector2 Volume_center;
    raylib::Texture2D Playstate;
    raylib::Texture2D Pausestate;
    raylib::Texture2D VolumeHigh_icon;
    raylib::Texture2D VolumeMute_icon;
    bool volumedrag;
    float volume;
    float volume_before_mute;
    bool muted;

    // Tracker
    raylib::Vector2 Tracker_center;
    bool trackerdrag;

    raylib::Vector2 mp;
    bool btnR;
    bool Quit;
    float t0;
    float len;
} Entity;

Entity top;
int w = 1280;
int h = 720;
raylib::Color BACKGROUND_COLOR = raylib::Color {.r = 0x20, .g = 0x20, .b = 0x20, .a = 0xFF};

float amp(MyComplex z) 
{
    float a = z.Real;
    float b = z.Imag;
    return logf(a * a + b * b);
}

void load_assets(void) 
{
    top.Playstate = raylib::LoadTexture(PLAYSTATEPATH);
    top.Playstate.width *= ICON_SCALE;
    top.Playstate.height *= ICON_SCALE;
    top.Pausestate = raylib::LoadTexture(PAUSESTATEPATH);
    top.Pausestate.width *= ICON_SCALE;
    top.Pausestate.height *= ICON_SCALE;
    top.VolumeHigh_icon = raylib::LoadTexture(VOLUMEHIGHICONPATH);
    top.VolumeHigh_icon.width *= ICON_SCALE;
    top.VolumeHigh_icon.height *= ICON_SCALE;
    top.VolumeMute_icon = raylib::LoadTexture(VOLUMEMUTEICONPATH);
    top.VolumeMute_icon.width *= ICON_SCALE;
    top.VolumeMute_icon.height *= ICON_SCALE;

    raylib::Image icon = raylib::LoadImage(APPICONPATH);
    raylib::SetWindowIcon(icon);
    top.shader = raylib::LoadShader(NULL, SHADERCIRCLEPATH);
    top.shader_radius_location = raylib::GetShaderLocation(top.shader, "radius");
    top.shader_power_location = raylib::GetShaderLocation(top.shader, "power");
    raylib::UnloadImage(icon);
}


void Init() 
{
    raylib::SetConfigFlags(raylib::FLAG_WINDOW_RESIZABLE | raylib::FLAG_WINDOW_ALWAYS_RUN);
    raylib::InitWindow(w, h, "Music Visualization");
    raylib::SetTargetFPS(FPS);
    raylib::InitAudioDevice();
    top.samples.clear();
    top.btnR = false;
    top.muted = false;
    top.volumedrag = false;
    top.trackerdrag = false;
    top.capturing = false;
    top.Quit = false;
    top.CurrentSample = -1;
    top.t0 = 0;
    top.len = 1;
    top.volume = 0;
    top.volume_before_mute = 0;
    top.Tracker_center = raylib::Vector2{ .x = 0, .y = (float)h - 100 };
    top.mp = raylib::Vector2{ .x = 0, .y = 0 };
    top.Volume_center = raylib::Vector2{ .x = 0, .y = 0 };
    load_assets();
}

void FFTClean(void) 
{
    memset(top.in_raw, 0, sizeof(top.in_raw));
    memset(top.in_win, 0, sizeof(top.in_win));
    memset((void*)&top.out_raw, 0, sizeof(top.out_raw));
    memset(top.out_log, 0, sizeof(top.out_log));
    memset(top.out_smooth, 0, sizeof(top.out_smooth));
    memset(top.out_smear, 0, sizeof(top.out_smear));
    memset(top.in, 0, sizeof(top.in));
    memset(top.in2, 0, sizeof(top.in2));
    memset((void*)&top.out, 0, sizeof(top.out));
}

void fft(float in[], size_t stride, MyComplex out[], size_t n) 
{
    assert(n > 0);

    if (n == 1) {
        out[0] = MyComplex{.x = 0, .y = in[0]};
        return;
    }

    fft(in, stride * 2, out, n / 2);
    fft(in + stride, stride * 2, out + n / 2, n / 2);

    for (size_t k = 0; k < n / 2; ++k) {
        float t = (float)k / n;
        MyComplex v = MyComplex(0, 0);
        MyComplex temp = MyComplex(cosf(-2 * PI * t), sinf(-2 * PI * t));
        v.Real = temp.Real * out[k + n / 2].Real - temp.Imag * out[k + n / 2].Imag;
        v.Imag = temp.Real * out[k + n / 2].Imag + out[k + n / 2].Real * temp.Imag;
        MyComplex e = out[k];
        out[k].Real = e.Real + v.Real;
        out[k].Imag = e.Imag + v.Imag;
        out[k + n / 2].Real = e.Real - v.Real;
        out[k + n / 2].Imag = e.Imag - v.Imag;
    }
}

void CallBack(void* bufferData, unsigned int frames) {

    // https://cdecl.org/?q=float+%28*fs%29%5B2%5D
    float (*fs)[2] = (float (*)[2])bufferData;

    for (size_t i = 0; i < frames; ++i) {
        memmove(top.in_raw, top.in_raw + 1, (N - 1) * sizeof(top.in_raw[0]));
        top.in_raw[N - 1] = fs[i][0];
    }

}

void MACallBack(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) 
{
    CallBack((void*)pInput, frameCount);
    (void)pOutput;
    (void)pDevice;
}

size_t fft_analyze(float dt) {
    // Apply the Hann Window on the Input - https://en.wikipedia.org/wiki/Hann_function
    for (size_t i = 0; i < N; ++i) {
        float t = (float)i / (N - 1);
        float hann = 0.5 - 0.5 * cosf(2 * PI * t);
        top.in_win[i] = top.in_raw[i] * hann;
    }

    // FFT
    fft(top.in_win, 1, top.out_raw, N);

    // "Squash" into the Logarithmic Scale
    float step = 1.06;
    float lowf = 1.0f;
    size_t m = 0;
    float max_amp = 1.0f;
    for (float f = lowf; (size_t)f < N / 2; f = ceilf(f * step)) {
        float f1 = ceilf(f * step);
        float a = 0.0f;
        for (size_t q = (size_t)f; q < N / 2 && q < (size_t)f1; ++q) {
            float b = amp(top.out_raw[q]);
            if (b > a) a = b;
        }
        if (max_amp < a) max_amp = a;
        top.out_log[m++] = a;
    }

    // Normalize Frequencies to 0..1 range
    for (size_t i = 0; i < m; ++i) {
        top.out_log[i] /= max_amp;
    }

    // Smooth out and smear the values
    for (size_t i = 0; i < m; ++i) {
        float smoothness = 8;
        top.out_smooth[i] += (top.out_log[i] - top.out_smooth[i]) * smoothness * dt;
        float smearness = 3;
        top.out_smear[i] += (top.out_smooth[i] - top.out_smear[i]) * smearness * dt;
    }

    return m;
}

void fft_render(raylib::Rectangle boundary, size_t m) {
    // The width of a single bar
    float cell_width = boundary.width / m;

    // Global color parameters
    float saturation = 0.75f;
    float value = 1.0f;

    // Display the Bars
    for (size_t i = 0; i < m; ++i) {
        float t = top.out_smooth[i];
        float hue = (float)i / m;
        raylib::Color color = raylib::ColorFromHSV(hue * 360, saturation, value);
        raylib::Vector2 startPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * t,
        };
        raylib::Vector2 endPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height,
        };
        float thick = cell_width / 3 * sqrtf(t);
        DrawLineEx(startPos, endPos, thick, color);
    }

    raylib::Texture2D texture = { 1, 1, 1, 1, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    // Display the Smears
    float radius1 = 0.3f;
    float power1 = 3.0f;
    raylib::SetShaderValue(top.shader, top.shader_radius_location, &radius1, raylib::SHADER_UNIFORM_FLOAT);
    raylib::SetShaderValue(top.shader, top.shader_power_location, &power1, raylib::SHADER_UNIFORM_FLOAT);
    raylib::BeginShaderMode(top.shader);
    for (size_t i = 0; i < m; ++i) {
        float start = top.out_smear[i];
        float end = top.out_smooth[i];
        float hue = (float)i / m;
        raylib::Color color = raylib::ColorFromHSV(hue * 360, saturation, value);
        raylib::Vector2 startPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * start,
        };
        raylib::Vector2 endPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * end,
        };
        float radius = cell_width * 3 * sqrtf(end);
        raylib::Vector2 origin = raylib::Vector2{0, 0};
        if (endPos.y >= startPos.y) {
            raylib::Rectangle dest = {
                .x = startPos.x - radius / 2,
                .y = startPos.y,
                .width = radius,
                .height = endPos.y - startPos.y
            };
            raylib::Rectangle source = { 0, 0, 1, 0.5 };
            raylib::DrawTexturePro(texture, source, dest, origin, 0, color);
        }
        else {
            raylib::Rectangle dest = {
                .x = endPos.x - radius / 2,
                .y = endPos.y,
                .width = radius,
                .height = startPos.y - endPos.y
            };
            raylib::Rectangle source = { 0, 0.5, 1, 0.5 };
            raylib::DrawTexturePro(texture, source, dest, origin, 0, color);
        }
    }
    raylib::EndShaderMode();

    // Display the Circles
    float radius = 0.07f;
    float power = 5.0f;
    raylib::SetShaderValue(top.shader, top.shader_radius_location, &radius, raylib::SHADER_UNIFORM_FLOAT);
    raylib::SetShaderValue(top.shader, top.shader_power_location, &power, raylib::SHADER_UNIFORM_FLOAT);
    raylib::BeginShaderMode(top.shader);
    for (size_t i = 0; i < m; ++i) {
        float t = top.out_smooth[i];
        float hue = (float)i / m;
        raylib::Color color = raylib::ColorFromHSV(hue * 360, saturation, value);
        raylib::Vector2 center = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * t,
        };
        float radius = cell_width * 6 * sqrtf(t);
        raylib::Vector2 position = {
            .x = center.x - radius,
            .y = center.y - radius,
        };
        raylib::DrawTextureEx(texture, position, 0, 2 * radius, color);
    }
    raylib::EndShaderMode();
}


// void FreqDomainAnalysis() {
//     fft(top.in2, 1, top.out, N);
// }
// void FreqDomainVisualInfo(raylib::Rectangle boundary) {
//     FreqDomainAnalysis();
//     if (true) {
//         float rw = (float)(boundary.width) / (N);
//         size_t thr = N / 8;
//         for (size_t i = 0; i < N; i++) {
//             int j = (i + N / 2) % N;
//             MyComplex c = top.out[j];
//             float t = fabsf(c.Real) / 10;
//             float freq = t * boundary.height;
//             if (i <= thr || i >= N - thr)
//                 freq *= 1 * (1 - cos(2 * PI * ((float)i / N)));
//             if (freq > boundary.height)
//                 freq /= 2;
//             raylib::Color color = raylib::ColorFromHSV((float)i / N * 360, 0.75f, 1.0f);
//             raylib::Rectangle rec = { .x = boundary.x + i * rw, .y = boundary.height - freq, .width = rw, .height = freq };
//             raylib::DrawRectangleRec(rec, color);
//         }
//     }
//     else {
//         float rw = (float)(boundary.width) / (N);
//         for (size_t i = 0; i < N; i++) {
//             int j = (i + N / 2) % N;
//             MyComplex c = top.out[j];
//             float t = fabsf(c.Real);
//             float freq = t * boundary.height;
//             if (freq > boundary.height)
//                 freq /= 2;
//             raylib::Color color = raylib::ColorFromHSV((float)i / N * 360, 0.75f, 1.0f);
//             raylib::Rectangle rec = { .x = boundary.x + i * rw, .y = boundary.height - freq, .width = rw, .height = freq };
//             raylib::DrawRectangleRec(rec, color);
//         }
//     }
// }

float limit(float val, float lower, float upper) {
    return (val < lower) ? (lower) : ((val > upper) ? (upper) : (val));
}

float normalize_val(float val, float lower, float upper) {
    val = limit(val, lower, upper);
    return (float)(val - lower) / (upper - lower);
}

void UpdateMusicPlayingByIndex(size_t index) {

    if ((int)index != top.CurrentSample && 0 <= (int)index && index < top.samples.size()) {
        if (0 <= top.CurrentSample && top.CurrentSample < (int)top.samples.size() && raylib::IsMusicValid(GetCurrentSample(top.CurrentSample).music))
            raylib::PauseMusicStream(GetCurrentSample(top.CurrentSample).music);

        top.CurrentSample = index;

        if (!GetCurrentSample(top.CurrentSample).paused)
            raylib::PlayMusicStream(GetCurrentSample(top.CurrentSample).music);

        top.len = raylib::GetMusicTimeLength(GetCurrentSample(top.CurrentSample).music);
        raylib::SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
    }

}

void ToggleCurrenSampleState() {

    if (0 <= top.CurrentSample && top.CurrentSample < (int)top.samples.size() && IsMusicValid(GetCurrentSample(top.CurrentSample).music)) {
        if (raylib::IsMusicStreamPlaying(GetCurrentSample(top.CurrentSample).music)) {
            raylib::PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
            GetCurrentSample(top.CurrentSample).paused = true;
        }
        else {
            raylib::ResumeMusicStream(GetCurrentSample(top.CurrentSample).music);
            GetCurrentSample(top.CurrentSample).paused = false;
        }
    }

}

void Update() {
    top.btnR = raylib::IsMouseButtonReleased(raylib::MOUSE_BUTTON_LEFT);
    top.mp = raylib::GetMousePosition();
    
    raylib::UpdateMusicStream(GetCurrentSample(top.CurrentSample).music);
    top.t0 = raylib::GetMusicTimePlayed(GetCurrentSample(top.CurrentSample).music);
    if (raylib::IsKeyPressed(KEY_PAUSE_MUSIC)) {
        ToggleCurrenSampleState();
    }
    top.Tracker_center.y = h - 100;
}

void ManageTracker() {
    raylib::Rectangle tracker = { .x = 0, .y = (float)top.Tracker_center.y, .width = (float)w, .height = 1 };
    raylib::Rectangle tracker_boundary = { .x = 0, .y = (float)top.Tracker_center.y - TRACKER_RADIUS, .width = (float)w, .height = 2 * TRACKER_RADIUS };
    raylib::DrawRectangleRec(tracker, raylib::GRAY); // line for the tracker
    float posx = (top.t0 / top.len) * w;
    bool collision_with_a_tracker_boundary = raylib::CheckCollisionPointRec(top.mp, tracker_boundary);
    if (raylib::IsMouseButtonPressed(raylib::MOUSE_BUTTON_LEFT) && collision_with_a_tracker_boundary) {
        top.trackerdrag = true;
        raylib::PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        raylib::SeekMusicStream(GetCurrentSample(top.CurrentSample).music, (posx) * (top.len / (w)));
    }
    if (top.trackerdrag) {
        posx = top.mp.x;
        posx = limit(posx, 0.0f, w);
        if (top.btnR) {
            top.trackerdrag = false;
            raylib::SeekMusicStream(GetCurrentSample(top.CurrentSample).music, (posx) * (top.len / w));
        }
        if (!top.trackerdrag && !GetCurrentSample(top.CurrentSample).paused) {
            raylib::ResumeMusicStream(GetCurrentSample(top.CurrentSample).music);
        }
    }
    // hh:mm:ss
    int Seconds = (int)top.t0 % 60;
    int Minutes = (int)(top.t0 / 60) % 60;
    int Hours = (int)Minutes / 60;
    const char* DurationText = raylib::TextFormat("%d%d:%d%d:%d%d", Hours/10, Hours%10, Minutes/10, Minutes%10, Seconds/10, Seconds%10);
    int DurationLength = raylib::MeasureText(DurationText, 20);
    raylib::DrawText(DurationText, w - DurationLength - 40, h - 65, 20, raylib::WHITE);
    top.Tracker_center.x = posx;
    raylib::DrawCircleV(top.Tracker_center, TRACKER_RADIUS, raylib::GRAY);
}

void file_name_to_name(const char* file_name, char* name) {
    size_t index = strrchr(file_name, '\\') - file_name + 1;
    size_t j;
    for (j = index; j < index + 17; j++) {
        if (file_name[j] == '.')
            break;
        name[j - index] = file_name[j];
    }
    name[j - index] = '\0';
}

void POPUPErrorFileNotSupported(char* name) {
    (void)name;
    assert(0 && "NOT IMPLEMENTED");
}

void ManagePlayList() {
    if (raylib::IsFileDropped()) {
        raylib::FilePathList FilePathList = raylib::LoadDroppedFiles();
        if (FilePathList.count > 0) {
            for (size_t i = 0; i < FilePathList.count; i++) {
                static char temp[20];
                file_name_to_name(FilePathList.paths[i], temp);

                raylib::Music music = raylib::LoadMusicStream(FilePathList.paths[i]);
                if (raylib::IsMusicValid(music)) {

                    AttachAudioStreamProcessor(music.stream, CallBack);
                    top.samples.push_back(Sample(music, _strdup(temp), _strdup(FilePathList.paths[i]), false));
                }
                else {
                    // POPUPErrorFileNotSupported(FilePathList.paths[i]);
                }
            }
            if (top.CurrentSample == -1) UpdateMusicPlayingByIndex(0);
        }
        raylib::UnloadDroppedFiles(FilePathList);
    }
    // drawing the playlist
    static float playlist_scroll = 0;
    static float scroll_velocity = 0;
    float playlist_boundary_height = top.Tracker_center.y - TRACKER_RADIUS;
    float item_height = playlist_boundary_height * (0.07f);
    raylib::Rectangle playlist_boundary = { .x = 0, .y = 0, .width = PLAYLIST_WIDTH, .height = playlist_boundary_height };
    bool colplaylistboundary = raylib::CheckCollisionPointRec(top.mp, playlist_boundary);

    if (raylib::CheckCollisionPointRec(top.mp, playlist_boundary)) {
        scroll_velocity *= 0.9;
        scroll_velocity += 4 * raylib::GetMouseWheelMove() * item_height;
        playlist_scroll += scroll_velocity * raylib::GetFrameTime();
    }

    // not to able to scroll up more than it should be
    if (playlist_scroll > 0) playlist_scroll = 0;
    // not to be able to scroll if the playlist page is not full
    if (playlist_scroll < 0 && top.samples.size() * (item_height + 5) <= playlist_boundary_height) playlist_scroll = 0;
    // no to be able to scroll down more than it should be
    if (playlist_scroll < 0 && playlist_scroll + top.samples.size() * (item_height + 5) - playlist_boundary_height <= 0)
        playlist_scroll = -1 * (top.samples.size() * (item_height + 5) - playlist_boundary_height);

    playlist_boundary.y = playlist_scroll;

    raylib::BeginScissorMode(playlist_boundary.x, 0, playlist_boundary.width, playlist_boundary.height);
    for (size_t i = 0; i < top.samples.size(); i++) {
        raylib::Rectangle rec = { .x = 0, .y = i * (item_height + 5) + playlist_boundary.y,
                          .width = playlist_boundary.width, .height = item_height };
        bool col_with_item = raylib::CheckCollisionPointRec(top.mp, rec);
        raylib::Color color;
        if (colplaylistboundary && col_with_item)
            color = raylib::DARKGRAY;
        else
            color = raylib::GRAY;
        // if music is selected then update the currently playing music
        if (colplaylistboundary && col_with_item && top.btnR)
            UpdateMusicPlayingByIndex(i);

        raylib::DrawRectangleRounded(rec, 0.4f, 20, color);
        raylib::DrawText(raylib::TextFormat("%d-%s", i + 1, top.samples[i].name), 0, rec.y + rec.height / 2 - 10, 20, raylib::WHITE);
    }
    raylib::EndScissorMode();

    if (top.samples.size() > 0) {
        raylib::DrawRectangle(PLAYLIST_WIDTH, 0, 1, top.Tracker_center.y, raylib::GRAY); // line to split the playlist and the FFT
    }
    const char* samplescount = raylib::TextFormat("Samples Count : %d", top.samples.size());
    raylib::DrawText(samplescount, w - raylib::MeasureText(samplescount, 20) - 5, 0, 20, raylib::WHITE);
}

void MusicSettings() {
    // Reset Button
    raylib::Rectangle btnreset = {
        .x = 10 , .y = (float)h - 50,
        .width = 75 , .height = 40
    };
    raylib::DrawRectangleRec(btnreset, raylib::WHITE);
    raylib::DrawText("Reset", btnreset.x + btnreset.width / 7, btnreset.y + btnreset.height / 4, 20, raylib::BLACK);

    bool btnresetcol = raylib::CheckCollisionPointRec(top.mp, btnreset);

    if (btnresetcol && top.btnR && IsMusicValid(GetCurrentSample(top.CurrentSample).music)) {
        raylib::SeekMusicStream(GetCurrentSample(top.CurrentSample).music, 0.0f);
    }

    raylib::Vector2 PlayingState_pos = { .x = (float)btnreset.x + btnreset.width + GAP,
                                 .y = (float)btnreset.y + btnreset.height / 2 - top.Playstate.height / 2 };
    raylib::Rectangle PlayingState_boundary = { .x = (float)PlayingState_pos.x, .y = (float)PlayingState_pos.y, .width = (float)top.Playstate.width, .height = (float)top.Playstate.height };
    if (raylib::CheckCollisionPointRec(top.mp, PlayingState_boundary) && top.btnR) {
        ToggleCurrenSampleState();
    }

    if (GetCurrentSample(top.CurrentSample).paused) {
        raylib::DrawTextureV(top.Pausestate, PlayingState_pos, raylib::ColorBrightness(raylib::WHITE, -0.10));
    }
    else {
        raylib::DrawTextureV(top.Playstate, PlayingState_pos, raylib::ColorBrightness(raylib::WHITE, -0.10));
    }


    // Volume
    static float volRadius = 10.0f;
    raylib::Vector2 Volume_icon_pos = { .x = (float)PlayingState_pos.x + top.Playstate.width + GAP, .y = (float)btnreset.y + btnreset.height / 2 - top.VolumeHigh_icon.height / 2 };
    raylib::Rectangle Volume_icon_boundary = { .x = (float)Volume_icon_pos.x, .y = (float)Volume_icon_pos.y, .width = (float)top.VolumeHigh_icon.width, .height = (float)top.VolumeHigh_icon.height };
    raylib::Rectangle volline = {
        .x = (float)Volume_icon_pos.x + top.VolumeHigh_icon.width + GAP + 20 , .y = (float)btnreset.y + btnreset.height / 2 - VOLUME_LINE_HEIGHT / 2,
        .width = 250 , .height = VOLUME_LINE_HEIGHT
    };

    if (raylib::CheckCollisionPointRec(top.mp, Volume_icon_boundary) && top.btnR) {
        if (!top.muted) {
            top.volume = 0;
            raylib::SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
        }
        else {
            top.volume = top.volume_before_mute;
            raylib::SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
        }
    }

    DrawRectangleRounded(volline, 10.0f, 1, raylib::WHITE);
    bool colVolcir = CheckCollisionPointCircle(top.mp, top.Volume_center, volRadius);
    raylib::Rectangle Volume_boundary = { .x = volline.x, .y = (float)top.Volume_center.y - volRadius, .width = volline.width, .height = 2 * volRadius };
    bool col_volume_boundary = raylib::CheckCollisionPointRec(top.mp, Volume_boundary);
    if (raylib::IsMouseButtonPressed(raylib::MOUSE_BUTTON_LEFT) && col_volume_boundary) {
        top.volumedrag = true;
    }
    if (top.volumedrag) {
        top.volume = normalize_val(top.mp.x, volline.x, volline.x + volline.width);
        raylib::SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
        top.volume_before_mute = top.volume;
        if (top.btnR)
            top.volumedrag = false;
    }

    if (top.volume == 0.0f) {
        DrawTextureV(top.VolumeMute_icon, Volume_icon_pos, ColorBrightness(raylib::WHITE, -0.10));
        top.muted = true;
    }
    else {
        DrawTextureV(top.VolumeHigh_icon, Volume_icon_pos, ColorBrightness(raylib::WHITE, -0.10));
        top.muted = false;
    }


    volRadius = (top.volumedrag || colVolcir) ? 14.0f : 10.0f;
    top.Volume_center.x = volline.x + top.volume * volline.width;
    top.Volume_center.y = volline.y + volline.height / 2;
    DrawCircleV(top.Volume_center, volRadius, raylib::WHITE);
    if (col_volume_boundary || top.volumedrag) {
        int Text_volume_x = volline.x + volline.width + volRadius + 5;
        int text_height = 20;
        const char* volume_text = raylib::TextFormat("Volume: %.2f%%", top.volume * 100.0f);
        DrawText(volume_text, Text_volume_x, top.Volume_center.y - text_height / 2, text_height, raylib::WHITE);
    }
}

void ManageFeatures() {
    ManagePlayList();
    ManageTracker();
    MusicSettings();
}

void InitScreen() {
    int height = 50;
    static char text[] = "Drag & Drop Music";
    int width = raylib::MeasureText(text, height);
    DrawText(text, w / 2 - width / 2, h / 2 - height / 2, height, raylib::WHITE);
}

void ToggleMicrophoneCapture() {
    top.capturing = !top.capturing;
    if (top.capturing) {
        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.playback.format = ma_format_f32;
        config.playback.channels = 2;
        config.sampleRate = 41000;
        config.dataCallback = MACallBack;
        config.pUserData = NULL;

        if (ma_device_init(NULL, &config, &top.device) != MA_SUCCESS) {
            fprintf(stderr, "Microphone Not Initialized Successfuly\n");
            top.capturing = false;
            return;
        }
        if (top.CurrentSample >= 0 && IsMusicValid(GetCurrentSample(top.CurrentSample).music))
            raylib::PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        ma_device_start(&top.device);

    }
    else {
        ma_device_uninit(&top.device);
        if (0 <= top.CurrentSample && top.CurrentSample < (int)top.samples.size() && IsMusicValid(GetCurrentSample(top.CurrentSample).music) && !GetCurrentSample(top.CurrentSample).paused)
            raylib::PlayMusicStream(GetCurrentSample(top.CurrentSample).music);
        FFTClean();
    }
}

void VisualizeFFT(raylib::Rectangle boundary, float dt) 
{
    size_t m = fft_analyze(dt);
    fft_render(boundary, m);
}

void fft_push(float frame)
{
    memmove(top.in_raw, top.in_raw + 1, (N - 1)*sizeof(top.in_raw[0]));
    top.in_raw[N-1] = frame;
}

int main(void)
{
    
    Init();
    bool FULLSCREEN = false;
    bool RENDERING = false;
    unsigned int WaveCursor;
    Sample s;
    raylib::Wave Wave;
    float* WaveSamples;
    FFMPEG* ffmpeg;
    raylib::RenderTexture2D screen;
    while (!top.Quit)
    {
        w = raylib::GetScreenWidth();
        h = raylib::GetScreenHeight();
        if (raylib::IsKeyPressed(KEY_FULLSCREEN))
            FULLSCREEN = !FULLSCREEN;
        if (raylib::IsKeyPressed(KEY_CAPUTRE_MICROPHONE))
            ToggleMicrophoneCapture();
        if (raylib::IsKeyPressed(KEY_RENDER_VIDEO) && !RENDERING)
        {
            screen = raylib::LoadRenderTexture(1280, 720);
            RENDERING = true;
            raylib::PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
            GetCurrentSample(top.CurrentSample).paused = true;
            FFTClean();
            
            WaveCursor = 0;
            s = GetCurrentSample(top.CurrentSample);
            Wave = raylib::LoadWave(s.file_name);
            WaveSamples = raylib::LoadWaveSamples(Wave);
            ffmpeg = ffmpeg_start_rendering(screen.texture.width, screen.texture.height, FPS, s.file_name);
            raylib::SetTargetFPS(0);
        }

        raylib::BeginDrawing();
        raylib::ClearBackground(BACKGROUND_COLOR);

        if (top.capturing) {
            VisualizeFFT((raylib::Rectangle {
                .x = 0,
                .y = 0,
                .width =  (float)w,
                .height = (float)h,
            }), 1.0f / FPS);
            if (raylib::IsKeyPressed(raylib::KEY_ESCAPE)) ToggleMicrophoneCapture();
        }
        else if (!RENDERING)
        {
           if (top.samples.size() > 0) 
            {
                Update();
                raylib::Rectangle boundary;
                if (FULLSCREEN)
                {
                    boundary = raylib::Rectangle {
                        .x = 0,
                        .y = 0,
                        .width =  (float)w,
                        .height = (float)h
                    };
                    if (raylib::IsKeyPressed(raylib::KEY_ESCAPE)) FULLSCREEN = false;
                }
                else
                {
                    boundary = raylib::Rectangle {
                        .x = PLAYLIST_WIDTH,
                        .y = 0,
                        .width =  (float)w - PLAYLIST_WIDTH,
                        .height = (float)h - TRACHER_HEIGHT
                    };
                }
                VisualizeFFT(boundary, 1.0f / FPS);
                if (!FULLSCREEN)
                    ManageFeatures();
            }
            else {
                ManagePlayList();
                InitScreen();
            }
        }
        else
        {
            size_t ChunkSize = Wave.sampleRate/FPS;
            float *fs = WaveSamples;
            for (size_t i = 0; i < ChunkSize; ++i) {
                if (WaveCursor < Wave.frameCount) {
                    fft_push(fs[WaveCursor*Wave.channels + 0]);
                } else {
                    fft_push(0);
                }
                WaveCursor += 1;
            }

            raylib::BeginTextureMode(screen);
            raylib::ClearBackground(BACKGROUND_COLOR);
            VisualizeFFT((raylib::Rectangle {
                .x = 0,
                .y = 0,
                .width =  (float)screen.texture.width,
                .height = (float)screen.texture.height,
            }), 1.0f / FPS);
            raylib::EndTextureMode();
            raylib::Image image = raylib::LoadImageFromTexture(screen.texture);
            ffmpeg_send_frame_flipped(ffmpeg, image.data, screen.texture.width, screen.texture.height);
            raylib::UnloadImage(image);
            
            raylib::ClearBackground(BACKGROUND_COLOR);
            float Progress = (float)WaveCursor / Wave.frameCount;
            float ProgressBarWidth = (float)w / 3;
            float ProgressBarHeight = (float)h / 10;
            raylib::Rectangle ProgressBar = raylib::Rectangle {.x = w / 2 - ProgressBarWidth / 2, .y = h / 2 - ProgressBarHeight / 2, .width = Progress * ProgressBarWidth, .height = ProgressBarHeight};
            raylib::DrawRectangleRec(ProgressBar, raylib::WHITE);
            raylib::Rectangle ProgressBarBoundary = raylib::Rectangle {.x = w / 2 - ProgressBarWidth / 2, .y = h / 2 - ProgressBarHeight / 2, .width = ProgressBarWidth, .height = ProgressBarHeight};
            raylib::DrawRectangleLinesEx(ProgressBarBoundary, 10.0f, raylib::WHITE);

            if (raylib::IsKeyPressed(raylib::KEY_ESCAPE)) RENDERING = false;
            if (WaveCursor >= Wave.frameCount || !RENDERING)
            {
                raylib::UnloadRenderTexture(screen);
                ffmpeg_end_rendering(ffmpeg);
                raylib::UnloadWave(Wave);
                raylib::UnloadWaveSamples(WaveSamples);
                raylib::SetTargetFPS(FPS);
                FFTClean();
                raylib::PlayMusicStream(GetCurrentSample(top.CurrentSample).music);
                GetCurrentSample(top.CurrentSample).paused = false;
                RENDERING = false;
            }
        }
        raylib::DrawFPS(0, 0);
        raylib::EndDrawing();
        top.Quit = raylib::IsKeyPressed(raylib::KEY_ESCAPE) && !FULLSCREEN && !top.capturing && !RENDERING;
    }

    raylib::UnloadTexture(top.Playstate);
    raylib::UnloadTexture(top.Pausestate);
    raylib::UnloadTexture(top.VolumeHigh_icon);
    raylib::UnloadTexture(top.VolumeMute_icon);
    raylib::UnloadShader(top.shader);
    ma_device_uninit(&top.device);
    raylib::CloseAudioDevice();
    raylib::CloseWindow();
    return 0;
}


