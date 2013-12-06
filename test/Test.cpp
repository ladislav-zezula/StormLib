/*****************************************************************************/
/* StormLibTest.cpp                       Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* Test module for StormLib                                                  */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 25.03.03  1.00  Lad  The first version of StormLibTest.cpp                */
/*****************************************************************************/

#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_DEPRECATE
#define __INCLUDE_CRYPTOGRAPHY__
#define __STORMLIB_SELF__                   // Don't use StormLib.lib
#include <stdio.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "../src/StormLib.h"
#include "../src/StormCommon.h"

#include "TLogHelper.cpp"                   // Helper class for showing test results

#ifdef _MSC_VER
#pragma warning(disable: 4505)              // 'XXX' : unreferenced local function has been removed
#pragma comment(lib, "winmm.lib")
#endif

//------------------------------------------------------------------------------
// Defines

#ifdef PLATFORM_WINDOWS
#define WORK_PATH_ROOT "E:\\Multimedia\\MPQs"
#endif

#ifdef PLATFORM_LINUX
#define WORK_PATH_ROOT "/home/ladik/MPQs"
#endif

#ifdef PLATFORM_MAC
#define WORK_PATH_ROOT "/Users/sam/StormLib/test"
#endif

// Global for the work MPQ
static const char * szMpqSubDir   = "1996 - Test MPQs";
static const char * szMpqPatchDir = "1996 - Test MPQs\\patches";

typedef int (*ARCHIVE_TEST)(const char * szMpqName);

//-----------------------------------------------------------------------------
// Testing data

static DWORD AddFlags[] = 
{
//  Compression          Encryption             Fixed key           Single Unit            Sector CRC
    0                 |  0                   |  0                 | 0                    | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | 0,
    0                 |  0                   |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  0                   |  0                 | 0                    | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | 0,
    MPQ_FILE_IMPLODE  |  0                   |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  0                   |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_COMPRESS |  0                   |  0                 | 0                    | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | 0,
    MPQ_FILE_COMPRESS |  0                   |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_COMPRESS |  0                   |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | MPQ_FILE_SECTOR_CRC,
    0xFFFFFFFF
};

static DWORD Compressions[] = 
{
    MPQ_COMPRESSION_ADPCM_MONO | MPQ_COMPRESSION_HUFFMANN,
    MPQ_COMPRESSION_ADPCM_STEREO | MPQ_COMPRESSION_HUFFMANN,
    MPQ_COMPRESSION_PKWARE,
    MPQ_COMPRESSION_ZLIB,
    MPQ_COMPRESSION_BZIP2
};

static const wchar_t szUnicodeName1[] = {   // Czech
    0x010C, 0x0065, 0x0073, 0x006B, 0x00FD, _T('.'), _T('m'), _T('p'), _T('q'), 0
};

static const wchar_t szUnicodeName2[] = {   // Russian
    0x0420, 0x0443, 0x0441, 0x0441, 0x043A, 0x0438, 0x0439, _T('.'), _T('m'), _T('p'), _T('q'), 0
};

static const wchar_t szUnicodeName3[] = {   // Greek
    0x03B5, 0x03BB, 0x03BB, 0x03B7, 0x03BD, 0x03B9, 0x03BA, 0x03AC, _T('.'), _T('m'), _T('p'), _T('q'), 0
};

static const wchar_t szUnicodeName4[] = {   // Chinese
    0x65E5, 0x672C, 0x8A9E, _T('.'), _T('m'), _T('p'), _T('q'), 0
};

static const wchar_t szUnicodeName5[] = {   // Japanese
    0x7B80, 0x4F53, 0x4E2D, 0x6587, _T('.'), _T('m'), _T('p'), _T('q'), 0
};

static const wchar_t szUnicodeName6[] = {   // Arabic
    0x0627, 0x0644, 0x0639, 0x0639, 0x0631, 0x0628, 0x064A, 0x0629, _T('.'), _T('m'), _T('p'), _T('q'), 0
};

static const char * PatchList_WoW_OldWorld13286[] =
{
    "MPQ_2012_v4_OldWorld.MPQ",
    "wow-update-oldworld-13154.MPQ",
    "wow-update-oldworld-13286.MPQ",
    NULL
};

static const char * PatchList_WoW15050[] =
{
    "MPQ_2013_v4_world.MPQ",
    "wow-update-13164.MPQ",
    "wow-update-13205.MPQ",
    "wow-update-13287.MPQ",
    "wow-update-13329.MPQ",
    "wow-update-13596.MPQ",
    "wow-update-13623.MPQ",
    "wow-update-base-13914.MPQ",
    "wow-update-base-14007.MPQ",
    "wow-update-base-14333.MPQ",
    "wow-update-base-14480.MPQ",
    "wow-update-base-14545.MPQ",
    "wow-update-base-14946.MPQ",
    "wow-update-base-15005.MPQ",
    "wow-update-base-15050.MPQ",
    NULL
};

static const char * PatchList_WoW16965[] = 
{
    "MPQ_2013_v4_locale-enGB.MPQ",
    "wow-update-enGB-16016.MPQ",
    "wow-update-enGB-16048.MPQ",
    "wow-update-enGB-16057.MPQ",
    "wow-update-enGB-16309.MPQ",
    "wow-update-enGB-16357.MPQ",
    "wow-update-enGB-16516.MPQ",
    "wow-update-enGB-16650.MPQ",
    "wow-update-enGB-16844.MPQ",
    "wow-update-enGB-16965.MPQ",
    NULL
};

//-----------------------------------------------------------------------------
// Local file functions

// Definition of the path separator
#ifdef PLATFORM_WINDOWS
#define PATH_SEPARATOR   '\\'           // Path separator for Windows platforms
#else
#define PATH_SEPARATOR   '/'            // Path separator for Windows platforms
#endif

// This must be the directory where our test MPQs are stored.
// We also expect a subdirectory named 
static char szMpqDirectory[MAX_PATH];
size_t cchMpqDirectory = 0;

static size_t ConvertSha1ToText(const unsigned char * sha1_digest, char * szSha1Text)
{
    const char * szTable = "0123456789abcdef";

    for(size_t i = 0; i < SHA1_DIGEST_SIZE; i++)
    {
        *szSha1Text++ = szTable[(sha1_digest[0] >> 0x04)];
        *szSha1Text++ = szTable[(sha1_digest[0] & 0x0F)];
        sha1_digest++;
    }

    *szSha1Text = 0;
    return (SHA1_DIGEST_SIZE * 2);
}

#ifdef _UNICODE
static const TCHAR * GetShortPlainName(const TCHAR * szFileName)
{
    const TCHAR * szPlainName = szFileName;
    const TCHAR * szPlainEnd = szFileName + _tcslen(szFileName);

    // If there is terminating slash or backslash, move to it
    while(szFileName < szPlainEnd)
    {
        if(szFileName[0] == _T('\\') || szFileName[0] == _T('/'))
            szPlainName = szFileName + 1;
        szFileName++;
    }

    // If the name is still too long, cut it
    if((szPlainEnd - szPlainName) > 50)
        szPlainName = szPlainEnd - 50;
    return szPlainName;
}

static void CreateFullPathName(TCHAR * szBuffer, const char * szSubDir, const char * szNamePart1, const char * szNamePart2 = NULL)
{
    size_t nLength;

    // Copy the master MPQ directory
    mbstowcs(szBuffer, szMpqDirectory, cchMpqDirectory);
    szBuffer += cchMpqDirectory;

    // Append the subdirectory, if any
    if(szSubDir != NULL && (nLength = strlen(szSubDir)) != 0)
    {
        // No leading or trailing separators allowed
        assert(szSubDir[0] != '/' && szSubDir[0] != '\\');
        assert(szSubDir[nLength - 1] != '/' && szSubDir[nLength - 1] != '\\');

        // Append file path separator
        *szBuffer++ = PATH_SEPARATOR;

        // Copy the subdirectory
        mbstowcs(szBuffer, szSubDir, nLength);
        
        // Fix the path separators
        for(size_t i = 0; i < nLength; i++)
            szBuffer[i] = (szBuffer[i] != '\\' && szBuffer[i] != '/') ? szBuffer[i] : PATH_SEPARATOR;
        
        // Move the buffer pointer
        szBuffer += nLength;
    }

    // Copy the file name, if any
    if(szNamePart1 != NULL && (nLength = strlen(szNamePart1)) != 0)
    {
        // No path separator can be there
        assert(strchr(szNamePart1, '\\') == NULL);
        assert(strchr(szNamePart1, '/') == NULL);

        // Append file path separator
        *szBuffer++ = PATH_SEPARATOR;

        // Copy the file name
        mbstowcs(szBuffer, szNamePart1, nLength);
        szBuffer += nLength;
    }

    // Append the second part of the name
    if(szNamePart2 != NULL && (nLength = strlen(szNamePart2)) != 0)
    {
        // Copy the file name
        mbstowcs(szBuffer, szNamePart2, nLength);
        szBuffer += nLength;
    }

    // Terminate the buffer with zero
    *szBuffer = 0;
}

TFileStream * FileStream_OpenFile(const char * szFileName, DWORD dwStreamFlags)
{
    TFileStream * pStream = NULL;
    TCHAR * szFileNameT;
    size_t nLength = strlen(szFileName);

    // Allocate buffer for the UNICODE file name
    szFileNameT = STORM_ALLOC(TCHAR, nLength + 1);
    if(szFileNameT != NULL)
    {
        CopyFileName(szFileNameT, szFileName, nLength);
        pStream = FileStream_OpenFile(szFileNameT, dwStreamFlags);
        STORM_FREE(szFileNameT);
    }

    // Return what we got
    return pStream;
}
#endif

static const char * GetShortPlainName(const char * szFileName)
{
    const char * szPlainName = szFileName;
    const char * szPlainEnd = szFileName + strlen(szFileName);

    // If there is terminating slash or backslash, move to it
    while(szFileName < szPlainEnd)
    {
        if(szFileName[0] == '\\' || szFileName[0] == '/')
            szPlainName = szFileName + 1;
        szFileName++;
    }

    // If the name is still too long, cut it
    if((szPlainEnd - szPlainName) > 50)
        szPlainName = szPlainEnd - 50;
    return szPlainName;
}

static void CreateFullPathName(char * szBuffer, const char * szSubDir, const char * szNamePart1, const char * szNamePart2 = NULL)
{
    size_t nLength;

    // Copy the master MPQ directory
    memcpy(szBuffer, szMpqDirectory, cchMpqDirectory);
    szBuffer += cchMpqDirectory;

    // Append the subdirectory, if any
    if(szSubDir != NULL && (nLength = strlen(szSubDir)) != 0)
    {
        // No leading or trailing separator must be there
        assert(szSubDir[0] != '/' && szSubDir[0] != '\\');
        assert(szSubDir[nLength - 1] != '/' && szSubDir[nLength - 1] != '\\');

        // Append file path separator
        *szBuffer++ = PATH_SEPARATOR;

        // Copy the subdirectory
        memcpy(szBuffer, szSubDir, nLength);

        // Fix the path separators
        for(size_t i = 0; i < nLength; i++)
            szBuffer[i] = (szBuffer[i] != '\\' && szBuffer[i] != '/') ? szBuffer[i] : PATH_SEPARATOR;
        
        // Move the buffer pointer
        szBuffer += nLength;
    }

    // Copy the file name, if any
    if(szNamePart1 != NULL && (nLength = strlen(szNamePart1)) != 0)
    {
        // No path separator can be there
        assert(strchr(szNamePart1, '\\') == NULL);
        assert(strchr(szNamePart1, '/') == NULL);

        // Append file path separator
        *szBuffer++ = PATH_SEPARATOR;

        // Copy the file name
        memcpy(szBuffer, szNamePart1, nLength);
        szBuffer += nLength;
    }

    // Append the second part of the name
    if(szNamePart2 != NULL && (nLength = strlen(szNamePart2)) != 0)
    {
        // Copy the file name
        memcpy(szBuffer, szNamePart2, nLength);
        szBuffer += nLength;
    }

    // Terminate the buffer with zero
    *szBuffer = 0;
}

