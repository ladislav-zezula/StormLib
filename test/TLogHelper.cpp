/*****************************************************************************/
/* TLogHelper.cpp                         Copyright (c) Ladislav Zezula 2013 */
/*---------------------------------------------------------------------------*/
/* Helper class for reporting StormLib tests                                 */
/* This file should be included directly from Test.cpp using #include        */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 26.11.13  1.00  Lad  The first version of TLogHelper.cpp                  */
/*****************************************************************************/

//-----------------------------------------------------------------------------
// Definition of the TLogHelper class

class TLogHelper
{
    public:

    TLogHelper(const char * szNewMainTitle = NULL, const char * szNewSubTitle = NULL);
    ~TLogHelper();

#if defined(UNICODE) || defined(UNICODE)
    // TCHAR-based functions. They are only needed on UNICODE builds.
    // On ANSI builds is TCHAR = char, so we don't need them at all
    int  PrintWithClreol(const TCHAR * szFormat, va_list argList, bool bPrintPrefix, bool bPrintLastError, bool bPrintEndOfLine);
    void PrintProgress(const TCHAR * szFormat, ...);
    int  PrintErrorVa(const TCHAR * szFormat, ...);
    int  PrintError(const TCHAR * szFormat, const TCHAR * szFileName = NULL);
#endif  // defined(UNICODE) || defined(UNICODE)

    // ANSI functions
    int  PrintWithClreol(const char * szFormat, va_list argList, bool bPrintPrefix, bool bPrintLastError, bool bPrintEndOfLine);
    void PrintProgress(const char * szFormat, ...);
    void PrintMessage(const char * szFormat, ...);
    int  PrintErrorVa(const char * szFormat, ...);
    int  PrintError(const char * szFormat, const char * szFileName = NULL);

    const char * UserString;
    unsigned int UserCount;
    unsigned int UserTotal;
    bool bDontPrintResult;

    protected:

#if defined(UNICODE) || defined(UNICODE)
    TCHAR * CopyFormatCharacter(TCHAR * szBuffer, const TCHAR *& szFormat);
#endif    
    char * CopyFormatCharacter(char * szBuffer, const char *& szFormat);
    int  GetConsoleWidth();

    const char * szMainTitle;                       // Title of the text (usually name)
    const char * szSubTitle;                        // Title of the text (can be name of the tested file)
    size_t nTextLength;                             // Length of the previous progress message
    bool bMessagePrinted;
};

//-----------------------------------------------------------------------------
// String replacements for format strings

#ifdef _MSC_VER
#define I64u_t _T("%I64u")
#define I64u_a "%I64u"
#define I64X_t _T("%I64X")
#define I64X_a "%I64X"
#else
#define I64u_t _T("%llu")
#define I64u_a "%llu"
#define I64X_t _T("%llX")
#define I64X_a "%llX"
#endif

//-----------------------------------------------------------------------------
// Constructor and destructor

TLogHelper::TLogHelper(const char * szNewTestTitle, const char * szNewSubTitle)
{
    UserString = "";
    UserCount = 1;
    UserTotal = 1;

    // Fill the test line structure
    szMainTitle = szNewTestTitle;
    szSubTitle = szNewSubTitle;
    nTextLength = 0;
    bMessagePrinted = false;
    bDontPrintResult = false;

    // Print the initial information
    if(szMainTitle != NULL)
    {
        if(szSubTitle != NULL)
            printf("Running %s (%s) ...", szMainTitle, szSubTitle);
        else
            printf("Running %s ...", szMainTitle);
    }
}

TLogHelper::~TLogHelper()
{
    const char * szSaveMainTitle = szMainTitle;
    const char * szSaveSubTitle = szSubTitle;

    // Set both to NULL so the won't be printed
    szMainTitle = NULL;
    szSubTitle = NULL;

    // Print the final information
    if(szSaveMainTitle != NULL && bMessagePrinted == false)
    {
        if(bDontPrintResult == false)
        {
            if(szSaveSubTitle != NULL)
                PrintMessage("%s (%s) succeeded.", szSaveMainTitle, szSaveSubTitle);
            else
                PrintMessage("%s succeeded.", szSaveMainTitle);
        }
        else
        {
            PrintProgress(" ");
            printf("\r");
        }
    }
}

