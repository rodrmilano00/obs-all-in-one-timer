/*
 * All In One Timer - OBS Studio source
 *
 * Port of the "All In One Timer" FFGL plugin for Resolume Arena.
 * Stopwatch / countdown rendered with GDI into a texture, drawn on a
 * canvas-sized source with the same Size / X / Y controls as the original.
 */

#include <obs-module.h>
#include <graphics/vec4.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("all-in-one-timer", "en-US")

#define T_(s) obs_module_text(s)

#define FONT_SIZE 400 /* high resolution for crisp text, same as the FFGL version */
#define PAD_X 40
#define PAD_Y 20
#define MIN_TEX_SIZE 64
#define DEFAULT_FONT "Arial"
#define FONT_NAME_MAX (LF_FACESIZE * 4) /* UTF-8 */

#define S_FORMAT "format"
#define S_MODE "mode"
#define S_CD_HOURS "cd_hours"
#define S_CD_MINUTES "cd_minutes"
#define S_CD_SECONDS "cd_seconds"
#define S_TEXT_COLOR "text_color"
#define S_SIZE "size"
#define S_X_POS "x_pos"
#define S_Y_POS "y_pos"
#define S_FONT "font"
#define S_SHOW_BG "show_bg"
#define S_BG_COLOR "bg_color"

enum timer_format {
	FORMAT_MMSS,
	FORMAT_HHMMSS,
};

enum timer_mode {
	MODE_COUNT_UP,
	MODE_COUNTDOWN,
};

struct timer_source {
	obs_source_t *source;
	SRWLOCK lock; /* guards settings + timer state (UI, hotkey and graphics threads) */

	/* Settings */
	int format;
	int mode;
	int cd_hours;
	int cd_minutes;
	int cd_seconds;
	uint32_t text_color; /* 0xAABBGGRR, OBS color picker format */
	double size;
	double x_pos;
	double y_pos;
	char font[FONT_NAME_MAX];
	bool show_bg;
	uint32_t bg_color;

	/* Timer state */
	bool running;
	bool active; /* shown on the program output */
	double elapsed;
	char text[32];

	obs_hotkey_id hk_start_pause;
	obs_hotkey_id hk_reset;

	/* Render state (graphics thread only) */
	uint8_t *mask; /* text coverage, one byte per pixel */
	int mask_w;
	int mask_h;
	char mask_text[32];
	char mask_font[FONT_NAME_MAX];
	bool mask_valid;
	gs_texture_t *tex;
	uint32_t tex_w;
	uint32_t tex_h;
	uint32_t tex_color;
	bool tex_dirty;
};

/* ------------------------------------------------------------------------- */
/* Timer logic                                                               */

static double countdown_total(const struct timer_source *t)
{
	return (double)t->cd_hours * 3600.0 + (double)t->cd_minutes * 60.0 + (double)t->cd_seconds;
}

/* Must be called with the lock held */
static void format_time(struct timer_source *t)
{
	double display_time;
	if (t->mode == MODE_COUNTDOWN) {
		display_time = countdown_total(t) - t->elapsed;
		if (display_time < 0.0)
			display_time = 0.0;
	} else {
		display_time = t->elapsed;
	}

	int hours = (int)(display_time / 3600.0);
	int minutes = (int)(fmod(display_time, 3600.0) / 60.0);
	int seconds = (int)fmod(display_time, 60.0);

	if (t->format == FORMAT_HHMMSS)
		snprintf(t->text, sizeof(t->text), "%02d:%02d:%02d", hours, minutes, seconds);
	else
		snprintf(t->text, sizeof(t->text), "%02d:%02d", minutes, seconds);
}

static void timer_start_pause(struct timer_source *t)
{
	AcquireSRWLockExclusive(&t->lock);
	t->running = !t->running;
	ReleaseSRWLockExclusive(&t->lock);
}

static void timer_reset(struct timer_source *t)
{
	AcquireSRWLockExclusive(&t->lock);
	t->running = false;
	t->elapsed = 0.0;
	format_time(t);
	ReleaseSRWLockExclusive(&t->lock);
}

static void hotkey_start_pause(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		timer_start_pause(data);
}

static void hotkey_reset(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		timer_reset(data);
}

static bool start_pause_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	timer_start_pause(data);
	return false;
}

