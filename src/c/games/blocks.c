#include "../gp.h"

// ============================================================================
//  BLOCKS  -  the falling-tetromino classic.
//
//  Controls:  UP = move left   DOWN = move right
//             SELECT tap = rotate    SELECT hold = soft drop    (BACK = menu)
// ============================================================================

#define COLS  10
#define ROWS  16

// Each piece in 4 rotations, each a list of 4 (x,y) cells inside a 4x4 box.
typedef struct { int8_t x, y; } Cell;
static const Cell PIECES[7][4][4] = {
  // I
  {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
  // O
  {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
  // T
  {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
  // S
  {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}}, {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
  // Z
  {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}},
  // J
  {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
  // L
  {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}},
};

static GColor piece_color(int p) {
  // explicit b/w fallbacks: purple/red/blue luminance-map to black (invisible)
  switch (p) {
    case 0: return COLOR_FALLBACK(GColorCyan, GColorWhite);
    case 1: return COLOR_FALLBACK(GColorYellow, GColorWhite);
    case 2: return COLOR_FALLBACK(GColorPurple, GColorWhite);
    case 3: return COLOR_FALLBACK(GColorGreen, GColorWhite);
    case 4: return COLOR_FALLBACK(GColorRed, GColorWhite);
    case 5: return COLOR_FALLBACK(GColorBlue, GColorWhite);
    default: return COLOR_FALLBACK(GColorOrange, GColorWhite);
  }
}

static Window   *s_window;
static AppTimer *s_timer;
static uint8_t   s_board[ROWS][COLS];   // 0 empty, else piece+1
static int  s_piece, s_rot, s_px, s_py, s_next;
static int  s_score, s_lines, s_level, s_best;
static bool s_dead, s_record, s_blink, s_soft;
static int  s_cell, s_bx, s_by;         // board pixel geometry

static int gravity_ms(void) {
  int g = 800 - (s_level - 1) * 65;
  return g < 90 ? 90 : g;
}

static bool fits(int piece, int rot, int px, int py) {
  for (int k = 0; k < 4; k++) {
    int bx = px + PIECES[piece][rot][k].x;
    int by = py + PIECES[piece][rot][k].y;
    if (bx < 0 || bx >= COLS || by >= ROWS) return false;
    if (by >= 0 && s_board[by][bx]) return false;
  }
  return true;
}

static void spawn(void) {
  s_piece = s_next;
  s_next = rand() % 7;
  s_rot = 0;
  s_px = COLS / 2 - 2;
  s_py = 0;
  if (!fits(s_piece, s_rot, s_px, s_py)) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_BLOCKS, s_score);
    vibes_double_pulse();
  }
}

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  int hud = 22;
  int ph = b.size.h - hud;
  s_cell = ph / ROWS;
  if (s_cell * COLS > b.size.w - 6) s_cell = (b.size.w - 6) / COLS;
  s_bx = (b.size.w - s_cell * COLS) / 2;
  s_by = hud + (ph - s_cell * ROWS) / 2;

  memset(s_board, 0, sizeof(s_board));
  s_score = 0; s_lines = 0; s_level = 1;
  s_dead = false; s_record = false; s_soft = false;
  s_best = gp_highscore_get(GAME_BLOCKS);
  s_next = rand() % 7;
  spawn();
}

static void clear_lines(void) {
  int cleared = 0;
  for (int y = ROWS - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < COLS; x++) if (!s_board[y][x]) { full = false; break; }
    if (full) {
      cleared++;
      for (int yy = y; yy > 0; yy--) memcpy(s_board[yy], s_board[yy - 1], COLS);
      memset(s_board[0], 0, COLS);
      y++;                              // recheck this row after the shift
    }
  }
  if (cleared) {
    static const int pts[5] = {0, 100, 300, 500, 800};
    s_score += pts[cleared] * s_level;
    s_lines += cleared;
    s_level = 1 + s_lines / 10;
    vibes_short_pulse();
  }
}

static void lock_piece(void) {
  for (int k = 0; k < 4; k++) {
    int bx = s_px + PIECES[s_piece][s_rot][k].x;
    int by = s_py + PIECES[s_piece][s_rot][k].y;
    if (by >= 0) s_board[by][bx] = s_piece + 1;
  }
  clear_lines();
  spawn();
}

static void gravity_step(void) {
  if (fits(s_piece, s_rot, s_px, s_py + 1)) s_py++;
  else lock_piece();
}

