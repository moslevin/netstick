#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif
//---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
	uint16_t tag;
	uint16_t length;	
} tlvc_header_t;

//---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
	uint16_t checksum;
} tlvc_footer_t;

//---------------------------------------------------------------------------
typedef struct {
	tlvc_header_t header;	
	tlvc_footer_t footer;
	void* 	data;
	size_t dataLen;
} tlvc_data_t;

//---------------------------------------------------------------------------
void tlvc_encode_data(tlvc_data_t* tlvc_, uint16_t tag_, size_t dataLen_, void* data_);

//---------------------------------------------------------------------------
bool tlvc_decode_data(tlvc_data_t* tlv, void* data_, size_t dataLen_);

#if defined(__cplusplus)
} // extern "C"
#endif