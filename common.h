#ifndef __CMDPACK_COMMON_H__
#define __CMDPACK_COMMON_H__

////////////////////////////////////////////////////////////////////////////////
//
// Common headers for Command-Line Pack programs
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

// Disable fopen() warnings on VC++. It means well...
#define _CRT_SECURE_NO_WARNINGS

// Try to enable 64-bit file offsets on platforms where it's optional
#define _LARGEFILE64_SOURCE 1
#define __USE_FILE_OFFSET64 1
#define __USE_LARGEFILE64 1
#define _FILE_OFFSET_BITS 64

// Try to enable long filename support on Watcom
#define __WATCOM_LFN__ 1

// Convince MinGW that we want to glob arguments
#ifdef __MINGW32__
int _dowildcard = -1;
#endif

////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

// Try to bring in unistd.h if possible
#if !defined(__TURBOC__) && !defined(_MSC_VER)
#include <unistd.h>
#endif

// Bring in direct.h if we need to; sometimes mkdir/rmdir is defined here
#if defined(__WATCOMC__) || defined(_MSC_VER)
#include <direct.h>
#endif

// Fill in S_ISDIR
#if !defined(_POSIX_VERSION) && !defined(S_ISDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#if defined(__TURBOC__) || defined(__WATCOMC__) || defined(__MINGW32__) || defined(_MSC_VER)
//
// Already have a single-argument mkdir()
//
#else
//
// Provide a single-argument mkdir()
//
#define mkdir(a) mkdir(a, S_IRWXU | S_IRWXG | S_IRWXO)
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Enforce large memory model for 16-bit DOS targets
//
#if defined(__MSDOS__) || defined(MSDOS)
#if defined(__TURBOC__) || defined(__WATCOMC__)
#if !defined(__LARGE__)
#error This is not the memory model we should be using!
#endif
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Try to figure out integer types
//
#if defined(_STDINT_H) || defined(_EXACT_WIDTH_INTS)

// _STDINT_H_ - presume stdint.h has already been included
// _EXACT_WIDTH_INTS - OpenWatcom already provides int*_t in sys/types.h

#elif defined(__STDC__) && __STDC__ && __STDC_VERSION__ >= 199901L

// Assume C99 compliance when the compiler specifically tells us it is
#include <stdint.h>

#elif defined(_MSC_VER)

// On Visual Studio, use its integral types
typedef   signed __int8   int8_t;
typedef unsigned __int8  uint8_t;
typedef   signed __int16  int16_t;
typedef unsigned __int16 uint16_t;
typedef   signed __int32  int32_t;
typedef unsigned __int32 uint32_t;

#else

// Guess integer sizes from limits.h

//
// int8_t
//
#ifndef __int8_t_defined
#if SCHAR_MIN == -128 && SCHAR_MAX == 127 && UCHAR_MAX == 255
typedef signed char int8_t;
#else
#error Unknown how to define int8_t!
#endif
#endif

//
// uint8_t
//
#ifndef __uint8_t_defined
#if SCHAR_MIN == -128 && SCHAR_MAX == 127 && UCHAR_MAX == 255
typedef unsigned char uint8_t;
#else
#error Unknown how to define uint8_t!
#endif
#endif

//
// int16_t
//
#ifndef __int16_t_defined
#if SHRT_MIN == -32768 && SHRT_MAX == 32767 && USHRT_MAX == 65535
typedef signed short int16_t;
#else
#error Unknown how to define int16_t!
#endif
#endif

//
// uint16_t
//
#ifndef __uint16_t_defined
#if SHRT_MIN == -32768 && SHRT_MAX == 32767 && USHRT_MAX == 65535
typedef unsigned short uint16_t;
#else
#error Unknown how to define uint16_t!
#endif
#endif

//
// int32_t
//
#ifndef __int32_t_defined
#if    INT_MIN == -2147483648 &&  INT_MAX == 2147483647 &&  UINT_MAX == 4294967295
typedef signed int int32_t;
#elif LONG_MIN == -2147483648 && LONG_MAX == 2147483647 && ULONG_MAX == 4294967295
typedef signed long int32_t;
#else
#error Unknown how to define int32_t!
#endif
#endif

