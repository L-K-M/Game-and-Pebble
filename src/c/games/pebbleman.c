#include "../gp.h"

// ============================================================================
//  PEBBLE-MAN  -  our wrist-sized take on the dot-munching maze classic.
//
//  Chomp every pellet, dodge the ghosts, and grab a power pellet to turn the
//  tables and eat them for big points.
//
//  Controls (relative steering):  UP = turn left   DOWN = turn right
//                                 SELECT = restart when caught   (BACK = menu)
// ============================================================================

#define MR 17
#define MC 15
#define NG 4
#define TICK_MS      40
#define FRIGHT_TICKS 150

// 15x15... no, 15 wide x 17 tall. Verified fully-connected (tools/verify_maze.py)
static const char *MAZE[MR] = {
  "###############",
  "#o....#.#....o#",
  "#.###.#.#.###.#",
  "#.............#",
  "#.###.###.###.#",
  "#...#...#...#.#",
  "###.###.###.#.#",
  "#.....# #.....#",
  "#.###.#-#.###.#",
  "#.#...G G...#.#",
  "#.#.#.###.#.#.#",
  "#...#..P..#...#",
  "#.#########.#.#",
  "#.#.......#.#.#",
  "#o..#.###.#..o#",
  "#####.....#####",
  "###############",
};

// (col,row) starts
static const int PAC_C = 7, PAC_R = 11;
static const int GH_C[NG] = {6, 8, 7, 7};
static const int GH_R[NG] = {9, 9, 9, 7};

static Window   *s_window;
static AppTimer *s_timer;

static char  s_grid[MR][MC];
static int   s_remaining;
static int   s_tile, s_ox, s_oy;
static int   s_score, s_best, s_lives, s_level, s_anim;
static int   s_fright, s_combo, s_ready;
static bool  s_dead, s_record, s_blink;

// pac
static int   s_pc, s_pr, s_pdx, s_pdy, s_ldx, s_ldy, s_desx, s_desy;
static float s_pprog;
// ghosts
static int   s_gc[NG], s_gr[NG], s_gdx[NG], s_gdy[NG], s_gcool[NG];
static float s_gprog[NG];

static bool open_cell(int c, int r) {
  if (c < 0 || c >= MC || r < 0 || r >= MR) return false;
  return s_grid[r][c] != '#';
}
static GPoint pix(int c, int r, int dx, int dy, float prog) {
  return GPoint(s_ox + c * s_tile + s_tile / 2 + (int)(dx * prog * s_tile),
                s_oy + r * s_tile + s_tile / 2 + (int)(dy * prog * s_tile));
}
static GColor ghost_color(int i) {
  // explicit b/w fallbacks: red/orange luminance-map to black (invisible)
  switch (i) { case 0: return COLOR_FALLBACK(GColorRed, GColorWhite);
               case 1: return COLOR_FALLBACK(GColorBrilliantRose, GColorWhite);
               case 2: return COLOR_FALLBACK(GColorCyan, GColorWhite);
               default: return COLOR_FALLBACK(GColorOrange, GColorWhite); }
}
static int head_deg(int dx, int dy) {
  if (dx > 0) return 90;
  if (dx < 0) return 270;
  if (dy > 0) return 180;
  return 0;
}

static void build_grid(void) {
  s_remaining = 0;
  for (int r = 0; r < MR; r++) {
    for (int c = 0; c < MC; c++) {
      char ch = MAZE[r][c];
      if (ch == '.' || ch == 'o') { s_grid[r][c] = ch; s_remaining++; }
      else if (ch == '#') s_grid[r][c] = '#';
      else s_grid[r][c] = ' ';
    }
  }
}