//-----------------------------------------------------------------------------
// TCHAR-based functions. They are only needed on UNICODE builds.
// On ANSI builds is TCHAR = char, so we don't need them at all

#if defined(UNICODE) || defined(UNICODE)
int TLogHelper::PrintWithClreol(const TCHAR * szFormat, va_list argList, bool bPrintPrefix, bool bPrintLastError, bool bPrintEndOfLine)
{
    TCHAR szFormatBuff[0x200];
    TCHAR szMessage[0x200];
    TCHAR * szBuffer = szFormatBuff;
    int nRemainingWidth;
    int nConsoleWidth = GetConsoleWidth();
    int nLength = 0;
    int nError = GetLastError();

    // Always start the buffer with '\r'
    *szBuffer++ = '\r';

    // Print the prefix, if needed
    if(szMainTitle != NULL && bPrintPrefix)
    {
        while(szMainTitle[nLength] != 0)
            *szBuffer++ = szMainTitle[nLength++];
        
        *szBuffer++ = ':';
        *szBuffer++ = ' ';
    }

    // Copy the message format itself. Replace %s with "%s", unless it's (%s)
    if(szFormat != NULL)
    {
        szBuffer = CopyFormatCharacter(szBuffer, szFormat);
        szFormat += nLength;
    }

    // Append the last error
    if(bPrintLastError)
    {
        nLength = _stprintf(szBuffer, _T(" (error code: %u)"), nError);
        szBuffer += nLength;
    }

    // Create the result string
    szBuffer[0] = 0;
    nLength = _vstprintf(szMessage, szFormatBuff, argList);
    szBuffer = szMessage + nLength;

    // Shall we pad the string?
    if(nLength < nConsoleWidth)
    {
        // Calculate the remaining width
        nRemainingWidth = nConsoleWidth - nLength - 1;

        // Pad the string with spaces to fill it up to the end of the line
        for(int i = 0; i < nRemainingWidth; i++)
            *szBuffer++ = 0x20;
    }

    // Put the newline, if requested
    *szBuffer++ = bPrintEndOfLine ? '\n' : 0;
    *szBuffer = 0;

    // Remember if we printed a message
    if(bPrintEndOfLine)
        bMessagePrinted = true;

    // Spit out the text in one single printf
    _tprintf(_T("%s"), szMessage);
    return nError;
}

void TLogHelper::PrintProgress(const TCHAR * szFormat, ...)
{
    va_list argList;

    va_start(argList, szFormat);
    PrintWithClreol(szFormat, argList, true, false, false);
    va_end(argList);
}

int TLogHelper::PrintErrorVa(const TCHAR * szFormat, ...)
{
    va_list argList;
    int nResult;

    va_start(argList, szFormat);
    nResult = PrintWithClreol(szFormat, argList, true, true, true);
    va_end(argList);

    return nResult;
}

int TLogHelper::PrintError(const TCHAR * szFormat, const TCHAR * szFileName)
{
    return PrintErrorVa(szFormat, szFileName);
}
#endif  // defined(UNICODE) || defined(UNICODE)

//-----------------------------------------------------------------------------
// ANSI functions

int TLogHelper::PrintWithClreol(const char * szFormat, va_list argList, bool bPrintPrefix, bool bPrintLastError, bool bPrintEndOfLine)
{
    char szFormatBuff[0x200];
    char szMessage[0x200];
    char * szBuffer = szFormatBuff;
    int nRemainingWidth;
    int nConsoleWidth = GetConsoleWidth();
    int nLength = 0;
    int nError = GetLastError();

    // Always start the buffer with '\r'
    *szBuffer++ = '\r';

    // Print the prefix, if needed
    if(szMainTitle != NULL && bPrintPrefix)
    {
        while(szMainTitle[nLength] != 0)
            *szBuffer++ = szMainTitle[nLength++];
        
        *szBuffer++ = ':';
        *szBuffer++ = ' ';
    }

    // Copy the message format itself. Replace %s with "%s", unless it's (%s)
    if(szFormat != NULL)
    {
        while(szFormat[0] != 0)
        {
            szBuffer = CopyFormatCharacter(szBuffer, szFormat);
        }
    }

    // Append the last error
    if(bPrintLastError)
    {
        nLength = sprintf(szBuffer, " (error code: %u)", nError);
        szBuffer += nLength;
    }

    // Create the result string
    szBuffer[0] = 0;
    nLength = vsprintf(szMessage, szFormatBuff, argList);

    // Shall we pad the string?
    szBuffer = szMessage + nLength;
    if(nLength < nConsoleWidth)
    {
        // Calculate the remaining width
        nRemainingWidth = nConsoleWidth - nLength - 1;

        // Pad the string with spaces to fill it up to the end of the line
        for(int i = 0; i < nRemainingWidth; i++)
            *szBuffer++ = 0x20;
    }

    // Put the newline, if requested
    *szBuffer++ = bPrintEndOfLine ? '\n' : 0;
    *szBuffer = 0;

    // Remember if we printed a message
    if(bPrintEndOfLine)
        bMessagePrinted = true;

    // Spit out the text in one single printf
    printf("%s", szMessage);
    return nError;
}

