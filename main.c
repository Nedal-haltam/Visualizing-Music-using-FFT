#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "raylib.h"
#include "nob.c"
#include "miniaudio.c"


typedef struct {
    float x, y;
} mycomplex;

#define GetCurrentSample(cs) top.samples.items[cs]
#define PLAYSTATEPATH ".\\resources\\icons\\play.png"
#define PAUSESTATEPATH ".\\resources\\icons\\pause.png"
#define VOLUMEHIGHICONPATH ".\\resources\\icons\\volumehigh.png"
#define VOLUMEMUTEICONPATH ".\\resources\\icons\\mute.png"
#define APPICONPATH ".\\resources\\icons\\images.png"
#define SHADERCIRCLEPATH ".\\resources\\shaders\\glsl330\\circle.fs"

#define var true
#if var
#define N (1<<13)
#else
#define N (1<<11)
#endif

#define Tracker_Radius 16
#define fgap 20
#define playlist_width 220
#define tracker_height 130
#define volline_height 6
#define FPS 60
#define Display_FPS 70
#define scale 0.07f

// constants we use
int w = 1600;
int h = 800;

typedef struct {
    Music music;
    char* name;
    char* file_name;
    bool paused;
} Sample;

typedef struct {
    Sample* items;
    size_t count;
    size_t capacity;
} Samples;

typedef struct {
    // Tracks
    Samples samples;
    int CurrentSample;

    // Informative Buffers
    float in[N];
    float in2[N];
    mycomplex out[N];

    // Visualized Smooth Buffers
    float in_raw[N];
    float in_win[N];
    mycomplex out_raw[N];
    float out_log[N];
    float out_smooth[N];
    float out_smear[N];
    Shader shader;
    int shader_radius_location;
    int shader_power_location;


    // Capturing Audio
    ma_device device;
    bool capturing;

    // Volume
    Vector2 Volume_center;
    Texture2D Playstate;
    Texture2D Pausestate;
    Texture2D VolumeHigh_icon;
    Texture2D VolumeMute_icon;
    bool volumedrag;
    float volume;
    float volume_before_mute;
    bool muted;

    // Tracker
    Vector2 Tracker_center;
    bool trackerdrag;

    Vector2 mp;
    bool btnD;
    bool btnP;
    bool btnR;

    float t0;
    float len;
} Entity;

Entity top;

//TODO:  -tooltip for shwing key pressing and showing the volline when hovering on icon
//       -popup for failure
//       -rendering video
//       -Draw the time played in the following format hh:mm:ss 

float amp(mycomplex z) {
    float a = z.x;
    float b = z.y;
    return logf(a * a + b * b);
}

float amp1(mycomplex z) {
    return fabsf(z.x);
}

void load_assets(void) {

    top.Playstate = LoadTexture(PLAYSTATEPATH);
    top.Playstate.width *= scale;
    top.Playstate.height *= scale;
    top.Pausestate = LoadTexture(PAUSESTATEPATH);
    top.Pausestate.width *= scale;
    top.Pausestate.height *= scale;
    top.VolumeHigh_icon = LoadTexture(VOLUMEHIGHICONPATH);
    top.VolumeHigh_icon.width *= scale;
    top.VolumeHigh_icon.height *= scale;
    top.VolumeMute_icon = LoadTexture(VOLUMEMUTEICONPATH);
    top.VolumeMute_icon.width *= scale;
    top.VolumeMute_icon.height *= scale;

    Image icon = LoadImage(APPICONPATH);
    SetWindowIcon(icon);
    top.shader = LoadShader(NULL, SHADERCIRCLEPATH);
    top.shader_radius_location = GetShaderLocation(top.shader, "radius");
    top.shader_power_location = GetShaderLocation(top.shader, "power");
    UnloadImage(icon);
}

void PlayListClean() {
    top.samples.count = 0;
    top.samples.capacity = 0;
    free(top.samples.items);
    top.samples.items = NULL;
}