//
// uint32_t
//
#ifndef __uint32_t_defined
#if    INT_MIN == -2147483648 &&  INT_MAX == 2147483647 &&  UINT_MAX == 4294967295
typedef unsigned int uint32_t;
#elif LONG_MIN == -2147483648 && LONG_MAX == 2147483647 && ULONG_MAX == 4294967295
typedef unsigned long uint32_t;
#else
#error Unknown how to define uint32_t!
#endif
#endif

#endif

//
// There are some places in the code where it's assumed 'long' can hold at least
// 32 bits.  Verify that here:
//
#if LONG_MAX < 2147483647 || ULONG_MAX < 4294967295
#error long type must be at least 32 bits!
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Figure out how big file offsets should be
//
#if defined(_OFF64_T_) || defined(_OFF64_T_DEFINED) || defined(__off64_t_defined)
//
// We have off64_t
// Regular off_t may be smaller, so check this first
//

#ifdef off_t
#undef off_t
#endif
#ifdef fseeko
#undef fseeko
#endif
#ifdef ftello
#undef ftello
#endif

#define off_t off64_t
#define fseeko fseeko64
#define ftello ftello64

#elif defined(_OFF_T) || defined(__OFF_T_TYPE) || defined(__off_t_defined) || defined(_OFF_T_DEFINED_)
//
// We have off_t
//

#else
//
// Assume offsets are just 'long'
//
#ifdef off_t
#undef off_t
#endif
#ifdef fseeko
#undef fseeko
#endif
#ifdef ftello
#undef ftello
#endif

#define off_t long
#define fseeko fseek
#define ftello ftell

#endif

