/*
 * Q1: LAST STAND - native AstroEMU guest demo.
 * Uses the documented integer C subset, input MMIO, and Port-1 GPU packets.
 */
#define INPUT_BUTTONS 0x10030000
#define INPUT_LEFT_STICK 0x10030004
#define ENEMY_BASE 0x81100000
#define BULLET_BASE 0x81110000
#define ENEMY_COUNT 64
#define BULLET_COUNT 64
#define GPU_CLEAR 0x10000000
#define GPU_FILL_RECT 0x20000000
#define GPU_DRAW_LINE 0x30000000
#define GPU_PRESENT 0x40000000
#define COLOR_BG 0x07111F
#define COLOR_GRID 0x14263A
#define COLOR_PANEL 0x102033
#define COLOR_PANEL_EDGE 0x29445B
#define COLOR_TOWER_DARK 0x536579
#define COLOR_TOWER 0x9AAFC1
#define COLOR_TOWER_LIGHT 0xD5E3EC
#define COLOR_AIM 0x55D6BE
#define COLOR_BULLET 0xFFE27A
#define COLOR_ENEMY 0xE85D75
#define COLOR_ENEMY_DARK 0x9E304D
#define COLOR_HEALTH 0x55D68B
#define COLOR_HEALTH_LOW 0xF05B67
#define COLOR_DAMAGE 0xFFB454
#define COLOR_RATE 0x6EA8FE
#define COLOR_REPAIR 0xB89BFA
#define COLOR_WHITE 0xF2F6FA
#define COLOR_RED 0xFF405C

int g_initialized = 0;
int g_frame = 0;
int g_wave = 1;
int g_spawned = 0;
int g_alive = 0;
int g_spawn_cd = 20;
int g_break_timer = 0;
int g_kills = 0;
int g_score = 0;
int g_tower_hp = 100;
int g_damage = 1;
int g_fire_rate = 12;
int g_fire_cd = 0;
int g_bullet_speed = 6;
int g_aim_vx = 0;
int g_aim_vy = -6;
int g_prev_buttons = 0;
int g_game_over = 0;
int g_flash = 0;

int abs_i(int n) {
    if (n < 0) return 0 - n;
    return n;
}

int wave_size(int wave) {
    if (wave == 1) return 7;
    if (wave == 2) return 9;
    if (wave == 3) return 12;
    if (wave == 4) return 23;
    if (wave == 5) return 30;
    if (wave == 6) return 40;
    if (wave == 7) return 55;
    if (wave == 8) return 75;
    return 75 + (wave - 8) * 18;
}

/* Packed coordinate words keep helper functions within the guest ABI's
 * four-argument call limit. */
void rect(int xy, int wh, int color) {
    gpu_packet(GPU_FILL_RECT, xy, wh, color);
}

void line(int p0, int p1, int color) {
    gpu_packet(GPU_DRAW_LINE, p0, p1, color);
}

void reset_game(void) {
    int i;
    int a;
    for (i = 0; i < ENEMY_COUNT; i++) {
        a = ENEMY_BASE + i * 16;
        poke32(a + 12, 0);
    }
    for (i = 0; i < BULLET_COUNT; i++) {
        a = BULLET_BASE + i * 20;
        poke32(a + 16, 0);
    }
    g_frame = 0;
    g_wave = 1;
    g_spawned = 0;
    g_alive = 0;
    g_spawn_cd = 20;
    g_break_timer = 0;
    g_kills = 0;
    g_score = 0;
    g_tower_hp = 100;
    g_damage = 1;
    g_fire_rate = 12;
    g_fire_cd = 0;
    g_bullet_speed = 6;
    g_aim_vx = 0;
    g_aim_vy = -6;
    g_game_over = 0;
    g_flash = 0;
}

