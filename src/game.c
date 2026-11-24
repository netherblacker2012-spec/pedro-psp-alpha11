/* My Friend Pedro (2014 Flash) - PSP port, alpha 1: level 1 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "level.h"
#include "atlas.h"
#include "window.h"
#include "gang.h"

#define SW 480
#define SH 272
#define FPS 32
#define PI 3.14159265f
#define MAX_BULLETS 96
#define MAX_PART 160

typedef struct { int mag, total, maxmag, cd, pellets, power, spread10, sfx; } Weapon;
/* weapons 1..7: knife, pistol, dual pistols, uzi, dual uzi, shotgun, m16 */
static const Weapon WEAPONS[7] = {
    {0, 0, 0, 14, 0, 0, 0, -1},
    {18, 36, 18, 8, 1, 7, 10, 0},
    {36, 36, 36, 4, 1, 7, 18, 0},
    {32, 64, 32, 3, 1, 5, 30, 1},
    {64, 64, 64, 2, 1, 5, 40, 1},
    {8, 16, 8, 20, 10, 3, 70, 2},
    {30, 60, 30, 4, 1, 7, 14, 3},
};

typedef struct { float x, y, vx, vy; int power, friendly, alive; } Bullet;
typedef struct { float x, y, vx, vy; int life, r, g, b; } Particle;
typedef struct {
    float x, y, vx, vy; int health, dead, dir, timer, gunTimer, aim, deadTimer, hit, moving, engaged, deadKind, flash;
    float anim;
} Enemy;
typedef struct {
    float x, y, vx, vx2, vy, jumpPower;
    int onGround, jumpReady, dir, health;
    int weapon, mag[7], total[7], gunTimer, reloadTimer, jumpHeld;
    float energy, aimx, aimy, runFrame;
} Player;

static Player P;
static Enemy E[N_ENEMIES];
static Bullet B[MAX_BULLETS];
static Particle PT[MAX_PART];
static unsigned char *col;
static int tex_level[LV_TILES], tex_atlas, tex_gang;
static int pflash = 0;
static int sfx[8], sfx_hit, sfx_rico, sfx_slow, sfx_cock;
static float camx, camy, timescale = 1.0f, slowTarget = 1.0f;
static int windowBroken = 0;
static int paused = 0;
static int kills = 0, won = 0, lost = 0, frameCount = 0;
static unsigned rngs = 12345u;

static int rnd(int n) { rngs = rngs * 1664525u + 1013904223u; return (int)((rngs >> 8) % (unsigned)n); }

static int solid(float wx, float wy) {
    if (!windowBroken && wx >= WIN_X0 && wx <= WIN_X1 && wy >= WIN_Y0 && wy <= WIN_Y1) return 1;
    int px = (int)floorf((wx - LV_X0) * LV_S), py = (int)floorf((wy - LV_Y0) * LV_S);
    if (px < 0 || px >= LV_W || py < 0 || py >= LV_H) return 1;
    return (col[py * LV_STRIDE + (px >> 3)] >> (7 - (px & 7))) & 1;
}

static void spawn_part(float x, float y, int r, int g, int b, int n, float spd) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < MAX_PART; i++) if (PT[i].life <= 0) {
            PT[i].x = x; PT[i].y = y;
            PT[i].vx = ((rnd(200) - 100) / 100.0f) * spd; PT[i].vy = ((rnd(200) - 140) / 100.0f) * spd;
            PT[i].life = 12 + rnd(14); PT[i].r = r; PT[i].g = g; PT[i].b = b; break;
        }
}

static void fire_bullet(float x, float y, float ang, int power, int friendly) {
    for (int i = 0; i < MAX_BULLETS; i++) if (!B[i].alive) {
        B[i].x = x; B[i].y = y; B[i].vx = cosf(ang) * 14.0f; B[i].vy = sinf(ang) * 14.0f;
        B[i].power = power; B[i].friendly = friendly; B[i].alive = 1; return;
    }
}

