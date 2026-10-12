/*****************************************************************************/
/* w3xe_support.c                         Copyright (c) Ladislav Zezula 2026 */
/*---------------------------------------------------------------------------*/
/* Handler for W3XE files (Warcraft III maps encrypted by AES-256)           */
/* Credits:  https://github.com/mythic-p/w3xd-toolkit                        */
/*                                                                           */
/* #included by FileStream.cpp, do not put into the project.                 */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 20.09.26  1.00  Lad  Created                                              */
/*****************************************************************************/

//-----------------------------------------------------------------------------
// Local structures

#define W3XE_MIN_HEADER_SIZE    0x6Cu
#define W3XE_TAIL_ZEROS_OFFS    0x16u       // Offset of zeros in the packed file tail
#define W3XE_TAIL_ZEROS_LEN     0x16u       // Length of zeros in the packed file tail
#define W3XE_TAIL_SIZE_PACKED   0x40u       // Size of the packed file tail

// A 32x32 bit matrix over GF(2):
// row[i] is a bitmask over the 32 input bits that XOR together to form output bit i
typedef struct _MATRIX_32x32
{
    DWORD d[32];
} MATRIX_32x32, *PMATRIX_32x32;

// Structure of the encrypted map tail (unpacked, aligned)
typedef struct _W3XE_TAIL
{
    BYTE version;
    BYTE flags;
    USHORT tail_size;
    USHORT header_size;
    ULONGLONG payload_size;
    ULONGLONG license;
    BYTE zeros[W3XE_TAIL_ZEROS_LEN];
} W3XE_TAIL, *PW3XE_TAIL;

//-----------------------------------------------------------------------------
// Portable bit-scan/popcount helpers (no compiler builtins, so this also
// builds under MSVC, which lacks __builtin_ctz/__builtin_clz/__builtin_popcount).
// Only ever called on the GF(2) solver's small setup phase, never per byte
// of the payload, so the naive loops below cost nothing measurable.

static int ctz32(DWORD x)
{
    int n = 0;
    while((x & 1u) == 0u)
    {
        x >>= 1;
        n++;
    }
    return n;
}

static int clz32(DWORD x)
{
    int n = 0;

    while((x & 0x80000000u) == 0u)
    {
        x <<= 1;
        n++;
    }
    return n;
}

static int popcount32(DWORD x)
{
    int n = 0;

    while(x)
    {
        n += (int)(x & 1u);
        x >>= 1;
    }
    return n;
}

//-----------------------------------------------------------------------------
// Support for the xor-stream

