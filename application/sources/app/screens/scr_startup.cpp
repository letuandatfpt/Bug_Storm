#include "scr_startup.h"
#include "startup_logo.h"

static void view_scr_startup();
static void startup_draw_item(int y, const char* text, uint8_t icon, bool selected);
static void startup_draw_bow_icon(int x, int y, uint16_t color);
static void startup_draw_gear_icon(int x, int y, uint16_t color);
static void startup_draw_chart_icon(int x, int y, uint16_t color);

static uint8_t startup_menu_index = 0;
static bool startup_show_logo = true;
static const uint8_t STARTUP_MENU_COUNT = 3;

view_dynamic_t dyn_view_startup = {
	{
		.item_type = ITEM_TYPE_DYNAMIC,
	},
	view_scr_startup
};

view_screen_t scr_startup = {
	&dyn_view_startup,
	ITEM_NULL,
	ITEM_NULL,

	.focus_item = 0,
};

void view_scr_startup() {
	view_render.clear();

	if (startup_show_logo) {
		view_render.setTextColor(WHITE);
		view_render.setTextSize(2);
		view_render.setCursor(44, 7);
		view_render.print("BUG");
		view_render.setTextSize(1);
		view_render.setCursor(47, 27);
		view_render.print("STORM");
		view_render.drawLine(57, 43, 51, 53, WHITE);
		view_render.drawLine(57, 43, 63, 53, WHITE);
		view_render.drawLine(51, 53, 63, 53, WHITE);
		view_render.fillRect(56, 48, 3, 6, WHITE);
		view_render.drawPixel(57, 39, WHITE);
		view_render.drawPixel(57, 36, WHITE);
		return;
	}

	startup_draw_item(2, "Bug_Storm", 0, startup_menu_index == 0);
	startup_draw_item(23, "Cai dat", 1, startup_menu_index == 1);
	startup_draw_item(44, "Thong ke", 2, startup_menu_index == 2);
}

static void startup_draw_item(int y, const char* text, uint8_t icon, bool selected) {
	uint16_t fill_color = selected ? BLACK : WHITE;
	uint16_t line_color = WHITE;
	uint16_t text_color = selected ? WHITE : BLACK;
	int icon_x = 12;
	int icon_y = y + 4;

	view_render.fillRoundRect(4, y, 120, 18, 3, fill_color);
	view_render.drawRoundRect(4, y, 120, 18, 3, line_color);

	view_render.setTextSize(1);
	view_render.setTextColor(text_color);
	view_render.setCursor(42, y + 6);
	view_render.print(text);

	switch (icon) {
	case 0:
		startup_draw_bow_icon(icon_x, icon_y, text_color);
		break;

	case 1:
		startup_draw_gear_icon(icon_x, icon_y, text_color);
		break;

	default:
		startup_draw_chart_icon(icon_x, icon_y, text_color);
		break;
	}
}

static void startup_draw_bow_icon(int x, int y, uint16_t color) {
	view_render.drawLine(x + 10, y, x + 15, y + 4, color);
	view_render.drawLine(x + 15, y + 4, x + 15, y + 10, color);
	view_render.drawLine(x + 15, y + 10, x + 10, y + 14, color);
	view_render.drawLine(x + 5, y + 7, x + 15, y + 7, color);
	view_render.drawLine(x + 4, y + 4, x + 8, y + 7, color);
	view_render.drawLine(x + 4, y + 10, x + 8, y + 7, color);
	view_render.drawLine(x + 1, y + 7, x + 5, y + 7, color);
}

static void startup_draw_gear_icon(int x, int y, uint16_t color) {
	view_render.drawCircle(x + 8, y + 7, 7, color);
	view_render.drawCircle(x + 8, y + 7, 3, color);
	view_render.drawLine(x + 8, y - 1, x + 8, y + 1, color);
	view_render.drawLine(x + 8, y + 13, x + 8, y + 15, color);
	view_render.drawLine(x, y + 7, x + 2, y + 7, color);
	view_render.drawLine(x + 14, y + 7, x + 16, y + 7, color);
	view_render.drawLine(x + 2, y + 1, x + 4, y + 3, color);
	view_render.drawLine(x + 12, y + 11, x + 14, y + 13, color);
	view_render.drawLine(x + 2, y + 13, x + 4, y + 11, color);
	view_render.drawLine(x + 12, y + 3, x + 14, y + 1, color);
}

static void startup_draw_chart_icon(int x, int y, uint16_t color) {
	view_render.drawRect(x, y + 4, 4, 10, color);
	view_render.drawRect(x + 6, y, 4, 14, color);
	view_render.drawRect(x + 12, y + 7, 4, 7, color);
	view_render.drawLine(x - 2, y + 14, x + 18, y + 14, color);
}

void scr_startup_handle(ak_msg_t *msg) {
	switch (msg->sig) {
	case AC_DISPLAY_INITIAL: {
		APP_DBG_SIG("AC_DISPLAY_INITIAL\n");
		view_render.initialize();
		view_render_display_on();
		startup_show_logo = true;
		timer_set(AC_TASK_DISPLAY_ID, AC_DISPLAY_SHOW_LOGO, AC_DISPLAY_STARTUP_INTERVAL, TIMER_ONE_SHOT);
	} break;

	case AC_DISPLAY_BUTON_MODE_PRESSED: {
		APP_DBG_SIG("AC_DISPLAY_BUTON_MODE_PRESSED\n");
		if (startup_show_logo) {
			startup_show_logo = false;
			timer_remove_attr(AC_TASK_DISPLAY_ID, AC_DISPLAY_SHOW_LOGO);
			break;
		}

		if (startup_menu_index == 0) {
			SCREEN_TRAN(scr_game_handle, &scr_game);
		}
	} break;

	case AC_DISPLAY_BUTON_UP_PRESSED: {
		APP_DBG_SIG("AC_DISPLAY_BUTON_UP_PRESSED\n");
		if (startup_show_logo) {
			startup_show_logo = false;
			timer_remove_attr(AC_TASK_DISPLAY_ID, AC_DISPLAY_SHOW_LOGO);
			break;
		}

		startup_menu_index = (startup_menu_index + STARTUP_MENU_COUNT - 1) % STARTUP_MENU_COUNT;
	} break;

	case AC_DISPLAY_BUTON_DOWN_PRESSED: {
		APP_DBG_SIG("AC_DISPLAY_BUTON_DOWN_PRESSED\n");
		if (startup_show_logo) {
			startup_show_logo = false;
			timer_remove_attr(AC_TASK_DISPLAY_ID, AC_DISPLAY_SHOW_LOGO);
			break;
		}

		startup_menu_index = (startup_menu_index + 1) % STARTUP_MENU_COUNT;
	} break;

	case AC_DISPLAY_SHOW_LOGO: {
		APP_DBG_SIG("AC_DISPLAY_SHOW_LOGO\n");
		startup_show_logo = false;
	} break;

	case AC_DISPLAY_SHOW_IDLE: {
		APP_DBG_SIG("AC_DISPLAY_SHOW_IDLE\n");
		SCREEN_TRAN(scr_idle_handle, &scr_idle);
	} break;

	default:
		break;
	}
}


