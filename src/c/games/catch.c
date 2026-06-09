#include "../gp.h"

// ============================================================================
//  CATCH!  -  an homage to the Nintendo LCD handhelds / Game & Watch.
//
//  Eggs tumble down four chutes a step at a time, just like segmented LCD
//  pixels switching on and off. Slide your basket under each one before it
//  hits the ground. Three misses and it's over.
//
//  Controls:  UP = basket left   DOWN = basket right   SELECT = retry
// ============================================================================

#define LANES       4
#define STEPS       7
#define MAXITEMS    8
#define MISS_LIMIT  3

// The classic grey-green LCD look (degrades to white/black on mono watches).
#define LCD_BG   GColorFromRGB(170, 175, 90)
#define LCD_INK  GColorBlack

static Window   *s_window;
static AppTimer *s_timer;

static struct { int lane, step; bool active; } s_items[MAXITEMS];
static int  s_catcher;                 // lane 0..LANES-1
static int  s_score, s_best, s_misses, s_speed;
static bool s_dead, s_record, s_blink;

static int active_count(void) {
  int n = 0;
  for (int i = 0; i < MAXITEMS; i++) if (s_items[i].active) n++;
  return n;
}

static void spawn(void) {
  if (active_count() >= LANES) return;
  // pick a lane that doesn't already have a fresh egg up top
  int lane = rand() % LANES;
  for (int i = 0; i < MAXITEMS; i++)
    if (s_items[i].active && s_items[i].lane == lane && s_items[i].step <= 1) return;
  for (int i = 0; i < MAXITEMS; i++) {
    if (!s_items[i].active) {
      s_items[i].lane = lane; s_items[i].step = 0; s_items[i].active = true;
      return;
    }
  }
}

static void reset_game(void) {
  for (int i = 0; i < MAXITEMS; i++) s_items[i].active = false;
  s_catcher = LANES / 2;
  s_score = 0; s_misses = 0; s_speed = 560;
  s_dead = false; s_record = false;
  s_best = gp_highscore_get(GAME_CATCH);
  spawn();
}

static void step_world(void) {
  for (int i = 0; i < MAXITEMS; i++) {
    if (!s_items[i].active) continue;
    s_items[i].step++;
    if (s_items[i].step >= STEPS) {                 // reached the basket line
      if (s_items[i].lane == s_catcher) {
        s_score++;
        if (s_speed > 240) s_speed -= 12;
      } else {
        s_misses++;
        vibes_short_pulse();
        if (s_misses >= MISS_LIMIT) {
          s_dead = true;
          s_record = gp_highscore_submit(GAME_CATCH, s_score);
          vibes_double_pulse();
        }
      }
      s_items[i].active = false;
    }
  }
  int chance = 42 + s_score;                        // ramps up with score
  if (chance > 80) chance = 80;
  if ((rand() % 100) < chance) spawn();
}

static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  // bezel + LCD panel
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  GRect lcd = GRect(5, 5, b.size.w - 10, b.size.h - 10);
  graphics_context_set_fill_color(ctx, LCD_BG);
  graphics_fill_rect(ctx, lcd, 4, GCornersAll);

  // header: title + segmented score readout
  graphics_context_set_text_color(ctx, LCD_INK);
  gp_text(ctx, "CATCH", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
          GRect(lcd.origin.x + 6, lcd.origin.y + 2, 80, 22), GTextAlignmentLeft);
  char sc[8];
  snprintf(sc, sizeof(sc), "%d", s_score);
  gp_text(ctx, sc, fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS),
          GRect(lcd.origin.x + lcd.size.w - 64, lcd.origin.y + 2, 58, 24), GTextAlignmentRight);

  // miss pips
  for (int m = 0; m < MISS_LIMIT; m++) {
    GRect pip = GRect(lcd.origin.x + 6 + m * 9, lcd.origin.y + 23, 6, 6);
    if (m < s_misses) { graphics_context_set_fill_color(ctx, LCD_INK); graphics_fill_rect(ctx, pip, 1, GCornersAll); }
    else { graphics_context_set_stroke_color(ctx, LCD_INK); graphics_draw_rect(ctx, pip); }
  }

  int grid_top   = lcd.origin.y + 34;
  int catch_y    = lcd.origin.y + lcd.size.h - 22;
  int grid_h     = catch_y - grid_top;
  int lw         = lcd.size.w / LANES;
  int sh         = grid_h / STEPS;
  int gx         = lcd.origin.x + (lcd.size.w - lw * LANES) / 2;

  // falling eggs (chunky LCD blocks)
  graphics_context_set_fill_color(ctx, LCD_INK);
  for (int i = 0; i < MAXITEMS; i++) {
    if (!s_items[i].active) continue;
    int cx = gx + s_items[i].lane * lw + lw / 2;
    int cy = grid_top + s_items[i].step * sh + sh / 2;
    graphics_fill_circle(ctx, GPoint(cx, cy), 5);
  }

  // basket
  int bx = gx + s_catcher * lw + lw / 2;
  graphics_context_set_fill_color(ctx, LCD_INK);
  graphics_fill_rect(ctx, GRect(bx - 13, catch_y + 8, 26, 4), 1, GCornersAll); // base
  graphics_fill_rect(ctx, GRect(bx - 13, catch_y, 4, 10), 1, GCornersAll);     // left wall
  graphics_fill_rect(ctx, GRect(bx + 9,  catch_y, 4, 10), 1, GCornersAll);     // right wall

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
  step_world();
  layer_mark_dirty(window_get_root_layer(s_window));
  s_timer = app_timer_register(s_speed, tick, NULL);
}

static void up_click(ClickRecognizerRef r, void *c)   { if (!s_dead && s_catcher > 0) s_catcher--; layer_mark_dirty(window_get_root_layer(s_window)); }
static void down_click(ClickRecognizerRef r, void *c) { if (!s_dead && s_catcher < LANES - 1) s_catcher++; layer_mark_dirty(window_get_root_layer(s_window)); }
static void select_click(ClickRecognizerRef r, void *c) {
  if (s_dead) {
    reset_game();
    if (s_timer) app_timer_cancel(s_timer);
    s_timer = app_timer_register(s_speed, tick, NULL);
  }
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
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

void catch_push(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}
