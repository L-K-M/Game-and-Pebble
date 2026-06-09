#include "../gp.h"

// ============================================================================
//  FLAPPY  -  tap to flap, thread the gaps, don't touch a thing.
//
//  Controls:  SELECT (or UP) = flap     SELECT after a crash = retry
// ============================================================================

#define NPIPE     3
#define PW        24       // pipe width
#define GAP       62       // gap height
#define SPACING   96       // horizontal spacing between pipes
#define TICK_MS   33

#define SKY    GColorVividCerulean
#define PIPE   GColorIslamicGreen
#define GROUND GColorWindsorTan

static Window   *s_window;
static AppTimer *s_timer;

static float s_by, s_vy;                 // bird y and velocity
static int   s_bird_x, s_ground_y, s_top;
static struct { float x; int gap_y; bool scored; } s_pipe[NPIPE];
static int   s_score, s_best;
static bool  s_started, s_dead, s_record, s_blink;
static int   s_w, s_h;

static int rand_gap(void) {
  int lo = s_top + GAP / 2 + 6;
  int hi = s_ground_y - GAP / 2 - 6;
  return lo + rand() % (hi - lo + 1);
}

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  s_w = b.size.w; s_h = b.size.h;
  s_top = 0; s_ground_y = s_h - 12;
  s_bird_x = s_w / 3;
  s_by = s_h / 2.0f; s_vy = 0;
  s_score = 0; s_started = false; s_dead = false; s_record = false;
  s_best = gp_highscore_get(GAME_FLAPPY);
  for (int i = 0; i < NPIPE; i++) {
    s_pipe[i].x = s_w + 30 + i * SPACING;
    s_pipe[i].gap_y = rand_gap();
    s_pipe[i].scored = false;
  }
}

static void die(void) {
  s_dead = true;
  s_record = gp_highscore_submit(GAME_FLAPPY, s_score);
  vibes_long_pulse();
}

static void step(void) {
  if (!s_started) return;
  s_vy += 0.45f;
  s_by += s_vy;

  if (s_by - 7 < s_top) { s_by = s_top + 7; if (s_vy < 0) s_vy = 0; }   // bump the ceiling
  if (s_by + 7 >= s_ground_y) { s_by = s_ground_y - 7; die(); return; }

  for (int i = 0; i < NPIPE; i++) {
    s_pipe[i].x -= 2.0f;
    if (s_pipe[i].x + PW < 0) {                 // recycle off the left
      float maxx = 0;
      for (int j = 0; j < NPIPE; j++) if (s_pipe[j].x > maxx) maxx = s_pipe[j].x;
      s_pipe[i].x = maxx + SPACING;
      s_pipe[i].gap_y = rand_gap();
      s_pipe[i].scored = false;
    }
    // score when cleared
    if (!s_pipe[i].scored && s_pipe[i].x + PW < s_bird_x) {
      s_pipe[i].scored = true; s_score++; vibes_short_pulse();
    }
    // collision
    if (s_bird_x + 7 > s_pipe[i].x && s_bird_x - 7 < s_pipe[i].x + PW) {
      if (s_by - 7 < s_pipe[i].gap_y - GAP / 2 || s_by + 7 > s_pipe[i].gap_y + GAP / 2) {
        die(); return;
      }
    }
  }
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, SKY);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // pipes
  for (int i = 0; i < NPIPE; i++) {
    int x = (int)s_pipe[i].x, gy = s_pipe[i].gap_y;
    graphics_context_set_fill_color(ctx, PIPE);
    graphics_fill_rect(ctx, GRect(x, s_top, PW, gy - GAP / 2 - s_top), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(x, gy + GAP / 2, PW, s_ground_y - (gy + GAP / 2)), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorGreen);
    graphics_fill_rect(ctx, GRect(x - 2, gy - GAP / 2 - 6, PW + 4, 6), 1, GCornersAll);
    graphics_fill_rect(ctx, GRect(x - 2, gy + GAP / 2, PW + 4, 6), 1, GCornersAll);
  }

  // ground
  graphics_context_set_fill_color(ctx, GROUND);
  graphics_fill_rect(ctx, GRect(0, s_ground_y, b.size.w, b.size.h - s_ground_y), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorGreen);
  graphics_fill_rect(ctx, GRect(0, s_ground_y, b.size.w, 3), 0, GCornerNone);

  // bird
  int bx = s_bird_x, by = (int)s_by;
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_circle(ctx, GPoint(bx, by), 7);
  graphics_context_set_fill_color(ctx, GColorOrange);
  graphics_fill_rect(ctx, GRect(bx + 5, by - 1, 5, 3), 0, GCornerNone);     // beak
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(bx + 3, by - 3), 2);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(bx + 4, by - 3), 1);

  // score (big, centred near the top)
  char sc[8];
  snprintf(sc, sizeof(sc), "%d", s_score);
  graphics_context_set_text_color(ctx, GColorWhite);
  gp_text(ctx, sc, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK),
          GRect(0, 6, b.size.w, 36), GTextAlignmentCenter);

  if (!s_started && !s_dead) {
    graphics_context_set_text_color(ctx, GColorWhite);
    gp_text(ctx, "SELECT to flap", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(0, s_h / 2 + 18, b.size.w, 24), GTextAlignmentCenter);
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

static void flap(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(TICK_MS, tick, NULL);
    return;
  }
  s_started = true;
  s_vy = -5.0f;
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, flap);
  window_single_click_subscribe(BUTTON_ID_UP, flap);
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

void flappy_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
