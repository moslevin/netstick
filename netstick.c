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

	int toWrite = encode->index;
	int nWritten = 0;
	raw = encode->encoded;


	bool died = false;
	while (toWrite > 0) {
		nWritten = write(sockFd_, raw, toWrite);
		if ((nWritten == 0) ||
			((nWritten == -1) && !((errno == EINTR) || (errno == EAGAIN)))) {
			died = true;
			break;
		}
		toWrite -= nWritten;
		raw += nWritten;
	}

	slip_encode_message_destroy(encode);

	if (died) {
		printf("socket died during write\n");
		return false;
	}

	return true;
}

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
	config.pid = info.pid;
	config.vid = info.vid;

	// Get the device's name
	char devName[256];
	ioctl(fd, EVIOCGNAME(sizeof(devName)), devName);

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
		return;
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
	if (!encode_and_transmit(sockFd, 0, &config, sizeof(config))) {
		return;
	}

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
				if (!encode_and_transmit(sockFd, 1, rawReport, rawReportSize)) {
					return;
				}
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
int main(int argc, void** argv) {
	if (argc < 4) {
		printf("usage: netstick [path to input device] [server address] [server port]\n");
		return -1;
	}

	jsproxy_client_uinput(argv[1], argv[2], atoi(argv[3]));
}
