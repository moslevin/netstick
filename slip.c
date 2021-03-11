// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#include "slip.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

//---------------------------------------------------------------------------
slip_encode_message_t* slip_encode_message_create(size_t rawSize_)
{
    slip_encode_message_t* newMessage = (slip_encode_message_t*)(calloc(1, sizeof(slip_encode_message_t)));

    newMessage->encodedSize = (rawSize_ * 2) + 2;
    newMessage->encoded     = (uint8_t*)(calloc(1, newMessage->encodedSize));

    newMessage->index = 0;

    return newMessage;
}

//---------------------------------------------------------------------------
void slip_encode_message_destroy(slip_encode_message_t* msg_)
{
    free(msg_->encoded);
    free(msg_);
}

//---------------------------------------------------------------------------
void slip_encode_begin(slip_encode_message_t* msg_)
{
    msg_->index                  = 0;
    msg_->encoded[msg_->index++] = SLIP_END;
}

//---------------------------------------------------------------------------
slip_encode_return_t slip_encode_finish(slip_encode_message_t* msg_)
{
    if (msg_->index >= msg_->encodedSize) {
        return SlipEncodeErrorTooBig;
    }
    msg_->encoded[msg_->index++] = SLIP_END;
    return SlipEncodeOk;
}

//---------------------------------------------------------------------------
slip_encode_return_t slip_encode_byte(slip_encode_message_t* msg_, uint8_t b_)
{
    if (msg_->index >= msg_->encodedSize) {
        return SlipEncodeErrorTooBig;
    }

    switch (b_) {
        case SLIP_END: {
            msg_->encoded[msg_->index++] = SLIP_ESC;
            if (msg_->index >= msg_->encodedSize) {
                return SlipEncodeErrorTooBig;
            }
            msg_->encoded[msg_->index++] = SLIP_ESC_END;
        } break;
        case SLIP_ESC: {
            msg_->encoded[msg_->index++] = SLIP_ESC;
            if (msg_->index >= msg_->encodedSize) {
                return SlipEncodeErrorTooBig;
            }
            msg_->encoded[msg_->index++] = SLIP_ESC_ESC;
        } break;
        default: {
            msg_->encoded[msg_->index++] = b_;
        } break;
    }
    return SlipEncodeOk;
}

//---------------------------------------------------------------------------
slip_decode_message_t* slip_decode_message_create(size_t rawSize_)
{
    slip_decode_message_t* newMessage = (slip_decode_message_t*)(calloc(1, sizeof(slip_decode_message_t)));

    newMessage->rawSize = (rawSize_);
    newMessage->raw     = (uint8_t*)(calloc(1, newMessage->rawSize));

    newMessage->inEscape = false;
    newMessage->index    = 0;

    return newMessage;
}

//---------------------------------------------------------------------------
void slip_decode_message_destroy(slip_decode_message_t* context_)
{
    free(context_->raw);
    free(context_);
}

//---------------------------------------------------------------------------
void slip_decode_begin(slip_decode_message_t* msg_)
{
    msg_->index = 0;
}

//---------------------------------------------------------------------------
slip_decode_return_t slip_decode_byte(slip_decode_message_t* msg_, uint8_t b_)
{
    if (msg_->index >= msg_->rawSize) {
        return SlipDecodeErrorTooBig;
    }

    switch (b_) {
        case SLIP_END: {
            // end of message
            msg_->inEscape = false;
        }
            return SlipDecodeEndOfFrame;
        case SLIP_ESC: {
            if (msg_->inEscape) {
                return SlipDecodeErrorInvalidFrame;
            }
            msg_->inEscape = true;
        } break;
        case SLIP_ESC_END: {
            if (msg_->inEscape == true) {
                msg_->inEscape           = false;
                msg_->raw[msg_->index++] = SLIP_END;
            } else {
                msg_->raw[msg_->index++] = b_;
            }
        } break;
        case SLIP_ESC_ESC: {
            if (msg_->inEscape == true) {
                msg_->inEscape           = false;
                msg_->raw[msg_->index++] = SLIP_ESC;
            } else {
                msg_->raw[msg_->index++] = b_;
            }
        } break;
        default: {
            if (msg_->inEscape) {
                return SlipDecodeErrorInvalidFrame;
            }
            msg_->raw[msg_->index++] = b_;
        } break;
    }
    return SlipDecodeOk;
}
