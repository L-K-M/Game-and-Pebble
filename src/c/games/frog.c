#include "../gp.h"

// ============================================================================
//  PEBBLE-FROG  -  hop across the busy road without getting flattened.
//  Reach the top, score, and do it again - a little faster each time.
//
//  Controls:  SELECT = hop forward   UP = hop left   DOWN = hop right
//             SELECT after a squish = retry
// ============================================================================

#define COLS     10
#define MAXROWS  12
#define TICK_MS  33

static Window   *s_window;
static AppTimer *s_timer;

static int   s_rows, s_cell, s_ox, s_oy;
static float s_offset[MAXROWS], s_speed[MAXROWS];   // per-lane scroll + velocity
static int   s_fcol, s_frow;
static int   s_score, s_best, s_lives, s_level;
static bool  s_dead, s_record, s_blink;
static int   s_cargap, s_carw;

static bool is_traffic(int r) { return r > 0 && r < s_rows - 1 && (r % 3 != 0); }

static GColor car_color(int r) {
  switch (r % 4) { case 0: return GColorRed; case 1: return GColorYellow;
                   case 2: return GColorOrange; default: return GColorPictonBlue; }
}

static void setup_lanes(void) {
  float lf = 1.0f + (s_level - 1) * 0.18f;
  for (int r = 0; r < s_rows; r++) {
    if (is_traffic(r)) {
      float mag = 1.0f + (r % 3) * 0.6f;
      float dir = (r % 2) ? 1.0f : -1.0f;
      s_speed[r] = dir * mag * lf;
      if (s_speed[r] > 4.5f) s_speed[r] = 4.5f;
      if (s_speed[r] < -4.5f) s_speed[r] = -4.5f;
      s_offset[r] = rand() % s_cargap;
    } else {
      s_speed[r] = 0; s_offset[r] = 0;
    }
  }
}

static void reset_frog(void) { s_fcol = COLS / 2; s_frow = s_rows - 1; }

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  int hud = 22;
  s_cell = b.size.w / COLS;
  s_rows = (b.size.h - hud) / s_cell;
  if (s_rows > MAXROWS) s_rows = MAXROWS;
  if (s_rows < 6) s_rows = 6;
  s_ox = (b.size.w - s_cell * COLS) / 2;
  s_oy = hud + (b.size.h - hud - s_cell * s_rows) / 2;
  s_cargap = s_cell * 3;
  s_carw = (s_cell * 3) / 2;
  s_score = 0; s_lives = 3; s_level = 1;
  s_dead = false; s_record = false;
  s_best = gp_highscore_get(GAME_FROG);
  setup_lanes();
  reset_frog();
}

static void squish(void) {
  s_lives--;
  vibes_long_pulse();
  if (s_lives <= 0) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_FROG, s_score);
  } else {
    reset_frog();
  }
}

static bool car_at(int r, float grid_x) {
  if (!is_traffic(r)) return false;
  float rel = grid_x - s_offset[r];
  rel = rel - s_cargap * __builtin_floorf(rel / s_cargap);     // positive modulo
  return rel < s_carw;
}

static void check_squish(void) {
  if (s_dead) return;
  float fx = s_fcol * s_cell + s_cell / 2.0f;
  if (car_at(s_frow, fx)) squish();
}

static void step(void) {
  for (int r = 0; r < s_rows; r++) {
    if (s_speed[r] == 0) continue;
    s_offset[r] += s_speed[r];
    s_offset[r] = s_offset[r] - s_cargap * __builtin_floorf(s_offset[r] / s_cargap);
  }
  check_squish();
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  int gw = COLS * s_cell;
  for (int r = 0; r < s_rows; r++) {
    int y = s_oy + r * s_cell;
    bool traffic = is_traffic(r);
    graphics_context_set_fill_color(ctx, traffic ? GColorDarkGray :
                                    (r == 0 ? GColorDarkGreen : GColorJaegerGreen));
    graphics_fill_rect(ctx, GRect(s_ox, y, gw, s_cell), 0, GCornerNone);
    if (traffic) {
      graphics_context_set_fill_color(ctx, car_color(r));
      for (float x = s_offset[r] - s_cargap; x < gw; x += s_cargap) {
        graphics_fill_rect(ctx, GRect(s_ox + (int)x, y + 2, s_carw, s_cell - 4), 2, GCornersAll);
      }
    }
  }
  // goal markers on the top row
  graphics_context_set_fill_color(ctx, GColorBrightGreen);
  for (int c = 1; c < COLS; c += 2)
    graphics_fill_circle(ctx, GPoint(s_ox + c * s_cell + s_cell / 2, s_oy + s_cell / 2), 2);

  // frog
  if (!s_dead) {
    int fx = s_ox + s_fcol * s_cell + s_cell / 2;
    int fy = s_oy + s_frow * s_cell + s_cell / 2;
    int rr = s_cell / 2 - 2;
    graphics_context_set_fill_color(ctx, GColorKellyGreen);
    graphics_fill_circle(ctx, GPoint(fx, fy), rr);
    graphics_context_set_fill_color(ctx, GColorBrightGreen);
    graphics_fill_circle(ctx, GPoint(fx - rr / 2, fy - rr / 2), 2);
    graphics_fill_circle(ctx, GPoint(fx + rr / 2, fy - rr / 2), 2);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(fx - rr / 2, fy - rr / 2), 1);
    graphics_fill_circle(ctx, GPoint(fx + rr / 2, fy - rr / 2), 1);
  }

  char l[16], r[12];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  snprintf(r, sizeof(r), "x%d", s_lives);
  gp_hud(ctx, b, l, r, GColorKellyGreen);

  if (s_dead) gp_overlay_gameover(ctx, b, "GAME OVER", s_score, s_best, s_record, s_blink);
}

static void tick(void *data) {
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

static void hop(int dcol, int drow) {
  if (s_dead) return;
  s_fcol += dcol; s_frow += drow;
  if (s_fcol < 0) s_fcol = 0;
  if (s_fcol > COLS - 1) s_fcol = COLS - 1;
  if (s_frow < 0) s_frow = 0;
  if (s_frow > s_rows - 1) s_frow = s_rows - 1;
  if (s_frow == 0) {                          // reached the far bank!
    s_score += 10 * s_level;
    s_level++;
    vibes_short_pulse();
    setup_lanes();
    reset_frog();
  } else {
    check_squish();
  }
  layer_mark_dirty(window_get_root_layer(s_window));
}
static void up_click(ClickRecognizerRef r, void *c)   { hop(-1, 0); }
static void down_click(ClickRecognizerRef r, void *c) { hop(+1, 0); }
static void select_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(TICK_MS, tick, NULL);
  } else {
    hop(0, -1);
  }
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

static void win_load(Window *window) {
  layer_set_update_proc(window_get_root_layer(window), render);
  reset_game();
  s_timer = app_timer_register(TICK_MS, tick, NULL);
}
static void win_unload(Window *window) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
}

void frog_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