static void choose_ghost_dir(int i) {
  static const int D[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
  bool fr = (s_fright > 0 && s_gcool[i] == 0);
  int best = -1; long bestscore = 0;
  for (int k = 0; k < 4; k++) {
    int dx = D[k][0], dy = D[k][1];
    if (dx == -s_gdx[i] && dy == -s_gdy[i]) continue;      // no 180s
    if (!open_cell(s_gc[i] + dx, s_gr[i] + dy)) continue;
    long ddx = s_gc[i] + dx - s_pc, ddy = s_gr[i] + dy - s_pr;
    long dist = ddx * ddx + ddy * ddy;
    long score = (fr ? -dist : dist) + (rand() % 3 - 1);
    if (best < 0 || score < bestscore) { best = k; bestscore = score; }
  }
  if (best < 0) { s_gdx[i] = -s_gdx[i]; s_gdy[i] = -s_gdy[i]; }  // dead end: turn around
  else { s_gdx[i] = D[best][0]; s_gdy[i] = D[best][1]; }
}

static void reset_positions(void) {
  s_pc = PAC_C; s_pr = PAC_R; s_pprog = 0;
  s_pdx = -1; s_pdy = 0; s_ldx = -1; s_ldy = 0; s_desx = 0; s_desy = 0;
  if (!open_cell(s_pc - 1, s_pr)) { s_pdx = 0; }              // safety
  s_fright = 0; s_combo = 0;
  for (int i = 0; i < NG; i++) {
    s_gc[i] = GH_C[i]; s_gr[i] = GH_R[i]; s_gprog[i] = 0;
    s_gdx[i] = 0; s_gdy[i] = 0; s_gcool[i] = 0;
    choose_ghost_dir(i);
  }
}

static void reset_game(void) {
  GRect b = layer_get_bounds(window_get_root_layer(s_window));
  int hud = 22;
  s_tile = (b.size.w) / MC;
  if (s_tile * MR > b.size.h - hud) s_tile = (b.size.h - hud) / MR;
  s_ox = (b.size.w - s_tile * MC) / 2;
  s_oy = hud + (b.size.h - hud - s_tile * MR) / 2;

  build_grid();
  s_score = 0; s_lives = 3; s_level = 1; s_ready = 45;
  s_dead = false; s_record = false;
  s_best = gp_highscore_get(GAME_PEBBLEMAN);
  reset_positions();
}

static void next_level(void) {
  s_level++;
  build_grid();
  reset_positions();
  s_ready = 45;
  vibes_double_pulse();
}

static float pac_speed(void)   { float s = 0.15f + s_level * 0.012f; return s > 0.30f ? 0.30f : s; }
static float ghost_speed(void) { float s = 0.13f + s_level * 0.012f; return s > 0.26f ? 0.26f : s; }

static void eat(int c, int r) {
  char *cell = &s_grid[r][c];
  if (*cell == '.') { *cell = ' '; s_score += 10; s_remaining--; }
  else if (*cell == 'o') { *cell = ' '; s_score += 50; s_remaining--; s_fright = FRIGHT_TICKS; s_combo = 0; }
  if (s_remaining <= 0) next_level();
}

static void move_pac(void) {
  if (s_pdx == 0 && s_pdy == 0) {                            // stopped: try to go
    if ((s_desx || s_desy) && open_cell(s_pc + s_desx, s_pr + s_desy)) {
      s_pdx = s_desx; s_pdy = s_desy; s_desx = s_desy = 0;
    }
  }
  if (s_pdx || s_pdy) {
    s_ldx = s_pdx; s_ldy = s_pdy;
    s_pprog += pac_speed();
    while (s_pprog >= 1.0f) {
      s_pc += s_pdx; s_pr += s_pdy; s_pprog -= 1.0f;
      eat(s_pc, s_pr);
      if (s_ready > 0) break;                                 // level just cleared; positions reset
      if ((s_desx || s_desy) && open_cell(s_pc + s_desx, s_pr + s_desy)) {
        s_pdx = s_desx; s_pdy = s_desy; s_desx = s_desy = 0; s_ldx = s_pdx; s_ldy = s_pdy;
      }
      if (!open_cell(s_pc + s_pdx, s_pr + s_pdy)) { s_pdx = 0; s_pdy = 0; s_pprog = 0; break; }
    }
  }
}

static void move_ghosts(void) {
  float sp = (s_fright > 0) ? 0.085f : ghost_speed();
  for (int i = 0; i < NG; i++) {
    if (s_gcool[i] > 0) { s_gcool[i]--; continue; }
    if (s_gdx[i] == 0 && s_gdy[i] == 0) choose_ghost_dir(i);
    s_gprog[i] += sp;
    while (s_gprog[i] >= 1.0f) {
      s_gc[i] += s_gdx[i]; s_gr[i] += s_gdy[i]; s_gprog[i] -= 1.0f;
      choose_ghost_dir(i);
    }
  }
}

static void lose_life(void) {
  s_lives--;
  vibes_long_pulse();
  if (s_lives <= 0) {
    s_dead = true;
    s_record = gp_highscore_submit(GAME_PEBBLEMAN, s_score);
  } else {
    reset_positions();
    s_ready = 35;
  }
}

static void check_collisions(void) {
  GPoint pp = pix(s_pc, s_pr, s_pdx, s_pdy, s_pprog);
  int thr = (int)(s_tile * 0.7f);
  for (int i = 0; i < NG; i++) {
    if (s_gcool[i] > 0) continue;
    GPoint gp = pix(s_gc[i], s_gr[i], s_gdx[i], s_gdy[i], s_gprog[i]);
    int dx = pp.x - gp.x, dy = pp.y - gp.y;
    if (dx * dx + dy * dy < thr * thr) {
      if (s_fright > 0) {
        int shift = s_combo < 3 ? s_combo : 3;
        s_score += 200 << shift;
        s_combo++;
        s_gcool[i] = 90;
        s_gc[i] = GH_C[i]; s_gr[i] = GH_R[i]; s_gprog[i] = 0; s_gdx[i] = 0; s_gdy[i] = 0;
        vibes_short_pulse();
      } else {
        lose_life();
        return;
      }
    }
  }
}

// ---- drawing ----
static void draw_pac(GContext *ctx, int cx, int cy, int r, int heading, int open) {
  graphics_context_set_fill_color(ctx, GColorYellow);
  GRect rr = GRect(cx - r, cy - r, 2 * r, 2 * r);
  graphics_fill_radial(ctx, rr, GOvalScaleModeFitCircle, r,
                       DEG_TO_TRIGANGLE(heading + open / 2),
                       DEG_TO_TRIGANGLE(heading + 360 - open / 2));
}

static void draw_ghost(GContext *ctx, int i, int cx, int cy, int r) {
  GColor body;
  if (s_gcool[i] > 0) body = GColorDarkGray;
  else if (s_fright > 0) body = (s_fright < 40 && (s_anim / 3) % 2)
                                ? GColorWhite : COLOR_FALLBACK(GColorBlue, GColorDarkGray);
  else body = ghost_color(i);

  graphics_context_set_fill_color(ctx, body);
  graphics_fill_circle(ctx, GPoint(cx, cy), r);
  graphics_fill_rect(ctx, GRect(cx - r, cy, 2 * r + 1, r + 1), 0, GCornerNone);
  // carve a wavy skirt with the background colour
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int k = 0; k < 3; k++) {
    graphics_fill_circle(ctx, GPoint(cx - r + r / 2 + k * r, cy + r), r / 3 + 1);
  }
  // eyes
  bool scared = (s_fright > 0 && s_gcool[i] == 0);
  graphics_context_set_fill_color(ctx, scared ? body : GColorWhite);
  if (!scared) {
    graphics_fill_circle(ctx, GPoint(cx - r / 2, cy - 1), r / 3 + 1);
    graphics_fill_circle(ctx, GPoint(cx + r / 2, cy - 1), r / 3 + 1);
    graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorBlue, GColorBlack));
    int ex = s_gdx[i] * 2, ey = s_gdy[i] * 2;
    graphics_fill_circle(ctx, GPoint(cx - r / 2 + ex, cy - 1 + ey), 2);
    graphics_fill_circle(ctx, GPoint(cx + r / 2 + ex, cy - 1 + ey), 2);
  } else {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(cx - r / 2, cy - 1), 2);
    graphics_fill_circle(ctx, GPoint(cx + r / 2, cy - 1), 2);
  }
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // maze
  for (int r = 0; r < MR; r++) {
    for (int c = 0; c < MC; c++) {
      int x = s_ox + c * s_tile, y = s_oy + r * s_tile;
      char ch = s_grid[r][c];
      if (ch == '#') {
        // blue → black on 1-bit displays; dithered gray keeps the maze visible
        graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorBlue, GColorLightGray));
        graphics_fill_rect(ctx, GRect(x, y, s_tile, s_tile), 0, GCornerNone);
      } else if (ch == '.') {
        graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorChromeYellow, GColorWhite));
        graphics_fill_circle(ctx, GPoint(x + s_tile / 2, y + s_tile / 2), 2);
      } else if (ch == 'o') {
        if ((s_anim / 4) % 2 == 0) {
          graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorChromeYellow, GColorWhite));
          graphics_fill_circle(ctx, GPoint(x + s_tile / 2, y + s_tile / 2), s_tile / 4 + 1);
        }
      }
    }
  }

  // ghosts
  int er = s_tile / 2;
  for (int i = 0; i < NG; i++) {
    GPoint gp = pix(s_gc[i], s_gr[i], s_gdx[i], s_gdy[i], s_gprog[i]);
    draw_ghost(ctx, i, gp.x, gp.y, er);
  }

  // pac (hidden for a beat on death-pause so it reads as "caught")
  if (!s_dead) {
    GPoint pp = pix(s_pc, s_pr, s_pdx, s_pdy, s_pprog);
    int heading = head_deg(s_pdx ? s_pdx : s_ldx, s_pdy ? s_pdy : s_ldy);
    int open = ((s_anim / 2) % 2) ? 50 : 12;
    draw_pac(ctx, pp.x, pp.y, er, heading, open);
  }

  // HUD: score + lives
  char l[16];
  snprintf(l, sizeof(l), "SCORE %d", s_score);
  gp_hud(ctx, b, l, NULL, GColorYellow);
  for (int i = 0; i < s_lives; i++) draw_pac(ctx, b.size.w - 11 - i * 14, 11, 5, 270, 45);

  if (s_ready > 0 && !s_dead) {
    graphics_context_set_text_color(ctx, GColorYellow);
    gp_text(ctx, "READY!", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
            GRect(0, s_oy + s_tile * MR / 2 - 16, b.size.w, 30), GTextAlignmentCenter);
  }
  if (s_dead) gp_overlay_gameover(ctx, b, "GAME OVER", s_score, s_best, s_record, s_blink);
}

