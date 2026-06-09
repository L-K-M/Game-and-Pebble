#include "../gp.h"

// ============================================================================
//  ASTEROIDS  -  drift through a vector-graphics asteroid field and blast it
//  to gravel. Everything wraps around the screen edges.
//
//  Controls:  UP = turn left   DOWN = turn right
//             SELECT tap = fire     SELECT hold = thrust     (BACK = menu)
// ============================================================================

#define MAX_AST   40
#define MAX_BUL   8
#define TICK_MS   33

typedef struct { float x, y, vx, vy; int size; int angle, spin; int8_t shape[8]; bool active; } Asteroid;
typedef struct { float x, y, vx, vy; int life; bool active; } Bullet;

static Window   *s_window;
static AppTimer *s_timer;

static Asteroid s_ast[MAX_AST];
static Bullet   s_bul[MAX_BUL];
static float    s_sx, s_sy, s_svx, s_svy;   // ship
static int      s_sangle, s_inv, s_lives, s_wave;
static bool     s_thrust, s_dead, s_record, s_blink;
static int      s_score, s_best;
static int      s_w, s_h;

static const float ACCEL = 0.16f, FRICTION = 0.99f, MAXV = 4.2f, BSPEED = 5.2f;

static int ast_radius(int size) { return size == 2 ? 18 : size == 1 ? 11 : 6; }

static GPoint polar(float cx, float cy, int angle, float r) {
  return GPoint((int)(cx + r * sin_lookup(angle) / TRIG_MAX_RATIO),
                (int)(cy - r * cos_lookup(angle) / TRIG_MAX_RATIO));
}
static void wrap(float *x, float *y) {
  if (*x < 0) *x += s_w; else if (*x >= s_w) *x -= s_w;
  if (*y < 0) *y += s_h; else if (*y >= s_h) *y -= s_h;
}

static void spawn_ast(float x, float y, int size) {
  for (int i = 0; i < MAX_AST; i++) {
    if (s_ast[i].active) continue;
    Asteroid *a = &s_ast[i];
    a->active = true; a->x = x; a->y = y; a->size = size;
    float sp = 0.5f + (rand() % 100) / 100.0f + (2 - size) * 0.4f;
    int dir = rand() % TRIG_MAX_ANGLE;
    a->vx = sp * sin_lookup(dir) / TRIG_MAX_RATIO;
    a->vy = -sp * cos_lookup(dir) / TRIG_MAX_RATIO;
    a->angle = rand() % TRIG_MAX_ANGLE;
    a->spin = (rand() % 800) - 400;
    int base = ast_radius(size);
    for (int k = 0; k < 8; k++) a->shape[k] = (rand() % (base / 2 + 1)) - base / 4;
    return;
  }
}

static void start_wave(void) {
  int n = 3 + s_wave;
  if (n > 7) n = 7;
  for (int i = 0; i < n; i++) {
    // spawn around the edges, away from the centre (where the ship is)
    float x = (rand() % 2) ? (rand() % 30) : (s_w - (rand() % 30));
    float y = rand() % s_h;
    spawn_ast(x, y, 2);
  }
  s_wave++;
}

static void respawn_ship(void) {
  s_sx = s_w / 2.0f; s_sy = s_h / 2.0f; s_svx = s_svy = 0; s_sangle = 0; s_inv = 60;
}

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  s_w = b.size.w; s_h = b.size.h;
  for (int i = 0; i < MAX_AST; i++) s_ast[i].active = false;
  for (int i = 0; i < MAX_BUL; i++) s_bul[i].active = false;
  s_score = 0; s_lives = 3; s_wave = 0;
  s_dead = false; s_record = false; s_thrust = false;
  s_best = gp_highscore_get(GAME_ASTEROIDS);
  respawn_ship();
  start_wave();
}

static void fire(void) {
  for (int i = 0; i < MAX_BUL; i++) {
    if (s_bul[i].active) continue;
    GPoint nose = polar(s_sx, s_sy, s_sangle, 9);
    s_bul[i].active = true;
    s_bul[i].x = nose.x; s_bul[i].y = nose.y;
    s_bul[i].vx = s_svx + BSPEED * sin_lookup(s_sangle) / TRIG_MAX_RATIO;
    s_bul[i].vy = s_svy - BSPEED * cos_lookup(s_sangle) / TRIG_MAX_RATIO;
    s_bul[i].life = 38;
    return;
  }
}

static void hit_ast(int i) {
  Asteroid *a = &s_ast[i];
  s_score += a->size == 2 ? 20 : a->size == 1 ? 50 : 100;
  if (a->size > 0) {
    float x = a->x, y = a->y; int ns = a->size - 1;
    a->active = false;
    spawn_ast(x, y, ns);
    spawn_ast(x, y, ns);
  } else {
    a->active = false;
  }
  vibes_short_pulse();
}

static void lose_life(void) {
  s_lives--;
  vibes_long_pulse();
  if (s_lives <= 0) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_ASTEROIDS, s_score);
  } else {
    respawn_ship();
  }
}