int spawn_enemy(void) {
    int i;
    int slot;
    int a;
    int side;
    int x;
    int y;
    int hp;
    slot = -1;
    for (i = 0; i < ENEMY_COUNT; i++) {
        a = ENEMY_BASE + i * 16;
        if (peek32(a + 12) == 0 && slot == -1) slot = i;
    }
    if (slot == -1) return 0;

    side = g_spawned % 4;
    x = 50 + ((g_spawned * 137 + g_wave * 71) % 878);
    y = 35 + ((g_spawned * 97 + g_wave * 43) % 480);
    if (side == 0) y = 18;
    if (side == 1) x = 958;
    if (side == 2) y = 532;
    if (side == 3) x = 18;

    hp = 1 + g_kills / 100;
    if (hp > 5) hp = 5;
    a = ENEMY_BASE + slot * 16;
    poke32(a, x);
    poke32(a + 4, y);
    poke32(a + 8, hp);
    poke32(a + 12, 1);
    g_spawned++;
    g_alive++;
    return 1;
}

void fire_bullet(void) {
    int i;
    int slot;
    int a;
    slot = -1;
    for (i = 0; i < BULLET_COUNT; i++) {
        a = BULLET_BASE + i * 20;
        if (peek32(a + 16) == 0 && slot == -1) slot = i;
    }
    if (slot == -1) return;
    a = BULLET_BASE + slot * 20;
    poke32(a, 489);
    poke32(a + 4, 275);
    poke32(a + 8, g_aim_vx * g_bullet_speed / 6);
    poke32(a + 12, g_aim_vy * g_bullet_speed / 6);
    poke32(a + 16, 1);
}

void update_bullets(void) {
    int i;
    int j;
    int a;
    int e;
    int x;
    int y;
    int ex;
    int ey;
    int hp;
    for (i = 0; i < BULLET_COUNT; i++) {
        a = BULLET_BASE + i * 20;
        if (peek32(a + 16) != 0) {
            x = peek32(a) + peek32(a + 8);
            y = peek32(a + 4) + peek32(a + 12);
            poke32(a, x);
            poke32(a + 4, y);
            if (x < 0 || x > 978 || y < 0 || y > 550) poke32(a + 16, 0);

            for (j = 0; j < ENEMY_COUNT; j++) {
                e = ENEMY_BASE + j * 16;
                if (peek32(a + 16) != 0 && peek32(e + 12) != 0) {
                    ex = peek32(e);
                    ey = peek32(e + 4);
                    if (abs_i(x - ex) < 11 && abs_i(y - ey) < 11) {
                        hp = peek32(e + 8) - g_damage;
                        poke32(e + 8, hp);
                        poke32(a + 16, 0);
                        if (hp <= 0) {
                            poke32(e + 12, 0);
                            g_alive--;
                            g_kills++;
                            g_score = g_score + 10;
                            g_flash = 3;
                        }
                    }
                }
            }
        }
    }
}

void update_enemies(void) {
    int i;
    int a;
    int x;
    int y;
    int speed;
    speed = 1 + g_wave / 5 + g_kills / 120;
    if (speed > 6) speed = 6;
    for (i = 0; i < ENEMY_COUNT; i++) {
        a = ENEMY_BASE + i * 16;
        if (peek32(a + 12) != 0) {
            x = peek32(a);
            y = peek32(a + 4);
            if (x < 489) x = x + speed;
            if (x > 489) x = x - speed;
            if (y < 275) y = y + speed;
            if (y > 275) y = y - speed;
            poke32(a, x);
            poke32(a + 4, y);
            if (abs_i(x - 489) < 15 && abs_i(y - 275) < 15) {
                poke32(a + 12, 0);
                g_alive--;
                g_tower_hp = g_tower_hp - 8;
                if (g_tower_hp < 0) g_tower_hp = 0;
            }
        }
    }
}

void update_wave(void) {
    if (g_spawned < wave_size(g_wave)) {
        if (g_spawn_cd > 0) g_spawn_cd--;
        if (g_spawn_cd == 0) {
            if (spawn_enemy() != 0) g_spawn_cd = 9;
        }
    } else {
        if (g_alive == 0) {
            if (g_break_timer == 0) g_break_timer = 60;
            else g_break_timer--;
            if (g_break_timer == 0) {
                g_wave++;
                g_spawned = 0;
                g_spawn_cd = 24;
            }
        }
    }
}

