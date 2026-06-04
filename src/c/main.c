#include "gp.h"

// ============================================================================
//  Game & Pebble - main menu
//
//  A scrolling arcade-style game picker with an animated "Pebble-Man chasing
//  dots" banner up top. UP/DOWN move the cursor, SELECT launches a game, and
//  BACK exits to the watch (the system default for the root window).
// ============================================================================

typedef struct {
  const char *name;
  const char *tagline;
  GColor      accent;
  GameId      id;
  void      (*launch)(void);
} MenuEntry;

static MenuEntry s_games[GAME_COUNT];

static Window   *s_window;
static Layer    *s_layer;
static AppTimer *s_timer;
static int       s_sel    = 0;   // selected game index
static int       s_anim   = 0;   // animation frame counter
static int       s_scroll = 0;   // current scroll offset in px (smoothed)

#define ROW_H        38
#define BANNER_H     34

// ----------------------------------------------------------------------------
//  Little vector glyphs, one per game, drawn inside a square tile.
// ----------------------------------------------------------------------------
static void draw_glyph(GContext *ctx, GameId id, GRect t, GColor fg) {
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);
  int cx = t.origin.x + t.size.w / 2;
  int cy = t.origin.y + t.size.h / 2;

  switch (id) {
    case GAME_PEBBLEMAN: {  // chomper facing right
      GRect r = GRect(cx - 9, cy - 9, 18, 18);
      graphics_fill_radial(ctx, r, GOvalScaleModeFitCircle, 9,
                           DEG_TO_TRIGANGLE(40), DEG_TO_TRIGANGLE(320));
    } break;
    case GAME_SNAKE: {      // a little staircase of segments
      for (int i = 0; i < 4; i++) {
        graphics_fill_rect(ctx, GRect(cx - 10 + i * 5, cy + 6 - i * 5, 5, 5), 1, GCornersAll);
      }
    } break;
    case GAME_ASTEROIDS: {  // a triangular ship
      GPoint a = GPoint(cx, cy - 9);
      GPoint b = GPoint(cx - 7, cy + 8);
      GPoint c = GPoint(cx + 7, cy + 8);
      graphics_draw_line(ctx, a, b);
      graphics_draw_line(ctx, a, c);
      graphics_draw_line(ctx, GPoint(cx - 4, cy + 3), GPoint(cx + 4, cy + 3));
    } break;
    case GAME_BLOCKS: {     // an S-tetromino
      graphics_fill_rect(ctx, GRect(cx - 2, cy - 9, 7, 7), 1, GCornersAll);
      graphics_fill_rect(ctx, GRect(cx + 4, cy - 9, 7, 7), 1, GCornersAll);
      graphics_fill_rect(ctx, GRect(cx - 9, cy - 2, 7, 7), 1, GCornersAll);
      graphics_fill_rect(ctx, GRect(cx - 2, cy - 2, 7, 7), 1, GCornersAll);
    } break;
    case GAME_BREAKOUT: {   // bricks above, ball below
      for (int i = 0; i < 3; i++) {
        graphics_fill_rect(ctx, GRect(cx - 10 + i * 7, cy - 8, 6, 4), 1, GCornersAll);
      }
      graphics_fill_circle(ctx, GPoint(cx, cy + 4), 3);
    } break;
    case GAME_CATCH: {      // a basket catching a drop
      graphics_fill_circle(ctx, GPoint(cx, cy - 6), 3);
      graphics_draw_line(ctx, GPoint(cx - 9, cy + 2), GPoint(cx - 9, cy + 8));
      graphics_draw_line(ctx, GPoint(cx + 9, cy + 2), GPoint(cx + 9, cy + 8));
      graphics_draw_line(ctx, GPoint(cx - 9, cy + 8), GPoint(cx + 9, cy + 8));
    } break;
    default: break;
  }
  graphics_context_set_stroke_width(ctx, 1);
}

// ----------------------------------------------------------------------------
//  Animated banner: Pebble-Man chasing a trail of dots, with a ghost behind.
// ----------------------------------------------------------------------------
static void draw_banner(GContext *ctx, GRect bounds) {
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, BANNER_H), 0, GCornerNone);

  int w = bounds.size.w;
  int lane_y = BANNER_H - 9;
  int span = w + 40;
  int pac_x = (s_anim * 3) % span - 20;

  // dots not yet eaten (anything ahead of the chomper)
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  for (int x = 8; x < w; x += 12) {
    if (x > pac_x + 6) {
      graphics_fill_circle(ctx, GPoint(x, lane_y), 2);
    }
  }

  // ghost trailing behind
  int gx = pac_x - 22;
  if (gx > -16 && gx < w + 16) {
    graphics_context_set_fill_color(ctx, GColorMelon);
    graphics_fill_rect(ctx, GRect(gx - 6, lane_y - 6, 12, 10), 0, GCornerNone);
    graphics_fill_radial(ctx, GRect(gx - 6, lane_y - 11, 12, 12), GOvalScaleModeFitCircle, 6,
                         0, DEG_TO_TRIGANGLE(360));
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(gx - 2, lane_y - 5), 2);
    graphics_fill_circle(ctx, GPoint(gx + 3, lane_y - 5), 2);
  }

  // the chomper, mouth opening and closing
  graphics_context_set_fill_color(ctx, GColorYellow);
  bool open = (s_anim % 2) == 0;
  GRect pr = GRect(pac_x - 7, lane_y - 7, 14, 14);
  if (open) {
    graphics_fill_radial(ctx, pr, GOvalScaleModeFitCircle, 7,
                         DEG_TO_TRIGANGLE(40), DEG_TO_TRIGANGLE(320));
  } else {
    graphics_fill_circle(ctx, GPoint(pac_x, lane_y), 7);
  }

  // title text on top of the banner
  graphics_context_set_text_color(ctx, GColorWhite);
  gp_text(ctx, "GAME & PEBBLE", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
          GRect(0, 1, w, 20), GTextAlignmentCenter);
}

