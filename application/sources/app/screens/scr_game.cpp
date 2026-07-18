#include "scr_game.h"

#include "button.h"
#include "app_bsp.h"

#include <stdlib.h>

#define GAME_TICK_INTERVAL_MS       (100)
#define PLAY_TOP                    (9)
#define SHIP_Y                      (52)
#define SHIP_WIDTH                  (11)
#define SHIP_HEIGHT                 (7)
#define SHIP_STEP                   (5)
#define BUG_COLS                (6)
#define BUG_ROWS                (3)
#define BUG_COUNT               (BUG_COLS * BUG_ROWS)
#define BUG_WIDTH               (10)
#define BUG_HEIGHT              (7)
#define BUG_COL_STEP            (16)
#define BUG_ROW_STEP            (10)
#define MAX_PLAYER_BULLETS          (20)
#define MAX_EGGS                    (5)
#define MAX_GIFTS                   (3)
#define GIFT_SIZE                   (5)
#define MAX_SHOT_LEVEL              (4)
#define BOSS_WIDTH                  (30)
#define BOSS_HEIGHT                 (18)
#define BOSS_START_Y                (15)
#define BOSS_HP_BAR_WIDTH           (50)
#define PLAYER_BULLET_SPEED         (5)
#define PLAYER_FIRE_COOLDOWN        (2)
#define PLAYER_INVULNERABLE_TICKS   (12)
#define NEXT_WAVE_DELAY_TICKS       (12)

struct projectile_t {
	bool active;
	int x;
	int y;
};

static void view_scr_game();
static void game_init();
static void game_start_wave();
static void game_start_boss();
static void game_update();
static void game_update_boss();
static void game_update_formation();
static void game_update_projectiles();
static void game_spawn_egg();
static void game_spawn_gift(int x, int y);
static void game_fire();
static void game_player_hit();
static bool boxes_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh);
static int bug_x(int index);
static int bug_y(int index);
static void draw_bug(int x, int y, bool wings_up);
static void draw_boss(int x, int y, bool wings_up);
static void draw_ship(int x, int y);

static bool bugs[BUG_COUNT];
static projectile_t player_bullets[MAX_PLAYER_BULLETS];
static projectile_t eggs[MAX_EGGS];
static projectile_t gifts[MAX_GIFTS];

static int player_x;
static int score;
static int high_score;
static int wave;
static int lives;
static int shot_level;
static int bugs_left;
static int formation_x;
static int formation_y;
static int formation_direction;
static int formation_move_counter;
static int boss_x;
static int boss_y;
static int boss_direction;
static int boss_move_counter;
static int boss_hp;
static int boss_max_hp;
static int egg_spawn_counter;
static int fire_cooldown;
static int invulnerable_ticks;
static int next_wave_ticks;
static int animation_frame;
static bool boss_active;
static bool game_over;

view_dynamic_t dyn_view_game = {
	{
		.item_type = ITEM_TYPE_DYNAMIC,
	},
	view_scr_game
};

view_screen_t scr_game = {
	&dyn_view_game,
	ITEM_NULL,
	ITEM_NULL,

	.focus_item = 0,
};

static void clear_projectiles() {
	for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
		player_bullets[i].active = false;
	}
	
	for (int i = 0; i < MAX_EGGS; ++i) {
		eggs[i].active = false;
	}
	for (int i = 0; i < MAX_GIFTS; ++i) {
		gifts[i].active = false;
	}
}

static void game_init() {
	player_x = (LCD_WIDTH - SHIP_WIDTH) / 2;
	score = 0;
	wave = 1;
	lives = 3;
	shot_level = 1;
	fire_cooldown = 0;
	invulnerable_ticks = 0;
	next_wave_ticks = 0;
	animation_frame = 0;
	game_over = false;
	clear_projectiles();
	game_start_wave();
}

static void game_start_wave() {
	boss_active = false;
	for (int i = 0; i < BUG_COUNT; ++i) {
		bugs[i] = true;
	}

	bugs_left = BUG_COUNT;
	formation_x = 14;
	formation_y = PLAY_TOP + 2;
	formation_direction = 1;
	formation_move_counter = 0;
	egg_spawn_counter = 16;
	next_wave_ticks = 0;
	clear_projectiles();
}

