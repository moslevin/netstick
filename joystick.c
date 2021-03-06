#include "joystick.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

	
#include <linux/uinput.h>
#include <linux/input.h>

//---------------------------------------------------------------------------
static js_context_t* joystick_create_context(const js_config_t* config_) {
	
	js_context_t* newContext = (js_context_t*)(malloc(sizeof(js_context_t)));
	
	newContext->config = *config_;

	if (config_->absAxisCount) {
		newContext->currentReport.absAxis = (int32_t*)(malloc(sizeof(int32_t) * config_->absAxisCount));
		newContext->previousReport.absAxis = (int32_t*)(malloc(sizeof(int32_t) * config_->absAxisCount));
	}
	if (config_->relAxisCount) {
		newContext->currentReport.relAxis = (int32_t*)(malloc(sizeof(int32_t) * config_->relAxisCount));
		newContext->previousReport.relAxis = (int32_t*)(malloc(sizeof(int32_t) * config_->relAxisCount));
	}
	if (config_->buttonCount) {
		newContext->previousReport.buttons = (uint8_t*)(malloc(sizeof(uint8_t) * config_->buttonCount));
		newContext->currentReport.buttons = (uint8_t*)(malloc(sizeof(uint8_t) * config_->buttonCount));
	}
	
	newContext->fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);	
	
	return newContext;
}

//---------------------------------------------------------------------------
void joystick_destroy_context(js_context_t* context_) {
	if (!context_) {
		return;
	}
	if (context_->currentReport.absAxis) {
		free(context_->currentReport.absAxis);
	}
	if (context_->currentReport.relAxis) {
		free(context_->currentReport.relAxis);
	}
	if (context_->currentReport.buttons) {
		free(context_->currentReport.buttons);
	}
	if (context_->previousReport.absAxis) {
		free(context_->currentReport.absAxis);
	}
	if (context_->previousReport.relAxis) {
		free(context_->currentReport.relAxis);
	}
	if (context_->previousReport.buttons) {
		free(context_->currentReport.buttons);
	}
	free(context_);
}

//---------------------------------------------------------------------------
static void joystick_add_device(const js_context_t* context_) {
	struct uinput_setup setup = {};

	setup.id.bustype = BUS_VIRTUAL;
	setup.id.vendor = context_->config.vid; 
	setup.id.product = context_->config.pid; 
	strncpy(setup.name, context_->config.name, UINPUT_MAX_NAME_SIZE);

	ioctl(context_->fd, UI_DEV_SETUP, &setup);
	ioctl(context_->fd, UI_DEV_CREATE);
}

//---------------------------------------------------------------------------
static void joystick_add_relative_axis(const js_context_t* context_) {
	if (context_->config.relAxisCount <= 0) {
		return;
	}
	
	ioctl(context_->fd, UI_SET_EVBIT, EV_REL);	
	for (int i = 0; i < context_->config.relAxisCount; i++) {
		ioctl(context_->fd, UI_SET_RELBIT, context_->config.relAxis[i]);
	}
}

//---------------------------------------------------------------------------
static void joystick_add_absolute_axis(const js_context_t* context_) {
	if (context_->config.absAxisCount <= 0) {
		return;
	}
	
	ioctl(context_->fd, UI_SET_EVBIT, EV_ABS);
	for (int i = 0; i < context_->config.absAxisCount; i++) {
		struct uinput_abs_setup setup = {};
		
		setup.code = context_->config.absAxis[i];
		setup.absinfo.value = 0;
		setup.absinfo.minimum = context_->config.absAxisMin[i];
		setup.absinfo.maximum = context_->config.absAxisMax[i];
		setup.absinfo.fuzz = context_->config.absAxisFuzz[i];
		setup.absinfo.flat = context_->config.absAxisFlat[i];
		setup.absinfo.resolution = context_->config.absAxisResolution[i];
			
		ioctl(context_->fd, UI_ABS_SETUP, &setup);
	}
}

