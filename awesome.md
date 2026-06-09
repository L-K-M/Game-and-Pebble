# Game & Pebble — code review notes

A thorough read-through of `src/c`, the tools, and the CI workflow. Organized
as: **bugs**, **general issues**, **missing features**, and **ideas** (novel /
delightful / quirky). Items marked **[implementing]** are being fixed in
follow-up PRs; the rest are written up here for future consideration.

---

## Bugs

### 1. Menu rows almost never show the high score **[implementing]**
`main.c` builds the selected row's subtitle into `char hi[20]`:

```c
snprintf(hi, sizeof(hi), "%s  ·  HI %d", s_games[i].tagline, gp_highscore_get(...));
```

`"blast the rocks"` (15 bytes) + `"  ·  HI "` (8 bytes — the `·` is 2 bytes of
UTF-8) already exceeds the buffer before the number is even printed. Almost
every tagline gets truncated and the `HI <score>` part is silently cut off.
The buffer needs to be ~40 bytes.

### 2. Breakout: phantom brick collisions above the brick wall **[implementing]**
`step()` computes the brick row as:

```c
int row = (int)((s_by - s_btop) / s_bh);
```

C casts truncate **toward zero**, so any ball position up to 10 px *above*
`s_btop` (the band between the HUD and the bricks, since the ball bounces at
`s_fy + BALL_R` but bricks start at `s_fy + 8`) computes `row == 0` and
registers a hit on a top-row brick the ball never touched. Needs a
`s_by >= s_btop` guard.

### 3. Invaders: shields erode from outside their own bounding box **[implementing]**
Same truncation-toward-zero class of bug in `shield_hit()`:

```c
int lc = (x - s_shield_x[i]) / s_scell;   // x 1..5 px LEFT of the shield → lc == 0
int lr = (y - s_shield_y) / s_scell;      // y above the shield → lr == 0
```

A bullet or bomb passing just left of / above a shield eats its top-left
cell. Guard for `x >= s_shield_x[i]` and `y >= s_shield_y` before dividing.

### 4. Black-and-white watches (diorite, flint): several games are unplayable **[implementing]**
The app targets the mono platforms in `package.json`, but on a 1-bit display
each `GColor` collapses to plain black or white by luminance, and several
key colours collapse to **black on a black background**:

- **Pebble-Man**: maze walls are `GColorBlue` → invisible maze. Blinky is
  `GColorRed` → invisible ghost. Frightened ghosts turn `GColorBlue` →
  invisible.
- **Snake**: the apple is `GColorRed` → invisible food.
- **Invaders**: bombs are stroked `GColorRed` → invisible death from above.
- **Blocks**: the Z (red), J (blue) and T (purple) pieces go invisible.
- **Frog**: red/orange cars go invisible against the dark-gray road.
- **Menu**: some selected-row accent fills map to black, leaving black text
  on a black row.

The SDK's `COLOR_FALLBACK(color, bw)` macro exists exactly for this; every
game needs a pass choosing legible 1-bit fallbacks (white sprites, dithered
gray via `GColorLightGray`/`GColorDarkGray` fills for walls/roads).

### 5. Games keep playing underneath notifications **[implementing]**
When a notification or alarm takes over the screen, the app loses focus but
its `AppTimer`s keep firing — you dismiss the notification and find yourself
dead. The SDK's `app_focus_service` reports focus loss; each game's tick loop
should idle (without stepping the world) while unfocused.

### 6. Asteroids: exhaust flame "flicker" is keyed to the wrong counter
The thrust flame draws when `(s_inv / 2) % 2 == 0`, but `s_inv` is the
*invulnerability countdown* — it only changes during the ~2 s after a respawn
and is 0 the rest of the time. So the flame flickers briefly after spawning
and is rock-solid afterwards. Cosmetic; needs a real animation counter.

### 7. Pong: "SELECT to serve" is a little white lie
`serve()` sets `s_serve_delay = 22` and auto-launches ~0.7 s later whether or
not you press SELECT. Either honour the prompt (wait for SELECT) or change
the text to "GET READY".

## General issues

- **Window lifecycle**: each game destroys its previous `Window` only on the
  *next* `*_push()` of the same game. After visiting all ten games, ten dead
  windows stay allocated until app exit. Bounded, but watch RAM is tiny;
  destroying in the window's `unload` handler would return it eagerly.
- **Round watches (chalk)**: every layout is rectangular; on the 180×180
  circular display HUD text at the corners and the outer menu-row edges get
  clipped. Playable, but a `PBL_IF_ROUND_ELSE` inset pass would polish it.
- **Snake**: if `place_food()` exhausts its 500 random tries (huge snake),
  the food can quietly remain underneath the body; and filling the board is
  a *win* with no win screen — it just stops spawning food.
- `GP_BG` / `GP_INK` in `gp.h` are defined but never used.
- CI runs on both `push` *and* `pull_request`, so PR branches build twice.
- Pebble-Man's eaten-ghost combo (`200 << shift`) intentionally persists for
  the whole fright window and resets per power pellet — matches the arcade,
  noting it here so nobody "fixes" it.

## Missing features

- **Auto-pause on focus loss** (see bug 5) — the most player-visible one.
- **Remember the last-played game**: the menu always reopens on Pebble-Man;
  persisting `s_sel` (one `persist_write_int`) makes relaunch friendlier.
  **[implementing]**
- **Blocks**: no hard drop (the soft drop is a long-press; a double-tap
  gesture could hard-drop), no lock-delay slide, no points for soft drops.
- **Invaders**: no bonus UFO saucer across the top; the player bullet can't
  intercept bombs (both classic touches).
- **Asteroids**: no hyperspace escape, no extra life every 10 000 points.
- **Frog**: the goal dots on the top row are purely decorative — classic
  Frogger fills five home slots per level; also no per-crossing timer.
- **Pebble-Man**: the maze has no wrap-around side tunnels, and no bonus
  fruit appears mid-level.

## Ideas — novel, cool, delightful, quirky

- **Daily-challenge seed**: seed `srand()` from today's date (a quirky twist
  on the watch being… a watch). Everyone in the world gets the same Blocks
  bag order, asteroid field, and pipe gaps for a day; compare scores fairly.
- **High-score initials**: a three-letter AAA-style initials entry on a new
  record, picked with UP/DOWN — pure arcade nostalgia, and it fits the two
  buttons + select scheme perfectly.
- **"INSERT COIN" attract mode**: leave the menu idle for 30 s and the banner
  Pebble-Man chase becomes a tiny self-playing demo of a random game.
- **Banner drama**: once in a while the menu banner's trailing ghost should
  turn blue and the chase should reverse — a two-line delight.
- **Wrist-flick to restart**: the accelerometer tap service could let a
  frustrated flick of the wrist restart after game over (configurable).
- **Backlight pop on events**: a `light_enable_interaction()` when you level
  up / lose a life, so night-time play flashes at the dramatic moments.
- **Konami code on the menu** (UP UP DOWN DOWN UP DOWN UP DOWN SELECT): show
  total play stats, or unlock a "turbo" mode with 2× game speed.
- **Per-game medals on the menu**: bronze/silver/gold thresholds per game so
  the roster rows show a little cup glyph — gives long-term goals beyond a
  single number.

---

*Reviewed at commit `e5927cb`. The truncation, collision, mono-display,
focus-pause and menu-persistence items are being implemented in separate PRs
(kept to disjoint hunks so they merge cleanly in any order).*