void update_upgrades(int buttons) {
    /* X: damage, B: fire rate, Y: repair. Only buy on the press edge. */
    if ((buttons & 4) != 0 && (g_prev_buttons & 4) == 0) {
        if (g_score >= 100) {
            g_score = g_score - 100;
            g_damage++;
            if (g_damage > 8) g_damage = 8;
        }
    }
    if ((buttons & 2) != 0 && (g_prev_buttons & 2) == 0) {
        if (g_score >= 120) {
            g_score = g_score - 120;
            g_fire_rate = g_fire_rate - 2;
            if (g_fire_rate < 3) g_fire_rate = 3;
        }
    }
    if ((buttons & 8) != 0 && (g_prev_buttons & 8) == 0) {
        if (g_score >= 100) {
            g_score = g_score - 100;
            g_tower_hp = g_tower_hp + 25;
            if (g_tower_hp > 100) g_tower_hp = 100;
        }
    }
}

/* Seven-segment pixel digits keep the HUD readable without relying on
 * a text renderer or font asset. */
void draw_digit(int x, int y, int d, int color) {
    int m;
    m = 0;
    if (d == 0) m = 119;
    if (d == 1) m = 6;
    if (d == 2) m = 91;
    if (d == 3) m = 79;
    if (d == 4) m = 46;
    if (d == 5) m = 109;
    if (d == 6) m = 125;
    if (d == 7) m = 7;
    if (d == 8) m = 127;
    if (d == 9) m = 111;
    if ((m & 1) != 0) rect(((x + 3) << 16) | y, (10 << 16) | 3, color);
    if ((m & 2) != 0) rect((x << 16) | (y + 3), (3 << 16) | 9, color);
    if ((m & 4) != 0) rect(((x + 13) << 16) | (y + 3), (3 << 16) | 9, color);
    if ((m & 8) != 0) rect(((x + 3) << 16) | (y + 13), (10 << 16) | 3, color);
    if ((m & 16) != 0) rect((x << 16) | (y + 16), (3 << 16) | 9, color);
    if ((m & 32) != 0) rect(((x + 13) << 16) | (y + 16), (3 << 16) | 9, color);
    if ((m & 64) != 0) rect(((x + 3) << 16) | (y + 25), (10 << 16) | 3, color);
}