static int InitializeMpqDirectory(char * argv[], int argc)
{
    TLogHelper Logger("InitWorkDir");
    TFileStream * pStream;
    const char * szWhereFrom = NULL;
    const char * szDirName;
    TCHAR szFileName[MAX_PATH];

#ifdef _MSC_VER
    // Mix the random number generator
    srand(GetTickCount());
#endif

    // Retrieve the name of the MPQ directory
    if(argc > 1 && argv[1] != NULL)
    {
        szWhereFrom = "entered at command line";
        szDirName = argv[1];
    }
    else
    {
        szWhereFrom = "default";
        szDirName = WORK_PATH_ROOT;
    }

    // Copy the name of the MPQ directory.
    strcpy(szMpqDirectory, szDirName);
    cchMpqDirectory = strlen(szMpqDirectory);

    // Cut trailing slashes and/or backslashes
    while(cchMpqDirectory > 0 && szMpqDirectory[cchMpqDirectory - 1] == '/' || szMpqDirectory[cchMpqDirectory - 1] == '\\')
        cchMpqDirectory--;
    szMpqDirectory[cchMpqDirectory] = 0;

    // Print the work directory info
    Logger.PrintMessage("Work directory %s (%s)", szMpqDirectory, szWhereFrom);

    // Verify if the work MPQ directory is writable
    CreateFullPathName(szFileName, NULL, "TestFile.bin");
    pStream = FileStream_CreateFile(szFileName, 0);
    if(pStream == NULL)
        return Logger.PrintError("MPQ subdirectory is not writable");

    // Close the stream
    FileStream_Close(pStream);

    // Verify if the working directory exists and if there is a subdirectory with the file name
    CreateFullPathName(szFileName, szMpqSubDir, "ListFile_Blizzard.txt");
    pStream = FileStream_OpenFile(szFileName, STREAM_FLAG_READ_ONLY);
    if(pStream == NULL)
        return Logger.PrintError(_T("The main listfile (%s) was not found. Check your paths"), szFileName);

    // Close the stream
    FileStream_Close(pStream);
    return ERROR_SUCCESS;                                
}

static int GetFilePatchCount(TLogHelper * pLogger, HANDLE hMpq, const char * szFileName)
{
    TCHAR * szPatchName;
    HANDLE hFile;
    TCHAR szPatchChain[0x400];
    int nPatchCount = 0;
    int nError = ERROR_SUCCESS;

    // Open the MPQ file
    if(SFileOpenFileEx(hMpq, szFileName, 0, &hFile))
    {
        // Notify the user
        pLogger->PrintProgress("Verifying patch chain for %s ...", GetShortPlainName(szFileName));

        // Query the patch chain
        if(!SFileGetFileInfo(hFile, SFileInfoPatchChain, szPatchChain, sizeof(szPatchChain), NULL))
            nError = pLogger->PrintError("Failed to retrieve the patch chain on %s", szFileName);

        // Is there anything at all in the patch chain?
        if(nError == ERROR_SUCCESS && szPatchChain[0] == 0)
        {
            pLogger->PrintError("The patch chain for %s is empty", szFileName);
            nError = ERROR_FILE_CORRUPT;
        }

        // Now calculate the number of patches
        if(nError == ERROR_SUCCESS)
        {
            // Get the pointer to the patch
            szPatchName = szPatchChain;

            // Skip the base name
            for(;;)
            {
                // Skip the current name
                szPatchName = szPatchName + _tcslen(szPatchName) + 1;
                if(szPatchName[0] == 0)
                    break;

                // Increment number of patches
                nPatchCount++;
            }
        }

        SFileCloseFile(hFile);
    }
    else
    {
        pLogger->PrintError("Failed to open file %s", szFileName);
    }

    return nPatchCount;
}

static int VerifyFilePatchCount(TLogHelper * pLogger, HANDLE hMpq, const char * szFileName, int nExpectedPatchCount)
{
    int nPatchCount = 0;

    // Retrieve the patch count
    pLogger->PrintProgress("Verifying patch count for %s ...", szFileName);
    nPatchCount = GetFilePatchCount(pLogger, hMpq, szFileName);

    // Check if there are any patches at all
    if(nExpectedPatchCount != 0 && nPatchCount == 0)
    {
        pLogger->PrintMessage("There are no patches beyond %s", szFileName);
        return ERROR_FILE_CORRUPT;
    }

    // Check if the number of patches fits
    if(nPatchCount != nExpectedPatchCount)
    {
        pLogger->PrintMessage("Unexpected number of patches for %s", szFileName);
        return ERROR_FILE_CORRUPT;
    }

    return ERROR_SUCCESS;
}

static int CreateEmptyFile(TLogHelper * pLogger, const char * szPlainName, ULONGLONG FileSize, TCHAR * szBuffer)
{
    TFileStream * pStream;

    // Notify the user
    pLogger->PrintProgress("Creating empty file %s ...", szPlainName);

    // Construct the full path and crete the file
    CreateFullPathName(szBuffer, NULL, szPlainName);
    pStream = FileStream_CreateFile(szBuffer, STREAM_PROVIDER_LINEAR | BASE_PROVIDER_FILE);
    if(pStream == NULL)
        return pLogger->PrintError(_T("Failed to create file %s"), szBuffer);

    // Write the required size
    FileStream_SetSize(pStream, FileSize);
    FileStream_Close(pStream);
    return ERROR_SUCCESS;
}

static int WriteMpqUserDataHeader(
    TLogHelper * pLogger,
    TFileStream * pStream,
    ULONGLONG ByteOffset,
    DWORD dwByteCount)
{
    TMPQUserData UserData;
    int nError = ERROR_SUCCESS;

    // Notify the user
    pLogger->PrintProgress("Writing user data header...");

    // Fill the user data header
    UserData.dwID = ID_MPQ_USERDATA;
    UserData.cbUserDataSize = dwByteCount;
    UserData.dwHeaderOffs = (dwByteCount + sizeof(TMPQUserData));
    UserData.cbUserDataHeader = dwByteCount / 2;
    if(!FileStream_Write(pStream, &ByteOffset, &UserData, sizeof(TMPQUserData)))
        nError = GetLastError();
    return nError;
}

static int WriteFileData(
    TLogHelper * pLogger,
    TFileStream * pStream,
    ULONGLONG ByteOffset,
    ULONGLONG ByteCount)
{
    ULONGLONG SaveByteCount = ByteCount;
    ULONGLONG BytesWritten = 0;
    LPBYTE pbDataBuffer;
    DWORD cbDataBuffer = 0x10000;
    int nError = ERROR_SUCCESS;

    // Write some data
    pbDataBuffer = new BYTE[cbDataBuffer];
    if(pbDataBuffer != NULL)
    {
        memset(pbDataBuffer, 0, cbDataBuffer);
        strcpy((char *)pbDataBuffer, "This is a test data written to a file.");

        // Perform the write
        while(ByteCount > 0)
        {
            DWORD cbToWrite = (ByteCount > cbDataBuffer) ? cbDataBuffer : (DWORD)ByteCount;

            // Notify the user
            pLogger->PrintProgress("Writing file data (%I64u of %I64u) ...", BytesWritten, SaveByteCount);

            // Write the data
            if(!FileStream_Write(pStream, &ByteOffset, pbDataBuffer, cbToWrite))
            {
                nError = GetLastError();
                break;
            }

            BytesWritten += cbToWrite;
            ByteOffset += cbToWrite;
            ByteCount -= cbToWrite;
        }

        delete [] pbDataBuffer;
    }
    return nError;
}

static int CopyFileData(
    TLogHelper * pLogger,
    TFileStream * pStream1,
    TFileStream * pStream2,
    ULONGLONG ByteOffset,
    ULONGLONG ByteCount)
{
    ULONGLONG BytesCopied = 0;
    ULONGLONG EndOffset = ByteOffset + ByteCount;
    LPBYTE pbCopyBuffer;
    DWORD BytesToRead;
    DWORD BlockLength = 0x100000;
    int nError = ERROR_SUCCESS;

    // Allocate copy buffer
    pbCopyBuffer = STORM_ALLOC(BYTE, BlockLength);
    if(pbCopyBuffer != NULL)
    {
        while(ByteOffset < EndOffset)
        {
            // Notify the user
            pLogger->PrintProgress("Copying %I64u of %I64u ...", BytesCopied, ByteCount);

            // Read source
            BytesToRead = ((EndOffset - ByteOffset) > BlockLength) ? BlockLength : (DWORD)(EndOffset - ByteOffset);
            if(!FileStream_Read(pStream1, &ByteOffset, pbCopyBuffer, BytesToRead))
            {
                nError = GetLastError();
                break;
            }

            // Write to the destination file
            if(!FileStream_Write(pStream2, NULL, pbCopyBuffer, BytesToRead))
            {
                nError = GetLastError();
                break;
            }

            BytesCopied += BytesToRead;
            ByteOffset += BytesToRead;
        }

        STORM_FREE(pbCopyBuffer);
    }

    return nError;
}

// Support function for copying file
static int CreateMpqCopy(
    TLogHelper * pLogger,
    const char * szPlainName,
    const char * szFileCopy,
    TCHAR * szBuffer,
    ULONGLONG PreMpqDataSize = 0,
    ULONGLONG UserDataSize = 0)
{
    TFileStream * pStream1;             // Source file
    TFileStream * pStream2;             // Target file
    ULONGLONG ByteOffset = 0;
    ULONGLONG FileSize = 0;
    TCHAR szFileName1[MAX_PATH];
    TCHAR szFileName2[MAX_PATH];
    int nError = ERROR_SUCCESS;

    // Notify the user
    pLogger->PrintProgress("Creating copy of %s ...", szPlainName);

    // Construct both file names. Check if they are not the same
    CreateFullPathName(szFileName1, szMpqSubDir, szPlainName);
    CreateFullPathName(szFileName2, NULL, szFileCopy);
    if(!_tcsicmp(szFileName1, szFileName2))
    {
        pLogger->PrintError("Failed to create copy of MPQ (the copy name is the same like the original name)");
        return ERROR_CAN_NOT_COMPLETE;
    }

    // Open the source file
    pStream1 = FileStream_OpenFile(szFileName1, STREAM_FLAG_READ_ONLY);
    if(pStream1 == NULL)
    {
        pLogger->PrintError(_T("Failed to open the source file %s"), szFileName1);
        return ERROR_CAN_NOT_COMPLETE;
    }

    // Create the destination file
    pStream2 = FileStream_CreateFile(szFileName2, 0);
    if(pStream2 != NULL)
    {
        // If we should write some pre-MPQ data to the target file, do it
        if(PreMpqDataSize != 0)
        {
            nError = WriteFileData(pLogger, pStream2, ByteOffset, PreMpqDataSize);
            ByteOffset += PreMpqDataSize;
        }

        // If we should write some MPQ user data, write the header first
        if(UserDataSize != 0)
        {
            nError = WriteMpqUserDataHeader(pLogger, pStream2, ByteOffset, (DWORD)UserDataSize);
            ByteOffset += sizeof(TMPQUserData);

            nError = WriteFileData(pLogger, pStream2, ByteOffset, UserDataSize);
            ByteOffset += UserDataSize;
        }

        // Copy the file data from the source file to the destination file
        FileStream_GetSize(pStream1, &FileSize);
        if(FileSize != 0)
        {
            nError = CopyFileData(pLogger, pStream1, pStream2, 0, FileSize);
            ByteOffset += FileSize;
        }
        FileStream_Close(pStream2);
    }

    // Close the source file
    FileStream_Close(pStream1);

    if(szBuffer != NULL)
        _tcscpy(szBuffer, szFileName2);
    if(nError != ERROR_SUCCESS)
        pLogger->PrintError("Failed to create copy of MPQ");
    return nError;
}

static void WINAPI AddFileCallback(void * pvUserData, DWORD dwBytesWritten, DWORD dwTotalBytes, bool bFinalCall)
{
    TLogHelper * pLogger = (TLogHelper *)pvUserData;

    // Keep compiler happy
    bFinalCall = bFinalCall;

    pLogger->PrintProgress("Adding file (%s) (%u of %u) (%u of %u) ...", pLogger->UserString,
                                                                         pLogger->UserCount,
                                                                         pLogger->UserTotal,
                                                                         dwBytesWritten,
                                                                         dwTotalBytes);
}

static void WINAPI CompactCallback(void * pvUserData, DWORD dwWork, ULONGLONG BytesDone, ULONGLONG TotalBytes)
{
    TLogHelper * pLogger = (TLogHelper *)pvUserData;
    const char * szWork = NULL;

    switch(dwWork)
    {
        case CCB_CHECKING_FILES:
            szWork = "Checking files in archive";
            break;

        case CCB_CHECKING_HASH_TABLE:
            szWork = "Checking hash table";
            break;

        case CCB_COPYING_NON_MPQ_DATA:
            szWork = "Copying non-MPQ data";
            break;

        case CCB_COMPACTING_FILES:
            szWork = "Compacting files";
            break;

        case CCB_CLOSING_ARCHIVE:
            szWork = "Closing archive";
            break;
    }

    if(szWork != NULL)
    {
        if(pLogger != NULL)
            pLogger->PrintProgress("%s (%I64u of %I64u) ...", szWork, BytesDone, TotalBytes);
        else
            printf("%s (%I64u of %I64u) ...     \r", szWork, (DWORD)BytesDone, (DWORD)TotalBytes);
    }
}

