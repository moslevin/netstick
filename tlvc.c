// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#include "tlvc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//---------------------------------------------------------------------------
void tlvc_encode_data(tlvc_data_t* tlvc_, uint16_t tag_, size_t dataLen_, void* data_)
{
    tlvc_->header.tag    = tag_;
    tlvc_->header.length = dataLen_;

    tlvc_->data    = data_;
    tlvc_->dataLen = dataLen_;

    // Compute checksum and add it to the footer
    uint16_t checksum = 0;
    uint8_t* raw      = (uint8_t*)(&tlvc_->header);
    for (size_t i = 0; i < sizeof(tlvc_header_t); i++) { checksum += raw[i]; }
    raw = (uint8_t*)(data_);
    for (size_t i = 0; i < dataLen_; i++) { checksum += raw[i]; }

    tlvc_->footer.checksum = checksum;
}

//---------------------------------------------------------------------------
bool tlvc_decode_data(tlvc_data_t* tlvc_, void* data_, size_t dataLen_)
{
    tlvc_header_t* header = (tlvc_header_t*)data_;

    // Can't decode a tlvc structure if the raw data size is < header + footer.
    if (dataLen_ < (sizeof(tlvc_footer_t) + sizeof(tlvc_header_t))) {
        return false;
    }

    // Verify payload is the same size as specified in the header
    if (header->length != (dataLen_ - sizeof(tlvc_footer_t) - sizeof(tlvc_header_t))) {
        return false;
    }

    // Compute + verify the message/header checksum
    uint8_t* rawData  = data_;
    uint16_t checksum = 0;
    for (size_t i = 0; i < dataLen_ - 2; i++) { checksum += rawData[i]; }

    tlvc_footer_t* footer = (tlvc_footer_t*)(data_ + sizeof(tlvc_header_t) + header->length);
    if (footer->checksum != checksum) {
        return false;
    }

    tlvc_->header  = *header;
    tlvc_->footer  = *footer;
    tlvc_->data    = ((uint8_t*)data_) + sizeof(tlvc_header_t);
    tlvc_->dataLen = header->length;

    return true;
}