static void tick(void *data) {
  if (s_dead) {
    s_blink = !s_blink;
    layer_mark_dirty(window_get_root_layer(s_window));
    s_timer = app_timer_register(400, tick, NULL);
    return;
  }
  s_anim++;
  if (s_ready > 0) {
    s_ready--;
    layer_mark_dirty(window_get_root_layer(s_window));
    s_timer = app_timer_register(TICK_MS, tick, NULL);
    return;
  }
  move_pac();
  move_ghosts();
  check_collisions();
  if (s_fright > 0) s_fright--;
  layer_mark_dirty(window_get_root_layer(s_window));
  s_timer = app_timer_register(TICK_MS, tick, NULL);
}

static void turn_left(ClickRecognizerRef r, void *c) {     // CCW
  if (s_dead) return;
  int rx = s_pdx ? s_pdx : s_ldx, ry = s_pdy ? s_pdy : s_ldy;
  s_desx = ry; s_desy = -rx;
}
static void turn_right(ClickRecognizerRef r, void *c) {    // CW
  if (s_dead) return;
  int rx = s_pdx ? s_pdx : s_ldx, ry = s_pdy ? s_pdy : s_ldy;
  s_desx = -ry; s_desy = rx;
}
static void select_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(TICK_MS, tick, NULL);
  }
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, turn_left);
  window_single_click_subscribe(BUTTON_ID_DOWN, turn_right);
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

void pebbleman_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
