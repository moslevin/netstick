// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif
//---------------------------------------------------------------------------
// Struct used to represent the first elements of the tag-length-value-checksum
// message.
typedef struct __attribute__((packed)) {
    uint16_t tag;
    uint16_t length;
} tlvc_header_t;

//---------------------------------------------------------------------------
// Struct used to represent the first elements of the tag-length-value-checksum
// message.
typedef struct __attribute__((packed)) {
    uint16_t checksum;
} tlvc_footer_t;

//---------------------------------------------------------------------------
// Struct used to represent a tag-length-value-checksum message.
typedef struct {
    tlvc_header_t header;
    tlvc_footer_t footer;
    void*         data;
    size_t        dataLen;
} tlvc_data_t;

//---------------------------------------------------------------------------
/**
 * @brief tlvc_encode_data construct a tlvc object for a payload of data.
 * NOTE: the tlvc object must not outlive the data_ parameter, as it does not
 * duplicate its data.
 * @param tlvc_ [in|out] data structure that is constructured from the argument data
 * @param tag_ value representing the tag type
 * @param dataLen_ length of the payload data in bytes
 * @param data_ payload data to encode
 */
void tlvc_encode_data(tlvc_data_t* tlvc_, uint16_t tag_, size_t dataLen_, void* data_);

//---------------------------------------------------------------------------
/**
 * @brief tlvc_decode_data decode a raw tlvc message into a
 * @param tlvc_ [in|out] data structure that is constructured from the argument data
 * @param data_ pointer to a raw binary blob containing tlvc encoded payload
 * @param dataLen_ size of the data_ blob in bytes
 * @return true if the data stream was successfully decoded from the source data
 */
bool tlvc_decode_data(tlvc_data_t* tlvc_, void* data_, size_t dataLen_);

#if defined(__cplusplus)
} // extern "C"
#endif
