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

int main()
{
    HANDLE hMpq = NULL;
    HANDLE hFile = NULL;

    if(StormOpenArchive("e:\\Multimedia\\MPQs\\1995 - Test MPQs\\MPQ_2015_v1_MessListFile.mpq", 0, 0, &hMpq))
    {                             
        _asm int 3;
        if(StormOpenFileEx(hMpq, "\\\\\\*¹BTNGoblinPyrotechnician.blp", 0, &hFile))
        {
            StormCloseFile(hFile);
        }

        StormCloseArchive(hMpq);
    }

    return 0;
}
