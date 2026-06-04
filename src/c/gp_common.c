#include "gp.h"

// ---- High scores -----------------------------------------------------------
// We offset the persist key so high-score slots never collide with any future
// settings stored at low keys.
#define HISCORE_KEY_BASE 100

int gp_highscore_get(GameId id) {
  uint32_t key = HISCORE_KEY_BASE + (uint32_t)id;
  return persist_exists(key) ? persist_read_int(key) : 0;
}

bool gp_highscore_submit(GameId id, int score) {
  if (score > gp_highscore_get(id)) {
    persist_write_int(HISCORE_KEY_BASE + (uint32_t)id, score);
    return true;
  }
  return false;
}

// ---- Drawing helpers -------------------------------------------------------
void gp_text(GContext *ctx, const char *text, GFont font, GRect box, GTextAlignment align) {
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeTrailingEllipsis, align, NULL);
}

void gp_panel(GContext *ctx, GRect rect, GColor fill, GColor border) {
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, rect, 6, GCornersAll);
  graphics_context_set_stroke_color(ctx, border);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, rect, 6);
  graphics_context_set_stroke_width(ctx, 1);
}

int gp_hud(GContext *ctx, GRect bounds, const char *left, const char *right, GColor accent) {
  const int bar_h = 22;
  GRect bar = GRect(0, 0, bounds.size.w, bar_h);
  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorBlack);
  GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  if (left) {
    gp_text(ctx, left, f, GRect(6, 1, bounds.size.w / 2, bar_h - 2), GTextAlignmentLeft);
  }
  if (right) {
    gp_text(ctx, right, f, GRect(bounds.size.w / 2 - 6, 1, bounds.size.w / 2, bar_h - 2),
            GTextAlignmentRight);
  }
  return bar_h;
}

void gp_overlay_gameover(GContext *ctx, GRect bounds, const char *title,
                         int score, int best, bool is_record, bool blink_on) {
  static char s_score[24];
  static char s_best[24];

  const int pw = bounds.size.w - 22;
  const int ph = 128;
  GRect panel = GRect((bounds.size.w - pw) / 2, (bounds.size.h - ph) / 2, pw, ph);
  gp_panel(ctx, panel, GColorBlack, GColorWhite);

  int x = panel.origin.x;
  int w = panel.size.w;
  int y = panel.origin.y + 8;

  graphics_context_set_text_color(ctx, GColorWhite);
  gp_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
          GRect(x, y, w, 32), GTextAlignmentCenter);
  y += 38;

  snprintf(s_score, sizeof(s_score), "SCORE  %d", score);
  gp_text(ctx, s_score, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
          GRect(x, y, w, 22), GTextAlignmentCenter);
  y += 24;

  if (is_record) {
    if (blink_on) {
      graphics_context_set_text_color(ctx, GColorWhite);
      gp_text(ctx, "★ NEW BEST! ★", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
              GRect(x, y, w, 22), GTextAlignmentCenter);
    }
  } else {
    snprintf(s_best, sizeof(s_best), "BEST  %d", best);
    gp_text(ctx, s_best, fonts_get_system_font(FONT_KEY_GOTHIC_18),
            GRect(x, y, w, 22), GTextAlignmentCenter);
  }
  y += 30;

  graphics_context_set_text_color(ctx, GColorWhite);
  gp_text(ctx, "SELECT → play again", fonts_get_system_font(FONT_KEY_GOTHIC_14),
          GRect(x, y, w, 18), GTextAlignmentCenter);
  y += 18;
  gp_text(ctx, "BACK → menu", fonts_get_system_font(FONT_KEY_GOTHIC_14),
          GRect(x, y, w, 18), GTextAlignmentCenter);
}