static void step(void) {
  // ship physics
  if (s_thrust) {
    s_svx += ACCEL * sin_lookup(s_sangle) / TRIG_MAX_RATIO;
    s_svy -= ACCEL * cos_lookup(s_sangle) / TRIG_MAX_RATIO;
    float v = s_svx * s_svx + s_svy * s_svy;
    if (v > MAXV * MAXV) { float s = MAXV / __builtin_sqrtf(v); s_svx *= s; s_svy *= s; }
  }
  s_svx *= FRICTION; s_svy *= FRICTION;
  s_sx += s_svx; s_sy += s_svy; wrap(&s_sx, &s_sy);
  if (s_inv > 0) s_inv--;

  // bullets
  for (int i = 0; i < MAX_BUL; i++) {
    if (!s_bul[i].active) continue;
    s_bul[i].x += s_bul[i].vx; s_bul[i].y += s_bul[i].vy; wrap(&s_bul[i].x, &s_bul[i].y);
    if (--s_bul[i].life <= 0) s_bul[i].active = false;
  }

  // asteroids
  int alive = 0;
  for (int i = 0; i < MAX_AST; i++) {
    if (!s_ast[i].active) continue;
    alive++;
    s_ast[i].x += s_ast[i].vx; s_ast[i].y += s_ast[i].vy; wrap(&s_ast[i].x, &s_ast[i].y);
    s_ast[i].angle = (s_ast[i].angle + s_ast[i].spin) & 0xffff;
  }

  // bullet / asteroid collisions
  for (int i = 0; i < MAX_AST; i++) {
    if (!s_ast[i].active) continue;
    int r = ast_radius(s_ast[i].size);
    for (int j = 0; j < MAX_BUL; j++) {
      if (!s_bul[j].active) continue;
      float dx = s_bul[j].x - s_ast[i].x, dy = s_bul[j].y - s_ast[i].y;
      if (dx * dx + dy * dy < (float)(r * r)) { s_bul[j].active = false; hit_ast(i); break; }
    }
  }

  // ship / asteroid collisions
  if (s_inv <= 0) {
    for (int i = 0; i < MAX_AST; i++) {
      if (!s_ast[i].active) continue;
      int r = ast_radius(s_ast[i].size) + 6;
      float dx = s_sx - s_ast[i].x, dy = s_sy - s_ast[i].y;
      if (dx * dx + dy * dy < (float)(r * r)) { lose_life(); break; }
    }
  }

  if (alive == 0) start_wave();
}

static void draw_poly_wrapped(GContext *ctx, Asteroid *a) {
  // draw the asteroid, plus copies across edges so wrapping looks seamless
  int r = ast_radius(a->size);
  for (int oy = -1; oy <= 1; oy++) {
    for (int ox = -1; ox <= 1; ox++) {
      float cx = a->x + ox * s_w, cy = a->y + oy * s_h;
      if (cx < -r - 2 || cx > s_w + r + 2 || cy < -r - 2 || cy > s_h + r + 2) continue;
      GPoint prev = polar(cx, cy, a->angle, r + a->shape[0]);
      GPoint first = prev;
      for (int k = 1; k < 8; k++) {
        GPoint p = polar(cx, cy, a->angle + k * (TRIG_MAX_ANGLE / 8), r + a->shape[k]);
        graphics_draw_line(ctx, prev, p);
        prev = p;
      }
      graphics_draw_line(ctx, prev, first);
    }
  }
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // asteroids
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < MAX_AST; i++) if (s_ast[i].active) draw_poly_wrapped(ctx, &s_ast[i]);

  // bullets
  graphics_context_set_fill_color(ctx, GColorYellow);
  for (int i = 0; i < MAX_BUL; i++)
    if (s_bul[i].active) graphics_fill_circle(ctx, GPoint((int)s_bul[i].x, (int)s_bul[i].y), 2);

  // ship (blink while invulnerable)
  if (!s_dead && !(s_inv > 0 && (s_inv / 4) % 2)) {
    GPoint nose = polar(s_sx, s_sy, s_sangle, 9);
    GPoint l = polar(s_sx, s_sy, s_sangle + DEG_TO_TRIGANGLE(140), 8);
    GPoint r = polar(s_sx, s_sy, s_sangle - DEG_TO_TRIGANGLE(140), 8);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, nose, l);
    graphics_draw_line(ctx, nose, r);
    graphics_draw_line(ctx, l, r);
    if (s_thrust && (s_inv / 2) % 2 == 0) {        // flickering exhaust flame
      GPoint tail = polar(s_sx, s_sy, s_sangle + DEG_TO_TRIGANGLE(180), 11);
      graphics_context_set_stroke_color(ctx, GColorOrange);
      graphics_draw_line(ctx, GPoint((l.x + r.x) / 2, (l.y + r.y) / 2), tail);
    }
  }
  graphics_context_set_stroke_width(ctx, 1);

  // HUD
  char l[16];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  gp_hud(ctx, b, l, NULL, GColorCyan);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < s_lives; i++) {
    int x = b.size.w - 12 - i * 12, y = 11;
    graphics_draw_line(ctx, GPoint(x, y - 5), GPoint(x - 4, y + 4));
    graphics_draw_line(ctx, GPoint(x, y - 5), GPoint(x + 4, y + 4));
    graphics_draw_line(ctx, GPoint(x - 4, y + 4), GPoint(x + 4, y + 4));
  }
  graphics_context_set_stroke_width(ctx, 1);

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

static void rot_left(ClickRecognizerRef r, void *c)  { if (!s_dead) s_sangle = (s_sangle - DEG_TO_TRIGANGLE(11)) & 0xffff; }
static void rot_right(ClickRecognizerRef r, void *c) { if (!s_dead) s_sangle = (s_sangle + DEG_TO_TRIGANGLE(11)) & 0xffff; }
static void fire_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(TICK_MS, tick, NULL);
  } else {
    fire();
  }
}
static void thrust_on(ClickRecognizerRef r, void *c)  { if (!s_dead) s_thrust = true; }
static void thrust_off(ClickRecognizerRef r, void *c) { s_thrust = false; }

static void click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 45, rot_left);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 45, rot_right);
  window_single_click_subscribe(BUTTON_ID_SELECT, fire_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 180, thrust_on, thrust_off);
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

void asteroids_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
