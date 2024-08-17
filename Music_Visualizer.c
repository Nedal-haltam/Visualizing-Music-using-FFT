#include "raylib.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"
#include "complex.h"
#include "math.h"
#include "nob.c"
#include "miniaudio.c"
#define var true
#if var
    #define N (1<<13)
#else
    #define N (1<<11)
#endif

#define Tracker_Radius 20
#define fgap 20
#define playlist_width 220
#define tracker_height 130

#define FPS 60
#define Display_FPS 70

#define GetCurrentSample(cs) top.samples.items[cs]
#define PLAYSTATEPATH ".\\resources\\icons\\play.png"
#define PAUSESTATEPATH ".\\resources\\icons\\pause.png"
#define VOLUMEHIGHICONPATH ".\\resources\\icons\\volumehigh.png"
#define VOLUMEMUTEICONPATH ".\\resources\\icons\\mute.png"

#define scale 0.07f
#define GLSL_VERSION 330

#define cfromreal(re) (re)
#define cfromimag(im) ((im)*I)
#define mulcc(a, b) ((a)*(b))
#define addcc(a, b) ((a)+(b))
#define subcc(a, b) ((a)-(b))


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
    Sample *items;
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
    float complex out[N];
    
    // Visualized Smooth Buffers
    float in_raw[N];
    float in_win[N];
    float complex out_raw[N];
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

float amp(float complex z) {
    float a = crealf(z);
    float b = cimagf(z);
    return logf(a*a + b*b);
}

float amp1(float complex z) {
    return fabsf(crealf(z));
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
    
    Image icon = LoadImage(".\\resources\\icons\\images.png");
    SetWindowIcon(icon);
    top.shader = LoadShader(NULL, ".\\resources\\shaders\\glsl330\\circle.fs");
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
    SetRandomSeed(0);
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
    top.Tracker_center = (Vector2) {.x = 0, .y = h - 100 };
    top.mp = (Vector2) { .x = 0, .y = 0 };
    top.Volume_center = (Vector2) { .x = 0, .y = 0 };
    load_assets();
}

void fft_clean(void) {
    memset(top.in_raw, 0, sizeof(top.in_raw));
    memset(top.in_win, 0, sizeof(top.in_win));
    memset(top.out_raw, 0, sizeof(top.out_raw));
    memset(top.out_log, 0, sizeof(top.out_log));
    memset(top.out_smooth, 0, sizeof(top.out_smooth));
    memset(top.out_smear, 0, sizeof(top.out_smear));
    memset(top.in, 0, sizeof(top. in));
    memset(top.in2, 0, sizeof(top.in2));
    memset(top.out, 0, sizeof(top.out));
}

void fftt(float input[], size_t stride , float complex output[] , size_t n) {
    
    if (n < 1)
        return;
    
    if (n == 1) {
        output[0] = input[0];
        return;
    }
    
    fftt(input , stride*2 , output , n/2);
    fftt(input + stride, stride*2 , output + n/2 , n/2);
    
    for (size_t i = 0; i < n/2; i++) {
        float t = (float)i / n;
        float complex h = cexp(-2*I*PI*t)*output[i + n/2];
        float complex e = output[i];
        output[i]     = e + h;
        output[i+n/2] = e - h;
    }
}

void fft(float in[], size_t stride, float complex out[], size_t n) {
    assert(n > 0);

    if (n == 1) {
        out[0] = cfromreal(in[0]);
        return;
    }

    fft(in, stride*2, out, n/2);
    fft(in + stride, stride*2,  out + n/2, n/2);

    for (size_t k = 0; k < n/2; ++k) {
        float t = (float)k/n;
        float complex v = mulcc(cexpf(cfromimag(-2*PI*t)), out[k + n/2]);
        float complex e = out[k];
        out[k]       = addcc(e, v);
        out[k + n/2] = subcc(e, v);
    }
}

