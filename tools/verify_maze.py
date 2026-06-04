#!/usr/bin/env python3
"""Validate the Pebble-Man maze: rectangular, symmetric-ish, and fully
connected so every pellet is reachable from Pac-Man's start. Run before
embedding the maze in C."""
from collections import deque

# 15 cols x 17 rows. Legend:
#   # wall   . dot   o power pellet   space = open (no dot)
#   P pac start   - ghost-house door   G ghost start
MAZE = [
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
]

def check(maze):
    h = len(maze); w = len(maze[0])
    print(f"size {w}x{h}")
    ok = True
    for i, row in enumerate(maze):
        if len(row) != w:
            print(f"  ROW {i} len {len(row)} != {w}: {row!r}"); ok = False
    # find start
    start = None
    for r in range(h):
        for c in range(w):
            if maze[r][c] == 'P':
                start = (r, c)
    if not start:
        print("  no P start!"); return False
    walkable = set(".oPG- ")
    # flood fill
    seen = set([start]); q = deque([start])
    while q:
        r, c = q.popleft()
        for dr, dc in ((1,0),(-1,0),(0,1),(0,-1)):
            nr, nc = r+dr, c+dc
            if 0 <= nr < h and 0 <= nc < w and (nr,nc) not in seen and maze[nr][nc] in walkable:
                seen.add((nr,nc)); q.append((nr,nc))
    # every dot / power / ghost reachable?
    dots = power = unreached = 0
    for r in range(h):
        for c in range(w):
            ch = maze[r][c]
            if ch == '.': dots += 1
            if ch == 'o': power += 1
            if ch in ".oG-" and (r,c) not in seen:
                unreached += 1
                print(f"  UNREACHED {ch} at {r},{c}")
    print(f"  dots={dots} power={power} unreached={unreached}")
    return ok and unreached == 0

print("OK" if check(MAZE) else "FAILED")