static void game_start_boss() {
	boss_active = true;
	boss_max_hp = 10 + wave * 5;
	boss_hp = boss_max_hp;
	boss_x = (LCD_WIDTH - BOSS_WIDTH) / 2;
	boss_y = BOSS_START_Y;
	boss_direction = 1;
	boss_move_counter = 0;
	egg_spawn_counter = 5;
	clear_projectiles();
	BUZZER_PlaySound(BUZZER_SOUND_LETS_GO);
}

static void game_update_boss() {
	int move_period = 2 - wave / 4;
	if (move_period < 1) move_period = 1;
	if (++boss_move_counter < move_period) return;
	boss_move_counter = 0;

	int step = 1 + wave / 4;
	boss_x += boss_direction * step;
	if (boss_x <= 0) {
		boss_x = 0;
		boss_direction = 1;
	}
	else if (boss_x >= LCD_WIDTH - BOSS_WIDTH) {
		boss_x = LCD_WIDTH - BOSS_WIDTH;
		boss_direction = -1;
	}
}

static void game_update() {
	++animation_frame;

	if (fire_cooldown > 0) {
		--fire_cooldown;
	}
	if (fire_cooldown == 0 && next_wave_ticks == 0) {
		game_fire();
	}
	if (invulnerable_ticks > 0) {
		--invulnerable_ticks;
	}

	if (next_wave_ticks > 0) {
		--next_wave_ticks;
		if (next_wave_ticks == 0) {
			++wave;
			game_start_wave();
			BUZZER_PlaySound(BUZZER_SOUND_LETS_GO);
		}
		return;
	}

	if (boss_active) game_update_boss();
	else game_update_formation();
	game_update_projectiles();
	if (next_wave_ticks > 0) return;

	if (--egg_spawn_counter <= 0) {
		game_spawn_egg();
		int minimum_delay = 7 - wave / 2;
		if (minimum_delay < 3) {
			minimum_delay = 3;
		}
		egg_spawn_counter = minimum_delay + (rand() % 8);
	}

	if (!boss_active && bugs_left == 0) {
		game_start_boss();
	}
}

static void game_update_formation() {
	int move_period = 4 - wave / 2;
	if (move_period < 1) {
		move_period = 1;
	}
	if (++formation_move_counter < move_period) {
		return;
	}
	formation_move_counter = 0;

	int left = LCD_WIDTH;
	int right = 0;
	int bottom = 0;
	for (int i = 0; i < BUG_COUNT; ++i) {
		if (!bugs[i]) {
			continue;
		}
		int x = bug_x(i);
		int y = bug_y(i);
		if (x < left) left = x;
		if (x + BUG_WIDTH > right) right = x + BUG_WIDTH;
		if (y + BUG_HEIGHT > bottom) bottom = y + BUG_HEIGHT;
	}

	int step = 1 + (wave - 1) / 3;
	if ((formation_direction > 0 && right + step >= LCD_WIDTH) ||
		(formation_direction < 0 && left - step <= 0)) {
		formation_direction = -formation_direction;
		formation_y += 2;
	}
	else {
		formation_x += formation_direction * step;
	}

	if (bottom >= SHIP_Y - 1) {
		game_player_hit();
		if (!game_over) {
			game_start_wave();
		}
	}
}