static float enemy_feet(const Enemy *e) { return e->y + (float)(GA_GIDLE[0].h - GA_GIDLE[0].py) / LV_S; }

static void init_game(void) {
    memset(&P, 0, sizeof P);
    P.x = PLAYER_X; P.y = PLAYER_Y; P.dir = 1; P.health = 100; P.weapon = 2; P.energy = 100;
    P.aimx = 1; P.aimy = 0;
    for (int i = 0; i < 7; i++) { P.mag[i] = WEAPONS[i].mag; P.total[i] = WEAPONS[i].total; }
    memset(B, 0, sizeof B); memset(PT, 0, sizeof PT);
    for (int i = 0; i < N_ENEMIES; i++) {
        memset(&E[i], 0, sizeof E[i]);
        E[i].x = ENEMY_POS[i][0]; E[i].y = ENEMY_POS[i][1]; E[i].health = 20;
        E[i].dir = -1; E[i].timer = 30 + rnd(40); E[i].gunTimer = 20 + rnd(40);
    }
    kills = 0; won = lost = 0; frameCount = 0; windowBroken = 0;
}

static void give_ammo(void) {  /* enemies drop ammo (alpha simplification) */
    for (int i = 1; i < 7; i++) { P.total[i] += WEAPONS[i].maxmag / 2 + 2; if (P.total[i] > 999) P.total[i] = 999; }
}

static void damage_player(int d) {
    P.health -= d; spawn_part(P.x, P.y - 10, 180, 0, 0, 8, 2.0f);
    if (sfx_hit >= 0) plat_play_sfx(sfx_hit, 90);
    if (P.health <= 0) { P.health = 0; lost = 1; }
}

static void update_bullets(float ts) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &B[i]; if (!b->alive) continue;
        float steps = 2.0f;  /* sub-steps so thin walls are not skipped */
        for (int s = 0; s < 2 && b->alive; s++) {
            b->x += b->vx * ts / steps; b->y += b->vy * ts / steps;
            if (!windowBroken && b->friendly && b->x >= WIN_X0 && b->x <= WIN_X1 && b->y >= WIN_Y0 && b->y <= WIN_Y1) {
                windowBroken = 1; spawn_part(b->x, b->y, 200, 230, 255, 30, 4.0f); b->alive = 0; break;
            }
            if (solid(b->x, b->y)) {
                spawn_part(b->x, b->y, 255, 210, 80, 5, 3.0f);
                if (sfx_rico >= 0 && rnd(3) == 0) plat_play_sfx(sfx_rico, 50);
                b->alive = 0; break;
            }
            if (fabsf(b->x - P.x) > 520 || fabsf(b->y - P.y) > 420) { b->alive = 0; break; }
            if (b->friendly) {
                for (int e = 0; e < N_ENEMIES; e++) {
                    Enemy *en = &E[e]; if (en->dead) continue;
                    float feet = enemy_feet(en);
                    if (fabsf(b->x - en->x) < 16 && b->y > feet - 66 && b->y < feet) {
                        int dmg = b->power; if (b->y < feet - 50) dmg *= 3;  /* headshot */
                        en->health -= dmg; en->hit = 6;
                        spawn_part(b->x, b->y, 190, 0, 0, 10, 2.5f);
                        if (sfx_hit >= 0) plat_play_sfx(sfx_hit, 80);
                        if (en->health <= 0) { en->dead = 1; en->deadTimer = 400; en->anim = 0; en->deadKind = (b->vx * en->dir < 0) ? 0 : 1; kills++; P.energy += 20; if (P.energy > 100) P.energy = 100; give_ammo(); }
                        else { en->timer = 20; }
                        b->alive = 0; break;
                    }
                }
            } else if (fabsf(b->x - P.x) < 14 && b->y > P.y - 38 && b->y < P.y + 40) {
                damage_player(b->power > 4 ? 4 : 2); b->alive = 0;
            }
        }
    }
}

