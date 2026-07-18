# Coding Rules — Bug_Storm

These rules keep the game deterministic and suitable for an STM32L151 with 16 KB RAM.

## 1. Runtime allocation

- Use fixed-size arrays for bullets, eggs and gifts.
- Do not allocate memory in the periodic game tick.
- Reuse inactive object slots by checking the `active` flag.
- Keep bitmap and constant lookup data read-only.

Current object pools:

```cpp
static projectile_t player_bullets[20];
static projectile_t eggs[5];
static projectile_t gifts[3];
static bool bugs[18];
```

## 2. Single owner of game state

All gameplay state belongs to `scr_game.cpp` and is changed only through `scr_game_handle()` or functions called from it.

Button callbacks do not edit the ship directly. They post signals to the display task:

```text
Hardware button → callback → AK message → display task → screen handler
```

This prevents interrupt-time gameplay updates and avoids state races.

## 3. Tick-based timing

Gameplay timing uses integer tick counters rather than blocking delays.

| Timing | Value |
|---|---:|
| Game tick | 100 ms |
| Automatic fire cooldown | 2 ticks = 200 ms |
| Player invulnerability | 12 ticks ≈ 1.2 s |
| Wave-clear pause | 12 ticks ≈ 1.2 s |
| Maximum screen render rate | 20 FPS |

Never call a blocking sleep from the game screen.

## 4. Update before render

The game follows this order:

1. Receive `AC_DISPLAY_GAME_TICK`.
2. Update counters.
3. Update the active enemy phase.
4. Move bullets, eggs and gifts.
5. Resolve collisions and state transitions.
6. Return to the screen manager.
7. Render the latest state.

Rendering functions must not change gameplay state.

## 5. Coordinate convention

- `(0, 0)` is the upper-left corner.
- X increases to the right.
- Y increases downward.
- Logical screen size is 124 × 60 px.
- HUD occupies Y `0..8`.
- Gameplay begins at Y `9`.
- The ship is positioned at Y `52`.

All collision boxes use the same coordinate system.

## 6. Collision convention

Use axis-aligned bounding boxes:

```cpp
return ax < bx + bw && ax + aw > bx &&
       ay < by + bh && ay + ah > by;
```

Collision processing order matters:

1. Player bullet versus Boss when Boss is active.
2. Player bullet versus bug during formation phase.
3. Gift versus player.
4. Egg versus player.

An inactive projectile must never participate in another collision during the same tick.

## 7. State transitions

Use explicit state variables rather than inferring every state from coordinates:

```text
game_over
boss_active
next_wave_ticks
invulnerable_ticks
bugs_left
boss_hp
```

State transitions must clear obsolete projectiles so objects from one phase cannot leak into the next phase.

## 8. Difficulty formulas

Keep difficulty formulas bounded:

```text
Formation move period = max(1, 4 - wave / 2)
Formation move step   = 1 + (wave - 1) / 3
Egg fall speed        = 2 + wave / 4
Egg minimum delay     = max(3, 7 - wave / 2)
Boss HP               = 10 + wave × 5
Boss move period      = max(1, 2 - wave / 4)
Boss move step        = 1 + wave / 4
```

Every divisor and array index must remain valid at high wave numbers.

## 9. Naming

- Constants: `UPPER_SNAKE_CASE`.
- Types: `lower_snake_case_t`.
- Functions and variables: `lower_snake_case`.
- Boolean variables should describe a true condition: `active`, `game_over`, `boss_active`.
- Signal names retain the AK framework naming convention.

## 10. Verification before flashing

- Compile with `-Wall` and resolve new warnings.
- Verify array bounds for all object pools.
- Verify the game can restart after Game Over.
- Verify holding MODE removes the periodic game timer.
- Verify Boss appears only after all bugs are destroyed.
- Verify the next wave begins only after Boss HP reaches zero.
- Verify `P:4` never creates more bullets than the pool capacity.


