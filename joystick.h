// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
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
// Tag types corresponding to our joystick events
typedef enum { JsEventSendReport = 0, JsEventCreateDevice, JsEventRemoveDevice } js_event_type_t;

//---------------------------------------------------------------------------
// Message structure that completely defines a device' configuration
typedef struct __attribute__((packed)) {
    char     name[256];     //!< Device "friendly" name
    uint16_t vid;           //!< USB Device Vendor ID
    uint16_t pid;           //!< USB Device Product ID

    int32_t absAxisCount;   //!< Number of absolute axis supported on this device
    int32_t relAxisCount;   //!< Number of relative axis supported on this device
    int32_t buttonCount;    //!< Number of buttons supported on this device

    uint32_t absAxis[ABS_CNT];              //!< ID for each axis
    int32_t  absAxisMin[ABS_CNT];           //!< Minimum possible values for axis
    int32_t  absAxisMax[ABS_CNT];           //!< Maximum possible values for axis
    int32_t  absAxisFuzz[ABS_CNT];          //!< If Changes are within X counts, ignore
    int32_t  absAxisFlat[ABS_CNT];          //!< Dead-zone for the axis
    int32_t  absAxisResolution[ABS_CNT];    //!< Resolution of the axis (unitless)

    uint32_t relAxis[REL_CNT];     //!< IDs for each relative axis
    uint32_t buttons[KEY_CNT];     //!< IDs for each key/button supported
} js_config_t;

//---------------------------------------------------------------------------
// Report data structure, used to report joystick state to the client
typedef struct {
    int32_t* absAxis;
    int32_t* relAxis;
    uint8_t* buttons;
} js_report_t;

//---------------------------------------------------------------------------
// Data structure that describes the instance of a joystick
typedef struct {
    int fd; //!< fd corresponding to a server connection

    js_config_t config; //!< configuration data for the object

    js_report_t previousReport; //!< previous joystick report data
    js_report_t currentReport;  //!< current joystick report data
} js_context_t;

//---------------------------------------------------------------------------
/**
 * @brief joystick_create Construct a new joystick object based on the configuration provided
 * @param config_ data that describes the device to create
 * @return newly-constructed joystick context, or NULL on error initiatlizing the context
 */
js_context_t* joystick_create(const js_config_t* config_);

//---------------------------------------------------------------------------
/**
 * @brief joystick_destroy destroy a previously-constrcted joystick object.
 * Note: object must not be used after calling destroy on it.
 * @param context_ object to destroy.
 */
void joystick_destroy(js_context_t* context_);

//---------------------------------------------------------------------------
/**
 * @brief joystick_begin_update indicate the beginning of a new report is taking
 * place.  This is called before processing any new events read from the HID
 * device.
 * @param context_ pointer to the joystick context_ object to make ready for updates
 */
void joystick_begin_update(js_context_t* context_);

//---------------------------------------------------------------------------
/**
 * @brief joystick_update_button Update the state of a specified button in the current
 * report.
 * @param context_ pointer to the joystick context_ object corresponding to the event
 * @param button_ ID of the button to update
 * @param set_ value to set the button to (0 == not set, 1 == set)
 */
void joystick_update_button(js_context_t* context_, int button_, uint8_t set_);

//---------------------------------------------------------------------------
/**
 * @brief joystick_update_abs_axis Update the value of an absolute axis in the
 * current report.
 * @param context_ pointer to the joystick context_ object corresponding to the event
 * @param axis_ ID of the axis to update
 * @param value_ value to set for the axis in the report
 */
void joystick_update_abs_axis(js_context_t* context_, int axis_, int32_t value_);

//---------------------------------------------------------------------------
/**
 * @brief joystick_update_rel_axis Update the value of a relative axis in the
 * current report.
 * @param context_ pointer to the joystick context_ object corresponding to the event
 * @param axis_ ID of the axis to update
 * @param value_ value to set for the axis in the report
 */
void joystick_update_rel_axis(js_context_t* context_, int axis_, int32_t value_);

//---------------------------------------------------------------------------
/**
 * @brief joystick_get_report_size Return the size of the report structure for
 * the given joystick context.  Note that this varies based on the number of
 * buttons and axis configured for the device.
 * @param context_ pointer to the joystick context_ to return the report size for
 * @return size of the report structure for a given joystick context
 */
size_t joystick_get_report_size(const js_config_t* context_);

#if defined(__cplusplus)
}
#endif
