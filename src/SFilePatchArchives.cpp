/*****************************************************************************/
/* SFilePatchArchives.cpp                 Copyright (c) Ladislav Zezula 2010 */
/*---------------------------------------------------------------------------*/
/* Description:                                                              */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 18.08.10  1.00  Lad  The first version of SFilePatchArchives.cpp          */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"

//-----------------------------------------------------------------------------
// Local structures

#define PATCH_SIGNATURE_HEADER 0x48435450
#define PATCH_SIGNATURE_MD5    0x5f35444d
#define PATCH_SIGNATURE_XFRM   0x4d524658

typedef struct _BLIZZARD_BSDIFF40_FILE
{
    ULONGLONG Signature;
    ULONGLONG CtrlBlockSize;
    ULONGLONG DataBlockSize;
    ULONGLONG NewFileSize;
} BLIZZARD_BSDIFF40_FILE, *PBLIZZARD_BSDIFF40_FILE;

//-----------------------------------------------------------------------------
// Local variables

static const char * LanguageList[] =
{
    "deDE",
    "enCN",
    "enGB",
    "enTW",
    "enUS",
    "esES",
    "esMX",
    "frFR",
    "koKR",
    "ptBR",
    "ptPT",
    "ruRU",
    "zhCN",
    "zhTW",
    NULL
};

//-----------------------------------------------------------------------------
// Local functions

static void Decompress_RLE(LPBYTE pbDecompressed, DWORD cbDecompressed, LPBYTE pbCompressed, DWORD cbCompressed)
{
    LPBYTE pbDecompressedEnd = pbDecompressed + cbDecompressed;
    LPBYTE pbCompressedEnd = pbCompressed + cbCompressed;
    BYTE RepeatCount; 
    BYTE OneByte;

    // Cut the initial DWORD from the compressed chunk
    pbCompressed += sizeof(DWORD);

    // Pre-fill decompressed buffer with zeros
    memset(pbDecompressed, 0, cbDecompressed);

    // Unpack
    while(pbCompressed < pbCompressedEnd && pbDecompressed < pbDecompressedEnd)
    {
        OneByte = *pbCompressed++;
        
        // Is it a repetition byte ?
        if(OneByte & 0x80)
        {
            RepeatCount = (OneByte & 0x7F) + 1;
            for(BYTE i = 0; i < RepeatCount; i++)
            {
                if(pbDecompressed == pbDecompressedEnd || pbCompressed == pbCompressedEnd)
                    break;

                *pbDecompressed++ = *pbCompressed++;
            }
        }
        else
        {
            pbDecompressed += (OneByte + 1);
        }
    }
}

static int LoadFilePatch_COPY(TMPQFile * hf, TPatchHeader * pPatchHeader)
{
    int nError = ERROR_SUCCESS;

    // Allocate space for patch header and compressed data
    hf->pPatchHeader = (TPatchHeader *)STORM_ALLOC(BYTE, pPatchHeader->dwSizeOfPatchData);
    if(hf->pPatchHeader == NULL)
        nError = ERROR_NOT_ENOUGH_MEMORY;

    // Load the patch data and decide if they are compressed or not
    if(nError == ERROR_SUCCESS)
    {
        LPBYTE pbPatchFile = (LPBYTE)hf->pPatchHeader;

        // Copy the patch header itself
        memcpy(pbPatchFile, pPatchHeader, sizeof(TPatchHeader));
        pbPatchFile += sizeof(TPatchHeader);

        // Load the rest of the patch
        if(!SFileReadFile((HANDLE)hf, pbPatchFile, pPatchHeader->dwSizeOfPatchData - sizeof(TPatchHeader), NULL, NULL))
            nError = GetLastError();
    }

    return nError;
}