void Init() {
    InitWindow(w, h, "Mymusulizer");
    SetTargetFPS(FPS);
    InitAudioDevice();
    PlayListClean();
    top.CurrentSample = -1;
    top.btnD = false;
    top.btnP = false;
    top.btnR = false;
    top.t0 = 0;
    top.len = 1;
    top.muted = false;
    top.volumedrag = false;
    top.trackerdrag = false;
    top.volume = 0;
    top.volume_before_mute = 0;
    top.capturing = false;
    top.Tracker_center = (Vector2){ .x = 0, .y = h - 100 };
    top.mp = (Vector2){ .x = 0, .y = 0 };
    top.Volume_center = (Vector2){ .x = 0, .y = 0 };
    load_assets();
}

void fft_clean(void) {
    memset(top.in_raw, 0, sizeof(top.in_raw));
    memset(top.in_win, 0, sizeof(top.in_win));
    memset(top.out_raw, 0, sizeof(top.out_raw));
    memset(top.out_log, 0, sizeof(top.out_log));
    memset(top.out_smooth, 0, sizeof(top.out_smooth));
    memset(top.out_smear, 0, sizeof(top.out_smear));
    memset(top.in, 0, sizeof(top.in));
    memset(top.in2, 0, sizeof(top.in2));
    memset(top.out, 0, sizeof(top.out));
}

void fft(float in[], size_t stride, mycomplex out[], size_t n) {
    assert(n > 0);

    if (n == 1) {
        out[0] = (mycomplex) {.x = 0, .y = in[0]};
        return;
    }

    fft(in, stride * 2, out, n / 2);
    fft(in + stride, stride * 2, out + n / 2, n / 2);

    for (size_t k = 0; k < n / 2; ++k) {
        float t = (float)k / n;
        mycomplex v = { 0 };
        mycomplex temp = {.x = cosf(-2 * PI * t), .y = sinf(-2 * PI * t),};
        v.x = temp.x * out[k + n / 2].x - temp.y * out[k + n / 2].y;
        v.y = temp.x * out[k + n / 2].y + out[k + n / 2].x * temp.y;
        //mycomplex v = mulcc(cexpf(cfromimag(-2*PI*t)), out[k + n/2]);
        mycomplex e = out[k];
        out[k].x = e.x + v.x;
        out[k].y = e.y + v.y;
        //out[k]       = addcc(e, v);
        out[k + n / 2].x = e.x - v.x;
        out[k + n / 2].y = e.y - v.y;
        //out[k + n/2] = subcc(e, v);
    }
}


void callbackvar(void* bufferData, unsigned int frames) {

    // https://cdecl.org/?q=float+%28*fs%29%5B2%5D
    float (*fs)[2] = bufferData;

    for (size_t i = 0; i < frames; ++i) {
        memmove(top.in_raw, top.in_raw + 1, (N - 1) * sizeof(top.in_raw[0]));
        top.in_raw[N - 1] = fs[i][0];
    }

}

void TimeDomainAnalysis(void* bufferData, unsigned int frames) {

    if (frames > N) frames = N;
    float (*fs)[2] = bufferData;
    for (size_t i = 0; i < frames; i++) {
        memmove(top.in, top.in + 1, sizeof(top.in[0]) * (N - 1));
        top.in[N - 1] = fs[i][0];
    }

    for (size_t i = 0; i < N; i++) {
        float t = (float)i / N;
        float hann = (0.5 - 0.5 * cos(2 * PI * t));
        top.in2[i] = top.in[i] * pow(hann, 10);
    }
}

void callback(void* bufferData, unsigned int frames) {

    TimeDomainAnalysis(bufferData, frames);

}

