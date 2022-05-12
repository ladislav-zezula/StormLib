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
#include <windows.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#define STORM_ALTERNATE_NAMES               // Use Storm* prefix for functions
#include "storm.h"                          // Header file for Storm.dll

//-----------------------------------------------------------------------------
// List of files

const char * IntToHexChar = "0123456789ABCDEF";

LPCSTR ListFile[] = 
{
    "music\\tdefeat.wav",
    "setupdat\\inst.vis",
    "setupdat\\audio\\mouseover.wav",
    "files\\font\\font12.fnt",
    "music\\zvict.wav",
    "files\\readme.txt",
    "music\\zerg2.wav",
    "music\\zerg1.wav",
    "setupdat\\inst_regs.ins",
    "files\\font\\font50.fnt",
    "files\\msv2all.vxp",
    "setupdat\\scr_options.vis",
    "setupdat\\font\\font16x.fnt",
    "setupdat\\setup.vis",
    "maps\\128x128_wasteland4.scm",
    "setupdat\\scr_main.vis",
    "music\\pvict.wav",
    "music\\zdefeat.wav",
    "files\\font\\font16x.fnt",
    "setupdat\\nt\\comctl32.dll",
    "setupdat\\debug.ins",
    "setupdat\\inst.ins",
    "music\\pdefeat.wav",
    "files\\smackw32.dll",
    "music\\tvict.wav",
    "setupdat\\optvox.vis",
    "setupdat\\gen\\maps.lst",
    "files\\font\\font8.fnt",
    "files\\mvoice.vxp",
    "files\\font\\font14.fnt",
    "files\\battle.snp",
    "files\\storm.dll",
    "maps\\128x128_ash4.scm",
    "files\\font\\font32.fnt",
    "setupdat\\normal.ins",
    "files\\font\\font16.fnt",
    "setupdat\\mainplay.vis",
    "setupdat\\gendefs.ins",
    "setupdat\\audio\\installermusic.wav",
    "setupdat\\templates.ins",
    "music\\terran1.wav",
    "maps\\96x96_ash4.scm",
    "setupdat\\defaults.vis",
    "music\\prdyroom.wav",
    "setupdat\\scr_blizzard.vis",
    "setupdat\\scr_isp.vis",
    "setupdat\\95\\comctl32.dll",
    "files\\font\\font10.fnt",
    "files\\local.dll",
    "music\\terran3.wav",
    "music\\terran2.wav",
    "setupdat\\audio\\battlenetclick.wav",
    "music\\zrdyroom.wav",
    "setupdat\\starunin.exe",
    "files\\vct32150.dll",
    "maps\\96x96_wasteland4.scm",
    "setupdat\\inst_sys.ins",
    "music\\zerg3.wav",
    "setupdat\\license.txt",
    "music\\protoss2.wav",
    "files\\starcraft.exe",
    "music\\trdyroom.wav",
    "maps\\96x96_space4.scm",
    "files\\vfonts.vxp",
    "setupdat\\installed.ins",
    "music\\protoss1.wav",
    "files\\stardat.mpq",
    "setupdat\\font\\font16.fnt",
    "smk\\starintr.smk",
    "files\\vadagc.vxp",
    "setupdat\\strings.ins",
    "music\\title.wav",
    "setupdat\\font\\font32.fnt",
    "setupdat\\inst_files.ins",
    "setupdat\\images\\install.pcx",
    "files\\report bugs.url",
    "setupdat\\single.ins",
    "maps\\128x128_space4.scm",
    "setupdat\\audio\\mousedown.wav",
    NULL
};

//-----------------------------------------------------------------------------
// Main

static void CalculateMD5(LPBYTE md5_digest, LPBYTE pbData, DWORD cbData)
{
    HCRYPTPROV hCryptProv = NULL;
    HCRYPTHASH hCryptHash = NULL;

    if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if(CryptCreateHash(hCryptProv, CALG_MD5, NULL, 0, &hCryptHash))
        {
            DWORD dwHashLen = 0x10;

            CryptHashData(hCryptHash, pbData, cbData, 0);
            CryptGetHashParam(hCryptHash, HP_HASHVAL, md5_digest, &dwHashLen, 0);
            CryptDestroyHash(hCryptHash);
        }

        CryptReleaseContext(hCryptProv, 0);
    }
}

template <typename XCHAR>
DWORD BinaryToString(XCHAR * szBuffer, size_t cchBuffer, LPCVOID pvBinary, size_t cbBinary)
{
    LPCBYTE pbBinary = (LPCBYTE)pvBinary;

    // The size of the string must be enough to hold the binary + EOS
    if(cchBuffer < ((cbBinary * 2) + 1))
        return ERROR_INSUFFICIENT_BUFFER;

    // Convert the string to the array of MD5
    // Copy the blob data as text
    for(size_t i = 0; i < cbBinary; i++)
    {
        *szBuffer++ = IntToHexChar[pbBinary[0] >> 0x04];
        *szBuffer++ = IntToHexChar[pbBinary[0] & 0x0F];
        pbBinary++;
    }

    // Terminate the string
    *szBuffer = 0;
    return ERROR_SUCCESS;
}


int main(int argc, char * argv[])
{
    LPCSTR szArchiveName;
    LPCSTR szFileName;
    LPCSTR szFormat;
    HANDLE hMpq = NULL;
    HANDLE hFile = NULL;
    LPBYTE pbBuffer = NULL;
    DWORD dwBytesRead = 0;
    DWORD dwFileSize = 0;
    BOOL bResult;
    BYTE md5_digest[0x10];
    char md5_string[0x40];

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

    // Break for kernel debugger
    //__debugbreak();

    // Put Storm.dll to the current folder before running this
    //printf("[*] Opening archive '%s' ...\n", szArchiveName);
    if(StormOpenArchive(szArchiveName, 0, 0, &hMpq))
    {
        for(size_t i = 0; ListFile[i] != NULL; i++)
        {
            //printf("[*] Opening file '%s' ...\n", ListFile[i]);
            if(StormOpenFileEx(hMpq, ListFile[i], 0, &hFile))
            {
                //printf("[*] Retrieving file size ... ");
                dwFileSize = StormGetFileSize(hFile, NULL);
                szFormat = (dwFileSize == INVALID_FILE_SIZE) ? ("(invalid)\n") : ("(%u bytes)\n");
                //printf(szFormat, dwFileSize);

                // Allocate the buffer for the entire file
                //printf("[*] Allocating buffer ...\n");
                if((pbBuffer = new BYTE[dwFileSize]) != NULL)
                {
                    //printf("[*] Moving to begin of the file ...\n");
                    StormSetFilePointer(hFile, 0, NULL, FILE_BEGIN);

                    //printf("[*] Reading file ... ");
                    bResult = StormReadFile(hFile, pbBuffer, dwFileSize, &dwBytesRead, NULL);
                    szFormat = (bResult != FALSE && dwBytesRead == dwFileSize) ? ("(OK)\n") : ("(error %u)\n");
                    //printf(szFormat, GetLastError());

                    //printf("[*] Calculating MD5 ... ");
                    CalculateMD5(md5_digest, pbBuffer, dwFileSize);
                    BinaryToString(md5_string, _countof(md5_string), md5_digest, sizeof(md5_digest));
                    printf("%s *%s\n", md5_string, ListFile[i]);

                    delete[] pbBuffer;
                }

                //printf("[*] Closing file ...\n");
                StormCloseFile(hFile);
            }
        }

        //printf("[*] Closing archive ...\n");
        StormCloseArchive(hMpq);
    }

    //printf("Done.\n\n");
    return 0;
}