static void game_update_projectiles() {
	for (int b = 0; b < MAX_PLAYER_BULLETS; ++b) {
		if (!player_bullets[b].active) {
			continue;
		}

		player_bullets[b].y -= PLAYER_BULLET_SPEED;
		if (player_bullets[b].y < PLAY_TOP) {
			player_bullets[b].active = false;
			continue;
		}

		if (boss_active && boxes_overlap(player_bullets[b].x, player_bullets[b].y, 2, 4,
				boss_x, boss_y, BOSS_WIDTH, BOSS_HEIGHT)) {
			player_bullets[b].active = false;
			--boss_hp;
			score += 5 * wave;
			BUZZER_PlaySound(BUZZER_SOUND_TONE_2);
			if (boss_hp <= 0) {
				boss_active = false;
				score += 100 * wave;
				if (score > high_score) high_score = score;
				next_wave_ticks = NEXT_WAVE_DELAY_TICKS;
				clear_projectiles();
				BUZZER_PlaySound(BUZZER_SOUND_HIGHSCORE);
			}
			continue;
		}

		for (int i = 0; i < BUG_COUNT; ++i) {
			if (!bugs[i]) {
				continue;
			}
			if (boxes_overlap(player_bullets[b].x, player_bullets[b].y, 2, 4,
					bug_x(i), bug_y(i), BUG_WIDTH, BUG_HEIGHT)) {
				int defeated_x = bug_x(i);
				int defeated_y = bug_y(i);
				bugs[i] = false;
				player_bullets[b].active = false;
				--bugs_left;
				score += 10 * wave;
				if ((rand() % 4) == 0) game_spawn_gift(defeated_x + 3, defeated_y);
				if (score > high_score) high_score = score;
				BUZZER_PlaySound(BUZZER_SOUND_TONE_2);
				break;
			}
		}
	}

	for (int g = 0; g < MAX_GIFTS; ++g) {
		if (!gifts[g].active) continue;
		gifts[g].y += 2;
		if (gifts[g].y >= LCD_HEIGHT) {
			gifts[g].active = false;
			continue;
		}
		if (boxes_overlap(gifts[g].x, gifts[g].y, GIFT_SIZE, GIFT_SIZE,
				player_x, SHIP_Y, SHIP_WIDTH, SHIP_HEIGHT)) {
			gifts[g].active = false;
			if (shot_level < MAX_SHOT_LEVEL) ++shot_level;
			else score += 50;
			BUZZER_PlaySound(BUZZER_SOUND_TONE_4);
		}
	}

	for (int e = 0; e < MAX_EGGS; ++e) {
		if (!eggs[e].active) {
			continue;
		}

		eggs[e].y += 2 + wave / 4;
		if (eggs[e].y >= LCD_HEIGHT) {
			eggs[e].active = false;
			continue;
		}

		if (invulnerable_ticks == 0 && boxes_overlap(eggs[e].x - 1, eggs[e].y - 1, 3, 4,
				player_x, SHIP_Y, SHIP_WIDTH, SHIP_HEIGHT)) {
			eggs[e].active = false;
			game_player_hit();
		}
	}
}

static void game_spawn_egg() {
	int slot = -1;
	for (int i = 0; i < MAX_EGGS; ++i) {
		if (!eggs[i].active) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		return;
	}

	if (boss_active) {
		eggs[slot].active = true;
		eggs[slot].x = boss_x + 3 + (rand() % (BOSS_WIDTH - 6));
		eggs[slot].y = boss_y + BOSS_HEIGHT;
		return;
	}
	if (bugs_left == 0) return;

	int start_col = rand() % BUG_COLS;
	for (int offset = 0; offset < BUG_COLS; ++offset) {
		int col = (start_col + offset) % BUG_COLS;
		for (int row = BUG_ROWS - 1; row >= 0; --row) {
			int index = row * BUG_COLS + col;
			if (bugs[index]) {
				eggs[slot].active = true;
				eggs[slot].x = bug_x(index) + BUG_WIDTH / 2;
				eggs[slot].y = bug_y(index) + BUG_HEIGHT;
				return;
			}
		}
	}
}

static void game_spawn_gift(int x, int y) {
	for (int i = 0; i < MAX_GIFTS; ++i) {
		if (!gifts[i].active) {
			gifts[i].active = true;
			gifts[i].x = x;
			gifts[i].y = y;
			if (gifts[i].x > LCD_WIDTH - GIFT_SIZE) gifts[i].x = LCD_WIDTH - GIFT_SIZE;
			return;
		}
	}
}

static void game_fire() {
	if (fire_cooldown > 0 || next_wave_ticks > 0) return;

	static const int shot_offsets[MAX_SHOT_LEVEL][MAX_SHOT_LEVEL] = {
		{0, 0, 0, 0},
		{-3, 3, 0, 0},
		{-5, 0, 5, 0},
		{-6, -2, 2, 6}
	};
	int spawned = 0;
	for (int shot = 0; shot < shot_level; ++shot) {
		int bullet_x = player_x + SHIP_WIDTH / 2 + shot_offsets[shot_level - 1][shot];
		if (bullet_x < 0 || bullet_x > LCD_WIDTH - 2) continue;
		for (int slot = 0; slot < MAX_PLAYER_BULLETS; ++slot) {
			if (!player_bullets[slot].active) {
				player_bullets[slot].active = true;
				player_bullets[slot].x = bullet_x;
				player_bullets[slot].y = SHIP_Y - 4;
				++spawned;
				break;
			}
		}
	}
	if (spawned > 0) fire_cooldown = PLAYER_FIRE_COOLDOWN;
}