void callbackvar(void *bufferData, unsigned int frames) {
    if (!top.capturing && GetCurrentSample(top.CurrentSample).paused)
        return;
    
    // https://cdecl.org/?q=float+%28*fs%29%5B2%5D
    float (*fs)[2] = bufferData;
    
    for (size_t i = 0; i < frames; ++i) {
        memmove(top.in_raw, top.in_raw + 1, (N - 1)*sizeof(top.in_raw[0]));
        top.in_raw[N-1] = fs[i][0];
    }
    
}

void TimeDomainAnalysis(void* bufferData, unsigned int frames) {
    if (!top.capturing && GetCurrentSample(top.CurrentSample).paused)
        return;
    if (frames > N) frames = N;
    float (*fs)[2] = bufferData;
    for (size_t i = 0; i < frames; i++) {
        memmove(top.in, top.in + 1, sizeof(top.in[0])*(N - 1));
        top.in[N-1] = fs[i][0];
    }
    
    for (size_t i = 0; i < N; i++) {
        float t = (float)i / N;
        float hann = (0.5 - 0.5*cos(2*PI*t));
        top.in2[i] = top.in[i]*pow(hann,10);
    }
}

void callback(void *bufferData, unsigned int frames) {
    
    TimeDomainAnalysis(bufferData, frames);

}

void ma_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    if (var)
        callbackvar((void*)pInput,frameCount);
    else
        callback((void*)pInput,frameCount);
    (void)pOutput;
    (void)pDevice;
}

