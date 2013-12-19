/*****************************************************************************/
/* SBaseCommon.cpp                        Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* Common functions for StormLib, used by all SFile*** modules               */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 24.03.03  1.00  Lad  The first version of SFileCommon.cpp                 */
/* 19.11.03  1.01  Dan  Big endian handling                                  */
/* 12.06.04  1.01  Lad  Renamed to SCommon.cpp                               */
/* 06.09.10  1.01  Lad  Renamed to SBaseCommon.cpp                           */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"

char StormLibCopyright[] = "StormLib v " STORMLIB_VERSION_STRING " Copyright Ladislav Zezula 1998-2014";

//-----------------------------------------------------------------------------
// Local variables

LCID    lcFileLocale = LANG_NEUTRAL;            // File locale
USHORT  wPlatform = 0;                          // File platform

//-----------------------------------------------------------------------------
// Conversion to uppercase/lowercase

// Converts ASCII characters to lowercase
// Converts slash (0x2F) to backslash (0x5C)
unsigned char AsciiToLowerTable[256] = 
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x5C, 
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

// Converts ASCII characters to uppercase
// Converts slash (0x2F) to backslash (0x5C)
unsigned char AsciiToUpperTable[256] = 
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x5C, 
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

// Converts ASCII characters to uppercase
// Does NOT convert slash (0x2F) to backslash (0x5C)
unsigned char AsciiToUpperTable_Slash[256] = 
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

//-----------------------------------------------------------------------------
// Storm hashing functions

#define STORM_BUFFER_SIZE       0x500

#define HASH_INDEX_MASK(ha) (ha->pHeader->dwHashTableSize ? (ha->pHeader->dwHashTableSize - 1) : 0)

static DWORD StormBuffer[STORM_BUFFER_SIZE];    // Buffer for the decryption engine
static bool  bMpqCryptographyInitialized = false;

void InitializeMpqCryptography()
{
    DWORD dwSeed = 0x00100001;
    DWORD index1 = 0;
    DWORD index2 = 0;
    int   i;

    // Initialize the decryption buffer.
    // Do nothing if already done.
    if(bMpqCryptographyInitialized == false)
    {
        for(index1 = 0; index1 < 0x100; index1++)
        {
            for(index2 = index1, i = 0; i < 5; i++, index2 += 0x100)
            {
                DWORD temp1, temp2;

                dwSeed = (dwSeed * 125 + 3) % 0x2AAAAB;
                temp1  = (dwSeed & 0xFFFF) << 0x10;

                dwSeed = (dwSeed * 125 + 3) % 0x2AAAAB;
                temp2  = (dwSeed & 0xFFFF);

                StormBuffer[index2] = (temp1 | temp2);
            }
        }

        // Also register both MD5 and SHA1 hash algorithms
        register_hash(&md5_desc);
        register_hash(&sha1_desc);

        // Use LibTomMath as support math library for LibTomCrypt
        ltc_mp = ltm_desc;    

        // Don't do that again
        bMpqCryptographyInitialized = true;
    }
}

DWORD HashString(const char * szFileName, DWORD dwHashType)
{
    LPBYTE pbKey   = (BYTE *)szFileName;
    DWORD  dwSeed1 = 0x7FED7FED;
    DWORD  dwSeed2 = 0xEEEEEEEE;
    DWORD  ch;

    while(*pbKey != 0)
    {
        // Convert the input character to uppercase
        // Convert slash (0x2F) to backslash (0x5C)
        ch = AsciiToUpperTable[*pbKey++];

        dwSeed1 = StormBuffer[dwHashType + ch] ^ (dwSeed1 + dwSeed2);
        dwSeed2 = ch + dwSeed1 + dwSeed2 + (dwSeed2 << 5) + 3;
    }

    return dwSeed1;
}

DWORD HashStringSlash(const char * szFileName, DWORD dwHashType)
{
    LPBYTE pbKey   = (BYTE *)szFileName;
    DWORD  dwSeed1 = 0x7FED7FED;
    DWORD  dwSeed2 = 0xEEEEEEEE;
    DWORD  ch;

    while(*pbKey != 0)
    {
        // Convert the input character to uppercase
        // DON'T convert slash (0x2F) to backslash (0x5C)
        ch = AsciiToUpperTable_Slash[*pbKey++];

        dwSeed1 = StormBuffer[dwHashType + ch] ^ (dwSeed1 + dwSeed2);
        dwSeed2 = ch + dwSeed1 + dwSeed2 + (dwSeed2 << 5) + 3;
    }

    return dwSeed1;
}

DWORD HashStringLower(const char * szFileName, DWORD dwHashType)
{
    LPBYTE pbKey   = (BYTE *)szFileName;
    DWORD  dwSeed1 = 0x7FED7FED;
    DWORD  dwSeed2 = 0xEEEEEEEE;
    DWORD  ch;

    while(*pbKey != 0)
    {
        // Convert the input character to lower
        // DON'T convert slash (0x2F) to backslash (0x5C)
        ch = AsciiToLowerTable[*pbKey++];

        dwSeed1 = StormBuffer[dwHashType + ch] ^ (dwSeed1 + dwSeed2);
        dwSeed2 = ch + dwSeed1 + dwSeed2 + (dwSeed2 << 5) + 3;
    }

    return dwSeed1;
}

//-----------------------------------------------------------------------------
// Calculates the hash table size for a given amount of files

DWORD GetHashTableSizeForFileCount(DWORD dwFileCount)
{
    DWORD dwPowerOfTwo = HASH_TABLE_SIZE_MIN;

    // For zero files, there is no hash table needed
    if(dwFileCount == 0)
        return 0;

    // Round the hash table size up to the nearest power of two
    // Don't allow the hash table size go over allowed maximum
    while(dwPowerOfTwo < HASH_TABLE_SIZE_MAX && dwPowerOfTwo < dwFileCount)
        dwPowerOfTwo <<= 1;
    return dwPowerOfTwo;
}

//-----------------------------------------------------------------------------
// Calculates a Jenkin's Encrypting and decrypting MPQ file data

ULONGLONG HashStringJenkins(const char * szFileName)
{
    LPBYTE pbFileName = (LPBYTE)szFileName;
    char * szTemp;
    char szLocFileName[0x108];
    size_t nLength = 0;
    unsigned int primary_hash = 1;
    unsigned int secondary_hash = 2;

    // Normalize the file name - convert to uppercase, and convert "/" to "\\".
    if(pbFileName != NULL)
    {
        szTemp = szLocFileName;
        while(*pbFileName != 0)
            *szTemp++ = (char)AsciiToLowerTable[*pbFileName++];
        *szTemp = 0;

        nLength = szTemp - szLocFileName;
    }

    // Thanks Quantam for finding out what the algorithm is.
    // I am really getting old for reversing large chunks of assembly
    // that does hashing :-)
    hashlittle2(szLocFileName, nLength, &secondary_hash, &primary_hash);

    // Combine those 2 together
    return (ULONGLONG)primary_hash * (ULONGLONG)0x100000000ULL + (ULONGLONG)secondary_hash;
}

//-----------------------------------------------------------------------------
// Default flags for (attributes) and (listfile)