static void draw_cell(GContext *ctx, int px, int py, GColor col, bool ghost) {
  GRect r = GRect(s_bx + px * s_cell, s_by + py * s_cell, s_cell, s_cell);
  if (ghost) {
    graphics_context_set_stroke_color(ctx, col);
    graphics_draw_rect(ctx, grect_inset(r, GEdgeInsets(1)));
  } else {
    graphics_context_set_fill_color(ctx, col);
    graphics_fill_rect(ctx, grect_inset(r, GEdgeInsets(0, 1, 1, 0)), 1, GCornersAll);
  }
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  char l[16], r[12];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  snprintf(r, sizeof(r), "LV %d", s_level);
  gp_hud(ctx, b, l, r, COLOR_FALLBACK(GColorMagenta, GColorWhite));

  // board well
  GRect well = GRect(s_bx - 2, s_by - 2, COLS * s_cell + 4, ROWS * s_cell + 4);
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, well, 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_rect(ctx, well);

  // settled blocks
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++)
      if (s_board[y][x]) draw_cell(ctx, x, y, piece_color(s_board[y][x] - 1), false);

  if (!s_dead) {
    // ghost (landing preview)
    int gy = s_py;
    while (fits(s_piece, s_rot, s_px, gy + 1)) gy++;
    for (int k = 0; k < 4; k++)
      draw_cell(ctx, s_px + PIECES[s_piece][s_rot][k].x, gy + PIECES[s_piece][s_rot][k].y,
                GColorLightGray, true);
    // active piece
    for (int k = 0; k < 4; k++)
      draw_cell(ctx, s_px + PIECES[s_piece][s_rot][k].x, s_py + PIECES[s_piece][s_rot][k].y,
                piece_color(s_piece), false);
  }

  // NEXT preview in the right margin (if there's room)
  int panel_x = s_bx + COLS * s_cell + 6;
  if (b.size.w - panel_x >= 30) {
    graphics_context_set_text_color(ctx, GColorWhite);
    gp_text(ctx, "NEXT", fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(panel_x, s_by, 40, 16),
            GTextAlignmentLeft);
    int mc = 7;
    for (int k = 0; k < 4; k++) {
      int cx = panel_x + PIECES[s_next][0][k].x * mc;
      int cy = s_by + 18 + PIECES[s_next][0][k].y * mc;
      graphics_context_set_fill_color(ctx, piece_color(s_next));
      graphics_fill_rect(ctx, GRect(cx, cy, mc - 1, mc - 1), 1, GCornersAll);
    }
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
  gravity_step();
  layer_mark_dirty(window_get_root_layer(s_window));
  s_timer = app_timer_register(s_soft ? 45 : gravity_ms(), tick, NULL);
}

static void move(int dx) {
  if (s_dead) return;
  if (fits(s_piece, s_rot, s_px + dx, s_py)) { s_px += dx; layer_mark_dirty(window_get_root_layer(s_window)); }
}
static void rotate_click(ClickRecognizerRef r, void *c) {
  if (s_dead) { return; }
  int nr = (s_rot + 1) % 4;
  int kicks[] = {0, -1, 1, -2, 2};
  for (int i = 0; i < 5; i++) {
    if (fits(s_piece, nr, s_px + kicks[i], s_py)) { s_rot = nr; s_px += kicks[i]; break; }
  }
  layer_mark_dirty(window_get_root_layer(s_window));
}
static void left_click(ClickRecognizerRef r, void *c)  { move(-1); }
static void right_click(ClickRecognizerRef r, void *c) { move(+1); }
static void select_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(gravity_ms(), tick, NULL);
  } else {
    rotate_click(r, c);
  }
}
static void soft_down(ClickRecognizerRef r, void *c) {
  if (s_dead) return;
  s_soft = true;
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = app_timer_register(45, tick, NULL);
}
static void soft_up(ClickRecognizerRef r, void *c) { s_soft = false; }

static void click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 130, left_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 130, right_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 220, soft_down, soft_up);
}

static void win_load(Window *window) {
  gp_focus_subscribe();
  layer_set_update_proc(window_get_root_layer(window), render);
  reset_game();
  s_timer = app_timer_register(gravity_ms(), tick, NULL);
}
static void win_unload(Window *window) {
  gp_focus_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
}

void blocks_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
