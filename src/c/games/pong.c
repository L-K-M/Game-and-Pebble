#include "../gp.h"

// ============================================================================
//  PONG  -  rally against the CPU. You're the bottom paddle; miss three and
//  it's game over. Score a point every time the CPU whiffs.
//
//  Controls:  UP = move left   DOWN = move right   SELECT = serve / retry
// ============================================================================

#define TICK_MS  33

static Window   *s_window;
static AppTimer *s_timer;

static float s_bx, s_by, s_vx, s_vy;     // ball
static float s_px, s_aix;                // paddle centres (player / ai)
static int   s_py, s_aiy, s_pw;          // paddle rows + width
static int   s_score, s_best, s_lives, s_serve_delay;
static bool  s_serving, s_dead, s_record, s_blink;
static int   s_w, s_h;

static float base_speed(void) { return 3.0f + s_score * 0.12f; }

static void serve(int dir) {
  s_bx = s_w / 2.0f; s_by = s_h / 2.0f;
  float sp = base_speed();
  s_vx = ((rand() % 2) ? 1 : -1) * sp * 0.45f;
  s_vy = dir * sp;
  s_serving = true;
  s_serve_delay = 22;                       // auto-launches after a short beat
}

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  s_w = b.size.w; s_h = b.size.h;
  s_pw = s_w / 4;
  s_py = s_h - 16; s_aiy = 30;
  s_px = s_w / 2.0f; s_aix = s_w / 2.0f;
  s_score = 0; s_lives = 3; s_dead = false; s_record = false;
  s_best = gp_highscore_get(GAME_PONG);
  serve(1);                                 // first serve heads toward player
}

static void point_player(void) {            // CPU missed
  s_score++;
  vibes_short_pulse();
  serve(1);
}
static void point_cpu(void) {                // player missed
  s_lives--;
  if (s_lives <= 0) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_PONG, s_score);
    vibes_double_pulse();
  } else {
    vibes_long_pulse();
    serve(1);
  }
}

static void bounce_paddle(float paddle_x, int into_dir) {
  // reflect vertically and add english based on where it hit the paddle
  float hit = (s_bx - paddle_x) / (s_pw / 2.0f);
  if (hit < -1) hit = -1;
  if (hit > 1) hit = 1;
  float sp = base_speed();
  s_vx = hit * sp;
  s_vy = into_dir * sp;
}

static void step(void) {
  if (s_serving) {
    if (s_serve_delay > 0) { s_serve_delay--; return; }
    s_serving = false;
  }
  s_bx += s_vx; s_by += s_vy;

  if (s_bx - 4 < 0)        { s_bx = 4;        s_vx = -s_vx; }
  if (s_bx + 4 > s_w)      { s_bx = s_w - 4;  s_vx = -s_vx; }

  // player paddle (bottom)
  if (s_vy > 0 && s_by + 4 >= s_py && s_by + 4 <= s_py + 8 &&
      s_bx >= s_px - s_pw / 2.0f && s_bx <= s_px + s_pw / 2.0f) {
    s_by = s_py - 4; bounce_paddle(s_px, -1);
  }
  // cpu paddle (top)
  if (s_vy < 0 && s_by - 4 <= s_aiy + 5 && s_by - 4 >= s_aiy - 3 &&
      s_bx >= s_aix - s_pw / 2.0f && s_bx <= s_aix + s_pw / 2.0f) {
    s_by = s_aiy + 5 + 4; bounce_paddle(s_aix, 1);
  }

  // CPU tracks the ball with a capped speed (imperfect on purpose)
  float target = s_bx;
  float diff = target - s_aix;
  float aispd = 2.2f + s_score * 0.06f;
  if (aispd > 5.0f) aispd = 5.0f;
  if (diff > aispd) diff = aispd;
  if (diff < -aispd) diff = -aispd;
  s_aix += diff;
  if (s_aix < s_pw / 2.0f) s_aix = s_pw / 2.0f;
  if (s_aix > s_w - s_pw / 2.0f) s_aix = s_w - s_pw / 2.0f;

  if (s_by > s_h)  point_cpu();              // past the player
  if (s_by < 0)    point_player();           // past the cpu
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // centre net
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  for (int x = 4; x < b.size.w; x += 12)
    graphics_fill_rect(ctx, GRect(x, b.size.h / 2 - 1, 6, 2), 0, GCornerNone);

  // paddles
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect((int)(s_px - s_pw / 2.0f), s_py, s_pw, 5), 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  graphics_fill_rect(ctx, GRect((int)(s_aix - s_pw / 2.0f), s_aiy, s_pw, 5), 2, GCornersAll);

  // ball
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect((int)s_bx - 3, (int)s_by - 3, 6, 6), 1, GCornersAll);

  char l[16], r[12];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  snprintf(r, sizeof(r), "x%d", s_lives);
  gp_hud(ctx, b, l, r, GColorPictonBlue);

  if (s_serving && !s_dead) {
    graphics_context_set_text_color(ctx, GColorWhite);
    gp_text(ctx, "SELECT to serve", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(0, b.size.h / 2 + 8, b.size.w, 24), GTextAlignmentCenter);
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

static void move(int dx) {
  if (s_dead) return;
  s_px += dx * 8;
  if (s_px < s_pw / 2.0f) s_px = s_pw / 2.0f;
  if (s_px > s_w - s_pw / 2.0f) s_px = s_w - s_pw / 2.0f;
}
static void up_click(ClickRecognizerRef r, void *c)   { move(-1); }
static void down_click(ClickRecognizerRef r, void *c) { move(+1); }
static void select_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(TICK_MS, tick, NULL);
  } else if (s_serving) {
    s_serving = false;
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

void pong_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