static void game_player_hit() {
	if (invulnerable_ticks > 0 || game_over) {
		return;
	}

	--lives;
	BUZZER_PlaySound(BUZZER_SOUND_BANG);
	clear_projectiles();
	if (lives <= 0) {
		game_over = true;
		if (score > high_score) high_score = score;
		timer_remove_attr(AC_TASK_DISPLAY_ID, AC_DISPLAY_GAME_TICK);
	}
	else {
		player_x = (LCD_WIDTH - SHIP_WIDTH) / 2;
		invulnerable_ticks = PLAYER_INVULNERABLE_TICKS;
	}
}

static bool boxes_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
	return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static int bug_x(int index) {
	return formation_x + (index % BUG_COLS) * BUG_COL_STEP;
}

static int bug_y(int index) {
	return formation_y + (index / BUG_COLS) * BUG_ROW_STEP;
}

static void draw_bug(int x, int y, bool wings_up) {
	view_render.fillRect(x + 3, y + 1, 4, 5, WHITE);
	view_render.drawPixel(x + 2, y, WHITE);
	view_render.drawPixel(x + 7, y, WHITE);
	view_render.drawPixel(x + 4, y + 2, BLACK);
	view_render.drawPixel(x + 6, y + 2, BLACK);
	if (wings_up) {
		view_render.drawLine(x + 3, y + 2, x, y, WHITE);
		view_render.drawLine(x + 6, y + 2, x + 9, y, WHITE);
	}
	else {
		view_render.drawLine(x + 3, y + 3, x, y + 5, WHITE);
		view_render.drawLine(x + 6, y + 3, x + 9, y + 5, WHITE);
	}
	view_render.drawPixel(x + 2, y + 6, WHITE);
	view_render.drawPixel(x + 4, y + 6, WHITE);
	view_render.drawPixel(x + 6, y + 6, WHITE);
	view_render.drawPixel(x + 8, y + 6, WHITE);
}

static void draw_boss(int x, int y, bool wings_up) {
	view_render.fillRoundRect(x + 5, y + 3, 20, 11, 4, WHITE);
	view_render.drawPixel(x + 10, y + 6, BLACK);
	view_render.drawPixel(x + 19, y + 6, BLACK);
	view_render.drawLine(x + 12, y + 1, x + 14, y + 3, WHITE);
	view_render.drawLine(x + 16, y, x + 17, y + 3, WHITE);
	view_render.drawLine(x + 24, y + 7, x + 29, y + 9, WHITE);
	if (wings_up) {
		view_render.drawLine(x + 5, y + 5, x, y + 1, WHITE);
		view_render.drawLine(x + 24, y + 5, x + 28, y + 1, WHITE);
	}
	else {
		view_render.drawLine(x + 5, y + 8, x, y + 13, WHITE);
		view_render.drawLine(x + 24, y + 8, x + 28, y + 13, WHITE);
	}
	view_render.drawLine(x + 10, y + 14, x + 8, y + 17, WHITE);
	view_render.drawLine(x + 19, y + 14, x + 21, y + 17, WHITE);
}

static void draw_ship(int x, int y) {
	view_render.drawLine(x + 5, y, x, y + 6, WHITE);
	view_render.drawLine(x + 5, y, x + 10, y + 6, WHITE);
	view_render.drawLine(x, y + 6, x + 10, y + 6, WHITE);
	view_render.fillRect(x + 4, y + 3, 3, 4, WHITE);
}