//-----------------------------------------------------------------------------
// MPQ file utilities

#define TEST_FLAG_LOAD_FILES          0x00000001    // Test function should load all files in the MPQ
#define TEST_FLAG_HASH_FILES          0x00000002    // Test function should load all files in the MPQ
#define TEST_FLAG_PLAY_WAVES          0x00000004    // Play extracted WAVE files
#define TEST_FLAG_MOST_PATCHED        0x00000008    // Find the most patched file

struct TFileData
{
    DWORD dwBlockIndex;
    DWORD dwFileSize;
    DWORD dwFlags;
    DWORD dwReserved;                               // Alignment
    BYTE FileData[1];
};

static bool CheckIfFileIsPresent(TLogHelper * pLogger, HANDLE hMpq, const char * szFileName, bool bShouldExist)
{
    HANDLE hFile = NULL;

    if(SFileOpenFileEx(hMpq, szFileName, 0, &hFile))
    {
        if(bShouldExist == false)
            pLogger->PrintMessage("The file %s is present, but it should not be", szFileName);
        SFileCloseFile(hFile);
        return true;
    }
    else
    {
        if(bShouldExist)
            pLogger->PrintMessage("The file %s is not present, but it should be", szFileName);
        return false;
    }
}

static TFileData * LoadLocalFile(TLogHelper * pLogger, const char * szFileName, bool bMustSucceed)
{
    TFileStream * pStream;
    TFileData * pFileData = NULL;
    ULONGLONG FileSize = 0;
    size_t nAllocateBytes;

    // Notify the user
    if(pLogger != NULL)
        pLogger->PrintProgress("Loading local file ...");

    // Attempt to open the file
    pStream = FileStream_OpenFile(szFileName, STREAM_FLAG_READ_ONLY);
    if(pStream == NULL)
    {
        if(pLogger != NULL && bMustSucceed == true)
            pLogger->PrintError("Failed to open the file %s", szFileName);
        return NULL;
    }

    // Verify the size
    FileStream_GetSize(pStream, &FileSize);
    if((FileSize >> 0x20) == 0)
    {
        // Allocate space for the file
        nAllocateBytes = sizeof(TFileData) + (size_t)FileSize;
        pFileData = (TFileData *)STORM_ALLOC(BYTE, nAllocateBytes);
        if(pFileData != NULL)
        {
            // Make sure it;s properly zeroed
            memset(pFileData, 0, nAllocateBytes);
            pFileData->dwFileSize = (DWORD)FileSize;

            // Load to memory
            if(!FileStream_Read(pStream, NULL, pFileData->FileData, pFileData->dwFileSize))
            {
                STORM_FREE(pFileData);
                pFileData = NULL;
            }
        }
    }

    FileStream_Close(pStream);
    return pFileData;
}

