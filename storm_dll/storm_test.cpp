/*****************************************************************************/
/* Storm_test.cpp                         Copyright (c) Ladislav Zezula 2014 */
/*---------------------------------------------------------------------------*/
/* Test module for storm.dll (original Blizzard MPQ dynalic library          */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 24.08.14  1.00  Lad  The first version of Storm_test.cpp                  */
/*****************************************************************************/

#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#define STORM_ALTERNATE_NAMES               // Use Storm* prefix for functions
#include "storm.h"                          // Header file for Storm.dll

//-----------------------------------------------------------------------------
// Main

int main(int argc, char * argv[])
{
    LPCSTR szArchiveName;
    LPCSTR szFileName;
    LPCSTR szFormat;
    HANDLE hMpq = NULL;
    HANDLE hFile = NULL;
    BYTE Buffer[0x100];
    DWORD dwBytesRead = 0;
    DWORD dwFileSize = 0;
    BOOL bResult;

    // Check parameters
    if(argc != 3)
    {
        printf("Error: Missing MPQ or file name\n");
        return 3;
    }

    // Get both arguments
    SetLastError(ERROR_SUCCESS);
    szArchiveName = argv[1];
    szFileName = argv[2];

    // Put Storm.dll to the current folder before running this
    printf("[*] Opening archive '%s' ...\n", szArchiveName);
    if(StormOpenArchive(szArchiveName, 0, 0, &hMpq))
    {
        printf("[*] Opening file '%s' ...\n", szFileName);
        if(StormOpenFileEx(hMpq, "staredit\\scenario.chk", 0, &hFile))
        {
            printf("[*] Retrieving file size ... ");
            dwFileSize = StormGetFileSize(hFile, NULL);
            szFormat = (dwFileSize == INVALID_FILE_SIZE) ? ("(invalid)\n") : ("(%u bytes)\n");
            printf(szFormat, dwFileSize);

            printf("[*] Moving to begin of the file ...\n");
            StormSetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            
            printf("[*] Reading file ... ");
            bResult = StormReadFile(hFile, Buffer, sizeof(Buffer), &dwBytesRead, NULL);
            szFormat = (bResult == FALSE) ? ("(error %u)\n") : ("(OK)\n");
            printf(szFormat, GetLastError());
            
            printf("[*] Closing file ...\n");
            StormCloseFile(hFile);
        }

        printf("[*] Closing archive ...\n");
        StormCloseArchive(hMpq);
    }

    printf("Done.\n\n");
    return 0;
}
