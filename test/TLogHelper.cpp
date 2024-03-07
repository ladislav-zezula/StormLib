/*****************************************************************************/
/* TLogHelper.cpp                         Copyright (c) Ladislav Zezula 2013 */
/*---------------------------------------------------------------------------*/
/* Helper class for reporting StormLib tests                                 */
/* This file should be included directly from StormTest.cpp using #include   */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 26.11.13  1.00  Lad  Created                                              */
/*****************************************************************************/

//-----------------------------------------------------------------------------
// String replacements for format strings

#if defined(STORMLIB_WINDOWS) || defined(CASCLIB_PLATFORM_WINDOWS)
#define TEST_PLATFORM_WINDOWS
#endif

#ifdef _MSC_VER
#define fmt_I64u_t _T("%I64u")
#define fmt_I64u_a "%I64u"
#define fmt_I64X_t _T("%I64X")
#define fmt_I64X_a "%I64X"
#else
#define fmt_I64u_t _T("%llu")
#define fmt_I64u_a "%llu"
#define fmt_I64X_t _T("%llX")
#define fmt_I64X_a "%llX"
#endif

#define fmt_X_of_Y_a  "(" fmt_I64u_a " of " fmt_I64u_a ")"

#ifdef __CASCLIB_SELF__
#define TEST_MIN CASCLIB_MIN
#define TEST_PRINT_PREFIX   false
#else
#define TEST_MIN STORMLIB_MIN
#define TEST_PRINT_PREFIX   true
#endif

//-----------------------------------------------------------------------------
// Local functions

inline DWORD TestInterlockedIncrement(DWORD * PtrValue)
{
#ifdef TEST_PLATFORM_WINDOWS
    return (DWORD)InterlockedIncrement((LONG *)(PtrValue));
#elif defined(__GNUC__)
    return __sync_add_and_fetch(PtrValue, 1);
#else
    return ++(*PtrValue);
#endif
}

inline DWORD Test_GetLastError()
{
#if defined(CASCLIB_PLATFORM_WINDOWS)
    return GetCascError();
#else
    return GetLastError();
#endif
}

