#include "../gp.h"

// ============================================================================
//  BREAKOUT  -  bounce the ball, clear every brick, don't drop it.
//
//  Controls:  UP = paddle left   DOWN = paddle right   SELECT = launch / retry
// ============================================================================

#define COLS      7
#define ROWS      5
#define TICK_MS   33

static Window   *s_window;
static AppTimer *s_timer;

static bool  s_brick[ROWS][COLS];
static int   s_left;                 // bricks remaining
static int   s_score, s_best, s_lives, s_level;
static bool  s_launched, s_dead, s_record, s_blink;

// geometry (computed from the screen at reset)
static int   s_fx, s_fy, s_fw, s_fh; // playfield
static int   s_bw, s_bh, s_btop;     // brick metrics
static int   s_paddle_w, s_paddle_y;

// ball state in floats for smooth angles
static float s_bx, s_by, s_vx, s_vy;
static float s_paddle_x;             // paddle centre x
static const float BALL_R = 3.0f;

static GColor row_color(int row) {
  // explicit b/w fallbacks: red/orange luminance-map to black (invisible)
  switch (row) {
    case 0: return COLOR_FALLBACK(GColorRed, GColorWhite);
    case 1: return COLOR_FALLBACK(GColorOrange, GColorWhite);
    case 2: return COLOR_FALLBACK(GColorYellow, GColorWhite);
    case 3: return COLOR_FALLBACK(GColorGreen, GColorWhite);
    default: return COLOR_FALLBACK(GColorCyan, GColorWhite);
  }
}

static float ball_speed(void) { return 2.4f + 0.25f * s_level; }

static void launch_ball(void) {
  s_bx = s_paddle_x;
  s_by = s_paddle_y - BALL_R - 1;
  float sp = ball_speed();
  s_vx = (rand() % 2 ? 1 : -1) * sp * 0.5f;
  s_vy = -sp;
  s_launched = false;             // resting on paddle until SELECT
}

static void build_bricks(void) {
  s_left = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) { s_brick[r][c] = true; s_left++; }
}

static void reset_geometry(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  int hud = 22;
  s_fx = 0; s_fy = hud; s_fw = b.size.w; s_fh = b.size.h - hud;
  s_bw = s_fw / COLS;
  s_bh = 11;
  s_btop = s_fy + 8;
  s_paddle_w = s_fw / 5;
  s_paddle_y = s_fy + s_fh - 12;
  s_paddle_x = s_fx + s_fw / 2.0f;
}

static void reset_game(void) {
  reset_geometry();
  s_score = 0; s_lives = 3; s_level = 1;
  s_dead = false; s_record = false;
  s_best = gp_highscore_get(GAME_BREAKOUT);
  build_bricks();
  launch_ball();
}

static void lose_life(void) {
  s_lives--;
  if (s_lives <= 0) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_BREAKOUT, s_score);
    vibes_double_pulse();
  } else {
    vibes_short_pulse();
    launch_ball();
  }
}

static void step(void) {
  if (!s_launched) {                 // glued to paddle
    s_bx = s_paddle_x;
    s_by = s_paddle_y - BALL_R - 1;
    return;
  }

  s_bx += s_vx;
  s_by += s_vy;

  // walls
  if (s_bx - BALL_R < s_fx)            { s_bx = s_fx + BALL_R;           s_vx = -s_vx; }
  if (s_bx + BALL_R > s_fx + s_fw)     { s_bx = s_fx + s_fw - BALL_R;    s_vx = -s_vx; }
  if (s_by - BALL_R < s_fy)            { s_by = s_fy + BALL_R;           s_vy = -s_vy; }

  // bricks (guard the casts: C truncates toward zero, so a ball up to one
  // brick above s_btop would otherwise alias to row 0 and hit through air)
  int col = (s_bx >= s_fx) ? (int)((s_bx - s_fx) / s_bw) : -1;
  int row = (s_by >= s_btop) ? (int)((s_by - s_btop) / s_bh) : -1;
  if (row >= 0 && row < ROWS && col >= 0 && col < COLS && s_brick[row][col]) {
    s_brick[row][col] = false;
    s_left--;
    s_score += (ROWS - row) * 10;
    s_vy = -s_vy;
    if (s_left <= 0) {               // cleared -> next level, faster
      s_level++;
      build_bricks();
      launch_ball();
      return;
    }
  }

  // paddle
  if (s_vy > 0 && s_by + BALL_R >= s_paddle_y && s_by + BALL_R <= s_paddle_y + 8 &&
      s_bx >= s_paddle_x - s_paddle_w / 2.0f - BALL_R &&
      s_bx <= s_paddle_x + s_paddle_w / 2.0f + BALL_R) {
    s_by = s_paddle_y - BALL_R;
    float hit = (s_bx - s_paddle_x) / (s_paddle_w / 2.0f);   // -1..1
    if (hit < -1) hit = -1;
    if (hit > 1) hit = 1;
    float sp = ball_speed();
    float ah = (hit < 0) ? -hit : hit;
    s_vx = hit * sp;
    s_vy = -(sp - 0.3f * ah * sp);                           // shallower near edges
    if (s_vy > -1.0f) s_vy = -1.0f;                          // always heading up
  }

  // dropped
  if (s_by - BALL_R > s_fy + s_fh) lose_life();
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  char l[16], r[20];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  snprintf(r, sizeof(r), "BALLS %d", s_lives);
  gp_hud(ctx, b, l, r, COLOR_FALLBACK(GColorOrange, GColorWhite));

  // bricks
  for (int row = 0; row < ROWS; row++) {
    for (int c = 0; c < COLS; c++) {
      if (!s_brick[row][c]) continue;
      GRect br = GRect(s_fx + c * s_bw + 1, s_btop + row * s_bh + 1, s_bw - 2, s_bh - 2);
      graphics_context_set_fill_color(ctx, row_color(row));
      graphics_fill_rect(ctx, br, 1, GCornersAll);
    }
  }

  // paddle
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect((int)(s_paddle_x - s_paddle_w / 2.0f), s_paddle_y, s_paddle_w, 5),
                     2, GCornersAll);

  // ball
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint((int)s_bx, (int)s_by), (int)BALL_R);

  if (!s_launched && !s_dead) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    gp_text(ctx, "SELECT to launch", fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(0, s_paddle_y - 26, b.size.w, 18), GTextAlignmentCenter);
  }
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

static void move_paddle(int dir) {
  if (s_dead) return;
  s_paddle_x += dir * 7;
  float half = s_paddle_w / 2.0f;
  if (s_paddle_x < s_fx + half)         s_paddle_x = s_fx + half;
  if (s_paddle_x > s_fx + s_fw - half)  s_paddle_x = s_fx + s_fw - half;
}
static void up_click(ClickRecognizerRef r, void *c)   { move_paddle(-1); }
static void down_click(ClickRecognizerRef r, void *c) { move_paddle(+1); }
static void select_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(TICK_MS, tick, NULL);
  } else if (!s_launched) {
    s_launched = true;
  }
}
static void click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 50, up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 50, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
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

void breakout_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
