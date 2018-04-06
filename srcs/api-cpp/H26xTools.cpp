/*
 * This file is part of Nokia HEIF library
 *
 * Copyright (c) 2015-2018 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 *
 * Contact: heif@nokia.com
 *
 * This software, including documentation, is protected by copyright controlled by Nokia Corporation and/ or its
 * subsidiaries. All rights are reserved. Copying, including reproducing, storing, adapting or translating, any or all
 * of this material requires the prior written consent of Nokia.
 */

#include "H26xTools.h"
using namespace HEIFPP;
/*
Byte stream parsing  Rec. ITU-T H.265 v4 (12/2016)  - annex B (matches the H.264 too)

Input to this process consists of an ordered stream of bytes consisting of a sequence of byte stream NAL unit syntax
structures.

Output of this process consists of a sequence of NAL unit syntax structures.

At the beginning of the decoding process, the decoder initializes its current position in the byte stream to the
beginning of the byte stream.

*/
bool NAL_State::init_parse(const uint8_t* aData, uint64_t aLength)
{
    /*
    At the beginning of the decoding process, the decoder initializes its current position in the byte stream to the
    beginning of the byte stream.
    */
    mData   = aData;
    mLength = aLength;
    /*
    It then extracts and discards each leading_zero_8bits syntax element(when present),
    moving the current position in the byte stream forward one byte at a time,
    until the current position in the byte stream is such that the next four bytes in the bitstream form the four - byte
    sequence 0x00000001.*/
    while (mLength >= 4)
    {
        if ((mData[0] == 0) && (mData[1] == 0) && (mData[2] == 0) && (mData[3] == 1))
        {
            return true;
        }
        if (mData[0] != 0)
            break;
        mData++;
        mLength--;
    }
    // invalid bytestream.
    return false;
}
bool NAL_State::parse_byte_stream(const uint8_t*& nal_unit, uint64_t& nal_unit_length)
{
    /*
    The decoder then performs the following step-wise process repeatedly to extract and decode each NAL unit syntax
    structure in the byte stream until the end of the byte stream has been encountered  (as determined by unspecified
    means and the last NAL unit in the byte stream has been decoded:
    */


    /*1. When the next four bytes in the bitstream form the four-byte sequence 0x00000001,
    the next byte in the byte stream (which is a zero_byte syntax element) is extracted
    and discarded and the current position in the byte stream is set equal to the position
    of the byte following this discarded byte.
    */
    if ((mLength >= 4) && ((mData[0] == 0) && (mData[1] == 0) && (mData[2] == 0) && (mData[3] == 1)))
    {
        // discard zero byte
        /*
        When one or more of the following conditions are true, the zero_byte syntax element shall be present:
        H265
        � The nal_unit_type within the nal_unit( ) syntax structure is equal to VPS_NUT, SPS_NUT or PPS_NUT.
        � The byte stream NAL unit syntax structure contains the first NAL unit of an access unit in decoding order, as
        specified in clause 7.4.2.4.4. H264 � The nal_unit_type within the nal_unit( ) is equal to 7 (sequence parameter
        set) or 8 (picture parameter set), � The byte stream NAL unit syntax structure contains the first NAL unit of an
        access unit in decoding order, as specified in clause 7.4.1.2.3.
        */
        // Currently not validating that the nal unit is of type that CAN have the zero byte.
        mLength--;
        mData++;
    }
    if (mLength < 3)
    {
        // end of stream.
        nal_unit        = nullptr;
        nal_unit_length = 0;
        return false;
    }
    /*2. The next three-byte sequence in the byte stream (which is a start_code_prefix_one_3bytes)
    is extracted and discarded and the current position in the byte stream is set equal to
    the position of the byte following this three-byte sequence.
    */
    if ((mData[0] == 0) && (mData[1] == 0) && (mData[2] == 1))
    {
        // skip start_code_prefix
        mLength -= 3;
        mData += 3;
    }
    else
    {
        // corrupted stream.
        nal_unit        = nullptr;
        nal_unit_length = 0;
        return false;
    }

    /*3. NumBytesInNalUnit is set equal to the number of bytes starting with the byte at the current
    position in the byte stream up to and including the last byte that precedes the location of
    one or more of the following conditions:
    � A subsequent byte-aligned three-byte sequence equal to 0x000000,
    � A subsequent byte-aligned three-byte sequence equal to 0x000001,
    � The end of the byte stream, as determined by unspecified means.
    */
    const uint8_t* src  = mData;
    uint64_t bytes_left = mLength;
    while (bytes_left)
    {
        if ((bytes_left >= 3) && ((src[0] == 0) && (src[1] == 0) && ((src[2] == 0) || (src[2] == 1))))
        {
            break;
        }
        src++;
        bytes_left--;
    }
    /*4. NumBytesInNalUnit bytes are removed from the bitstream and the current position in the byte
    stream is advanced by NumBytesInNalUnit bytes.
    This sequence of bytes is nal_unit( NumBytesInNalUnit ) and is decoded using the NAL unit decoding process.
    */
    nal_unit        = mData;
    nal_unit_length = (uint64_t)(src - mData);
    mData += nal_unit_length;
    mLength -= nal_unit_length;


    /*5. When the current position in the byte stream is not at the end of the byte stream (as determined by unspecified
    means) and the next bytes in the byte stream do not start with a three-byte sequence equal to 0x000001 and the next
    bytes in the byte stream do not start with a four byte sequence equal to 0x00000001 ,the decoder extracts and
    discards each trailing_zero_8bits syntax element, moving the current position in the byte stream forward one byte at
    a time, until the current position in the byte stream is such that the next bytes in the byte stream form the
    fourbyte sequence 0x00000001 or the end of the byte stream has been encountered (as determined by unspecified
    means).
    */
    if ((mLength >= 3) && ((mData[0] == 0) && (mData[1] == 0) && (mData[2] == 1)))
    {
        return true;
    }
    if ((mLength >= 4) && ((mData[0] == 0) && (mData[1] == 0) && (mData[2] == 0) && (mData[3] == 1)))
    {
        return true;
    }

    /*
    while( more_data_in_byte_stream( )  &&  next_bits( 24 )  !=  0x000001  && next_bits( 32 )  !=  0x00000001 )
    trailing_zero_8bits  // equal to 0x00 f(8)
    */
    while (mLength)
    {
        if ((mLength >= 4) && ((mData[0] == 0) && (mData[1] == 0) && (mData[2] == 0) && (mData[3] == 1)))
        {
            break;
        }
        if (mData[0] != 0)
        {
            // corrupted stream!
            return false;
        }
        mData++;
        mLength--;
    }
    return true;
}

bool NAL_State::end_of_stream()
{
    if (mLength == 0)
        return true;
    return false;
}
