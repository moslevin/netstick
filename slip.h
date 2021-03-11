// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
// Binary constants that implement the escape characters used in slip encoding
//---------------------------------------------------------------------------
#define SLIP_END ((uint8_t)(0xC0))
#define SLIP_ESC ((uint8_t)(0xDB))
#define SLIP_ESC_END ((uint8_t)(0xDC))
#define SLIP_ESC_ESC ((uint8_t)(0xDD))

//---------------------------------------------------------------------------
// Return values for encoding operations
typedef enum {
    SlipEncodeOk = 0,       //!< Operation completed successfully
    SlipEncodeErrorTooBig,  //!< Encoding failed because the message was too large
    SlipEncodeErrorMessageComplete //!< Encoding failed because the message frame was already complete
} slip_encode_return_t;

//---------------------------------------------------------------------------
// Data structure used for encoding data into slip frames
typedef struct {
    uint8_t* encoded;       //!< pointer to the buffer that holds the encoded frame
    size_t   encodedSize;   //!< Size of the buffer allocated for the encoded frame

    size_t index;           //!< Current write-index of the buffer / size of the encoded frame (if complete)
} slip_encode_message_t;

//---------------------------------------------------------------------------
// Return values for decoding operations
typedef enum {
    SlipDecodeOk = 0,       //!< Operation completed successfully
    SlipDecodeErrorTooBig,  //!< Decoding failed due to the frame being too large
    SlipDecodeErrorInvalidFrame, //!< Decoding failed as a result of invalid framing bytes / escape sequences
    SlipDecodeEndOfFrame    //!< Decoder recognized an end-of-frame condition
} slip_decode_return_t;

//---------------------------------------------------------------------------
// Data structure used for slip frame-decoding operations.
typedef struct {
    uint8_t* raw;       //!< pointer to the buffer holding the decoded frame
    size_t   rawSize;   //!< Size of the buffer allocated for the decoded frame

    bool   inEscape;    //!< Indicates whether or not the message decoder is decoding an escape character
    size_t index;       //!< Current write index in the buffer / size of the decoded frame (if complete)
} slip_decode_message_t;

//---------------------------------------------------------------------------
/**
 * @brief slip_encode_message_create construct a new slip_encode_message_t
 * object with a raw data size large enough to satisfy a message of size
 * rawSize_
 * @param rawSize_ largest un-encoded message size that this object will need
 * to hold.
 * @return newly-constructured message object, or NULL on allocation error
 */
slip_encode_message_t* slip_encode_message_create(size_t rawSize_);

//---------------------------------------------------------------------------
/**
 * @brief slip_encode_message_destroy destruct a previously-constructed
 * slip_encode_t object, freeing its help resources.
 * NOTE: object must not be used after this is called.
 * @param msg_ message to destroy
 */
void slip_encode_message_destroy(slip_encode_message_t* msg_);

//---------------------------------------------------------------------------
/**
 * @brief slip_encode_begin prepare the object to encode a new frame.  Resets
 * and invalidates any previously-held data.
 * @param msg_ message to initialize for
 */
void slip_encode_begin(slip_encode_message_t* msg_);

//---------------------------------------------------------------------------
/**
 * @brief slip_encode_finish indicate that the object is finished being
 * encoded, and complete the frame.
 * @param msg_ message to complete framing
 * @return SlipEncodeOk on success, others on errors.
 */
slip_encode_return_t slip_encode_finish(slip_encode_message_t* msg_);

//---------------------------------------------------------------------------
/**
 * @brief slip_encode_byte encode a byte of data into an in-progress frame
 * @param msg_ message to append
 * @param b_ data to encode into the frame
 * @return SlipEncodeOk on success, others on errors.
 */
slip_encode_return_t slip_encode_byte(slip_encode_message_t* msg_, uint8_t b_);

//---------------------------------------------------------------------------
/**
 * @brief slip_decode_message_create construct an object used to process and
 * de-frame slip-encoded data streams.
 * @param rawSize_ Maximum size of a framed message to be considered in
 * processing.
 * @return newly-constructed object on success, NULL on error
 */
slip_decode_message_t* slip_decode_message_create(size_t rawSize_);

//---------------------------------------------------------------------------
/**
 * @brief slip_decode_message_destroy destruct a previously-constructed
 * slip_decode_data_t object.
 * NOTE: object must not be used after it has been destroyed.
 * @param context_ object to destroy.
 */
void slip_decode_message_destroy(slip_decode_message_t* context_);

//---------------------------------------------------------------------------
/**
 * @brief slip_decode_begin reset the message frame object, indicating it
 * is ready to begin decoding the next frame.
 * @param msg_ message to decode
 */
void slip_decode_begin(slip_decode_message_t* msg_);

//---------------------------------------------------------------------------
/**
 * @brief slip_decode_byte process a byte of data from a stream, and store
 * the processed result within the slip_decode_message_t object
 * @param msg_ message to hold the decoded data
 * @param b_ byte to decode
 * @return  SlipDecodeOk on scuess, others on error.
 */
slip_decode_return_t slip_decode_byte(slip_decode_message_t* msg_, uint8_t b_);

#if defined(__cplusplus)
} // extern "C"
#endif
