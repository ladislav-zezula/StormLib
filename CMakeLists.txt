project(StormLib)
cmake_minimum_required(VERSION 3.10)

set(LIBRARY_NAME storm)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakeDependentOption)

option(BUILD_SHARED_LIBS "Compile shared libraries" OFF)
option(STORM_SKIP_INSTALL "Skip installing files" OFF)
option(STORM_USE_BUNDLED_LIBRARIES
    "Force use of bundled dependencies instead of system libraries."
    OFF
)
option(STORM_BUILD_TESTS
    "Compile StormLib test application" OFF
#   "BUILD_TESTING" OFF # Stay coherent with CTest variables
)

set(SRC_FILES
           src/adpcm/adpcm.cpp
           src/huffman/huff.cpp
           src/jenkins/lookup3.c
           src/lzma/C/LzFind.c
           src/lzma/C/LzmaDec.c
           src/lzma/C/LzmaEnc.c
           src/pklib/explode.c
           src/pklib/implode.c
           src/sparse/sparse.cpp
           src/FileStream.cpp
           src/SBaseCommon.cpp
           src/SBaseDumpData.cpp
           src/SBaseFileTable.cpp
           src/SBaseSubTypes.cpp
           src/SCompression.cpp
           src/SFileAddFile.cpp
           src/SFileAttributes.cpp
           src/SFileCompactArchive.cpp
           src/SFileCreateArchive.cpp
           src/SFileExtractFile.cpp
           src/SFileFindFile.cpp
           src/SFileGetFileInfo.cpp
           src/SFileListFile.cpp
           src/SFileOpenArchive.cpp
           src/SFileOpenFileEx.cpp
           src/SFilePatchArchives.cpp
           src/SFileReadFile.cpp
           src/SFileVerify.cpp
           src/libtomcrypt/src/pk/rsa/rsa_verify_simple.c
           src/libtomcrypt/src/misc/crypt_libc.c
)

if(MSVC)
    # This file is used to create a DLL on windows
    # Use BUILD_SHARED_LIBS to create StormLib.dll
    set(STORM_DEF_FILES
           src/DllMain.def
    )
endif()

set(TOMCRYPT_FILES
           src/LibTomCrypt.c
)

set(TOMMATH_FILES
            src/LibTomMath.c
            src/LibTomMathDesc.c
)

set(BZIP2_FILES
           src/bzip2/blocksort.c
           src/bzip2/bzlib.c
           src/bzip2/compress.c
           src/bzip2/crctable.c
           src/bzip2/decompress.c
           src/bzip2/huffman.c
           src/bzip2/randtable.c
)

set(ZLIB_FILES
           src/zlib/adler32.c
           src/zlib/compress.c
           src/zlib/crc32.c
           src/zlib/deflate.c
           src/zlib/inffast.c
           src/zlib/inflate.c
           src/zlib/inftrees.c
           src/zlib/trees.c
           src/zlib/zutil.c
)

set(TEST_SRC_FILES
           test/StormTest.cpp
)

add_definitions(-D_7ZIP_ST -DBZ_STRICT_ANSI)
set(LINK_LIBS)

find_package(ZLIB)
if (ZLIB_FOUND AND NOT STORM_USE_BUNDLED_LIBRARIES)
	set(LINK_LIBS ${LINK_LIBS} ZLIB::ZLIB)
    add_definitions(-D__SYS_ZLIB)
else()
	set(SRC_FILES ${SRC_FILES} ${ZLIB_FILES})
endif()

find_package(BZip2)
if (BZIP2_FOUND AND NOT STORM_USE_BUNDLED_LIBRARIES)
	set(LINK_LIBS ${LINK_LIBS} BZip2::BZip2)
    add_definitions(-D__SYS_BZLIB)
else()
	set(SRC_FILES ${SRC_FILES} ${BZIP2_FILES})
endif()

if(WIN32)
    set(SRC_ADDITIONAL_FILES ${TOMCRYPT_FILES} ${TOMMATH_FILES})
	set(LINK_LIBS ${LINK_LIBS} wininet)