//
// Add the ability to read off_t
// (assumes off_t is a signed type)
//
off_t strtoofft(const char* s_start, char** endptr, int base) {
    off_t max =
        ((((off_t)1) << ((sizeof(off_t)*8)-2)) - 1) +
        ((((off_t)1) << ((sizeof(off_t)*8)-2))    );
    off_t min = ((-1) - max);
    const char* s = s_start;
    off_t accumulator;
    off_t limit_tens;
    off_t limit_ones;
    int c;
    int negative = 0;
    int anyinput;
    do {
        c = *s++;
    } while(isspace(c));
    if(c == '-') {
        negative = 1;
        c = *s++;
    } else if (c == '+') {
        c = *s++;
    }
    if(
        (base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X')
    ) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if(!base) {
        base = (c == '0') ? 8 : 10;
    }
    limit_ones = max % ((off_t)base);
    limit_tens = max / ((off_t)base);
    if(negative) {
        limit_ones++;
        if(limit_ones >= base) { limit_ones = 0; limit_tens++; }
    }
    for(accumulator = 0, anyinput = 0;; c = *s++) {
        if(isdigit(c)) {
            c -= '0';
        } else if(isalpha(c)) {
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        } else {
            break;
        }
        if(c >= base) { break; }
        if(
            (anyinput < 0) ||
            (accumulator < 0) ||
            (accumulator > limit_tens) ||
            (accumulator == limit_tens && c > limit_ones)
        ) {
            anyinput = -1;
        } else {
            anyinput = 1;
            accumulator *= base;
            accumulator += c;
        }
    }
    if(anyinput < 0) {
        accumulator = negative ? min : max;
        errno = ERANGE;
    } else if(negative) {
        accumulator = -accumulator;
    }
    if(endptr) {
        *endptr = (char*)(anyinput ? (char*)s - 1 : s_start);
    }
    return accumulator;
}

//
// Add the ability to print off_t
//
void fprinthex(FILE* f, off_t off, int min_digits) {
    unsigned anydigit = 0;
    int place;
    for(place = 2 * sizeof(off_t) - 1; place >= 0; place--) {
        if(sizeof(off_t) > (((size_t)(place)) / 2)) {
            unsigned digit = (off >> (4 * place)) & 0xF;
            anydigit |= digit;
            if(anydigit || place < min_digits) {
                fputc("0123456789ABCDEF"[digit], f);
            }
        }
    }
}

static void fprintdec_digit(FILE* f, off_t off) {
    if(off == 0) { return; }
    if(off >= 10) {
        fprintdec_digit(f, off / ((off_t)10));
        off %= ((off_t)10);
    }
    fputc('0' + off, f);
}

void fprintdec(FILE* f, off_t off) {
    if(off == 0) {
        fputc('0', f);
        return;
    }
    if(off < 0) {
        fputc('-', f);
        off = -off;
        if(off < 0) {
            off_t ones = off % ((off_t)10);
            off /= ((off_t)10);
            off = -off;
            fprintdec_digit(f, off);
            fputc('0' - ones, f);
            return;
        }
    }
    fprintdec_digit(f, off);
}

////////////////////////////////////////////////////////////////////////////////
//
// Define truncate() for systems that don't have it
//
#if !defined(_POSIX_VERSION)

#if (defined(__MSDOS__) || defined(MSDOS)) && (defined(__TURBOC__) || defined(__WATCOMC__))

#include <dos.h>
#include <io.h>
#include <fcntl.h>
int truncate(const char *filename, off_t size) {
    if(size < 0) {
        errno = EINVAL;
        return -1;
    }
    //
    // Extend (or do nothing) if necessary
    //
    {   off_t end;
        FILE* f = fopen(filename, "rb");
        if(!f) {
            return -1;
        }
        if(fseeko(f, 0, SEEK_END) != 0) {
            fclose(f);
            return -1;
        }
        end = ftello(f);
        if(end <= size) {
            for(; end < size; end++) {
                if(fputc(0, f) == EOF) {
                    fclose(f);
                    return -1;
                }
            }
            fclose(f);
            return 0;
        }
        fclose(f);
    }
    //
    // Shrink if necessary (DOS-specific call)
    //
    {   int doshandle = 0;
        unsigned nwritten = 0;
        if(_dos_open(filename, O_WRONLY, &doshandle)) {
            return -1;
        }
        if(lseek(doshandle, size, SEEK_SET) == -1L) {
            _dos_close(doshandle);
            return -1;
        }
        if(_dos_write(doshandle, &doshandle, 0, &nwritten)) {
            _dos_close(doshandle);
            return -1;
        }
        _dos_close(doshandle);
    }
    //
    // Success
    //
    return 0;
}

#elif (defined(_WIN32) && defined(_MSC_VER))

#if defined(_MSC_VER)
// Disable extension warnings for <windows.h> and friends
#pragma warning (disable: 4226)
#endif

#include <windows.h>

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)(-1))
#endif