void TLogHelper::PrintProgress(const char * szFormat, ...)
{
    va_list argList;

    va_start(argList, szFormat);
    PrintWithClreol(szFormat, argList, true, false, false);
    va_end(argList);
}

void TLogHelper::PrintMessage(const char * szFormat, ...)
{
    va_list argList;

    va_start(argList, szFormat);
    PrintWithClreol(szFormat, argList, true, false, true);
    va_end(argList);
}

int TLogHelper::PrintErrorVa(const char * szFormat, ...)
{
    va_list argList;
    int nResult;

    va_start(argList, szFormat);
    nResult = PrintWithClreol(szFormat, argList, true, true, true);
    va_end(argList);

    return nResult;
}

int TLogHelper::PrintError(const char * szFormat, const char * szFileName)
{
    return PrintErrorVa(szFormat, szFileName);
}

//-----------------------------------------------------------------------------
// Protected functions

#ifdef _UNICODE
TCHAR * TLogHelper::CopyFormatCharacter(TCHAR * szBuffer, const TCHAR *& szFormat)
{
    static const TCHAR * szStringFormat = _T("\"%s\"");
    static const TCHAR * szUint64Format = I64u_t;

    // String format
    if(szFormat[0] == '%')
    {
        if(szFormat[1] == 's' && szFormat[2] != ')')
        {
            _tcscpy(szBuffer, szStringFormat);
            szFormat += 2;
            return szBuffer + _tcslen(szStringFormat);
        }

        // Replace %I64u with the proper platform-dependent suffix
        if(szFormat[1] == 'I' && szFormat[2] == '6' && szFormat[3] == '4' && szFormat[4] == 'u')
        {
            _tcscpy(szBuffer, szUint64Format);
            szFormat += 5;
            return szBuffer + _tcslen(szUint64Format);
        }
    }

    // Copy the character as-is
    *szBuffer++ = *szFormat++;
    return szBuffer;
}
#endif

char * TLogHelper::CopyFormatCharacter(char * szBuffer, const char *& szFormat)
{
    static const char * szStringFormat = "\"%s\"";
    static const char * szUint64Format = I64u_a;

    // String format
    if(szFormat[0] == '%')
    {
        if(szFormat[1] == 's' && szFormat[2] != ')')
        {
            strcpy(szBuffer, szStringFormat);
            szFormat += 2;
            return szBuffer + strlen(szStringFormat);
        }

        // Replace %I64u with the proper platform-dependent suffix
        if(szFormat[1] == 'I' && szFormat[2] == '6' && szFormat[3] == '4' && szFormat[4] == 'u')
        {
            strcpy(szBuffer, szUint64Format);
            szFormat += 5;
            return szBuffer + strlen(szUint64Format);
        }
    }

    // Copy the character as-is
    *szBuffer++ = *szFormat++;
    return szBuffer;
}

int TLogHelper::GetConsoleWidth()
{
#ifdef PLATFORM_WINDOWS

    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ScreenInfo);
    return (int)(ScreenInfo.srWindow.Right - ScreenInfo.srWindow.Left);

#else

    // On non-Windows platforms, we assume that width of the console line
    // is 80 characters
    return 120;

#endif
}