static TFileData * LoadMpqFile(TLogHelper * pLogger, HANDLE hMpq, const char * szFileName)
{
    TFileData * pFileData = NULL;
    HANDLE hFile;
    DWORD dwFileSizeHi = 0xCCCCCCCC;
    DWORD dwFileSizeLo = 0;
    DWORD dwBytesRead;
    int nError = ERROR_SUCCESS;

    // Notify the user that we are loading a file from MPQ
    pLogger->PrintProgress("Loading file %s ...", GetShortPlainName(szFileName));

    // Open the file from MPQ
    if(!SFileOpenFileEx(hMpq, szFileName, 0, &hFile))
        nError = pLogger->PrintError("Failed to open the file %s", szFileName);

    // Get the size of the file
    if(nError == ERROR_SUCCESS)
    {
        dwFileSizeLo = SFileGetFileSize(hFile, &dwFileSizeHi);
        if(dwFileSizeLo == SFILE_INVALID_SIZE || dwFileSizeHi != 0)
            nError = pLogger->PrintError("Failed to query the file size");
    }

    // Allocate buffer for the file content
    if(nError == ERROR_SUCCESS)
    {
        pFileData = (TFileData *)STORM_ALLOC(BYTE, sizeof(TFileData) + dwFileSizeLo);
        if(pFileData == NULL)
        {
            pLogger->PrintError("Failed to allocate buffer for the file content");
            nError = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    // get the file index of the MPQ file
    if(nError == ERROR_SUCCESS)
    {
        // Store the file size
        memset(pFileData, 0, sizeof(TFileData) + dwFileSizeLo);
        pFileData->dwFileSize = dwFileSizeLo;

        // Retrieve the block index and file flags
        if(!SFileGetFileInfo(hFile, SFileInfoFileIndex, &pFileData->dwBlockIndex, sizeof(DWORD), NULL))
            nError = pLogger->PrintError("Failed retrieve the file index of %s", szFileName);
        if(!SFileGetFileInfo(hFile, SFileInfoFlags, &pFileData->dwFlags, sizeof(DWORD), NULL))
            nError = pLogger->PrintError("Failed retrieve the file flags of %s", szFileName);
    }

    // Load the entire file
    if(nError == ERROR_SUCCESS)
    {
        // Read the file data
        SFileReadFile(hFile, pFileData->FileData, dwFileSizeLo, &dwBytesRead, NULL);
        if(dwBytesRead != dwFileSizeLo)
            nError = pLogger->PrintError("Failed to read the content of the file %s", szFileName);
    }

    // Close the file and return what we got
    if(hFile != NULL)
        SFileCloseFile(hFile);
    if(nError != ERROR_SUCCESS)
        SetLastError(nError);
    return pFileData;
}

static bool CompareTwoFiles(TLogHelper * pLogger, TFileData * pFileData1, TFileData * pFileData2)
{
    // Compare the file size
    if(pFileData1->dwFileSize != pFileData2->dwFileSize)
    {
        pLogger->PrintErrorVa(_T("The files have different size (%u vs %u)"), pFileData1->dwFileSize, pFileData2->dwFileSize);
        SetLastError(ERROR_FILE_CORRUPT);
        return false;
    }

    // Compare the files
    for(DWORD i = 0; i < pFileData1->dwFileSize; i++)
    {
        if(pFileData1->FileData[i] != pFileData2->FileData[i])
        {
            pLogger->PrintErrorVa(_T("Files are different at offset %08X"), i);
            SetLastError(ERROR_FILE_CORRUPT);
            return false;
        }
    }

    // The files are identical
    return true;
}

static int SearchArchive(
    TLogHelper * pLogger,
    HANDLE hMpq,
    DWORD dwTestFlags = 0,
    DWORD * pdwFileCount = NULL,
    LPBYTE pbFileHash = NULL)
{
    SFILE_FIND_DATA sf;
    TFileData * pFileData;
    HANDLE hFind;
    DWORD dwFileCount = 0;
    hash_state md5state;
    char szMostPatched[MAX_PATH] = "";
    char szListFile[MAX_PATH];
    bool bFound = true;
    int nMaxPatchCount = 0;
    int nPatchCount = 0;
    int nError = ERROR_SUCCESS;

    // Construct the full name of the listfile
    CreateFullPathName(szListFile, szMpqSubDir, "ListFile_Blizzard.txt");

    // Prepare hashing
    md5_init(&md5state);

    // Initiate the MPQ search
    pLogger->PrintProgress("Searching the archive ...");
    hFind = SFileFindFirstFile(hMpq, "*", &sf, szListFile);
    if(hFind == NULL)
    {
        nError = GetLastError();
        nError = (nError == ERROR_NO_MORE_FILES) ? ERROR_SUCCESS : nError;
        return nError;
    }

    // Perform the search
    while(bFound == true)
    {
        // Increment number of files
        dwFileCount++;

        if(dwTestFlags & TEST_FLAG_MOST_PATCHED)
        {
            // Load the patch count
            nPatchCount = GetFilePatchCount(pLogger, hMpq, sf.cFileName);

            // Check if it's greater than maximum
            if(nPatchCount > nMaxPatchCount)
            {
                strcpy(szMostPatched, sf.cFileName);
                nMaxPatchCount = nPatchCount;
            }
        }

        // Load the file to memory, if required
        if(dwTestFlags & TEST_FLAG_LOAD_FILES)
        {
            // Load the entire file to the MPQ
            pFileData = LoadMpqFile(pLogger, hMpq, sf.cFileName);
            if(pFileData == NULL)
            {
                nError = pLogger->PrintError("Failed to load the file %s", sf.cFileName);
                break;
            }

            // Hash the file data, if needed
            if((dwTestFlags & TEST_FLAG_HASH_FILES) && !IsInternalMpqFileName(sf.cFileName))
                md5_process(&md5state, pFileData->FileData, pFileData->dwFileSize);

            // Play sound files, if required
            if((dwTestFlags & TEST_FLAG_PLAY_WAVES) && strstr(sf.cFileName, ".wav") != NULL)
            {
#ifdef _MSC_VER
                pLogger->PrintProgress("Playing sound %s", sf.cFileName);
                PlaySound((LPCTSTR)pFileData->FileData, NULL, SND_MEMORY);
#endif
            }

            STORM_FREE(pFileData);
        }

        bFound = SFileFindNextFile(hFind, &sf);
    }
    SFileFindClose(hFind);

    // Give the file count, if required
    if(pdwFileCount != NULL)
        pdwFileCount[0] = dwFileCount;

    // Give the hash, if required
    if(pbFileHash != NULL && (dwTestFlags & TEST_FLAG_HASH_FILES))
        md5_done(&md5state, pbFileHash);

    return nError;
}

static int CreateNewArchive_FullPath(TLogHelper * pLogger, const TCHAR * szMpqName, DWORD dwCreateFlags, DWORD dwMaxFileCount, HANDLE * phMpq)
{
    HANDLE hMpq = NULL;

    // Make sure that the MPQ is deleted
    _tremove(szMpqName);

    // Fix the flags
    dwCreateFlags |= (MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES);

    // Create the new MPQ
    if(!SFileCreateArchive(szMpqName, dwCreateFlags, dwMaxFileCount, &hMpq))
        return pLogger->PrintError(_T("Failed to create archive %s"), szMpqName);

    // Shall we close it right away?
    if(phMpq == NULL)
        SFileCloseArchive(hMpq);
    else
        *phMpq = hMpq;

    return ERROR_SUCCESS;
}

static int CreateNewArchive(TLogHelper * pLogger, const TCHAR * szPlainName, DWORD dwCreateFlags, DWORD dwMaxFileCount, HANDLE * phMpq)
{
    TCHAR szMpqName[MAX_PATH];

    CreateFullPathName(szMpqName, NULL, "StormLibTest_", NULL);
    _tcscat(szMpqName, szPlainName);
    return CreateNewArchive_FullPath(pLogger, szMpqName, dwCreateFlags, dwMaxFileCount, phMpq);
}

#ifdef _UNICODE
static int CreateNewArchive(TLogHelper * pLogger, const char * szPlainName, DWORD dwCreateFlags, DWORD dwMaxFileCount, HANDLE * phMpq)
{
    TCHAR szMpqName[MAX_PATH];

    CreateFullPathName(szMpqName, NULL, "StormLibTest_", szPlainName);
    return CreateNewArchive_FullPath(pLogger, szMpqName, dwCreateFlags, dwMaxFileCount, phMpq);
}
#endif

static int OpenExistingArchive(TLogHelper * pLogger, const char * szFileName, const char * szCopyName, HANDLE * phMpq)
{
    TCHAR szMpqName[MAX_PATH];
    HANDLE hMpq = NULL;
    DWORD dwFlags = 0;
    int nError = ERROR_SUCCESS;

    // We expect MPQ directory to be already prepared by InitializeMpqDirectory
    assert(szMpqDirectory[0] != 0);

    // At least one name must be entered
    assert(szFileName != NULL || szCopyName != NULL);

    // If both names entered, create a copy
    if(szFileName != NULL && szCopyName != NULL)
    {
        nError = CreateMpqCopy(pLogger, szFileName, szCopyName, szMpqName);
        if(nError != ERROR_SUCCESS)
            return nError;
    }
    
    // If only source name entered, open it for read-only access
    else if(szFileName != NULL && szCopyName == NULL)
    {
        CreateFullPathName(szMpqName, szMpqSubDir, szFileName);
        dwFlags |= MPQ_OPEN_READ_ONLY;
    }

    // If only target name entered, open it directly
    else if(szFileName == NULL && szCopyName != NULL)
    {
        CreateFullPathName(szMpqName, NULL, szCopyName);
    }

    // Is it an encrypted MPQ ?
    if(_tcsstr(szMpqName, _T(".MPQE")) != NULL)
        dwFlags |= MPQ_OPEN_ENCRYPTED;

    // Open the copied archive
    pLogger->PrintProgress("Opening archive %s ...", (szCopyName != NULL) ? szCopyName : szFileName);
    if(!SFileOpenArchive(szMpqName, 0, dwFlags, &hMpq))
        return pLogger->PrintError(_T("Failed to open archive %s"), szMpqName);

    // Store the archive handle or close the archive
    if(phMpq == NULL)
        SFileCloseArchive(hMpq);
    else
        *phMpq = hMpq;
    return nError;
}

static int OpenPatchedArchive(TLogHelper * pLogger, HANDLE * phMpq, const char * PatchList[])
{
    TCHAR szMpqName[MAX_PATH];
    HANDLE hMpq = NULL;
    int nError = ERROR_SUCCESS;

    // The first file is expected to be valid
    assert(PatchList[0] != NULL);

    // Open the primary MPQ
    CreateFullPathName(szMpqName, szMpqSubDir, PatchList[0]);
    pLogger->PrintProgress("Opening base MPQ %s ...", PatchList[0]);
    if(!SFileOpenArchive(szMpqName, 0, MPQ_OPEN_READ_ONLY, &hMpq))
        nError = pLogger->PrintError(_T("Failed to open the archive %s"), szMpqName);

    // Add all patches
    if(nError == ERROR_SUCCESS)
    {
        for(size_t i = 1; PatchList[i] != NULL; i++)
        {
            CreateFullPathName(szMpqName, szMpqPatchDir, PatchList[i]);
            pLogger->PrintProgress("Adding patch %s ...", PatchList[i]);
            if(!SFileOpenPatchArchive(hMpq, szMpqName, NULL, 0))
            {
                nError = pLogger->PrintError(_T("Failed to add patch %s ..."), szMpqName);
                break;
            }
        }
    }

    // Store the archive handle or close the archive
    if(phMpq == NULL)
        SFileCloseArchive(hMpq);
    else
        *phMpq = hMpq;
    return nError;
}

static int AddFileToMpq(
    TLogHelper * pLogger,
    HANDLE hMpq,
    const char * szFileName,
    const char * szFileData,
    DWORD dwFlags = 0,
    DWORD dwCompression = 0,
    bool bMustSucceed = false)
{
    HANDLE hFile = NULL;
    DWORD dwFileSize = (DWORD)strlen(szFileData);
    int nError = ERROR_SUCCESS;

    // Notify the user
    pLogger->PrintProgress("Adding file %s ...", szFileName);

    // Get the default flags
    if(dwFlags == 0)
        dwFlags = MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED;
    if(dwCompression == 0)
        dwCompression = MPQ_COMPRESSION_ZLIB;

    // Create the file within the MPQ
    if(!SFileCreateFile(hMpq, szFileName, 0, dwFileSize, 0, dwFlags, &hFile))
    {
        // If success is not expected, it is actually a good thing
        if(bMustSucceed == true)
            return pLogger->PrintError("Failed to create MPQ file %s", szFileName);

        return GetLastError();
    }

    // Write the file
    if(!SFileWriteFile(hFile, szFileData, dwFileSize, dwCompression))
        nError = pLogger->PrintError("Failed to write data to the MPQ");

    SFileCloseFile(hFile);
    return nError;
}

static int AddLocalFileToMpq(
    TLogHelper * pLogger,
    HANDLE hMpq,
    const char * szArchivedName,
    const TCHAR * szFileName,
    DWORD dwFlags = 0,
    DWORD dwCompression = 0,
    bool bMustSucceed = false)
{
    DWORD dwVerifyResult;

    // Notify the user
    pLogger->PrintProgress("Adding file %s (%u of %u)...", szArchivedName, pLogger->UserCount, pLogger->UserTotal);
    pLogger->UserString = szArchivedName;

    // Get the default flags
    if(dwFlags == 0)
        dwFlags = MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED;
    if(dwCompression == 0)
        dwCompression = MPQ_COMPRESSION_ZLIB;

    // Set the notification callback
    SFileSetAddFileCallback(hMpq, AddFileCallback, pLogger);

    // Add the file to the MPQ
    if(!SFileAddFileEx(hMpq, szFileName, szArchivedName, dwFlags, dwCompression, MPQ_COMPRESSION_NEXT_SAME))
    {
        if(bMustSucceed)
            return pLogger->PrintError("Failed to add the file %s", szArchivedName);
        return GetLastError();
    }

    // Verify the file unless it was lossy compression
    if((dwCompression & (MPQ_COMPRESSION_ADPCM_MONO | MPQ_COMPRESSION_ADPCM_STEREO)) == 0)
    {
        // Notify the user
        pLogger->PrintProgress("Verifying file %s (%u of %u) ...", szArchivedName, pLogger->UserCount, pLogger->UserTotal);

        // Perform the verification
        dwVerifyResult = SFileVerifyFile(hMpq, szArchivedName, MPQ_ATTRIBUTE_CRC32 | MPQ_ATTRIBUTE_MD5);
        if(dwVerifyResult & (VERIFY_OPEN_ERROR | VERIFY_READ_ERROR | VERIFY_FILE_SECTOR_CRC_ERROR | VERIFY_FILE_CHECKSUM_ERROR | VERIFY_FILE_MD5_ERROR))
            return pLogger->PrintError("CRC error on %s", szArchivedName);
    }

    return ERROR_SUCCESS;
}

static int RenameMpqFile(TLogHelper * pLogger, HANDLE hMpq, const char * szOldFileName, const char * szNewFileName, bool bMustSucceed)
{
    // Notify the user
    pLogger->PrintProgress("Renaming %s to %s ...", szOldFileName, szNewFileName);

    // Perform the deletion
    if(!SFileRenameFile(hMpq, szOldFileName, szNewFileName))
    {
        if(bMustSucceed == true)
            return pLogger->PrintErrorVa("Failed to rename %s to %s", szOldFileName, szNewFileName);
        return GetLastError();
    }

    return ERROR_SUCCESS;
}

static int RemoveMpqFile(TLogHelper * pLogger, HANDLE hMpq, const char * szFileName, bool bMustSucceed)
{
    // Notify the user
    pLogger->PrintProgress("Removing file %s ...", szFileName);

    // Perform the deletion
    if(!SFileRemoveFile(hMpq, szFileName, 0))
    {
        if(bMustSucceed == true)
            return pLogger->PrintError("Failed to remove the file %s from the archive", szFileName);
        return GetLastError();
    }

    return ERROR_SUCCESS;
}

//-----------------------------------------------------------------------------
// Tests

static void TestGetFileInfo(
    TLogHelper * pLogger,
    HANDLE hMpqOrFile,
    SFileInfoClass InfoClass,
    void * pvFileInfo,
    DWORD cbFileInfo,
    DWORD * pcbLengthNeeded,
    bool bExpectedResult,
    int nExpectedError)
{
    bool bResult;
    int nError = ERROR_SUCCESS;

    // Call the get file info
    bResult = SFileGetFileInfo(hMpqOrFile, InfoClass, pvFileInfo, cbFileInfo, pcbLengthNeeded);
    if(!bResult)
        nError = GetLastError();

    if(bResult != bExpectedResult)
        pLogger->PrintMessage("Different result of SFileGetFileInfo.");
    if(nError != nExpectedError)
        pLogger->PrintMessage("Different error from SFileGetFileInfo (expected %u, returned %u)", nExpectedError, nError);
}

static int TestVerifyFileChecksum(const char * szFullPath)
{
    const char * szShortPlainName = GetShortPlainName(szFullPath);
    unsigned char sha1_digest[SHA1_DIGEST_SIZE];
    TFileStream * pStream;
    TFileData * pFileData;
    hash_state sha1_state;
    ULONGLONG ByteOffset = 0;
    ULONGLONG FileSize = 0;
    char * szExtension;
    LPBYTE pbFileBlock;
    char szShaFileName[MAX_PATH];
    char Sha1Text[0x40];
    DWORD cbBytesToRead;
    DWORD cbFileBlock = 0x10000;
    size_t nLength;
    int nError = ERROR_SUCCESS;

    // Try to load the file with the SHA extension
    strcpy(szShaFileName, szFullPath);
    szExtension = strrchr(szShaFileName, '.');
    if(szExtension == NULL)
        return ERROR_SUCCESS;

    // Skip .SHA and .TXT files
    if(!_stricmp(szExtension, ".sha") || !_stricmp(szExtension, ".txt"))
        return ERROR_SUCCESS;

    // Load the local file to memory
    strcpy(szExtension, ".sha");
    pFileData = LoadLocalFile(NULL, szShaFileName, false);
    if(pFileData != NULL)
    {
        TLogHelper Logger("VerifyFileHash", szShortPlainName);

        // Open the file to be verified
        pStream = FileStream_OpenFile(szFullPath, STREAM_FLAG_READ_ONLY);
        if(pStream != NULL)
        {
            // Notify the user
            Logger.PrintProgress("Verifying file %s", szShortPlainName);

            // Retrieve the size of the file
            FileStream_GetSize(pStream, &FileSize);

            // Allocate the buffer for loading file parts
            pbFileBlock = STORM_ALLOC(BYTE, cbFileBlock);
            if(pbFileBlock != NULL)
            {
                // Initialize SHA1 calculation
                sha1_init(&sha1_state);

                // Calculate the SHA1 of the file
                while(ByteOffset < FileSize)
                {
                    // Notify the user
                    Logger.PrintProgress("Verifying file %s (%I64u of %I64u)", szShortPlainName, ByteOffset, FileSize);

                    // Load the file block
                    cbBytesToRead = ((FileSize - ByteOffset) > cbFileBlock) ? cbFileBlock : (DWORD)(FileSize - ByteOffset);
                    if(!FileStream_Read(pStream, &ByteOffset, pbFileBlock, cbBytesToRead))
                    {
                        nError = GetLastError();
                        break;
                    }

                    // Add to SHA1
                    sha1_process(&sha1_state, pbFileBlock, cbBytesToRead);
                    ByteOffset += cbBytesToRead;
                }

                // Finalize SHA1
                sha1_done(&sha1_state, sha1_digest);
                STORM_FREE(pbFileBlock);

                // Compare with what we loaded from the file
                if(pFileData->dwFileSize >= (SHA1_DIGEST_SIZE * 2))
                {
                    // Compare the Sha1
                    nLength = ConvertSha1ToText(sha1_digest, Sha1Text);
                    if(_strnicmp(Sha1Text, (char *)pFileData->FileData, nLength))
                    {
                        Logger.PrintError("File CRC check failed: %s", szFullPath);
                        nError = ERROR_FILE_CORRUPT;
                    }
                }
            }

            // Close the file
            FileStream_Close(pStream);
        }

        STORM_FREE(pFileData);
    }

    return nError;
}

// StormLib is able to open local files (as well as the original Storm.dll)
// I want to keep this for occasional use
static int TestOpenLocalFile(const char * szPlainName)
{
    TLogHelper Logger("OpenLocalFile", szPlainName);
    HANDLE hFile;
    DWORD dwFileSizeHi = 0;
    DWORD dwFileSizeLo = 0;
    char szFileName1[MAX_PATH];
    char szFileName2[MAX_PATH];
    char szFileLine[0x40];
    
    CreateFullPathName(szFileName1, szMpqSubDir, szPlainName);
    if(SFileOpenFileEx(NULL, szFileName1, SFILE_OPEN_LOCAL_FILE, &hFile))
    {
        // Retrieve the file name. It must match the name under which the file was open
        SFileGetFileName(hFile, szFileName2);
        if(strcmp(szFileName2, szFileName1))
            Logger.PrintMessage("The retrieved name does not match the open name");

        // Retrieve the file size
        dwFileSizeLo = SFileGetFileSize(hFile, &dwFileSizeHi);
        if(dwFileSizeHi != 0 || dwFileSizeLo != 3904784)
            Logger.PrintMessage("Local file size mismatch");

        // Read the first line
        memset(szFileLine, 0, sizeof(szFileLine));
        SFileReadFile(hFile, szFileLine, 18, NULL, NULL);
        if(strcmp(szFileLine, "(1)Enslavers01.scm"))
            Logger.PrintMessage("Content of the listfile does not match");

        SFileCloseFile(hFile);
    }

    return ERROR_SUCCESS;
}

// 
static int TestPartFileRead(const char * szPlainName)
{
    TLogHelper Logger("PartFileRead", szPlainName);
    TMPQHeader Header;
    ULONGLONG ByteOffset;
    ULONGLONG FileSize = 0;
    TFileStream * pStream;
    TCHAR szFileName[MAX_PATH];
    BYTE Buffer[0x100];
    int nError = ERROR_SUCCESS;

    // Open the partial file
    CreateFullPathName(szFileName, szMpqSubDir, szPlainName);
    pStream = FileStream_OpenFile(szFileName, STREAM_PROVIDER_PARTIAL | BASE_PROVIDER_FILE | STREAM_FLAG_READ_ONLY);
    if(pStream == NULL)
        nError = Logger.PrintError(_T("Failed to open %s"), szFileName);

    // Get the size of the stream
    if(nError == ERROR_SUCCESS)
    {
        if(!FileStream_GetSize(pStream, &FileSize))
            nError = Logger.PrintError("Failed to retrieve virtual file size");
    }

    // Read the MPQ header
    if(nError == ERROR_SUCCESS)
    {
        ByteOffset = 0;
        if(!FileStream_Read(pStream, &ByteOffset, &Header, MPQ_HEADER_SIZE_V2))
            nError = Logger.PrintError("Failed to read the MPQ header");
        if(Header.dwID != ID_MPQ || Header.dwHeaderSize != MPQ_HEADER_SIZE_V2)
            nError = Logger.PrintError("MPQ Header error");
    }

    // Read the last 0x100 bytes
    if(nError == ERROR_SUCCESS)
    {
        ByteOffset = FileSize - sizeof(Buffer);
        if(!FileStream_Read(pStream, &ByteOffset, Buffer, sizeof(Buffer)))
            nError = Logger.PrintError("Failed to read from the file");
    }

    // Read 0x100 bytes from position (FileSize - 0xFF)
    // This test must fail
    if(nError == ERROR_SUCCESS)
    {
        ByteOffset = FileSize - sizeof(Buffer) + 1;
        if(FileStream_Read(pStream, &ByteOffset, Buffer, sizeof(Buffer)))
            nError = Logger.PrintError("Test Failed: Reading 0x100 bytes from (FileSize - 0xFF)");
    }

    FileStream_Close(pStream);
    return nError;
}

static int TestOpenFile_OpenById(const char * szPlainName)
{
    TLogHelper Logger("OpenFileById", szPlainName);
    TFileData * pFileData1 = NULL;
    TFileData * pFileData2 = NULL;
    HANDLE hMpq;
    int nError;

    // Copy the archive so we won't fuck up the original one
    nError = OpenExistingArchive(&Logger, szPlainName, NULL, &hMpq);

    // Now try to open a file without knowing the file name
    if(nError == ERROR_SUCCESS)
    {
        // File00000023.xxx = music\dintro.wav
        pFileData1 = LoadMpqFile(&Logger, hMpq, "File00000023.xxx");
        if(pFileData1 == NULL)
            nError = Logger.PrintError("Failed to load the file %s", "File00000023.xxx");
    }

    // Now try to open the file again with its original name
    if(nError == ERROR_SUCCESS)
    {
        // File00000023.xxx = music\dintro.wav
        pFileData2 = LoadMpqFile(&Logger, hMpq, "music\\dintro.wav");
        if(pFileData2 == NULL)
            nError = Logger.PrintError("Failed to load the file %s", "music\\dintro.wav");
    }

    // Now compare both files
    if(nError == ERROR_SUCCESS)
    {
        if(!CompareTwoFiles(&Logger, pFileData1, pFileData1))
            nError = Logger.PrintError("The file has different size/content when open without name");
    }

    // Close the archive
    if(pFileData2 != NULL)
        STORM_FREE(pFileData2);
    if(pFileData1 != NULL)
        STORM_FREE(pFileData1);
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    return nError;
}

// Open an empty archive (found in WoW cache - it's just a header)
static int TestOpenArchive(const char * szPlainName, const char * szListFile = NULL)
{
    TLogHelper Logger("OpenMpqTest", szPlainName);
    TFileData * pFileData;
    HANDLE hMpq;
    DWORD dwFileCount = 0;
    char szListFileBuff[MAX_PATH];
    int nError;

    // Copy the archive so we won't fuck up the original one
    nError = OpenExistingArchive(&Logger, szPlainName, NULL, &hMpq);
    if(nError == ERROR_SUCCESS)
    {
        // If the listfile was given, add it to the MPQ
        if(szListFile != NULL)
        {
            Logger.PrintProgress("Adding listfile %s ...", szListFile);
            CreateFullPathName(szListFileBuff, szMpqSubDir, szListFile);
            nError = SFileAddListFile(hMpq, szListFileBuff);
            if(nError != ERROR_SUCCESS)
                Logger.PrintMessage("Failed to add the listfile to the MPQ");
        }

        // Attempt to open the listfile and attributes
        if(SFileHasFile(hMpq, LISTFILE_NAME))
        {
            pFileData = LoadMpqFile(&Logger, hMpq, LISTFILE_NAME);
            if(pFileData != NULL)
                STORM_FREE(pFileData);
        }

        // Attempt to open the listfile and attributes
        if(SFileHasFile(hMpq, ATTRIBUTES_NAME))
        {
            pFileData = LoadMpqFile(&Logger, hMpq, ATTRIBUTES_NAME);
            if(pFileData != NULL)
                STORM_FREE(pFileData);
        }

        // Search the archive and load every file
        nError = SearchArchive(&Logger, hMpq, TEST_FLAG_LOAD_FILES, &dwFileCount);
        SFileCloseArchive(hMpq);
    }

    return nError;
}

// Opens a patched MPQ archive
static int TestOpenArchive_Patched(const char * PatchList[], const char * szPatchedFile = NULL, int nExpectedPatchCount = 0)
{
    TLogHelper Logger("OpenPatchedMpqTest", PatchList[0]);
    HANDLE hMpq;
    DWORD dwFileCount = 0;
    int nError;

    // Open a patched MPQ archive
    nError = OpenPatchedArchive(&Logger, &hMpq, PatchList);
    if(nError == ERROR_SUCCESS)
    {
        // Check patch count
        if(szPatchedFile != NULL)
            nError = VerifyFilePatchCount(&Logger, hMpq, szPatchedFile, nExpectedPatchCount);

        // Search the archive and load every file
        if(nError == ERROR_SUCCESS)
            nError = SearchArchive(&Logger, hMpq, TEST_FLAG_LOAD_FILES, &dwFileCount);
        
        // Close the archive
        SFileCloseArchive(hMpq);
    }

    return nError;
}

// Open an archive for read-only access
static int TestOpenArchive_ReadOnly(const char * szPlainName, bool bReadOnly)
{
    const char * szCopyName;
    TLogHelper Logger("ReadOnlyTest", szPlainName);
    HANDLE hMpq;
    TCHAR szMpqName[MAX_PATH];
    DWORD dwFlags = 0;
    bool bMustSucceed;
    int nError;

    // Copy the fiel so we wont screw up something
    szCopyName = bReadOnly ? "StormLibTest_ReadOnly.mpq" : "StormLibTest_ReadWrite.mpq";
    nError = CreateMpqCopy(&Logger, szPlainName, szCopyName, szMpqName);

    // Now open the archive for read-only access
    if(nError == ERROR_SUCCESS)
    {
        Logger.PrintProgress("Opening archive %s ...", szCopyName);
        
        dwFlags = bReadOnly ? MPQ_OPEN_READ_ONLY : 0;
        if(!SFileOpenArchive(szMpqName, 0, dwFlags, &hMpq))
            nError = Logger.PrintError("Failed to open the archive %s", szCopyName);
    }

    // Now try to add a file. This must fail if the MPQ is read only
    if(nError == ERROR_SUCCESS)
    {
        bMustSucceed = (bReadOnly == false);
        nError = AddFileToMpq(&Logger, hMpq, "AddedFile.txt", "This is an added file.", MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED, 0, bMustSucceed);
        if(nError != ERROR_SUCCESS && bMustSucceed == false)
            nError = ERROR_SUCCESS;
    }

    // Now try to rename a file in the MPQ. This must only succeed if the MPQ is not read only
    if(nError == ERROR_SUCCESS)
    {
        bMustSucceed = (bReadOnly == false);
        nError = RenameMpqFile(&Logger, hMpq, "spawn.mpq", "spawn-renamed.mpq", bMustSucceed);
        if(nError != ERROR_SUCCESS && bMustSucceed == false)
            nError = ERROR_SUCCESS;
    }

    // Now try to delete a file in the MPQ. This must only succeed if the MPQ is not read only
    if(nError == ERROR_SUCCESS)
    {
        bMustSucceed = (bReadOnly == false);
        nError = RemoveMpqFile(&Logger, hMpq, "spawn-renamed.mpq", bMustSucceed);
        if(nError != ERROR_SUCCESS && bMustSucceed == false)
            nError = ERROR_SUCCESS;
    }

    // Close the archive
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    return nError;
}

static int TestOpenArchive_GetFileInfo(const char * szPlainName1, const char * szPlainName4)
{
    TLogHelper Logger("GetFileInfoTest");
    HANDLE hFile;
    HANDLE hMpq4;
    HANDLE hMpq1;
    DWORD cbLength;
    BYTE DataBuff[0x400];
    int nError1;
    int nError4;

    // Copy the archive so we won't fuck up the original one
    nError1 = OpenExistingArchive(&Logger, szPlainName1, NULL, &hMpq1);
    nError4 = OpenExistingArchive(&Logger, szPlainName4, NULL, &hMpq4);
    if(nError1 == ERROR_SUCCESS && nError4 == ERROR_SUCCESS)
    {
        // Invalid handle - expected (false, ERROR_INVALID_HANDLE)
        TestGetFileInfo(&Logger, NULL, SFileMpqBetHeader, NULL, 0, NULL, false, ERROR_INVALID_HANDLE);

        // Valid handle but invalid value of file info class (false, ERROR_INVALID_PARAMETER)
        TestGetFileInfo(&Logger, NULL, (SFileInfoClass)0xFFF, NULL, 0, NULL, false, ERROR_INVALID_PARAMETER);

        // Valid archive handle but file info class is for file (false, ERROR_INVALID_HANDLE)
        TestGetFileInfo(&Logger, NULL, SFileInfoNameHash1, NULL, 0, NULL, false, ERROR_INVALID_HANDLE);

        // Valid handle and all parameters NULL
        // Returns (true, ERROR_SUCCESS), if BET table is present, otherwise (false, ERROR_CAN_NOT_COMPLETE)
        TestGetFileInfo(&Logger, hMpq1, SFileMpqBetHeader, NULL, 0, NULL, false, ERROR_FILE_NOT_FOUND);
        TestGetFileInfo(&Logger, hMpq4, SFileMpqBetHeader, NULL, 0, NULL, true, ERROR_SUCCESS);

        // Now try to retrieve the required size of the BET table header
        TestGetFileInfo(&Logger, hMpq4, SFileMpqBetHeader, NULL, 0, &cbLength, true, ERROR_SUCCESS);

        // When we call SFileInfo with buffer = NULL and nonzero buffer size, it is ignored
        TestGetFileInfo(&Logger, hMpq4, SFileMpqBetHeader, NULL, 3, &cbLength, true, ERROR_SUCCESS);

        // When we call SFileInfo with buffer != NULL and nonzero buffer size, it should return error
        TestGetFileInfo(&Logger, hMpq4, SFileMpqBetHeader, DataBuff, 3, &cbLength, false, ERROR_INSUFFICIENT_BUFFER);

        // Request for bet table header should also succeed if we want header only
        TestGetFileInfo(&Logger, hMpq4, SFileMpqBetHeader, DataBuff, sizeof(TMPQBetHeader), &cbLength, true, ERROR_SUCCESS);

        // Request for bet table header should also succeed if we want header+flag table only
        TestGetFileInfo(&Logger, hMpq4, SFileMpqBetHeader, DataBuff, sizeof(DataBuff), &cbLength, true, ERROR_SUCCESS);

        // Try to retrieve strong signature from the MPQ
        TestGetFileInfo(&Logger, hMpq1, SFileMpqStrongSignature, NULL, 0, NULL, true, ERROR_SUCCESS);
        TestGetFileInfo(&Logger, hMpq4, SFileMpqStrongSignature, NULL, 0, NULL, false, ERROR_FILE_NOT_FOUND);

        // Strong signature is returned including the signature ID
        TestGetFileInfo(&Logger, hMpq1, SFileMpqStrongSignature, NULL, 0, &cbLength, true, ERROR_SUCCESS);
        assert(cbLength == MPQ_STRONG_SIGNATURE_SIZE + 4);

        // Retrieve the signature
        TestGetFileInfo(&Logger, hMpq1, SFileMpqStrongSignature, DataBuff, sizeof(DataBuff), &cbLength, true, ERROR_SUCCESS);
        assert(memcmp(DataBuff, "NGIS", 4) == 0);

        // Check SFileGetFileInfo on 
        if(SFileOpenFileEx(hMpq4, LISTFILE_NAME, 0, &hFile))
        {
            // Valid parameters but the handle should be file handle 
            TestGetFileInfo(&Logger, hMpq4, SFileInfoFileTime, DataBuff, sizeof(DataBuff), &cbLength, false, ERROR_INVALID_HANDLE);

            // Valid parameters
            TestGetFileInfo(&Logger, hFile, SFileInfoFileTime, DataBuff, sizeof(DataBuff), &cbLength, true, ERROR_SUCCESS);

            SFileCloseFile(hFile);
        }
    }

    if(hMpq4 != NULL)
        SFileCloseArchive(hMpq4);
    if(hMpq1 != NULL)
        SFileCloseArchive(hMpq1);
    return ERROR_SUCCESS;
}

static int TestOpenArchive_VerifySignature(const char * szPlainName, const char * szOriginalName)
{
    TLogHelper Logger("VerifySignatureTest", szPlainName);
    HANDLE hMpq;
    DWORD dwSignatures = 0;
    int nVerifyError;
    int nError = ERROR_SUCCESS;

    // We need original name for the signature check
    nError = OpenExistingArchive(&Logger, szPlainName, szOriginalName, &hMpq);
    if(nError == ERROR_SUCCESS)
    {
        // Query the signature types
        Logger.PrintProgress("Retrieving signatures ...");
        TestGetFileInfo(&Logger, hMpq, SFileMpqSignatures, &dwSignatures, sizeof(DWORD), NULL, true, ERROR_SUCCESS);

        // Verify any of the present signatures
        Logger.PrintProgress("Verifying archive signature ...");
        nVerifyError = SFileVerifyArchive(hMpq);

        // Verify the result
        if((dwSignatures & SIGNATURE_TYPE_STRONG) && (nVerifyError != ERROR_STRONG_SIGNATURE_OK))
        {
            Logger.PrintMessage("Strong signature verification error");
            nError = ERROR_FILE_CORRUPT;
        }

        // Verify the result
        if((dwSignatures & SIGNATURE_TYPE_WEAK) && (nVerifyError != ERROR_WEAK_SIGNATURE_OK))
        {
            Logger.PrintMessage("Weak signature verification error");
            nError = ERROR_FILE_CORRUPT;
        }

        SFileCloseArchive(hMpq);
    }
    return nError;
}

// Open an empty archive (found in WoW cache - it's just a header)
static int TestOpenArchive_CraftedUserData(const char * szPlainName, const char * szCopyName)
{
    TLogHelper Logger("CraftedMpqTest", szPlainName);
    HANDLE hMpq;
    DWORD dwFileCount1 = 0;
    DWORD dwFileCount2 = 0;
    TCHAR szMpqName[MAX_PATH];
    BYTE FileHash1[MD5_DIGEST_SIZE];
    BYTE FileHash2[MD5_DIGEST_SIZE];
    int nError;

    // Create copy of the archive, with interleaving some user data
    nError = CreateMpqCopy(&Logger, szPlainName, szCopyName, szMpqName, 0x400, 0x531);
    
    // Open the archive and load some files
    if(nError == ERROR_SUCCESS)
    {
        Logger.PrintProgress("Opening archive %s ...", szCopyName);
        if(!SFileOpenArchive(szMpqName, 0, 0, &hMpq))
            return Logger.PrintError(_T("Failed to open archive %s"), szMpqName);

        // Verify presence of (listfile) and (attributes)
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, true);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, true);
        
        // Search the archive and load every file
        nError = SearchArchive(&Logger, hMpq, TEST_FLAG_LOAD_FILES | TEST_FLAG_HASH_FILES, &dwFileCount1, FileHash1);
        SFileCloseArchive(hMpq);
    }

    // Try to compact the MPQ
    if(nError == ERROR_SUCCESS)
    {
        // Open the archive again
        Logger.PrintProgress("Reopening archive %s ...", szCopyName);
        if(!SFileOpenArchive(szMpqName, 0, 0, &hMpq))
            return Logger.PrintError(_T("Failed to open archive %s"), szMpqName);
        
        // Compact the archive
        Logger.PrintProgress("Compacting archive %s ...", szMpqName);
        if(!SFileSetCompactCallback(hMpq, CompactCallback, &Logger))
            nError = Logger.PrintError(_T("Failed to compact archive %s"), szMpqName);

        SFileCompactArchive(hMpq, NULL, false);
        SFileCloseArchive(hMpq);
    }

    // Open the archive and load some files
    if(nError == ERROR_SUCCESS)
    {
        Logger.PrintProgress("Reopening archive %s ...", szCopyName);
        if(!SFileOpenArchive(szMpqName, 0, 0, &hMpq))
            return Logger.PrintError(_T("Failed to open archive %s"), szMpqName);

        // Verify presence of (listfile) and (attributes)
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, true);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, true);
        
        // Search the archive and load every file
        nError = SearchArchive(&Logger, hMpq, TEST_FLAG_LOAD_FILES | TEST_FLAG_HASH_FILES, &dwFileCount2, FileHash2);
        SFileCloseArchive(hMpq);
    }

    // Compare the file counts and their hashes
    if(nError == ERROR_SUCCESS)
    {
        if(dwFileCount2 != dwFileCount1)
            Logger.PrintMessage("Different file count after compacting archive: %u vs %u", dwFileCount2, dwFileCount1);
        
        if(memcmp(FileHash2, FileHash1, MD5_DIGEST_SIZE))
            Logger.PrintMessage("Different file hash after compacting archive");
    }

    return nError;
}

