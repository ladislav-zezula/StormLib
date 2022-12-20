/*****************************************************************************/
/* explode.c                              Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* Implode function of PKWARE Data Compression library                       */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 11.03.03  1.00  Lad  Splitted from Pkware.cpp                             */
/* 08.04.03  1.01  Lad  Renamed to explode.c to be compatible with pklib     */
/* 02.05.03  1.01  Lad  Stress test done                                     */
/* 22.04.10  1.01  Lad  Documented                                           */
/*****************************************************************************/

#include <assert.h>
#include <string.h>

#include "pklib.h"

#define PKDCL_OK                    0
#define PKDCL_STREAM_END            1   // All data from the input stream is read
#define PKDCL_NEED_DICT             2   // Need more data (dictionary)
#define PKDCL_CONTINUE             10   // Internal flag, not returned to user
#define PKDCL_GET_INPUT            11   // Internal flag, not returned to user

char CopyrightPkware[] = "PKWARE Data Compression Library for Win32\r\n"
                         "Copyright 1989-1995 PKWARE Inc.  All Rights Reserved\r\n"
                         "Patent No. 5,051,745\r\n"
                         "PKWARE Data Compression Library Reg. U.S. Pat. and Tm. Off.\r\n"
                         "Version 1.11\r\n";

//-----------------------------------------------------------------------------
// Local functions

static void GenDecodeTabs(
    unsigned char * positions,           // [out] Table of positions
    const unsigned char * start_indexes, // [in] Table of start indexes
    const unsigned char * length_bits,   // [in] Table of lengths. Each length is stored as number of bits
    size_t elements)                     // [in] Number of elements in start_indexes and length_bits
{
    unsigned int index;
    unsigned int length;
    size_t i;

    for(i = 0; i < elements; i++)
    {
        length = 1 << length_bits[i];   // Get the length in bytes

        for(index = start_indexes[i]; index < 0x100; index += length)
        {
            positions[index] = (unsigned char)i;
        }
    }
}

static void GenAscTabs(TDcmpStruct * pWork)
{
    const unsigned short * pChCodeAsc = &ChCodeAsc[0xFF];
    unsigned int  acc, add;
    unsigned short count;

    for(count = 0x00FF; pChCodeAsc >= ChCodeAsc; pChCodeAsc--, count--)
    {
        unsigned char * pChBitsAsc = pWork->ChBitsAsc + count;
        unsigned char bits_asc = *pChBitsAsc;

        if(bits_asc <= 8)
        {
            add = (1 << bits_asc);
            acc = *pChCodeAsc;

            do
            {
                pWork->offs2C34[acc] = (unsigned char)count;
                acc += add;
            }
            while(acc < 0x100);
        }
        else if((acc = (*pChCodeAsc & 0xFF)) != 0)
        {
            pWork->offs2C34[acc] = 0xFF;

            if(*pChCodeAsc & 0x3F)
            {
                bits_asc -= 4;
                *pChBitsAsc = bits_asc;

                add = (1 << bits_asc);
                acc = *pChCodeAsc >> 4;
                do
                {
                    pWork->offs2D34[acc] = (unsigned char)count;
                    acc += add;
                }
                while(acc < 0x100);
            }
            else
            {
                bits_asc -= 6;
                *pChBitsAsc = bits_asc;

                add = (1 << bits_asc);
                acc = *pChCodeAsc >> 6;
                do
                {
                    pWork->offs2E34[acc] = (unsigned char)count;
                    acc += add;
                }
                while(acc < 0x80);
            }
        }
        else
        {
            bits_asc -= 8;
            *pChBitsAsc = bits_asc;

            add = (1 << bits_asc);
            acc = *pChCodeAsc >> 8;
            do
            {
                pWork->offs2EB4[acc] = (unsigned char)count;
                acc += add;
            }
            while(acc < 0x100);
        }
    }
}

//-----------------------------------------------------------------------------
// Removes given number of bits in the bit buffer. New bits are reloaded from
// the input buffer, if needed.
// Returns: PKDCL_OK:         Operation was successful
//          PKDCL_STREAM_END: There are no more bits in the input buffer

static int WasteBits(TDcmpStruct * pWork, unsigned int nBits)
{
    // If number of bits required is less than number of (bits in the buffer) ?
    if(nBits <= pWork->extra_bits)
    {
        pWork->extra_bits -= nBits;
        pWork->bit_buff  >>= nBits;
        return PKDCL_OK;
    }

    // Load input buffer if necessary
    pWork->bit_buff >>= pWork->extra_bits;
    if(pWork->in_pos == pWork->in_bytes)
    {
        pWork->in_pos = sizeof(pWork->in_buff);
        if((pWork->in_bytes = pWork->read_buf((char *)pWork->in_buff, &pWork->in_pos, pWork->param)) == 0)
            return PKDCL_STREAM_END;
        pWork->in_pos = 0;
    }

    // Update bit buffer
    pWork->bit_buff  |= (pWork->in_buff[pWork->in_pos++] << 8);
    pWork->bit_buff >>= (nBits - pWork->extra_bits);
    pWork->extra_bits = (pWork->extra_bits - nBits) + 8;
    return PKDCL_OK;
}