static void player_fire(int w, int fire) {
    Weapon wd = WEAPONS[w - 1];
    if (P.gunTimer > 0 || P.reloadTimer > 0 || !fire) return;
    float sx = P.x, sy = P.y - 18;
    float ang = atan2f(P.aimy, P.aimx);
    if (w == 1) {  /* knife */
        P.gunTimer = wd.cd;
        for (int e = 0; e < N_ENEMIES; e++) if (!E[e].dead) {
            float dx = E[e].x - P.x; if (fabsf(dx) < 38 && fabsf(enemy_feet(&E[e]) - 30 - P.y) < 50 && dx * P.aimx >= 0) {
                E[e].health -= 12; spawn_part(E[e].x, E[e].y, 190, 0, 0, 8, 2.0f);
                if (E[e].health <= 0) { E[e].dead = 1; E[e].deadTimer = 400; E[e].anim = 0; E[e].deadKind = 0; kills++; P.energy += 20; if (P.energy > 100) P.energy = 100; give_ammo(); }
            }
        }
        return;
    }
    if (P.mag[w - 1] <= 0) { if (sfx_cock >= 0) plat_play_sfx(sfx_cock, 70); P.gunTimer = 12; return; }
    P.mag[w - 1]--; P.gunTimer = wd.cd;
    for (int k = 0; k < wd.pellets; k++) {
        float spread = ((rnd(wd.spread10 * 2 + 1) - wd.spread10) / 10.0f) * PI / 180.0f;
        fire_bullet(sx + cosf(ang) * 28, sy + sinf(ang) * 28, ang + spread, wd.power, 1);
    }
    if (wd.sfx >= 0 && sfx[wd.sfx] >= 0) plat_play_sfx(sfx[wd.sfx], 100);
    spawn_part(sx + cosf(ang) * 30, sy + sinf(ang) * 30, 255, 190, 60, 3, 1.5f);
    pflash = 5;
}

static void update_player(const Input *in, float ts) {
    /* aim: face buttons give 8 directions; otherwise aim along facing direction */
    if (in->aim) {
        P.aimx = in->ax; P.aimy = in->ay;
        if (P.aimx > 0.01f) P.dir = 1; else if (P.aimx < -0.01f) P.dir = -1;
    } else {
        if (in->runx) P.dir = in->runx;
        P.aimx = (float)P.dir; P.aimy = 0;
    }
    P.vx2 = in->runx * 6.0f;
    P.vx += (P.vx2 - P.vx) * 0.2f * ts;

    /* weapon switch / reload */
    static int lastNext = 0, lastPrev = 0, lastRel = 0;
    if (in->nextw && !lastNext) { P.weapon = P.weapon % 7 + 1; P.reloadTimer = 0; }
    if (in->prevw && !lastPrev) { P.weapon = (P.weapon + 5) % 7 + 1; P.reloadTimer = 0; }
    lastNext = in->nextw; lastPrev = in->prevw;
    int wi = P.weapon - 1;
    if ((in->reload && !lastRel) || (P.mag[wi] <= 0 && in->fire && P.total[wi] > 0 && P.weapon > 1)) {
        if (WEAPONS[wi].maxmag && P.mag[wi] < WEAPONS[wi].maxmag && P.total[wi] > 0 && P.reloadTimer <= 0) P.reloadTimer = 40;
    }
    lastRel = in->reload;
    if (P.reloadTimer > 0) {
        P.reloadTimer -= (int)(ts >= 0.5f ? 1 : (rnd(100) < ts * 100 ? 1 : 0));
        if (P.reloadTimer <= 0) {
            int need = WEAPONS[wi].maxmag - P.mag[wi], take = need < P.total[wi] ? need : P.total[wi];
            P.mag[wi] += take; P.total[wi] -= take;
        }
    }
    if (P.gunTimer > 0) P.gunTimer -= 1;
    if (pflash > 0) pflash--;
    player_fire(P.weapon, in->fire);

    /* jump (original: jumpPower 5, decays x0.45 each frame while held) */
    if (in->jump) {
        if (P.onGround && !P.jumpHeld) { P.jumpReady = 1; P.jumpPower = 5.0f; }
        if (P.jumpReady) { P.vy -= P.jumpPower; P.jumpPower *= 0.45f; }
    } else if (!P.onGround) P.jumpReady = 0;
    P.jumpHeld = in->jump;

    /* horizontal move with wall check */
    float dx = P.vx * ts, nx = P.x + dx;
    int sgn = dx > 0 ? 1 : -1;
    if (dx != 0 && (solid(nx + sgn * 14, P.y + 20) || solid(nx + sgn * 14, P.y - 10) || solid(nx + sgn * 14, P.y - 30)))
        P.vx = 0;
    else P.x = nx;

    /* gravity */
    P.vy += 0.4f * ts; P.y += P.vy * ts;
    if (solid(P.x, P.y - 40) && P.vy < 0) { P.vy = 0; P.y += 3; }
    P.onGround = 0;
    if (solid(P.x, P.y + 40)) {
        P.onGround = 1; P.vy = 0;
        for (int i = 1; i < 14 && solid(P.x, P.y + 40 - i + 1); i++) P.y -= 1;
    }
    if (P.y > LV_Y0 + (LV_H / LV_S) - 20) { P.health = 0; lost = 1; }
    P.runFrame += fabsf(P.vx) * 0.045f * ts;
}