static void view_scr_game() {
	view_render.setTextSize(1);
	view_render.setTextColor(WHITE);
	view_render.setCursor(0, 0);
	view_render.print("S:");
	view_render.print(score);
	view_render.setCursor(51, 0);
	view_render.print("W:");
	view_render.print(wave);
	view_render.setCursor(73, 0);
	view_render.print("P:");
	view_render.print(shot_level);
	view_render.setCursor(98, 0);
	view_render.print("V:");
	view_render.print(lives);
	view_render.drawLine(0, 8, LCD_WIDTH - 1, 8, WHITE);

	static const int stars[][2] = {{5, 14}, {22, 35}, {37, 18}, {58, 44}, {75, 13}, {91, 38}, {112, 22}, {119, 47}};
	for (unsigned int i = 0; i < sizeof(stars) / sizeof(stars[0]); ++i) {
		if (((animation_frame / 4) + i) % 3 != 0) {
			view_render.drawPixel(stars[i][0], stars[i][1], WHITE);
		}
	}

	if (boss_active) {
		int hp_width = (BOSS_HP_BAR_WIDTH - 2) * boss_hp / boss_max_hp;
		view_render.drawRect((LCD_WIDTH - BOSS_HP_BAR_WIDTH) / 2, 10, BOSS_HP_BAR_WIDTH, 4, WHITE);
		view_render.fillRect((LCD_WIDTH - BOSS_HP_BAR_WIDTH) / 2 + 1, 11, hp_width, 2, WHITE);
		draw_boss(boss_x, boss_y, (animation_frame / 2) % 2 == 0);
	}
	for (int i = 0; i < BUG_COUNT; ++i) {
		if (bugs[i]) {
			draw_bug(bug_x(i), bug_y(i), ((animation_frame / 3) + i) % 2 == 0);
		}
	}

	for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
		if (player_bullets[i].active) {
			view_render.fillRect(player_bullets[i].x, player_bullets[i].y, 2, 4, WHITE);
		}
	}
	for (int i = 0; i < MAX_GIFTS; ++i) {
		if (gifts[i].active) {
			view_render.drawRect(gifts[i].x, gifts[i].y, GIFT_SIZE, GIFT_SIZE, WHITE);
			view_render.drawPixel(gifts[i].x + 2, gifts[i].y + 2, WHITE);
		}
	}
	for (int i = 0; i < MAX_EGGS; ++i) {
		if (eggs[i].active) {
			view_render.drawCircle(eggs[i].x, eggs[i].y, 1, WHITE);
			view_render.drawPixel(eggs[i].x, eggs[i].y + 2, WHITE);
		}
	}

	if (invulnerable_ticks == 0 || (invulnerable_ticks % 2) == 0) {
		draw_ship(player_x, SHIP_Y);
	}

	if (next_wave_ticks > 0) {
		view_render.fillRect(22, 24, 80, 17, BLACK);
		view_render.drawRoundRect(22, 24, 80, 17, 3, WHITE);
		view_render.setCursor(35, 30);
		view_render.print("WAVE CLEAR!");
	}

	if (game_over) {
		view_render.fillRect(12, 17, 100, 32, BLACK);
		view_render.drawRoundRect(12, 17, 100, 32, 3, WHITE);
		view_render.setCursor(34, 22);
		view_render.print("GAME OVER");
		view_render.setCursor(29, 32);
		view_render.print("BEST:");
		view_render.print(high_score);
		view_render.setCursor(20, 41);
		view_render.print("PRESS TO RESTART");
	}
}

void scr_game_handle(ak_msg_t* msg) {
	switch (msg->sig) {
	case SCREEN_ENTRY: {
		game_init();
		timer_remove_attr(AC_TASK_DISPLAY_ID, AC_DISPLAY_SHOW_IDLE);
		timer_set(AC_TASK_DISPLAY_ID, AC_DISPLAY_GAME_TICK, GAME_TICK_INTERVAL_MS, TIMER_PERIODIC);
		BUZZER_PlaySound(BUZZER_SOUND_LETS_GO);
	} break;

	case AC_DISPLAY_GAME_TICK: {
		if (!game_over) game_update();
	} break;

	case AC_DISPLAY_BUTON_UP_PRESSED: {
		if (game_over) {
			game_init();
			timer_set(AC_TASK_DISPLAY_ID, AC_DISPLAY_GAME_TICK, GAME_TICK_INTERVAL_MS, TIMER_PERIODIC);
		}
		else {
			player_x += SHIP_STEP;
			if (player_x > LCD_WIDTH - SHIP_WIDTH) player_x = LCD_WIDTH - SHIP_WIDTH;
		}
	} break;

	case AC_DISPLAY_BUTON_DOWN_PRESSED: {
		if (game_over) {
			game_init();
			timer_set(AC_TASK_DISPLAY_ID, AC_DISPLAY_GAME_TICK, GAME_TICK_INTERVAL_MS, TIMER_PERIODIC);
		}
		else {
			player_x -= SHIP_STEP;
			if (player_x < 0) player_x = 0;
		}
	} break;

	case AC_DISPLAY_BUTON_MODE_PRESSED: {
		if (game_over) {
			game_init();
			timer_set(AC_TASK_DISPLAY_ID, AC_DISPLAY_GAME_TICK, GAME_TICK_INTERVAL_MS, TIMER_PERIODIC);
		}
		else {
			game_fire();
		}
	} break;

	case AC_DISPLAY_BUTON_MODE_LONG_PRESSED: {
		timer_remove_attr(AC_TASK_DISPLAY_ID, AC_DISPLAY_GAME_TICK);
		SCREEN_TRAN(scr_startup_handle, &scr_startup);
	} break;

	default:
		break;
	}
}












