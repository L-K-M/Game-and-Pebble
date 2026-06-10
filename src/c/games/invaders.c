#include "../gp.h"

// ============================================================================
//  PEBBLE INVADERS  -  hold the line against the descending alien horde.
//
//  Controls:  UP = move left   DOWN = move right   SELECT = fire / retry
// ============================================================================

#define AROWS    4
#define ACOLS    6
#define NBOMB    4
#define NSHIELD  4
#define SH       3
#define SW       5
#define TICK_MS  40

static Window   *s_window;
static AppTimer *s_timer;

static bool s_alien[AROWS][ACOLS];
static int  s_fx, s_fy, s_dir, s_ax, s_ay, s_alien_w;
static int  s_killed, s_frame, s_march_acc;
static int  s_cannon_x, s_cannon_y;
static int  s_bx, s_by; static bool s_bul;          // single player bullet
static struct { int x, y; bool active; } s_bomb[NBOMB];
static bool s_shield[NSHIELD][SH][SW];
static int  s_shield_x[NSHIELD], s_shield_y, s_scell;
static int  s_score, s_best, s_lives, s_wave, s_inv;
static bool s_dead, s_record, s_blink;
static int  s_w, s_h;

static int alive_count(void) {
  int n = 0;
  for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) if (s_alien[r][c]) n++;
  return n;
}
static int march_interval(void) {
  int m = 520 - s_killed * 18 - (s_wave - 1) * 40;
  return m < 110 ? 110 : m;
}

static void build_aliens(void) {
  for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) s_alien[r][c] = true;
  s_fx = 8; s_fy = 28 + (s_wave - 1) * 6; s_dir = 1; s_frame = 0; s_march_acc = 0;
}

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  s_w = b.size.w; s_h = b.size.h;
  s_ax = (s_w - 16) / ACOLS; s_ay = 15; s_alien_w = s_ax - 6;
  s_cannon_y = s_h - 14; s_cannon_x = s_w / 2;
  s_score = 0; s_lives = 3; s_wave = 1; s_killed = 0; s_inv = 0;
  s_dead = false; s_record = false; s_bul = false;
  s_best = gp_highscore_get(GAME_INVADERS);
  for (int i = 0; i < NBOMB; i++) s_bomb[i].active = false;
  build_aliens();
  // shields
  s_scell = 6; s_shield_y = s_cannon_y - 30;
  for (int i = 0; i < NSHIELD; i++) {
    s_shield_x[i] = (s_w * (i + 1)) / (NSHIELD + 1) - (SW * s_scell) / 2;
    for (int r = 0; r < SH; r++) for (int c = 0; c < SW; c++) s_shield[i][r][c] = true;
  }
}

static void next_wave(void) {
  s_wave++; s_killed = 0;
  build_aliens();
  vibes_double_pulse();
}

// erode a shield cell at pixel (x,y); returns true if something was hit
static bool shield_hit(int x, int y) {
  if (y < s_shield_y) return false;     // int division truncates toward zero,
  for (int i = 0; i < NSHIELD; i++) {   // so guard before dividing or shots
    if (x < s_shield_x[i]) continue;    // just outside would erode cell 0
    int lc = (x - s_shield_x[i]) / s_scell;
    int lr = (y - s_shield_y) / s_scell;
    if (lc >= 0 && lc < SW && lr >= 0 && lr < SH && s_shield[i][lr][lc]) {
      s_shield[i][lr][lc] = false;
      return true;
    }
  }
  return false;
}

static void lose_life(void) {
  s_lives--;
  vibes_long_pulse();
  if (s_lives <= 0) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_INVADERS, s_score);
  } else {
    s_cannon_x = s_w / 2; s_inv = 30;
  }
}

static void drop_bomb(void) {
  // pick a random column that still has aliens, drop from its lowest one
  int col = rand() % ACOLS;
  for (int tries = 0; tries < ACOLS; tries++, col = (col + 1) % ACOLS) {
    for (int r = AROWS - 1; r >= 0; r--) {
      if (s_alien[r][col]) {
        for (int i = 0; i < NBOMB; i++) {
          if (!s_bomb[i].active) {
            s_bomb[i].x = s_fx + col * s_ax + s_alien_w / 2;
            s_bomb[i].y = s_fy + r * s_ay + 8;
            s_bomb[i].active = true;
            return;
          }
        }
        return;
      }
    }
  }
}

static void march(void) {
  s_frame ^= 1;
  int minx = s_w, maxx = 0;
  for (int c = 0; c < ACOLS; c++) {
    for (int r = 0; r < AROWS; r++) {
      if (s_alien[r][c]) {
        int x = s_fx + c * s_ax;
        if (x < minx) minx = x;
        if (x + s_alien_w > maxx) maxx = x + s_alien_w;
        break;
      }
    }
  }
  if (minx + s_dir * 6 < 4 || maxx + s_dir * 6 > s_w - 4) {
    s_dir = -s_dir;
    s_fy += 10;
  } else {
    s_fx += s_dir * 6;
  }
  // reached the cannon line?
  if (s_fy + (AROWS - 1) * s_ay + 10 >= s_cannon_y) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_INVADERS, s_score);
    vibes_long_pulse();
  }
  if ((rand() % 100) < 30) drop_bomb();
}