DWORD GetDefaultSpecialFileFlags(DWORD dwFileSize, USHORT wFormatVersion)
{
    // Fixed for format 1.0
    if(wFormatVersion == MPQ_FORMAT_VERSION_1)
        return MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED | MPQ_FILE_FIX_KEY;

    // Size-dependent for formats 2.0-4.0
    return (dwFileSize > 0x4000) ? (MPQ_FILE_COMPRESS | MPQ_FILE_SECTOR_CRC) : (MPQ_FILE_COMPRESS | MPQ_FILE_SINGLE_UNIT);
}


//-----------------------------------------------------------------------------
// Encrypting and decrypting MPQ file data

void EncryptMpqBlock(void * pvFileBlock, DWORD dwLength, DWORD dwSeed1)
{
    LPDWORD block = (LPDWORD)pvFileBlock;
    DWORD dwSeed2 = 0xEEEEEEEE;
    DWORD ch;

    // Round to DWORDs
    dwLength >>= 2;

    while(dwLength-- > 0)
    {
        dwSeed2 += StormBuffer[0x400 + (dwSeed1 & 0xFF)];
        ch     = *block;
        *block++ = ch ^ (dwSeed1 + dwSeed2);

        dwSeed1  = ((~dwSeed1 << 0x15) + 0x11111111) | (dwSeed1 >> 0x0B);
        dwSeed2  = ch + dwSeed2 + (dwSeed2 << 5) + 3;
    }
}

void DecryptMpqBlock(void * pvFileBlock, DWORD dwLength, DWORD dwSeed1)
{
    LPDWORD block = (LPDWORD)pvFileBlock;
    DWORD dwSeed2 = 0xEEEEEEEE;
    DWORD ch;

    // Round to DWORDs
    dwLength >>= 2;

    while(dwLength-- > 0)
    {
        dwSeed2 += StormBuffer[0x400 + (dwSeed1 & 0xFF)];
        ch     = *block ^ (dwSeed1 + dwSeed2);

        dwSeed1  = ((~dwSeed1 << 0x15) + 0x11111111) | (dwSeed1 >> 0x0B);
        dwSeed2  = ch + dwSeed2 + (dwSeed2 << 5) + 3;
        *block++ = ch;
    }
}

/**
 * Functions tries to get file decryption key. The trick comes from sector
 * positions which are stored at the begin of each compressed file. We know the
 * file size, that means we know number of sectors that means we know the first
 * DWORD value in sector position. And if we know encrypted and decrypted value,
 * we can find the decryption key !!!
 *
 * hf            - MPQ file handle
 * SectorOffsets - DWORD array of sector positions
 * ch            - Decrypted value of the first sector pos
 */

DWORD DetectFileKeyBySectorSize(LPDWORD SectorOffsets, DWORD decrypted)
{
    DWORD saveKey1;
    DWORD temp = *SectorOffsets ^ decrypted;    // temp = seed1 + seed2
    temp -= 0xEEEEEEEE;                 // temp = seed1 + StormBuffer[0x400 + (seed1 & 0xFF)]

    for(int i = 0; i < 0x100; i++)      // Try all 255 possibilities
    {
        DWORD seed1;
        DWORD seed2 = 0xEEEEEEEE;
        DWORD ch;

        // Try the first DWORD (We exactly know the value)
        seed1  = temp - StormBuffer[0x400 + i];
        seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = SectorOffsets[0] ^ (seed1 + seed2);

        if(ch != decrypted)
            continue;

        // Add 1 because we are decrypting sector positions
        saveKey1 = seed1 + 1;

        // If OK, continue and test the second value. We don't know exactly the value,
        // but we know that the second one has lower 16 bits set to zero
        // (no compressed sector is larger than 0xFFFF bytes)
        seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
        seed2  = ch + seed2 + (seed2 << 5) + 3;

        seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = SectorOffsets[1] ^ (seed1 + seed2);

        if((ch & 0xFFFF0000) == 0)
            return saveKey1;
    }
    return 0;
}

// Function tries to detect file encryption key. It expectes at least two uncompressed bytes
DWORD DetectFileKeyByKnownContent(void * pvFileContent, DWORD nDwords, ...)
{
    LPDWORD pdwContent = (LPDWORD)pvFileContent;
    va_list argList;
    DWORD dwDecrypted[0x10];
    DWORD saveKey1;
    DWORD dwTemp;
    DWORD i, j;
    
    // We need at least two DWORDS to detect the file key
    if(nDwords < 0x02 || nDwords > 0x10)
        return 0;
    memset(dwDecrypted, 0, sizeof(dwDecrypted));
    
    va_start(argList, nDwords);
    for(i = 0; i < nDwords; i++)
        dwDecrypted[i] = va_arg(argList, DWORD);
    va_end(argList);
    
    dwTemp = (*pdwContent ^ dwDecrypted[0]) - 0xEEEEEEEE;
    for(i = 0; i < 0x100; i++)      // Try all 256 possibilities
    {
        DWORD seed1;
        DWORD seed2 = 0xEEEEEEEE;
        DWORD ch;

        // Try the first DWORD
        seed1  = dwTemp - StormBuffer[0x400 + i];
        seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = pdwContent[0] ^ (seed1 + seed2);

        if(ch != dwDecrypted[0])
            continue;

        saveKey1 = seed1;

        // If OK, continue and test all bytes.
        for(j = 1; j < nDwords; j++)
        {
            seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
            seed2  = ch + seed2 + (seed2 << 5) + 3;

            seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
            ch     = pdwContent[j] ^ (seed1 + seed2);

            if(ch == dwDecrypted[j] && j == nDwords - 1)
                return saveKey1;
        }
    }
    return 0;
}

DWORD DetectFileKeyByContent(void * pvFileContent, DWORD dwFileSize)
{
    DWORD dwFileKey;

    // Try to break the file encryption key as if it was a WAVE file
    if(dwFileSize >= 0x0C)
    {
        dwFileKey = DetectFileKeyByKnownContent(pvFileContent, 3, 0x46464952, dwFileSize - 8, 0x45564157);
        if(dwFileKey != 0)
            return dwFileKey;
    }

    // Try to break the encryption key as if it was an EXE file
    if(dwFileSize > 0x40)
    {
        dwFileKey = DetectFileKeyByKnownContent(pvFileContent, 2, 0x00905A4D, 0x00000003);
        if(dwFileKey != 0)
            return dwFileKey;
    }

    // Try to break the encryption key as if it was a XML file
    if(dwFileSize > 0x04)
    {
        dwFileKey = DetectFileKeyByKnownContent(pvFileContent, 2, 0x6D783F3C, 0x6576206C);
        if(dwFileKey != 0)
            return dwFileKey;
    }

    // Not detected, sorry
    return 0;
}

DWORD DecryptFileKey(
    const char * szFileName,
    ULONGLONG MpqPos,
    DWORD dwFileSize,
    DWORD dwFlags)
{
    DWORD dwFileKey;
    DWORD dwMpqPos = (DWORD)MpqPos;

    // File key is calculated from plain name
    szFileName = GetPlainFileName(szFileName);
    dwFileKey = HashString(szFileName, MPQ_HASH_FILE_KEY);

    // Fix the key, if needed
    if(dwFlags & MPQ_FILE_FIX_KEY)
        dwFileKey = (dwFileKey + dwMpqPos) ^ dwFileSize;

    // Return the key
    return dwFileKey;
}