size_t fft_analyze(float dt) {
    // Apply the Hann Window on the Input - https://en.wikipedia.org/wiki/Hann_function
    for (size_t i = 0; i < N; ++i) {
        float t = (float)i/(N - 1);
        float hann = 0.5 - 0.5*cosf(2*PI*t);
        top.in_win[i] = top.in_raw[i]*hann;
    }

    // FFT
    fft(top.in_win, 1, top.out_raw, N);

    // "Squash" into the Logarithmic Scale
    float step = 1.06;
    float lowf = 1.0f;
    size_t m = 0;
    float max_amp = 1.0f;
    for (float f = lowf; (size_t) f < N/2; f = ceilf(f*step)) {
        float f1 = ceilf(f*step);
        float a = 0.0f;
        for (size_t q = (size_t) f; q < N/2 && q < (size_t) f1; ++q) {
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
        top.out_smooth[i] += (top.out_log[i] - top.out_smooth[i])*smoothness*dt;
        float smearness = 3;
        top.out_smear[i] += (top.out_smooth[i] - top.out_smear[i])*smearness*dt;
    }

    return m;
}

void fft_render(Rectangle boundary, size_t m) {
    // The width of a single bar
    float cell_width = boundary.width/m;

    // Global color parameters
    float saturation = 0.75f;
    float value = 1.0f;

    // Display the Bars
    for (size_t i = 0; i < m; ++i) {
        float t = top.out_smooth[i];
        float hue = (float)i/m;
        Color color = ColorFromHSV(hue*360, saturation, value);
        Vector2 startPos = {
            boundary.x + i*cell_width + cell_width/2,
            boundary.y + boundary.height - boundary.height*2/3*t,
        };
        Vector2 endPos = {
            boundary.x + i*cell_width + cell_width/2,
            boundary.y + boundary.height,
        };
        float thick = cell_width/3*sqrtf(t);
        DrawLineEx(startPos, endPos, thick, color);
    }

    Texture2D texture = { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    // Display the Smears
    SetShaderValue(top.shader, top.shader_radius_location, (float[1]){ 0.3f }, SHADER_UNIFORM_FLOAT);
    SetShaderValue(top.shader, top.shader_power_location, (float[1]){ 3.0f }, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(top.shader);
    for (size_t i = 0; i < m; ++i) {
        float start = top.out_smear[i];
        float end = top.out_smooth[i];
        float hue = (float)i/m;
        Color color = ColorFromHSV(hue*360, saturation, value);
        Vector2 startPos = {
            boundary.x + i*cell_width + cell_width/2,
            boundary.y + boundary.height - boundary.height*2/3*start,
        };
        Vector2 endPos = {
            boundary.x + i*cell_width + cell_width/2,
            boundary.y + boundary.height - boundary.height*2/3*end,
        };
        float radius = cell_width*3*sqrtf(end);
        Vector2 origin = {0};
        if (endPos.y >= startPos.y) {
            Rectangle dest = {
                .x = startPos.x - radius/2,
                .y = startPos.y,
                .width = radius,
                .height = endPos.y - startPos.y
            };
            Rectangle source = {0, 0, 1, 0.5};
            DrawTexturePro(texture, source, dest, origin, 0, color);
        } else {
            Rectangle dest = {
                .x = endPos.x - radius/2,
                .y = endPos.y,
                .width = radius,
                .height = startPos.y - endPos.y
            };
            Rectangle source = {0, 0.5, 1, 0.5};
            DrawTexturePro(texture, source, dest, origin, 0, color);
        }
    }
    EndShaderMode();

    // Display the Circles
    SetShaderValue(top.shader, top.shader_radius_location, (float[1]){ 0.07f }, SHADER_UNIFORM_FLOAT);
    SetShaderValue(top.shader, top.shader_power_location, (float[1]){ 5.0f }, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(top.shader);
    for (size_t i = 0; i < m; ++i) {
        float t = top.out_smooth[i];
        float hue = (float)i/m;
        Color color = ColorFromHSV(hue*360, saturation, value);
        Vector2 center = {
            boundary.x + i*cell_width + cell_width/2,
            boundary.y + boundary.height - boundary.height*2/3*t,
        };
        float radius = cell_width*6*sqrtf(t);
        Vector2 position = {
            .x = center.x - radius,
            .y = center.y - radius,
        };
        DrawTextureEx(texture, position, 0, 2*radius, color);
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
    fftt(top.in2, 1, top.out, N);
}

void FreqDomainVisual(Rectangle boundary) {
    FreqDomainAnalysis();
    if (true) {
        float rw = (float)(boundary.width) / (N);
        int thr = N/8;
        for (size_t i = 0; i < N; i++) {
            int j = (i + N/2) % N;
            float complex c = top.out[j];
            float t = amp1(c)/10;
            float freq = t * boundary.height;
            if (i <= thr || i >= N - thr)
                freq *= 1*(1 - cos(2*PI*((float) i / N)));
            if (freq > boundary.height)
                freq /= 2;
            Color color = ColorFromHSV((float)i/N*360, 0.75f, 1.0f);
            Rectangle rec = { .x = boundary.x+i*rw, .y = boundary.height - freq, .width = rw, .height = freq };
            DrawRectangleRec(rec, color);
        }
    }
    else {
        float rw = (float)(boundary.width) / (N);
        for (size_t i = 0; i < N; i++) {
            int j = (i + N/2) % N;
            float complex c = top.out[j];
            float t = amp1(c);
            float freq = t * boundary.height;
            if (freq > boundary.height)
                freq /= 2;
            Color color = ColorFromHSV((float)i/N*360, 0.75f, 1.0f);
            Rectangle rec = { .x = boundary.x+i*rw, .y = boundary.height - freq, .width = rw, .height = freq };
            DrawRectangleRec(rec, color);
        }
    }
}

float normalize_volx(float val , float lower , float upper) {
    
    if (val < lower)
        val = lower;
    else if (val > upper)
        val = upper;
    val = (float)(val - lower) / (upper - lower);
    return val;
}

void UpdateMusicPlaying(size_t index) {
    
    if (0 <= index && index < top.samples.count && index != top.CurrentSample) {
        if (0 <= top.CurrentSample && top.CurrentSample < top.samples.count)
            PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        top.CurrentSample = index;
        if (!GetCurrentSample(top.CurrentSample).paused) PlayMusicStream(GetCurrentSample(top.CurrentSample).music);
        top.len = GetMusicTimeLength(GetCurrentSample(top.CurrentSample).music);
        SetMusicVolume(GetCurrentSample(top.CurrentSample).music , top.volume);
    }
}

void Toggle_CurrenSampleState() {
    
    if (IsMusicStreamPlaying(GetCurrentSample(top.CurrentSample).music)) {
        PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        GetCurrentSample(top.CurrentSample).paused = true;
    }
    else {
        ResumeMusicStream(GetCurrentSample(top.CurrentSample).music);
        GetCurrentSample(top.CurrentSample).paused = false;
    }
    
}

void DrawNumber(float n, float marginx, float marginy) {
    DrawText(TextFormat("%.0f", n), w - marginx , h - marginy, 20, WHITE);
    
}

void Update() {
    top.btnD   = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    top.btnP   = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    top.btnR   = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    top.mp = GetMousePosition();
    UpdateMusicStream(GetCurrentSample(top.CurrentSample).music);    
    top.t0  = GetMusicTimePlayed(GetCurrentSample(top.CurrentSample).music);
    DrawNumber(top.t0, 50.0f, 50.0f);        // time indication
    if (IsKeyPressed(KEY_SPACE)) {
        Toggle_CurrenSampleState();
    }
}

void ManageTracker() {
    Rectangle tracker = { .x = 0, .y = h - 100, .width = w, .height = 1 };
    DrawRectangleRec(tracker, GRAY); // line for the tracker
    float posx = (top.t0/top.len)*w;        
    if (top.btnP && (top.Tracker_center.y - Tracker_Radius <= top.mp.y && top.mp.y <= top.Tracker_center.y + Tracker_Radius)) {
        top.trackerdrag = true;
        PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        SeekMusicStream(GetCurrentSample(top.CurrentSample).music, (posx)*(top.len/(w)));
    }
    if (top.trackerdrag) {
        posx = top.mp.x;
        if (posx <= 0) posx = 0.01f;
        if (posx >= w) posx = w;
        PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        SeekMusicStream(GetCurrentSample(top.CurrentSample).music, (posx)*(top.len/w));
        if (top.btnR)
            top.trackerdrag = false;
        if (!top.trackerdrag && !GetCurrentSample(top.CurrentSample).paused) {
            ResumeMusicStream(GetCurrentSample(top.CurrentSample).music);
        }
    }
    top.Tracker_center.x = posx;
    top.Tracker_center.y = h - 100;
    DrawCircleV(top.Tracker_center, Tracker_Radius, GRAY);
}

void file_name_to_name(const char* file_name, char* name) {
    size_t index = strrchr(file_name, '\\') - file_name + 1;
    size_t j;
    for (j = index; j < index + 17; j++) {
        if (file_name[j] == '.')
            break;
        name[j-index] = file_name[j];
    }
    name[j-index] = '\0';
}

void POPUP_ErrorFileNotSupported(char* name) {
    
}

void ManagePlayList() {
    
    if (IsFileDropped()) {
        FilePathList fpl = LoadDroppedFiles();
        if (fpl.count > 0) {
            for (size_t i = 0; i < fpl.count; i++) {
                char temp[20];
                file_name_to_name(fpl.paths[i], temp);
                
                Music music = LoadMusicStream(fpl.paths[i]);
                if (IsMusicReady(music)) {
                    if (var)
                        AttachAudioStreamProcessor(music.stream, callbackvar);
                    else
                        AttachAudioStreamProcessor(music.stream, callback);
                    nob_da_append(&top.samples, (CLITERAL(Sample) {
                        .file_name = strdup(fpl.paths[i]), 
                        .name = strdup(temp),
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
    float playlist_boundary_height = h - 100;
    float item_height = playlist_boundary_height*(0.07f);
    Rectangle playlist_boundary = { .x = 0, .y = 0, .width = playlist_width, .height = playlist_boundary_height };
    
    
    if (CheckCollisionPointRec(top.mp, playlist_boundary)) {
        scroll_velocity *= 0.9;
        scroll_velocity += 4*GetMouseWheelMove() * item_height;
        playlist_scroll += scroll_velocity * GetFrameTime(); 
    }
    
        
    if (playlist_scroll > 0) playlist_scroll = 0;
    if (playlist_scroll < 0 && top.samples.count * (item_height + 5) <= playlist_boundary_height) playlist_scroll = 0;
    if (playlist_scroll < 0 && playlist_scroll + top.samples.count * (item_height + 5) - playlist_boundary_height <= 0)
        playlist_scroll = -1*(top.samples.count * (item_height + 5) - playlist_boundary_height);
    
    playlist_boundary.y = playlist_scroll;
    
    BeginScissorMode(playlist_boundary.x, 0, playlist_boundary.width, playlist_boundary.height-Tracker_Radius);
    for (size_t i = 0; i < top.samples.count; i++) {
        Rectangle rec = { .x = 0, .y = i*item_height + playlist_boundary.y+5*i, 
                          .width = playlist_boundary.width, .height = item_height };
        bool colrec = CheckCollisionPointRec(top.mp, rec);
        Color color;
        if (colrec) {
            color = BLUE;
            if (top.btnR) {
                UpdateMusicPlaying(i);
            }
        }   
        else {
            color = RED;
        }
        DrawRectangleRounded(rec, 0.4f, 20, color);
        DrawText(TextFormat("%d-%s",i+1,(char*)top.samples.items[i].name), 0, rec.y + rec.height/2 - 10, 20, WHITE);
    }
    EndScissorMode();
    
    if (top.samples.count > 0) {
        DrawRectangle(playlist_width, 0, 1, h - 100, GRAY); // line to split the playlist and the FFT
    }
    const char* samplescount = TextFormat("Samples Count : %d",top.samples.count);
    DrawText(samplescount, w - MeasureText(samplescount, 20) - 5, 0, 20, WHITE);
    //free(samplescount);
}

void MusicSettings() {
    // Reset Button
    Rectangle btnreset = {
        .x = 10 , .y = h - 50,
        .width = 75 , .height = 40
    };
    DrawRectangleRec(btnreset , WHITE);
    DrawText("Reset" , btnreset.x + btnreset.width/7, btnreset.y + btnreset.height/4 , 20, BLACK);
    
    bool btnresetcol = CheckCollisionPointRec(top.mp, btnreset);
    
    if (IsMusicReady(GetCurrentSample(top.CurrentSample).music) && btnresetcol && top.btnR) {
        SeekMusicStream(GetCurrentSample(top.CurrentSample).music, 0.0f);
    }
    
    Vector2 PlayingState_pos = { .x = btnreset.x + btnreset.width + fgap,     
                                 .y = btnreset.y + btnreset.height/2 - top.Playstate.height/2 };    
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
    static float volx = 0;
    static float volRadius = 7.0f;
    
    Vector2 Volume_icon_pos = { .x = PlayingState_pos.x + top.Playstate.width + fgap, .y =  btnreset.y + btnreset.height/2 - top.VolumeHigh_icon.height/2 };
    Rectangle Volume_boundary = { .x =  Volume_icon_pos.x, .y = Volume_icon_pos.y, .width = top.VolumeHigh_icon.width, .height = top.VolumeHigh_icon.height };
    float volline_height = 6;
    Rectangle volline = {
        .x = Volume_icon_pos.x + top.VolumeHigh_icon.width + fgap+20 , .y = btnreset.y + btnreset.height/2 - volline_height/2,
        .width = 250 , .height = volline_height
    };
    
    if (CheckCollisionPointRec(top.mp, Volume_boundary) && top.btnR) {
        if (!top.muted) {
            top.volume_before_mute = volline.x + top.volume * volline.width;
            //ChangeVolume(0);
            volx = normalize_volx(0 , volline.x , volline.x + volline.width);                 
            top.volume = volx;                                                     
            SetMusicVolume(GetCurrentSample(top.CurrentSample).music , top.volume);
        }
        else {
            //ChangeVolume(top.volume_before_mute);
            volx = normalize_volx(top.volume_before_mute , volline.x , volline.x + volline.width);
            top.volume = volx;                                                     
            SetMusicVolume(GetCurrentSample(top.CurrentSample).music , top.volume);
        }   
    }
    
    if (top.volume == 0.0f) {
        DrawTextureV(top.VolumeMute_icon, Volume_icon_pos, ColorBrightness(WHITE, -0.10));
        top.muted = true;
    }
    else {
        DrawTextureV(top.VolumeHigh_icon, Volume_icon_pos, ColorBrightness(WHITE, -0.10));
        top.muted = false;
    }
    
    DrawRectangleRounded(volline, 10.0f, 1, WHITE);
    bool colVolcir = CheckCollisionPointCircle(top.mp , top.Volume_center , volRadius);
    bool colvolline = CheckCollisionPointRec(top.mp, volline);
    bool ybound = top.Volume_center.y - volRadius <= top.mp.y && top.mp.y <= top.Volume_center.y + volRadius;
    bool xbound = volline.x <= top.mp.x && top.mp.x <= volline.x + volline.width;
    if (top.btnP && (colvolline || colVolcir || ( xbound && ybound ))) {
        top.volumedrag = true;
    }
    if (top.volumedrag) {
        volx = top.mp.x;
        //ChangeVolume(volx);
        volx = normalize_volx(volx , volline.x , volline.x + volline.width);  
        top.volume = volx;
        if (top.volume == 0) top.volume_before_mute = 0;
        SetMusicVolume(GetCurrentSample(top.CurrentSample).music , top.volume);
        if (top.btnR)
            top.volumedrag = false;
    }
    volRadius = (top.volumedrag || colVolcir) ? 14.0f : 10.0f;
    top.Volume_center.x = volline.x + volx * volline.width;
    top.Volume_center.y = volline.y + volline.height/2;
    DrawCircleV(top.Volume_center, volRadius, WHITE);
    if (colVolcir || colvolline || top.volumedrag) {
        int Text_volume_x = volline.x + volline.width + 10;
        const char* volume_text = TextFormat("Volume: %.2f%%", top.volume*100.0f);
        DrawText(volume_text, Text_volume_x, volline.y - 10, 20, WHITE);
        //free(volume_text);
    }
}

void ManageFeatures() {

    ManagePlayList();

    ManageTracker();

    MusicSettings();    
}

void Init_Screen() {
    int height = 50;
    const char *text = "Drag & Drop Music";
    int width = MeasureText(text, height);
    DrawText(text, w/2 - width/2, h/2 - height/2, height, WHITE);
    //free(text);
}

void toggle_capturing() {
    top.capturing = !top.capturing;
    if (top.capturing) {
        if (top.CurrentSample >= 0 && IsMusicReady(GetCurrentSample(top.CurrentSample).music))
            PauseMusicStream(GetCurrentSample(top.CurrentSample).music);
        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.playback.format   = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
        config.playback.channels = 2;               // Set to 0 to use the device's native channel count.
        config.sampleRate        = 41000;           // Set to 0 to use the device's native sample rate.
        config.dataCallback      = ma_callback;   // This function will be called when miniaudio needs more data.
        config.pUserData         = NULL;   // Can be accessed from the device object (device.pUserData).
        
        if (ma_device_init(NULL, &config, &top.device) != MA_SUCCESS) {
            exit(1);
        }
        
        ma_device_start(&top.device);

    }
    else {
        ma_device_uninit(&top.device);
        if (top.CurrentSample >= 0 && IsMusicReady(GetCurrentSample(top.CurrentSample).music) && !GetCurrentSample(top.CurrentSample).paused)
            PlayMusicStream(GetCurrentSample(top.CurrentSample).music);
        fft_clean();
    }
}

void Visualize_FFT(Rectangle boundary) {

    if (var)
        ShowFFT(boundary);
    else
        FreqDomainVisual(boundary);
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
    Init();
    // Main loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        w = GetScreenWidth();
        h = GetScreenHeight();
        if (IsKeyPressed(KEY_F))
            ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_C))
            toggle_capturing();
        BeginDrawing();
        ClearBackground(BLACK);

        if (!top.capturing) {
            
            if (top.samples.count > 0) {
                
                Update();
                Visualize_FFT((CLITERAL(Rectangle) {
                    .x = playlist_width,
                    .y = 0,
                    .width = w - playlist_width,
                    .height = h - tracker_height
                }));
                ManageFeatures();
            }
            else {
                ManagePlayList();
                Init_Screen();
            }
            
        }
        else {
            Visualize_FFT((CLITERAL(Rectangle) {
                .x = 0,
                .y = 0,
                .width = w,
                .height = h,
            }));
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
    CloseAudioDevice();
    ma_device_uninit(&top.device);
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
