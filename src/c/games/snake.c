#include "../gp.h"

// ============================================================================
//  SNAKE  -  steer the worm, eat apples, don't bite yourself.
//
//  Controls (relative steering - the only three legal moves at any moment):
//    UP   = turn left      DOWN = turn right      SELECT = restart when dead
//  A snake can only ever go straight/left/right, so two buttons cover it all.
// ============================================================================

#define CELL       10
#define MAX_COLS   30
#define MAX_ROWS   30
#define MAX_CELLS  (MAX_COLS * MAX_ROWS)
#define START_LEN  4

static Window   *s_window;
static AppTimer *s_timer;

static GPoint s_body[MAX_CELLS];   // ring buffer; s_body[s_head] is the head
static int    s_head;
static int    s_len;
static int    s_dx, s_dy;          // committed direction
static int    s_ndx, s_ndy;        // queued direction for next step
static GPoint s_food;
static int    s_cols, s_rows;
static int    s_ox, s_oy;          // grid pixel origin
static int    s_score, s_best, s_speed;
static bool   s_dead, s_record, s_blink;

static bool cell_is_snake(int x, int y) {
  for (int k = 0; k < s_len; k++) {
    int idx = (s_head - k + MAX_CELLS) % MAX_CELLS;
    if (s_body[idx].x == x && s_body[idx].y == y) return true;
  }
  return false;
}

static void place_food(void) {
  if (s_len >= s_cols * s_rows) return;   // board full (a win!)
  for (int tries = 0; tries < 500; tries++) {
    int x = rand() % s_cols;
    int y = rand() % s_rows;
    if (!cell_is_snake(x, y)) { s_food = GPoint(x, y); return; }
  }
}

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  int hud = 22;
  int pw = b.size.w, ph = b.size.h - hud;
  s_cols = pw / CELL; if (s_cols > MAX_COLS) s_cols = MAX_COLS;
  s_rows = ph / CELL; if (s_rows > MAX_ROWS) s_rows = MAX_ROWS;
  s_ox = (b.size.w - s_cols * CELL) / 2;
  s_oy = hud + (ph - s_rows * CELL) / 2;

  s_len = START_LEN;
  s_head = s_len - 1;
  int cx = s_cols / 3, cy = s_rows / 2;
  for (int k = 0; k < s_len; k++) {
    s_body[k] = GPoint(cx - (s_len - 1) + k, cy);  // tail..head, heading right
  }
  s_dx = 1; s_dy = 0; s_ndx = 1; s_ndy = 0;
  s_score = 0; s_speed = 150;
  s_dead = false; s_record = false;
  s_best = gp_highscore_get(GAME_SNAKE);
  place_food();
}

static void step(void) {
  s_dx = s_ndx; s_dy = s_ndy;
  GPoint head = s_body[s_head];
  int nx = head.x + s_dx, ny = head.y + s_dy;

  if (nx < 0 || nx >= s_cols || ny < 0 || ny >= s_rows) { s_dead = true; }
  bool grow = (!s_dead && nx == s_food.x && ny == s_food.y);

  if (!s_dead) {
    for (int k = 0; k < s_len; k++) {
      int idx = (s_head - k + MAX_CELLS) % MAX_CELLS;
      if (s_body[idx].x == nx && s_body[idx].y == ny) {
        if (k == s_len - 1 && !grow) continue;     // tail will move out of the way
        s_dead = true; break;
      }
    }
  }

  if (s_dead) {
    s_record = gp_highscore_submit(GAME_SNAKE, s_score);
    vibes_short_pulse();
    return;
  }

  s_head = (s_head + 1) % MAX_CELLS;
  s_body[s_head] = GPoint(nx, ny);
  if (grow) {
    if (s_len < MAX_CELLS) s_len++;
    s_score++;
    if (s_speed > 70) s_speed -= 4;
    place_food();
  }
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  char l[16], r[16];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  snprintf(r, sizeof(r), "BEST %d", s_best);
  gp_hud(ctx, b, l, r, GColorGreen);

  // playfield frame
  GRect field = GRect(s_ox - 2, s_oy - 2, s_cols * CELL + 4, s_rows * CELL + 4);
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_rect(ctx, field);

  // food (an apple)
  GPoint fc = GPoint(s_ox + s_food.x * CELL + CELL / 2, s_oy + s_food.y * CELL + CELL / 2);
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_circle(ctx, fc, CELL / 2 - 1);
  graphics_context_set_fill_color(ctx, GColorIslamicGreen);
  graphics_fill_rect(ctx, GRect(fc.x - 1, fc.y - CELL / 2 - 1, 2, 3), 0, GCornerNone);

  // snake
  for (int k = 0; k < s_len; k++) {
    int idx = (s_head - k + MAX_CELLS) % MAX_CELLS;
    GPoint c = s_body[idx];
    GRect seg = GRect(s_ox + c.x * CELL + 1, s_oy + c.y * CELL + 1, CELL - 2, CELL - 2);
    graphics_context_set_fill_color(ctx, (k == 0) ? GColorScreaminGreen : GColorGreen);
    graphics_fill_rect(ctx, seg, 2, GCornersAll);
  }
  // eyes on the head
  GPoint h = s_body[s_head];
  int hx = s_ox + h.x * CELL, hy = s_oy + h.y * CELL;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(hx + CELL / 2 + s_dx * 2 - 2, hy + CELL / 2 + s_dy * 2 - 1), 1);
  graphics_fill_circle(ctx, GPoint(hx + CELL / 2 + s_dx * 2 + 2, hy + CELL / 2 + s_dy * 2 + 1), 1);

  if (s_dead) {
    gp_overlay_gameover(ctx, b, "GAME OVER", s_score, s_best, s_record, s_blink);
  }
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
  s_timer = app_timer_register(s_speed, tick, NULL);
}

static void turn_left(ClickRecognizerRef r, void *c) {   // CCW (screen y-down)
  if (s_dead) return;
  s_ndx = s_dy; s_ndy = -s_dx;
}
static void turn_right(ClickRecognizerRef r, void *c) {   // CW
  if (s_dead) return;
  s_ndx = -s_dy; s_ndy = s_dx;
}
static void select_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(s_speed, tick, NULL);
  }
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, turn_left);
  window_single_click_subscribe(BUTTON_ID_DOWN, turn_right);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

static void win_load(Window *window) {
  gp_focus_subscribe();
  layer_set_update_proc(window_get_root_layer(window), render);
  reset_game();
  s_timer = app_timer_register(s_speed, tick, NULL);
}
static void win_unload(Window *window) {
  gp_focus_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
}

void snake_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