else()
    option(WITH_LIBTOMCRYPT "Use system LibTomCrypt library" OFF)
    if(WITH_LIBTOMCRYPT)
        include(FindPkgConfig)
        pkg_check_modules(PC_LIBTOMCRYPT libtomcrypt REQUIRED)
        find_path(LIBTOMCRYPT_INCLUDE_DIR NAMES tomcrypt.h HINTS ${PC_LIBTOMCRYPT_INCLUDE_DIRS} REQUIRED)
        find_library(LIBTOMCRYPT_LIBRARY NAMES tomcrypt HINTS ${PC_LIBTOMCRYPT_LIBRARY_DIRS} REQUIRED)
        set(LINK_LIBS ${LINK_LIBS} ${LIBTOMCRYPT_LIBRARY})
        include_directories(${LIBTOMCRYPT_INCLUDE_DIR})
    else()
        set(SRC_ADDITIONAL_FILES ${TOMCRYPT_FILES} ${TOMMATH_FILES})
    endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "(Free|Net|Open)BSD")
    message(STATUS "Using BSD port")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DO_LARGEFILE=0 -Dstat64=stat -Dlstat64=lstat -Dlseek64=lseek -Doff64_t=off_t -Dfstat64=fstat -Dftruncate64=ftruncate")
endif()

add_library(${LIBRARY_NAME} ${LIB_TYPE} ${SRC_FILES} ${SRC_ADDITIONAL_FILES} ${STORM_DEF_FILES})
if(WIN32)
    set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "StormLib")
endif()

target_link_libraries(${LIBRARY_NAME} ${LINK_LIBS})
target_compile_definitions(${LIBRARY_NAME} INTERFACE STORMLIB_NO_AUTO_LINK) #CMake will take care of the linking
target_include_directories(${LIBRARY_NAME} PUBLIC src/)
set_target_properties(${LIBRARY_NAME} PROPERTIES PUBLIC_HEADER "src/StormLib.h;src/StormPort.h")
if(BUILD_SHARED_LIBS)
    message(STATUS "Linking against dependent libraries dynamically")

    if(APPLE)
        set_target_properties(${LIBRARY_NAME} PROPERTIES FRAMEWORK true)
        set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "-framework Carbon")
    endif()
    if(UNIX)
        SET(VERSION_MAJOR "9")
        SET(VERSION_MINOR "22")
        SET(VERSION_PATCH "0")
        SET(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
        set_target_properties(${LIBRARY_NAME} PROPERTIES VERSION "${VERSION_STRING}")
        set_target_properties(${LIBRARY_NAME} PROPERTIES SOVERSION "${VERSION_MAJOR}")
    endif()
else()
    message(STATUS "Linking against dependent libraries statically")
endif()

if (NOT STORM_SKIP_INSTALL)
    install(TARGETS ${LIBRARY_NAME}
	    RUNTIME DESTINATION bin
	    LIBRARY DESTINATION lib
	    ARCHIVE DESTINATION lib
	    FRAMEWORK DESTINATION /Library/Frameworks
	    PUBLIC_HEADER DESTINATION include
	    INCLUDES DESTINATION include)

        #CPack configurtion
        SET(CPACK_GENERATOR "DEB" "RPM")
        SET(CPACK_PACKAGE_NAME ${PROJECT_NAME})
        SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MPQ manipulation library")
        SET(CPACK_PACKAGE_VENDOR "Ladislav Zezula")
        SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
        SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        SET(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
        SET(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
        SET(CPACK_PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")

        #DEB configuration
        SET(CPACK_DEBIAN_PACKAGE_SECTION "libs")
        SET(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://www.zezula.net/en/mpq/stormlib.html")
        SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "imbacen@gmail.com")
        SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        SET(CPACK_DEBIAN_PACKAGE_DEPENDS "zlib1g,bzip2")

        #RPM configuration
        SET(CPACK_RPM_PACKAGE_RELEASE 1)
        SET(CPACK_RPM_PACKAGE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        SET(CPACK_RPM_PACKAGE_GROUP "${PROJECT_NAME}")
        SET(CPACK_RPM_PACKAGE_URL "http://www.zezula.net/en/mpq/stormlib.html")
        SET(CPACK_RPM_PACKAGE_REQUIRES "zlib,bzip2")

        INCLUDE(CPack)
endif()

if(STORM_BUILD_TESTS)
    add_executable(StormLib_test ${TEST_SRC_FILES})
    target_link_libraries(StormLib_test ${LIBRARY_NAME})
    install(TARGETS StormLib_test RUNTIME DESTINATION bin)
endif()

