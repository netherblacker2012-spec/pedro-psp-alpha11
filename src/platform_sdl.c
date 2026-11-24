/* SDL2 backend for PSP (pspdev). Needs: psp-pacman -S sdl2 sdl2_image sdl2_mixer */
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include "platform.h"

PSP_MODULE_INFO("MyFriendPedro", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

/* PSP buttons as seen by SDL joystick */
enum { BTN_TRIANGLE = 0, BTN_CIRCLE, BTN_CROSS, BTN_SQUARE, BTN_L, BTN_R,
       BTN_DOWN, BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_SELECT, BTN_START };

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Joystick *js;
static SDL_Texture *tex[64]; static int ntex;
static Mix_Chunk *chunks[64]; static int nchunk;
static Mix_Music *music;
static char base[256] = "./assets/";
static int quitFlag;

int plat_init(const char *exe) {
    const char *p = strrchr(exe, '/'); if (!p) p = strrchr(exe, ':');
    if (p) { int n = (int)(p - exe) + 1; if (n < 200) { memcpy(base, exe, n); strcpy(base + n, "assets/"); } }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) return 1;
    win = SDL_CreateWindow("Pedro", 0, 0, 480, 272, 0);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) return 2;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    js = SDL_JoystickOpen(0);
    IMG_Init(IMG_INIT_PNG);
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 1024) == 0) Mix_AllocateChannels(12);
    return 0;
}
void plat_shutdown(void) {
    if (music) Mix_FreeMusic(music);
    Mix_CloseAudio(); IMG_Quit();
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    sceKernelExitGame();
}
void plat_path(char *out, int n, const char *name) { snprintf(out, n, "%s%s", base, name); }
void plat_poll(Input *in) {
    SDL_Event ev; while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) quitFlag = 1;
    memset(in, 0, sizeof *in);
    if (!js) return;
    float lx = SDL_JoystickGetAxis(js, 0) / 32768.0f, ly = SDL_JoystickGetAxis(js, 1) / 32768.0f;
    in->runx = lx > 0.3f ? 1 : (lx < -0.3f ? -1 : 0);
    in->jump = ly < -0.55f;
    in->crouch = ly > 0.55f;
    in->fire = SDL_JoystickGetButton(js, BTN_R);
    in->slow = SDL_JoystickGetButton(js, BTN_L);
    in->reload = SDL_JoystickGetButton(js, BTN_DOWN);
    in->nextw = SDL_JoystickGetButton(js, BTN_RIGHT);
    in->prevw = SDL_JoystickGetButton(js, BTN_LEFT);
    in->pause = SDL_JoystickGetButton(js, BTN_SELECT);
    in->quit = quitFlag || SDL_JoystickGetButton(js, BTN_START);
    /* face buttons = aim: triangle up, cross down, square left, circle right (combine for diagonals) */
    float ax = (SDL_JoystickGetButton(js, BTN_CIRCLE) ? 1.0f : 0) - (SDL_JoystickGetButton(js, BTN_SQUARE) ? 1.0f : 0);
    float ay = (SDL_JoystickGetButton(js, BTN_CROSS) ? 1.0f : 0) - (SDL_JoystickGetButton(js, BTN_TRIANGLE) ? 1.0f : 0);
    float m = ax * ax + ay * ay;
    if (m > 0.1f) { m = 1.0f / (float)__builtin_sqrtf(m); in->aim = 1; in->ax = ax * m; in->ay = ay * m; }
}
void plat_clear(int r, int g, int b) { SDL_SetRenderDrawColor(ren, r, g, b, 255); SDL_RenderClear(ren); }
void plat_present(void) { SDL_RenderPresent(ren); }
int plat_load_tex(const char *name) {
    char path[300]; plat_path(path, sizeof path, name);
    SDL_Surface *s = IMG_Load(path); if (!s || ntex >= 64) return -1;
    tex[ntex] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
    SDL_SetTextureBlendMode(tex[ntex], SDL_BLENDMODE_BLEND);
    return ntex++;
}
void plat_draw(int t, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
               double angle, int cx, int cy, int flip, int alpha) {
    if (t < 0) return;
    SDL_Rect s = {sx, sy, sw, sh}, d = {dx, dy, dw, dh}; SDL_Point c = {cx, cy};
    SDL_SetTextureAlphaMod(tex[t], (Uint8)alpha);
    SDL_RenderCopyEx(ren, tex[t], &s, &d, angle, &c, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
void plat_rect(int x, int y, int w, int h, int r, int g, int b, int a) {
    SDL_SetRenderDrawColor(ren, r, g, b, a); SDL_Rect q = {x, y, w, h}; SDL_RenderFillRect(ren, &q);
}
int plat_load_sfx(const char *name) {
    char path[300]; plat_path(path, sizeof path, name);
    Mix_Chunk *c = Mix_LoadWAV(path); if (!c || nchunk >= 64) return -1;
    chunks[nchunk] = c; return nchunk++;
}
void plat_play_sfx(int id, int vol) {
    if (id < 0) return; Mix_VolumeChunk(chunks[id], vol); Mix_PlayChannel(-1, chunks[id], 0);
}
void plat_play_music(const char *name) {
    char path[300]; plat_path(path, sizeof path, name);
    music = Mix_LoadMUS(path); if (music) Mix_PlayMusic(music, -1);
}
void plat_music_volume(int v) { Mix_VolumeMusic(v); }
uint32_t plat_ticks(void) { return SDL_GetTicks(); }
void plat_delay(uint32_t ms) { SDL_Delay(ms); }
