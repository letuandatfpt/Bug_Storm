# Runtime Signal Processing and Game Loop

This document follows the complete runtime path from hardware input and software timers to state update and OLED rendering.

## 1. Main participants

| Participant | Responsibility |
|---|---|
| Hardware timer ISR | Calls button polling every 10 ms. |
| Button driver | Debounces buttons and identifies pressed/long-pressed states. |
| BSP callback | Converts button state into an AK display-task signal. |
| AK message queue | Transfers signals outside interrupt/polling context. |
| Display task | Sends messages to the active screen manager. |
| Screen manager | Dispatches the signal and rate-limits OLED rendering. |
| Game screen | Owns and updates all gameplay state. |
| OLED renderer | Draws the 1-bit frame and transfers it to the display. |

## 2. Game entry

```mermaid
sequenceDiagram
    actor Player
    participant Startup as Startup Screen
    participant Manager as Screen Manager
    participant Game as Game Screen
    participant Timer as AK Timer
    participant Buzzer
    participant OLED

    Player->>Startup: Press MODE on Bug_Storm
    Startup->>Manager: SCREEN_TRAN(scr_game)
    Manager->>Game: SCREEN_ENTRY
    Game->>Game: game_init()
    Game->>Game: Initialize lives=3, wave=1, P=1
    Game->>Game: Spawn 18 bugs
    Game->>Timer: Start 100 ms periodic GAME_TICK
    Game->>Buzzer: Play start sound
    Manager->>OLED: Render first frame
```

## 3. Button signal path

```mermaid
sequenceDiagram
    actor Player
    participant IRQ as 10 ms Timer ISR
    participant Driver as Button Driver
    participant Callback as BSP Callback
    participant Queue as AK Queue
    participant Display as Display Task
    participant Manager as Screen Manager
    participant Game as Game Screen
    participant OLED

    Player->>Driver: Physical press
    IRQ->>Driver: button_timer_polling()
    Driver->>Driver: Debounce and classify state
    Driver->>Callback: Invoke registered callback
    Callback->>Queue: task_post_pure_msg(DISPLAY, signal)
    Queue->>Display: Deliver message
    Display->>Manager: scr_mng_dispatch(msg)
    Manager->>Game: scr_game_handle(msg)
    Game->>Game: Change player position or screen state
    Manager->>Manager: Check 50 ms render limit
    Manager->>OLED: Render updated frame
```

### Button-to-signal mapping

| Physical event | Signal | Game action |
|---|---|---|
| UP pressed | `AC_DISPLAY_BUTON_UP_PRESSED` | Move right by 5 px and clamp. |
| DOWN pressed | `AC_DISPLAY_BUTON_DOWN_PRESSED` | Move left by 5 px and clamp. |
| MODE pressed | `AC_DISPLAY_BUTON_MODE_PRESSED` | Restart only when Game Over; fire is automatic. |
| MODE held | `AC_DISPLAY_BUTON_MODE_LONG_PRESSED` | Stop game timer and return to startup menu. |

## 4. Periodic game tick

```mermaid
sequenceDiagram
    participant Timer as AK Timer
    participant Queue as AK Queue
    participant Display as Display Task
    participant Manager as Screen Manager
    participant Game as Game Screen
    participant Enemy as Formation or Boss
    participant Objects as Bullets / Eggs / Gifts
    participant Collision
    participant OLED

    loop Every 100 ms while game is active
        Timer->>Queue: AC_DISPLAY_GAME_TICK
        Queue->>Display: Deliver timer signal
        Display->>Manager: Dispatch
        Manager->>Game: scr_game_handle(GAME_TICK)
        Game->>Game: Update animation and cooldown counters
        Game->>Game: Create automatic bullet volley if ready
        alt Wave-clear delay active
            Game->>Game: Decrement next_wave_ticks
            opt Delay reaches zero
                Game->>Game: wave++ and game_start_wave()
            end
        else Boss active
            Game->>Enemy: game_update_boss()
        else Formation active
            Game->>Enemy: game_update_formation()
        end
        Game->>Objects: Move active projectiles and gifts
        Objects->>Collision: Resolve AABB collisions
        Collision-->>Game: Update HP, lives, score and active flags
        Game->>Objects: Attempt enemy egg spawn
        Game->>Game: Evaluate phase transition
        Manager->>OLED: Render latest state
    end
```

