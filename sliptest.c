#include "slip.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {

	srand(time(NULL));

	slip_encode_t* encode = slip_encode_message_create(64);

	while (1) {

		slip_encode_begin(encode);
		for (int i = 0; i < 64; i++) {
			slip_encode_byte(encode, rand());
		}
		slip_encode_finish(encode);

		slip_decode_t* decode = slip_decode_message_create(64);

		slip_decode_begin(decode);

		for (int i = 0; i < encode->index; i++) {
			if (slip_decode_byte(decode, encode->encoded[i]);
		}

		

		slip_decode_message_destroy(decode);
	}

	return 0;
}
