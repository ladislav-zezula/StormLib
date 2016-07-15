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

#define STORM_ALTERNATE_NAMES
#include "storm_dll.h"                      // Header file for Storm.dll

//-----------------------------------------------------------------------------
// Main

unsigned char szKoreanFileName[] = {0x77, 0x61, 0x72, 0x33, 0x6D, 0x61, 0x70, 0x49, 0x6D, 0x70, 0x6F, 0x72, 0x74, 0x65, 0x64, 0x5C, 0xBF, 0xD5, 0xB1, 0xB9, 0x2E, 0x6D, 0x70, 0x33, 0x00};

int main()
{
    LPCSTR szArchiveName = "e:\\MPQ_2016_v1_KoreanFile.w3m";
    HANDLE hMpq = NULL;
    HANDLE hFile = NULL;
    char szFileName[MAX_PATH];

    _asm int 3;

    if(StormOpenArchive(szArchiveName, 0, 0, &hMpq))
    {                             
        memcpy(szFileName, szKoreanFileName, _countof(szKoreanFileName));
        if(StormOpenFileEx(hMpq, szFileName, 0, &hFile))
        {
            StormCloseFile(hFile);
        }

        StormCloseArchive(hMpq);
    }

    return 0;
}
