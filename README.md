# StormLib

This is official repository for the StomLib library, an open-source project that can work with Blizzard MPQ archives.

## Installation and basic usage
### Linux
1. Download latest release
2. Install StormLib:
```
$ cd <path-to-StormLib>
$ cmake CMakeLists.txt
$ make
$ make install
```
3. Include StormLib in your project: `#include <StormLib.h>`
4. Make sure you compile your project with `-lstorm -lz -lbz2`

