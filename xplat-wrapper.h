// project neccessary includes and comments, dependent on OS
#ifdef __linux__

#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>

#include "linux_util.h"

#elif _WIN32
#pragma comment(linker, "/stack:268435456")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "shlwapi.lib")

#include <windows.h>
#include <fileapi.h>
#include <shlwapi.h>
#else
#error
#endif

#include <string>

namespace XPlat
{

#ifdef __linux__

    using HandleType = FILE *;
    using PathAttributeType = struct stat;

    using FlagType = int;

    const char PATH_SEPARATOR = '/';

    const int INVALID_HANDLE = -1;

#elif _WIN32

    using HandleType = HANDLE;
    using PathAttributeType = DWORD;

    using FlagType = DWORD;

    const char PATH_SEPARATOR = '\\';

    const typeof(INVALID_FILE_HANDLE) INVALID_HANDLE = INVALID_FILE_HANDLE; // should be HANDLE -> LPVOID
#endif

    static bool is_directory(XPlat::PathAttributeType &attribute)
    {
#ifdef _WIN32
        return attribute & FILE_ATTRIBUTE_DIRECTORY;
#else
        return S_ISDIR(attribute.st_mode) != 0;
#endif
    }

    static bool is_file(XPlat::PathAttributeType &attribute)
    {
    #ifdef _WIN32
            return attribute & INVALID_FILE_ATTRIBUTES == 0 && is_directory(attribute) == false;
    #else
            return S_ISREG(attribute.st_mode) != 0;
    #endif
    }

    static HandleType open_file(const char* path, FlagType accessFlags, FlagType creationFlags, FlagType additionalFlags );

    static bool close_file(HandleType handle);
}