//---------------------------------------------------------------------------
static void joystick_add_buttons(const js_context_t* context_) {
	if (context_->config.buttonCount <= 0) {
		return;
	}
	
	ioctl(context_->fd, UI_SET_EVBIT, EV_KEY);
	for (int i = 0; i < context_->config.buttonCount; i++) {
		ioctl(context_->fd, UI_SET_KEYBIT, context_->config.buttons[i]);
	}
}

//---------------------------------------------------------------------------
static void joystick_add_force_feedback(const js_context_t* context_) {
	// stub.
}

//---------------------------------------------------------------------------
js_context_t* joystick_create(const js_config_t* config_) {

	js_context_t* context = joystick_create_context(config_);
	
	joystick_add_absolute_axis(context);
	joystick_add_relative_axis(context);
	joystick_add_buttons(context);
	joystick_add_force_feedback(context);
	joystick_add_device(context);
	
	return context;
}

//---------------------------------------------------------------------------
void joystick_begin_update(js_context_t* context_) {
	// Copy "current" report into "previous" report.
	if (context_->config.buttonCount > 0) {
		size_t toCopy = sizeof(uint8_t) * context_->config.buttonCount;
		memcpy(context_->previousReport.buttons, context_->currentReport.buttons, toCopy);
	}
	
	if (context_->config.absAxisCount > 0) {
		size_t toCopy = sizeof(int32_t) * context_->config.absAxisCount;
		memcpy(context_->previousReport.absAxis, context_->currentReport.absAxis, toCopy);
	}
	
	if (context_->config.relAxisCount > 0) {
		size_t toCopy = sizeof(int32_t) * context_->config.relAxisCount;
		memcpy(context_->previousReport.relAxis, context_->currentReport.relAxis, toCopy);
	}
}

//---------------------------------------------------------------------------
void joystick_update_button(js_context_t* context_, int button_, uint8_t set_) {
	if (button_ >= context_->config.buttonCount) {
		return; // out of range
	}
	context_->currentReport.buttons[button_] = !!set_;	
}

//---------------------------------------------------------------------------
void joystick_update_abs_axis(js_context_t* context_, int axis_, int32_t value_) {
	if (axis_ >= context_->config.absAxisCount) {
		return; // out of range
	}
	
	// clamp values.
	if (value_ < context_->config.absAxisMin[axis_]) {
		value_ = context_->config.absAxisMin[axis_];
	}
	
	if (value_ > context_->config.absAxisMax[axis_]) {
		value_ = context_->config.absAxisMax[axis_];
	}
	
	context_->currentReport.absAxis[axis_] = value_;	
}

//---------------------------------------------------------------------------
void joystick_update_rel_axis(js_context_t* context_, int axis_, int32_t value_) {
	if (axis_ >= context_->config.relAxisCount) {
		return; // out of range
	}
	
	context_->currentReport.relAxis[axis_] = value_;	
}

//---------------------------------------------------------------------------
size_t joystick_get_report_size(js_context_t* context_) {
	size_t reportSize = (sizeof(uint8_t) * context_->config.buttonCount) 
					  + (sizeof(int32_t) * context_->config.absAxisCount)
					  + (sizeof(int32_t) * context_->config.relAxisCount);
					  
	return reportSize;
}

//---------------------------------------------------------------------------
void joystick_serialize_report(js_context_t* context_, void* data_) {
	uint8_t* dst = data_;
	
	uint8_t* src = (uint8_t*)(context_->currentReport.absAxis);
	for (int i = 0; i < context_->config.absAxisCount * sizeof(int32_t); i++) {
		*dst++ = *src++;
	}
	
	src = (uint8_t*)(context_->currentReport.relAxis);
	for (int i = 0; i < context_->config.relAxisCount * sizeof(int32_t); i++) {
		*dst++ = *src++;
	}
	
	src = (uint8_t*)(context_->currentReport.buttons);
	for (int i = 0; i < context_->config.buttonCount * sizeof(uint8_t); i++) {
		*dst++ = *src++;
	}	
}
