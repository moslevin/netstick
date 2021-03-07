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
static bool encode_and_transmit(int sockFd_, uint16_t messageType_, void* data_, size_t dataLen_) {
	tlvc_data_t tlvc = {};
	tlvc_encode_data(&tlvc, messageType_, dataLen_, data_);

	slip_encode_message_t* encode = slip_encode_message_create(dataLen_);
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

	int nWritten = write(sockFd_, encode->encoded, encode->index);
	
	slip_encode_message_destroy(encode);
	
	if ( (nWritten == 0) || 
	    ((nWritten == -1) && !((errno == EINTR) || (errno == EAGAIN)))) {
		printf("socket died during write\n");
		return false;
	}
	
	return true;
}

//---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
	uint16_t bus;
	uint16_t vid;
	uint16_t pid;
	uint16_t version;
} input_dev_info_t;

//---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
	int32_t value;
	int32_t minimum;	
	int32_t maximum;
	int32_t flat;
	int32_t fuzz;
} abs_axis_info_t;

//---------------------------------------------------------------------------
// map linux button/axis IDs to indexes in the config + report structures.
typedef struct {
	int absAxis[KEY_MAX];
	int relAxis[KEY_MAX];
	int buttons[KEY_MAX];
} js_index_map_t;

//---------------------------------------------------------------------------
static void js_index_map_init(js_index_map_t* indexMap_) {
	for (int i = 0; i < KEY_MAX; i++) {
		indexMap_->absAxis[i] = -1;
		indexMap_->relAxis[i] = -1;
		indexMap_->buttons[i] = -1;
	}
}

//---------------------------------------------------------------------------
static void js_index_map_set(js_index_map_t* indexMap_, int eventType_, int eventId_, int index_) {
	if (eventType_ == EV_ABS) {
		indexMap_->absAxis[eventId_] = index_;
	} else if (eventType_ == EV_REL) {
		indexMap_->relAxis[eventId_] = index_;
	} else if (eventType_ == EV_KEY) {
		indexMap_->buttons[eventId_] = index_;
	} 
}

//---------------------------------------------------------------------------
static int js_index_map_get_index(js_index_map_t* indexMap_, int eventType_, int eventId_) {
	if (eventType_ == EV_ABS) {
		return indexMap_->absAxis[eventId_];
	} else if (eventType_ == EV_REL) {
		return indexMap_->relAxis[eventId_];
	} else if (eventType_ == EV_KEY) {
		return indexMap_->buttons[eventId_];
	} 
	return -1;
}

//---------------------------------------------------------------------------
static inline int byte_offset(int bitIndex_) {
	return bitIndex_ / 8;
}

//---------------------------------------------------------------------------
static inline int bit_offset(int bitIndex_) {
	return bitIndex_ % 8;
}