static bool reset_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	timer_reset(data);
	return false;
}

/* ------------------------------------------------------------------------- */
/* Text rendering (GDI -> coverage mask -> colored texture)                  */

static bool render_mask(struct timer_source *t, const char *text, const char *font)
{
	wchar_t wtext[32];
	wchar_t wfont[LF_FACESIZE];

	if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 32))
		return false;
	if (!MultiByteToWideChar(CP_UTF8, 0, font, -1, wfont, LF_FACESIZE))
		wcscpy_s(wfont, LF_FACESIZE, L"" DEFAULT_FONT);

	HDC hdc = CreateCompatibleDC(NULL);
	if (!hdc)
		return false;

	HFONT hfont = CreateFontW(FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
				  OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
				  DEFAULT_PITCH | FF_DONTCARE, wfont);
	HFONT old_font = (HFONT)SelectObject(hdc, hfont);

	/* Measure text size, add padding to avoid cut-off */
	RECT measure = {0, 0, 0, 0};
	DrawTextW(hdc, wtext, -1, &measure, DT_CALCRECT | DT_SINGLELINE);

	int w = measure.right + PAD_X * 2;
	int h = measure.bottom + PAD_Y * 2;
	if (w < MIN_TEX_SIZE)
		w = MIN_TEX_SIZE;
	if (h < MIN_TEX_SIZE)
		h = MIN_TEX_SIZE;

	BITMAPINFO bi = {0};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = w;
	bi.bmiHeader.biHeight = -h; /* top-down DIB */
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	void *bits = NULL;
	HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	bool ok = bmp && bits;

	if (ok) {
		HBITMAP old_bmp = (HBITMAP)SelectObject(hdc, bmp);

		memset(bits, 0, (size_t)w * h * 4);
		SetTextColor(hdc, RGB(255, 255, 255));
		SetBkMode(hdc, TRANSPARENT);

		RECT rc = {PAD_X, PAD_Y, w - PAD_X, h - PAD_Y};
		DrawTextW(hdc, wtext, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		GdiFlush();

		/* Coverage = brightness of each pixel (text is white so channels are ~equal) */
		size_t count = (size_t)w * h;
		uint8_t *mask = brealloc(t->mask, count);
		const uint8_t *px = bits;
		for (size_t i = 0; i < count; i++)
			mask[i] = (uint8_t)((px[i * 4] + px[i * 4 + 1] + px[i * 4 + 2]) / 3);

		t->mask = mask;
		t->mask_w = w;
		t->mask_h = h;

		SelectObject(hdc, old_bmp);
		DeleteObject(bmp);
	}

	SelectObject(hdc, old_font);
	DeleteObject(hfont);
	DeleteDC(hdc);
	return ok;
}

static void upload_texture(struct timer_source *t, uint32_t color)
{
	const uint8_t r = color & 0xFF;
	const uint8_t g = (color >> 8) & 0xFF;
	const uint8_t b = (color >> 16) & 0xFF;
	const uint32_t a = (color >> 24) & 0xFF;

	const uint32_t w = (uint32_t)t->mask_w;
	const uint32_t h = (uint32_t)t->mask_h;
	const size_t count = (size_t)w * h;

	uint8_t *bgra = bmalloc(count * 4);
	for (size_t i = 0; i < count; i++) {
		bgra[i * 4 + 0] = b;
		bgra[i * 4 + 1] = g;
		bgra[i * 4 + 2] = r;
		bgra[i * 4 + 3] = (uint8_t)(t->mask[i] * a / 255);
	}

	if (t->tex && (t->tex_w != w || t->tex_h != h)) {
		gs_texture_destroy(t->tex);
		t->tex = NULL;
	}

	if (t->tex) {
		gs_texture_set_image(t->tex, bgra, w * 4, false);
	} else {
		const uint8_t *planes[1] = {bgra};
		t->tex = gs_texture_create(w, h, GS_BGRA, 1, planes, GS_DYNAMIC);
		t->tex_w = w;
		t->tex_h = h;
	}

	bfree(bgra);
	t->tex_color = color;
}