// Adding a file to MPQ that had no (listfile) and no (attributes).
// We expect that neither of these will be present after the archive is closed
static int TestAddFile_ListFileTest(const char * szSourceMpq, bool bShouldHaveListFile, bool bShouldHaveAttributes)
{
    TLogHelper Logger("ListFileTest", szSourceMpq);
    TFileData * pFileData = NULL;
    const char * szBackupMpq = bShouldHaveListFile ? "StormLibTest_HasListFile.mpq" : "StormLibTest_NoListFile.mpq";
    const char * szFileName = "AddedFile001.txt";
    const char * szFileData = "0123456789ABCDEF";
    HANDLE hMpq = NULL;
    DWORD dwFileSize = (DWORD)strlen(szFileData);
    int nError = ERROR_SUCCESS;

    // Copy the archive so we won't fuck up the original one
    nError = OpenExistingArchive(&Logger, szSourceMpq, szBackupMpq, &hMpq);

    // Add a file
    if(nError == ERROR_SUCCESS)
    {
        // Now add a file
        nError = AddFileToMpq(&Logger, hMpq, szFileName, szFileData, MPQ_FILE_IMPLODE, MPQ_COMPRESSION_PKWARE);
        SFileCloseArchive(hMpq);
    }

    // Now reopen the archive
    if(nError == ERROR_SUCCESS)
        nError = OpenExistingArchive(&Logger, NULL, szBackupMpq, &hMpq);

    // Now the file has been written and the MPQ has been saved.
    // We Reopen the MPQ and check if there is no (listfile) nor (attributes).
    if(nError == ERROR_SUCCESS)
    {
        // Verify presence of (listfile) and (attributes)
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, bShouldHaveListFile);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, bShouldHaveAttributes);

        // Try to open the file that we recently added
        pFileData = LoadMpqFile(&Logger, hMpq, szFileName);
        if(pFileData != NULL)
        {
            // Verify if the file size matches
            if(pFileData->dwFileSize == dwFileSize)
            {
                // Verify if the file data match
                if(memcmp(pFileData->FileData, szFileData, dwFileSize))
                {
                    Logger.PrintError("The data of the added file does not match");
                    nError = ERROR_FILE_CORRUPT;
                }
            }
            else
            {
                Logger.PrintError("The size of the added file does not match");
                nError = ERROR_FILE_CORRUPT;
            }

            // Delete the file data
            STORM_FREE(pFileData);
        }
        else
        {
            nError = Logger.PrintError("Failed to open the file previously added");
        }
    }

    // Close the MPQ archive
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    return nError;
}