static DWORD w3xe_xs_step(unsigned int seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static void w3xe_xs_crypt(unsigned char * out, const unsigned char * in, size_t length, unsigned int seed)
{
    for(size_t i = 0; i < length; i++)
    {
        seed = w3xe_xs_step(seed);
        out[i] = in[i] ^ (unsigned char)(seed & 0xFF);
    }
}

//-----------------------------------------------------------------------------
// Support for 32x32 matrix and finding out the initial XORstream seed

static void matrix_identity(MATRIX_32x32 & m)
{
    for(int i = 0; i < 32; i++)
    {
        m.d[i] = 1u << i;
    }
}

// The matrix for a single xorshift step
static void matrix_base(MATRIX_32x32 & base)
{
    memset(&base, 0, sizeof(MATRIX_32x32));

    for(int j = 0; j < 32; j++)
    {
        DWORD v = w3xe_xs_step(1u << j);

        for(int i = 0; i < 32; i++)
        {
            if((v >> i) & 1u)
            {
                base.d[i] |= 1u << j;
            }
        }
    }
}

// Composition: apply `a` first, then `b` (out[i] = XOR_{j in b[i]} a[j]).
static void matrix_mul(const MATRIX_32x32 & a, const MATRIX_32x32 & b, MATRIX_32x32 & out)
{
    for(int i = 0; i < 32; i++)
    {
        DWORD m = b.d[i];
        DWORD r = 0;

        while(m)
        {
            DWORD lsb = m & (~m + 1u);
            int idx = ctz32(lsb);

            r ^= a.d[idx];
            m &= m - 1u;
        }
        out.d[i] = r;
    }
}

// Matrix representing "xorshift step applied n times", i.e. S[n] as a
// linear function of the initial state S[0].
static void matrix_pow(MATRIX_32x32 & out, ULONGLONG step_count)
{
    MATRIX_32x32 base_e;
    MATRIX_32x32 tmp;
    MATRIX_32x32 r;

    matrix_identity(r);
    matrix_base(base_e);

    while(step_count)
    {
        if(step_count & 1)
        {
            matrix_mul(base_e, r, tmp);
            memcpy(&r, &tmp, sizeof(MATRIX_32x32));
        }
        matrix_mul(base_e, base_e, tmp);
        memcpy(&base_e, &tmp, sizeof(MATRIX_32x32));
        step_count >>= 1;
    }
    memcpy(&out, &r, sizeof(MATRIX_32x32));
}

// Find the initial xorshift seed from the trailer's 22 known zero bytes.
// Returns false if the constraints are inconsistent (not a .w3xd container)
bool w3xe_get_xorstream_seed(const unsigned char * cipher_text, size_t cipher_len, DWORD * xorstream_seed)
{
    MATRIX_32x32 matrix;
    DWORD piv_row[32];
    size_t w3xe_tail_offs = cipher_len - W3XE_TAIL_SIZE_PACKED;
    DWORD xor_seed = 0;
    int piv_rhs[32];
    int piv_used[32] = { 0 };

    // Check the minimum required size
    if(cipher_len < W3XE_MIN_HEADER_SIZE + W3XE_TAIL_SIZE_PACKED)
        return false;

    // Build the matrix
    for(size_t j = 0; j < W3XE_TAIL_ZEROS_LEN; j++)
    {
        size_t pos = w3xe_tail_offs + W3XE_TAIL_ZEROS_OFFS + j;

        matrix_pow(matrix, pos + 1);                // low byte of S[pos+1]
        unsigned char ct = cipher_text[pos];
        for(int bit = 0; bit < 8; bit++)
        {
            DWORD row = matrix.d[bit];
            int rhs = (ct >> bit) & 1;
            bool consumed = false;

            while(row)
            {
                int p = 31 - clz32(row);
                if(piv_used[p]) {
                    row ^= piv_row[p];
                    rhs ^= piv_rhs[p];
                }
                else
                {
                    piv_row[p] = row;
                    piv_rhs[p] = rhs;
                    piv_used[p] = 1;
                    consumed = true;
                    break;
                }
            }
            if(!consumed && rhs)
                return false;           // inconsistent -> not a .w3xd file
        }
    }

    for(int p = 0; p < 32; p++)
    {
        if(!piv_used[p])
            continue;
        DWORD masked = xor_seed & (piv_row[p] & ~(1u << p));
        int parity = popcount32(masked) & 1;

        if(piv_rhs[p] ^ parity)
            xor_seed |= 1u << p;
    }

    // Give out the xorstream seed
    if(xorstream_seed != NULL)
        xorstream_seed[0] = xor_seed;
    return true;
}

//-----------------------------------------------------------------------------
// Derive the raw key for AES-256

#define W3ME_SALT_0     0x30534541454d3357ULL       // "W3MEAES0"
#define W3ME_SALT_1     0x31534541454d3357ULL       // "W3MEAES1"

static void SHA1(ULONGLONG license, ULONGLONG salt, unsigned char * hash)
{
    hash_state sha1_state;
    ULONGLONG KeyBuffer[2] = { license, salt };

    sha1_init(&sha1_state);
    sha1_process(&sha1_state, (unsigned char *)(KeyBuffer), sizeof(KeyBuffer));
    sha1_done(&sha1_state, hash);
}

static void w3xe_derive_raw_key(ULONGLONG license, unsigned char * key)
{
    SHA1(license, W3ME_SALT_0, key);
    SHA1(license, W3ME_SALT_1, key + 20);
}

//-----------------------------------------------------------------------------
// Brute-forcing the n-once

#define AES_NONCE_DEF_BYTE0 0x04
#define AES_NONCE_CTR_LEN   0x05
#define NONCE_LEN (15 - AES_NONCE_CTR_LEN)  // 10

static void w3xe_make_counter_block(
    unsigned char counter_block[AES_BLOCK_SIZE],
    unsigned char the_first_byte,
    const unsigned char * nonce,
    ULONGLONG counter)
{
    // Put the first byte
    counter_block[0x00] = the_first_byte;

    // Put the nonce_base to the next 10 bytes
    counter_block[0x01] = nonce[0];
    counter_block[0x02] = nonce[1];
    counter_block[0x03] = nonce[2];
    counter_block[0x04] = nonce[3];
    counter_block[0x05] = nonce[4];
    counter_block[0x06] = nonce[5];
    counter_block[0x07] = nonce[6];
    counter_block[0x08] = nonce[7];
    counter_block[0x09] = nonce[8];
    counter_block[0x0A] = nonce[9];

    // Put the counter to the last 5 bytes (big endian)
    counter_block[0x0B] = (unsigned char)((counter >> 0x20) & 0xFF);
    counter_block[0x0C] = (unsigned char)((counter >> 0x18) & 0xFF);
    counter_block[0x0D] = (unsigned char)((counter >> 0x10) & 0xFF);
    counter_block[0x0E] = (unsigned char)((counter >> 0x08) & 0xFF);
    counter_block[0x0F] = (unsigned char)((counter >> 0x00) & 0xFF);
}

static void w3xe_make_b0_block(unsigned char b0_block[AES_BLOCK_SIZE], const unsigned char * nonce, ULONGLONG counter)
{
    unsigned char the_first_byte = (unsigned char)(((AES_BLOCK_SIZE - 2) / 2) << 3) | AES_NONCE_DEF_BYTE0;

    w3xe_make_counter_block(b0_block, the_first_byte, nonce, counter);
}

static void w3xe_aes_decrypt_block(
    symmetric_key & aes_key,
    const unsigned char * nonce,
    const unsigned char * in,
    unsigned char * out,
    ULONGLONG counter,
    size_t length)
{
    unsigned char counter_block[AES_BLOCK_SIZE];
    unsigned char key_schedule[AES_BLOCK_SIZE];

    w3xe_make_counter_block(counter_block, AES_NONCE_DEF_BYTE0, nonce, counter);
    aes_desc.ecb_encrypt(counter_block, key_schedule, &aes_key);

    length = STORMLIB_MIN(AES_BLOCK_SIZE, length);
    for(size_t i = 0; i < length; i++)
    {
        out[i] = in[i] ^ key_schedule[i];
    }
}

static bool w3xe_find_nonce_offset(
    symmetric_key & aes_key,
    const unsigned char * layer_data,
    size_t layer_data_length,
    size_t header_length,
    size_t scan_length,
    size_t * out_nonce_offset)
{
    const unsigned char * payload = layer_data + header_length;
    size_t payload_length = layer_data_length - header_length;

    assert(scan_length > header_length);
    assert(payload_length >= 0x10);
    STORMLIB_UNUSED(payload_length);

    if(header_length >= NONCE_LEN)
    {
        size_t search_limit = STORMLIB_MIN(header_length, scan_length) - NONCE_LEN;

        for(size_t nonce_offset = 0; nonce_offset <= search_limit; nonce_offset++)
        {
            const unsigned char * nonce = layer_data + nonce_offset;
            unsigned int aes_block[4];

            w3xe_aes_decrypt_block(aes_key, nonce, payload, (unsigned char *)(aes_block), 1, sizeof(aes_block));
            if(aes_block[0] == ID_MPQ || aes_block[0] == 0x574D3348)
            {
                if(out_nonce_offset != NULL)
                    out_nonce_offset[0] = nonce_offset;
                return true;
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// Decrypting the W3XE payload

static void w3xe_ctr_crypt(
    symmetric_key & aes_key,
    const unsigned char * nonce,
    const unsigned char * in, 
    unsigned char * out,
    ULONGLONG start_offset, 
    size_t total_length)
{
    for(size_t offset = 0; offset < total_length; offset += AES_BLOCK_SIZE)
    {
        size_t block_length = STORMLIB_MIN((total_length - offset), AES_BLOCK_SIZE);
        size_t block_index = (offset / AES_BLOCK_SIZE);

        w3xe_aes_decrypt_block(aes_key, nonce, in, out, start_offset + block_index, block_length);
        out += AES_BLOCK_SIZE;
        in += AES_BLOCK_SIZE;
    }
}

// CBC-MAC over the *plaintext* (CCM authenticates M, then encrypts it)
static void w3xe_create_hmac(
    symmetric_key & aes_key,
    const unsigned char * nonce,
    const unsigned char * message,
    size_t message_length,
    unsigned char out_hmac[AES_BLOCK_SIZE])
{
    unsigned char b0_block[AES_BLOCK_SIZE];
    unsigned char key_schedule[AES_BLOCK_SIZE];

    w3xe_make_b0_block(b0_block, nonce, message_length);
    aes_desc.ecb_encrypt(b0_block, key_schedule, &aes_key);

    for(size_t offset = 0; offset < message_length; offset += AES_BLOCK_SIZE)
    {
        unsigned char xored[16] = { 0 };
        unsigned char blk[16] = { 0 };
        size_t block_length = message_length - offset;

        block_length = STORMLIB_MIN(AES_BLOCK_SIZE, block_length);
        memcpy(blk, message + offset, block_length);

        for(int i = 0; i < AES_BLOCK_SIZE; i++)
            xored[i] = key_schedule[i] ^ blk[i];
        aes_desc.ecb_encrypt(xored, key_schedule, &aes_key);
    }
    memcpy(out_hmac, key_schedule, AES_BLOCK_SIZE);
}


static void w3xe_make_tag(
    symmetric_key & aes_key,
    const unsigned char * nonce,
    const unsigned char * data,
    size_t length,
    unsigned char out_tag[AES_BLOCK_SIZE])
{
    unsigned char hmac_hash[AES_BLOCK_SIZE];

    w3xe_create_hmac(aes_key, nonce, data, length, hmac_hash);
    w3xe_aes_decrypt_block(aes_key, nonce, hmac_hash, out_tag, 0, AES_BLOCK_SIZE);
}

// Decrypt and verify; returns 1 true success (writes `length` bytes to `out`), false on a tag mismatch
static bool w3xe_ccm_decrypt(
    symmetric_key & aes_key,
    const unsigned char * nonce,
    const unsigned char tag[16],
    const unsigned char * in,
    unsigned char * out,
    size_t length)
{
    unsigned char expected_tag[AES_BLOCK_SIZE];

    w3xe_ctr_crypt(aes_key, nonce, in, out, 1, length);
    w3xe_make_tag(aes_key, nonce, out, length, expected_tag);
    return memcmp(expected_tag, tag, sizeof(expected_tag)) == 0;
}

bool w3xe_decrypt_payload(
    const W3XE_TAIL & FileTail,
    symmetric_key & aes_key,
    const unsigned char * layer_data,
    size_t layer_data_length,
    size_t nonce_offset,
    unsigned char ** out_plain_text)
{
    const unsigned char * payload_ct;
    const unsigned char * nonce;
    unsigned char * payload_pt;
    size_t payload_length = (size_t)(FileTail.payload_size);
    size_t header_length = FileTail.header_size;
    size_t quick_offset = nonce_offset + 12;
    size_t last_offset;
    bool quick_valid;

    // Set the input text and n-once
    payload_ct = layer_data + FileTail.header_size;
    nonce = layer_data + nonce_offset;
    assert(out_plain_text != NULL);

    // Allocate buffer for plaintext
    if((payload_pt = STORM_ALLOC(BYTE, payload_length)) != NULL)
    {
        // Try quick decryption if the 
        quick_valid = (quick_offset + AES_BLOCK_SIZE <= header_length) && (quick_offset + AES_BLOCK_SIZE <= layer_data_length);
        if(quick_valid && w3xe_ccm_decrypt(aes_key, nonce, layer_data + quick_offset, payload_ct, payload_pt, payload_length))
        {
            out_plain_text[0] = payload_pt;
            return true;
        }

        if(header_length > AES_BLOCK_SIZE)
        {
            for(size_t offs = 0; offs <= header_length - AES_BLOCK_SIZE; offs++)
            {
                if(quick_valid && offs == quick_offset)
                    continue;
                if(offs + AES_BLOCK_SIZE > layer_data_length)
                    continue;
                if(w3xe_ccm_decrypt(aes_key, nonce, layer_data + offs, payload_ct, payload_pt, payload_length))
                {
                    out_plain_text[0] = payload_pt;
                    return true;
                }
            }
        }

        last_offset = header_length + payload_length;
        if(last_offset + AES_BLOCK_SIZE <= layer_data_length && w3xe_ccm_decrypt(aes_key, nonce, layer_data + last_offset, payload_ct, payload_pt, payload_length))
        {
            out_plain_text[0] = payload_pt;
            return true;
        }

        STORM_FREE(payload_pt);
    }
    return false;   // Failed
}