void ma_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {

#if var
    callbackvar((void*)pInput, frameCount);
#else
    callback((void*)pInput, frameCount);
#endif
    //if (var)
    //    callbackvar((void*)pInput,frameCount);
    //else
    //    callback((void*)pInput,frameCount);
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

void fft_render(Rectangle boundary, size_t m) {
    // The width of a single bar
    float cell_width = boundary.width / m;

    // Global color parameters
    float saturation = 0.75f;
    float value = 1.0f;

    // Display the Bars
    for (size_t i = 0; i < m; ++i) {
        float t = top.out_smooth[i];
        float hue = (float)i / m;
        Color color = ColorFromHSV(hue * 360, saturation, value);
        Vector2 startPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * t,
        };
        Vector2 endPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height,
        };
        float thick = cell_width / 3 * sqrtf(t);
        DrawLineEx(startPos, endPos, thick, color);
    }

    Texture2D texture = { 1, 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    // Display the Smears
    SetShaderValue(top.shader, top.shader_radius_location, (float[1]) { 0.3f }, SHADER_UNIFORM_FLOAT);
    SetShaderValue(top.shader, top.shader_power_location, (float[1]) { 3.0f }, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(top.shader);
    for (size_t i = 0; i < m; ++i) {
        float start = top.out_smear[i];
        float end = top.out_smooth[i];
        float hue = (float)i / m;
        Color color = ColorFromHSV(hue * 360, saturation, value);
        Vector2 startPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * start,
        };
        Vector2 endPos = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * end,
        };
        float radius = cell_width * 3 * sqrtf(end);
        Vector2 origin = { 0 };
        if (endPos.y >= startPos.y) {
            Rectangle dest = {
                .x = startPos.x - radius / 2,
                .y = startPos.y,
                .width = radius,
                .height = endPos.y - startPos.y
            };
            Rectangle source = { 0, 0, 1, 0.5 };
            DrawTexturePro(texture, source, dest, origin, 0, color);
        }
        else {
            Rectangle dest = {
                .x = endPos.x - radius / 2,
                .y = endPos.y,
                .width = radius,
                .height = startPos.y - endPos.y
            };
            Rectangle source = { 0, 0.5, 1, 0.5 };
            DrawTexturePro(texture, source, dest, origin, 0, color);
        }
    }
    EndShaderMode();

    // Display the Circles
    SetShaderValue(top.shader, top.shader_radius_location, (float[1]) { 0.07f }, SHADER_UNIFORM_FLOAT);
    SetShaderValue(top.shader, top.shader_power_location, (float[1]) { 5.0f }, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(top.shader);
    for (size_t i = 0; i < m; ++i) {
        float t = top.out_smooth[i];
        float hue = (float)i / m;
        Color color = ColorFromHSV(hue * 360, saturation, value);
        Vector2 center = {
            boundary.x + i * cell_width + cell_width / 2,
            boundary.y + boundary.height - boundary.height * 2 / 3 * t,
        };
        float radius = cell_width * 6 * sqrtf(t);
        Vector2 position = {
            .x = center.x - radius,
            .y = center.y - radius,
        };
        DrawTextureEx(texture, position, 0, 2 * radius, color);
    }
    EndShaderMode();
}

void ShowFFT(Rectangle Show_boundary) {

    size_t m = fft_analyze(GetFrameTime());

    BeginScissorMode(Show_boundary.x, Show_boundary.y, Show_boundary.width, Show_boundary.height);
    fft_render(Show_boundary, m);
    EndScissorMode();
}

void FreqDomainAnalysis() {
    fft(top.in2, 1, top.out, N);
}

void FreqDomainVisual(Rectangle boundary) {
    FreqDomainAnalysis();
    if (true) {
        float rw = (float)(boundary.width) / (N);
        size_t thr = N / 8;
        for (size_t i = 0; i < N; i++) {
            int j = (i + N / 2) % N;
            mycomplex c = top.out[j];
            float t = amp1(c) / 10;
            float freq = t * boundary.height;
            if (i <= thr || i >= N - thr)
                freq *= 1 * (1 - cos(2 * PI * ((float)i / N)));
            if (freq > boundary.height)
                freq /= 2;
            Color color = ColorFromHSV((float)i / N * 360, 0.75f, 1.0f);
            Rectangle rec = { .x = boundary.x + i * rw, .y = boundary.height - freq, .width = rw, .height = freq };
            DrawRectangleRec(rec, color);
        }
    }
    else {
        float rw = (float)(boundary.width) / (N);
        for (size_t i = 0; i < N; i++) {
            int j = (i + N / 2) % N;
            mycomplex c = top.out[j];
            float t = amp1(c);
            float freq = t * boundary.height;
            if (freq > boundary.height)
                freq /= 2;
            Color color = ColorFromHSV((float)i / N * 360, 0.75f, 1.0f);
            Rectangle rec = { .x = boundary.x + i * rw, .y = boundary.height - freq, .width = rw, .height = freq };
            DrawRectangleRec(rec, color);
        }
    }
}