static int TestCreateArchive_EmptyMpq(const char * szPlainName, DWORD dwCreateFlags)
{
    TLogHelper Logger("CreateEmptyMpq", szPlainName);
    HANDLE hMpq = NULL;
    DWORD dwFileCount = 0;
    int nError;

    // Create the full path name
    nError = CreateNewArchive(&Logger, szPlainName, dwCreateFlags, 0, &hMpq);
    if(nError == ERROR_SUCCESS)
    {
        SearchArchive(&Logger, hMpq);
        SFileCloseArchive(hMpq);
    }

    // Reopen the empty MPQ
    if(nError == ERROR_SUCCESS)
    {
        nError = OpenExistingArchive(&Logger, NULL, szPlainName, &hMpq);
        if(nError == ERROR_SUCCESS)
        {
            SFileGetFileInfo(hMpq, SFileMpqNumberOfFiles, &dwFileCount, sizeof(dwFileCount), NULL);

            CheckIfFileIsPresent(&Logger, hMpq, "File00000000.xxx", false);
            CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, false);
            SearchArchive(&Logger, hMpq);
            SFileCloseArchive(hMpq);
        }
    }

    return nError;
}

static int TestCreateArchive_FillArchive(const char * szPlainName)
{
    TLogHelper Logger("CreateFullMpq", szPlainName);
    const char * szFileData = "TestCreateArchive_FillArchive: Testing file data";
    char szFileName[MAX_PATH];
    HANDLE hMpq = NULL;
    DWORD dwMaxFileCount = 6;
    DWORD dwCompression = MPQ_COMPRESSION_ZLIB;
    DWORD dwFlags = MPQ_FILE_ENCRYPTED | MPQ_FILE_COMPRESS;
    int nError;

    // Create the new MPQ
    nError = CreateNewArchive(&Logger, szPlainName, 0, dwMaxFileCount, &hMpq);

    // Now we should be able to add 6 files
    if(nError == ERROR_SUCCESS)
    {
        for(DWORD i = 0; i < dwMaxFileCount; i++)
        {
            sprintf(szFileName, "AddedFile%03u.txt", i);
            nError = AddFileToMpq(&Logger, hMpq, szFileName, szFileData, dwFlags, dwCompression);
            if(nError != ERROR_SUCCESS)
                break;
        }
    }

    // Now the MPQ should be full. It must not be possible to add another file
    if(nError == ERROR_SUCCESS)
    {
        nError = AddFileToMpq(&Logger, hMpq, "ShouldNotBeHere.txt", szFileData, MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB, false);
        assert(nError != ERROR_SUCCESS);
        nError = ERROR_SUCCESS;
    }

    // Close the archive to enforce saving all tables
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    hMpq = NULL;

    // Reopen the archive again
    if(nError == ERROR_SUCCESS)
        nError = OpenExistingArchive(&Logger, NULL, szPlainName, &hMpq);

    // The archive should still be full
    if(nError == ERROR_SUCCESS)
    {
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, true);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, true);
        AddFileToMpq(&Logger, hMpq, "ShouldNotBeHere.txt", szFileData, MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB, false);
    }

    // The (listfile) must be present
    if(nError == ERROR_SUCCESS)
    {
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, true);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, true);
        nError = RemoveMpqFile(&Logger, hMpq, szFileName, true);
    }

    // Now add the file again. This time, it should be possible OK
    if(nError == ERROR_SUCCESS)
    {
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, false);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, false);
        nError = AddFileToMpq(&Logger, hMpq, szFileName, szFileData, dwFlags, dwCompression, true);
    }

    // Now add the file again. This time, it should be fail
    if(nError == ERROR_SUCCESS)
    {
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, false);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, false);
        AddFileToMpq(&Logger, hMpq, szFileName, szFileData, dwFlags, dwCompression, false);
    }

    // Close the archive and return
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    hMpq = NULL;

    // Reopen the archive for the third time to verify that both internal files are there
    if(nError == ERROR_SUCCESS)
    {
        nError = OpenExistingArchive(&Logger, NULL, szPlainName, &hMpq);
        if(nError == ERROR_SUCCESS)
        {
            CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, true);
            CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, true);
            SFileCloseArchive(hMpq);
        }
    }

    return nError;
}

