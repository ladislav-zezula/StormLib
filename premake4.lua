
solution 'StormLib'
    location 'build'
    language 'C++'
    platforms { "x32", "x64" }

    targetdir 'bin'

    configurations {
        'DebugAnsiDynamic',
        'ReleaseAnsiDynamic',
        'DebugAnsiStatic',
        'ReleaseAnsiStatic',

        'DebugUnicodeDynamic',
        'ReleaseUnicodeDynamic',
        'DebugUnicodeStatic',
        'ReleaseUnicodeStatic',
    }

--------------------------------------------------------------------------------

    configuration 'DebugAnsiDynamic'
        targetsuffix 'DAD'
    configuration 'ReleaseAnsiDynamic'
        targetsuffix 'RAD'
    configuration 'DebugAnsiStatic'
        targetsuffix 'DAS'
    configuration 'ReleaseAnsiStatic'
        targetsuffix 'RAS'

    configuration 'DebugUnicodeDynamic'
        targetsuffix 'DUD'
    configuration 'ReleaseUnicodeDynamic'
        targetsuffix 'RUD'
    configuration 'DebugUnicodeStatic'
        targetsuffix 'DUS'
    configuration 'ReleaseUnicodeStatic'
        targetsuffix 'RUS'

    configuration 'Debug*'
        flags { 'Symbols' }
        defines { '_DEBUG' }

    configuration 'Release*'
        flags { 'OptimizeSpeed' }
        defines { 'NDEBUG' }

    configuration { '*Unicode*', 'vs*' }
        flags { 'Unicode' }

    configuration { '*Unicode*', 'not vs*' }
        defines { 'UNICODE', '_UNICODE' }

    configuration '*Static'
        flags { 'StaticRuntime' }

    configuration 'windows'
        defines { 'WIN32', '_WINDOWS' }

    configuration 'linux'
        defines { '_7ZIP_ST', 'BZ_STRICT_ANSI' }
        excludes {
            'src/lzma/C/LzFindMt.*',
            'src/lzma/C/Threads.*',
        }
--------------------------------------------------------------------------------

project 'ThirdLib'
    kind 'StaticLib'

    files {
        'src/*/**.cpp',
        'src/*/**.c',
    }

    excludes {
        'src/adpcm/*_old.*',
        'src/huffman/*_old.*',
        'src/pklib/crc32.c',
        'src/zlib/compress.c',
    }

--------------------------------------------------------------------------------

project 'StormLib'
    kind 'StaticLib'

    files {
        'src/*.h',
        'src/*.cpp',
    }

    links {
        'ThirdLib',
    }

    configuration "vs*"
        pchheader 'StormPrehead.h'
        pchsource 'src/StormPrehead.cpp'

    configuration "not vs*"
        includedirs { "src" }
        pchheader 'StormPrehead.h'

project 'StormLib_dll'
    kind 'SharedLib'

    files {
        'src/*.h',
        'src/*.cpp',
        'stormlib_dll/StormLib.def',
    }

    links {
        'ThirdLib',
    }

    configuration 'windows'
        links {
            'Wininet',
        }

    configuration "vs*"
        pchheader 'StormPrehead.h'
        pchsource 'src/StormPrehead.cpp'

    configuration "not vs*"
        includedirs { "src" }
        pchheader 'StormPrehead.h'

project 'StormLib_Test'
    kind 'ConsoleApp'
    defines { '__STORMLIB_TEST__' }

    files {
        'test/StormTest.cpp',
    }

    links {
        'StormLib',
    }

    configuration 'windows'
        links {
            'Wininet',
        }