## 5. Complete match state machine

```mermaid
stateDiagram-v2
    [*] --> Initializing: SCREEN_ENTRY
    Initializing --> Formation: Start Wave 1

    state Formation {
        [*] --> Moving
        Moving --> Moving: Bugs remain
        Moving --> PlayerHit: Egg hit or invasion
        PlayerHit --> Moving: Lives remain
        PlayerHit --> GameOver: Lives == 0
        Moving --> FormationClear: bugs_left == 0
    }

    FormationClear --> BossIntro: Initialize Boss

    state BossFight {
        [*] --> BossMoving
        BossMoving --> BossMoving: boss_hp > 0
        BossMoving --> PlayerHitBoss: Egg hits player
        PlayerHitBoss --> BossMoving: Lives remain
        PlayerHitBoss --> GameOver: Lives == 0
        BossMoving --> BossDefeated: boss_hp <= 0
    }

    BossIntro --> BossFight
    BossDefeated --> WaveClear
    WaveClear --> Formation: Delay expires; wave++
    GameOver --> Initializing: Short button press
    Formation --> Startup: Hold MODE
    BossFight --> Startup: Hold MODE
    GameOver --> Startup: Hold MODE
```

## 6. Formation-to-Boss transition

```mermaid
sequenceDiagram
    participant Bullet
    participant Bug
    participant Game
    participant Pool as Object Pools
    participant Boss
    participant Buzzer
    participant OLED

    Bullet->>Bug: Hit final active bug
    Bug->>Game: bugs_left becomes 0
    Game->>Game: Detect formation complete
    Game->>Boss: game_start_boss()
    Boss->>Boss: Set position, direction and scaled HP
    Game->>Pool: Clear bullets, eggs and gifts
    Game->>Buzzer: Play Boss start sound
    Game->>OLED: Render Boss sprite and HP bar
```

## 7. Boss-to-next-wave transition

```mermaid
sequenceDiagram
    participant Bullet
    participant Boss
    participant Game
    participant Timer
    participant Score
    participant OLED

    Bullet->>Boss: Final hit
    Boss->>Boss: boss_hp becomes 0
    Boss->>Score: Add hit score + defeat bonus
    Boss->>Game: boss_active = false
    Game->>Game: Clear object pools
    Game->>Timer: next_wave_ticks = 12
    Game->>OLED: Display WAVE CLEAR
    loop 12 game ticks
        Timer->>Game: AC_DISPLAY_GAME_TICK
        Game->>Game: next_wave_ticks--
    end
    Game->>Game: wave++
    Game->>Game: game_start_wave()
    Game->>OLED: Render new bug formation
```

## 8. Render scheduling

The game tick is 100 ms, while the screen manager allows rendering every 50 ms. Button events may therefore produce an intermediate render without changing the simulation tick rate.

```mermaid
flowchart TD
    A[Signal dispatched] --> B[Active screen handles signal]
    B --> C{First render or at least 50 ms elapsed?}
    C -- Yes --> D[Clear frame buffer]
    D --> E[Render dynamic screen item]
    E --> F[OLED update]
    C -- No --> G[Schedule one-shot render signal]
    G --> H[Render when remaining interval expires]
```

## 9. Runtime invariants

- At most one enemy phase is active: formation or Boss.
- `boss_active` is false at the beginning of every normal wave.
- A new wave cannot start until Boss HP is zero and the clear delay expires.
- A projectile with `active == false` is ignored by update, collision and render code.
- `shot_level` remains between 1 and 4.
- Player X is always clamped to the screen bounds.
- The periodic game timer is removed on Game Over and when leaving the game screen.