static void update_enemies(float ts) {
    for (int i = 0; i < N_ENEMIES; i++) {
        Enemy *e = &E[i];
        float feet = enemy_feet(e);
        if (e->hit > 0) e->hit--;
        e->moving = 0;
        if (e->flash > 0) e->flash--;
        e->anim += e->dead ? ts / 2.0f : (e->moving ? ts / 3.0f : ts / 4.0f);
        if (e->dead) { if (e->deadTimer > 0) e->deadTimer--; }
        /* gravity / ground */
        if (!solid(e->x, feet + 1)) { e->vy += 0.3f * ts; e->y += e->vy * ts; }
        else { e->vy = 0; if (solid(e->x, feet - 1)) e->y -= 1; }
        if (e->dead) continue;
        float dx = P.x - e->x, dy = P.y - feet;
        e->dir = dx >= 0 ? 1 : -1;
        int close = fabsf(dx) < 460 && fabsf(dy) < 160;
        e->engaged = close;
        if (close) {
            if (fabsf(dx) > 230) {
                float nx = e->x + e->dir * 2.0f * ts;
                if (!solid(nx + e->dir * 12, feet - 12) && solid(nx, feet + 4)) { e->x = nx; e->moving = 1; }
            }
            e->gunTimer -= (ts >= 0.5f ? 1 : (rnd(100) < ts * 100 ? 1 : 0));
            if (e->gunTimer <= 0) {
                float ang = atan2f((P.y - 8) - (feet - 36), dx) + ((rnd(120) - 60) / 10.0f) * PI / 180.0f;
                fire_bullet(e->x + e->dir * 26, feet - 38, ang, 7, 0);
                if (sfx[0] >= 0) plat_play_sfx(sfx[0], 70);
                e->gunTimer = 45 + rnd(35); e->aim = 8; e->flash = 5;
            }
        }
        if (e->aim > 0) e->aim--;
    }
}

static void update_particles(float ts) {
    for (int i = 0; i < MAX_PART; i++) if (PT[i].life > 0) {
        PT[i].x += PT[i].vx * ts; PT[i].y += PT[i].vy * ts; PT[i].vy += 0.3f * ts; PT[i].life--;
    }
}

