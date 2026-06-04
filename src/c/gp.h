#pragma once
#include <pebble.h>

// ============================================================================
//  Game & Pebble  -  a pocketful of classic arcade games for the wrist.
//
//  gp.h is the shared "framework": the list of games, persistent high-score
//  helpers, and a handful of drawing helpers that every game reuses so the
//  whole collection looks and feels consistent.
// ============================================================================

// --- The roster -------------------------------------------------------------
// Keep this enum in sync with the menu table in main.c. The value doubles as
// the persistent-storage slot for that game's high score, so never reorder it
// without bumping the keys (it would shuffle saved scores around).
typedef enum {
  GAME_PEBBLEMAN = 0,
  GAME_SNAKE,
  GAME_ASTEROIDS,
  GAME_BLOCKS,
  GAME_BREAKOUT,
  GAME_CATCH,
  GAME_INVADERS,
  GAME_FLAPPY,
  GAME_PONG,
  GAME_FROG,
  GAME_COUNT
} GameId;

// --- High scores (persisted across launches) --------------------------------
int  gp_highscore_get(GameId id);
// Stores score if it beats the saved best. Returns true when a record was set.
bool gp_highscore_submit(GameId id, int score);

// --- Shared drawing helpers -------------------------------------------------
// One-liner text draw (no word wrap, single line).
void gp_text(GContext *ctx, const char *text, GFont font, GRect box, GTextAlignment align);

// A rounded "screen panel" with a fill and a 2px border. Used for overlays.
void gp_panel(GContext *ctx, GRect rect, GColor fill, GColor border);

// The shared game-over / paused overlay every game uses. Draws a centered
// panel with a title, the score and best, an optional "NEW BEST!" flash, and
// the standard control hints. Pass is_record=true to show the celebration.
void gp_overlay_gameover(GContext *ctx, GRect bounds, const char *title,
                         int score, int best, bool is_record, bool blink_on);

// A thin top status bar drawn in `accent`, with left/right labels in white.
// Returns the y just below the bar so callers can lay out the play area.
int gp_hud(GContext *ctx, GRect bounds, const char *left, const char *right, GColor accent);

// --- Common palette ---------------------------------------------------------
// These degrade gracefully to black/white on the non-colour watches.
#define GP_BG        GColorBlack
#define GP_INK       GColorWhite

// --- Game entry points ------------------------------------------------------
// Each game owns its own window; these just build and push it onto the stack.
void pebbleman_push(void);
void snake_push(void);
void asteroids_push(void);
void blocks_push(void);
void breakout_push(void);
void catch_push(void);
void invaders_push(void);
void flappy_push(void);
void pong_push(void);
void frog_push(void);
