// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#include "joystick.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <linux/uinput.h>
#include <linux/input.h>

//---------------------------------------------------------------------------
static js_context_t* joystick_create_context(const js_config_t* config_)
{
    js_context_t* newContext = (js_context_t*)(calloc(1, sizeof(js_context_t)));

    newContext->config = *config_;
    newContext->fd     = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    return newContext;
}

//---------------------------------------------------------------------------
static void joystick_destroy_context(js_context_t* context_)
{
    if (!context_) {
        return;
    }
    free(context_);
}

//---------------------------------------------------------------------------
static void joystick_add_device(const js_context_t* context_)
{
    struct uinput_setup setup = {};

    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor  = context_->config.vid;
    setup.id.product = context_->config.pid;
    strncpy(setup.name, context_->config.name, UINPUT_MAX_NAME_SIZE);

    ioctl(context_->fd, UI_DEV_SETUP, &setup);
    ioctl(context_->fd, UI_DEV_CREATE);
}

//---------------------------------------------------------------------------
static void joystick_add_relative_axis(const js_context_t* context_)
{
    if (context_->config.relAxisCount <= 0) {
        return;
    }

    ioctl(context_->fd, UI_SET_EVBIT, EV_REL);
    for (int i = 0; i < context_->config.relAxisCount; i++) {
        ioctl(context_->fd, UI_SET_RELBIT, context_->config.relAxis[i]);
    }
}

//---------------------------------------------------------------------------
static void joystick_add_absolute_axis(const js_context_t* context_)
{
    if (context_->config.absAxisCount <= 0) {
        return;
    }

    ioctl(context_->fd, UI_SET_EVBIT, EV_ABS);
    for (int i = 0; i < context_->config.absAxisCount; i++) {
        struct uinput_abs_setup setup = {};

        setup.code               = context_->config.absAxis[i];
        setup.absinfo.value      = 0;
        setup.absinfo.minimum    = context_->config.absAxisMin[i];
        setup.absinfo.maximum    = context_->config.absAxisMax[i];
        setup.absinfo.fuzz       = context_->config.absAxisFuzz[i];
        setup.absinfo.flat       = context_->config.absAxisFlat[i];
        setup.absinfo.resolution = context_->config.absAxisResolution[i];

        ioctl(context_->fd, UI_ABS_SETUP, &setup);
    }
}

//---------------------------------------------------------------------------
static void joystick_add_buttons(const js_context_t* context_)
{
    if (context_->config.buttonCount <= 0) {
        return;
    }

    ioctl(context_->fd, UI_SET_EVBIT, EV_KEY);
    for (int i = 0; i < context_->config.buttonCount; i++) {
        ioctl(context_->fd, UI_SET_KEYBIT, context_->config.buttons[i]);
    }
}

//---------------------------------------------------------------------------
static void joystick_add_force_feedback(const js_context_t* context_)
{
    // stub.
    (void)context_;
}

//---------------------------------------------------------------------------
js_context_t* joystick_create(const js_config_t* config_)
{
    js_context_t* context = joystick_create_context(config_);

    joystick_add_absolute_axis(context);
    joystick_add_relative_axis(context);
    joystick_add_buttons(context);
    joystick_add_force_feedback(context);
    joystick_add_device(context);

    return context;
}

//---------------------------------------------------------------------------
void joystick_destroy(js_context_t* context_)
{
    ioctl(context_->fd, UI_DEV_DESTROY);
    close(context_->fd);
    joystick_destroy_context(context_);
}

//---------------------------------------------------------------------------
size_t joystick_get_report_size(const js_config_t* config)
{
    size_t reportSize = (sizeof(uint8_t) * config->buttonCount) + (sizeof(int32_t) * config->absAxisCount)
                        + (sizeof(int32_t) * config->relAxisCount);

    return reportSize;
}