int truncate(const char *filename, off_t size) {
    if(size < 0) {
        errno = EINVAL;
        return -1;
    }
    //
    // Extend (or do nothing) if necessary
    //
    {   off_t end;
        FILE* f = fopen(filename, "rb");
        if(!f) {
            return -1;
        }
        if(fseeko(f, 0, SEEK_END) != 0) {
            fclose(f);
            return -1;
        }
        end = ftello(f);
        if(end <= size) {
            for(; end < size; end++) {
                if(fputc(0, f) == EOF) {
                    fclose(f);
                    return -1;
                }
            }
            fclose(f);
            return 0;
        }
        fclose(f);
    }
    //
    // Shrink if necessary (Windows-specific call)
    //
    {   HANDLE f = CreateFile(
            filename,
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if(f == INVALID_HANDLE_VALUE) {
            return -1;
        }
        if(size > ((off_t)0x7FFFFFFFL)) {
            // use fancy 64-bit SetFilePointer
            LONG lo = size;
            LONG hi = size >> 32;
            if(SetFilePointer(f, lo, &hi, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
                CloseHandle(f);
                return -1;
            }
        } else {
            // use plain 32-bit SetFilePointer
            if(SetFilePointer(f, size, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
                CloseHandle(f);
                return -1;
            }
        }
        if(!SetEndOfFile(f)) {
            CloseHandle(f);
            return -1;
        }
    }
    //
    // Success
    //
    return 0;
}

#endif

#endif // !defined(_POSIX_VERSION)

////////////////////////////////////////////////////////////////////////////////
//
// Normalize argv[0]
//
void normalize_argv0(char* argv0) {
    size_t i;
    size_t start = 0;
    int c;
    for(i = 0; argv0[i]; i++) {
        if(argv0[i] == '/' || argv0[i] == '\\') {
            start = i + 1;
        }
    }
    i = 0;
    do {
        c = ((unsigned char)(argv0[start + i]));
        if(c == '.') { c = 0; }
        if(c != 0) { c = tolower(c); }
        argv0[i++] = c;
    } while(c != 0);
}

////////////////////////////////////////////////////////////////////////////////

void printfileerror(FILE* f, const char* name) {
    printf("Error: ");
    if(name) { printf("%s: ", name); }
    printf("%s\n", f && feof(f) ? "Unexpected end-of-file" : strerror(errno));
}

////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)

//
// Detect if the user double-clicked on the .exe rather than executing this from
// the command line, and if so, display a warning and wait for input before
// exiting
//
#include <windows.h>

static HWND getconsolewindow(void) {
    HWND hConsoleWindow = NULL;
    HANDLE k32;
    //
    // See if GetConsoleWindow is available (Windows 2000 or later)
    //
    k32 = LoadLibrary(TEXT("kernel32.dll"));
    if(k32) {
        typedef HWND (* WINAPI gcw_t)(void);
        gcw_t gcw = (gcw_t)GetProcAddress(k32, TEXT("GetConsoleWindow"));
        if(gcw) {
            hConsoleWindow = gcw();
        }
        FreeLibrary(k32);
    }
    //
    // If that didn't work, try the FindWindow trick
    //
    if(!hConsoleWindow) {
        TCHAR savedTitle[1024];
        TCHAR tempTitle[64];
        DWORD id = GetCurrentProcessId();
        unsigned i;
        //
        // Create a random temporary title
        //
        sprintf(tempTitle, "%08lX", (unsigned long)(id));
        srand(id + time(NULL));
        for(i = 8; i < sizeof(tempTitle) - 1; i++) {
            tempTitle[i] = 0x20 + (rand() % 95);
        }
        tempTitle[sizeof(tempTitle) - 1] = 0;
        if(GetConsoleTitle(savedTitle, sizeof(savedTitle))) {
            SetConsoleTitle(tempTitle);
            //
            // Sleep for a tenth of a second to make sure the title actually got set
            //
            Sleep(100);
            //
            // Find the console HWND using the temp title
            //
            hConsoleWindow = FindWindow(0, tempTitle);
            //
            // Restore the old title
            //
            SetConsoleTitle(savedTitle);
        }
    }
    return hConsoleWindow;
}

void commandlinewarning(void) {
    HWND hConsoleWindow = getconsolewindow();
    DWORD processId = 0;
    //
    // See if the console window belongs to my own process
    //
    if(!hConsoleWindow) { return; }
    GetWindowThreadProcessId(hConsoleWindow, &processId);
    if(GetCurrentProcessId() == processId) {
        printf(
            "\n"
            "Note: This is a command-line application.\n"
            "It was meant to run from a Windows command prompt.\n\n"
            "Press ENTER to close this window..."
        );
        fflush(stdout);
        fgetc(stdin);
    }
}

#else

void commandlinewarning(void) {}

#endif

////////////////////////////////////////////////////////////////////////////////
//
// Work around some problems with the Mariko CC toolchain
//
#ifdef MARIKO_CC

// 32-bit signed and unsigned mod seem buggy; this solves it
unsigned long __umodsi3(unsigned long a, unsigned long b) { return a - (a / b) * b; }
signed long __modsi3(signed long a, signed long b) { return a - (a / b) * b; }

// Some kind of soft float linkage issue?
void __cmpdf2(void) {}

#endif

////////////////////////////////////////////////////////////////////////////////

#endif