// ----------------------------------------------------------------------------
static void menu_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int list_top = BANNER_H + 4;

  for (int i = 0; i < GAME_COUNT; i++) {
    int y = list_top + i * ROW_H - s_scroll;
    if (y + ROW_H < list_top || y > bounds.size.h) continue;  // off-screen

    bool sel = (i == s_sel);
    GRect row = GRect(4, y + 2, bounds.size.w - 8, ROW_H - 4);
    GRect tile = GRect(row.origin.x + 4, y + (ROW_H - 26) / 2, 26, 26);

    if (sel) {
      graphics_context_set_fill_color(ctx, s_games[i].accent);
      graphics_fill_rect(ctx, row, 6, GCornersAll);
      draw_glyph(ctx, s_games[i].id, tile, GColorBlack);

      graphics_context_set_text_color(ctx, GColorBlack);
      gp_text(ctx, s_games[i].name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
              GRect(tile.origin.x + 30, y + 2, row.size.w - 36, 24), GTextAlignmentLeft);

      char hi[20];
      snprintf(hi, sizeof(hi), "%s  ·  HI %d", s_games[i].tagline, gp_highscore_get(s_games[i].id));
      gp_text(ctx, hi, fonts_get_system_font(FONT_KEY_GOTHIC_14),
              GRect(tile.origin.x + 30, y + 22, row.size.w - 36, 16), GTextAlignmentLeft);
    } else {
      draw_glyph(ctx, s_games[i].id, tile, s_games[i].accent);
      graphics_context_set_text_color(ctx, GColorWhite);
      gp_text(ctx, s_games[i].name, fonts_get_system_font(FONT_KEY_GOTHIC_18),
              GRect(tile.origin.x + 30, y + (ROW_H - 20) / 2, row.size.w - 36, 22),
              GTextAlignmentLeft);
    }
  }

  // banner drawn last so the list scrolls "under" it
  draw_banner(ctx, bounds);
}

// ----------------------------------------------------------------------------
static void tick(void *data) {
  s_anim++;

  // smooth-scroll so the selection stays comfortably in view
  GRect bounds = layer_get_bounds(s_layer);
  int list_top = BANNER_H + 4;
  int list_h = bounds.size.h - list_top;
  int target = s_sel * ROW_H - (list_h - ROW_H) / 2;
  int max_scroll = GAME_COUNT * ROW_H - list_h;
  if (max_scroll < 0) max_scroll = 0;
  if (target < 0) target = 0;
  if (target > max_scroll) target = max_scroll;
  s_scroll += (target - s_scroll) / 3;            // ease toward target
  if (target - s_scroll != 0 && target - s_scroll > -2 && target - s_scroll < 2) s_scroll = target;

  layer_mark_dirty(s_layer);
  s_timer = app_timer_register(90, tick, NULL);
}

// ----------------------------------------------------------------------------
static void up_click(ClickRecognizerRef r, void *ctx) {
  s_sel = (s_sel - 1 + GAME_COUNT) % GAME_COUNT;
  layer_mark_dirty(s_layer);
}
static void down_click(ClickRecognizerRef r, void *ctx) {
  s_sel = (s_sel + 1) % GAME_COUNT;
  layer_mark_dirty(s_layer);
}
static void select_click(ClickRecognizerRef r, void *ctx) {
  if (s_games[s_sel].launch) s_games[s_sel].launch();
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

// ----------------------------------------------------------------------------
static void win_appear(Window *window) {
  layer_mark_dirty(s_layer);
  s_timer = app_timer_register(90, tick, NULL);
}
static void win_disappear(Window *window) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
}
static void win_load(Window *window) {
  s_layer = window_get_root_layer(window);
  layer_set_update_proc(s_layer, menu_update);
}

// ----------------------------------------------------------------------------
static void init_roster(void) {
  s_games[GAME_PEBBLEMAN] = (MenuEntry){"PEBBLE-MAN", "chomp the dots",  GColorYellow,   GAME_PEBBLEMAN, pebbleman_push};
  s_games[GAME_SNAKE]     = (MenuEntry){"SNAKE",      "eat & grow",      GColorGreen,    GAME_SNAKE,     snake_push};
  s_games[GAME_ASTEROIDS] = (MenuEntry){"ASTEROIDS",  "blast the rocks", GColorCyan,     GAME_ASTEROIDS, asteroids_push};
  s_games[GAME_BLOCKS]    = (MenuEntry){"BLOCKS",     "stack & clear",   GColorMagenta,  GAME_BLOCKS,    blocks_push};
  s_games[GAME_BREAKOUT]  = (MenuEntry){"BREAKOUT",   "smash bricks",    GColorOrange,   GAME_BREAKOUT,  breakout_push};
  s_games[GAME_CATCH]     = (MenuEntry){"CATCH!",     "an LCD classic",  GColorSpringBud,GAME_CATCH,     catch_push};
}

static void init(void) {
  srand((unsigned)time(NULL));
  init_roster();
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = win_load, .appear = win_appear, .disappear = win_disappear,
  });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