/* ---------- drawing ---------- */
static void draw_spr(int tex, const Spr *s, float wx, float wy, int flip, double angle, int cxo, int cyo, int alpha) {
    int sx = (int)((wx - camx) * LV_S), sy = (int)((wy - camy) * LV_S);
    int dx = flip ? sx - (s->w - s->px) : sx - s->px, dy = sy - s->py;
    int cx = flip ? s->w - cxo : cxo, cy = cyo;
    plat_draw(tex, s->x, s->y, s->w, s->h, dx, dy, s->w, s->h, angle, cx, cy, flip, alpha);
}

static const char *GLYPHS =
 "111101101101111" "010110010010111" "111001111100111" "111001111001111" "101101111001001"
 "111100111001111" "111100111101111" "111001001001001" "111101111101111" "111101111001111"
 "111101111101101" "110101110101110" "111100100100111" "110101101101110" "111100110100111"
 "111100110100100" "111100101101111" "101101111101101" "111010010010111" "001001001101111"
 "101101110101101" "100100100100111" "101111111101101" "110101101101101" "111101101101111"
 "111101111100100" "111101101111001" "111101110101101" "111100111001111" "111010010010010"
 "101101101101111" "101101101101010" "101101111111101" "101101010101101" "101101010010010"
 "111001010100111" "001001010100100" "000010000010000" "010010010000010" "000000111000000" "000000000000010";
static const char *CHARSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/:!-.";

static void text(int x, int y, const char *s, int sc, int r, int g, int b) {
    for (; *s; s++, x += 4 * sc) {
        const char *c = strchr(CHARSET, *s); if (!c || *s == 0) continue;
        const char *gl = GLYPHS + (c - CHARSET) * 15;
        for (int i = 0; i < 15; i++) if (gl[i] == '1') plat_rect(x + (i % 3) * sc, y + (i / 3) * sc, sc, sc, r, g, b, 255);
    }
}