//-----------------------------------------------------------------------------
// Handle validation functions

TMPQArchive * IsValidMpqHandle(HANDLE hMpq)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    
    return (ha != NULL && ha->pHeader != NULL && ha->pHeader->dwID == ID_MPQ) ? ha : NULL;
}

TMPQFile * IsValidFileHandle(HANDLE hFile)
{
    TMPQFile * hf = (TMPQFile *)hFile;

    // Must not be NULL
    if(hf != NULL && hf->dwMagic == ID_MPQ_FILE)
    {
        // Local file handle?
        if(hf->pStream != NULL)
            return hf;

        // Also verify the MPQ handle within the file handle
        if(IsValidMpqHandle(hf->ha))
            return hf;
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// Hash table and block table manipulation

// Attempts to search a free hash entry, or an entry whose names and locale matches
TMPQHash * FindFreeHashEntry(TMPQArchive * ha, DWORD dwStartIndex, DWORD dwName1, DWORD dwName2, LCID lcLocale)
{
    TMPQHash * pDeletedEntry = NULL;            // If a deleted entry was found in the continuous hash range
    TMPQHash * pFreeEntry = NULL;               // If a free entry was found in the continuous hash range
    DWORD dwHashIndexMask = HASH_INDEX_MASK(ha);
    DWORD dwIndex;

    // Set the initial index
    dwStartIndex = dwIndex = (dwStartIndex & dwHashIndexMask);

    // Search the hash table and return the found entries in the following priority:
    // 1) <MATCHING_ENTRY>
    // 2) <DELETED-ENTRY>
    // 3) <FREE-ENTRY>
    // 4) NULL
    for(;;)
    {
        TMPQHash * pHash = ha->pHashTable + dwIndex;

        // If we found a matching entry, return that one
        if(pHash->dwName1 == dwName1 && pHash->dwName2 == dwName2 && pHash->lcLocale == lcLocale)
            return pHash;

        // If we found a deleted entry, remember it but keep searching
        if(pHash->dwBlockIndex == HASH_ENTRY_DELETED && pDeletedEntry == NULL)
            pDeletedEntry = pHash;

        // If we found a free entry, we need to stop searching
        if(pHash->dwBlockIndex == HASH_ENTRY_FREE)
        {
            pFreeEntry = pHash;
            break;
        }

        // Move to the next hash entry.
        // If we reached the starting entry, it's failure.
        dwIndex = (dwIndex + 1) & dwHashIndexMask;
        if(dwIndex == dwStartIndex)
            break;
    }

    // If we found a deleted entry, return that one preferentially
    return (pDeletedEntry != NULL) ? pDeletedEntry : pFreeEntry;
}

// Retrieves the first hash entry for the given file.
// Every locale version of a file has its own hash entry
TMPQHash * GetFirstHashEntry(TMPQArchive * ha, const char * szFileName)
{
    DWORD dwHashIndexMask = HASH_INDEX_MASK(ha);
    DWORD dwStartIndex = ha->pfnHashString(szFileName, MPQ_HASH_TABLE_INDEX);
    DWORD dwName1 = ha->pfnHashString(szFileName, MPQ_HASH_NAME_A);
    DWORD dwName2 = ha->pfnHashString(szFileName, MPQ_HASH_NAME_B);
    DWORD dwIndex;

    // Set the initial index
    dwStartIndex = dwIndex = (dwStartIndex & dwHashIndexMask);

    // Search the hash table
    for(;;)
    {
        TMPQHash * pHash = ha->pHashTable + dwIndex;

        // If the entry matches, we found it.
        if(pHash->dwName1 == dwName1 && pHash->dwName2 == dwName2 && pHash->dwBlockIndex < ha->dwFileTableSize)
            return pHash;

        // If that hash entry is a free entry, it means we haven't found the file
        if(pHash->dwBlockIndex == HASH_ENTRY_FREE)
            return NULL;

        // Move to the next hash entry. Stop searching
        // if we got reached the original hash entry
        dwIndex = (dwIndex + 1) & dwHashIndexMask;
        if(dwIndex == dwStartIndex)
            return NULL;
    }
}

TMPQHash * GetNextHashEntry(TMPQArchive * ha, TMPQHash * pFirstHash, TMPQHash * pHash)
{
    DWORD dwHashIndexMask = HASH_INDEX_MASK(ha);
    DWORD dwStartIndex = (DWORD)(pFirstHash - ha->pHashTable);
    DWORD dwName1 = pHash->dwName1;
    DWORD dwName2 = pHash->dwName2;
    DWORD dwIndex = (DWORD)(pHash - ha->pHashTable);

    // Now go for any next entry that follows the pHash,
    // until either free hash entry was found, or the start entry was reached
    for(;;)
    {
        // Move to the next hash entry. Stop searching
        // if we got reached the original hash entry
        dwIndex = (dwIndex + 1) & dwHashIndexMask;
        if(dwIndex == dwStartIndex)
            return NULL;
        pHash = ha->pHashTable + dwIndex;

        // If the entry matches, we found it.
        if(pHash->dwName1 == dwName1 && pHash->dwName2 == dwName2 && pHash->dwBlockIndex < ha->pHeader->dwBlockTableSize)
            return pHash;

        // If that hash entry is a free entry, it means we haven't found the file
        if(pHash->dwBlockIndex == HASH_ENTRY_FREE)
            return NULL;
    }
}

// Allocates an entry in the hash table
TMPQHash * AllocateHashEntry(
    TMPQArchive * ha,
    TFileEntry * pFileEntry)
{
    TMPQHash * pHash;
    DWORD dwStartIndex = ha->pfnHashString(pFileEntry->szFileName, MPQ_HASH_TABLE_INDEX);
    DWORD dwName1 = ha->pfnHashString(pFileEntry->szFileName, MPQ_HASH_NAME_A);
    DWORD dwName2 = ha->pfnHashString(pFileEntry->szFileName, MPQ_HASH_NAME_B);

    // Attempt to find a free hash entry
    pHash = FindFreeHashEntry(ha, dwStartIndex, dwName1, dwName2, pFileEntry->lcLocale);
    if(pHash != NULL)
    {
        // Fill the free hash entry
        pHash->dwName1      = dwName1;
        pHash->dwName2      = dwName2;
        pHash->lcLocale     = pFileEntry->lcLocale;
        pHash->wPlatform    = pFileEntry->wPlatform;
        pHash->dwBlockIndex = (DWORD)(pFileEntry - ha->pFileTable);

        // Fill the hash index in the file entry
        pFileEntry->dwHashIndex = (DWORD)(pHash - ha->pHashTable);
    }

    return pHash;
}

// Finds a free space in the MPQ where to store next data
// The free space begins beyond the file that is stored at the fuhrtest
// position in the MPQ.
ULONGLONG FindFreeMpqSpace(TMPQArchive * ha)
{
    TMPQHeader * pHeader = ha->pHeader;
    TFileEntry * pFileTableEnd = ha->pFileTable + ha->dwFileTableSize;
    TFileEntry * pFileEntry;
    ULONGLONG FreeSpacePos = ha->pHeader->dwHeaderSize;
    DWORD dwChunkCount;

    // Parse the entire block table
    for(pFileEntry = ha->pFileTable; pFileEntry < pFileTableEnd; pFileEntry++)
    {
        // Only take existing files with nonzero size
        if((pFileEntry->dwFlags & MPQ_FILE_EXISTS) && (pFileEntry->dwCmpSize != 0))
        {
            // If the end of the file is bigger than current MPQ table pos, update it
            if((pFileEntry->ByteOffset + pFileEntry->dwCmpSize) > FreeSpacePos)
            {
                // Get the end of the file data
                FreeSpacePos = pFileEntry->ByteOffset + pFileEntry->dwCmpSize;

                // Add the MD5 chunks, if present
                if(pHeader->dwRawChunkSize != 0 && pFileEntry->dwCmpSize != 0)
                {
                    dwChunkCount = ((pFileEntry->dwCmpSize - 1) / pHeader->dwRawChunkSize) + 1;
                    FreeSpacePos += dwChunkCount * MD5_DIGEST_SIZE;
                }
            }
        }
    }

    // Give the free space position to the caller
    return FreeSpacePos;
}

//-----------------------------------------------------------------------------
// Common functions - MPQ File

TMPQFile * CreateMpqFile(TMPQArchive * ha)
{
    TMPQFile * hf;

    // Allocate space for TMPQFile
    hf = STORM_ALLOC(TMPQFile, 1);
    if(hf != NULL)
    {
        // Fill the file structure
        memset(hf, 0, sizeof(TMPQFile));
        hf->ha = ha;
        hf->pStream = NULL;
        hf->dwMagic = ID_MPQ_FILE;
    }

    return hf;
}

// Loads a table from MPQ.
// Can be used for hash table, block table, sector offset table or sector checksum table
void * LoadMpqTable(
    TMPQArchive * ha,
    ULONGLONG ByteOffset,
    DWORD dwCompressedSize,
    DWORD dwTableSize,
    DWORD dwKey)
{
    LPBYTE pbCompressed = NULL;
    LPBYTE pbMpqTable;
    LPBYTE pbToRead;
    DWORD dwBytesToRead = dwCompressedSize;
    DWORD dwValidLength = dwTableSize;
    int nError = ERROR_SUCCESS;

    // Allocate the MPQ table
    pbMpqTable = pbToRead = STORM_ALLOC(BYTE, dwTableSize);
    if(pbMpqTable != NULL)
    {
        // Check if the MPQ table is encrypted
        if(dwCompressedSize < dwTableSize)
        {
            // Allocate temporary buffer for holding compressed data
            pbCompressed = pbToRead = STORM_ALLOC(BYTE, dwCompressedSize);
            if(pbCompressed == NULL)
            {
                STORM_FREE(pbMpqTable);
                return NULL;
            }
        }

        // If everything succeeded, read the raw table form the MPQ
        if(FileStream_Read(ha->pStream, &ByteOffset, pbToRead, dwBytesToRead))
        {
            // First of all, decrypt the table
            if(dwKey != 0)
            {
                BSWAP_ARRAY32_UNSIGNED(pbToRead, dwCompressedSize);
                DecryptMpqBlock(pbToRead, dwCompressedSize, dwKey);
                BSWAP_ARRAY32_UNSIGNED(pbToRead, dwCompressedSize);
            }

            // If the table is compressed, decompress it
            if(dwCompressedSize < dwTableSize)
            {
                int cbOutBuffer = (int)dwTableSize;
                int cbInBuffer = (int)dwCompressedSize;

                if(!SCompDecompress2(pbMpqTable, &cbOutBuffer, pbCompressed, cbInBuffer))
                    nError = GetLastError();
            }

            // Make sure that the table is properly byte-swapped
            BSWAP_ARRAY32_UNSIGNED(pbMpqTable, dwTableSize);

            // If the table was not fully readed, fill the rest with zeros
            if(dwValidLength < dwTableSize)
                memset(pbMpqTable + dwValidLength, 0, (dwTableSize - dwValidLength));
        }
        else
        {
            nError = GetLastError();
        }

        // If read failed, free the table and return
        if(nError != ERROR_SUCCESS)
        {
            STORM_FREE(pbMpqTable);
            pbMpqTable = NULL;
        }

        // Free the compression buffer, if any
        if(pbCompressed != NULL)
            STORM_FREE(pbCompressed);
    }

    // Return the MPQ table
    return pbMpqTable;
}

void CalculateRawSectorOffset(
    ULONGLONG & RawFilePos, 
    TMPQFile * hf,
    DWORD dwSectorOffset)
{
    //
    // Some MPQ protectors place the sector offset table after the actual file data.
    // Sector offsets in the sector offset table are negative. When added
    // to MPQ file offset from the block table entry, the result is a correct
    // position of the file data in the MPQ.
    //
    // The position of sector table must be always within the MPQ, however.
    // When a negative sector offset is found, we make sure that we make the addition
    // just in 32-bits, and then add the MPQ offset.
    //

    if(dwSectorOffset & 0x80000000)
    {
        RawFilePos = hf->ha->MpqPos + ((DWORD)hf->pFileEntry->ByteOffset + dwSectorOffset);
    }
    else
    {
        RawFilePos = hf->RawFilePos + dwSectorOffset;
    }

    // We also have to add patch header size, if patch header is present
    if(hf->pPatchInfo != NULL)
        RawFilePos += hf->pPatchInfo->dwLength;
}

unsigned char * AllocateMd5Buffer(
    DWORD dwRawDataSize,
    DWORD dwChunkSize,
    LPDWORD pcbMd5Size)
{
    unsigned char * md5_array;
    DWORD cbMd5Size;

    // Sanity check
    assert(dwRawDataSize != 0);
    assert(dwChunkSize != 0);

    // Calculate how many MD5's we will calculate
    cbMd5Size = (((dwRawDataSize - 1) / dwChunkSize) + 1) * MD5_DIGEST_SIZE;

    // Allocate space for array or MD5s
    md5_array = STORM_ALLOC(BYTE, cbMd5Size);

    // Give the size of the MD5 array
    if(pcbMd5Size != NULL)
        *pcbMd5Size = cbMd5Size;
    return md5_array;
}

// Allocates sector buffer and sector offset table
int AllocateSectorBuffer(TMPQFile * hf)
{
    TMPQArchive * ha = hf->ha;

    // Caller of AllocateSectorBuffer must ensure these
    assert(hf->pbFileSector == NULL);
    assert(hf->pFileEntry != NULL);
    assert(hf->ha != NULL);

    // Don't allocate anything if the file has zero size
    if(hf->pFileEntry->dwFileSize == 0 || hf->dwDataSize == 0)
        return ERROR_SUCCESS;

    // Determine the file sector size and allocate buffer for it
    hf->dwSectorSize = (hf->pFileEntry->dwFlags & MPQ_FILE_SINGLE_UNIT) ? hf->dwDataSize : ha->dwSectorSize;
    hf->pbFileSector = STORM_ALLOC(BYTE, hf->dwSectorSize);
    hf->dwSectorOffs = SFILE_INVALID_POS;

    // Return result
    return (hf->pbFileSector != NULL) ? (int)ERROR_SUCCESS : (int)ERROR_NOT_ENOUGH_MEMORY;
}

// Allocates sector offset table
int AllocatePatchInfo(TMPQFile * hf, bool bLoadFromFile)
{
    TMPQArchive * ha = hf->ha;
    DWORD dwLength = sizeof(TPatchInfo);

    // The following conditions must be true
    assert(hf->pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE);
    assert(hf->pPatchInfo == NULL);

__AllocateAndLoadPatchInfo:

    // Allocate space for patch header. Start with default size,
    // and if its size if bigger, then we reload them
    hf->pPatchInfo = (TPatchInfo *)STORM_ALLOC(BYTE, dwLength);
    if(hf->pPatchInfo == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Do we have to load the patch header from the file ?
    if(bLoadFromFile)
    {
        // Load the patch header
        if(!FileStream_Read(ha->pStream, &hf->RawFilePos, hf->pPatchInfo, dwLength))
        {
            // Free the patch info
            STORM_FREE(hf->pPatchInfo);
            hf->pPatchInfo = NULL;
            return GetLastError();
        }

        // Perform necessary swapping
        hf->pPatchInfo->dwLength = BSWAP_INT32_UNSIGNED(hf->pPatchInfo->dwLength);
        hf->pPatchInfo->dwFlags = BSWAP_INT32_UNSIGNED(hf->pPatchInfo->dwFlags);
        hf->pPatchInfo->dwDataSize = BSWAP_INT32_UNSIGNED(hf->pPatchInfo->dwDataSize);

        // Verify the size of the patch header
        // If it's not default size, we have to reload them
        if(hf->pPatchInfo->dwLength > dwLength)
        {
            // Free the patch info
            dwLength = hf->pPatchInfo->dwLength;
            STORM_FREE(hf->pPatchInfo);
            hf->pPatchInfo = NULL;

            // If the length is out of all possible ranges, fail the operation
            if(dwLength > 0x400)
                return ERROR_FILE_CORRUPT;
            goto __AllocateAndLoadPatchInfo;
        }

        // Patch file data size according to the patch header
        hf->dwDataSize = hf->pPatchInfo->dwDataSize;
    }
    else
    {
        memset(hf->pPatchInfo, 0, dwLength);
    }

    // Save the final length to the patch header
    hf->pPatchInfo->dwLength = dwLength;
    hf->pPatchInfo->dwFlags  = 0x80000000;
    return ERROR_SUCCESS;
}

// Allocates sector offset table
int AllocateSectorOffsets(TMPQFile * hf, bool bLoadFromFile)
{
    TMPQArchive * ha = hf->ha;
    TFileEntry * pFileEntry = hf->pFileEntry;
    DWORD dwSectorOffsLen;
    bool bSectorOffsetTableCorrupt = false;

    // Caller of AllocateSectorOffsets must ensure these
    assert(hf->SectorOffsets == NULL);
    assert(hf->pFileEntry != NULL);
    assert(hf->dwDataSize != 0);
    assert(hf->ha != NULL);

    // If the file is stored as single unit, just set number of sectors to 1
    if(pFileEntry->dwFlags & MPQ_FILE_SINGLE_UNIT)
    {
        hf->dwSectorCount = 1;
        return ERROR_SUCCESS;
    }

    // Calculate the number of data sectors
    // Note that this doesn't work if the file size is zero
    hf->dwSectorCount = ((hf->dwDataSize - 1) / hf->dwSectorSize) + 1;

    // Calculate the number of file sectors
    dwSectorOffsLen = (hf->dwSectorCount + 1) * sizeof(DWORD);
    
    // If MPQ_FILE_SECTOR_CRC flag is set, there will either be extra DWORD
    // or an array of MD5's. Either way, we read at least 4 bytes more
    // in order to save additional read from the file.
    if(pFileEntry->dwFlags & MPQ_FILE_SECTOR_CRC)
        dwSectorOffsLen += sizeof(DWORD);

    // Only allocate and load the table if the file is compressed
    if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK)
    {
        __LoadSectorOffsets:

        // Allocate the sector offset table
        hf->SectorOffsets = STORM_ALLOC(DWORD, (dwSectorOffsLen / sizeof(DWORD)));
        if(hf->SectorOffsets == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        // Only read from the file if we are supposed to do so
        if(bLoadFromFile)
        {
            ULONGLONG RawFilePos = hf->RawFilePos;

            if(hf->pPatchInfo != NULL)
                RawFilePos += hf->pPatchInfo->dwLength;

            // Load the sector offsets from the file
            if(!FileStream_Read(ha->pStream, &RawFilePos, hf->SectorOffsets, dwSectorOffsLen))
            {
                // Free the sector offsets
                STORM_FREE(hf->SectorOffsets);
                hf->SectorOffsets = NULL;
                return GetLastError();
            }

            // Swap the sector positions
            BSWAP_ARRAY32_UNSIGNED(hf->SectorOffsets, dwSectorOffsLen);

            // Decrypt loaded sector positions if necessary
            if(pFileEntry->dwFlags & MPQ_FILE_ENCRYPTED)
            {
                // If we don't know the file key, try to find it.
                if(hf->dwFileKey == 0)
                {
                    hf->dwFileKey = DetectFileKeyBySectorSize(hf->SectorOffsets, dwSectorOffsLen);
                    if(hf->dwFileKey == 0)
                    {
                        STORM_FREE(hf->SectorOffsets);
                        hf->SectorOffsets = NULL;
                        return ERROR_UNKNOWN_FILE_KEY;
                    }
                }

                // Decrypt sector positions
                DecryptMpqBlock(hf->SectorOffsets, dwSectorOffsLen, hf->dwFileKey - 1);
            }

            //
            // Validate the sector offset table
            //
            // Note: Some MPQ protectors put the actual file data before the sector offset table.
            // In this case, the sector offsets are negative (> 0x80000000).
            //

            for(DWORD i = 0; i < hf->dwSectorCount; i++)
            {
                DWORD dwSectorOffset1 = hf->SectorOffsets[i+1];
                DWORD dwSectorOffset0 = hf->SectorOffsets[i];

                // Every following sector offset must be bigger than the previous one
                if(dwSectorOffset1 <= dwSectorOffset0)
                {
                    bSectorOffsetTableCorrupt = true;
                    break;
                }

                // The sector size must not be bigger than compressed file size
                if((dwSectorOffset1 - dwSectorOffset0) > pFileEntry->dwCmpSize)
                {
                    bSectorOffsetTableCorrupt = true;
                    break;
                }
            }

            // If data corruption detected, free the sector offset table
            if(bSectorOffsetTableCorrupt)
            {
                STORM_FREE(hf->SectorOffsets);
                hf->SectorOffsets = NULL;
                return ERROR_FILE_CORRUPT;
            }

            //
            // There may be various extra DWORDs loaded after the sector offset table.
            // They are mostly empty on WoW release MPQs, but on MPQs from PTR,
            // they contain random non-zero data. Their meaning is unknown.
            //
            // These extra values are, however, include in the dwCmpSize in the file
            // table. We cannot ignore them, because compacting archive would fail
            // 

            if(hf->SectorOffsets[0] > dwSectorOffsLen)
            {
                dwSectorOffsLen = hf->SectorOffsets[0];
                STORM_FREE(hf->SectorOffsets);
                hf->SectorOffsets = NULL;
                goto __LoadSectorOffsets;
            }
        }
        else
        {
            memset(hf->SectorOffsets, 0, dwSectorOffsLen);
            hf->SectorOffsets[0] = dwSectorOffsLen;
        }
    }

    return ERROR_SUCCESS;
}

int AllocateSectorChecksums(TMPQFile * hf, bool bLoadFromFile)
{
    TMPQArchive * ha = hf->ha;
    TFileEntry * pFileEntry = hf->pFileEntry;
    ULONGLONG RawFilePos;
    DWORD dwCompressedSize = 0;
    DWORD dwExpectedSize;
    DWORD dwCrcOffset;                      // Offset of the CRC table, relative to file offset in the MPQ
    DWORD dwCrcSize;

    // Caller of AllocateSectorChecksums must ensure these
    assert(hf->SectorChksums == NULL);
    assert(hf->SectorOffsets != NULL);
    assert(hf->pFileEntry != NULL);
    assert(hf->ha != NULL);

    // Single unit files don't have sector checksums
    if(pFileEntry->dwFlags & MPQ_FILE_SINGLE_UNIT)
        return ERROR_SUCCESS;

    // Caller must ensure that we are only called when we have sector checksums
    assert(pFileEntry->dwFlags & MPQ_FILE_SECTOR_CRC);

    // 
    // Older MPQs store an array of CRC32's after
    // the raw file data in the MPQ.
    //
    // In newer MPQs, the (since Cataclysm BETA) the (attributes) file
    // contains additional 32-bit values beyond the sector table.
    // Their number depends on size of the (attributes), but their
    // meaning is unknown. They are usually zeroed in retail game files,
    // but contain some sort of checksum in BETA MPQs
    //

    // Does the size of the file table match with the CRC32-based checksums?
    dwExpectedSize = (hf->dwSectorCount + 2) * sizeof(DWORD);
    if(hf->SectorOffsets[0] != 0 && hf->SectorOffsets[0] == dwExpectedSize)
    {
        // If we are not loading from the MPQ file, we just allocate the sector table
        // In that case, do not check any sizes
        if(bLoadFromFile == false)
        {
            hf->SectorChksums = STORM_ALLOC(DWORD, hf->dwSectorCount);
            if(hf->SectorChksums == NULL)
                return ERROR_NOT_ENOUGH_MEMORY;

            // Fill the checksum table with zeros
            memset(hf->SectorChksums, 0, hf->dwSectorCount * sizeof(DWORD));
            return ERROR_SUCCESS;
        }
        else
        {
            // Is there valid size of the sector checksums?
            if(hf->SectorOffsets[hf->dwSectorCount + 1] >= hf->SectorOffsets[hf->dwSectorCount])
                dwCompressedSize = hf->SectorOffsets[hf->dwSectorCount + 1] - hf->SectorOffsets[hf->dwSectorCount];

            // Ignore cases when the length is too small or too big.
            if(dwCompressedSize < sizeof(DWORD) || dwCompressedSize > hf->dwSectorSize)
                return ERROR_SUCCESS;

            // Calculate offset of the CRC table
            dwCrcSize = hf->dwSectorCount * sizeof(DWORD);
            dwCrcOffset = hf->SectorOffsets[hf->dwSectorCount];
            CalculateRawSectorOffset(RawFilePos, hf, dwCrcOffset); 

            // Now read the table from the MPQ
            hf->SectorChksums = (DWORD *)LoadMpqTable(ha, RawFilePos, dwCompressedSize, dwCrcSize, 0);
            if(hf->SectorChksums == NULL)
                return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    // If the size doesn't match, we ignore sector checksums
//  assert(false);
    return ERROR_SUCCESS;
}

int WritePatchInfo(TMPQFile * hf)
{
    TMPQArchive * ha = hf->ha;
    TPatchInfo * pPatchInfo = hf->pPatchInfo;

    // The caller must make sure that this function is only called
    // when the following is true.
    assert(hf->pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE);
    assert(pPatchInfo != NULL);

    BSWAP_ARRAY32_UNSIGNED(pPatchInfo, 3 * sizeof(DWORD));
    if(!FileStream_Write(ha->pStream, &hf->RawFilePos, pPatchInfo, sizeof(TPatchInfo)))
        return GetLastError();

    return ERROR_SUCCESS;
}

int WriteSectorOffsets(TMPQFile * hf)
{
    TMPQArchive * ha = hf->ha;
    TFileEntry * pFileEntry = hf->pFileEntry;
    ULONGLONG RawFilePos = hf->RawFilePos;
    DWORD dwSectorOffsLen;

    // The caller must make sure that this function is only called
    // when the following is true.
    assert(hf->pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK);
    assert(hf->SectorOffsets != NULL);
    dwSectorOffsLen = hf->SectorOffsets[0];

    // If file is encrypted, sector positions are also encrypted
    if(pFileEntry->dwFlags & MPQ_FILE_ENCRYPTED)
        EncryptMpqBlock(hf->SectorOffsets, dwSectorOffsLen, hf->dwFileKey - 1);
    BSWAP_ARRAY32_UNSIGNED(hf->SectorOffsets, dwSectorOffsLen);

    // Adjust sector offset table position, if we also have patch info
    if(hf->pPatchInfo != NULL)
        RawFilePos += hf->pPatchInfo->dwLength;

    // Write sector offsets to the archive
    if(!FileStream_Write(ha->pStream, &RawFilePos, hf->SectorOffsets, dwSectorOffsLen))
        return GetLastError();
    
    // Not necessary, as the sector checksums
    // are going to be freed when this is done.
//  BSWAP_ARRAY32_UNSIGNED(hf->SectorOffsets, dwSectorOffsLen);
    return ERROR_SUCCESS;
}


int WriteSectorChecksums(TMPQFile * hf)
{
    TMPQArchive * ha = hf->ha;
    ULONGLONG RawFilePos;
    TFileEntry * pFileEntry = hf->pFileEntry;
    LPBYTE pbCompressed;
    DWORD dwCompressedSize = 0;
    DWORD dwCrcSize;
    int nOutSize;
    int nError = ERROR_SUCCESS;

    // The caller must make sure that this function is only called
    // when the following is true.
    assert(hf->pFileEntry->dwFlags & MPQ_FILE_SECTOR_CRC);
    assert(hf->SectorOffsets != NULL);
    assert(hf->SectorChksums != NULL);

    // If the MPQ has MD5 of each raw data chunk,
    // we leave sector offsets empty
    if(ha->pHeader->dwRawChunkSize != 0)
    {
        hf->SectorOffsets[hf->dwSectorCount + 1] = hf->SectorOffsets[hf->dwSectorCount];
        return ERROR_SUCCESS;
    }

    // Calculate size of the checksum array
    dwCrcSize = hf->dwSectorCount * sizeof(DWORD);

    // Allocate buffer for compressed sector CRCs.
    pbCompressed = STORM_ALLOC(BYTE, dwCrcSize);
    if(pbCompressed == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Perform the compression
    BSWAP_ARRAY32_UNSIGNED(hf->SectorChksums, dwCrcSize);

    nOutSize = (int)dwCrcSize;
    SCompCompress(pbCompressed, &nOutSize, hf->SectorChksums, (int)dwCrcSize, MPQ_COMPRESSION_ZLIB, 0, 0);
    dwCompressedSize = (DWORD)nOutSize;

    // Write the sector CRCs to the archive
    RawFilePos = hf->RawFilePos + hf->SectorOffsets[hf->dwSectorCount];
    if(hf->pPatchInfo != NULL)
        RawFilePos += hf->pPatchInfo->dwLength;
    if(!FileStream_Write(ha->pStream, &RawFilePos, pbCompressed, dwCompressedSize))
        nError = GetLastError();

    // Not necessary, as the sector checksums
    // are going to be freed when this is done.
//  BSWAP_ARRAY32_UNSIGNED(hf->SectorChksums, dwCrcSize);

    // Store the sector CRCs 
    hf->SectorOffsets[hf->dwSectorCount + 1] = hf->SectorOffsets[hf->dwSectorCount] + dwCompressedSize;
    pFileEntry->dwCmpSize += dwCompressedSize;
    STORM_FREE(pbCompressed);
    return nError;
}

int WriteMemDataMD5(
    TFileStream * pStream,
    ULONGLONG RawDataOffs,
    void * pvRawData,
    DWORD dwRawDataSize,
    DWORD dwChunkSize,
    LPDWORD pcbTotalSize)
{
    unsigned char * md5_array;
    unsigned char * md5;
    LPBYTE pbRawData = (LPBYTE)pvRawData;
    DWORD dwBytesRemaining = dwRawDataSize;
    DWORD dwMd5ArraySize = 0;
    int nError = ERROR_SUCCESS;

    // Allocate buffer for array of MD5
    md5_array = md5 = AllocateMd5Buffer(dwRawDataSize, dwChunkSize, &dwMd5ArraySize);
    if(md5_array == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // For every file chunk, calculate MD5
    while(dwBytesRemaining != 0)
    {
        // Get the remaining number of bytes to read
        dwChunkSize = STORMLIB_MIN(dwBytesRemaining, dwChunkSize);

        // Calculate MD5
        CalculateDataBlockHash(pbRawData, dwChunkSize, md5);
        md5 += MD5_DIGEST_SIZE;

        // Move offset and size
        dwBytesRemaining -= dwChunkSize;
        pbRawData += dwChunkSize;
    }

    // Write the array od MD5's to the file
    RawDataOffs += dwRawDataSize;
    if(!FileStream_Write(pStream, &RawDataOffs, md5_array, dwMd5ArraySize))
        nError = GetLastError();

    // Give the caller the size of the MD5 array
    if(pcbTotalSize != NULL)
        *pcbTotalSize = dwRawDataSize + dwMd5ArraySize;

    // Free buffers and exit
    STORM_FREE(md5_array);
    return nError;
}


// Writes the MD5 for each chunk of the raw file data
int WriteMpqDataMD5(
    TFileStream * pStream,
    ULONGLONG RawDataOffs,
    DWORD dwRawDataSize,
    DWORD dwChunkSize)
{
    unsigned char * md5_array;
    unsigned char * md5;
    LPBYTE pbFileChunk;
    DWORD dwMd5ArraySize = 0;
    DWORD dwToRead = dwRawDataSize;
    int nError = ERROR_SUCCESS;

    // Allocate buffer for array of MD5
    md5_array = md5 = AllocateMd5Buffer(dwRawDataSize, dwChunkSize, &dwMd5ArraySize);
    if(md5_array == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Allocate space for file chunk
    pbFileChunk = STORM_ALLOC(BYTE, dwChunkSize);
    if(pbFileChunk == NULL)
    {
        STORM_FREE(md5_array);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    // For every file chunk, calculate MD5
    while(dwRawDataSize != 0)
    {
        // Get the remaining number of bytes to read
        dwToRead = STORMLIB_MIN(dwRawDataSize, dwChunkSize);

        // Read the chunk
        if(!FileStream_Read(pStream, &RawDataOffs, pbFileChunk, dwToRead))
        {
            nError = GetLastError();
            break;
        }

        // Calculate MD5
        CalculateDataBlockHash(pbFileChunk, dwToRead, md5);
        md5 += MD5_DIGEST_SIZE;

        // Move offset and size
        RawDataOffs += dwToRead;
        dwRawDataSize -= dwToRead;
    }

    // Write the array od MD5's to the file
    if(nError == ERROR_SUCCESS)
    {
        if(!FileStream_Write(pStream, NULL, md5_array, dwMd5ArraySize))
            nError = GetLastError();
    }

    // Free buffers and exit
    STORM_FREE(pbFileChunk);
    STORM_FREE(md5_array);
    return nError;
}

// Frees the structure for MPQ file
void FreeMPQFile(TMPQFile *& hf)
{
    if(hf != NULL)
    {
        // If we have patch file attached to this one, free it first
        if(hf->hfPatch != NULL)
            FreeMPQFile(hf->hfPatch);

        // Then free all buffers allocated in the file structure
        if(hf->pPatchHeader != NULL)
            STORM_FREE(hf->pPatchHeader);
        if(hf->pbFileData != NULL)
            STORM_FREE(hf->pbFileData);
        if(hf->pPatchInfo != NULL)
            STORM_FREE(hf->pPatchInfo);
        if(hf->SectorOffsets != NULL)
            STORM_FREE(hf->SectorOffsets);
        if(hf->SectorChksums != NULL)
            STORM_FREE(hf->SectorChksums);
        if(hf->pbFileSector != NULL)
            STORM_FREE(hf->pbFileSector);
        FileStream_Close(hf->pStream);
        STORM_FREE(hf);
        hf = NULL;
    }
}

// Frees the MPQ archive
void FreeMPQArchive(TMPQArchive *& ha)
{
    if(ha != NULL)
    {
        // First of all, free the patch archive, if any
        if(ha->haPatch != NULL)
            FreeMPQArchive(ha->haPatch);

        // Close the file stream
        FileStream_Close(ha->pStream);
        ha->pStream = NULL;

        // Free the file names from the file table
        if(ha->pFileTable != NULL)
        {
            for(DWORD i = 0; i < ha->dwFileTableSize; i++)
            {
                if(ha->pFileTable[i].szFileName != NULL)
                    STORM_FREE(ha->pFileTable[i].szFileName);
                ha->pFileTable[i].szFileName = NULL;
            }

            // Then free all buffers allocated in the archive structure
            STORM_FREE(ha->pFileTable);
        }

        if(ha->pHashTable != NULL)
            STORM_FREE(ha->pHashTable);
        if(ha->pHetTable != NULL)
            FreeHetTable(ha->pHetTable);
        STORM_FREE(ha);
        ha = NULL;
    }
}

bool IsInternalMpqFileName(const char * szFileName)
{
    if(szFileName != NULL && szFileName[0] == '(')
    {
        if(!_stricmp(szFileName, LISTFILE_NAME) ||
           !_stricmp(szFileName, ATTRIBUTES_NAME) ||
           !_stricmp(szFileName, SIGNATURE_NAME))
        {
            return true;
        }
    }

    return false;
}

// Verifies if the file name is a pseudo-name
bool IsPseudoFileName(const char * szFileName, DWORD * pdwFileIndex)
{
    DWORD dwFileIndex = 0;

    if(szFileName != NULL)
    {
        // Must be "File########.ext"
        if(!_strnicmp(szFileName, "File", 4))
        {
            // Check 8 digits
            for(int i = 4; i < 4+8; i++)
            {
                if(szFileName[i] < '0' || szFileName[i] > '9')
                    return false;
                dwFileIndex = (dwFileIndex * 10) + (szFileName[i] - '0');
            }

            // An extension must follow
            if(szFileName[12] == '.')
            {
                if(pdwFileIndex != NULL)
                    *pdwFileIndex = dwFileIndex;
                return true;
            }
        }
    }

    // Not a pseudo-name
    return false;
}

//-----------------------------------------------------------------------------
// Functions calculating and verifying the MD5 signature

bool IsValidMD5(LPBYTE pbMd5)
{
    BYTE BitSummary = 0;
    
    // The MD5 is considered invalid of it is zeroed
    BitSummary |= pbMd5[0x00] | pbMd5[0x01] | pbMd5[0x02] | pbMd5[0x03] | pbMd5[0x04] | pbMd5[0x05] | pbMd5[0x06] | pbMd5[0x07];
    BitSummary |= pbMd5[0x08] | pbMd5[0x09] | pbMd5[0x0A] | pbMd5[0x0B] | pbMd5[0x0C] | pbMd5[0x0D] | pbMd5[0x0E] | pbMd5[0x0F];
    return (BitSummary != 0);
}

bool VerifyDataBlockHash(void * pvDataBlock, DWORD cbDataBlock, LPBYTE expected_md5)
{
    hash_state md5_state;
    BYTE md5_digest[MD5_DIGEST_SIZE];

    // Don't verify the block if the MD5 is not valid.
    if(!IsValidMD5(expected_md5))
        return true;

    // Calculate the MD5 of the data block
    md5_init(&md5_state);
    md5_process(&md5_state, (unsigned char *)pvDataBlock, cbDataBlock);
    md5_done(&md5_state, md5_digest);

    // Does the MD5's match?
    return (memcmp(md5_digest, expected_md5, MD5_DIGEST_SIZE) == 0);
}

void CalculateDataBlockHash(void * pvDataBlock, DWORD cbDataBlock, LPBYTE md5_hash)
{
    hash_state md5_state;

    md5_init(&md5_state);
    md5_process(&md5_state, (unsigned char *)pvDataBlock, cbDataBlock);
    md5_done(&md5_state, md5_hash);
}


//-----------------------------------------------------------------------------
// Swapping functions

#ifndef PLATFORM_LITTLE_ENDIAN

//
// Note that those functions are implemented for Mac operating system,
// as this is the only supported platform that uses big endian.
//

// Swaps a signed 16-bit integer
int16_t SwapInt16(uint16_t data)
{
	return (int16_t)CFSwapInt16(data);
}

// Swaps an unsigned 16-bit integer
uint16_t SwapUInt16(uint16_t data)
{
	return CFSwapInt16(data);
}

// Swaps signed 32-bit integer
int32_t SwapInt32(uint32_t data)
{
	return (int32_t)CFSwapInt32(data);
}

// Swaps an unsigned 32-bit integer
uint32_t SwapUInt32(uint32_t data)
{
	return CFSwapInt32(data);
}

// Swaps signed 64-bit integer
int64_t SwapInt64(int64_t data)
{
       return (int64_t)CFSwapInt64(data);
}

// Swaps an unsigned 64-bit integer
uint64_t SwapUInt64(uint64_t data)
{
       return CFSwapInt64(data);
}

// Swaps array of unsigned 16-bit integers
void ConvertUInt16Buffer(void * ptr, size_t length)
{
    uint16_t * buffer = (uint16_t *)ptr;
    uint32_t nElements = (uint32_t)(length / sizeof(uint16_t));

    while(nElements-- > 0)
	{
		*buffer = SwapUInt16(*buffer);
		buffer++;
	}
}

// Swaps array of unsigned 32-bit integers
void ConvertUInt32Buffer(void * ptr, size_t length)
{
    uint32_t * buffer = (uint32_t *)ptr;
    uint32_t nElements = (uint32_t)(length / sizeof(uint32_t));

	while(nElements-- > 0)
	{
		*buffer = SwapUInt32(*buffer);
		buffer++;
	}
}

// Swaps array of unsigned 64-bit integers
void ConvertUInt64Buffer(void * ptr, size_t length)
{
    uint64_t * buffer = (uint64_t *)ptr;
    uint32_t nElements = (uint32_t)(length / sizeof(uint64_t));

	while(nElements-- > 0)
	{
		*buffer = SwapUInt64(*buffer);
		buffer++;
	}
}

// Swaps the TMPQHeader structure
void ConvertTMPQHeader(void *header, uint16_t version)
{
	TMPQHeader * theHeader = (TMPQHeader *)header;

    // Swap header part version 1
    if(version == MPQ_FORMAT_VERSION_1)
    {
	    theHeader->dwID = SwapUInt32(theHeader->dwID);
	    theHeader->dwHeaderSize = SwapUInt32(theHeader->dwHeaderSize);
	    theHeader->dwArchiveSize = SwapUInt32(theHeader->dwArchiveSize);
	    theHeader->wFormatVersion = SwapUInt16(theHeader->wFormatVersion);
	    theHeader->wSectorSize = SwapUInt16(theHeader->wSectorSize);
	    theHeader->dwHashTablePos = SwapUInt32(theHeader->dwHashTablePos);
	    theHeader->dwBlockTablePos = SwapUInt32(theHeader->dwBlockTablePos);
	    theHeader->dwHashTableSize = SwapUInt32(theHeader->dwHashTableSize);
	    theHeader->dwBlockTableSize = SwapUInt32(theHeader->dwBlockTableSize);
    }

	if(version == MPQ_FORMAT_VERSION_2)
	{
		theHeader->HiBlockTablePos64 = SwapUInt64(theHeader->HiBlockTablePos64);
        theHeader->wHashTablePosHi = SwapUInt16(theHeader->wHashTablePosHi);
		theHeader->wBlockTablePosHi = SwapUInt16(theHeader->wBlockTablePosHi);
    }

    if(version == MPQ_FORMAT_VERSION_3)
    {
        theHeader->ArchiveSize64 = SwapUInt64(theHeader->ArchiveSize64);
        theHeader->BetTablePos64 = SwapUInt64(theHeader->BetTablePos64);
        theHeader->HetTablePos64 = SwapUInt64(theHeader->HetTablePos64);
    }

    if(version == MPQ_FORMAT_VERSION_4)
	{
        theHeader->HashTableSize64    = SwapUInt64(theHeader->HashTableSize64);
        theHeader->BlockTableSize64   = SwapUInt64(theHeader->BlockTableSize64);
        theHeader->HiBlockTableSize64 = SwapUInt64(theHeader->HiBlockTableSize64);
        theHeader->HetTableSize64     = SwapUInt64(theHeader->HetTableSize64);
        theHeader->BetTableSize64     = SwapUInt64(theHeader->BetTableSize64);
    }
}

#endif  // PLATFORM_LITTLE_ENDIAN