//-----------------------------------------------------------------------------
// Decodes next literal from the input (compressed) data.
// Returns : 0x000: One byte 0x00
//           0x001: One byte 0x01
//           ...
//           0x0FF: One byte 0xFF
//           0x100: Repetition, length of 0x02 bytes
//           0x101: Repetition, length of 0x03 bytes
//           ...
//           0x304: Repetition, length of 0x206 bytes
//           0x305: End of stream
//           0x306: Error

static unsigned int DecodeLit(TDcmpStruct * pWork)
{
    unsigned int extra_length_bits;    // Number of bits of extra literal length
    unsigned int length_code;          // Length code
    unsigned int value;

    // Test the current bit in byte buffer. If is not set, simply return the next 8 bits.
    if(pWork->bit_buff & 1)
    {
        // Remove one bit from the input data
        if(WasteBits(pWork, 1))
            return 0x306;

        // The next 8 bits hold the index to the length code table
        length_code = pWork->LengthCodes[pWork->bit_buff & 0xFF];

        // Remove the apropriate number of bits
        if(WasteBits(pWork, pWork->LenBits[length_code]))
            return 0x306;

        // Are there some extra bits for the obtained length code ?
        if((extra_length_bits = pWork->ExLenBits[length_code]) != 0)
        {
            unsigned int extra_length = pWork->bit_buff & ((1 << extra_length_bits) - 1);

            if(WasteBits(pWork, extra_length_bits))
            {
                if((length_code + extra_length) != 0x10E)
                    return 0x306;
            }
            length_code = pWork->LenBase[length_code] + extra_length;
        }

        // In order to distinguish uncompressed byte from repetition length,
        // we have to add 0x100 to the length.
        return length_code + 0x100;
    }

    // Remove one bit from the input data
    if(WasteBits(pWork, 1))
        return 0x306;

    // If the binary compression type, read 8 bits and return them as one byte.
    if(pWork->ctype == CMP_BINARY)
    {
        unsigned int uncompressed_byte = pWork->bit_buff & 0xFF;

        if(WasteBits(pWork, 8))
            return 0x306;
        return uncompressed_byte;
    }

    // When ASCII compression ...
    if(pWork->bit_buff & 0xFF)
    {
        value = pWork->offs2C34[pWork->bit_buff & 0xFF];

        if(value == 0xFF)
        {
            if(pWork->bit_buff & 0x3F)
            {
                if(WasteBits(pWork, 4))
                    return 0x306;

                value = pWork->offs2D34[pWork->bit_buff & 0xFF];
            }
            else
            {
                if(WasteBits(pWork, 6))
                    return 0x306;

                value = pWork->offs2E34[pWork->bit_buff & 0x7F];
            }
        }
    }
    else
    {
        if(WasteBits(pWork, 8))
            return 0x306;

        value = pWork->offs2EB4[pWork->bit_buff & 0xFF];
    }

    return WasteBits(pWork, pWork->ChBitsAsc[value]) ? 0x306 : value;
}

//-----------------------------------------------------------------------------
// Decodes the distance of the repetition, backwards relative to the
// current output buffer position

static unsigned int DecodeDist(TDcmpStruct * pWork, unsigned int rep_length)
{
    unsigned int dist_pos_code;            // Distance position code
    unsigned int dist_pos_bits;            // Number of bits of distance position
    unsigned int distance;                 // Distance position

    // Next 2-8 bits in the input buffer is the distance position code
    dist_pos_code = pWork->DistPosCodes[pWork->bit_buff & 0xFF];
    dist_pos_bits = pWork->DistBits[dist_pos_code];
    if(WasteBits(pWork, dist_pos_bits))
        return 0;

    if(rep_length == 2)
    {
        // If the repetition is only 2 bytes length,
        // then take 2 bits from the stream in order to get the distance
        distance = (dist_pos_code << 2) | (pWork->bit_buff & 0x03);
        if(WasteBits(pWork, 2))
            return 0;
    }
    else
    {
        // If the repetition is more than 2 bytes length,
        // then take "dsize_bits" bits in order to get the distance
        distance = (dist_pos_code << pWork->dsize_bits) | (pWork->bit_buff & pWork->dsize_mask);
        if(WasteBits(pWork, pWork->dsize_bits))
            return 0;
    }
    return distance + 1;
}