float limit(float val, float lower, float upper) {
    // if val < lower return lower, else if val > upper return upper else return val;
    return (val < lower) ? (lower) : ((val > upper) ? (upper) : (val));
}

float normalize_val(float val, float lower, float upper) {

    val = limit(val, lower, upper);
    return (float)(val - lower) / (upper - lower);

}

void UpdateMusicPlaying(size_t index) {

    if ((int)index != top.CurrentSample && 0 <= (int)index && index < top.samples.count) {
        if (0 <= top.CurrentSample && top.CurrentSample < (int)top.samples.count && IsMusicValid(GetCurrentSample(top.CurrentSample).music))
            PauseMusicStream(GetCurrentSample(top.CurrentSample).music);

        top.CurrentSample = index;

        if (!GetCurrentSample(top.CurrentSample).paused)
            PlayMusicStream(GetCurrentSample(top.CurrentSample).music);

        top.len = GetMusicTimeLength(GetCurrentSample(top.CurrentSample).music);
        SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
    }

}

void Toggle_CurrenSampleState() {

    if (0 <= top.CurrentSample && top.CurrentSample < (int)top.samples.count && IsMusicValid(GetCurrentSample(top.CurrentSample).music)) {
        if (IsMusicStreamPlaying(GetCurrentSample(top.CurrentSample).music)) {
            PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
            GetCurrentSample(top.CurrentSample).paused = true;
        }
        else {
            ResumeMusicStream(GetCurrentSample(top.CurrentSample).music);
            GetCurrentSample(top.CurrentSample).paused = false;
        }
    }

}

void DrawNumber(float n, float marginx, float marginy) {

    DrawText(TextFormat("%.0f", n), w - marginx, h - marginy, 20, WHITE);

}

void Update() {
    top.btnD = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    top.btnP = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    top.btnR = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    top.mp = GetMousePosition();
    UpdateMusicStream(GetCurrentSample(top.CurrentSample).music);
    top.t0 = GetMusicTimePlayed(GetCurrentSample(top.CurrentSample).music);
    if (IsKeyPressed(KEY_SPACE)) {
        Toggle_CurrenSampleState();
    }
    top.Tracker_center.y = h - 100;
}