/* Re-render text only when the displayed string, font or color changes */
static void update_texture(struct timer_source *t, const char *text, const char *font, uint32_t color)
{
	bool text_changed = !t->mask_valid || strcmp(text, t->mask_text) != 0 || strcmp(font, t->mask_font) != 0;

	if (text_changed) {
		if (!render_mask(t, text, font))
			return;
		strcpy_s(t->mask_text, sizeof(t->mask_text), text);
		strcpy_s(t->mask_font, sizeof(t->mask_font), font);
		t->mask_valid = true;
	}

	if (text_changed || !t->tex || t->tex_color != color)
		upload_texture(t, color);
}

/* ------------------------------------------------------------------------- */
/* Source callbacks                                                          */

static const char *timer_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_("AllInOneTimer");
}

static void timer_update(void *data, obs_data_t *settings)
{
	struct timer_source *t = data;

	AcquireSRWLockExclusive(&t->lock);
	t->format = (int)obs_data_get_int(settings, S_FORMAT);
	t->mode = (int)obs_data_get_int(settings, S_MODE);
	t->cd_hours = (int)obs_data_get_int(settings, S_CD_HOURS);
	t->cd_minutes = (int)obs_data_get_int(settings, S_CD_MINUTES);
	t->cd_seconds = (int)obs_data_get_int(settings, S_CD_SECONDS);
	t->text_color = (uint32_t)obs_data_get_int(settings, S_TEXT_COLOR);
	t->size = obs_data_get_double(settings, S_SIZE);
	t->x_pos = obs_data_get_double(settings, S_X_POS);
	t->y_pos = obs_data_get_double(settings, S_Y_POS);
	t->show_bg = obs_data_get_bool(settings, S_SHOW_BG);
	t->bg_color = (uint32_t)obs_data_get_int(settings, S_BG_COLOR);

	const char *font = obs_data_get_string(settings, S_FONT);
	strcpy_s(t->font, sizeof(t->font), (font && *font) ? font : DEFAULT_FONT);

	format_time(t);
	ReleaseSRWLockExclusive(&t->lock);
}

static void *timer_create(obs_data_t *settings, obs_source_t *source)
{
	struct timer_source *t = bzalloc(sizeof(struct timer_source));
	t->source = source;
	InitializeSRWLock(&t->lock);

	t->hk_start_pause = obs_hotkey_register_source(source, "AllInOneTimer.StartPause", T_("StartPause"),
						       hotkey_start_pause, t);
	t->hk_reset = obs_hotkey_register_source(source, "AllInOneTimer.Reset", T_("Reset"), hotkey_reset, t);

	timer_update(t, settings);
	return t;
}

static void timer_destroy(void *data)
{
	struct timer_source *t = data;

	obs_enter_graphics();
	gs_texture_destroy(t->tex);
	obs_leave_graphics();

	bfree(t->mask);
	bfree(t);
}

static void timer_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_FORMAT, FORMAT_MMSS);
	obs_data_set_default_int(settings, S_MODE, MODE_COUNT_UP);
	obs_data_set_default_int(settings, S_CD_HOURS, 0);
	obs_data_set_default_int(settings, S_CD_MINUTES, 1);
	obs_data_set_default_int(settings, S_CD_SECONDS, 0);
	obs_data_set_default_int(settings, S_TEXT_COLOR, 0xFF00FF05); /* HSB(0.33, 1, 1) green */
	obs_data_set_default_double(settings, S_SIZE, 0.3);
	obs_data_set_default_double(settings, S_X_POS, 0.5);
	obs_data_set_default_double(settings, S_Y_POS, 0.5);
	obs_data_set_default_string(settings, S_FONT, DEFAULT_FONT);
	obs_data_set_default_bool(settings, S_SHOW_BG, false);
	obs_data_set_default_int(settings, S_BG_COLOR, 0x80000000); /* black, 50% alpha */
}

/* Installed font enumeration for the font dropdown */
struct font_list {
	wchar_t (*names)[LF_FACESIZE];
	size_t num;
	size_t capacity;
};