static unsigned int Expand(TDcmpStruct * pWork)
{
    unsigned int next_literal;         // Literal decoded from the compressed data
    unsigned int result;               // Value to be returned
    unsigned int copyBytes;             // Number of bytes to copy to the output buffer

    pWork->outputPos = 0x1000;          // Initialize output buffer position

    // Decode the next literal from the input data.
    // The returned literal can either be an uncompressed byte (next_literal < 0x100)
    // or an encoded length of the repeating byte sequence that
    // is to be copied to the current buffer position
    while((result = next_literal = DecodeLit(pWork)) < 0x305)
    {
        // If the literal is greater than 0x100, it holds length
        // of repeating byte sequence
        // literal of 0x100 means repeating sequence of 0x2 bytes
        // literal of 0x101 means repeating sequence of 0x3 bytes
        // ...
        // literal of 0x305 means repeating sequence of 0x207 bytes
        if(next_literal >= 0x100)
        {
            unsigned char * source;
            unsigned char * target;
            unsigned int rep_length;       // Length of the repetition, in bytes
            unsigned int minus_dist;       // Backward distance to the repetition, relative to the current buffer position

            // Get the length of the repeating sequence.
            // Note that the repeating block may overlap the current output position,
            // for example if there was a sequence of equal bytes
            rep_length = next_literal - 0xFE;

            // Get backward distance to the repetition
            if((minus_dist = DecodeDist(pWork, rep_length)) == 0)
            {
                result = 0x306;
                break;
            }

            // Target and source pointer
            target = &pWork->out_buff[pWork->outputPos];
            source = target - minus_dist;

            // Update buffer output position
            pWork->outputPos += rep_length;

            // Copy the repeating sequence
            while(rep_length-- > 0)
                *target++ = *source++;
        }
        else
        {
            pWork->out_buff[pWork->outputPos++] = (unsigned char)next_literal;
        }

        // Flush the output buffer, if number of extracted bytes has reached the end
        if(pWork->outputPos >= 0x2000)
        {
            // Copy decompressed data into user buffer
            copyBytes = 0x1000;
            pWork->write_buf((char *)&pWork->out_buff[0x1000], &copyBytes, pWork->param);

            // Now copy the decompressed data to the first half of the buffer.
            // This is needed because the decompression might reuse them as repetitions.
            // Note that if the output buffer overflowed previously, the extra decompressed bytes
            // are stored in "out_buff_overflow", and they will now be
            // within decompressed part of the output buffer.
            memmove(pWork->out_buff, &pWork->out_buff[0x1000], pWork->outputPos - 0x1000);
            pWork->outputPos -= 0x1000;
        }
    }

    // Flush any remaining decompressed bytes
    copyBytes = pWork->outputPos - 0x1000;
    pWork->write_buf((char *)&pWork->out_buff[0x1000], &copyBytes, pWork->param);
    return result;
}


//-----------------------------------------------------------------------------
// Main exploding function.

unsigned int PKEXPORT explode(
        unsigned int (*read_buf)(char *buf, unsigned  int *size, void *param),
        void         (*write_buf)(char *buf, unsigned  int *size, void *param),
        char         *work_buf,
        void         *param)
{
    TDcmpStruct * pWork = (TDcmpStruct *)work_buf;

    // Initialize work struct and load compressed data
    pWork->read_buf   = read_buf;
    pWork->write_buf  = write_buf;
    pWork->param      = param;
    pWork->in_pos     = sizeof(pWork->in_buff);
    pWork->in_bytes   = pWork->read_buf((char *)pWork->in_buff, &pWork->in_pos, pWork->param);
    if(pWork->in_bytes <= 4)
        return CMP_BAD_DATA;

    pWork->ctype      = pWork->in_buff[0]; // Get the compression type (CMP_BINARY or CMP_ASCII)
    pWork->dsize_bits = pWork->in_buff[1]; // Get the dictionary size
    pWork->bit_buff   = pWork->in_buff[2]; // Initialize 16-bit bit buffer
    pWork->extra_bits = 0;                 // Extra (over 8) bits
    pWork->in_pos     = 3;                 // Position in input buffer

    // Test for the valid dictionary size
    if(4 > pWork->dsize_bits || pWork->dsize_bits > 6)
        return CMP_INVALID_DICTSIZE;

    pWork->dsize_mask = 0xFFFF >> (0x10 - pWork->dsize_bits); // Shifted by 'sar' instruction

    if(pWork->ctype != CMP_BINARY)
    {
        if(pWork->ctype != CMP_ASCII)
            return CMP_INVALID_MODE;

        memcpy(pWork->ChBitsAsc, ChBitsAsc, sizeof(pWork->ChBitsAsc));
        GenAscTabs(pWork);
    }

    memcpy(pWork->LenBits, LenBits, sizeof(pWork->LenBits));
    GenDecodeTabs(pWork->LengthCodes, LenCode, LenBits, sizeof(LenBits));
    memcpy(pWork->ExLenBits, ExLenBits, sizeof(pWork->ExLenBits));
    memcpy(pWork->LenBase, LenBase, sizeof(pWork->LenBase));
    memcpy(pWork->DistBits, DistBits, sizeof(pWork->DistBits));
    GenDecodeTabs(pWork->DistPosCodes, DistCode, DistBits, sizeof(DistBits));
    if(Expand(pWork) != 0x306)
        return CMP_NO_ERROR;

    return CMP_ABORT;
}