static int LoadFilePatch_BSD0(TMPQFile * hf, TPatchHeader * pPatchHeader)
{
    LPBYTE pbDecompressed = NULL;
    LPBYTE pbCompressed = NULL;
    DWORD cbDecompressed = 0;
    DWORD cbCompressed = 0;
    DWORD dwBytesRead = 0;
    int nError = ERROR_SUCCESS;

    // Allocate space for compressed data
    cbCompressed = pPatchHeader->dwXfrmBlockSize - SIZE_OF_XFRM_HEADER;
    pbCompressed = STORM_ALLOC(BYTE, cbCompressed);
    if(pbCompressed == NULL)
        nError = ERROR_NOT_ENOUGH_MEMORY;

    // Read the compressed patch data
    if(nError == ERROR_SUCCESS)
    {
        // Load the rest of the header
        SFileReadFile((HANDLE)hf, pbCompressed, cbCompressed, &dwBytesRead, NULL);
        if(dwBytesRead != cbCompressed)
            nError = ERROR_FILE_CORRUPT;
    }

    // Get the uncompressed size of the patch
    if(nError == ERROR_SUCCESS)
    {
        cbDecompressed = pPatchHeader->dwSizeOfPatchData - sizeof(TPatchHeader);
        hf->pPatchHeader = (TPatchHeader *)STORM_ALLOC(BYTE, pPatchHeader->dwSizeOfPatchData);
        if(hf->pPatchHeader == NULL)
            nError = ERROR_NOT_ENOUGH_MEMORY;
    }

    // Now decompress the patch data
    if(nError == ERROR_SUCCESS)
    {
        // Copy the patch header
        memcpy(hf->pPatchHeader, pPatchHeader, sizeof(TPatchHeader));
        pbDecompressed = (LPBYTE)hf->pPatchHeader + sizeof(TPatchHeader);

        // Uncompress or copy the patch data
        if(cbCompressed < cbDecompressed)
        {
            Decompress_RLE(pbDecompressed, cbDecompressed, pbCompressed, cbCompressed);
        }
        else
        {
            assert(cbCompressed == cbDecompressed);
            memcpy(pbDecompressed, pbCompressed, cbCompressed);
        }
    }

    // Free buffers and exit
    if(pbCompressed != NULL)
        STORM_FREE(pbCompressed);
    return nError;
}

static int ApplyFilePatch_COPY(
    TMPQFile * hfFrom,
    TMPQFile * hf,
    TPatchHeader * pPatchHeader)
{
    // Sanity checks
    assert(hf->cbFileData == (pPatchHeader->dwXfrmBlockSize - SIZE_OF_XFRM_HEADER));
    assert(hf->pbFileData != NULL);
    hfFrom = hfFrom;

    // Copy the patch data as-is
    memcpy(hf->pbFileData, (LPBYTE)pPatchHeader + sizeof(TPatchHeader), hf->cbFileData);
    return ERROR_SUCCESS;
}

