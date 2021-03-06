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
	js_config_t config = {};
	config.vid = 0xDEAD;
	config.pid = 0xBEEF;
	
	strncpy(config.name, "mostick", sizeof(config.name));
	
	config.absAxisCount = 2;
	config.relAxisCount = 0;
	config.buttonCount = 8;
	
	for (int i = 0; i < config.absAxisCount; i++) {
		config.absAxis[i] = i;
		config.absAxisMin[i] = -16384;
		config.absAxisMax[i] = 16384;
		config.absAxisFuzz[i] = 0;
		config.absAxisFlat[i] = 0;
		config.absAxisResolution[i] = 100;
	}
	
	for (int i = 0; i < config.relAxisCount; i++) {
		config.relAxis[i] = i;
	}
	
	for (int i = 0; i < config.buttonCount; i++) {
		config.buttons[i] = i;
	}
	
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
	
	slip_encode_message_destroy(encode);
	
	#if 0
	
	// Write the tlv struct as an iovec in a single operation to the socket.
	struct iovec iov[3];
	iov[0].iov_base = &tlvc.header;
	iov[0].iov_len = sizeof(tlvc.header);
	iov[1].iov_base = encode->encoded;
	iov[1].iov_len = encode->encodedSize;
	iov[2].iov_base = &tlvc.footer;
	iov[2].iov_len = sizeof(tlvc.footer);
	
	size_t nWritten = writev(sockFd, iov, 3);
	printf("wrote %ld bytes\n", nWritten);
	// --
	#endif
	
	sleep(10);
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