/* Headless backend for testing the game logic on a PC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
static uint32_t t; static int frame, ntex;
int plat_init(const char *exe) { (void)exe; return 0; }
void plat_shutdown(void) {}
void plat_path(char *out, int n, const char *name) {
    const char *d = getenv("ASSETS"); snprintf(out, n, "%s/%s", d ? d : "assets", name);
}
void plat_poll(Input *in) {
    memset(in, 0, sizeof *in); frame++;
    /* scripted run: walk right, jump now and then, aim right, fire bursts, slow-mo in the middle */
    in->runx = 1; in->jump = (frame / 40) % 3 == 0;
    in->aim = 1; in->ax = 1.0f; in->ay = 0.0f; in->fire = (frame / 20) % 2;
    in->slow = (frame > 900 && frame < 1000);
    if (frame > 2600) in->quit = 1;
}
void plat_clear(int r, int g, int b) { (void)r; (void)g; (void)b; }
void plat_present(void) { t += 31; }
int plat_load_tex(const char *name) { (void)name; return ntex++; }
void plat_draw(int a, int b, int c, int d, int e, int f, int g, int h, int i, double an, int cx, int cy, int fl, int al)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)an;(void)cx;(void)cy;(void)fl;(void)al; }
void plat_rect(int x, int y, int w, int h, int r, int g, int b, int a) { (void)x;(void)y;(void)w;(void)h;(void)r;(void)g;(void)b;(void)a; }
int plat_load_sfx(const char *name) { (void)name; return ntex++; }
void plat_play_sfx(int id, int v) { (void)id; (void)v; }
void plat_play_music(const char *n) { (void)n; }
void plat_music_volume(int v) { (void)v; }
uint32_t plat_ticks(void) { return t; }
void plat_delay(uint32_t ms) { (void)ms; }