void ManageTracker() {
    Rectangle tracker = { .x = 0, .y = top.Tracker_center.y, .width = w, .height = 1 };
    Rectangle tracker_boundary = { .x = 0, .y = top.Tracker_center.y - Tracker_Radius, .width = w, .height = 2 * Tracker_Radius };
    DrawRectangleRec(tracker, GRAY); // line for the tracker
    float posx = (top.t0 / top.len) * w;
    bool collision_with_a_tracker_boundary = CheckCollisionPointRec(top.mp, tracker_boundary);
    if (top.btnP && collision_with_a_tracker_boundary) {
        top.trackerdrag = true;
        PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        SeekMusicStream(GetCurrentSample(top.CurrentSample).music, (posx) * (top.len / (w)));
    }
    if (top.trackerdrag) {
        posx = top.mp.x;
        posx = limit(posx, 0.0f, w);
        if (top.btnR) {
            top.trackerdrag = false;
            SeekMusicStream(GetCurrentSample(top.CurrentSample).music, (posx) * (top.len / w));
        }
        if (!top.trackerdrag && !GetCurrentSample(top.CurrentSample).paused) {
            ResumeMusicStream(GetCurrentSample(top.CurrentSample).music);
        }
    }
    DrawNumber((posx) * (top.len / w), 50.0f, 50.0f); // time indication
    top.Tracker_center.x = posx;
    DrawCircleV(top.Tracker_center, Tracker_Radius, GRAY);
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

void POPUP_ErrorFileNotSupported(char* name) {
    (void)name;
    //TODO: NOT IMPLEMENTED
}

void ManagePlayList() {

    if (IsFileDropped()) {
        FilePathList fpl = LoadDroppedFiles();
        if (fpl.count > 0) {
            for (size_t i = 0; i < fpl.count; i++) {
                char temp[20];
                file_name_to_name(fpl.paths[i], temp);

                Music music = LoadMusicStream(fpl.paths[i]);
                if (IsMusicValid(music)) {

#if var
                    AttachAudioStreamProcessor(music.stream, callbackvar);
#else
#endif
                    AttachAudioStreamProcessor(music.stream, callback);

                    nob_da_append(&top.samples, (CLITERAL(Sample) {
                        .file_name = _strdup(fpl.paths[i]),
                            .name = _strdup(temp),
                            .music = music,
                            .paused = false,
                    }));
                }
                else {
                    POPUP_ErrorFileNotSupported(fpl.paths[i]);
                }
            }
            if (top.CurrentSample == -1) {
                UpdateMusicPlaying(0);
            }
        }
        UnloadDroppedFiles(fpl);
    }
    // drawing the playlist
    static float playlist_scroll = 0;
    static float scroll_velocity = 0;
    float playlist_boundary_height = top.Tracker_center.y - Tracker_Radius;
    float item_height = playlist_boundary_height * (0.07f);
    Rectangle playlist_boundary = { .x = 0, .y = 0, .width = playlist_width, .height = playlist_boundary_height };
    bool colplaylistboundary = CheckCollisionPointRec(top.mp, playlist_boundary);

    if (CheckCollisionPointRec(top.mp, playlist_boundary)) {
        scroll_velocity *= 0.9;
        scroll_velocity += 4 * GetMouseWheelMove() * item_height;
        playlist_scroll += scroll_velocity * GetFrameTime();
    }

    // not to able to scroll up more than it should be
    if (playlist_scroll > 0) playlist_scroll = 0;
    // not to be able to scroll if the playlist page is not full
    if (playlist_scroll < 0 && top.samples.count * (item_height + 5) <= playlist_boundary_height) playlist_scroll = 0;
    // no to be able to scroll down more than it should be
    if (playlist_scroll < 0 && playlist_scroll + top.samples.count * (item_height + 5) - playlist_boundary_height <= 0)
        playlist_scroll = -1 * (top.samples.count * (item_height + 5) - playlist_boundary_height);

    playlist_boundary.y = playlist_scroll;

    BeginScissorMode(playlist_boundary.x, 0, playlist_boundary.width, playlist_boundary.height);
    for (size_t i = 0; i < top.samples.count; i++) {
        Rectangle rec = { .x = 0, .y = i * (item_height + 5) + playlist_boundary.y,
                          .width = playlist_boundary.width, .height = item_height };
        bool col_with_item = CheckCollisionPointRec(top.mp, rec);
        Color color;
        if (colplaylistboundary && col_with_item)
            color = DARKGRAY;
        else
            color = GRAY;
        // if music is selected then update the currently playing music
        if (colplaylistboundary && col_with_item && top.btnR)
            UpdateMusicPlaying(i);

        DrawRectangleRounded(rec, 0.4f, 20, color);
        DrawText(TextFormat("%d-%s", i + 1, top.samples.items[i].name), 0, rec.y + rec.height / 2 - 10, 20, WHITE);
    }
    EndScissorMode();

    if (top.samples.count > 0) {
        DrawRectangle(playlist_width, 0, 1, top.Tracker_center.y, GRAY); // line to split the playlist and the FFT
    }
    const char* samplescount/*[20]*/ = TextFormat("Samples Count : %d", top.samples.count);
    DrawText(samplescount, w - MeasureText(samplescount, 20) - 5, 0, 20, WHITE);
}

void MusicSettings() {
    // Reset Button
    Rectangle btnreset = {
        .x = 10 , .y = h - 50,
        .width = 75 , .height = 40
    };
    DrawRectangleRec(btnreset, WHITE);
    DrawText("Reset", btnreset.x + btnreset.width / 7, btnreset.y + btnreset.height / 4, 20, BLACK);

    bool btnresetcol = CheckCollisionPointRec(top.mp, btnreset);

    if (btnresetcol && top.btnR && IsMusicValid(GetCurrentSample(top.CurrentSample).music)) {
        SeekMusicStream(GetCurrentSample(top.CurrentSample).music, 0.0f);
    }

    Vector2 PlayingState_pos = { .x = btnreset.x + btnreset.width + fgap,
                                 .y = btnreset.y + btnreset.height / 2 - top.Playstate.height / 2 };
    Rectangle PlayingState_boundary = { .x = PlayingState_pos.x, .y = PlayingState_pos.y, .width = top.Playstate.width, .height = top.Playstate.height };
    if (CheckCollisionPointRec(top.mp, PlayingState_boundary) && top.btnR) {
        Toggle_CurrenSampleState();
    }

    if (GetCurrentSample(top.CurrentSample).paused) {
        DrawTextureV(top.Pausestate, PlayingState_pos, ColorBrightness(WHITE, -0.10));
    }
    else {
        DrawTextureV(top.Playstate, PlayingState_pos, ColorBrightness(WHITE, -0.10));
    }


    // Volume
    static float volRadius = 10.0f;
    Vector2 Volume_icon_pos = { .x = PlayingState_pos.x + top.Playstate.width + fgap, .y = btnreset.y + btnreset.height / 2 - top.VolumeHigh_icon.height / 2 };
    Rectangle Volume_icon_boundary = { .x = Volume_icon_pos.x, .y = Volume_icon_pos.y, .width = top.VolumeHigh_icon.width, .height = top.VolumeHigh_icon.height };
    Rectangle volline = {
        .x = Volume_icon_pos.x + top.VolumeHigh_icon.width + fgap + 20 , .y = btnreset.y + btnreset.height / 2 - volline_height / 2,
        .width = 250 , .height = volline_height
    };

    if (CheckCollisionPointRec(top.mp, Volume_icon_boundary) && top.btnR) {
        if (!top.muted) {
            top.volume = 0;
            SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
        }
        else {
            top.volume = top.volume_before_mute;
            SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
        }
    }

    DrawRectangleRounded(volline, 10.0f, 1, WHITE);
    bool colVolcir = CheckCollisionPointCircle(top.mp, top.Volume_center, volRadius);
    Rectangle Volume_boundary = { .x = volline.x, .y = top.Volume_center.y - volRadius, .width = volline.width, .height = 2 * volRadius };
    bool col_volume_boundary = CheckCollisionPointRec(top.mp, Volume_boundary);
    if (top.btnP && col_volume_boundary) {
        top.volumedrag = true;
    }
    if (top.volumedrag) {
        top.volume = normalize_val(top.mp.x, volline.x, volline.x + volline.width);
        SetMusicVolume(GetCurrentSample(top.CurrentSample).music, top.volume);
        top.volume_before_mute = top.volume;
        if (top.btnR)
            top.volumedrag = false;
    }

    if (top.volume == 0.0f) {
        DrawTextureV(top.VolumeMute_icon, Volume_icon_pos, ColorBrightness(WHITE, -0.10));
        top.muted = true;
    }
    else {
        DrawTextureV(top.VolumeHigh_icon, Volume_icon_pos, ColorBrightness(WHITE, -0.10));
        top.muted = false;
    }


    volRadius = (top.volumedrag || colVolcir) ? 14.0f : 10.0f;
    top.Volume_center.x = volline.x + top.volume * volline.width;
    top.Volume_center.y = volline.y + volline.height / 2;
    DrawCircleV(top.Volume_center, volRadius, WHITE);
    if (col_volume_boundary || top.volumedrag) {
        int Text_volume_x = volline.x + volline.width + volRadius + 5;
        int text_height = 20;
        const char* volume_text/*[16]*/ = TextFormat("Volume: %.2f%%", top.volume * 100.0f);
        DrawText(volume_text, Text_volume_x, top.Volume_center.y - text_height / 2, text_height, WHITE);
    }
}

void ManageFeatures() {

    ManagePlayList();

    ManageTracker();

    MusicSettings();
}

void Init_Screen() {
    int height = 50;
    char text[18] = "Drag & Drop Music";
    int width = MeasureText(text, height);
    DrawText(text, w / 2 - width / 2, h / 2 - height / 2, height, WHITE);
}

void toggle_capturing() {
    top.capturing = !top.capturing;
    if (top.capturing) {
        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.playback.format = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
        config.playback.channels = 2;               // Set to 0 to use the device's native channel count.
        config.sampleRate = 41000;           // Set to 0 to use the device's native sample rate.
        config.dataCallback = ma_callback;   // This function will be called when miniaudio needs more data.
        config.pUserData = NULL;   // Can be accessed from the device object (device.pUserData).

        if (ma_device_init(NULL, &config, &top.device) != MA_SUCCESS) {
            fprintf(stderr, "Microphone Not Initialized Successfuly\n");
            POPUP_ErrorFileNotSupported("Microphone Not Initialized Successfuly\n");
            top.capturing = false;
            return;
        }
        if (top.CurrentSample >= 0 && IsMusicValid(GetCurrentSample(top.CurrentSample).music))
            PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        ma_device_start(&top.device);

    }
    else {
        ma_device_uninit(&top.device);
        if (0 <= top.CurrentSample && top.CurrentSample < (int)top.samples.count && IsMusicValid(GetCurrentSample(top.CurrentSample).music) && !GetCurrentSample(top.CurrentSample).paused)
            PlayMusicStream(GetCurrentSample(top.CurrentSample).music);
        fft_clean();
    }
}

void Visualize_FFT(Rectangle boundary) {
#if var
    ShowFFT(boundary);
#else
    FreqDomainVisual(boundary);
#endif
    //if (var)
    //    ShowFFT(boundary);
    //else
    //    FreqDomainVisual(boundary);
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
    Init();
    bool FULLSCREEN = false;
    while (!WindowShouldClose())
    {
        w = GetScreenWidth();
        h = GetScreenHeight();
        if (IsKeyPressed(KEY_F))
            FULLSCREEN = !FULLSCREEN;
        if (IsKeyPressed(KEY_C))
            toggle_capturing();
        BeginDrawing();
        ClearBackground(BLACK);

        if (top.capturing) {
            Visualize_FFT((CLITERAL(Rectangle) {
                .x = 0,
                .y = 0,
                .width = w,
                .height = h,
            }));
        }
        else 
        {
           if (top.samples.count > 0) 
            {
                Update();
                Rectangle boundary;
                if (FULLSCREEN)
                {
                    boundary = (Rectangle) {
                        .x = 0,
                        .y = 0,
                        .width = w,
                        .height = h
                    };
                }
                else
                {
                    boundary = (Rectangle) {
                        .x = playlist_width,
                        .y = 0,
                        .width = w - playlist_width,
                        .height = h - tracker_height
                    };
                }
                Visualize_FFT(boundary);
                if (!FULLSCREEN)
                    ManageFeatures();
            }
            else {
                ManagePlayList();
                Init_Screen();
            }
        }

        EndDrawing();
    }

    UnloadTexture(top.Playstate);
    UnloadTexture(top.Pausestate);
    UnloadTexture(top.VolumeHigh_icon);
    UnloadTexture(top.VolumeMute_icon);
    UnloadShader(top.shader);
    // De-Initialization
    //--------------------------------------------------------------------------------------
    ma_device_uninit(&top.device);
    CloseAudioDevice();
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
