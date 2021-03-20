# StormLib

This is official repository for the StormLib library, an open-source project that can work with Blizzard MPQ archives.

## Installation
### Windows
Install Visual Studio 2019 with Windows 10 SDK 10.0.17134.0.  
Download the latest release of StormLib and compile the code:
```
> cd <path-to-StormLib>
> cmake CMakeLists.txt
> .\make-msvc.bat
```

### Linux
Download the latest release of StormLib and compile the code:
```
$ cd <path-to-StormLib>
$ cmake CMakeLists.txt
$ make
$ make install
```

## Basic Usage
1. Include StormLib in your project: `#include <StormLib.h>`
1. Make sure you compile your project with `-lstorm -lz -lbz2`
