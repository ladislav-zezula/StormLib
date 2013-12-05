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

    TLogHelper(const char * szTestName);
    ~TLogHelper();

#if defined(UNICODE) || defined(UNICODE)
    // TCHAR-based functions. They are only needed on UNICODE builds.
    // On ANSI builds is TCHAR = char, so we don't need them at all
    int  PrintWithClreol(const TCHAR * szFormat, va_list argList, bool bPrintPrefix, bool bPrintLastError, bool bPrintEndOfLine);
    int  PrintErrorVa(const TCHAR * szFormat, ...);
    int  PrintError(const TCHAR * szFormat, const TCHAR * szFileName = NULL);
#endif  // defined(UNICODE) || defined(UNICODE)

    // ANSI functions
    int  PrintWithClreol(const char * szFormat, va_list argList, bool bPrintPrefix, bool bPrintLastError, bool bPrintEndOfLine);
    void PrintProgress(const char * szFormat, ...);
    void PrintMessage(const char * szFormat, ...);
    int  PrintErrorVa(const char * szFormat, ...);
    int  PrintError(const char * szFormat, const char * szFileName = NULL);

    protected:

    int  GetConsoleWidth();

    const char * szTestName;                        // Title of the text
    size_t nTextLength;                             // Length of the previous progress message
    bool bMessagePrinted;
};

//-----------------------------------------------------------------------------
// Constructor and destructor

TLogHelper::TLogHelper(const char * szName)
{
    // Fill the test line structure
    szTestName = szName;
    nTextLength = 0;
    bMessagePrinted = false;

    // Show the user that a test is running
    PrintProgress("Running test \"%s\" ...", szTestName);
}

TLogHelper::~TLogHelper()
{
    // If no message has been printed, show "OK"
    if(bMessagePrinted == false)
        PrintMessage("Running test \"%s\" ... OK", szTestName);
}

//-----------------------------------------------------------------------------
// TCHAR-based functions. They are only needed on UNICODE builds.
// On ANSI builds is TCHAR = char, so we don't need them at all

#if defined(UNICODE) || defined(UNICODE)
int TLogHelper::PrintWithClreol(const TCHAR * szFormat, va_list argList, bool bPrintPrefix, bool bPrintLastError, bool bPrintEndOfLine)
{
    TCHAR szOneLineBuff[0x200];
    TCHAR * szSaveBuffer;
    TCHAR * szBuffer = szOneLineBuff;
    int nRemainingWidth;
    int nConsoleWidth = GetConsoleWidth();
    int nLength = 0;
    int nError = GetLastError();

    // Always start the buffer with '\r'
    *szBuffer++ = '\r';
    szSaveBuffer = szBuffer;

    // Print the prefix, if needed
    if(szTestName != NULL && bPrintPrefix)
    {
        while(szTestName[nLength] != 0)
            *szBuffer++ = szTestName[nLength++];
        
        *szBuffer++ = ':';
        *szBuffer++ = ' ';
    }

    // Format the message itself
    if(szFormat != NULL)
    {
        nLength = _vstprintf(szBuffer, szFormat, argList);
        szBuffer += nLength;
    }

    // Print the last error, if needed
    if(bPrintLastError)
    {
        nLength = _stprintf(szBuffer, _T(" (error code: %u)"), nError);
        szBuffer += nLength;
    }

    // Shall we pad the string?
    if((szBuffer - szSaveBuffer) < nConsoleWidth)
    {
        // Calculate the remaining width
        nRemainingWidth = GetConsoleWidth() - (int)(szBuffer - szSaveBuffer) - 1;

        // Pad the string with spaces to fill it up to the end of the line
        for(int i = 0; i < nRemainingWidth; i++)
            *szBuffer++ = 0x20;

        // Pad the buffer with backslashes to fill it up to the end of the line
        for(int i = 0; i < nRemainingWidth; i++)
            *szBuffer++ = 0x08;
    }

    // Put the newline, if requested
    *szBuffer++ = bPrintEndOfLine ? '\n' : 0;
    *szBuffer = 0;

    // Remember if we printed a message
    if(bPrintEndOfLine)
        bMessagePrinted = true;

    // Spit out the text in one single printf
    _tprintf(szOneLineBuff);
    return nError;
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
    char szOneLineBuff[0x200];
    char * szSaveBuffer;
    char * szBuffer = szOneLineBuff;
    int nRemainingWidth;
    int nConsoleWidth = GetConsoleWidth();
    int nLength = 0;
    int nError = GetLastError();

    // Always start the buffer with '\r'
    *szBuffer++ = '\r';
    szSaveBuffer = szBuffer;

    // Print the prefix, if needed
    if(szTestName != NULL && bPrintPrefix)
    {
        while(szTestName[nLength] != 0)
            *szBuffer++ = szTestName[nLength++];
        
        *szBuffer++ = ':';
        *szBuffer++ = ' ';
    }

    // Format the message itself
    if(szFormat != NULL)
    {
        nLength = vsprintf(szBuffer, szFormat, argList);
        szBuffer += nLength;
    }

    // Print the last error, if needed
    if(bPrintLastError)
    {
        nLength = sprintf(szBuffer, " (error code: %u)", nError);
        szBuffer += nLength;
    }

    // Shall we pad the string?
    if((szBuffer - szSaveBuffer) < nConsoleWidth)
    {
        // Calculate the remaining width
        nRemainingWidth = GetConsoleWidth() - (int)(szBuffer - szSaveBuffer) - 1;

        // Pad the string with spaces to fill it up to the end of the line
        for(int i = 0; i < nRemainingWidth; i++)
            *szBuffer++ = 0x20;

        // Pad the buffer with backslashes to fill it up to the end of the line
        for(int i = 0; i < nRemainingWidth; i++)
            *szBuffer++ = 0x08;
    }

    // Put the newline, if requested
    *szBuffer++ = bPrintEndOfLine ? '\n' : 0;
    *szBuffer = 0;

    // Remember if we printed a message
    if(bPrintEndOfLine)
        bMessagePrinted = true;

    // Spit out the text in one single printf
    printf(szOneLineBuff);
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

int TLogHelper::GetConsoleWidth()
{
#ifdef PLATFORM_WINDOWS

    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ScreenInfo);
    return (int)(ScreenInfo.srWindow.Right - ScreenInfo.srWindow.Left);

#else

    // On non-Windows platforms, we assume that width of the console line
    // is 80 characters
    return 80;

#endif
}