static void step(void) {
  if (s_inv > 0) s_inv--;

  // player bullet
  if (s_bul) {
    s_by -= 7;
    if (s_by < 22) s_bul = false;
    else if (shield_hit(s_bx, s_by)) s_bul = false;
    else {
      for (int r = 0; r < AROWS && s_bul; r++) {
        for (int c = 0; c < ACOLS; c++) {
          if (!s_alien[r][c]) continue;
          int ax = s_fx + c * s_ax, ay = s_fy + r * s_ay;
          if (s_bx >= ax && s_bx <= ax + s_alien_w && s_by >= ay && s_by <= ay + 9) {
            s_alien[r][c] = false; s_bul = false; s_killed++;
            s_score += (AROWS - r) * 10;
            if (alive_count() == 0) next_wave();
            break;
          }
        }
      }
    }
  }

  // bombs
  for (int i = 0; i < NBOMB; i++) {
    if (!s_bomb[i].active) continue;
    s_bomb[i].y += 4;
    if (s_bomb[i].y > s_h) { s_bomb[i].active = false; continue; }
    if (shield_hit(s_bomb[i].x, s_bomb[i].y)) { s_bomb[i].active = false; continue; }
    if (s_inv <= 0 && s_bomb[i].y >= s_cannon_y - 4 &&
        s_bomb[i].x >= s_cannon_x - 10 && s_bomb[i].x <= s_cannon_x + 10) {
      s_bomb[i].active = false; lose_life();
    }
  }

  // marching
  s_march_acc += TICK_MS;
  if (s_march_acc >= march_interval()) { s_march_acc = 0; march(); }
}

static void draw_alien(GContext *ctx, int x, int y) {
  graphics_context_set_fill_color(ctx, GColorBrightGreen);
  graphics_fill_rect(ctx, GRect(x, y, s_alien_w, 8), 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(x + 2, y + 2, 2, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + s_alien_w - 4, y + 2, 2, 2), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorBrightGreen);
  int legdx = s_frame ? 2 : -2;                    // wiggling legs
  graphics_fill_rect(ctx, GRect(x + legdx + 1, y + 8, 2, 3), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + s_alien_w - 3 - legdx, y + 8, 2, 3), 0, GCornerNone);
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  for (int r = 0; r < AROWS; r++)
    for (int c = 0; c < ACOLS; c++)
      if (s_alien[r][c]) draw_alien(ctx, s_fx + c * s_ax, s_fy + r * s_ay);

  // shields
  graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorJaegerGreen, GColorWhite));
  for (int i = 0; i < NSHIELD; i++)
    for (int r = 0; r < SH; r++)
      for (int c = 0; c < SW; c++)
        if (s_shield[i][r][c])
          graphics_fill_rect(ctx, GRect(s_shield_x[i] + c * s_scell, s_shield_y + r * s_scell, s_scell, s_scell), 0, GCornerNone);

  // bombs (red → black on 1-bit displays, i.e. invisible death from above)
  graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorRed, GColorWhite));
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < NBOMB; i++)
    if (s_bomb[i].active) graphics_draw_line(ctx, GPoint(s_bomb[i].x, s_bomb[i].y), GPoint(s_bomb[i].x, s_bomb[i].y + 5));
  graphics_context_set_stroke_width(ctx, 1);

  // player bullet
  if (s_bul) {
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_rect(ctx, GRect(s_bx - 1, s_by, 2, 6), 0, GCornerNone);
  }

  // cannon (blink while respawning)
  if (!s_dead && !(s_inv > 0 && (s_inv / 3) % 2)) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(s_cannon_x - 11, s_cannon_y, 22, 6), 1, GCornersAll);
    graphics_fill_rect(ctx, GRect(s_cannon_x - 2, s_cannon_y - 5, 4, 6), 1, GCornersTop);
  }

  char l[16];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  gp_hud(ctx, b, l, NULL, GColorBrightGreen);
  for (int i = 0; i < s_lives; i++)
    graphics_fill_rect(ctx, GRect(b.size.w - 14 - i * 13, 8, 10, 5), 1, GCornersAll);

  if (s_dead) gp_overlay_gameover(ctx, b, "GAME OVER", s_score, s_best, s_record, s_blink);
}

static void tick(void *data) {
  if (!gp_in_focus()) {     // a notification is covering us; idle, don't step
    s_timer = app_timer_register(200, tick, NULL);
    return;
  }
  if (s_dead) {
    s_blink = !s_blink;
    layer_mark_dirty(window_get_root_layer(s_window));
    s_timer = app_timer_register(400, tick, NULL);
    return;
  }
  step();
  layer_mark_dirty(window_get_root_layer(s_window));
  s_timer = app_timer_register(TICK_MS, tick, NULL);
}

static void move_cannon(int dx) {
  if (s_dead) return;
  s_cannon_x += dx * 7;
  if (s_cannon_x < 12) s_cannon_x = 12;
  if (s_cannon_x > s_w - 12) s_cannon_x = s_w - 12;
}
static void up_click(ClickRecognizerRef r, void *c)   { move_cannon(-1); }
static void down_click(ClickRecognizerRef r, void *c) { move_cannon(+1); }
static void fire_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(TICK_MS, tick, NULL);
  } else if (!s_bul) {
    s_bul = true; s_bx = s_cannon_x; s_by = s_cannon_y - 6;
  }
}
static void click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 50, up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 50, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, fire_click);
}

static void win_load(Window *window) {
  gp_focus_subscribe();
  layer_set_update_proc(window_get_root_layer(window), render);
  reset_game();
  s_timer = app_timer_register(TICK_MS, tick, NULL);
}
static void win_unload(Window *window) {
  gp_focus_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
}

void invaders_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