static int ApplyFilePatch_BSD0(
    TMPQFile * hfFrom,
    TMPQFile * hf,
    TPatchHeader * pPatchHeader)
{
    PBLIZZARD_BSDIFF40_FILE pBsdiff;
    LPDWORD pCtrlBlock;
    LPBYTE pbPatchData = (LPBYTE)pPatchHeader + sizeof(TPatchHeader);
    LPBYTE pDataBlock;
    LPBYTE pExtraBlock;
    LPBYTE pbOldData = hfFrom->pbFileData;
    LPBYTE pbNewData = hf->pbFileData;
    DWORD dwCombineSize;
    DWORD dwNewOffset = 0;                          // Current position to patch
    DWORD dwOldOffset = 0;                          // Current source position
    DWORD dwNewSize;                                // Patched file size
    DWORD dwOldSize = hfFrom->cbFileData;           // File size before patch

    // Get pointer to the patch header
    // Format of BSDIFF header corresponds to original BSDIFF, which is:
    // 0000   8 bytes   signature "BSDIFF40"
    // 0008   8 bytes   size of the control block
    // 0010   8 bytes   size of the data block
    // 0018   8 bytes   new size of the patched file
    pBsdiff = (PBLIZZARD_BSDIFF40_FILE)pbPatchData;
    pbPatchData += sizeof(BLIZZARD_BSDIFF40_FILE);

    // Get pointer to the 32-bit BSDIFF control block
    // The control block follows immediately after the BSDIFF header
    // and consists of three 32-bit integers
    // 0000   4 bytes   Length to copy from the BSDIFF data block the new file
    // 0004   4 bytes   Length to copy from the BSDIFF extra block
    // 0008   4 bytes   Size to increment source file offset
    pCtrlBlock = (LPDWORD)pbPatchData;
    pbPatchData += (size_t)BSWAP_INT64_UNSIGNED(pBsdiff->CtrlBlockSize);

    // Get the pointer to the data block
    pDataBlock = (LPBYTE)pbPatchData;
    pbPatchData += (size_t)BSWAP_INT64_UNSIGNED(pBsdiff->DataBlockSize);

    // Get the pointer to the extra block
    pExtraBlock = (LPBYTE)pbPatchData;
    dwNewSize = (DWORD)BSWAP_INT64_UNSIGNED(pBsdiff->NewFileSize);

    // Now patch the file
    while(dwNewOffset < dwNewSize)
    {
        DWORD dwAddDataLength = BSWAP_INT32_UNSIGNED(pCtrlBlock[0]);
        DWORD dwMovDataLength = BSWAP_INT32_UNSIGNED(pCtrlBlock[1]);
        DWORD dwOldMoveLength = BSWAP_INT32_UNSIGNED(pCtrlBlock[2]);
        DWORD i;

        // Sanity check
        if((dwNewOffset + dwAddDataLength) > dwNewSize)
            return ERROR_FILE_CORRUPT;

        // Read the diff string to the target buffer
        memcpy(pbNewData + dwNewOffset, pDataBlock, dwAddDataLength);
        pDataBlock += dwAddDataLength;

        // Get the longest block that we can combine
        dwCombineSize = ((dwOldOffset + dwAddDataLength) >= dwOldSize) ? (dwOldSize - dwOldOffset) : dwAddDataLength;

        // Now combine the patch data with the original file
        for(i = 0; i < dwCombineSize; i++)
            pbNewData[dwNewOffset + i] = pbNewData[dwNewOffset + i] + pbOldData[dwOldOffset + i];
        
        // Move the offsets 
        dwNewOffset += dwAddDataLength;
        dwOldOffset += dwAddDataLength;

        // Sanity check
        if((dwNewOffset + dwMovDataLength) > dwNewSize)
            return ERROR_FILE_CORRUPT;

        // Copy the data from the extra block in BSDIFF patch
        memcpy(pbNewData + dwNewOffset, pExtraBlock, dwMovDataLength);
        pExtraBlock += dwMovDataLength;
        dwNewOffset += dwMovDataLength;

        // Move the old offset
        if(dwOldMoveLength & 0x80000000)
            dwOldMoveLength = 0x80000000 - dwOldMoveLength;
        dwOldOffset += dwOldMoveLength;
        pCtrlBlock += 3;
    }

    // Success
    return ERROR_SUCCESS;
}


static int LoadFilePatch(TMPQFile * hf)
{
    TPatchHeader PatchHeader;
    DWORD dwBytesRead;
    int nError = ERROR_SUCCESS;

    // Read the patch header
    SFileReadFile((HANDLE)hf, &PatchHeader, sizeof(TPatchHeader), &dwBytesRead, NULL);
    if(dwBytesRead != sizeof(TPatchHeader))
        nError = ERROR_FILE_CORRUPT;

    // Verify the signatures in the patch header
    if(nError == ERROR_SUCCESS)
    {
        // BSWAP the entire header, if needed
        BSWAP_ARRAY32_UNSIGNED(&PatchHeader, sizeof(DWORD) * 6);
        PatchHeader.dwXFRM          = BSWAP_INT32_UNSIGNED(PatchHeader.dwXFRM);
        PatchHeader.dwXfrmBlockSize = BSWAP_INT32_UNSIGNED(PatchHeader.dwXfrmBlockSize);
        PatchHeader.dwPatchType     = BSWAP_INT32_UNSIGNED(PatchHeader.dwPatchType);

        if(PatchHeader.dwSignature != PATCH_SIGNATURE_HEADER || PatchHeader.dwMD5 != PATCH_SIGNATURE_MD5 || PatchHeader.dwXFRM != PATCH_SIGNATURE_XFRM)
            nError = ERROR_FILE_CORRUPT;
    }

    // Read the patch, depending on patch type
    if(nError == ERROR_SUCCESS)
    {
        switch(PatchHeader.dwPatchType)
        {
            case 0x59504f43:    // 'COPY'
                nError = LoadFilePatch_COPY(hf, &PatchHeader);
                break;

            case 0x30445342:    // 'BSD0'
                nError = LoadFilePatch_BSD0(hf, &PatchHeader);
                break;

            default:
                nError = ERROR_FILE_CORRUPT;
                break;
        }
    }

    return nError;
}

