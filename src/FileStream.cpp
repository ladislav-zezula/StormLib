/*****************************************************************************/
/* FileStream.cpp                         Copyright (c) Ladislav Zezula 2010 */
/*---------------------------------------------------------------------------*/
/* File stream support for StormLib                                          */
/*                                                                           */
/* Windows support: Written by Ladislav Zezula                               */
/* Mac support:     Written by Sam Wilkins                                   */
/* Linux support:   Written by Sam Wilkins and Ivan Komissarov               */
/* Big-endian:      Written & debugged by Sam Wilkins                        */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 11.06.10  1.00  Lad  Derived from StormPortMac.cpp and StormPortLinux.cpp */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"
#include "FileStream.h"

#ifdef _MSC_VER
#pragma comment(lib, "wininet.lib")             // Internet functions for HTTP stream
#pragma warning(disable: 4800)                  // 'BOOL' : forcing value to bool 'true' or 'false' (performance warning)
#endif

//-----------------------------------------------------------------------------
// Local defines

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#endif

//-----------------------------------------------------------------------------
// Local functions - platform-specific functions

#ifndef PLATFORM_WINDOWS
static int nLastError = ERROR_SUCCESS;

int GetLastError()
{
    return nLastError;
}

void SetLastError(int nError)
{
    nLastError = nError;
}
#endif

//-----------------------------------------------------------------------------
// Preparing file bitmap for a complete file of a given size

static bool FileBitmap_CheckRange(
    TFileBitmap * pBitmap,
    ULONGLONG ByteOffset,
    ULONGLONG EndOffset)
{
    LPBYTE pbBitmap = (LPBYTE)(pBitmap + 1);
    DWORD BlockIndex = (DWORD)(ByteOffset / pBitmap->BlockSize);
    DWORD ByteIndex = (BlockIndex / 0x08);
    BYTE BitMask = (BYTE)(0x01 << (BlockIndex & 0x07));

    // Mask the bye offset down to the begin of the block
    ByteOffset = ByteOffset & ~((ULONGLONG)pBitmap->BlockSize - 1);

    // Check each block
    while(ByteOffset < EndOffset)
    {
        // Check availability of that block
        if((pbBitmap[ByteIndex] & BitMask) == 0)
            return false;

        // Move to the next block
        ByteOffset += pBitmap->BlockSize;
        ByteIndex += (BitMask >> 0x07);
        BitMask = (BitMask >> 0x07) | (BitMask << 0x01);
    }

    // All blocks are present
    return true;
}

static void FileBitmap_SetRange(
    TFileBitmap * pBitmap,
    ULONGLONG ByteOffset,
    ULONGLONG EndOffset)
{
    LPBYTE pbBitmap = (LPBYTE)(pBitmap + 1);
    DWORD BlockIndex = (DWORD)(ByteOffset / pBitmap->BlockSize);
    DWORD ByteIndex = (BlockIndex / 0x08);
    BYTE BitMask = (BYTE)(0x01 << (BlockIndex & 0x07));

    // Mask the bye offset down to the begin of the block
    ByteOffset = ByteOffset & ~((ULONGLONG)pBitmap->BlockSize - 1);

    // Check each block
    while(ByteOffset < EndOffset)
    {
        // Set that bit
        pbBitmap[ByteIndex] |= BitMask;

        // Move to the next block
        ByteOffset += pBitmap->BlockSize;
        ByteIndex += (BitMask >> 0x07);
        BitMask = (BitMask >> 0x07) | (BitMask << 0x01);
    }
}

static DWORD FileBitmap_CheckFile(TFileBitmap * pBitmap)
{
    LPBYTE pbBitmap = (LPBYTE)(pBitmap + 1);
    DWORD WholeByteCount = (pBitmap->BlockCount / 8);
    DWORD ExtraBitsCount = (pBitmap->BlockCount & 7);
    BYTE ExpectedValue;

    // Verify the whole bytes - their value must be 0xFF
    for(DWORD i = 0; i < WholeByteCount; i++)
    {
        if(pbBitmap[i] != 0xFF)
            return 0;
    }

    // If there are extra bits, calculate the mask
    if(ExtraBitsCount != 0)
    {
        ExpectedValue = (BYTE)((1 << ExtraBitsCount) - 1);
        if(pbBitmap[WholeByteCount] != ExpectedValue)
            return 0;
    }

    // Yes, the file is complete
    return 1;
}

//-----------------------------------------------------------------------------
// Local functions - base file support

static bool BaseFile_Read(
    TFileStream * pStream,                  // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->FilePos;
    DWORD dwBytesRead = 0;                  // Must be set by platform-specific code

#ifdef PLATFORM_WINDOWS
    {
        // Note: StormLib no longer supports Windows 9x.
        // Thus, we can use the OVERLAPPED structure to specify
        // file offset to read from file. This allows us to skip
        // one system call to SetFilePointer

        // Update the byte offset
        pStream->FilePos = ByteOffset;

        // Read the data
        if(dwBytesToRead != 0)
        {
            OVERLAPPED Overlapped;

            Overlapped.OffsetHigh = (DWORD)(ByteOffset >> 32);
            Overlapped.Offset = (DWORD)ByteOffset;
            Overlapped.hEvent = NULL;
            if(!ReadFile(pStream->Base.File.hFile, pvBuffer, dwBytesToRead, &dwBytesRead, &Overlapped))
                return false;
        }
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        ssize_t bytes_read;

        // If the byte offset is different from the current file position,
        // we have to update the file position
        if(ByteOffset != pStream->FilePos)
        {
            lseek64((intptr_t)pStream->Base.File.hFile, (off64_t)(ByteOffset), SEEK_SET);
            pStream->FilePos = ByteOffset;
        }

        // Perform the read operation
        if(dwBytesToRead != 0)
        {
            bytes_read = read((intptr_t)pStream->Base.File.hFile, pvBuffer, (size_t)dwBytesToRead);
            if(bytes_read == -1)
            {
                nLastError = errno;
                return false;
            }
            
            dwBytesRead = (DWORD)(size_t)bytes_read;
        }
    }
#endif

    // Increment the current file position by number of bytes read
    // If the number of bytes read doesn't match to required amount, return false
    pStream->FilePos = ByteOffset + dwBytesRead;
    if(dwBytesRead != dwBytesToRead)
        SetLastError(ERROR_HANDLE_EOF);
    return (dwBytesRead == dwBytesToRead);
}

/**
 * \a pStream Pointer to an open stream
 * \a pByteOffset Pointer to file byte offset. If NULL, writes to current position
 * \a pvBuffer Pointer to data to be written
 * \a dwBytesToWrite Number of bytes to write to the file
 */

static bool BaseFile_Write(TFileStream * pStream, ULONGLONG * pByteOffset, const void * pvBuffer, DWORD dwBytesToWrite)
{
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->FilePos;
    DWORD dwBytesWritten = 0;               // Must be set by platform-specific code

#ifdef PLATFORM_WINDOWS
    {
        // Note: StormLib no longer supports Windows 9x.
        // Thus, we can use the OVERLAPPED structure to specify
        // file offset to read from file. This allows us to skip
        // one system call to SetFilePointer

        // Update the byte offset
        pStream->FilePos = ByteOffset;

        // Read the data
        if(dwBytesToWrite != 0)
        {
            OVERLAPPED Overlapped;

            Overlapped.OffsetHigh = (DWORD)(ByteOffset >> 32);
            Overlapped.Offset = (DWORD)ByteOffset;
            Overlapped.hEvent = NULL;
            if(!WriteFile(pStream->Base.File.hFile, pvBuffer, dwBytesToWrite, &dwBytesWritten, &Overlapped))
                return false;
        }
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        ssize_t bytes_written;

        // If the byte offset is different from the current file position,
        // we have to update the file position
        if(ByteOffset != pStream->Base.File.FilePos)
        {
            lseek64((intptr_t)pStream->Base.File.hFile, (off64_t)(ByteOffset), SEEK_SET);
            pStream->Base.File.FilePos = ByteOffset;
        }

        // Perform the read operation
        bytes_written = write((intptr_t)pStream->Base.File.hFile, pvBuffer, (size_t)dwBytesToWrite);
        if(bytes_written == -1)
        {
            nLastError = errno;
            return false;
        }
        
        dwBytesWritten = (DWORD)(size_t)bytes_written;
    }
#endif

    // Increment the current file position by number of bytes read
    pStream->FilePos = ByteOffset + dwBytesWritten;

    // Also modify the file size, if needed
    if(pStream->FilePos > pStream->FileSize)
        pStream->FileSize = pStream->FilePos;

    if(dwBytesWritten != dwBytesToWrite)
        SetLastError(ERROR_DISK_FULL);
    return (dwBytesWritten == dwBytesToWrite);
}

/**
 * \a pStream Pointer to an open stream
 * \a NewFileSize New size of the file
 */
static bool BaseFile_SetSize(TFileStream * pStream, ULONGLONG NewFileSize)
{
#ifdef PLATFORM_WINDOWS
    {
        LONG FileSizeHi = (LONG)(NewFileSize >> 32);
        LONG FileSizeLo;
        DWORD dwNewPos;
        bool bResult;

        // Set the position at the new file size
        dwNewPos = SetFilePointer(pStream->Base.File.hFile, (LONG)NewFileSize, &FileSizeHi, FILE_BEGIN);
        if(dwNewPos == INVALID_SET_FILE_POINTER && GetLastError() != ERROR_SUCCESS)
            return false;

        // Set the current file pointer as the end of the file
        bResult = (bool)SetEndOfFile(pStream->Base.File.hFile);

        // Restore the file position
        FileSizeHi = (LONG)(pStream->FilePos >> 32);
        FileSizeLo = (LONG)(pStream->FilePos);
        SetFilePointer(pStream->Base.File.hFile, FileSizeLo, &FileSizeHi, FILE_BEGIN);
        return bResult;
    }
#endif
    
#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        if(ftruncate64((intptr_t)pStream->Base.File.hFile, (off64_t)NewFileSize) == -1)
        {
            nLastError = errno;
            return false;
        }
        
        return true;
    }
#endif
}

// Renames the file pointed by pStream so that it contains data from pNewStream
static bool BaseFile_Switch(TFileStream * pStream, TFileStream * pNewStream)
{
#ifdef PLATFORM_WINDOWS
    // Delete the original stream file. Don't check the result value,
    // because if the file doesn't exist, it would fail
    DeleteFile(pStream->szFileName);

    // Rename the new file to the old stream's file
    return (bool)MoveFile(pNewStream->szFileName, pStream->szFileName);
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    // "rename" on Linux also works if the target file exists
    if(rename(pNewStream->szFileName, pStream->szFileName) == -1)
    {
        nLastError = errno;
        return false;
    }
    
    return true;
#endif
}

static void BaseFile_Close(TFileStream * pStream)
{
    if(pStream->Base.File.hFile != INVALID_HANDLE_VALUE)
    {
#ifdef PLATFORM_WINDOWS
        CloseHandle(pStream->Base.File.hFile);
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
        close((intptr_t)pStream->Base.File.hFile);
#endif
    }

    // Also invalidate the handle
    pStream->Base.File.hFile = INVALID_HANDLE_VALUE;
}

