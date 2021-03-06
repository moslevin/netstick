#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
#define SLIP_END 		((uint8_t)(0xC0))
#define SLIP_ESC		((uint8_t)(0xDB))
#define SLIP_ESC_END	((uint8_t)(0xDC))
#define SLIP_ESC_ESC	((uint8_t)(0xDD))

//---------------------------------------------------------------------------
typedef enum {
	SlipEncodeOk = 0,
	SlipEncodeErrorTooBig,	
} slip_encode_return_t;

//---------------------------------------------------------------------------
typedef struct {
	uint8_t* encoded;
	size_t 	 encodedSize;
	
	size_t	 index;
} slip_encode_message_t;

//---------------------------------------------------------------------------
typedef enum {
	SlipDecodeOk = 0,
	SlipDecodeErrorTooBig,
	SlipDecodeErrorInvalidFrame,
	SlipDecodeEndOfFrame
} slip_decode_return_t;

//---------------------------------------------------------------------------
typedef struct {
	uint8_t* raw;
	size_t 	 rawSize;
	
	bool	 inEscape;
	size_t	 index;
} slip_decode_message_t;

//---------------------------------------------------------------------------
slip_encode_message_t* slip_encode_message_create(size_t rawSize_);

//---------------------------------------------------------------------------
void slip_encode_message_destroy(slip_encode_message_t* msg_);

//---------------------------------------------------------------------------
void slip_encode_begin(slip_encode_message_t* msg_);

//---------------------------------------------------------------------------
slip_encode_return_t slip_encode_finish(slip_encode_message_t* msg_);

//---------------------------------------------------------------------------
slip_encode_return_t slip_encode_byte(slip_encode_message_t* msg_, uint8_t b_);

//---------------------------------------------------------------------------
slip_decode_message_t* slip_decode_message_create(size_t rawSize_);

//---------------------------------------------------------------------------
void slip_decode_message_destroy(slip_decode_message_t* context_);

//---------------------------------------------------------------------------
void slip_decode_begin(slip_decode_message_t* msg_);

//---------------------------------------------------------------------------
slip_decode_return_t slip_decode_byte(slip_decode_message_t* msg_, uint8_t b_);

#if defined(__cplusplus)
} // extern "C"
#endif