static int ApplyFilePatch(
    TMPQFile * hfBase,          // The file in the base MPQ
    TMPQFile * hfPrev,          // The file in the previous MPQ
    TMPQFile * hf)
{
    TPatchHeader * pPatchHeader = hf->pPatchHeader;
    TMPQFile * hfFrom = NULL;
    int nError = ERROR_SUCCESS;

    // Sanity checks
    assert(hf->pbFileData == NULL);

    // Either take the base version or the previous version
    if(!memcmp(hfBase->FileDataMD5, pPatchHeader->md5_before_patch, MD5_DIGEST_SIZE))
        hfFrom = hfBase;
    if(!memcmp(hfPrev->FileDataMD5, pPatchHeader->md5_before_patch, MD5_DIGEST_SIZE))
        hfFrom = hfPrev;
    if(hfFrom == NULL)
        return ERROR_FILE_CORRUPT;

    // Allocate the buffer for patched file content
    hf->pbFileData = STORM_ALLOC(BYTE, pPatchHeader->dwSizeAfterPatch);
    hf->cbFileData = pPatchHeader->dwSizeAfterPatch;
    if(hf->pbFileData == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Apply the patch
    if(nError == ERROR_SUCCESS)
    {
        switch(pPatchHeader->dwPatchType)
        {
            case 0x59504f43:    // 'COPY'
                nError = ApplyFilePatch_COPY(hfFrom, hf, pPatchHeader);
                break;

            case 0x30445342:    // 'BSD0'
                nError = ApplyFilePatch_BSD0(hfFrom, hf, pPatchHeader);
                break;

            default:
                nError = ERROR_FILE_CORRUPT;
                break;
        }
    }

    // Verify MD5 after patch
    if(nError == ERROR_SUCCESS && pPatchHeader->dwSizeAfterPatch != 0)
    {
        // Verify the patched file
        if(!VerifyDataBlockHash(hf->pbFileData, hf->cbFileData, pPatchHeader->md5_after_patch))
            nError = ERROR_FILE_CORRUPT;
        
        // Copy the MD5 of the new block
        memcpy(hf->FileDataMD5, pPatchHeader->md5_after_patch, MD5_DIGEST_SIZE);
    }

    return nError;
}

static void FreePatchData(TMPQFile * hf)
{
    STORM_FREE(hf->pbFileData);
    hf->pbFileData = NULL;
    hf->cbFileData = 0;

    STORM_FREE(hf->pPatchHeader);
    hf->pPatchHeader = NULL;
}

//-----------------------------------------------------------------------------
// Local functions (patch prefix matching)

static TFileEntry * FindMd5ListFile(TMPQArchive * ha)
{
    TFileEntry * pFileEntry = ha->pFileTable + ha->dwFileTableSize;
    char * szLstName;
    size_t nTryCount = 0;
    size_t nLength;

    // Check every file entry for "*-md5.lst".
    // Go backwards, as the entry is usually at the end of the file table
    while(pFileEntry > ha->pFileTable && nTryCount < 10)
    {
        // The file name must be valid
        if(pFileEntry->szFileName != NULL)
        {
            // Get the name and length
            szLstName = pFileEntry->szFileName;
            nLength = strlen(szLstName);

            // Check for the tail name
            if(!_stricmp(szLstName + nLength - 8, "-md5.lst"))
                return pFileEntry;
        }

        // Move back
        pFileEntry--;
        nTryCount++;
    }

    // Not found, sorry
    return NULL;
}

static bool CreatePatchPrefix(TMPQArchive * ha, const char * szFileName, const char * szPrefixEnd)
{
    TMPQNamePrefix * pNewPrefix;
    size_t nLength;

    // If the end of the patch prefix was not entered, find it
    if(szFileName != NULL && szPrefixEnd == NULL)
        szPrefixEnd = szFileName + strlen(szFileName);

    // Create the patch prefix
    nLength = (szPrefixEnd - szFileName);
    pNewPrefix = (TMPQNamePrefix *)STORM_ALLOC(BYTE, sizeof(TMPQNamePrefix) + nLength);
    if(pNewPrefix != NULL)
    {
        // Fill the name prefix
        pNewPrefix->nLength = nLength;
        pNewPrefix->szPatchPrefix[0] = 0;
        
        // Fill the name prefix. Also add the backslash
        if(szFileName && nLength)
        {
            memcpy(pNewPrefix->szPatchPrefix, szFileName, nLength);
            pNewPrefix->szPatchPrefix[nLength] = 0;
        }
    }

    ha->pPatchPrefix = pNewPrefix;
    return (pNewPrefix != NULL);
}

static bool IsMatchingPatchFile(
    TMPQArchive * ha,
    const char * szFileName,
    LPBYTE pbFileMd5)
{
    TPatchHeader PatchHeader = {0};
    HANDLE hFile = NULL;
    DWORD dwTransferred = 0;
    bool bResult = false;

    // Open the file and load the patch header
    if(SFileOpenFileEx((HANDLE)ha, szFileName, SFILE_OPEN_BASE_FILE, &hFile))
    {
        // Load the patch header
        SFileReadFile(hFile, &PatchHeader, sizeof(TPatchHeader), &dwTransferred, NULL);
        BSWAP_ARRAY32_UNSIGNED(pPatchHeader, sizeof(DWORD) * 6);

        // If the file contains an incremental patch,
        // compare the "MD5 before patching" with the base file MD5
        if(dwTransferred == sizeof(TPatchHeader) && PatchHeader.dwSignature == PATCH_SIGNATURE_HEADER)
            bResult = (!memcmp(PatchHeader.md5_before_patch, pbFileMd5, MD5_DIGEST_SIZE));

        // Close the file
        SFileCloseFile(hFile);
    }

    return bResult;
}

static const char * GetLstFileLanguage(const char * szFileName)
{
    char szLstSuffix[0x80];
    size_t nLength;
    size_t nSuffixLength;

    // Each language-dependent file ends with "xxXX-md5.lst"
    nLength = strlen(szFileName);
    if(nLength < 12)
        return NULL;

    // Try each and every possibility
    for(size_t i = 0; LanguageList[i] != NULL; i++)
    {
        nSuffixLength = sprintf(szLstSuffix, "%s-md5.lst", LanguageList[i]);
        assert(nSuffixLength == 12);

        if(!_stricmp(szFileName + nLength - nSuffixLength, szLstSuffix))
            return LanguageList[i];
    }

    return NULL;
}

static bool FindPatchPrefix_WoW_13164_13623(TMPQArchive * haBase, TMPQArchive * haPatch)
{
    TFileEntry * pFileEntry;
    const char * szFilePrefix = "Base";
    const char * szLanguage;
    char szNamePrefix[0x10];
    int nLength;

    // Find a *-md5.lst file in the base archive
    pFileEntry = FindMd5ListFile(haBase);
    if(pFileEntry == NULL)
        return false;

    // Language-specific MPQs have the language identifier right before extension
    szLanguage = GetLstFileLanguage(pFileEntry->szFileName);
    if(szLanguage != NULL)
        szFilePrefix = szLanguage;

    // Format the name prefix
    nLength = sprintf(szNamePrefix, "%s\\", szFilePrefix);
    return CreatePatchPrefix(haPatch, szNamePrefix, &szNamePrefix[nLength]);
}

//
// Find match in Starcraft II patch MPQs
// Match a LST file in the root directory if the MPQ with any of the file in subdirectories
//
// The problem:
// Base:  enGB-md5.lst
// Patch: Campaigns\Liberty.SC2Campaign\enGB.SC2Assets\enGB-md5.lst
//        Campaigns\Liberty.SC2Campaign\enGB.SC2Data\enGB-md5.lst
//        Campaigns\LibertyStory.SC2Campaign\enGB.SC2Data\enGB-md5.lst
//        Campaigns\LibertyStory.SC2Campaign\enGB.SC2Data\enGB-md5.lst Mods\Core.SC2Mod\enGB.SC2Assets\enGB-md5.lst
//        Mods\Core.SC2Mod\enGB.SC2Data\enGB-md5.lst
//        Mods\Liberty.SC2Mod\enGB.SC2Assets\enGB-md5.lst
//        Mods\Liberty.SC2Mod\enGB.SC2Data\enGB-md5.lst
//        Mods\LibertyMulti.SC2Mod\enGB.SC2Data\enGB-md5.lst
//
// Solution:
// We need to match the file by its MD5
//

static bool FindPatchPrefix_SC2(TMPQArchive * haBase, TMPQArchive * haPatch)
{
    TFileEntry * pFileTableEnd;
    TFileEntry * pFileEntry;
    TFileEntry * pBaseEntry;
    const char * szPlainName;
    char * szLstFileName;
    size_t cchWorkBuffer = 0x400;
    size_t cchBaseName;
    size_t cchDirName;
    bool bResult = false;

    // Find a *-md5.lst file in the base archive
    pBaseEntry = FindMd5ListFile(haBase);
    if(pBaseEntry == NULL)
        return false;
    cchBaseName = strlen(pBaseEntry->szFileName) + 1;

    // Allocate working buffer for merging LST file
    szLstFileName = STORM_ALLOC(char, cchWorkBuffer);
    if(szLstFileName != NULL)
    {
        // Find that file in the patch MPQ
        pFileTableEnd = haPatch->pFileTable + haPatch->dwFileTableSize;
        for(pFileEntry = haPatch->pFileTable; pFileEntry < pFileTableEnd; pFileEntry++)
        {
            // Find the "(patch_metadata)" file within that folder
            // Note that the file is always relatively small and contains the patch prefix
            // Checking for file size greatly speeds up the search process
            if(pFileEntry->szFileName && !(pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE) && (0 < pFileEntry->dwFileSize && pFileEntry->dwFileSize < 0x40))
            {
                // If the plain file name matches, we need to check its MD5
                szPlainName = GetPlainFileName(pFileEntry->szFileName);
                cchDirName = (size_t)(szPlainName - pFileEntry->szFileName);

                // The file name must not too long and must be PATCH_METADATA_NAME
                if((cchDirName + cchBaseName) < cchWorkBuffer && _stricmp(szPlainName, PATCH_METADATA_NAME) == 0)
                {
                    // Construct the name of the eventuall LST file
                    memcpy(szLstFileName, pFileEntry->szFileName, cchDirName);
                    memcpy(szLstFileName + cchDirName, pBaseEntry->szFileName, cchBaseName);

                    // If there is the "*-md5.lst" file in that directory, we check its MD5
                    if(IsMatchingPatchFile(haPatch, szLstFileName, pBaseEntry->md5))
                    {
                        bResult = CreatePatchPrefix(haPatch, pFileEntry->szFileName, szPlainName);
                        break;
                    }
                }
            }
        }

        // Free the work buffer
        STORM_FREE(szLstFileName);
    }

    return bResult;
}

static bool FindPatchPrefix(TMPQArchive * haBase, TMPQArchive * haPatch, const char * szPatchPathPrefix)
{
    // If the patch prefix was explicitly entered, we use that one
    if(szPatchPathPrefix != NULL)
        return CreatePatchPrefix(haPatch, szPatchPathPrefix, szPatchPathPrefix + strlen(szPatchPathPrefix));

    // Patches for World of Warcraft - mostly the do not use prefix.
    // Those who do, they have the (patch_metadata) file present in the "base" subdirectory.
    // All patches that use patch prefix have the "base\\(patch_metadata) file present
    if(GetFileEntryAny(haPatch, "base\\" PATCH_METADATA_NAME))
        return FindPatchPrefix_WoW_13164_13623(haBase, haPatch);

    // Updates for Starcraft II
    // Match: LocalizedData\GameHotkeys.txt <==> Campaigns\Liberty.SC2Campaign\enGB.SC2Data\LocalizedData\GameHotkeys.txt 
    // All Starcraft II base archives seem to have the file "StreamingBuckets.txt" present
    if(GetFileEntryAny(haBase, "StreamingBuckets.txt"))
        return FindPatchPrefix_SC2(haBase, haPatch);

    // Diablo III patch MPQs don't use patch prefix
    // Hearthstone MPQs don't use patch prefix
    CreatePatchPrefix(haPatch, NULL, NULL);
    return true;
}

//-----------------------------------------------------------------------------
// Public functions (StormLib internals)

bool IsIncrementalPatchFile(const void * pvData, DWORD cbData, LPDWORD pdwPatchedFileSize)
{
    TPatchHeader * pPatchHeader = (TPatchHeader *)pvData;
    BLIZZARD_BSDIFF40_FILE DiffFile;
    DWORD dwPatchType;

    if(cbData >= sizeof(TPatchHeader) + sizeof(BLIZZARD_BSDIFF40_FILE))
    {
        dwPatchType = BSWAP_INT32_UNSIGNED(pPatchHeader->dwPatchType);
        if(dwPatchType == 0x30445342)
        {
            // Give the caller the patch file size
            if(pdwPatchedFileSize != NULL)
            {
                Decompress_RLE((LPBYTE)&DiffFile, sizeof(BLIZZARD_BSDIFF40_FILE), (LPBYTE)(pPatchHeader + 1), sizeof(BLIZZARD_BSDIFF40_FILE));
                DiffFile.NewFileSize = BSWAP_INT64_UNSIGNED(DiffFile.NewFileSize);
                *pdwPatchedFileSize = (DWORD)DiffFile.NewFileSize;
                return true;
            }
        }
    }

    return false;
}

//
// Note: The patch may either be applied to the base file or to the previous version
// In Starcraft II, Mods\Core.SC2Mod\Base.SC2Data, file StreamingBuckets.txt:
//
// Base file MD5: 31376b0344b6df59ad009d4296125539
//
// s2-update-base-23258: from 31376b0344b6df59ad009d4296125539 to 941a82683452e54bf024a8d491501824
// s2-update-base-24540: from 31376b0344b6df59ad009d4296125539 to 941a82683452e54bf024a8d491501824
// s2-update-base-26147: from 31376b0344b6df59ad009d4296125539 to d5d5253c762fac6b9761240288a0771a
// s2-update-base-28522: from 31376b0344b6df59ad009d4296125539 to 5a76c4b356920aab7afd22e0e1913d7a
// s2-update-base-30508: from 31376b0344b6df59ad009d4296125539 to 8cb0d4799893fe801cc78ae4488a3671
// s2-update-base-32283: from 31376b0344b6df59ad009d4296125539 to 8cb0d4799893fe801cc78ae4488a3671
//
// We don't keep all intermediate versions in memory, as it would cause massive
// memory usage during patching process. A prime example is the file
// DBFilesClient\\Item-Sparse.db2 from locale-enGB.MPQ (WoW 16965), which has
// 9 patches in a row, each requiring 70 MB memory (35 MB patch data + 35 MB work buffer)
//

int PatchFileData(TMPQFile * hf)
{
    TMPQFile * hfBase = hf;
    TMPQFile * hfPrev = hf;
    int nError = ERROR_SUCCESS;

    // We need to calculate the MD5 of the entire file
    assert(hf->pbFileData != NULL);
    assert(hf->cbFileData != 0);
    CalculateDataBlockHash(hf->pbFileData, hf->cbFileData, hf->FileDataMD5);

    // Apply all patches
    for(hf = hf->hfPatch; hf != NULL; hf = hf->hfPatch)
    {
        // This must be true
        assert(hf->pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE);

        // Make sure that the patch data is loaded
        nError = LoadFilePatch(hf);
        if(nError != ERROR_SUCCESS)
            break;

        // Apply the patch
        nError = ApplyFilePatch(hfBase, hfPrev, hf);
        if(nError != ERROR_SUCCESS)
            break;

        // Only keep base file version and previous version
        if(hfPrev != hfBase)
            FreePatchData(hfPrev);

        // Is this the last patch in the chain?
        if(hf->hfPatch == NULL)
            break;
        hfPrev = hf;
    }

    // When done, we need to rewrite the base file data
    // with the last of the patch chain
    if(nError == ERROR_SUCCESS)
    {
        // Free the base file data
        STORM_FREE(hfBase->pbFileData);

        // Switch the latest patched data to the base file
        hfBase->pbFileData = hf->pbFileData;
        hfBase->cbFileData = hf->cbFileData;
        hf->pbFileData = NULL;
        hf->cbFileData = 0;
    }
    return nError;
}

//-----------------------------------------------------------------------------
// Public functions

//
// Patch prefix is the path subdirectory where the patched files are within MPQ.
//
// Example 1:
// Main MPQ:  locale-enGB.MPQ
// Patch MPQ: wow-update-12694.MPQ
// File in main MPQ: DBFilesClient\Achievement.dbc
// File in patch MPQ: enGB\DBFilesClient\Achievement.dbc
// Path prefix: enGB
//
// Example 2:
// Main MPQ:  expansion1.MPQ
// Patch MPQ: wow-update-12694.MPQ
// File in main MPQ: DBFilesClient\Achievement.dbc
// File in patch MPQ: Base\DBFilesClient\Achievement.dbc
// Path prefix: Base
//

bool WINAPI SFileOpenPatchArchive(
    HANDLE hMpq,
    const TCHAR * szPatchMpqName,
    const char * szPatchPathPrefix,
    DWORD dwFlags)
{
    TMPQArchive * haPatch;
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    HANDLE hPatchMpq = NULL;
    int nError = ERROR_SUCCESS;

    // Keep compiler happy
    dwFlags = dwFlags;

    // Verify input parameters
    if(!IsValidMpqHandle(hMpq))
        nError = ERROR_INVALID_HANDLE;
    if(szPatchMpqName == NULL || *szPatchMpqName == 0)
        nError = ERROR_INVALID_PARAMETER;

    //
    // We don't allow adding patches to archives that have been open for write
    //
    // Error scenario:
    //
    // 1) Open archive for writing
    // 2) Modify or replace a file
    // 3) Add patch archive to the opened MPQ
    // 4) Read patched file
    // 5) Now what ?
    //

    if(nError == ERROR_SUCCESS)
    {
        if(!(ha->dwFlags & MPQ_FLAG_READ_ONLY))
            nError = ERROR_ACCESS_DENIED;
    }

    // Open the archive like it is normal archive
    if(nError == ERROR_SUCCESS)
    {
        if(!SFileOpenArchive(szPatchMpqName, 0, MPQ_OPEN_READ_ONLY | MPQ_OPEN_PATCH, &hPatchMpq))
            return false;
        haPatch = (TMPQArchive *)hPatchMpq;

        // We need to remember the proper patch prefix to match names of patched files
        FindPatchPrefix(ha, (TMPQArchive *)hPatchMpq, szPatchPathPrefix);

        // Now add the patch archive to the list of patches to the original MPQ
        while(ha != NULL)
        {
            if(ha->haPatch == NULL)
            {
                haPatch->haBase = ha;
                ha->haPatch = haPatch;
                return true;
            }

            // Move to the next archive
            ha = ha->haPatch;
        }

        // Should never happen
        nError = ERROR_CAN_NOT_COMPLETE;
    }

    SetLastError(nError);
    return false;
}

bool WINAPI SFileIsPatchedArchive(HANDLE hMpq)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;

    // Verify input parameters
    if(!IsValidMpqHandle(hMpq))
        return false;

    return (ha->haPatch != NULL);
}
