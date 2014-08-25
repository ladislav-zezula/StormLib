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

    _asm int 3;
    if(StormOpenArchive("E:\\Multimedia\\MPQs\\1995 - Test MPQs\\MPQ_2014_v1_ProtectedMap_Spazzler3.w3x", 0, 0, &hMpq))
    {
        StormCloseArchive(hMpq);
    }

    return 0;
}