void render_game(void) {
    int i;
    int a;
    int x;
    int y;
    int hpw;
    int wavew;
    int aimx;
    int aimy;
    int color;

    gpu_begin();
    gpu_packet(GPU_CLEAR, COLOR_BG, 0, 0);

    /* Subtle pixel-grid arena. */
    for (i = 0; i < 978; i = i + 48) {
        line(i << 16, (i << 16) | 549, COLOR_GRID);
    }
    for (i = 0; i < 550; i = i + 44) {
        line(i, (977 << 16) | i, COLOR_GRID);
    }

    /* Health, wave progress, and score are represented as compact bars. */
    rect((18 << 16) | 16, (220 << 16) | 10, COLOR_PANEL_EDGE);
    hpw = g_tower_hp * 2;
    color = COLOR_HEALTH;
    if (g_tower_hp < 35) color = COLOR_HEALTH_LOW;
    if (hpw > 0) rect((20 << 16) | 18, (hpw << 16) | 6, color);

    rect((740 << 16) | 16, (220 << 16) | 10, COLOR_PANEL_EDGE);
    wavew = g_spawned * 220 / wave_size(g_wave);
    if (wavew > 220) wavew = 220;
    if (wavew > 0) rect((740 << 16) | 18, (wavew << 16) | 6, COLOR_RATE);

    rect((18 << 16) | 34, (220 << 16) | 5, COLOR_PANEL);
    if (g_score > 0) {
        hpw = g_score / 5;
        if (hpw > 220) hpw = 220;
        rect((18 << 16) | 34, (hpw << 16) | 5, COLOR_DAMAGE);
    }

    /* Numeric HUD: score (three digits), wave, and tower HP. */
    draw_digit(20, 48, (g_score / 100) % 10, COLOR_WHITE);
    draw_digit(40, 48, (g_score / 10) % 10, COLOR_WHITE);
    draw_digit(60, 48, g_score % 10, COLOR_WHITE);
    draw_digit(744, 48, (g_wave / 10) % 10, COLOR_WHITE);
    draw_digit(764, 48, g_wave % 10, COLOR_WHITE);
    draw_digit(844, 48, (g_tower_hp / 100) % 10, COLOR_HEALTH);
    draw_digit(864, 48, (g_tower_hp / 10) % 10, COLOR_HEALTH);
    draw_digit(884, 48, g_tower_hp % 10, COLOR_HEALTH);

    /* Red pixel enemies with darker outlines. */
    for (i = 0; i < ENEMY_COUNT; i++) {
        a = ENEMY_BASE + i * 16;
        if (peek32(a + 12) != 0) {
            x = peek32(a);
            y = peek32(a + 4);
            rect(((x - 7) << 16) | (y - 7), (14 << 16) | 14, COLOR_ENEMY_DARK);
            rect(((x - 4) << 16) | (y - 4), (8 << 16) | 8, COLOR_ENEMY);
            rect(((x - 2) << 16) | (y - 2), (2 << 16) | 2, COLOR_WHITE);
        }
    }

    /* Player projectiles. */
    for (i = 0; i < BULLET_COUNT; i++) {
        a = BULLET_BASE + i * 20;
        if (peek32(a + 16) != 0) {
            x = peek32(a);
            y = peek32(a + 4);
            rect(((x - 2) << 16) | (y - 2), (4 << 16) | 4, COLOR_BULLET);
        }
    }

    /* Fixed central tower and its current aim direction. */
    rect((465 << 16) | 251, (48 << 16) | 48, COLOR_TOWER_DARK);
    rect((471 << 16) | 257, (36 << 16) | 36, COLOR_TOWER);
    rect((480 << 16) | 266, (18 << 16) | 18, COLOR_TOWER_LIGHT);
    aimx = 489 + g_aim_vx * 18;
    aimy = 275 + g_aim_vy * 18;
    line((489 << 16) | 275, (aimx << 16) | aimy, COLOR_AIM);
    rect((484 << 16) | 270, (10 << 16) | 10, COLOR_TOWER_DARK);

    /* Upgrade pips: damage, fire rate, and repair. */
    rect((900 << 16) | 500, (12 << 16) | 12, COLOR_DAMAGE);
    rect((918 << 16) | 500, (12 << 16) | 12, COLOR_RATE);
    rect((936 << 16) | 500, (12 << 16) | 12, COLOR_REPAIR);

    if (g_flash > 0) {
        rect((475 << 16) | 226, (28 << 16) | 4, COLOR_WHITE);
        g_flash--;
    }
    if (g_game_over != 0) {
        rect((260 << 16) | 235, (458 << 16) | 80, COLOR_RED);
        rect((270 << 16) | 245, (438 << 16) | 60, COLOR_BG);
        rect((420 << 16) | 270, (138 << 16) | 10, COLOR_RED);
    }

    gpu_packet(GPU_PRESENT, 0, 0, 0);
    gpu_end();
}

int main(void) {
    int buttons;
    int stick;
    int sx;
    int sy;
    if (g_initialized == 0) {
        reset_game();
        g_initialized = 1;
    }

    buttons = peek32(INPUT_BUTTONS);
    stick = peek32(INPUT_LEFT_STICK);
    sx = (stick >> 16) & 65535;
    sy = stick & 65535;
    if (sx > 32767) sx = sx - 65536;
    if (sy > 32767) sy = sy - 65536;

    if (abs_i(sx) > 5000 || abs_i(sy) > 5000) {
        g_aim_vx = sx / 4096;
        g_aim_vy = sy / 4096;
        if (g_aim_vx == 0 && sx > 0) g_aim_vx = 1;
        if (g_aim_vx == 0 && sx < 0) g_aim_vx = -1;
        if (g_aim_vy == 0 && sy > 0) g_aim_vy = 1;
        if (g_aim_vy == 0 && sy < 0) g_aim_vy = -1;
    }

    if (g_game_over != 0) {
        if ((buttons & 4096) != 0 && (g_prev_buttons & 4096) == 0) reset_game();
    } else {
        update_upgrades(buttons);
        if (g_fire_cd > 0) g_fire_cd--;
        if ((buttons & 1) != 0 && g_fire_cd == 0) {
            fire_bullet();
            g_fire_cd = g_fire_rate;
        }
        update_wave();
        update_bullets();
        update_enemies();
        if (g_tower_hp <= 0) g_game_over = 1;
    }

    render_game();
    g_prev_buttons = buttons;
    g_frame++;
    halt();
    return 0;
}