#ifdef STORMLIB_WINDOWS
wchar_t * CopyFormatCharacter(wchar_t * szBuffer, const wchar_t *& szFormat)
{
    static const wchar_t * szStringFormat = _T("%s");
    static const wchar_t * szUint64Format = fmt_I64u_t;

    // String format
    if(szFormat[0] == '%')
    {
        if(szFormat[1] == 's')
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
#endif  // STORMLIB_WINDOWS

char * CopyFormatCharacter(char * szBuffer, const char *& szFormat)
{
    static const char * szStringFormat = "\"%s\"";
    static const char * szUint64Format = fmt_I64u_a;

    // String format
    if(szFormat[0] == '%')
    {
        if(szFormat[1] == 's')
        {
            StringCopy(szBuffer, 32, szStringFormat);
            szFormat += 2;
            return szBuffer + strlen(szStringFormat);
        }

        // Replace %I64u with the proper platform-dependent suffix
        if(szFormat[1] == 'I' && szFormat[2] == '6' && szFormat[3] == '4' && szFormat[4] == 'u')
        {
            StringCopy(szBuffer, 32, szUint64Format);
            szFormat += 5;
            return szBuffer + strlen(szUint64Format);
        }
    }

    // Copy the character as-is
    *szBuffer++ = *szFormat++;
    return szBuffer;
}

size_t TestStrPrintfV(char * buffer, size_t nCount, const char * format, va_list argList)
{
    return vsnprintf(buffer, nCount, format, argList);
}

size_t TestStrPrintf(char * buffer, size_t nCount, const char * format, ...)
{
    va_list argList;
    size_t length;

    // Start the argument list
    va_start(argList, format);
    length = TestStrPrintfV(buffer, nCount, format, argList);
    va_end(argList);

    return length;
}

size_t TestStrPrintfV(wchar_t * buffer, size_t nCount, const wchar_t * format, va_list argList)
{
#ifdef TEST_PLATFORM_WINDOWS
    return _vsnwprintf(buffer, nCount, format, argList);
#else
    return vswprintf(buffer, nCount, format, argList);
#endif
}

size_t TestStrPrintf(wchar_t * buffer, size_t nCount, const wchar_t * format, ...)
{
    va_list argList;
    size_t length;

    // Start the argument list
    va_start(argList, format);
    length = TestStrPrintfV(buffer, nCount, format, argList);
    va_end(argList);

    return length;
}

//-----------------------------------------------------------------------------
// Definition of the TLogHelper class

class TLogHelper
{
    public:

    //
    //  Constructor and destructor
    //

    TLogHelper(const char * szNewMainTitle = NULL, const TCHAR * szNewSubTitle1 = NULL, const TCHAR * szNewSubTitle2 = NULL)
    {
        // Fill the variables
        memset(this, 0, sizeof(TLogHelper));
        UserString = "";
        TotalFiles = 1;
        UserCount = 1;
        UserTotal = 1;

#ifdef TEST_PLATFORM_WINDOWS
        InitializeCriticalSection(&Locker);
        TickCount = GetTickCount();
#endif

        // Remember the startup time
        SetStartTime();

        // Save all three titles
        szMainTitle = szNewMainTitle;
        szSubTitle1 = szNewSubTitle1;
        szSubTitle2 = szNewSubTitle2;

        // Print the initial information
        if(szNewMainTitle != NULL)
        {
#ifdef __CASCLIB_SELF__
            char szMainTitleT[0x100] = {0};
            size_t nLength;

            nLength = TestStrPrintf(szMainTitleT, _countof(szMainTitleT), "-- \"%s\" --", szNewMainTitle);
            while(nLength < 90)
                szMainTitleT[nLength++] = '-';
            if(nLength < sizeof(szMainTitleT))
                szMainTitleT[nLength++] = 0;

            printf("%s\n", szMainTitleT);
#endif

#ifdef __STORMLIB_SELF__
            TCHAR szMainTitleT[0x100] = {0};

            // Copy the UNICODE main title
            StringCopy(szMainTitleT, _countof(szMainTitleT), szMainTitle);

            if(szSubTitle1 != NULL && szSubTitle2 != NULL)
                nPrevPrinted = _tprintf(_T("\rRunning %s (%s+%s) ..."), szMainTitleT, szSubTitle1, szSubTitle2);
            else if(szSubTitle1 != NULL)
                nPrevPrinted = _tprintf(_T("\rRunning %s (%s) ..."), szMainTitleT, szSubTitle1);
            else
                nPrevPrinted = _tprintf(_T("\rRunning %s ..."), szMainTitleT);
#endif
        }
    }

    ~TLogHelper()
    {
        // Print a verdict, if no verdict was printed yet
        if(bMessagePrinted == false)
        {
            PrintVerdict(ERROR_SUCCESS);
        }

#ifdef TEST_PLATFORM_WINDOWS
        DeleteCriticalSection(&Locker);
#endif

#ifdef __CASCLIB_SELF__
        printf("\n");
#endif
    }

    //
    // Measurement of elapsed time
    //

    bool TimeElapsed(DWORD Milliseconds)
    {
        bool bTimeElapsed = false;

#ifdef TEST_PLATFORM_WINDOWS
        if(GetTickCount() > (TickCount + Milliseconds))
        {
            TickCount = GetTickCount();
            if(TestInterlockedIncrement(&TimeTrigger) == 1)
            {
                bTimeElapsed = true;
            }
        }

#endif
        return bTimeElapsed;
    }

    //
    //  Printing functions
    //

    template <typename XCHAR>
    DWORD PrintWithClreol(const XCHAR * szFormat, va_list argList, bool bPrintLastError, bool bPrintEndOfLine)
    {
        char * szBufferPtr;
        char * szBufferEnd;
        size_t nNewPrinted;
        size_t nLength = 0;
        DWORD dwErrCode = Test_GetLastError();
        XCHAR szMessage[0x200];
        char szBuffer[0x200];
        bool bPrintPrefix = TEST_PRINT_PREFIX;

        // Always start the buffer with '\r'
        szBufferEnd = szBuffer + _countof(szBuffer);
        szBufferPtr = szBuffer;
        *szBufferPtr++ = '\r';

        // Print the prefix, if needed
        if(szMainTitle != NULL && bPrintPrefix)
        {
            while(szMainTitle[nLength] != 0)
                *szBufferPtr++ = szMainTitle[nLength++];

            *szBufferPtr++ = ':';
            *szBufferPtr++ = ' ';
        }

        // Construct the message
        TestStrPrintfV(szMessage, _countof(szMessage), szFormat, argList);
        StringCopy(szBufferPtr, (szBufferEnd - szBufferPtr), szMessage);
        szBufferPtr = szBufferPtr + strlen(szBufferPtr);

        // Append the last error
        if(bPrintLastError)
        {
            nLength = TestStrPrintf(szBufferPtr, (szBufferEnd - szBufferPtr), " (error code: %u)", dwErrCode);
            szBufferPtr += nLength;
        }

        // Remember how much did we print
        nNewPrinted = (szBufferPtr - szBuffer);

        // Shall we pad the string?
        if((nLength = (szBufferPtr - szBuffer - 1)) < nPrevPrinted)
        {
            size_t nPadding = nPrevPrinted - nLength;

            if((size_t)(nLength + nPadding) > (size_t)(szBufferEnd - szBufferPtr))
                nPadding = (szBufferEnd - szBufferPtr);

            memset(szBufferPtr, ' ', nPadding);
            szBufferPtr += nPadding;
        }

        // Shall we add new line?
        if((bPrintEndOfLine != false) && (szBufferPtr < szBufferEnd))
            *szBufferPtr++ = '\n';
        *szBufferPtr = 0;

        // Remember if we printed a message
        if(bPrintEndOfLine != false)
        {
            bMessagePrinted = true;
            nPrevPrinted = 0;
        }
        else
        {
            nPrevPrinted = nNewPrinted;
        }

        // Finally print the message
        printf("%s", szBuffer);
        nMessageCounter++;
        return dwErrCode;
    }

    template <typename XCHAR>
    void PrintProgress(const XCHAR * szFormat, ...)
    {
        va_list argList;

        // Always reset the time trigger
        TimeTrigger = 0;

        // Only print progress when the cooldown is ready
        if(ProgressReady())
        {
            va_start(argList, szFormat);
            PrintWithClreol(szFormat, argList, false, false);
            va_end(argList);
        }
    }

    template <typename XCHAR>
    void PrintMessage(const XCHAR * szFormat, ...)
    {
        va_list argList;

        va_start(argList, szFormat);
        PrintWithClreol(szFormat, argList, false, true);
        va_end(argList);
    }

    void PrintTotalTime()
    {
        DWORD TotalTime = SetEndTime();

        if(TotalTime != 0)
            PrintMessage("TotalTime: %u.%u second(s)", (TotalTime / 1000), (TotalTime % 1000));
        PrintMessage("Work complete.");
    }

    template <typename XCHAR>
    int PrintErrorVa(const XCHAR * szFormat, ...)
    {
        va_list argList;
        int nResult;

        va_start(argList, szFormat);
        nResult = PrintWithClreol(szFormat, argList, true, true);
        va_end(argList);

        return nResult;
    }

    template <typename XCHAR>
    int PrintError(const XCHAR * szFormat, const XCHAR * szFileName = NULL)
    {
        return PrintErrorVa(szFormat, szFileName);
    }

    // Print final verdict
    DWORD PrintVerdict(DWORD dwErrCode = ERROR_SUCCESS)
    {
        LPCTSTR szSaveSubTitle1 = szSubTitle1;
        LPCTSTR szSaveSubTitle2 = szSubTitle2;
        TCHAR szSaveMainTitle[0x80];

        // Set both to NULL so they won't be printed
        StringCopy(szSaveMainTitle, _countof(szSaveMainTitle), szMainTitle);
        szMainTitle = NULL;
        szSubTitle1 = NULL;
        szSubTitle2 = NULL;

        // Print the final information
        if(szSaveMainTitle[0] != 0)
        {
            if(DontPrintResult == false)
            {
                const TCHAR * szVerdict = (dwErrCode == ERROR_SUCCESS) ? _T("succeeded") : _T("failed");

                if(szSaveSubTitle1 != NULL && szSaveSubTitle2 != NULL)
                    PrintMessage(_T("%s (%s+%s) %s."), szSaveMainTitle, szSaveSubTitle1, szSaveSubTitle2, szVerdict);
                else if(szSaveSubTitle1 != NULL)
                    PrintMessage(_T("%s (%s) %s."), szSaveMainTitle, szSaveSubTitle1, szVerdict);
                else
                    PrintMessage(_T("%s %s."), szSaveMainTitle, szVerdict);
            }
            else
            {
                PrintProgress(" ");
                printf("\r");
            }
        }

        // Return the error code so the caller can pass it fuhrter
        return dwErrCode;
    }

    //
    // Locking functions (Windows only)
    //

    void Lock()
    {
#ifdef TEST_PLATFORM_WINDOWS
        EnterCriticalSection(&Locker);
#endif
    }

    void Unlock()
    {
#ifdef TEST_PLATFORM_WINDOWS
        LeaveCriticalSection(&Locker);
#endif
    }

    //
    //  Time functions
    //

    ULONGLONG GetCurrentThreadTime()
    {
#ifdef _WIN32
        ULONGLONG TempTime = 0;

        GetSystemTimeAsFileTime((LPFILETIME)(&TempTime));
        return ((TempTime) / 10 / 1000);

        //ULONGLONG KernelTime = 0;
        //ULONGLONG UserTime = 0;
        //ULONGLONG TempTime = 0;

        //GetThreadTimes(GetCurrentThread(), (LPFILETIME)&TempTime, (LPFILETIME)&TempTime, (LPFILETIME)&KernelTime, (LPFILETIME)&UserTime);
        //return ((KernelTime + UserTime) / 10 / 1000);
#else
        return time(NULL) * 1000;
#endif
    }

    bool ProgressReady()
    {
        time_t dwTickCount = time(NULL);
        bool bResult = false;

        if(dwTickCount > dwPrevTickCount)
        {
            dwPrevTickCount = dwTickCount;
            bResult = true;
        }

        return bResult;
    }

    ULONGLONG SetStartTime()
    {
        StartTime = GetCurrentThreadTime();
        return StartTime;
    }

    DWORD SetEndTime()
    {
        EndTime = GetCurrentThreadTime();
        return (DWORD)(EndTime - StartTime);
    }

    void IncrementTotalBytes(ULONGLONG IncrementValue)
    {
        // For some weird reason, this is measurably faster then InterlockedAdd64
        Lock();
        TotalBytes = TotalBytes + IncrementValue;
        Unlock();
    }

    void FormatTotalBytes(char * szBuffer, size_t ccBuffer)
    {
        ULONGLONG Bytes = TotalBytes;
        ULONGLONG Divider = 1000000000;
        char * szBufferEnd = szBuffer + ccBuffer;
        bool bDividingOn = false;

        while((szBuffer + 4) < szBufferEnd && Divider > 0)
        {
            // Are we already dividing?
            if(bDividingOn)
            {
                szBuffer += TestStrPrintf(szBuffer, ccBuffer, " %03u", (DWORD)(Bytes / Divider));
                Bytes = Bytes % Divider;
            }
            else if(Bytes > Divider)
            {
                szBuffer += TestStrPrintf(szBuffer, ccBuffer, "%u", (DWORD)(Bytes / Divider));
                Bytes = Bytes % Divider;
                bDividingOn = true;
            }
            Divider /= 1000;
        }
    }

#ifdef TEST_PLATFORM_WINDOWS
    CRITICAL_SECTION Locker;
#endif

    ULONGLONG TotalBytes;                           // For user's convenience: Total number of bytes
    ULONGLONG ByteCount;                            // For user's convenience: Current number of bytes
    ULONGLONG StartTime;                            // Start time of an operation, in milliseconds
    ULONGLONG EndTime;                              // End time of an operation, in milliseconds
    const char * UserString;
    DWORD UserCount;
    DWORD UserTotal;
    DWORD TickCount;
    DWORD TimeTrigger;                              // For triggering elapsed timers
    DWORD TotalFiles;                               // For user's convenience: Total number of files
    DWORD FileCount;                                // For user's convenience: Curernt number of files
    DWORD DontPrintResult:1;                        // If true, supress printing result from the destructor

    protected:

    const char  * szMainTitle;                      // Title of the text (usually name)
    const TCHAR * szSubTitle1;                      // Title of the text (can be name of the tested file)
    const TCHAR * szSubTitle2;                      // Title of the text (can be name of the tested file)
    size_t nMessageCounter;
    size_t nPrevPrinted;                            // Length of the previously printed message
    time_t dwPrevTickCount;
    bool bMessagePrinted;
};
