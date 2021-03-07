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
// CLIENT CODE
//---------------------------------------------------------------------------
static void jsproxy_client() {
	
	// Create the client socket address
	int sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFd < 0) {
		printf("error connecting socket: %d (%s)\n", errno, strerror(errno));
	}
	
	// Connect to the server
	struct sockaddr_in addr = {};
	
	addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &(addr.sin_addr));
	addr.sin_port = htons(9001);

	int rc = connect(sockFd,(struct sockaddr *)&addr,sizeof(addr));
	if (rc < 0) {
		printf("error connecting to server: %d (%s)\n", errno, strerror(errno));
		close(sockFd);
	}

	sleep(1);

	// -- DO CLIENT THINGS --
	// 1) Send a fake joystick config
	js_config_t config = {};
	config.vid = 0xDEAD;
	config.pid = 0xBEEF;

	strncpy(config.name, "mostick", sizeof(config.name));

	config.absAxisCount = 4;
	config.relAxisCount = 0;
	config.buttonCount = 14;

	for (int i = 0; i < config.absAxisCount; i++) {
		config.absAxisMin[i] = -16384;
		config.absAxisMax[i] = 16384;
		config.absAxisFuzz[i] = 0;
		config.absAxisFlat[i] = 0;
		config.absAxisResolution[i] = 100;
	}

	config.absAxis[0] = ABS_X;
	config.absAxis[1] = ABS_Y;
	config.absAxis[2] = ABS_RX;
	config.absAxis[3] = ABS_RY;

	config.buttons[0] = BTN_NORTH;
	config.buttons[1] = BTN_EAST;
	config.buttons[2] = BTN_SOUTH;
	config.buttons[3] = BTN_WEST;
	config.buttons[4] = BTN_TL;
	config.buttons[5] = BTN_TR;
	config.buttons[6] = BTN_TL2;
	config.buttons[7] = BTN_TR2;

	config.buttons[8] = BTN_SELECT;
	config.buttons[9] = BTN_START;

	config.buttons[10] = BTN_DPAD_UP;
	config.buttons[11] = BTN_DPAD_LEFT;
	config.buttons[12] = BTN_DPAD_DOWN;
	config.buttons[13] = BTN_DPAD_RIGHT;

	tlvc_data_t tlvc = {};
	tlvc_encode_data(&tlvc, 0, sizeof(js_config_t), &config);

	slip_encode_message_t* encode = slip_encode_message_create(sizeof(js_config_t));
	slip_encode_begin(encode);

	uint8_t* raw = (uint8_t*)&tlvc.header;
	for (int i = 0; i < sizeof(tlvc.header); i++) {
		slip_encode_byte(encode, *raw++);
	}

	raw = (uint8_t*)tlvc.data;
	for (int i = 0; i < tlvc.dataLen; i++) {
		slip_encode_byte(encode, *raw++);
	}

	raw = (uint8_t*)&tlvc.footer;
	for (int i = 0; i < sizeof(tlvc.footer); i++) {
		slip_encode_byte(encode, *raw++);
	}

	slip_encode_finish(encode);

	size_t nWritten = write(sockFd, encode->encoded, encode->index);

	// Send some faked events...
	js_report_t report;
	size_t rawReportSize = (sizeof(int32_t) * config.absAxisCount)
		+ (sizeof(int32_t) * config.relAxisCount)
		+ (sizeof(uint8_t) * config.buttonCount);

	uint8_t* rawReport = calloc(1, rawReportSize);

	report.absAxis = (int32_t*)rawReport;
	report.relAxis = (int32_t*)(rawReport + (sizeof(int32_t) * config.absAxisCount));
	report.buttons = (uint8_t*)(rawReport + (sizeof(int32_t) * config.absAxisCount)
			+ (sizeof(int32_t) * config.relAxisCount));

	int counter = 0;
	while (1) {
		memset(rawReport, 0, rawReportSize);

		int buttonID = counter % 14;
		int axisID = counter % 4;
		int positive = counter % 2;
		report.buttons[buttonID] = 1;
		if (positive) {
			report.absAxis[axisID] = 16384;
		} else {
			report.absAxis[axisID] = -16384;
		}

		tlvc_encode_data(&tlvc, 1, rawReportSize, rawReport);

		slip_encode_begin(encode);

	        raw = (uint8_t*)&tlvc.header;
        	for (int i = 0; i < sizeof(tlvc.header); i++) {
                	slip_encode_byte(encode, *raw++);
        	}

        	raw = (uint8_t*)tlvc.data;
        	for (int i = 0; i < tlvc.dataLen; i++) {
               		slip_encode_byte(encode, *raw++);
        	}

        	raw = (uint8_t*)&tlvc.footer;
        	for (int i = 0; i < sizeof(tlvc.footer); i++) {
                	slip_encode_byte(encode, *raw++);
        	}
		slip_encode_finish(encode);

		nWritten = write(sockFd, encode->encoded, encode->index);

		sleep(1);
		counter++;
	}

	slip_encode_message_destroy(encode);

	printf("client kill\n");
	close(sockFd);
}