static bool BaseFile_Create(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS
    {
        DWORD dwWriteShare = (pStream->dwFlags & STREAM_FLAG_WRITE_SHARE) ? FILE_SHARE_WRITE : 0;

        pStream->Base.File.hFile = CreateFile(pStream->szFileName,
                                              GENERIC_READ | GENERIC_WRITE,
                                              dwWriteShare | FILE_SHARE_READ,
                                              NULL,
                                              CREATE_ALWAYS,
                                              0,
                                              NULL);
        if(pStream->Base.File.hFile == INVALID_HANDLE_VALUE)
            return false;
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        intptr_t handle;
        
        handle = open(pStream->szFileName, O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(handle == -1)
        {
            nLastError = errno;
            return false;
        }
        
        pStream->Base.File.hFile = (HANDLE)handle;
    }
#endif

    // Fill-in the entry points
    pStream->BaseRead    = BaseFile_Read;
    pStream->BaseWrite   = BaseFile_Write;
    pStream->BaseSetSize = BaseFile_SetSize;
    pStream->BaseClose   = BaseFile_Close;

    // Reset the file position
    pStream->FileSize = 0;
    pStream->FilePos = 0;
    return true;
}

static bool BaseFile_Open(TFileStream * pStream, DWORD dwStreamFlags)
{
#ifdef PLATFORM_WINDOWS
    {
        ULARGE_INTEGER FileSize;
        DWORD dwWriteAccess = (dwStreamFlags & STREAM_FLAG_READ_ONLY) ? 0 : FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES;
        DWORD dwWriteShare = (dwStreamFlags & STREAM_FLAG_WRITE_SHARE) ? FILE_SHARE_WRITE : 0;

        // Open the file
        pStream->Base.File.hFile = CreateFile(pStream->szFileName,
                                              FILE_READ_DATA | FILE_READ_ATTRIBUTES | dwWriteAccess,
                                              FILE_SHARE_READ | dwWriteShare,
                                              NULL,
                                              OPEN_EXISTING,
                                              0,
                                              NULL);
        if(pStream->Base.File.hFile == INVALID_HANDLE_VALUE)
            return false;

        // Query the file size
        FileSize.LowPart = GetFileSize(pStream->Base.File.hFile, &FileSize.HighPart);
        pStream->FileSize = FileSize.QuadPart;

        // Query last write time
        GetFileTime(pStream->Base.File.hFile, NULL, NULL, (LPFILETIME)&pStream->FileTime);
    }
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    {
        struct stat64 fileinfo;
        int oflag = (dwStreamFlags & STREAM_FLAG_READ_ONLY) ? O_RDONLY : O_RDWR;
        intptr_t handle;

        // Open the file
        handle = open(szFileName, oflag | O_LARGEFILE);
        if(handle == -1)
        {
            nLastError = errno;
            return false;
        }

        // Get the file size
        if(fstat64(handle, &fileinfo) == -1)
        {
            nLastError = errno;
            return false;
        }

        // time_t is number of seconds since 1.1.1970, UTC.
        // 1 second = 10000000 (decimal) in FILETIME
        // Set the start to 1.1.1970 00:00:00
        pStream->Base.File.FileTime = 0x019DB1DED53E8000ULL + (10000000 * fileinfo.st_mtime);
        pStream->Base.File.FileSize = (ULONGLONG)fileinfo.st_size;
        pStream->Base.File.hFile = (HANDLE)handle;
    }
#endif

    // Fill-in the entry points
    pStream->BaseRead    = BaseFile_Read;
    pStream->BaseWrite   = BaseFile_Write;
    pStream->BaseSetSize = BaseFile_SetSize;
    pStream->BaseClose   = BaseFile_Close;

    // Reset the file position
    pStream->FilePos = 0;
    return true;
}

//-----------------------------------------------------------------------------
// Local functions - base memory-mapped file support

static bool BaseMap_Read(
    TFileStream * pStream,                  // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->FilePos;

    // Do we have to read anything at all?
    if(dwBytesToRead != 0)
    {
        // Don't allow reading past file size
        if((ByteOffset + dwBytesToRead) > pStream->FileSize)
            return false;

        // Copy the required data
        memcpy(pvBuffer, pStream->Base.Map.pbFile + (size_t)ByteOffset, dwBytesToRead);
    }

    // Move the current file position
    pStream->FilePos += dwBytesToRead;
    return true;
}

static void BaseMap_Close(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS
    if(pStream->Base.Map.pbFile != NULL)
        UnmapViewOfFile(pStream->Base.Map.pbFile);
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    if(pStream->Base.Map.pbFile != NULL)
        munmap(pStream->Base.Map.pbFile, (size_t )pStream->FileSize);
#endif

    pStream->Base.Map.pbFile = NULL;
}

static bool BaseMap_Open(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS

    ULARGE_INTEGER FileSize;
    HANDLE hFile;
    HANDLE hMap;
    bool bResult = false;

    // Open the file for read access
    hFile = CreateFile(pStream->szFileName, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(hFile != NULL)
    {
        // Retrieve file size. Don't allow mapping file of a zero size.
        FileSize.LowPart = GetFileSize(hFile, &FileSize.HighPart);
        if(FileSize.QuadPart != 0)
        {
            // Retrieve file time
            GetFileTime(hFile, NULL, NULL, (LPFILETIME)&pStream->FileTime);

            // Now create mapping object
            hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
            if(hMap != NULL)
            {
                // Map the entire view into memory
                // Note that this operation will fail if the file can't fit
                // into usermode address space
                pStream->Base.Map.pbFile = (LPBYTE)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
                if(pStream->Base.Map.pbFile != NULL)
                {
                    pStream->FileSize = FileSize.QuadPart;
                    pStream->FilePos = 0;
                    bResult = true;
                }

                // Close the map handle
                CloseHandle(hMap);
            }
        }

        // Close the file handle
        CloseHandle(hFile);
    }

    // If the file is not there and is not available for random access,
    // report error
    if(bResult == false)
        return false;
#endif

#if defined(PLATFORM_MAC) || defined(PLATFORM_LINUX)
    struct stat64 fileinfo;
    intptr_t handle;
    bool bResult = false;

    // Open the file
    handle = open(szFileName, O_RDONLY);
    if(handle != -1)
    {
        // Get the file size
        if(fstat64(handle, &fileinfo) != -1)
        {
            pStream->Base.Map.pbFile = (LPBYTE)mmap(NULL, (size_t)fileinfo.st_size, PROT_READ, MAP_PRIVATE, handle, 0);
            if(pStream->Base.Map.pbFile != NULL)
            {
                // time_t is number of seconds since 1.1.1970, UTC.
                // 1 second = 10000000 (decimal) in FILETIME
                // Set the start to 1.1.1970 00:00:00
                pStream->FileTime = 0x019DB1DED53E8000ULL + (10000000 * fileinfo.st_mtime);
                pStream->FileSize = (ULONGLONG)fileinfo.st_size;
                pStream->FilePos = 0;
                bResult = true;
            }
        }
        close(handle);
    }

    // Did the mapping fail?
    if(bResult == false)
    {
        nLastError = errno;
        return false;
    }
#endif

    // Fill-in entry points
    pStream->BaseRead    = BaseMap_Read;
    pStream->BaseClose   = BaseMap_Close;
    return true;
}

//-----------------------------------------------------------------------------
// Local functions - base HTTP file support

static const TCHAR * BaseHttp_ExtractServerName(const TCHAR * szFileName, TCHAR * szServerName)
{
    // Check for HTTP
    if(!_tcsnicmp(szFileName, _T("http://"), 7))
        szFileName += 7;

    // Cut off the server name
    if(szServerName != NULL)
    {
        while(szFileName[0] != 0 && szFileName[0] != _T('/'))
            *szServerName++ = *szFileName++;
        *szServerName = 0;
    }
    else
    {
        while(szFileName[0] != 0 && szFileName[0] != _T('/'))
            szFileName++;
    }

    // Return the remainder
    return szFileName;
}

static bool BaseHttp_Read(
    TFileStream * pStream,                  // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
#ifdef PLATFORM_WINDOWS
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->FilePos;
    DWORD dwTotalBytesRead = 0;

    // Do we have to read anything at all?
    if(dwBytesToRead != 0)
    {
        HINTERNET hRequest;
        LPCTSTR szFileName;
        LPBYTE pbBuffer = (LPBYTE)pvBuffer;
        TCHAR szRangeRequest[0x80];
        DWORD dwStartOffset = (DWORD)ByteOffset;
        DWORD dwEndOffset = dwStartOffset + dwBytesToRead;

        // Open HTTP request to the file
        szFileName = BaseHttp_ExtractServerName(pStream->szFileName, NULL);
        hRequest = HttpOpenRequest(pStream->Base.Http.hConnect, _T("GET"), szFileName, NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if(hRequest != NULL)
        {
            // Add range request to the HTTP headers
            // http://www.clevercomponents.com/articles/article015/resuming.asp
            _stprintf(szRangeRequest, _T("Range: bytes=%d-%d"), dwStartOffset, dwEndOffset);
            HttpAddRequestHeaders(hRequest, szRangeRequest, 0xFFFFFFFF, HTTP_ADDREQ_FLAG_ADD_IF_NEW); 

            // Send the request to the server
            if(HttpSendRequest(hRequest, NULL, 0, NULL, 0))
            {
                while(dwTotalBytesRead < dwBytesToRead)
                {
                    DWORD dwBlockBytesToRead = dwBytesToRead - dwTotalBytesRead;
                    DWORD dwBlockBytesRead = 0;

                    // Read the block from the file
                    if(dwBlockBytesToRead > 0x200)
                        dwBlockBytesToRead = 0x200;
                    InternetReadFile(hRequest, pbBuffer, dwBlockBytesToRead, &dwBlockBytesRead);

                    // Check for end
                    if(dwBlockBytesRead == 0)
                        break;

                    // Move buffers
                    dwTotalBytesRead += dwBlockBytesRead;
                    pbBuffer += dwBlockBytesRead;
                }
            }
            InternetCloseHandle(hRequest);
        }
    }

    // Increment the current file position by number of bytes read
    pStream->FilePos = ByteOffset + dwTotalBytesRead;

    // If the number of bytes read doesn't match the required amount, return false
    if(dwTotalBytesRead != dwBytesToRead)
        SetLastError(ERROR_HANDLE_EOF);
    return (dwTotalBytesRead == dwBytesToRead);

#else

    // Not supported
    pStream = pStream;
    pByteOffset = pByteOffset;
    pvBuffer = pvBuffer;
    dwBytesToRead = dwBytesToRead;
    SetLastError(ERROR_NOT_SUPPORTED);
    return false;

#endif
}

static void BaseHttp_Close(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS
    if(pStream->Base.Http.hConnect != NULL)
        InternetCloseHandle(pStream->Base.Http.hConnect);
    pStream->Base.Http.hConnect = NULL;

    if(pStream->Base.Http.hInternet != NULL)
        InternetCloseHandle(pStream->Base.Http.hInternet);
    pStream->Base.Http.hInternet = NULL;
#else
    pStream = pStream;
#endif
}

static bool BaseHttp_Open(TFileStream * pStream)
{
#ifdef PLATFORM_WINDOWS

    const TCHAR * szFileName;
    HINTERNET hRequest;
    DWORD dwTemp = 0;
    bool bFileAvailable = false;
    int nError = ERROR_SUCCESS;

    // Don't connect to the internet
    if(!InternetGetConnectedState(&dwTemp, 0))
        nError = GetLastError();

    // Initiate the connection to the internet
    if(nError == ERROR_SUCCESS)
    {
        pStream->Base.Http.hInternet = InternetOpen(_T("StormLib HTTP MPQ reader"),
                                                    INTERNET_OPEN_TYPE_PRECONFIG,
                                                    NULL,
                                                    NULL,
                                                    0);
        if(pStream->Base.Http.hInternet == NULL)
            nError = GetLastError();
    }

    // Connect to the server
    if(nError == ERROR_SUCCESS)
    {
        TCHAR szServerName[MAX_PATH];
        DWORD dwFlags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_CACHE_WRITE;

        // Initiate connection with the server
        szFileName = BaseHttp_ExtractServerName(pStream->szFileName, szServerName);
        pStream->Base.Http.hConnect = InternetConnect(pStream->Base.Http.hInternet,
                                                      szServerName,
                                                      INTERNET_DEFAULT_HTTP_PORT,
                                                      NULL,
                                                      NULL,
                                                      INTERNET_SERVICE_HTTP,
                                                      dwFlags,
                                                      0);
        if(pStream->Base.Http.hConnect == NULL)
            nError = GetLastError();
    }

    // Now try to query the file size
    if(nError == ERROR_SUCCESS)
    {
        // Open HTTP request to the file
        hRequest = HttpOpenRequest(pStream->Base.Http.hConnect, _T("GET"), szFileName, NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if(hRequest != NULL)
        {
            if(HttpSendRequest(hRequest, NULL, 0, NULL, 0))
            {
                ULONGLONG FileTime = 0;
                DWORD dwFileSize = 0;
                DWORD dwDataSize;
                DWORD dwIndex = 0;

                // Check if the MPQ has Last Modified field
                dwDataSize = sizeof(ULONGLONG);
                if(HttpQueryInfo(hRequest, HTTP_QUERY_LAST_MODIFIED | HTTP_QUERY_FLAG_SYSTEMTIME, &FileTime, &dwDataSize, &dwIndex))
                    pStream->FileTime = FileTime;

                // Verify if the server supports random access
                dwDataSize = sizeof(DWORD);
                if(HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &dwFileSize, &dwDataSize, &dwIndex))
                {
                    if(dwFileSize != 0)
                    {
                        pStream->FileSize = dwFileSize;
                        pStream->FilePos = 0;
                        bFileAvailable = true;
                    }
                }
            }
            InternetCloseHandle(hRequest);
        }
    }

    // If the file is not there and is not available for random access,
    // report error
    if(bFileAvailable == false)
    {
        BaseHttp_Close(pStream);
        return false;
    }

    // Fill-in entry points
    pStream->BaseRead    = BaseHttp_Read;
    pStream->BaseClose   = BaseHttp_Close;
    return true;

#else

    // Not supported
    SetLastError(ERROR_NOT_SUPPORTED);
    pStream = pStream;
    return false;

#endif
}

//-----------------------------------------------------------------------------
// Local functions - bitmap for a complete file

#define DEFAULT_BLOCK_SIZE 0x4000

static bool CompleteFile_GetBmp(
    TFileStream * pStream,
    void * pvBitmap,
    DWORD Length,
    LPDWORD LengthNeeded)
{
    TFileBitmap * pBitmap = (TFileBitmap *)pvBitmap;
    ULONGLONG FileSize = pStream->FileSize;
    DWORD TotalLength;
    DWORD BlockCount = (DWORD)(((FileSize - 1) / DEFAULT_BLOCK_SIZE) + 1);
    DWORD BitmapSize = (DWORD)(((BlockCount - 1) / 8) + 1);
    DWORD LastByte;

    // Calculate and give the total length
    TotalLength = sizeof(TFileBitmap) + BitmapSize;
    if(LengthNeeded != NULL)
        *LengthNeeded = TotalLength;

    // Has the caller given enough space for storing the structure?
    if(Length >= sizeof(TFileBitmap))
    {
        pBitmap->StartOffset = 0;
        pBitmap->EndOffset   = FileSize;
        pBitmap->BitmapSize  = BitmapSize;
        pBitmap->BlockSize   = DEFAULT_BLOCK_SIZE;
        pBitmap->BlockCount  = BlockCount;
        pBitmap->IsComplete  = 1;

        // Do we have enough space to fill the bitmap as well?
        if(Length >= TotalLength)
        {
            LPBYTE pbBitmap = (LPBYTE)(pBitmap + 1);

            // Fill the full blocks
            memset(pbBitmap, 0xFF, (BlockCount / 8));
            pbBitmap += (BlockCount / 8);

            // Supply the last block
            if(BlockCount & 7)
            {
                LastByte = (1 << (BlockCount & 7)) - 1;
                pbBitmap[0] = (BYTE)LastByte;
            }
        }

        return true;
    }
    else
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return false;
    }
}

//-----------------------------------------------------------------------------
// Local functions - linear stream support

typedef struct _DATA_BLOCK_INFO
{
    ULONGLONG BlockOffset0;                 // Offset of the first block in the continuous array
    ULONGLONG BlockOffset;                  // Offset of the current block
    ULONGLONG ByteOffset;                   // Offset of the loaded data
    ULONGLONG EndOffset;                    // Offset of the end of the block
    LPBYTE ReadBuffer;                      // Pointer to the buffer where to read the data
    DWORD BlockSize;                        // Length of one block, in bytes
    DWORD ByteIndex0;                       // Byte index of the first block in the continuous array
    DWORD ByteIndex;                        // Index of the byte in the file bitmap
    BYTE MirrorUpdated;                     // If set to nonzero, the mirror stream has been updated
    BYTE BitMask0;                          // Bit mask of the first block in the continuous array
    BYTE BitMask;                           // Bit mask of the current bit in file bitmap

} DATA_BLOCK_INFO, *PDATA_BLOCK_INFO;

static bool LinearStream_LoadBitmap(
    TLinearStream * pStream)                // Pointer to an open stream
{
    FILE_BITMAP_FOOTER Footer;
    TFileBitmap * pBitmap;
    ULONGLONG ByteOffset; 
    DWORD BlockCount;
    DWORD BitmapSize;

    // Only if the size is greater than sizeof bitmap footer
    if(pStream->FileSize > sizeof(FILE_BITMAP_FOOTER))
    {
        // Load the bitmap footer
        ByteOffset = pStream->FileSize - sizeof(FILE_BITMAP_FOOTER);
        if(pStream->StreamRead(pStream, &ByteOffset, &Footer, sizeof(FILE_BITMAP_FOOTER)))
        {
            // Make sure that the array is properly BSWAP-ed
            BSWAP_ARRAY32_UNSIGNED((LPDWORD)(&Footer), sizeof(FILE_BITMAP_FOOTER));

            // Verify if there is actually a footer
            if(Footer.dwSignature == ID_FILE_BITMAP_FOOTER && Footer.dwAlways3 == 0x03)
            {
                // Get offset of the bitmap, size of the bitmap and check for match
                ByteOffset = MAKE_OFFSET64(Footer.dwMapOffsetHi, Footer.dwMapOffsetLo);
                BlockCount = (DWORD)(((ByteOffset - 1) / Footer.dwBlockSize) + 1);
                BitmapSize = ((BlockCount + 7) / 8);

                // Check if the sizes match
                if(ByteOffset + BitmapSize + sizeof(FILE_BITMAP_FOOTER) == pStream->FileSize)
                {
                    // Allocate space for the linear bitmap
                    pBitmap = (TFileBitmap *)STORM_ALLOC(BYTE, sizeof(TFileBitmap) + BitmapSize);
                    if(pBitmap != NULL)
                    {
                        // Fill the bitmap header
                        pBitmap->StartOffset = 0;
                        pBitmap->EndOffset  = ByteOffset;
                        pBitmap->BitmapSize = BitmapSize;
                        pBitmap->BlockSize  = Footer.dwBlockSize;
                        pBitmap->BlockCount = BlockCount;

                        // Load the bitmap bits
                        if(!pStream->BaseRead(pStream, &ByteOffset, (pBitmap + 1), BitmapSize))
                        {
                            STORM_FREE(pBitmap);
                            return false;
                        }

                        // Verify if the file is complete or not
                        pBitmap->IsComplete = FileBitmap_CheckFile(pBitmap);
                        
                        // Set the file bitmap into the file stream
                        pStream->FileSize = ByteOffset;
                        pStream->pBitmap = pBitmap;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static bool LinearStream_LoadMissingBlocks(
    TLinearStream * pStream,
    ULONGLONG ByteOffset,
    ULONGLONG EndOffset,
    void * pvBuffer)
{
    TFileBitmap * pBitmap = pStream->pBitmap;
    ULONGLONG BlockSizeMask = pStream->pBitmap->BlockSize;
    ULONGLONG BlockOffset = ByteOffset & ~(BlockSizeMask - 1);
    ULONGLONG BlockEnd = (EndOffset + (pStream->pBitmap->BlockSize - 1)) & ~(BlockSizeMask - 1);
    LPBYTE pbDataBlock;
    DWORD cbBytesToCopy = (DWORD)(EndOffset - ByteOffset);
    DWORD cbBlockSize = (DWORD)(BlockEnd - BlockOffset);
    bool bResult = false;

    // Sanity check
    assert(pStream->pBitmap != NULL);
    
    // Cannot load missing blocks if no master file
    if(pStream->pMaster == NULL)
        return false;

    // Allocate space for the file block
    pbDataBlock = STORM_ALLOC(BYTE, cbBlockSize);
    if(pbDataBlock != NULL)
    {
        // Load the entire missing block from the master MPQ
        if(FileStream_Read(pStream->pMaster, &BlockOffset, pbDataBlock, cbBlockSize))
        {
            // We can satisfy the read from the loaded data
            assert(cbBytesToCopy <= cbBlockSize);
            memcpy(pvBuffer, pbDataBlock + (DWORD)(ByteOffset - BlockOffset), cbBytesToCopy);
            bResult = true;

            // Write the file block to the cached archive
            if(pStream->BaseWrite(pStream, &BlockOffset, pbDataBlock, cbBlockSize))
            {
                // Update the file bitmap
                FileBitmap_SetRange(pStream->pBitmap, BlockOffset, BlockEnd);

                // If this fails, the data blocks will be re-downloaded next time,
                // but the file is not corrupt
                ByteOffset = pBitmap->EndOffset;
                if(!pStream->BaseWrite(pStream, &ByteOffset, pBitmap + 1, pBitmap->BitmapSize))
                    bResult = false;
            }
        }

        // Free the file block
        STORM_FREE(pbDataBlock);
    }

    return bResult;
}

static bool LinearStream_ReadBlocks(
    TLinearStream * pStream,                // Pointer to an open stream
    DATA_BLOCK_INFO & bi,
    bool bBlocksAreAvailable)
{
    ULONGLONG EndOffset;
    LPBYTE pbBlockBuffer;
    DWORD BlockToRead = 0;
    DWORD BytesToRead = 0;
    DWORD ReadOffset;
    bool bResult = true;

    // Only do something if there is at least one block to be read
    if(bi.BlockOffset > bi.BlockOffset0)
    {
        // Get the read range
        EndOffset = STORMLIB_MIN(bi.BlockOffset, bi.EndOffset);
        BytesToRead = (DWORD)(EndOffset - bi.ByteOffset);

        // If the block is not available, we need to load them from the master and store to the mirror
        if(bBlocksAreAvailable == false)
        {
            // If we have no master, we cannot satisfy read request
            if(pStream->pMaster == NULL)
                return false;

            // Allocate buffer and read the complete blocks
            BlockToRead = (DWORD)(bi.BlockOffset - bi.BlockOffset0);
            pbBlockBuffer = STORM_ALLOC(BYTE, BlockToRead);
            if(pbBlockBuffer == NULL)
                return false;

            // Load the block buffer from the master stream
            if(FileStream_Read(pStream->pMaster, &bi.BlockOffset0, pbBlockBuffer, BlockToRead))
            {
                // We can now satisfy the request
                ReadOffset = (DWORD)(bi.ByteOffset - bi.BlockOffset0);
                memcpy(bi.ReadBuffer, pbBlockBuffer + ReadOffset, BytesToRead);

                // Store the loaded blocks to the mirror file
                if(pStream->BaseWrite(pStream, &bi.BlockOffset0, pbBlockBuffer, BlockToRead))
                    bi.MirrorUpdated = 1;
            }

            // Free the transfer buffer
            STORM_FREE(pbBlockBuffer);
        }

        // If the blocks are available, we just read them from the mirror
        else
        {
            // Perform the file read
            bResult = pStream->BaseRead(pStream, &bi.ByteOffset, bi.ReadBuffer, BytesToRead);
        }

        // Move the offsets and bit masks
        bi.BlockOffset0 = bi.BlockOffset;
        bi.ByteOffset  += BytesToRead;
        bi.ReadBuffer  += BytesToRead;
    }
    return bResult;
}

static bool LinearStream_Read(
    TLinearStream * pStream,                // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
    DATA_BLOCK_INFO bi;
    TFileBitmap * pBitmap = pStream->pBitmap;
    ULONGLONG ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->FilePos;
    LPBYTE FileBitmap;
    DWORD BlockIndex;
    bool bPrevBlockAvailable;
    bool bBlockAvailable;

    // NOP reading zero bytes
    if(dwBytesToRead == 0)
        return true;

    // Cannot read past the end of the file
    if((ByteOffset + dwBytesToRead) > pStream->FileSize)
    {
        SetLastError(ERROR_HANDLE_EOF);
        return false;
    }

    // If we have no data bitmap, we assume that the file is complete.
    if(pBitmap == NULL)
        return pStream->BaseRead(pStream, pByteOffset, pvBuffer, dwBytesToRead);

    // Calculate the index of the block
    FileBitmap = (LPBYTE)(pBitmap + 1);
    BlockIndex = (DWORD)(ByteOffset / pBitmap->BlockSize);

    // Fill the data block info
    bi.BlockOffset0 = 
    bi.BlockOffset  = ByteOffset & ~((ULONGLONG)pBitmap->BlockSize - 1);
    bi.ByteOffset   = ByteOffset;
    bi.EndOffset    = ByteOffset + dwBytesToRead;
    bi.ReadBuffer   = (LPBYTE)pvBuffer;
    bi.BlockSize    = pBitmap->BlockSize;
    bi.ByteIndex0   =
    bi.ByteIndex    = (BlockIndex / 0x08);
    bi.BitMask0     =
    bi.BitMask      = (BYTE)(0x01 << (BlockIndex & 0x07));
    
    // Check if the current block is available
    bPrevBlockAvailable = (FileBitmap[bi.ByteIndex] & bi.BitMask) ? true : false;

    // Loop as long as we have something to read
    while(bi.BlockOffset < bi.EndOffset)
    {
        // Determine if that block is available in the mirror file
        bBlockAvailable = (FileBitmap[bi.ByteIndex] & bi.BitMask) ? true : false;

        // If the availability has changed,
        // reload all the previous blocks with the same availability
        if(bBlockAvailable != bPrevBlockAvailable)
        {
            if(!LinearStream_ReadBlocks(pStream, bi, bPrevBlockAvailable))
            {
                SetLastError(ERROR_CAN_NOT_COMPLETE);
                return false;
            }

            bPrevBlockAvailable = bBlockAvailable;
        }

        // Move to the next block in the stream
        bi.BlockOffset += bi.BlockSize;
        bi.ByteIndex += (bi.BitMask >> 0x07);
        bi.BitMask = (bi.BitMask >> 0x07) | (bi.BitMask << 0x01);
    }

    // We now need to read the last blocks that weren't loaded in the loop
    if(!LinearStream_ReadBlocks(pStream, bi, bPrevBlockAvailable))
    {
        SetLastError(ERROR_CAN_NOT_COMPLETE);
        return false;
    }

    // We also need to update the file bitmap in the file
    if(bi.MirrorUpdated)
    {
        // Update all bits in the bitmap
        while(bi.ByteIndex0 != bi.ByteIndex || bi.BitMask0 != bi.BitMask)
        {
            FileBitmap[bi.ByteIndex0] |= bi.BitMask0;
            bi.ByteIndex0 += (bi.BitMask0 >> 0x07);
            bi.BitMask0 = (bi.BitMask0 >> 0x07) | (bi.BitMask0 << 0x01);
        }

        // Write the updated bitmap to the mirror file
        pStream->BaseWrite(pStream, &pBitmap->EndOffset, FileBitmap, pBitmap->BitmapSize);
    }

    // Increment the position
    pStream->FilePos = ByteOffset + dwBytesToRead;
    return true;
}

static bool LinearStream_Write(TLinearStream *, ULONGLONG *, const void *, DWORD)
{
    // Writing to linear stream with bitmap is not allowed
    SetLastError(ERROR_ACCESS_DENIED);
    return false;
}

static bool LinearStream_GetSize(TLinearStream * pStream, ULONGLONG * pFileSize)
{
    *pFileSize = pStream->FileSize;
    return true;
}

static bool LinearStream_GetTime(TLinearStream * pStream, ULONGLONG * pFileTime)
{
    *pFileTime = pStream->FileTime;
    return true;
}

static bool LinearStream_GetPos(TLinearStream * pStream, ULONGLONG * pByteOffset)
{
    *pByteOffset = pStream->FilePos;
    return true;
}

static bool LinearStream_GetBmp(
    TLinearStream * pStream,
    void * pvBitmap,
    DWORD Length,
    LPDWORD LengthNeeded)
{
    DWORD TotalLength;
    DWORD CopyLength = sizeof(TFileBitmap);

    // Assumed that we have bitmap now
    assert(pStream->pBitmap != NULL);

    // Give the bitmap length
    TotalLength = sizeof(TFileBitmap) + pStream->pBitmap->BitmapSize;
    if(LengthNeeded != NULL)
        *LengthNeeded = TotalLength;

    // Do we have enough space to fill at least the bitmap structure?
    if(Length >= sizeof(TFileBitmap))
    {
        // Enough space for the complete bitmap?
        if(Length >= TotalLength)
            CopyLength = TotalLength;
        memcpy(pvBitmap, pStream->pBitmap, CopyLength);
        return true;
    }
    else
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return false;
    }
}

static bool LinearStream_Switch(TLinearStream * pStream, TLinearStream * pNewStream)
{
    // Sanity checks
    assert((pNewStream->dwFlags & STREAM_PROVIDER_MASK) == STREAM_PROVIDER_LINEAR);
    assert((pNewStream->dwFlags & BASE_PROVIDER_MASK) == BASE_PROVIDER_FILE);
    assert((pStream->dwFlags & STREAM_PROVIDER_MASK) == STREAM_PROVIDER_LINEAR);
    assert((pStream->dwFlags & BASE_PROVIDER_MASK) == BASE_PROVIDER_FILE);

    // Close the new stream
    pNewStream->BaseClose(pNewStream);

    // Close the source stream
    pStream->BaseClose(pStream);

    // Rename the new data source file to the existing file
    if(!BaseFile_Switch(pStream, pNewStream))
        return false;

    // Now we have to open the "pStream" again
    if(!BaseFile_Open(pStream, pStream->dwFlags))
        return false;

    // We need to cleanup the new data stream
    FileStream_Close(pNewStream);
    return true;
}

static void LinearStream_Close(TLinearStream * pStream)
{
    // Free the souce stream, if any
    if(pStream->pMaster != NULL)
        FileStream_Close(pStream->pMaster);
    pStream->pMaster = NULL;

    // Free the data map, if any
    if(pStream->pBitmap != NULL)
        STORM_FREE(pStream->pBitmap);
    pStream->pBitmap = NULL;

    // Call the base class for closing the stream
    pStream->BaseClose(pStream);
}

static bool LinearStream_Open(TLinearStream * pStream)
{
    // Set the entry points
    pStream->StreamRead    = pStream->BaseRead;
    pStream->StreamWrite   = pStream->BaseWrite;
    pStream->StreamSetSize = pStream->BaseSetSize;
    pStream->StreamGetSize = (STREAM_GETSIZE)LinearStream_GetSize;
    pStream->StreamGetTime = (STREAM_GETTIME)LinearStream_GetTime;
    pStream->StreamGetPos  = (STREAM_GETPOS)LinearStream_GetPos;
    pStream->StreamGetBmp  = (STREAM_GETBMP)CompleteFile_GetBmp;
    pStream->StreamSwitch  = (STREAM_SWITCH)LinearStream_Switch;
    pStream->StreamClose   = (STREAM_CLOSE)LinearStream_Close;

    // If the caller wanted us to load stream bitmap, do it
    if(pStream->dwFlags & STREAM_FLAG_USE_BITMAP)
    {
        // Attempt to load the file bitmap
        LinearStream_LoadBitmap(pStream);

        // Reset the position to zero after manipulating with file pointer
        pStream->FilePos = 0;
        
        // If there is a file bitmap and the file is not complete,
        // we need to set the reading function so that it verifies each block
        if(pStream->pBitmap != NULL)
        {
            // Set different function for retrieving file bitmap
            pStream->StreamGetBmp = (STREAM_GETBMP)LinearStream_GetBmp;

            // If the file is not complete, we also need to set different function for file read+write
            if(pStream->pBitmap->IsComplete == 0)
            {
                // If we also have source file, open that one 
                if(pStream->szSourceName != NULL)
                    pStream->pMaster = FileStream_OpenFile(pStream->szSourceName, STREAM_FLAG_READ_ONLY | STREAM_FLAG_USE_BITMAP);

                // Change functions for read+write
                pStream->StreamWrite = (STREAM_WRITE)LinearStream_Write;
                pStream->StreamRead = (STREAM_READ)LinearStream_Read;
                pStream->dwFlags |= STREAM_FLAG_READ_ONLY;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
// Local functions - partial stream support

static bool IsPartHeader(PPART_FILE_HEADER pPartHdr)
{
    // Version number must be 2
    if(pPartHdr->PartialVersion == 2)
    {
        // GameBuildNumber must be an ASCII number
        if(isdigit(pPartHdr->GameBuildNumber[0]) && isdigit(pPartHdr->GameBuildNumber[1]) && isdigit(pPartHdr->GameBuildNumber[2]))
        {
            // Block size must be power of 2
            if((pPartHdr->BlockSize & (pPartHdr->BlockSize - 1)) == 0)
                return true;
        }
    }

    return false;
}

static bool PartialStream_Read(
    TPartialStream * pStream,
    ULONGLONG * pByteOffset,
    void * pvBuffer,
    DWORD dwBytesToRead)
{
    ULONGLONG RawByteOffset;
    LPBYTE pbBuffer = (LPBYTE)pvBuffer;
    DWORD dwBytesRemaining = dwBytesToRead;
    DWORD dwPartOffset;
    DWORD dwPartIndex;
    DWORD dwBytesRead = 0;
    DWORD dwBlockSize = pStream->BlockSize;
    int nFailReason = ERROR_HANDLE_EOF;             // Why it failed if not enough bytes was read

    // If the byte offset is not entered, use the current position
    if(pByteOffset == NULL)
        pByteOffset = &pStream->VirtualPos;

    // Check if the file position is not at or beyond end of the file
    if(*pByteOffset >= pStream->VirtualSize)
    {
        SetLastError(ERROR_HANDLE_EOF);
        return false;
    }

    // Get the part index where the read offset is
    // Note that the part index should now be within the range,
    // as read requests beyond-EOF are handled by the previous test
    dwPartIndex = (DWORD)(*pByteOffset / pStream->BlockSize);
    assert(dwPartIndex < pStream->BlockCount);

    // If the number of bytes remaining goes past
    // the end of the file, cut them
    if((*pByteOffset + dwBytesRemaining) > pStream->VirtualSize)
        dwBytesRemaining = (DWORD)(pStream->VirtualSize - *pByteOffset);

    // Calculate the offset in the current part
    dwPartOffset = (DWORD)(*pByteOffset) & (pStream->BlockSize - 1);

    // Read all data, one part at a time
    while(dwBytesRemaining != 0)
    {
        PPART_FILE_MAP_ENTRY PartMap = pStream->PartMap + dwPartIndex;
        DWORD dwBytesInPart;

        // If the part is not present in the file, we fail the read
        if((PartMap->Flags & 3) == 0)
        {
            nFailReason = ERROR_FILE_CORRUPT;
            break;
        }

        // If we are in the last part, we have to cut the number of bytes in the last part
        if(dwPartIndex == pStream->BlockCount - 1)
            dwBlockSize = (DWORD)pStream->VirtualSize & (pStream->BlockSize - 1);

        // Get the number of bytes reamining in the current part
        dwBytesInPart = dwBlockSize - dwPartOffset;

        // Compute the raw file offset of the file part
        RawByteOffset = MAKE_OFFSET64(PartMap->BlockOffsHi, PartMap->BlockOffsLo);
        if(RawByteOffset == 0)
        {
            nFailReason = ERROR_FILE_CORRUPT;
            break;
        }

        // If the number of bytes in part is too big, cut it
        if(dwBytesInPart > dwBytesRemaining)
            dwBytesInPart = dwBytesRemaining;

        // Append the offset within the part
        RawByteOffset += dwPartOffset;
        if(!pStream->BaseRead(pStream, &RawByteOffset, pbBuffer, dwBytesInPart))
        {
            nFailReason = ERROR_FILE_CORRUPT;
            break;
        }

        // Increment the file position
        dwBytesRemaining -= dwBytesInPart;
        dwBytesRead += dwBytesInPart;
        pbBuffer += dwBytesInPart;

        // Move to the next file part
        dwPartOffset = 0;
        dwPartIndex++;
    }

    // Move the file position by the number of bytes read
    pStream->VirtualPos = *pByteOffset + dwBytesRead;
    if(dwBytesRead != dwBytesToRead)
        SetLastError(nFailReason);
    return (dwBytesRead == dwBytesToRead);
}

static bool PartialStream_GetPos(
    TPartialStream * pStream,
    ULONGLONG & ByteOffset)
{
    ByteOffset = pStream->VirtualPos;
    return true;
}

static bool PartialStream_GetSize(
    TPartialStream * pStream,               // Pointer to an open stream
    ULONGLONG & FileSize)                   // Pointer where to store file size
{
    FileSize = pStream->VirtualSize;
    return true;
}

static bool PartialStream_GetBitmap(
    TPartialStream * pStream,
    TFileBitmap * pBitmap,
    DWORD Length,
    LPDWORD LengthNeeded)
{
    LPBYTE pbBitmap;
    DWORD TotalLength;
    DWORD BitmapSize = 0;
    DWORD ByteOffset;
    DWORD BitMask;
    bool bResult = false;

    // Do we have stream bitmap?
    BitmapSize = ((pStream->BlockCount - 1) / 8) + 1;

    // Give the bitmap length
    TotalLength = sizeof(TFileBitmap) + BitmapSize;
    if(LengthNeeded != NULL)
        *LengthNeeded = TotalLength;

    // Do we have enough to fill at least the header?
    if(Length >= sizeof(TFileBitmap))
    {
        // Fill the bitmap header
        pBitmap->StartOffset = 0;
        pBitmap->EndOffset   = pStream->VirtualSize;
        pBitmap->BitmapSize  = BitmapSize;
        pBitmap->BlockSize   = pStream->BlockSize;
        pBitmap->BlockCount  = pStream->BlockCount;
        pBitmap->IsComplete  = 1;
        
        // Is there at least one incomplete block?
        for(DWORD i = 0; i < pStream->BlockCount; i++)
        {
            if(pStream->PartMap[i].Flags != 3)
            {
                pBitmap->IsComplete = 0;
                break;
            }
        }

        bResult = true;
    }
    
    // Do we have enough space for supplying the bitmap?
    if(Length >= TotalLength)
    {
        // Fill the file bitmap
        pbBitmap = (LPBYTE)(pBitmap + 1);
        for(DWORD i = 0; i < pStream->BlockCount; i++)
        {
            // Is the block there?
            if(pStream->PartMap[i].Flags == 3)
            {
                ByteOffset = i / 8;
                BitMask = 1 << (i & 7);
                pbBitmap[ByteOffset] |= BitMask;
            }
        }
        bResult = true;
    }

    return bResult;
}

static void PartialStream_Close(TPartialStream * pStream)
{
    // Free the part map
    if(pStream->PartMap != NULL)
        STORM_FREE(pStream->PartMap);
    pStream->PartMap = NULL;

    // Clear variables
    pStream->VirtualSize = 0;
    pStream->VirtualPos = 0;

    // Close the base stream
    assert(pStream->BaseClose != NULL);
    pStream->BaseClose(pStream);
}

static bool PartialStream_Open(TPartialStream * pStream)
{
    PART_FILE_HEADER PartHdr;
    ULONGLONG VirtualSize;                  // Size of the file stored in part file
    ULONGLONG ByteOffset = {0};
    DWORD BlockCount;

    // Sanity check
    assert(pStream->BaseRead != NULL);

    // Attempt to read PART file header
    if(pStream->BaseRead(pStream, &ByteOffset, &PartHdr, sizeof(PART_FILE_HEADER)))
    {
        // We need to swap PART file header on big-endian platforms
        BSWAP_ARRAY32_UNSIGNED(&PartHdr, sizeof(PART_FILE_HEADER));

        // Verify the PART file header
        if(IsPartHeader(&PartHdr))
        {
            // Calculate the number of parts in the file
            VirtualSize = MAKE_OFFSET64(PartHdr.FileSizeHi, PartHdr.FileSizeLo);
            assert(VirtualSize != 0);
            BlockCount = (DWORD)((VirtualSize + PartHdr.BlockSize - 1) / PartHdr.BlockSize);

            // Allocate the map entry array
            pStream->PartMap = STORM_ALLOC(PART_FILE_MAP_ENTRY, BlockCount);
            if(pStream->PartMap != NULL)
            {
                // Load the block map
                if(pStream->BaseRead(pStream, NULL, pStream->PartMap, BlockCount * sizeof(PART_FILE_MAP_ENTRY)))
                {
                    // Swap the array of file map entries
                    BSWAP_ARRAY32_UNSIGNED(pStream->PartMap, BlockCount * sizeof(PART_FILE_MAP_ENTRY));

                    // Fill the members of PART file stream
                    pStream->VirtualSize   = ((ULONGLONG)PartHdr.FileSizeHi) + PartHdr.FileSizeLo;
                    pStream->VirtualPos    = 0;
                    pStream->BlockCount    = BlockCount;
                    pStream->BlockSize     = PartHdr.BlockSize;

                    // Set new function pointers
                    pStream->StreamRead    = (STREAM_READ)PartialStream_Read;
                    pStream->StreamGetPos  = (STREAM_GETPOS)PartialStream_GetPos;
                    pStream->StreamGetSize = (STREAM_GETSIZE)PartialStream_GetSize;
                    pStream->StreamGetTime = (STREAM_GETTIME)LinearStream_GetTime;
                    pStream->StreamGetBmp  = (STREAM_GETBMP)PartialStream_GetBitmap;
                    pStream->StreamClose   = (STREAM_CLOSE)PartialStream_Close;
                    return true;
                }

                // Free the part map
                STORM_FREE(pStream->PartMap);
                pStream->PartMap = NULL;
            }
        }
    }

    SetLastError(ERROR_BAD_FORMAT);
    return false;
}

//-----------------------------------------------------------------------------
// Local functions - encrypted stream support

static const char * szKeyTemplate = "expand 32-byte k000000000000000000000000000000000000000000000000";

static const char * AuthCodeArray[] =
{
    // Starcraft II (Heart of the Swarm)
    // Authentication code URL: http://dist.blizzard.com/mediakey/hots-authenticationcode-bgdl.txt
    //                                                                                          -0C-    -1C--08-    -18--04-    -14--00-    -10-
    "S48B6CDTN5XEQAKQDJNDLJBJ73FDFM3U",         // SC2 Heart of the Swarm-all : "expand 32-byte kQAKQ0000FM3UN5XE000073FD6CDT0000LJBJS48B0000DJND"

    // Diablo III: Agent.exe (1.0.0.954)
    // Address of decryption routine: 00502b00                             
    // Pointer to decryptor object: ECX
    // Pointer to key: ECX+0x5C
    // Authentication code URL: http://dist.blizzard.com/mediakey/d3-authenticationcode-enGB.txt
    //                                                                                           -0C-    -1C--08-    -18--04-    -14--00-    -10-
    "UCMXF6EJY352EFH4XFRXCFH2XC9MQRZK",         // Diablo III Installer (deDE): "expand 32-byte kEFH40000QRZKY3520000XC9MF6EJ0000CFH2UCMX0000XFRX"
    "MMKVHY48RP7WXP4GHYBQ7SL9J9UNPHBP",         // Diablo III Installer (enGB): "expand 32-byte kXP4G0000PHBPRP7W0000J9UNHY4800007SL9MMKV0000HYBQ"
    "8MXLWHQ7VGGLTZ9MQZQSFDCLJYET3CPP",         // Diablo III Installer (enSG): "expand 32-byte kTZ9M00003CPPVGGL0000JYETWHQ70000FDCL8MXL0000QZQS"
    "EJ2R5TM6XFE2GUNG5QDGHKQ9UAKPWZSZ",         // Diablo III Installer (enUS): "expand 32-byte kGUNG0000WZSZXFE20000UAKP5TM60000HKQ9EJ2R00005QDG"
    "PBGFBE42Z6LNK65UGJQ3WZVMCLP4HQQT",         // Diablo III Installer (esES): "expand 32-byte kK65U0000HQQTZ6LN0000CLP4BE420000WZVMPBGF0000GJQ3"
    "X7SEJJS9TSGCW5P28EBSC47AJPEY8VU2",         // Diablo III Installer (esMX): "expand 32-byte kW5P200008VU2TSGC0000JPEYJJS90000C47AX7SE00008EBS"
    "5KVBQA8VYE6XRY3DLGC5ZDE4XS4P7YA2",         // Diablo III Installer (frFR): "expand 32-byte kRY3D00007YA2YE6X0000XS4PQA8V0000ZDE45KVB0000LGC5"
    "478JD2K56EVNVVY4XX8TDWYT5B8KB254",         // Diablo III Installer (itIT): "expand 32-byte kVVY40000B2546EVN00005B8KD2K50000DWYT478J0000XX8T"
    "8TS4VNFQRZTN6YWHE9CHVDH9NVWD474A",         // Diablo III Installer (koKR): "expand 32-byte k6YWH0000474ARZTN0000NVWDVNFQ0000VDH98TS40000E9CH"
    "LJ52Z32DF4LZ4ZJJXVKK3AZQA6GABLJB",         // Diablo III Installer (plPL): "expand 32-byte k4ZJJ0000BLJBF4LZ0000A6GAZ32D00003AZQLJ520000XVKK"
    "K6BDHY2ECUE2545YKNLBJPVYWHE7XYAG",         // Diablo III Installer (ptBR): "expand 32-byte k545Y0000XYAGCUE20000WHE7HY2E0000JPVYK6BD0000KNLB"
    "NDVW8GWLAYCRPGRNY8RT7ZZUQU63VLPR",         // Diablo III Installer (ruRU): "expand 32-byte kXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
    "6VWCQTN8V3ZZMRUCZXV8A8CGUX2TAA8H",         // Diablo III Installer (zhTW): "expand 32-byte kMRUC0000AA8HV3ZZ0000UX2TQTN80000A8CG6VWC0000ZXV8"
//  "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",         // Diablo III Installer (zhCN): "expand 32-byte kXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

    // Starcraft II (Wings of Liberty): Installer.exe (4.1.1.4219)
    // Address of decryption routine: 0053A3D0
    // Pointer to decryptor object: ECX
    // Pointer to key: ECX+0x5C
    // Authentication code URL: http://dist.blizzard.com/mediakey/sc2-authenticationcode-enUS.txt
    //                                                                                          -0C-    -1C--08-    -18--04-    -14--00-    -10-
    "Y45MD3CAK4KXSSXHYD9VY64Z8EKJ4XFX",         // SC2 Wings of Liberty (deDE): "expand 32-byte kSSXH00004XFXK4KX00008EKJD3CA0000Y64ZY45M0000YD9V"
    "G8MN8UDG6NA2ANGY6A3DNY82HRGF29ZH",         // SC2 Wings of Liberty (enGB): "expand 32-byte kANGY000029ZH6NA20000HRGF8UDG0000NY82G8MN00006A3D"
    "W9RRHLB2FDU9WW5B3ECEBLRSFWZSF7HW",         // SC2 Wings of Liberty (enSG): "expand 32-byte kWW5B0000F7HWFDU90000FWZSHLB20000BLRSW9RR00003ECE"
    "3DH5RE5NVM5GTFD85LXGWT6FK859ETR5",         // SC2 Wings of Liberty (enUS): "expand 32-byte kTFD80000ETR5VM5G0000K859RE5N0000WT6F3DH500005LXG"
    "8WLKUAXE94PFQU4Y249PAZ24N4R4XKTQ",         // SC2 Wings of Liberty (esES): "expand 32-byte kQU4Y0000XKTQ94PF0000N4R4UAXE0000AZ248WLK0000249P"
    "A34DXX3VHGGXSQBRFE5UFFDXMF9G4G54",         // SC2 Wings of Liberty (esMX): "expand 32-byte kSQBR00004G54HGGX0000MF9GXX3V0000FFDXA34D0000FE5U"
    "ZG7J9K938HJEFWPQUA768MA2PFER6EAJ",         // SC2 Wings of Liberty (frFR): "expand 32-byte kFWPQ00006EAJ8HJE0000PFER9K9300008MA2ZG7J0000UA76"
    "NE7CUNNNTVAPXV7E3G2BSVBWGVMW8BL2",         // SC2 Wings of Liberty (itIT): "expand 32-byte kXV7E00008BL2TVAP0000GVMWUNNN0000SVBWNE7C00003G2B"
    "3V9E2FTMBM9QQWK7U6MAMWAZWQDB838F",         // SC2 Wings of Liberty (koKR): "expand 32-byte kQWK70000838FBM9Q0000WQDB2FTM0000MWAZ3V9E0000U6MA"
    "2NSFB8MELULJ83U6YHA3UP6K4MQD48L6",         // SC2 Wings of Liberty (plPL): "expand 32-byte k83U6000048L6LULJ00004MQDB8ME0000UP6K2NSF0000YHA3"
    "QA2TZ9EWZ4CUU8BMB5WXCTY65F9CSW4E",         // SC2 Wings of Liberty (ptBR): "expand 32-byte kU8BM0000SW4EZ4CU00005F9CZ9EW0000CTY6QA2T0000B5WX"
    "VHB378W64BAT9SH7D68VV9NLQDK9YEGT",         // SC2 Wings of Liberty (ruRU): "expand 32-byte k9SH70000YEGT4BAT0000QDK978W60000V9NLVHB30000D68V"
    "U3NFQJV4M6GC7KBN9XQJ3BRDN3PLD9NE",         // SC2 Wings of Liberty (zhTW): "expand 32-byte k7KBN0000D9NEM6GC0000N3PLQJV400003BRDU3NF00009XQJ"

    NULL
};

static DWORD Rol32(DWORD dwValue, DWORD dwRolCount)
{
    DWORD dwShiftRight = 32 - dwRolCount;

    return (dwValue << dwRolCount) | (dwValue >> dwShiftRight);
}

static void CreateKeyFromAuthCode(
    LPBYTE pbKeyBuffer,
    const char * szAuthCode)
{
    LPDWORD KeyPosition = (LPDWORD)(pbKeyBuffer + 0x10);
    LPDWORD AuthCode32 = (LPDWORD)szAuthCode;

    memcpy(pbKeyBuffer, szKeyTemplate, MPQE_CHUNK_SIZE);
    KeyPosition[0x00] = AuthCode32[0x03];
    KeyPosition[0x02] = AuthCode32[0x07];
    KeyPosition[0x03] = AuthCode32[0x02];
    KeyPosition[0x05] = AuthCode32[0x06];
    KeyPosition[0x06] = AuthCode32[0x01];
    KeyPosition[0x08] = AuthCode32[0x05];
    KeyPosition[0x09] = AuthCode32[0x00];
    KeyPosition[0x0B] = AuthCode32[0x04];
    BSWAP_ARRAY32_UNSIGNED(pbKeyBuffer, MPQE_CHUNK_SIZE);
}

static void DecryptFileChunk(
    DWORD * MpqData,
    LPBYTE pbKey,
    ULONGLONG ByteOffset,
    DWORD dwLength)
{
    ULONGLONG ChunkOffset;
    DWORD KeyShuffled[0x10];
    DWORD KeyMirror[0x10];
    DWORD RoundCount = 0x14;

    // Prepare the key
    ChunkOffset = ByteOffset / MPQE_CHUNK_SIZE;
    memcpy(KeyMirror, pbKey, MPQE_CHUNK_SIZE);
    BSWAP_ARRAY32_UNSIGNED(KeyMirror, MPQE_CHUNK_SIZE);
    KeyMirror[0x05] = (DWORD)(ChunkOffset >> 32);
    KeyMirror[0x08] = (DWORD)(ChunkOffset);

    while(dwLength >= MPQE_CHUNK_SIZE)
    {
        // Shuffle the key - part 1
        KeyShuffled[0x0E] = KeyMirror[0x00];
        KeyShuffled[0x0C] = KeyMirror[0x01];
        KeyShuffled[0x05] = KeyMirror[0x02];
        KeyShuffled[0x0F] = KeyMirror[0x03];
        KeyShuffled[0x0A] = KeyMirror[0x04];
        KeyShuffled[0x07] = KeyMirror[0x05];
        KeyShuffled[0x0B] = KeyMirror[0x06];
        KeyShuffled[0x09] = KeyMirror[0x07];
        KeyShuffled[0x03] = KeyMirror[0x08];
        KeyShuffled[0x06] = KeyMirror[0x09];
        KeyShuffled[0x08] = KeyMirror[0x0A];
        KeyShuffled[0x0D] = KeyMirror[0x0B];
        KeyShuffled[0x02] = KeyMirror[0x0C];
        KeyShuffled[0x04] = KeyMirror[0x0D];
        KeyShuffled[0x01] = KeyMirror[0x0E];
        KeyShuffled[0x00] = KeyMirror[0x0F];
        
        // Shuffle the key - part 2
        for(DWORD i = 0; i < RoundCount; i += 2)
        {
            KeyShuffled[0x0A] = KeyShuffled[0x0A] ^ Rol32((KeyShuffled[0x0E] + KeyShuffled[0x02]), 0x07);
            KeyShuffled[0x03] = KeyShuffled[0x03] ^ Rol32((KeyShuffled[0x0A] + KeyShuffled[0x0E]), 0x09);
            KeyShuffled[0x02] = KeyShuffled[0x02] ^ Rol32((KeyShuffled[0x03] + KeyShuffled[0x0A]), 0x0D);
            KeyShuffled[0x0E] = KeyShuffled[0x0E] ^ Rol32((KeyShuffled[0x02] + KeyShuffled[0x03]), 0x12);

            KeyShuffled[0x07] = KeyShuffled[0x07] ^ Rol32((KeyShuffled[0x0C] + KeyShuffled[0x04]), 0x07);
            KeyShuffled[0x06] = KeyShuffled[0x06] ^ Rol32((KeyShuffled[0x07] + KeyShuffled[0x0C]), 0x09);
            KeyShuffled[0x04] = KeyShuffled[0x04] ^ Rol32((KeyShuffled[0x06] + KeyShuffled[0x07]), 0x0D);
            KeyShuffled[0x0C] = KeyShuffled[0x0C] ^ Rol32((KeyShuffled[0x04] + KeyShuffled[0x06]), 0x12);

            KeyShuffled[0x0B] = KeyShuffled[0x0B] ^ Rol32((KeyShuffled[0x05] + KeyShuffled[0x01]), 0x07);
            KeyShuffled[0x08] = KeyShuffled[0x08] ^ Rol32((KeyShuffled[0x0B] + KeyShuffled[0x05]), 0x09);
            KeyShuffled[0x01] = KeyShuffled[0x01] ^ Rol32((KeyShuffled[0x08] + KeyShuffled[0x0B]), 0x0D);
            KeyShuffled[0x05] = KeyShuffled[0x05] ^ Rol32((KeyShuffled[0x01] + KeyShuffled[0x08]), 0x12);

            KeyShuffled[0x09] = KeyShuffled[0x09] ^ Rol32((KeyShuffled[0x0F] + KeyShuffled[0x00]), 0x07);
            KeyShuffled[0x0D] = KeyShuffled[0x0D] ^ Rol32((KeyShuffled[0x09] + KeyShuffled[0x0F]), 0x09);
            KeyShuffled[0x00] = KeyShuffled[0x00] ^ Rol32((KeyShuffled[0x0D] + KeyShuffled[0x09]), 0x0D);
            KeyShuffled[0x0F] = KeyShuffled[0x0F] ^ Rol32((KeyShuffled[0x00] + KeyShuffled[0x0D]), 0x12);

            KeyShuffled[0x04] = KeyShuffled[0x04] ^ Rol32((KeyShuffled[0x0E] + KeyShuffled[0x09]), 0x07);
            KeyShuffled[0x08] = KeyShuffled[0x08] ^ Rol32((KeyShuffled[0x04] + KeyShuffled[0x0E]), 0x09);
            KeyShuffled[0x09] = KeyShuffled[0x09] ^ Rol32((KeyShuffled[0x08] + KeyShuffled[0x04]), 0x0D);
            KeyShuffled[0x0E] = KeyShuffled[0x0E] ^ Rol32((KeyShuffled[0x09] + KeyShuffled[0x08]), 0x12);

            KeyShuffled[0x01] = KeyShuffled[0x01] ^ Rol32((KeyShuffled[0x0C] + KeyShuffled[0x0A]), 0x07);
            KeyShuffled[0x0D] = KeyShuffled[0x0D] ^ Rol32((KeyShuffled[0x01] + KeyShuffled[0x0C]), 0x09);
            KeyShuffled[0x0A] = KeyShuffled[0x0A] ^ Rol32((KeyShuffled[0x0D] + KeyShuffled[0x01]), 0x0D);
            KeyShuffled[0x0C] = KeyShuffled[0x0C] ^ Rol32((KeyShuffled[0x0A] + KeyShuffled[0x0D]), 0x12);

            KeyShuffled[0x00] = KeyShuffled[0x00] ^ Rol32((KeyShuffled[0x05] + KeyShuffled[0x07]), 0x07);
            KeyShuffled[0x03] = KeyShuffled[0x03] ^ Rol32((KeyShuffled[0x00] + KeyShuffled[0x05]), 0x09);
            KeyShuffled[0x07] = KeyShuffled[0x07] ^ Rol32((KeyShuffled[0x03] + KeyShuffled[0x00]), 0x0D);
            KeyShuffled[0x05] = KeyShuffled[0x05] ^ Rol32((KeyShuffled[0x07] + KeyShuffled[0x03]), 0x12);

            KeyShuffled[0x02] = KeyShuffled[0x02] ^ Rol32((KeyShuffled[0x0F] + KeyShuffled[0x0B]), 0x07);
            KeyShuffled[0x06] = KeyShuffled[0x06] ^ Rol32((KeyShuffled[0x02] + KeyShuffled[0x0F]), 0x09);
            KeyShuffled[0x0B] = KeyShuffled[0x0B] ^ Rol32((KeyShuffled[0x06] + KeyShuffled[0x02]), 0x0D);
            KeyShuffled[0x0F] = KeyShuffled[0x0F] ^ Rol32((KeyShuffled[0x0B] + KeyShuffled[0x06]), 0x12);
        }

        // Decrypt one data chunk
        BSWAP_ARRAY32_UNSIGNED(MpqData, MPQE_CHUNK_SIZE);
        MpqData[0x00] = MpqData[0x00] ^ (KeyShuffled[0x0E] + KeyMirror[0x00]);
        MpqData[0x01] = MpqData[0x01] ^ (KeyShuffled[0x04] + KeyMirror[0x0D]);
        MpqData[0x02] = MpqData[0x02] ^ (KeyShuffled[0x08] + KeyMirror[0x0A]);
        MpqData[0x03] = MpqData[0x03] ^ (KeyShuffled[0x09] + KeyMirror[0x07]);
        MpqData[0x04] = MpqData[0x04] ^ (KeyShuffled[0x0A] + KeyMirror[0x04]);
        MpqData[0x05] = MpqData[0x05] ^ (KeyShuffled[0x0C] + KeyMirror[0x01]);
        MpqData[0x06] = MpqData[0x06] ^ (KeyShuffled[0x01] + KeyMirror[0x0E]);
        MpqData[0x07] = MpqData[0x07] ^ (KeyShuffled[0x0D] + KeyMirror[0x0B]);
        MpqData[0x08] = MpqData[0x08] ^ (KeyShuffled[0x03] + KeyMirror[0x08]);
        MpqData[0x09] = MpqData[0x09] ^ (KeyShuffled[0x07] + KeyMirror[0x05]);
        MpqData[0x0A] = MpqData[0x0A] ^ (KeyShuffled[0x05] + KeyMirror[0x02]);
        MpqData[0x0B] = MpqData[0x0B] ^ (KeyShuffled[0x00] + KeyMirror[0x0F]);
        MpqData[0x0C] = MpqData[0x0C] ^ (KeyShuffled[0x02] + KeyMirror[0x0C]);
        MpqData[0x0D] = MpqData[0x0D] ^ (KeyShuffled[0x06] + KeyMirror[0x09]);
        MpqData[0x0E] = MpqData[0x0E] ^ (KeyShuffled[0x0B] + KeyMirror[0x06]);
        MpqData[0x0F] = MpqData[0x0F] ^ (KeyShuffled[0x0F] + KeyMirror[0x03]);
        BSWAP_ARRAY32_UNSIGNED(MpqData, MPQE_CHUNK_SIZE);

        // Update byte offset in the key
        KeyMirror[0x08]++;
        if(KeyMirror[0x08] == 0)
            KeyMirror[0x05]++;

        // Move pointers and decrease number of bytes to decrypt
        MpqData  += (MPQE_CHUNK_SIZE / sizeof(DWORD));
        dwLength -= MPQE_CHUNK_SIZE;
    }
}

static bool DetectFileKey(LPBYTE pbKeyBuffer, LPBYTE pbEncryptedHeader)
{
    ULONGLONG ByteOffset = 0;
    BYTE FileHeader[MPQE_CHUNK_SIZE];

    // We just try all known keys one by one
    for(int i = 0; AuthCodeArray[i] != NULL; i++)
    {
        // Prepare they decryption key from game serial number
        CreateKeyFromAuthCode(pbKeyBuffer, AuthCodeArray[i]);

        // Try to decrypt with the given key 
        memcpy(FileHeader, pbEncryptedHeader, MPQE_CHUNK_SIZE);
        DecryptFileChunk((LPDWORD)FileHeader, pbKeyBuffer, ByteOffset, MPQE_CHUNK_SIZE);

        // We check the decrypted data
        // All known encrypted MPQs have header at the begin of the file,
        // so we check for MPQ signature there.
        if(FileHeader[0] == 'M' && FileHeader[1] == 'P' && FileHeader[2] == 'Q')
            return true;
    }

    // Key not found, sorry
    return false;
}

static bool EncryptedStream_Read(
    TEncryptedStream * pStream,             // Pointer to an open stream
    ULONGLONG * pByteOffset,                // Pointer to file byte offset. If NULL, it reads from the current position
    void * pvBuffer,                        // Pointer to data to be read
    DWORD dwBytesToRead)                    // Number of bytes to read from the file
{
    ULONGLONG StartOffset;                  // Offset of the first byte to be read from the file
    ULONGLONG ByteOffset;                   // Offset that the caller wants
    ULONGLONG EndOffset;                    // End offset that is to be read from the file
    DWORD dwBytesToAllocate;
    DWORD dwBytesToDecrypt;
    DWORD dwOffsetInCache;
    LPBYTE pbMpqData = NULL;
    bool bResult = false;

    // Get the byte offset
    ByteOffset = (pByteOffset != NULL) ? *pByteOffset : pStream->FilePos;

    // Cut it down to MPQE chunk size
    StartOffset = ByteOffset;
    StartOffset = StartOffset & ~(MPQE_CHUNK_SIZE - 1);
    EndOffset = ByteOffset + dwBytesToRead;

    // Calculate number of bytes to decrypt
    dwBytesToDecrypt = (DWORD)(EndOffset - StartOffset);
    dwBytesToAllocate = (dwBytesToDecrypt + (MPQE_CHUNK_SIZE - 1)) & ~(MPQE_CHUNK_SIZE - 1);

    // Allocate buffers for encrypted and decrypted data
    pbMpqData = STORM_ALLOC(BYTE, dwBytesToAllocate);
    if(pbMpqData)
    {
        // Get the offset of the desired data in the cache
        dwOffsetInCache = (DWORD)(ByteOffset - StartOffset);

        // Read the file from the stream as-is
        if(pStream->BaseRead(pStream, &StartOffset, pbMpqData, dwBytesToDecrypt))
        {
            // Decrypt the data
            DecryptFileChunk((LPDWORD)pbMpqData, pStream->Key, StartOffset, dwBytesToAllocate);

            // Copy the decrypted data
            memcpy(pvBuffer, pbMpqData + dwOffsetInCache, dwBytesToRead);
            bResult = true;
        }
        else
        {
            assert(false);
        }

        // Free decryption buffer        
        STORM_FREE(pbMpqData);
    }

    // Free buffers and exit
    return bResult;
}

static bool EncryptedStream_Open(TEncryptedStream * pStream)
{
    ULONGLONG ByteOffset = 0;
    BYTE EncryptedHeader[MPQE_CHUNK_SIZE];

    // Sanity check
    assert(pStream->BaseRead != NULL);

    // Load one MPQE chunk and try to detect the file key
    if(pStream->BaseRead(pStream, &ByteOffset, EncryptedHeader, sizeof(EncryptedHeader)))
    {
        // Attempt to decrypt the MPQ header with all known keys
        if(DetectFileKey(pStream->Key, EncryptedHeader))
        {
            // Assign functions
            pStream->StreamRead    = (STREAM_READ)EncryptedStream_Read;
            pStream->StreamGetSize = (STREAM_GETSIZE)LinearStream_GetSize;
            pStream->StreamGetTime = (STREAM_GETTIME)LinearStream_GetTime;
            pStream->StreamGetPos  = (STREAM_GETPOS)LinearStream_GetPos;
            pStream->StreamGetBmp  = (STREAM_GETBMP)CompleteFile_GetBmp;
            pStream->StreamClose   = pStream->BaseClose;

            // We need to reset the position back to the begin of the file
            pStream->BaseRead(pStream, &ByteOffset, EncryptedHeader, 0);
            return true;
        }

        // An unknown key
        SetLastError(ERROR_UNKNOWN_FILE_KEY);
    }
    return false;
}

//-----------------------------------------------------------------------------
// File stream allocation function

/**
 * This function allocates an empty structure for the file stream
 * The stream structure is created as variable length, linear block of data
 * The file name is placed after the end of the stream structure data
 *
 * \a szFileName    Name of the file
 * \a dwStreamFlags Stream flags telling what kind of stream structure to create
 */

static TFileStream * AllocateFileStream(
    const TCHAR * szFileName,
    DWORD dwStreamFlags)
{
    TFileStream * pStream;
    TCHAR * szSourceName;
    size_t FileNameSize = (_tcslen(szFileName) + 1) * sizeof(TCHAR);
    size_t StreamSize = 0;
    DWORD dwStreamProvider = dwStreamFlags & STREAM_PROVIDER_MASK;
    DWORD dwBaseProvider = dwStreamFlags & BASE_PROVIDER_MASK;

    // The "file:" prefix forces the BASE_PROVIDER_FILE
    if(!_tcsicmp(szFileName, _T("file:")))
    {
        dwBaseProvider = BASE_PROVIDER_FILE;
        szFileName += 5;
    }
    
    // The "map:" prefix forces the BASE_PROVIDER_MAP
    if(!_tcsicmp(szFileName, _T("map:")))
    {
        dwBaseProvider = BASE_PROVIDER_MAP;
        szFileName += 4;
    }
    
    // The "http:" prefix forces the BASE_PROVIDER_HTTP
    if(!_tcsicmp(szFileName, _T("http:")))
    {
        dwBaseProvider = BASE_PROVIDER_HTTP;
        szFileName += 5;
    }

    // Re-create the stream flags
    dwStreamFlags = (dwStreamFlags & STREAM_FLAG_MASK) | dwStreamProvider | dwBaseProvider;

    // Allocate file stream for each stream provider
    switch(dwStreamFlags & STREAM_PROVIDER_MASK)
    {
        case STREAM_PROVIDER_LINEAR:    // Allocate structure for linear stream 
            StreamSize = sizeof(TLinearStream);
            break;

        case STREAM_PROVIDER_PARTIAL:
            dwStreamFlags |= STREAM_FLAG_READ_ONLY;
            StreamSize = sizeof(TPartialStream);
            break;

        case STREAM_PROVIDER_ENCRYPTED:
            dwStreamFlags |= STREAM_FLAG_READ_ONLY;
            StreamSize = sizeof(TEncryptedStream);
            break;

        default:
            return NULL;
    }

    // Allocate the stream structure for the given stream type
    pStream = (TFileStream *)STORM_ALLOC(BYTE, StreamSize + FileNameSize);
    if(pStream == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    // Fill the stream structure with zeros
    memset(pStream, 0, StreamSize);

    // Remember the file name
    pStream->szFileName = (TCHAR *)((BYTE *)pStream + StreamSize);
    pStream->dwFlags = dwStreamFlags;
    memcpy(pStream->szFileName, szFileName, FileNameSize);

    // If we have source file name, setup it as well
    szSourceName = _tcschr(pStream->szFileName, _T('*'));
    if(szSourceName != NULL)
    {
        // Remember the source name and cut these two file names
        pStream->szSourceName = szSourceName + 1;
        szSourceName[0] = 0;
    }
    return pStream;
}

//-----------------------------------------------------------------------------
// Public functions

/**
 * This function creates a new file for read-write access
 *
 * - If the current platform supports file sharing,
 *   the file must be created for read sharing (i.e. another application
 *   can open the file for read, but not for write)
 * - If the file does not exist, the function must create new one
 * - If the file exists, the function must rewrite it and set to zero size
 * - The parameters of the function must be validate by the caller
 * - The function must initialize all stream function pointers in TFileStream
 * - If the function fails from any reason, it must close all handles
 *   and free all memory that has been allocated in the process of stream creation,
 *   including the TFileStream structure itself
 *
 * \a szFileName Name of the file to create
 */

TFileStream * FileStream_CreateFile(
    const TCHAR * szFileName,
    DWORD dwStreamFlags)
{
    TFileStream * pStream;

    // We only support creation of linear, local file
    if((dwStreamFlags & (STREAM_PROVIDER_MASK | BASE_PROVIDER_MASK)) != (STREAM_PROVIDER_LINEAR | BASE_PROVIDER_FILE))
    {
        SetLastError(ERROR_NOT_SUPPORTED);
        return NULL;
    }

    // Allocate file stream structure for linear stream
    pStream = AllocateFileStream(szFileName, dwStreamFlags);
    if(pStream != NULL)
    {
        // Attempt to create the disk file
        if(BaseFile_Create(pStream))
        {
            // Fill the stream provider functions
            pStream->StreamRead    = pStream->BaseRead;
            pStream->StreamWrite   = pStream->BaseWrite;
            pStream->StreamSetSize = pStream->BaseSetSize;
            pStream->StreamGetSize = (STREAM_GETSIZE)LinearStream_GetSize;
            pStream->StreamGetTime = (STREAM_GETTIME)LinearStream_GetTime;
            pStream->StreamGetPos  = (STREAM_GETPOS)LinearStream_GetPos;
            pStream->StreamGetBmp  = (STREAM_GETBMP)CompleteFile_GetBmp;
            pStream->StreamSwitch  = (STREAM_SWITCH)LinearStream_Switch;
            pStream->StreamClose   = pStream->BaseClose;
            return pStream;
        }

        // File create failed, delete the stream
        STORM_FREE(pStream);
        pStream = NULL;
    }

    // Return the stream
    return pStream;
}

/**
 * This function opens an existing file for read or read-write access
 * - If the current platform supports file sharing,
 *   the file must be open for read sharing (i.e. another application
 *   can open the file for read, but not for write)
 * - If the file does not exist, the function must return NULL
 * - If the file exists but cannot be open, then function must return NULL
 * - The parameters of the function must be validate by the caller
 * - The function must check if the file is a PART file,
 *   and create TPartialStream object if so.
 * - The function must initialize all stream function pointers in TFileStream
 * - If the function fails from any reason, it must close all handles
 *   and free all memory that has been allocated in the process of stream creation,
 *   including the TFileStream structure itself
 *
 * \a szFileName Name of the file to open
 * \a dwStreamFlags specifies the provider and base storage type
 */

TFileStream * FileStream_OpenFile(
    const TCHAR * szFileName,
    DWORD dwStreamFlags)
{
    TFileStream * pStream = NULL;
    DWORD dwClearMask = STREAM_OPTIONS_MASK;
    bool bStreamResult = false;
    bool bBaseResult = false;

    // Allocate the stream of the given type
    pStream = AllocateFileStream(szFileName, dwStreamFlags);
    if(pStream == NULL)
        return NULL;

    // Few special checks when we want the stream to be cached from another source,
    if(pStream->szSourceName != NULL)
    {
        // We don't allow other base types than BASE_PROVIDER_FILE
        if((pStream->dwFlags & BASE_PROVIDER_MASK) != BASE_PROVIDER_FILE)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            STORM_FREE(pStream);
            return NULL;
        }

        // Clear the STREAM_FLAG_READ_ONLY flag for the base file when being open
        // as local cache of a remote file
        dwClearMask &= ~STREAM_FLAG_READ_ONLY;
    }

    // Now initialize the respective base provider
    switch(pStream->dwFlags & BASE_PROVIDER_MASK)
    {
        case BASE_PROVIDER_FILE:
            bBaseResult = BaseFile_Open(pStream, pStream->dwFlags & dwClearMask);
            break;

        case BASE_PROVIDER_MAP:
            pStream->dwFlags |= STREAM_FLAG_READ_ONLY;
            bBaseResult = BaseMap_Open(pStream);
            break;

        case BASE_PROVIDER_HTTP:
            pStream->dwFlags |= STREAM_FLAG_READ_ONLY;
            bBaseResult = BaseHttp_Open(pStream);
            break;
    }

    // If we failed to open the base storage, fail the operation
    if(bBaseResult == false)
    {
        STORM_FREE(pStream);
        return NULL;
    }

    // Now initialize the stream provider
    switch(pStream->dwFlags & STREAM_PROVIDER_MASK)
    {
        case STREAM_PROVIDER_LINEAR:
            bStreamResult = LinearStream_Open((TLinearStream *)pStream);
            break;

        case STREAM_PROVIDER_PARTIAL:
            bStreamResult = PartialStream_Open((TPartialStream *)pStream);
            break;

        case STREAM_PROVIDER_ENCRYPTED:
            bStreamResult = EncryptedStream_Open((TEncryptedStream *)pStream);
            break;
    }

    // If the operation failed, free the stream and set it to NULL
    if(bStreamResult == false)
    {
        // Only close the base stream
        pStream->BaseClose(pStream);
        STORM_FREE(pStream);
        pStream = NULL;
    }

    return pStream;
}

/**
 * Reads data from the stream
 *
 * - Returns true if the read operation succeeded and all bytes have been read
 * - Returns false if either read failed or not all bytes have been read
 * - If the pByteOffset is NULL, the function must read the data from the current file position
 * - The function can be called with dwBytesToRead = 0. In that case, pvBuffer is ignored
 *   and the function just adjusts file pointer.
 *
 * \a pStream Pointer to an open stream
 * \a pByteOffset Pointer to file byte offset. If NULL, it reads from the current position
 * \a pvBuffer Pointer to data to be read
 * \a dwBytesToRead Number of bytes to read from the file
 *
 * \returns
 * - If the function reads the required amount of bytes, it returns true.
 * - If the function reads less than required bytes, it returns false and GetLastError() returns ERROR_HANDLE_EOF
 * - If the function fails, it reads false and GetLastError() returns an error code different from ERROR_HANDLE_EOF
 */
bool FileStream_Read(TFileStream * pStream, ULONGLONG * pByteOffset, void * pvBuffer, DWORD dwBytesToRead)
{
    assert(pStream->StreamRead != NULL);
    return pStream->StreamRead(pStream, pByteOffset, pvBuffer, dwBytesToRead);
}

/**
 * This function writes data to the stream
 *
 * - Returns true if the write operation succeeded and all bytes have been written
 * - Returns false if either write failed or not all bytes have been written
 * - If the pByteOffset is NULL, the function must write the data to the current file position
 *
 * \a pStream Pointer to an open stream
 * \a pByteOffset Pointer to file byte offset. If NULL, it reads from the current position
 * \a pvBuffer Pointer to data to be written
 * \a dwBytesToWrite Number of bytes to write to the file
 */
bool FileStream_Write(TFileStream * pStream, ULONGLONG * pByteOffset, const void * pvBuffer, DWORD dwBytesToWrite)
{
    if(pStream->dwFlags & STREAM_FLAG_READ_ONLY)
        return false;

    assert(pStream->StreamWrite != NULL);
    return pStream->StreamWrite(pStream, pByteOffset, pvBuffer, dwBytesToWrite);
}

/**
 * This function returns the current file position
 * \a pStream
 * \a ByteOffset
 */
bool FileStream_GetPos(TFileStream * pStream, ULONGLONG * pByteOffset)
{
    assert(pStream->StreamGetPos != NULL);
    return pStream->StreamGetPos(pStream, pByteOffset);
}

/**
 * Returns the size of a file
 *
 * \a pStream Pointer to an open stream
 * \a FileSize Pointer where to store the file size
 */
bool FileStream_GetSize(TFileStream * pStream, ULONGLONG * pFileSize)
{
    assert(pStream->StreamGetSize != NULL);
    return pStream->StreamGetSize(pStream, pFileSize);
}

/**
 * Sets the size of a file
 *
 * \a pStream Pointer to an open stream
 * \a NewFileSize File size to set
 */
bool FileStream_SetSize(TFileStream * pStream, ULONGLONG NewFileSize)
{                                 
    if(pStream->dwFlags & STREAM_FLAG_READ_ONLY)
        return false;

    assert(pStream->StreamSetSize != NULL);
    return pStream->StreamSetSize(pStream, NewFileSize);
}

/**
 * Returns the last write time of a file
 *
 * \a pStream Pointer to an open stream
 * \a pFileType Pointer where to store the file last write time
 */
bool FileStream_GetTime(TFileStream * pStream, ULONGLONG * pFileTime)
{
    assert(pStream->StreamGetTime != NULL);
    return pStream->StreamGetTime(pStream, pFileTime);
}

/**
 * Returns the stream flags
 *
 * \a pStream Pointer to an open stream
 * \a pdwStreamFlags Pointer where to store the stream flags
 */
bool FileStream_GetFlags(TFileStream * pStream, LPDWORD pdwStreamFlags)
{
    *pdwStreamFlags = pStream->dwFlags;
    return true;
}

/**
 * Switches a stream with another. Used for final phase of archive compacting.
 * Performs these steps:
 *
 * 1) Closes the handle to the existing MPQ
 * 2) Renames the temporary MPQ to the original MPQ, overwrites existing one
 * 3) Opens the MPQ stores the handle and stream position to the new stream structure
 *
 * \a pStream Pointer to an open stream
 * \a pTempStream Temporary ("working") stream (created during archive compacting)
 */
bool FileStream_Switch(TFileStream * pStream, TFileStream * pNewStream)
{
    if(pStream->dwFlags & STREAM_FLAG_READ_ONLY)
        return false;

    assert(pStream->StreamSwitch != NULL);
    return pStream->StreamSwitch(pStream, pNewStream);
}

/**
 * Returns the file name of the stream
 *
 * \a pStream Pointer to an open stream
 */
const TCHAR * FileStream_GetFileName(TFileStream * pStream)
{
    assert(pStream != NULL);
    return pStream->szFileName;
}

/**
 * Returns true if the stream is read-only
 *
 * \a pStream Pointer to an open stream
 */
bool FileStream_IsReadOnly(TFileStream * pStream)
{
    return (pStream->dwFlags & STREAM_FLAG_READ_ONLY) ? true : false;
}

/**
 * This function retrieves the file bitmap. A file bitmap is an array
 * of bits, each bit representing one file block. A value of 1 means
 * that the block is present in the file, a value of 0 means that the
 * block is not present.
 *
 * \a pStream Pointer to an open stream
 * \a pBitmap Pointer to buffer where to store the file bitmap
 * \a Length  Size of buffer pointed by pBitmap, in bytes
 * \a LengthNeeded If non-NULL, the function supplies the necessary byte size of the buffer
 */
bool FileStream_GetBitmap(TFileStream * pStream, void * pvBitmap, DWORD Length, LPDWORD LengthNeeded)
{
    assert(pStream->StreamGetBmp != NULL);
    return pStream->StreamGetBmp(pStream, pvBitmap, Length, LengthNeeded);
}

/**
 * This function closes an archive file and frees any data buffers
 * that have been allocated for stream management. The function must also
 * support partially allocated structure, i.e. one or more buffers
 * can be NULL, if there was an allocation failure during the process
 *
 * \a pStream Pointer to an open stream
 */
void FileStream_Close(TFileStream * pStream)
{
    // Check if the stream structure is allocated at all
    if(pStream != NULL)
    {
        // Close the stream provider.
        // This will also close the base stream
        assert(pStream->StreamClose != NULL);
        pStream->StreamClose(pStream);

        // Free the stream itself
        STORM_FREE(pStream);
    }
}

//-----------------------------------------------------------------------------
// Utility functions (ANSI)

const char * GetPlainFileName(const char * szFileName)
{
    const char * szPlainName = szFileName;

    while(*szFileName != 0)
    {
        if(*szFileName == '\\' || *szFileName == '/')
            szPlainName = szFileName + 1;
        szFileName++;
    }

    return szPlainName;
}

void CopyFileName(char * szTarget, const char * szSource, size_t cchLength)
{
    memcpy(szTarget, szSource, cchLength);
    szTarget[cchLength] = 0;
}

//-----------------------------------------------------------------------------
// Utility functions (UNICODE) only exist in the ANSI version of the library
// In ANSI builds, TCHAR = char, so we don't need these functions implemented

#ifdef _UNICODE
const TCHAR * GetPlainFileName(const TCHAR * szFileName)
{
    const TCHAR * szPlainName = szFileName;

    while(*szFileName != 0)
    {
        if(*szFileName == '\\' || *szFileName == '/')
            szPlainName = szFileName + 1;
        szFileName++;
    }

    return szPlainName;
}

void CopyFileName(TCHAR * szTarget, const char * szSource, size_t cchLength)
{
    mbstowcs(szTarget, szSource, cchLength);
    szTarget[cchLength] = 0;
}

void CopyFileName(char * szTarget, const TCHAR * szSource, size_t cchLength)
{
    wcstombs(szTarget, szSource, cchLength);
    szTarget[cchLength] = 0;
}
#endif

//-----------------------------------------------------------------------------
// main - for testing purposes

#ifdef __STORMLIB_TEST__
int FileStream_Test(const TCHAR * szFileName, DWORD dwStreamFlags)
{
    TFileStream * pStream;
    TMPQHeader MpqHeader;
    ULONGLONG FilePos;
    TMPQBlock * pBlock;
    TMPQHash * pHash;

    InitializeMpqCryptography();

    pStream = FileStream_OpenFile(szFileName, dwStreamFlags);
    if(pStream == NULL)
        return GetLastError();

    // Read the MPQ header
    FileStream_Read(pStream, NULL, &MpqHeader, MPQ_HEADER_SIZE_V2);
    if(MpqHeader.dwID != ID_MPQ)
        return ERROR_FILE_CORRUPT;

    // Read the hash table
    pHash = STORM_ALLOC(TMPQHash, MpqHeader.dwHashTableSize);
    if(pHash != NULL)
    {
        FilePos = MpqHeader.dwHashTablePos;
        FileStream_Read(pStream, &FilePos, pHash, MpqHeader.dwHashTableSize * sizeof(TMPQHash));
        DecryptMpqBlock(pHash, MpqHeader.dwHashTableSize * sizeof(TMPQHash), MPQ_KEY_HASH_TABLE);
        STORM_FREE(pHash);
    }

    // Read the block table
    pBlock = STORM_ALLOC(TMPQBlock, MpqHeader.dwBlockTableSize);
    if(pBlock != NULL)
    {
        FilePos = MpqHeader.dwBlockTablePos;
        FileStream_Read(pStream, &FilePos, pBlock, MpqHeader.dwBlockTableSize * sizeof(TMPQBlock));
        DecryptMpqBlock(pBlock, MpqHeader.dwBlockTableSize * sizeof(TMPQBlock), MPQ_KEY_BLOCK_TABLE);
        STORM_FREE(pBlock);
    }

    FileStream_Close(pStream);
    return ERROR_SUCCESS;
}
#endif