static int CALLBACK enum_font_proc(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD font_type, LPARAM param)
{
	UNUSED_PARAMETER(tm);
	UNUSED_PARAMETER(font_type);

	struct font_list *list = (struct font_list *)param;

	/* Skip vertical variants ("@Font") */
	if (lf->lfFaceName[0] == L'@' || lf->lfFaceName[0] == L'\0')
		return 1;

	/* Avoid duplicates (Windows enumerates each font per charset) */
	for (size_t i = 0; i < list->num; i++) {
		if (wcscmp(list->names[i], lf->lfFaceName) == 0)
			return 1;
	}

	if (list->num == list->capacity) {
		list->capacity = list->capacity ? list->capacity * 2 : 256;
		list->names = brealloc(list->names, list->capacity * sizeof(*list->names));
	}
	wcscpy_s(list->names[list->num++], LF_FACESIZE, lf->lfFaceName);
	return 1;
}

static int compare_font_names(const void *a, const void *b)
{
	return _wcsicmp((const wchar_t *)a, (const wchar_t *)b);
}

static void add_font_list(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_add_list(props, S_FONT, T_("Font"), OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);

	struct font_list list = {0};
	HDC hdc = GetDC(NULL);
	LOGFONTW lf = {0};
	lf.lfCharSet = DEFAULT_CHARSET;
	EnumFontFamiliesExW(hdc, &lf, enum_font_proc, (LPARAM)&list, 0);
	ReleaseDC(NULL, hdc);

	if (!list.num) {
		obs_property_list_add_string(p, DEFAULT_FONT, DEFAULT_FONT);
		return;
	}

	qsort(list.names, list.num, sizeof(*list.names), compare_font_names);

	char name[FONT_NAME_MAX];
	for (size_t i = 0; i < list.num; i++) {
		if (WideCharToMultiByte(CP_UTF8, 0, list.names[i], -1, name, sizeof(name), NULL, NULL))
			obs_property_list_add_string(p, name, name);
	}

	bfree(list.names);
}