//---------------------------------------------------------------------------
// SERVER CODE
//---------------------------------------------------------------------------
typedef struct {
	slip_decode_message_t* 	slipDecode;
	bool configSet;	
	js_context_t* 		joystickContext;
} jsproxy_client_context_t;

//---------------------------------------------------------------------------
void* jsproxy_connect(int clientFd_)  {
	printf("%s: enter\n", __func__);
	jsproxy_client_context_t* newContext = (jsproxy_client_context_t*)(malloc(sizeof(jsproxy_client_context_t)));
	newContext->slipDecode = slip_decode_message_create(32768);	
	slip_decode_begin(newContext->slipDecode);
	newContext->configSet = false;
	newContext->joystickContext = NULL;
	
	return newContext;
}

//---------------------------------------------------------------------------
void jsproxy_disconnect(void* clientContext_) {
	printf("%s: enter\n", __func__);
	jsproxy_client_context_t* context = (jsproxy_client_context_t*)clientContext_;
	slip_decode_message_destroy(context->slipDecode);
	
	if (context->configSet && context->joystickContext) {
		joystick_destroy_context(context->joystickContext);
	}
}

static void emit(int fd, int type, int code, int val)
{
   struct input_event ie;

   ie.type = type;
   ie.code = code;
   ie.value = val;
   /* timestamp values below are ignored */
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;

   write(fd, &ie, sizeof(ie));
}


//---------------------------------------------------------------------------
static void jsproxy_handle_message(jsproxy_client_context_t* context_, uint16_t eventType_, void* data_, size_t dataSize_) {
	printf("%s: enter\n", __func__);
	switch (eventType_) {
		case 0: {			
			printf("received joystick config message\n");
			if (context_->configSet) {
				printf("configuration already set - ignoring\n");
				return;
			}
			
			if (dataSize_ != sizeof(js_config_t)) {
				printf("expected configuration size %ld, got %ld\n", sizeof(js_config_t), dataSize_);
				return;
			}
			
			js_config_t* config = (js_config_t*)data_;
			// process config.			
			printf("Name: %s\n", config->name);
			printf("VID: %04X\n", config->vid);
			printf("PID: %04X\n", config->pid);
			printf("Number of absolute axes: %d\n", config->absAxisCount);
			for (int i = 0; i < config->absAxisCount; i++) {
				printf(" absolute axis %d: id %d\n", i, config->absAxis[i]);
			}
			
			printf("Number of relative axes: %d\n", config->relAxisCount);			
			for (int i = 0; i < config->relAxisCount; i++) {
				printf(" relative axis %d: id %d\n", i, config->relAxis[i]);
				
			}
			
			printf("Number of buttons: %d\n", config->buttonCount);
			for (int i = 0; i < config->buttonCount; i++) {
				printf(" button %d: id %d\n", i, config->buttons[i]);
			}	

			// Okay, so now that we have the configuration, we need to create the 
			// actual joystick object with its details
			context_->joystickContext = joystick_create(config);
			
			
		} break;
		case 1: {
			printf("received joystick report message\n");

			js_report_t report;
			js_config_t* config = &context_->joystickContext->config;
			int fd = context_->joystickContext->fd;

		        size_t rawReportSize = (sizeof(int32_t) * config->absAxisCount)
                		+ (sizeof(int32_t) * config->relAxisCount)
		                + (sizeof(uint8_t) * config->buttonCount);

		        uint8_t* rawReport = data_;

		        report.absAxis = (int32_t*)rawReport;
		        report.relAxis = (int32_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount));
		        report.buttons = (uint8_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount)
		                        + (sizeof(int32_t) * config->relAxisCount));

			for (int i = 0; i < config->absAxisCount; i++) {
				emit(fd, EV_ABS, config->absAxis[i], report.absAxis[i]);
			}
			for (int i = 0; i < config->relAxisCount; i++) {
				emit(fd, EV_REL, config->relAxis[i], report.relAxis[i]);
			}
			for (int i = 0; i < config->buttonCount; i++) {
				emit(fd, EV_KEY, config->buttons[i], report.buttons[i]);
			}
			emit(fd, EV_SYN, 0, 0);

		} break;
		default: {
			printf("unknown message %d\n", eventType_);
		} break;
	}
}

//---------------------------------------------------------------------------
bool jsproxy_read(int clientFd_, void* clientContext_) {
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
			}
			else if (rc != SlipDecodeOk) {
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
static void jsproxy_server() {
	client_handlers_t handlers = {
		.onConnect = jsproxy_connect,
		.onDisconnect = jsproxy_disconnect,
		.onReadData = jsproxy_read
	};
	
	server_context_t* server = server_create(9001, 10, &handlers);
	
	server_run(server);
}

//---------------------------------------------------------------------------
int main() {

	int id = fork();
	
	if (id != 0) {
		jsproxy_server();
	} else {
		jsproxy_client();
	}
}