static void render(void) {
    plat_clear(0, 0, 0);
    /* camera */
    float vw = SW / LV_S, vh = SH / LV_S;
    camx = P.x - vw / 2; camy = P.y - vh / 2 - 20;
    float maxx = LV_X0 + LV_W / LV_S - vw, maxy = LV_Y0 + LV_H / LV_S - vh;
    if (camx > maxx) camx = maxx;
    if (camx < LV_X0) camx = LV_X0;
    if (camy > maxy) camy = maxy;
    if (camy < LV_Y0) camy = LV_Y0;
    int offx = (int)((camx - LV_X0) * LV_S), offy = (int)((camy - LV_Y0) * LV_S);
    for (int t = 0; t < LV_TILES; t++) {
        int tx = t * 512 - offx;
        if (tx + 512 < 0 || tx > SW) continue;
        plat_draw(tex_level[t], 0, 0, 512, 512, tx, -offy, 512, 512, 0, 0, 0, 0, 255);
    }
    if (!windowBroken) {
        int wx = (int)((WIN_X0 - camx) * LV_S), wy = (int)((WIN_Y0 - camy) * LV_S);
        plat_rect(wx, wy, (int)((WIN_X1 - WIN_X0) * LV_S), (int)((WIN_Y1 - WIN_Y0) * LV_S), 150, 200, 230, 90);
    }
    /* enemies */
    for (int i = 0; i < N_ENEMIES; i++) {
        Enemy *e = &E[i];
        if (e->dead && e->deadTimer <= 0) continue;
        int flip = e->dir < 0, alpha = 255;
        const Spr *s;
        if (e->dead) {
            int n = e->deadKind ? N_GDEADF : N_GDEADB, k = (int)e->anim; if (k > n - 1) k = n - 1;
            s = e->deadKind ? &GA_GDEADF[k] : &GA_GDEADB[k];
            if (e->deadTimer < 60) alpha = e->deadTimer * 255 / 60;
        } else if (e->aim > 0 || (e->engaged && !e->moving)) s = &GA_GAIM[0];
        else if (e->moving) s = &GA_GWALK[((int)e->anim) % N_GWALK];
        else s = &GA_GIDLE[((int)e->anim) % N_GIDLE];
        draw_spr(tex_gang, s, e->x, e->y, flip, 0, s->px, s->py, alpha);
        if (e->flash > 0 && !e->dead) {
            const Spr *mf = &GA_MUZ[(5 - e->flash) < N_MUZ ? (5 - e->flash) : N_MUZ - 1];
            draw_spr(tex_gang, mf, e->x + e->dir * 26, enemy_feet(e) - 38, flip, 0, flip ? mf->w - 0 : 0, mf->py, 255);
        }
    }
    /* player */
    if (!lost) {
        int flip = P.dir < 0;
        const Spr *lg = (fabsf(P.vx) > 0.8f && P.onGround) ? &SP_LEG[((int)P.runFrame) % N_LEG] : &SP_LEGIDLE;
        if (!P.onGround) lg = &SP_LEG[4];
        draw_spr(tex_atlas, lg, P.x, P.y, flip, 0, lg->px, lg->py, 255);
        const Spr *bd = &SP_BODY[P.weapon - 1];
        double ang = atan2((double)P.aimy, (double)(flip ? -P.aimx : P.aimx)) * 180.0 / PI;
        if (flip) ang = -ang;
        draw_spr(tex_atlas, bd, P.x, P.y, flip, ang, bd->px, bd->py - 14, 255);
        if (pflash > 0) {
            const Spr *mf = &GA_MUZ[(5 - pflash) < N_MUZ ? (5 - pflash) : N_MUZ - 1];
            float a = atan2f(P.aimy, P.aimx);
            draw_spr(tex_gang, mf, P.x + cosf(a) * 30, P.y - 18 + sinf(a) * 30, 0, a * 180.0 / PI, mf->px, mf->py, 255);
        }
    }
    /* bullets */
    for (int i = 0; i < MAX_BULLETS; i++) if (B[i].alive) {
        int sx = (int)((B[i].x - camx) * LV_S), sy = (int)((B[i].y - camy) * LV_S);
        plat_rect(sx - 2, sy - 1, 4, 2, 255, B[i].friendly ? 230 : 120, 60, 255);
    }
    for (int i = 0; i < MAX_PART; i++) if (PT[i].life > 0) {
        int sx = (int)((PT[i].x - camx) * LV_S), sy = (int)((PT[i].y - camy) * LV_S);
        plat_rect(sx, sy, 2, 2, PT[i].r, PT[i].g, PT[i].b, 255);
    }
    /* aim reticle */
    {
        int rx = (int)((P.x - camx) * LV_S + P.aimx * 55), ry = (int)((P.y - 18 - camy) * LV_S + P.aimy * 55);
        plat_rect(rx - 3, ry, 7, 1, 255, 140, 0, 255); plat_rect(rx, ry - 3, 1, 7, 255, 140, 0, 255);
    }
    /* HUD */
    plat_rect(6, 6, 104, 8, 40, 0, 0, 200); plat_rect(8, 8, P.health, 4, 230, 40, 20, 255);
    plat_rect(6, 17, 104, 6, 0, 0, 40, 200); plat_rect(8, 19, (int)P.energy, 2, 60, 150, 255, 255);
    char buf[48];
    snprintf(buf, sizeof buf, "%d/%d", P.mag[P.weapon - 1], P.total[P.weapon - 1]);
    if (P.weapon == 1) snprintf(buf, sizeof buf, "KNIFE");
    text(SW - 70, SH - 16, buf, 2, 255, 160, 20);
    snprintf(buf, sizeof buf, "W%d", P.weapon); text(SW - 70, SH - 30, buf, 2, 255, 160, 20);
    snprintf(buf, sizeof buf, "KILLS %d/%d", kills, N_ENEMIES); text(SW - 110, 6, buf, 2, 255, 160, 20);
    text(6, SH - 12, "ALPHA 1.1", 1, 160, 160, 160);
    if (P.reloadTimer > 0) text(SW / 2 - 28, SH - 40, "RELOAD", 2, 255, 160, 20);
    if (paused) text(SW / 2 - 24, SH / 2 - 10, "PAUSED", 2, 255, 255, 255);
    if (won) text(SW / 2 - 70, SH / 2 - 10, "LEVEL COMPLETE!", 2, 255, 220, 40);
    if (lost) text(SW / 2 - 42, SH / 2 - 10, "YOU DIED - SELECT", 2, 230, 40, 40);
    plat_present();
}