//---------------------------------------------------------------------------
static inline bool is_bit_set(uint8_t* array_, int bitIndex_) {
	if ( array_[byte_offset(bitIndex_)] & (1 << bit_offset(bitIndex_)) ) {
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
static void jsproxy_client_uinput(const char* ioPath_, const char* serverAddr_, uint16_t serverPort_) {
	// Open the input device requested by the user
	int fd = open(ioPath_, O_RDONLY);
	if (fd < 0) {
		printf("Unable to open device %s for input\n", ioPath_);
		return;
	}

	// Create objects used to store the input device's configuration that we report 
	// to the server, enabling it to recreate a "virtual" version of it.
	js_index_map_t* indexMap = (js_index_map_t*)malloc(sizeof(js_index_map_t));
	js_index_map_init(indexMap);
	
	js_config_t config = {};

	// Get the basic information for the device at the path specified (USB vid/pid, etc.)
	input_dev_info_t info = {};
	ioctl(fd, EVIOCGID, &info);
	printf("bus=0x%04X, vid=0x%04X, pid=0x%04X, version=0x%04X\n",
			info.bus, info.vid, info.pid, info.version);
			
	config.pid = info.pid;
	config.vid = info.vid;

	// Get the device's name
	char devName[256];
	ioctl(fd, EVIOCGNAME(sizeof(devName)), devName);
	printf("deviceName=%s\n", devName);
	
	// Query the device for its supported event types
	uint8_t bit[EV_MAX][(KEY_MAX + 7) / 8] = {};
	ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
	strncpy(config.name, devName, sizeof(config.name));

	// Go through all the different r
	for (int i = 0; i < EV_MAX; i++) {
		if (is_bit_set(bit[0], i)) {
			if (i == EV_SYN) {
				continue;
			}
			
			ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
			
			for (int j = 0; j < KEY_MAX; j++) {
				if (is_bit_set(bit[i], j)) {
			
					if (i == EV_ABS) {
						abs_axis_info_t absAxis = {};
						ioctl(fd, EVIOCGABS(j), &absAxis);
						config.absAxisMin[config.absAxisCount] = absAxis.minimum;
						config.absAxisMax[config.absAxisCount] = absAxis.maximum;
						config.absAxisFuzz[config.absAxisCount] = absAxis.fuzz;
						config.absAxisFlat[config.absAxisCount] = absAxis.flat;
						config.absAxisResolution[config.absAxisCount] = 100;
						
						config.absAxis[config.absAxisCount] = j;
						
						js_index_map_set(indexMap, i, j, config.absAxisCount);
						
						config.absAxisCount++;						
					}
					else if (i == EV_REL) {
						js_index_map_set(indexMap, i, j, config.relAxisCount);
						config.relAxis[config.relAxisCount++] = j;						
					}
					else if (i == EV_KEY) {						
						js_index_map_set(indexMap, i, j, config.buttonCount);
						config.buttons[config.buttonCount++] = j;
					}				
				}
			}
		}
	}

	// Create the client socket address
	int sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFd < 0) {
		printf("error connecting socket: %d (%s)\n", errno, strerror(errno));
	}

	// Connect to the server
	struct sockaddr_in addr = {};

	addr.sin_family = AF_INET;
	inet_pton(AF_INET, serverAddr_, &(addr.sin_addr));
	addr.sin_port = htons(serverPort_);

	int rc = connect(sockFd,(struct sockaddr *)&addr,sizeof(addr));
	if (rc < 0) {
		printf("error connecting to server: %d (%s)\n", errno, strerror(errno));
		close(sockFd);
	}

	// Send the joystick configuration message to the server
	encode_and_transmit(sockFd, 0, &config, sizeof(config));

	// Wait for input on the open file descriptor.  Update local report 
	// structure when new events come in, and send report to server when
	// we get a SYN.
	js_report_t report = {};
	size_t rawReportSize = joystick_get_report_size(&config);
	uint8_t* rawReport = (uint8_t*)calloc(1, rawReportSize);

	report.absAxis = (int32_t*)rawReport;
        report.relAxis = (int32_t*)(rawReport + (sizeof(int32_t) * config.absAxisCount));
        report.buttons = (uint8_t*)(rawReport + (sizeof(int32_t) * config.absAxisCount)
                        + (sizeof(int32_t) * config.relAxisCount));
	while(1) {
		// Blocking read on the HID device
		struct input_event events[128];
		int nRead = read(fd, events, sizeof(events));
		if ((nRead == 0) || ((nRead == -1) && !((errno == EAGAIN) || (errno == EINTR)))) {
			printf("client socket died\n");
			break;
		}

		if (nRead < sizeof(struct input_event)) {
			printf("unexpected event size read %d\n", nRead);
		}

		// Data read into buffer;  parase out events
		size_t numEvents = (nRead/sizeof(struct input_event));
		for (size_t i = 0; i < numEvents; i++) {
			// Whenever we get a sync event, flush the current report
			if (events[i].type == EV_SYN) {
				encode_and_transmit(sockFd, 1, rawReport, rawReportSize);
			} else if (events[i].type == EV_KEY) {
				int index = js_index_map_get_index(indexMap, events[i].type, events[i].code);
                                if (index < 0) {
                                        printf("invalid key index \n");
                                        continue;
                                }
                                report.buttons[index] = !!events[i].value;
			} else if (events[i].type == EV_ABS) {
				int index = js_index_map_get_index(indexMap, events[i].type, events[i].code);
                                if (index < 0) {
                                        printf("invalid absAxis index \n");
                                        continue;
                                }
                                report.absAxis[index] = events[i].value;
			} else if (events[i].type == EV_REL) {
				int index = js_index_map_get_index(indexMap, events[i].type, events[i].code);
                                if (index < 0) {
                                        printf("invalid relAxis index \n");
                                        continue;
                                }
                                report.relAxis[index] = events[i].value;
			}
		}
	}
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
	jsproxy_client_context_t* newContext = (jsproxy_client_context_t*)(calloc(1, sizeof(jsproxy_client_context_t)));
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
		joystick_destroy(context->joystickContext);
	}
}

//---------------------------------------------------------------------------
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
			context_->configSet = true;
			
		} break;
		case 1: {
			printf("received joystick report message\n");

			if (!context_->configSet || !context_->joystickContext) {
				printf("joystick hasn't been configured.  Bailing\n");
				return;
			}

			js_report_t report;
			js_config_t* config = &context_->joystickContext->config;
			int fd = context_->joystickContext->fd;

			size_t rawReportSize = joystick_get_report_size(config);
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
int main(int argc, void** argv) {
	if (argc < 4) {
		printf("usage: netstick [path to input device] [server address] [server port]\n");
		return -1;
	}
	int id = fork();
	
	if (id != 0) {
		jsproxy_server();
	} else {
		jsproxy_client_uinput(argv[1], argv[2], atoi(argv[3]));
	}
}
