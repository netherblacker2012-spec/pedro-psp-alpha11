#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
typedef struct {
    int runx;                       /* -1, 0, 1 (analog left/right) */
    int jump, crouch;               /* analog up / down */
    int fire, slow;                 /* R, L */
    int reload, nextw, prevw;       /* D-pad down, right, left */
    int pause, quit;                /* Select, Start */
    int aim;                        /* 1 when a face button is held */
    float ax, ay;                   /* normalized aim direction from face buttons */
} Input;

int  plat_init(const char *exe_path);       /* returns 0 on success */
void plat_shutdown(void);
void plat_path(char *out, int n, const char *name);
void plat_poll(Input *in);
void plat_clear(int r, int g, int b);
void plat_present(void);
int  plat_load_tex(const char *name);       /* returns texture id or -1 */
void plat_draw(int tex, int sx, int sy, int sw, int sh,
               int dx, int dy, int dw, int dh,
               double angle, int cx, int cy, int flip_h, int alpha);
void plat_rect(int x, int y, int w, int h, int r, int g, int b, int a);
int  plat_load_sfx(const char *name);
void plat_play_sfx(int id, int volume);     /* volume 0..128 */
void plat_play_music(const char *name);
void plat_music_volume(int volume);         /* 0..128 */
uint32_t plat_ticks(void);
void plat_delay(uint32_t ms);
#endif