int main(int argc, char **argv) {
    if (plat_init(argc > 0 ? argv[0] : "")) return 1;
    char path[256], name[64];
    plat_path(path, sizeof path, "level1.col");
    FILE *f = fopen(path, "rb"); if (!f) return 2;
    col = malloc((size_t)LV_STRIDE * LV_H);
    if (fread(col, 1, (size_t)LV_STRIDE * LV_H, f) != (size_t)LV_STRIDE * LV_H) return 3;
    fclose(f);
    for (int i = 0; i < LV_TILES; i++) { snprintf(name, sizeof name, "lv1_%d.png", i); tex_level[i] = plat_load_tex(name); }
    tex_atlas = plat_load_tex("atlas.png"); tex_gang = plat_load_tex("gang.png");
    sfx[0] = plat_load_sfx("pistol.wav"); sfx[1] = plat_load_sfx("uzi.wav");
    sfx[2] = plat_load_sfx("shotgun.wav"); sfx[3] = plat_load_sfx("m16.wav");
    sfx_hit = plat_load_sfx("hit.wav"); sfx_rico = plat_load_sfx("ricochet.wav");
    sfx_slow = plat_load_sfx("slowdown.wav"); sfx_cock = plat_load_sfx("cock.wav");
    plat_play_music("music.wav");
    init_game();
    uint32_t last = plat_ticks(); float acc = 0; int slowSnd = 0;
    Input in; memset(&in, 0, sizeof in);
    for (;;) {
        plat_poll(&in);
        if (in.quit) break;
        uint32_t now = plat_ticks(); acc += (now - last) / 1000.0f; last = now;
        if (acc > 0.25f) acc = 0.25f;
        while (acc >= 1.0f / FPS) {
            acc -= 1.0f / FPS;
            { static int lastPause = 0;
              if (in.pause && !lastPause) { if (lost || won) init_game(); else paused = !paused; }
              lastPause = in.pause; }
            if (paused) continue;
            /* slow motion (original: shift key, drains energy) */
            if (in.slow && P.energy > 0 && !lost) {
                slowTarget = 0.3f; P.energy -= 0.6f; if (P.energy < 0) P.energy = 0;
                if (!slowSnd && sfx_slow >= 0) { plat_play_sfx(sfx_slow, 100); slowSnd = 1; }
            } else { slowTarget = 1.0f; slowSnd = 0; }
            timescale += (slowTarget - timescale) * 0.1f;
            if (!lost && !won) {
                update_player(&in, timescale);
                update_enemies(timescale);
                update_bullets(timescale);
                if (kills == N_ENEMIES && P.x > LV_X0 + LV_W / LV_S - 300) won = 1;
            } else { update_enemies(timescale); update_bullets(timescale); }
            update_particles(timescale);
            plat_music_volume((int)(timescale * 100));
            frameCount++;
#ifdef HEADLESS
            if (frameCount % 100 == 0) printf("f%d player(%.0f,%.0f) hp=%d ground=%d weapon=%d mag=%d kills=%d ts=%.2f\n", frameCount, P.x, P.y, P.health, P.onGround, P.weapon, P.mag[P.weapon-1], kills, timescale);
#endif
        }
        render();
    }
    plat_shutdown();
    return 0;
}
