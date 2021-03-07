#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <linux/uinput.h>
#include <linux/input.h>

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
typedef enum {
	JsEventSendReport = 0,
	JsEventCreateDevice,
	JsEventRemoveDevice
} js_event_type_t;

//---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
	char 		name[256];
	uint16_t	vid;
	uint16_t	pid;
	
	int32_t absAxisCount;
	int32_t relAxisCount;
	int32_t	buttonCount;
	
	uint32_t absAxis[ABS_CNT];
	int32_t absAxisMin[ABS_CNT];
	int32_t absAxisMax[ABS_CNT];
	int32_t absAxisFuzz[ABS_CNT];
	int32_t absAxisFlat[ABS_CNT];
	int32_t absAxisResolution[ABS_CNT];
	
	uint32_t relAxis[REL_CNT];
	uint32_t buttons[KEY_CNT];
} js_config_t;

//---------------------------------------------------------------------------
typedef struct {
	int32_t* absAxis;
	int32_t* relAxis;
	uint8_t* buttons;
} js_report_t;

//---------------------------------------------------------------------------
typedef struct {
	int fd;
	
	js_config_t config;
	
	js_report_t previousReport;
	js_report_t currentReport;
} js_context_t;

//---------------------------------------------------------------------------
js_context_t* joystick_create(const js_config_t* config_);

//---------------------------------------------------------------------------
void joystick_destroy(js_context_t* context_);

//---------------------------------------------------------------------------
void joystick_begin_update(js_context_t* context_);

//---------------------------------------------------------------------------
void joystick_update_button(js_context_t* context_, int button_, uint8_t set_);

//---------------------------------------------------------------------------
void joystick_update_abs_axis(js_context_t* context_, int axis_, int32_t value_);

//---------------------------------------------------------------------------
void joystick_update_rel_axis(js_context_t* context_, int axis_, int32_t value_);

//---------------------------------------------------------------------------
size_t joystick_get_report_size(const js_config_t* context_);

#if defined(__cplusplus)
}
#endif
