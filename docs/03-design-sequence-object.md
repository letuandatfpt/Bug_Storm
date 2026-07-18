# Game Object Runtime Sequences

This document describes the lifecycle and interaction of each gameplay object.

## 1. Player Ship

### State

| Field | Meaning |
|---|---|
| `player_x` | Horizontal position. |
| `lives` | Remaining lives, initially 3. |
| `shot_level` | Number of automatic firing streams, from 1 to 4. |
| `fire_cooldown` | Ticks remaining until another volley can be created. |
| `invulnerable_ticks` | Damage protection and blink timer. |

### Input sequence

```mermaid
sequenceDiagram
    actor Player
    participant Button as Button Driver
    participant BSP as Button Callback
    participant Queue as AK Message Queue
    participant Display as Display Task
    participant Game as scr_game_handle

    Player->>Button: Press UP or DOWN
    Button->>BSP: BUTTON_SW_STATE_PRESSED
    BSP->>Queue: Post display button signal
    Queue->>Display: Dispatch message
    Display->>Game: AC_DISPLAY_BUTON_*_PRESSED
    alt UP pressed
        Game->>Game: player_x += SHIP_STEP
        Game->>Game: Clamp at right edge
    else DOWN pressed
        Game->>Game: player_x -= SHIP_STEP
        Game->>Game: Clamp at left edge
    end
```

### Damage sequence

```mermaid
sequenceDiagram
    participant Egg
    participant Collision
    participant Player
    participant Timer
    participant Game

    Egg->>Collision: Overlap ship bounding box
    Collision->>Player: Check invulnerable_ticks == 0
    alt Player can take damage
        Player->>Player: lives--
        Player->>Game: Clear bullets, eggs and gifts
        alt lives > 0
            Player->>Player: Center ship
            Player->>Timer: invulnerable_ticks = 12
        else lives == 0
            Player->>Game: game_over = true
            Game->>Timer: Remove periodic game tick
        end
    else Player is invulnerable
        Collision-->>Egg: Ignore ship collision
    end
```

## 2. Automatic Bullet

### Volley pattern

Offsets are relative to the center of the ship:

| Firepower | Bullet X offsets |
|---:|---|
| `P:1` | `0` |
| `P:2` | `-3, +3` |
| `P:3` | `-5, 0, +5` |
| `P:4` | `-6, -2, +2, +6` |

### Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Inactive
    Inactive --> Active: cooldown == 0 and free pool slot
    Active --> Moving: Spawn at ship muzzle
    Moving --> Moving: y -= 5 px each tick
    Moving --> Inactive: Leaves top of play area
    Moving --> Inactive: Hits bug
    Moving --> Inactive: Hits Boss
```

### Collision sequence

```mermaid
sequenceDiagram
    participant Tick as Game Tick
    participant Bullet
    participant Boss
    participant Bug
    participant Score

    Tick->>Bullet: Move upward 5 px
    alt Boss phase
        Bullet->>Boss: Test AABB collision
        alt Hit
            Boss->>Boss: boss_hp--
            Boss->>Score: Add 5 × wave
            Bullet->>Bullet: active = false
        end
    else Formation phase
        loop Active bugs
            Bullet->>Bug: Test AABB collision
            alt Hit
                Bug->>Bug: active = false
                Bug->>Score: Add 10 × wave
                Bullet->>Bullet: active = false
            end
        end
    end
```

## 3. Bug Formation

### Layout

```text
Rows             : 3
Columns          : 6
Bug size     : 10 × 7 px
Column step      : 16 px
Row step         : 10 px
Initial origin   : X=14, Y=11
```

### Movement sequence

```mermaid
flowchart TD
    A[Formation update] --> B{Move counter reached period?}
    B -- No --> Z[Return]
    B -- Yes --> C[Find live formation bounds]
    C --> D{Next step touches an edge?}
    D -- No --> E[Move horizontally]
    D -- Yes --> F[Reverse direction]
    F --> G[Move formation down 2 px]
    E --> H{Formation reached ship?}
    G --> H
    H -- No --> Z
    H -- Yes --> I[Player loses one life]
    I --> J{Lives remain?}
    J -- Yes --> K[Restart current formation]
    J -- No --> L[Game Over]
```

## 4. Egg

An egg is emitted by the lowest living bug in a selected column. During Boss phase, the egg origin is a random horizontal point under the Boss body.

```mermaid
stateDiagram-v2
    [*] --> Inactive
    Inactive --> Active: spawn counter reaches zero
    Active --> Falling: Choose bug or Boss origin
    Falling --> Falling: y += 2 + wave/4
    Falling --> Inactive: Leaves bottom edge
    Falling --> Inactive: Hits vulnerable player
```

Only five eggs may exist at once. If the pool has no free slot, the spawn attempt is skipped.

## 5. Gift

### Drop and collection

```mermaid
sequenceDiagram
    participant Bullet
    participant Bug
    participant RNG
    participant GiftPool as Gift Pool
    participant Gift
    participant Player

    Bullet->>Bug: Destroy bug
    Bug->>RNG: rand() % 4
    alt Result is zero (25%)
        RNG->>GiftPool: Request free slot
        alt Slot available
            GiftPool->>Gift: Spawn 5 × 5 px square
            loop Every game tick
                Gift->>Gift: y += 2 px
                Gift->>Player: Test collision
            end
            alt shot_level < 4
                Player->>Player: shot_level++
            else shot_level == 4
                Player->>Player: Add 50 bonus points
            end
        end
    end
```

Gift state is preserved only while it remains active in the current phase. Projectiles and gifts are cleared at formation/Boss phase transitions.

## 6. Boss

### State and scaling

| Property | Formula |
|---|---|
| Size | 30 × 18 px |
| Initial X | `(LCD_WIDTH - BOSS_WIDTH) / 2` |
| Initial Y | 15 px |
| Maximum HP | `10 + wave × 5` |
| Move period | `max(1, 2 - wave / 4)` ticks |
| Move step | `1 + wave / 4` px |
| Hit score | `5 × wave` |
| Defeat bonus | `100 × wave` |

### Boss lifecycle

```mermaid
stateDiagram-v2
    [*] --> Waiting
    Waiting --> Entering: bugs_left == 0
    Entering --> Fighting: Initialize HP and clear projectiles
    Fighting --> Fighting: Move, fire eggs, receive bullet hits
    Fighting --> Defeated: boss_hp <= 0
    Defeated --> WaveClear: Award score and clear projectiles
    WaveClear --> Waiting: Delay expires and next wave starts
```

### Boss fight sequence

```mermaid
sequenceDiagram
    participant Game
    participant Boss
    participant Bullet
    participant Egg
    participant HUD
    participant Wave

    Game->>Boss: game_start_boss()
    Boss->>Boss: HP = 10 + wave × 5
    Boss->>Game: Clear old projectiles and gifts
    loop Boss phase
        Game->>Boss: Update horizontal position
        Game->>Egg: Spawn from Boss underside
        Game->>Bullet: Update automatic bullets
        Bullet->>Boss: Collision
        Boss->>Boss: HP--
        Boss->>HUD: Render proportional health bar
    end
    alt HP reaches zero
        Boss->>Wave: Add defeat bonus
        Boss->>Game: boss_active = false
        Game->>Game: next_wave_ticks = 12
        Game->>Wave: Increment wave after delay
    end
```