static int TestCreateArchive_IncMaxFileCount(const char * szPlainName)
{
    TLogHelper Logger("IncMaxFileCount", szPlainName);
    const char * szFileData = "TestCreateArchive_IncMaxFileCount: Testing file data";
    char szFileName[MAX_PATH];
    HANDLE hMpq = NULL;
    DWORD dwMaxFileCount = 1;
    int nError;

    // Create the new MPQ
    nError = CreateNewArchive(&Logger, szPlainName, MPQ_CREATE_ARCHIVE_V4, dwMaxFileCount, &hMpq);

    // Now add exactly one file
    if(nError == ERROR_SUCCESS)
    {
        nError = AddFileToMpq(&Logger, hMpq, "AddFile_base.txt", szFileData);
        SFileFlushArchive(hMpq);
        SFileCloseArchive(hMpq);
    }

    // Now add 10 files. Each time we cannot add the file due to archive being full,
    // we increment the max file count
    if(nError == ERROR_SUCCESS)
    {
        for(DWORD i = 0; i < 10; i++)
        {
            // Open the archive again
            nError = OpenExistingArchive(&Logger, NULL, szPlainName, &hMpq);
            if(nError != ERROR_SUCCESS)
                break;

            // Add one file
            sprintf(szFileName, "AddFile_%04u.txt", i);
            nError = AddFileToMpq(&Logger, hMpq, szFileName, szFileData);
            if(nError != ERROR_SUCCESS)
            {
                // Increment the ma file count by one
                dwMaxFileCount = SFileGetMaxFileCount(hMpq) + 1;
                Logger.PrintProgress("Increasing max file count to %u ...", dwMaxFileCount);
                SFileSetMaxFileCount(hMpq, dwMaxFileCount);

                // Attempt to create the file again
                nError = AddFileToMpq(&Logger, hMpq, szFileName, szFileData, 0, 0, true);
            }

            // Compact the archive and close it
            SFileSetCompactCallback(hMpq, CompactCallback, &Logger);
            SFileCompactArchive(hMpq, NULL, false);
            SFileCloseArchive(hMpq);
            if(nError != ERROR_SUCCESS)
                break;
        }
    }

    return nError;
}

static int TestCreateArchive_UnicodeNames()
{
    TLogHelper Logger("MpqUnicodeName");
    int nError = ERROR_SUCCESS;

#ifdef _UNICODE
    nError = CreateNewArchive(&Logger, szUnicodeName1, MPQ_CREATE_ARCHIVE_V1, 15, NULL);
    if(nError != ERROR_SUCCESS)
        return nError;

    nError = CreateNewArchive(&Logger, szUnicodeName2, MPQ_CREATE_ARCHIVE_V2, 58, NULL);
    if(nError != ERROR_SUCCESS)
        return nError;

    nError = CreateNewArchive(&Logger, szUnicodeName3, MPQ_CREATE_ARCHIVE_V3, 15874, NULL);
    if(nError != ERROR_SUCCESS)
        return nError;

    nError = CreateNewArchive(&Logger, szUnicodeName4, MPQ_CREATE_ARCHIVE_V4, 87541, NULL);
    if(nError != ERROR_SUCCESS)
        return nError;

    nError = CreateNewArchive(&Logger, szUnicodeName5, MPQ_CREATE_ARCHIVE_V3, 87541, NULL);
    if(nError != ERROR_SUCCESS)
        return nError;

    nError = CreateNewArchive(&Logger, szUnicodeName5, MPQ_CREATE_ARCHIVE_V2, 87541, NULL);
#endif  // _UNICODE
    return nError;
}

static int TestCreateArchive_FileFlagTest(const char * szPlainName)
{
    TLogHelper Logger("FileFlagTest", szPlainName);
    HANDLE hMpq = NULL;                 // Handle of created archive 
    TCHAR szFileName1[MAX_PATH];
    TCHAR szFileName2[MAX_PATH];
    TCHAR szMpqName[MAX_PATH];
    const char * szMiddleFile = "FileTest_10.exe";
    LCID LocaleIDs[] = {0x000, 0x405, 0x406, 0x407, 0xFFFF};
    char szArchivedName[MAX_PATH];
    DWORD dwMaxFileCount = 0;
    DWORD dwFileCount = 0;
    size_t i;
    int nError;

    // Create paths for local file to be added
    CreateFullPathName(szFileName1, szMpqSubDir, "AddFile.exe");
    CreateFullPathName(szFileName2, szMpqSubDir, "AddFile.bin");

    // Create an empty file that will serve as holder for the MPQ
    nError = CreateEmptyFile(&Logger, szPlainName, 0x100000, szMpqName);

    // Create new MPQ archive over that file
    if(nError == ERROR_SUCCESS)
        nError = CreateNewArchive_FullPath(&Logger, szMpqName, MPQ_CREATE_ARCHIVE_V1, 17, &hMpq);

    // Add the same file multiple times
    if(nError == ERROR_SUCCESS)
    {
        dwMaxFileCount = SFileGetMaxFileCount(hMpq);
        for(i = 0; AddFlags[i] != 0xFFFFFFFF; i++)
        {
            sprintf(szArchivedName, "FileTest_%02u.exe", i);
            nError = AddLocalFileToMpq(&Logger, hMpq, szArchivedName, szFileName1, AddFlags[i], 0);
            if(nError != ERROR_SUCCESS)
                break;

            dwFileCount++;
        }
    }
        
    // Delete a file in the middle of the file table
    if(nError == ERROR_SUCCESS)
    {
        Logger.PrintProgress("Removing file %s ...", szMiddleFile);
        nError = RemoveMpqFile(&Logger, hMpq, szMiddleFile, true);
        dwFileCount--;
    }

    // Add one more file
    if(nError == ERROR_SUCCESS)
    {
        nError = AddLocalFileToMpq(&Logger, hMpq, "FileTest_xx.exe", szFileName1);
        dwFileCount++;
    }
    
    // Try to decrement max file count. This must succeed
    if(nError == ERROR_SUCCESS)
    {
        Logger.PrintProgress("Attempting to decrement max file count ...");
        if(SFileSetMaxFileCount(hMpq, 5))
            nError = Logger.PrintError("Max file count decremented, even if it should fail");
    }

    // Add ZeroSize.txt several times under a different locale
    if(nError == ERROR_SUCCESS)
    {
        for(i = 0; LocaleIDs[i] != 0xFFFF; i++)
        {
            bool bMustSucceed = ((dwFileCount + 2) < dwMaxFileCount);

            SFileSetLocale(LocaleIDs[i]);
            nError = AddLocalFileToMpq(&Logger, hMpq, "ZeroSize_1.txt", szFileName2);
            if(nError != ERROR_SUCCESS)
            {
                if(bMustSucceed == false)
                    nError = ERROR_SUCCESS;
                break;
            }

            dwFileCount++;
        }
    }

    // Add ZeroSize.txt again several times under a different locale
    if(nError == ERROR_SUCCESS)
    {
        for(i = 0; LocaleIDs[i] != 0xFFFF; i++)
        {
            bool bMustSucceed = ((dwFileCount + 2) < dwMaxFileCount);

            SFileSetLocale(LocaleIDs[i]);
            nError = AddLocalFileToMpq(&Logger, hMpq, "ZeroSize_2.txt", szFileName2, 0, 0, bMustSucceed);
            if(nError != ERROR_SUCCESS)
            {
                if(bMustSucceed == false)
                    nError = ERROR_SUCCESS;
                break;
            }
            
            dwFileCount++;
        }
    }

    // Verify how many files did we add to the MPQ
    if(nError == ERROR_SUCCESS)
    {
        if(dwFileCount + 2 != dwMaxFileCount)
        {
            Logger.PrintErrorVa("Number of files added to MPQ was unexpected (expected %u, added %u)", dwFileCount, dwMaxFileCount - 2);
            nError = ERROR_FILE_CORRUPT;
        }
    }

    // Test rename function
    if(nError == ERROR_SUCCESS)
    {
        Logger.PrintProgress("Testing rename files ...");
        SFileSetLocale(LANG_NEUTRAL);
        if(!SFileRenameFile(hMpq, "FileTest_08.exe", "FileTest_08a.exe"))
            nError = Logger.PrintError("Failed to rename the file");
    }

    if(nError == ERROR_SUCCESS)
    {
        if(!SFileRenameFile(hMpq, "FileTest_08a.exe", "FileTest_08.exe"))
            nError = Logger.PrintError("Failed to rename the file");
    }

    if(nError == ERROR_SUCCESS)
    {
        if(SFileRenameFile(hMpq, "FileTest_10.exe", "FileTest_10a.exe"))
        {
            Logger.PrintError("Rename test succeeded even if it shouldn't");
            nError = ERROR_FILE_CORRUPT;
        }
    }

    if(nError == ERROR_SUCCESS)
    {
        if(SFileRenameFile(hMpq, "FileTest_10a.exe", "FileTest_10.exe"))
        {
            Logger.PrintError("Rename test succeeded even if it shouldn't");
            nError = ERROR_FILE_CORRUPT;
        }
    }

    // Close the archive
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    hMpq = NULL;

    // Try to reopen the archive
    nError = OpenExistingArchive(&Logger, NULL, szPlainName, NULL);
    return nError;
}

static int TestCreateArchive_CompressionsTest(const char * szPlainName)
{
    TLogHelper Logger("CompressionsTest", szPlainName);
    HANDLE hMpq = NULL;                 // Handle of created archive 
    TCHAR szFileName[MAX_PATH];        // Source file to be added
    TCHAR szMpqName[MAX_PATH];
    char szArchivedName[MAX_PATH];
    DWORD dwCmprCount = sizeof(Compressions) / sizeof(DWORD);
    DWORD dwAddedFiles = 0;
    DWORD dwFoundFiles = 0;
    size_t i;
    int nError;

    // Create paths for local file to be added
    CreateFullPathName(szFileName, szMpqSubDir, "AddFile.wav");
    CreateFullPathName(szMpqName,   NULL,        szPlainName);

    // Create new archive
    nError = CreateNewArchive_FullPath(&Logger, szMpqName, MPQ_CREATE_ARCHIVE_V4, 0x40, &hMpq); 

    // Add the same file multiple times
    if(nError == ERROR_SUCCESS)
    {
        Logger.UserTotal = dwCmprCount;
        for(i = 0; i < dwCmprCount; i++)
        {
            sprintf(szArchivedName, "WaveFile_%02u.wav", i + 1);
            nError = AddLocalFileToMpq(&Logger, hMpq, szArchivedName, szFileName, MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED | MPQ_FILE_SECTOR_CRC, Compressions[i]);
            if(nError != ERROR_SUCCESS)
                break;

            Logger.UserCount++;
            dwAddedFiles++;
        }

        SFileCloseArchive(hMpq);
    }

    // Reopen the archive extract each WAVE file and try to play it
    if(nError == ERROR_SUCCESS)
    {
        nError = OpenExistingArchive(&Logger, NULL, szPlainName, &hMpq);
        if(nError == ERROR_SUCCESS)
        {
            SearchArchive(&Logger, hMpq, TEST_FLAG_LOAD_FILES | TEST_FLAG_PLAY_WAVES, &dwFoundFiles, NULL);
            SFileCloseArchive(hMpq);
        }

        // Check if the number of found files is the same like the number of added files
        // DOn;t forget that there will be (listfile) and (attributes)
        if(dwFoundFiles != (dwAddedFiles + 2))
        {
            Logger.PrintError("Number of found files does not match number of added files.");
            nError = ERROR_FILE_CORRUPT;
        }
    }

    return nError;
}