static obs_properties_t *timer_get_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	/* Timer Controls */
	obs_properties_t *controls = obs_properties_create();
	obs_properties_add_button(controls, "start_pause", T_("StartPause"), start_pause_clicked);
	obs_properties_add_button(controls, "reset", T_("Reset"), reset_clicked);

	p = obs_properties_add_list(controls, S_FORMAT, T_("Format"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "MM:SS", FORMAT_MMSS);
	obs_property_list_add_int(p, "HH:MM:SS", FORMAT_HHMMSS);

	p = obs_properties_add_list(controls, S_MODE, T_("Mode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_("Mode.CountUp"), MODE_COUNT_UP);
	obs_property_list_add_int(p, T_("Mode.Countdown"), MODE_COUNTDOWN);

	obs_properties_add_int_slider(controls, S_CD_HOURS, T_("CdHours"), 0, 23, 1);
	obs_properties_add_int_slider(controls, S_CD_MINUTES, T_("CdMinutes"), 0, 59, 1);
	obs_properties_add_int_slider(controls, S_CD_SECONDS, T_("CdSeconds"), 0, 59, 1);
	obs_properties_add_group(props, "timer_controls", T_("Group.TimerControls"), OBS_GROUP_NORMAL, controls);

	/* Text Color */
	obs_properties_t *text_color = obs_properties_create();
	obs_properties_add_color_alpha(text_color, S_TEXT_COLOR, T_("TextColor"));
	obs_properties_add_group(props, "text_color_group", T_("Group.TextColor"), OBS_GROUP_NORMAL, text_color);

	/* Display */
	obs_properties_t *display = obs_properties_create();
	obs_properties_add_float_slider(display, S_SIZE, T_("Size"), 0.05, 2.0, 0.01);
	obs_properties_add_float_slider(display, S_X_POS, T_("XPos"), 0.0, 1.0, 0.001);
	obs_properties_add_float_slider(display, S_Y_POS, T_("YPos"), 0.0, 1.0, 0.001);
	add_font_list(display);
	obs_properties_add_group(props, "display", T_("Group.Display"), OBS_GROUP_NORMAL, display);

	/* Background */
	obs_properties_t *background = obs_properties_create();
	obs_properties_add_bool(background, S_SHOW_BG, T_("ShowBg"));
	obs_properties_add_color_alpha(background, S_BG_COLOR, T_("BgColor"));
	obs_properties_add_group(props, "background", T_("Group.Background"), OBS_GROUP_NORMAL, background);

	return props;
}

/*
 * Auto-pause: the timer only advances while the source is shown on the
 * program output, same as the FFGL version pausing when its layer leaves
 * the Resolume program.
 */
static void timer_activate(void *data)
{
	struct timer_source *t = data;
	AcquireSRWLockExclusive(&t->lock);
	t->active = true;
	ReleaseSRWLockExclusive(&t->lock);
}

static void timer_deactivate(void *data)
{
	struct timer_source *t = data;
	AcquireSRWLockExclusive(&t->lock);
	t->active = false;
	ReleaseSRWLockExclusive(&t->lock);
}

static void timer_tick(void *data, float seconds)
{
	struct timer_source *t = data;

	AcquireSRWLockExclusive(&t->lock);
	if (t->running && t->active && seconds > 0.0f) {
		t->elapsed += seconds;

		if (t->mode == MODE_COUNTDOWN) {
			double total = countdown_total(t);
			if (t->elapsed >= total) {
				t->elapsed = total;
				t->running = false;
			}
		}
	}
	format_time(t);
	ReleaseSRWLockExclusive(&t->lock);
}

static uint32_t timer_get_width(void *data)
{
	UNUSED_PARAMETER(data);
	struct obs_video_info ovi;
	return obs_get_video_info(&ovi) ? ovi.base_width : 1920;
}

static uint32_t timer_get_height(void *data)
{
	UNUSED_PARAMETER(data);
	struct obs_video_info ovi;
	return obs_get_video_info(&ovi) ? ovi.base_height : 1080;
}

static void timer_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct timer_source *t = data;

	char text[sizeof(t->text)];
	char font[sizeof(t->font)];

	AcquireSRWLockShared(&t->lock);
	strcpy_s(text, sizeof(text), t->text);
	strcpy_s(font, sizeof(font), t->font);
	const uint32_t text_color = t->text_color;
	const uint32_t bg_color = t->bg_color;
	const bool show_bg = t->show_bg;
	const double size = t->size;
	const double x_pos = t->x_pos;
	const double y_pos = t->y_pos;
	ReleaseSRWLockShared(&t->lock);

	update_texture(t, text, font, text_color);
	if (!t->tex)
		return;

	const float canvas_w = (float)timer_get_width(t);
	const float canvas_h = (float)timer_get_height(t);

	/* Text height = Size * canvas height; width follows the texture aspect ratio.
	 * Position is the center of the text; Y = 1 is the top, as in Resolume. */
	const float scale = (float)size * canvas_h / (float)t->tex_h;
	const float quad_w = (float)t->tex_w * scale;
	const float quad_h = (float)t->tex_h * scale;
	const float x0 = (float)x_pos * canvas_w - quad_w * 0.5f;
	const float y0 = (1.0f - (float)y_pos) * canvas_h - quad_h * 0.5f;

	gs_blend_state_push();
	gs_reset_blend_state();

	gs_matrix_push();
	gs_matrix_translate3f(x0, y0, 0.0f);
	gs_matrix_scale3f(scale, scale, 1.0f);

	if (show_bg) {
		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		struct vec4 color;
		vec4_from_rgba(&color, bg_color);
		gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);
		while (gs_effect_loop(solid, "Solid"))
			gs_draw_sprite(NULL, 0, t->tex_w, t->tex_h);
	}

	gs_effect_t *def = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_effect_set_texture(gs_effect_get_param_by_name(def, "image"), t->tex);
	while (gs_effect_loop(def, "Draw"))
		gs_draw_sprite(t->tex, 0, 0, 0);

	gs_matrix_pop();
	gs_blend_state_pop();
}

static struct obs_source_info timer_source_info = {
	.id = "all_in_one_timer",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.icon_type = OBS_ICON_TYPE_TEXT,
	.get_name = timer_get_name,
	.create = timer_create,
	.destroy = timer_destroy,
	.update = timer_update,
	.get_defaults = timer_get_defaults,
	.get_properties = timer_get_properties,
	.activate = timer_activate,
	.deactivate = timer_deactivate,
	.video_tick = timer_tick,
	.video_render = timer_render,
	.get_width = timer_get_width,
	.get_height = timer_get_height,
};

const char *obs_module_description(void)
{
	return "Customizable event timer/countdown for live productions";
}

bool obs_module_load(void)
{
	obs_register_source(&timer_source_info);
	return true;
}
