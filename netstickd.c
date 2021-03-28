// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <linux/uinput.h>
#include <linux/input.h>

#include "tlvc.h"
#include "slip.h"
#include "joystick.h"
#include "server.h"

//---------------------------------------------------------------------------
// SERVER CODE
//---------------------------------------------------------------------------
typedef struct {
    slip_decode_message_t* slipDecode;
    bool                   configSet;
    js_context_t*          joystickContext;
} jsproxy_client_context_t;

//---------------------------------------------------------------------------
void* jsproxy_connect(int clientFd_)
{
    printf("enter:%s, %d\n", __func__, clientFd_);
    (void)clientFd_;

    jsproxy_client_context_t* newContext = (jsproxy_client_context_t*)(calloc(1, sizeof(jsproxy_client_context_t)));
    newContext->slipDecode               = slip_decode_message_create(32768);
    slip_decode_begin(newContext->slipDecode);
    newContext->configSet       = false;
    newContext->joystickContext = NULL;

    return newContext;
}

//---------------------------------------------------------------------------
void jsproxy_disconnect(void* clientContext_)
{
    jsproxy_client_context_t* context = (jsproxy_client_context_t*)clientContext_;
    slip_decode_message_destroy(context->slipDecode);
    printf("enter:%s, %d\n", __func__, context->joystickContext->fd);

    if (context->configSet && context->joystickContext) {
        joystick_destroy(context->joystickContext);
    }
}

//---------------------------------------------------------------------------
static bool emit(int fd, int type, int code, int val)
{
    struct input_event ie;

    ie.type  = type;
    ie.code  = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec  = 0;
    ie.time.tv_usec = 0;

    int rc = write(fd, &ie, sizeof(ie));
    if (rc != sizeof(ie)) {
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------
static void
jsproxy_handle_message(jsproxy_client_context_t* context_, uint16_t eventType_, void* data_, size_t dataSize_)
{
    switch (eventType_) {
        case 0: {
            if (context_->configSet) {
                printf("configuration already set - ignoring\n");
                return;
            }

            if (dataSize_ != sizeof(js_config_t)) {
                printf("expected configuration size %d, got %d\n", (int)sizeof(js_config_t), (int)dataSize_);
                return;
            }

            js_config_t* config = (js_config_t*)data_;

            // Okay, so now that we have the configuration, we need to create the
            // actual joystick object with its details
            context_->joystickContext = joystick_create(config);
            context_->configSet       = true;

        } break;
        case 1: {
            if (!context_->configSet || !context_->joystickContext) {
                printf("joystick hasn't been configured.  Bailing\n");
                return;
            }

            js_report_t  report;
            js_config_t* config = &context_->joystickContext->config;
            int          fd     = context_->joystickContext->fd;

            uint8_t* rawReport = data_;

            report.absAxis = (int32_t*)rawReport;
            report.relAxis = (int32_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount));
            report.buttons = (uint8_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount)
                                        + (sizeof(int32_t) * config->relAxisCount));

            for (int i = 0; i < config->absAxisCount; i++) {
                if (!emit(fd, EV_ABS, config->absAxis[i], report.absAxis[i])) {
                    printf("error writing event to uinput\n");
                }
            }
            for (int i = 0; i < config->relAxisCount; i++) {
                if (!emit(fd, EV_REL, config->relAxis[i], report.relAxis[i])) {
                    printf("error writing event to uinput\n");
                }
            }
            for (int i = 0; i < config->buttonCount; i++) {
                if (!emit(fd, EV_KEY, config->buttons[i], report.buttons[i])) {
                    printf("error writing event to uinput\n");
                }
            }
            if (!emit(fd, EV_SYN, 0, 0)) {
                printf("error writing event to uinput\n");
            }

        } break;
        default: {
            printf("unknown message %d\n", eventType_);
        } break;
    }
}

//---------------------------------------------------------------------------
bool jsproxy_read(int clientFd_, void* clientContext_)
{
    uint8_t buf[256];

    int nRead = 0;
    do {
        nRead = read(clientFd_, buf, sizeof(buf));
        if (nRead <= 0) {
            break;
        }

        jsproxy_client_context_t* context = (jsproxy_client_context_t*)clientContext_;
        for (int i = 0; i < nRead; i++) {
            slip_decode_return_t rc = slip_decode_byte(context->slipDecode, buf[i]);
            if (rc == SlipDecodeEndOfFrame) {
                // Decoder contains the contents of the message into a TLVC frame, validate
                // that it's intact.
                tlvc_data_t tlvc;
                if (tlvc_decode_data(&tlvc, context->slipDecode->raw, context->slipDecode->index)) {
                    // Message is valid and intact, process it.
                    jsproxy_handle_message(context, tlvc.header.tag, tlvc.data, tlvc.dataLen);
                }
                slip_decode_begin(context->slipDecode);
            } else if (rc != SlipDecodeOk) {
                // Error decoding frame -- discard.
                slip_decode_begin(context->slipDecode);
            }
        }
    } while (nRead > 0);

    if (nRead == 0) {
        return false;
    }
    if ((nRead == -1) && ((errno == EINTR) || (errno == EAGAIN))) {
        return true;
    }

    return true;
}

//---------------------------------------------------------------------------
static void jsproxy_server(uint16_t port_)
{
    client_handlers_t handlers
        = { .onConnect = jsproxy_connect, .onDisconnect = jsproxy_disconnect, .onReadData = jsproxy_read };

    server_context_t* server = server_create(port_, 10, &handlers);

    server_run(server);
}

//---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: netstickd [server port]\n");
        return -1;
    }

    jsproxy_server(atoi(argv[1]));
}