static int TestCreateArchive_ListFilePos(const char * szPlainName)
{
    TFileData * pFileData;
    const char * szReaddedFile = "AddedFile_##.txt";
    const char * szFileMask = "AddedFile_%02u.txt";
    TLogHelper Logger("ListFilePos", szPlainName);
    HANDLE hMpq = NULL;                 // Handle of created archive 
    char szArchivedName[MAX_PATH];
    DWORD dwMaxFileCount = 0x1E;
    DWORD dwAddedCount = 0;
    size_t i;
    int nError;

    // Create a new archive with the limit of 0x20 files
    nError = CreateNewArchive(&Logger, szPlainName, MPQ_CREATE_ARCHIVE_V4, dwMaxFileCount, &hMpq);

    // Add 0x1E files
    if(nError == ERROR_SUCCESS)
    {
        for(i = 0; i < dwMaxFileCount; i++)
        {
            sprintf(szArchivedName, szFileMask, i);
            nError = AddFileToMpq(&Logger, hMpq, szArchivedName, "This is a text data.", 0, 0, true);
            if(nError != ERROR_SUCCESS)
                break;

            dwAddedCount++;
        }
    }

    // Delete few middle files
    if(nError == ERROR_SUCCESS)
    {
        for(i = 0; i < (dwMaxFileCount / 2); i++)
        {
            sprintf(szArchivedName, szFileMask, i);
            nError = RemoveMpqFile(&Logger, hMpq, szArchivedName, true);
            if(nError != ERROR_SUCCESS)
                break;
        }
    }

    // Close the archive
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    hMpq = NULL;

    // Reopen the archive to catch any asserts
    if(nError == ERROR_SUCCESS)
        nError = OpenExistingArchive(&Logger, NULL, szPlainName, &hMpq);

    // Check that (listfile) is at the end
    if(nError == ERROR_SUCCESS)
    {
        pFileData = LoadMpqFile(&Logger, hMpq, LISTFILE_NAME);
        if(pFileData != NULL)
        {
            if(pFileData->dwBlockIndex < dwAddedCount)
                Logger.PrintMessage("Unexpected file index of %s", LISTFILE_NAME);
            STORM_FREE(pFileData);
        }

        pFileData = LoadMpqFile(&Logger, hMpq, ATTRIBUTES_NAME);
        if(pFileData != NULL)
        {
            if(pFileData->dwBlockIndex <= dwAddedCount)
                Logger.PrintMessage("Unexpected file index of %s", ATTRIBUTES_NAME);
            STORM_FREE(pFileData);
        }

        // Add new file to the archive. It should be added to position 0
        // (since position 0 should be free)
        nError = AddFileToMpq(&Logger, hMpq, szReaddedFile, "This is a re-added file.", 0, 0, true);
        if(nError == ERROR_SUCCESS)
        {
            pFileData = LoadMpqFile(&Logger, hMpq, szReaddedFile);
            if(pFileData != NULL)
            {
                if(pFileData->dwBlockIndex != 0)
                    Logger.PrintMessage("Unexpected file index of %s", szReaddedFile);
                STORM_FREE(pFileData);
            }
        }

        SFileCloseArchive(hMpq);
    }

    return nError;
}

static int TestCreateArchive_BigArchive(const char * szPlainName)
{
    const char * szFileMask = "AddedFile_%02u.txt";
    TLogHelper Logger("BigMpqTest");
    HANDLE hMpq = NULL;                 // Handle of created archive 
    TCHAR szFileName[MAX_PATH];
    char szArchivedName[MAX_PATH];
    DWORD dwMaxFileCount = 0x20;
    DWORD dwAddedCount = 0;
    size_t i;
    int nError;

    // Create a new archive with the limit of 0x20 files
    nError = CreateNewArchive(&Logger, szPlainName, MPQ_CREATE_ARCHIVE_V3, dwMaxFileCount, &hMpq);
    if(nError == ERROR_SUCCESS)
    {
        // Now add few really big files
        CreateFullPathName(szFileName, szMpqSubDir, "MPQ_1997_v1_Diablo1_DIABDAT.MPQ");
        Logger.UserTotal = (dwMaxFileCount / 2);

        for(i = 0; i < dwMaxFileCount / 2; i++)
        {
            sprintf(szArchivedName, szFileMask, i + 1);
            nError = AddLocalFileToMpq(&Logger, hMpq, szArchivedName, szFileName, 0, 0, true);
            if(nError != ERROR_SUCCESS)
                break;

            Logger.UserCount++;
            dwAddedCount++;
        }
    }

    // Close the archive
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    hMpq = NULL;

    // Reopen the archive to catch any asserts
    if(nError == ERROR_SUCCESS)
        nError = OpenExistingArchive(&Logger, NULL, szPlainName, &hMpq);

    // Check that (listfile) is at the end
    if(nError == ERROR_SUCCESS)
    {
        CheckIfFileIsPresent(&Logger, hMpq, LISTFILE_NAME, true);
        CheckIfFileIsPresent(&Logger, hMpq, ATTRIBUTES_NAME, true);

        SFileCloseArchive(hMpq);
    }

    return nError;
}

static int TestForEachArchive(ARCHIVE_TEST pfnTest, char * szSearchMask, char * szPlainName)
{
    char * szPathBuff = NULL;
    int nError = ERROR_SUCCESS;

    // If the name was not entered, use new one
    if(szSearchMask == NULL)
    {
        szPathBuff = STORM_ALLOC(char, MAX_PATH);
        if(szPathBuff != NULL)
        {
            CreateFullPathName(szPathBuff, szMpqSubDir, "*");
            szSearchMask = szPathBuff;
            szPlainName = strrchr(szSearchMask, '*');
        }
    }

    // At this point, both pointers must be valid
    assert(szSearchMask != NULL && szPlainName != NULL);

    // Now both must be entered
    if(szSearchMask != NULL && szPlainName != NULL)
    {
#ifdef PLATFORM_WINDOWS
        WIN32_FIND_DATAA wf;
        HANDLE hFind;

        // Initiate search. Use ANSI function only
        hFind = FindFirstFileA(szSearchMask, &wf);
        if(hFind != INVALID_HANDLE_VALUE)
        {
            // Skip the first entry, since it's always "." or ".."
            while(FindNextFileA(hFind, &wf) && nError == ERROR_SUCCESS)
            {
                // Found a directory?
                if(wf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if(wf.cFileName[0] != '.')
                    {
                        sprintf(szPlainName, "%s\\*", wf.cFileName);
                        nError = TestForEachArchive(pfnTest, szSearchMask, strrchr(szSearchMask, '*'));
                    }
                }
                else
                {
                    if(pfnTest != NULL)
                    {
                        strcpy(szPlainName, wf.cFileName);
                        nError = pfnTest(szSearchMask);
                    }
                }
            }

            FindClose(hFind);
        }
#endif
    }

    // Free the path buffer, if any
    if(szPathBuff != NULL)
        STORM_FREE(szPathBuff);
    szPathBuff = NULL;
    return nError;
}

//-----------------------------------------------------------------------------
// Main

int main(int argc, char * argv[])
{
    int nError = ERROR_SUCCESS;

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif  // defined(_MSC_VER) && defined(_DEBUG)

    // Initialize storage and mix the random number generator
    printf("==== Test Suite for StormLib version %s ====\n", STORMLIB_VERSION_STRING);
    nError = InitializeMpqDirectory(argv, argc);

    // Search all testing archives and verify their SHA1 hash
    if(nError == ERROR_SUCCESS)
        nError = TestForEachArchive(TestVerifyFileChecksum, NULL, NULL);

    // Test opening local file with SFileOpenFileEx
    if(nError == ERROR_SUCCESS)
        nError = TestOpenLocalFile("ListFile_Blizzard.txt");

    // Test reading partial file
    if(nError == ERROR_SUCCESS)
        nError = TestPartFileRead("MPQ_2009_v2_WoW_patch.MPQ.part");

    // Test working with an archive that has no listfile
    if(nError == ERROR_SUCCESS)
        nError = TestOpenFile_OpenById("MPQ_1997_v1_Diablo1_DIABDAT.MPQ");

    // Open an empty archive (found in WoW cache - it's just a header)
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2012_v2_EmptyMpq.MPQ");

    // Open an empty archive (created artificially - it's just a header)
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2013_v4_EmptyMpq.MPQ");

    // Open an empty archive (found in WoW cache - it's just a header)
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2013_v4_patch-base-16357.MPQ");

    // Open an empty archive (found in WoW cache - it's just a header)
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2011_v4_InvalidHetEntryCount.MPQ");

    // Open an truncated archive
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2002_v1_BlockTableCut.MPQ");

    // Open an Warcraft III map locked by a protector
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2002_v1_ProtectedMap_HashTable_FakeValid.w3x");

    // Open an Warcraft III map locked by a protector
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2002_v1_ProtectedMap_InvalidUserData.w3x");

    // Open an Warcraft III map locked by a protector
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2002_v1_ProtectedMap_InvalidMpqFormat.w3x");

    // Open a MPQ that actually has user data
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2010_v2_HasUserData.s2ma");

    // Open a MPQ archive v 3.0
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2010_v3_expansion-locale-frFR.MPQ");

    // Open an encrypted archive from Starcraft II installer
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPQ_2011_v2_EncryptedMpq.MPQE");

    // Open a MPK archive from Longwu online
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPx_2013_v1_LongwuOnline.mpk");

    // Open a SQP archive from War of the Immortals
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive("MPx_2013_v1_WarOfTheImmortals.sqp", "ListFile_WarOfTheImmortals.txt");

    // Open a patched archive
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_Patched(PatchList_WoW_OldWorld13286, "OldWorld\\World\\Model.blob", 2);

    // Open a patched archive
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_Patched(PatchList_WoW15050, "World\\Model.blob", 8);

    // Open a patched archive. The file is in each patch as full, so there is 0 patches in the chain
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_Patched(PatchList_WoW16965, "DBFilesClient\\BattlePetNPCTeamMember.db2", 0);

    // Check the opening archive for read-only
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_ReadOnly("MPQ_1997_v1_Diablo1_DIABDAT.MPQ", true);

    // Check the opening archive for read-only
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_ReadOnly("MPQ_1997_v1_Diablo1_DIABDAT.MPQ", false);

    // Check the SFileGetFileInfo function
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_GetFileInfo("MPQ_2002_v1_StrongSignature.w3m", "MPQ_2013_v4_SC2_EmptyMap.SC2Map");

    // Check archive signature
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_VerifySignature("MPQ_1999_v1_WeakSignature.exe", "War2Patch_202.exe");

    // Check archive signature
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_VerifySignature("MPQ_2002_v1_StrongSignature.w3m", "(10)DustwallowKeys.w3m");

    // Compact the archive
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_CraftedUserData("MPQ_2010_v3_expansion-locale-frFR.MPQ", "StormLibTest_CraftedMpq1_v3.mpq");

    // Open a MPQ (add custom user data to it
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_CraftedUserData("MPQ_2013_v4_SC2_EmptyMap.SC2Map", "StormLibTest_CraftedMpq2_v4.mpq");

    // Open a MPQ (add custom user data to it)
    if(nError == ERROR_SUCCESS)
        nError = TestOpenArchive_CraftedUserData("MPQ_2013_v4_expansion1.MPQ", "StormLibTest_CraftedMpq3_v4.mpq");

    // Test modifying file with no (listfile) and no (attributes)
    if(nError == ERROR_SUCCESS)
        nError = TestAddFile_ListFileTest("MPQ_1997_v1_Diablo1_DIABDAT.MPQ", false, false);

    // Test modifying an archive that contains (listfile) and (attributes)
    if(nError == ERROR_SUCCESS)
        nError = TestAddFile_ListFileTest("MPQ_2013_v4_SC2_EmptyMap.SC2Map", true, true);

    // Create an empty archive v2
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_EmptyMpq("EmptyMpq_v2.mpq", MPQ_CREATE_ARCHIVE_V2);

    // Create an empty archive v4
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_EmptyMpq("EmptyMpq_v4.mpq", MPQ_CREATE_ARCHIVE_V4);

    // Create an archive and fill it with files up to the max file count
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_FillArchive("FileTableFull.mpq");

    // Create an archive, and increment max file count several times
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_IncMaxFileCount("IncMaxFileCount.mpq");

    // Create a MPQ archive with UNICODE names
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_UnicodeNames();

    // Create a MPQ file, add files with various flags
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_FileFlagTest("FileFlagTest.mpq");

    // Create a MPQ file, add files with various compressions
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_CompressionsTest("CompressionTest.mpq");

    // Check if the listfile is always created at the end of the file table in the archive
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_ListFilePos("ListFilePos.mpq");

    // Open a MPQ (add custom user data to it)
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchive_BigArchive("BigArchive_v4.mpq");

    return nError;
}
