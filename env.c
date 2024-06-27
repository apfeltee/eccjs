//
//  env.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

#if __MSDOS__
    #include <conio.h>
#elif _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#if _WIN32
    #include <sys/timeb.h>
#elif _DEFAULT_SOURCE || __APPLE__
    #include <sys/time.h>
#endif

// MARK: - Private

#define PRINT_MAX 2048


const int io_libecc_env_print_max = PRINT_MAX;

static struct
{
#if __MSDOS__
    int attribute;
#elif _WIN32
    HANDLE console;
    int attribute;
    int cp;
#else
    int terminal;
#endif
} env;

void ecc_env_setup(void)
{
    srand((unsigned)time(NULL));

#if __MSDOS__
    struct text_info textInfo;
    gettextinfo(&textInfo);
    env.attribute = textInfo.normattr;
#elif _WIN32
    CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
    env.console = GetStdHandle(STD_ERROR_HANDLE);
    env.cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    GetConsoleScreenBufferInfo(env.console, &consoleScreenBufferInfo);
    env.attribute = consoleScreenBufferInfo.wAttributes;
#else
    env.terminal = getenv("TERM") != NULL;
#endif
}

void ecc_env_teardown(void)
{
#if __MSDOS__
    textattr(env.attribute);
#elif _WIN32
    SetConsoleOutputCP(env.cp);
    SetConsoleTextAttribute(env.console, env.attribute);
#endif

    ecc_env_newline();
}

void eccenv_textc(int c, int a)
{
#if __MSDOS__ || _WIN32
    if(a == ECC_ENVATTR_INVISIBLE)
        c = (env.attribute >> 4) & 0x7;
    else if(c == ECC_COLOR_WHITE || !c)
        c = a == ECC_ENVATTR_BOLD ? 0xf : 0x7;
    else if(c == ECC_COLOR_BLACK)
        c = a == ECC_ENVATTR_BOLD ? 0x7 : 0x8;
    else
    {
        c -= 30;
        c = (a == ECC_ENVATTR_BOLD ? 0x8 : 0) | ((c << 2) & 0x4) | (c & 0x2) | ((c >> 2) & 0x1);
    }
    c |= env.attribute & 0xf0;

    #if __MSDOS__
    textattr(c);
    #elif _WIN32
    SetConsoleTextAttribute(env.console, c);
    #endif
#else
    if(env.terminal)
    {
        if(c && a)
            fprintf(stderr, "\x1B[%d;%dm", c, a);
        else if(c | a)
            fprintf(stderr, "\x1B[%dm", c | a);
        else
            fprintf(stderr, "\x1B[0m");
    }
#endif
}

void eccenv_vprintc(const char* format, va_list ap)
{
    char buffer[PRINT_MAX];
    size_t size = sizeof(buffer);

    if(vsnprintf(buffer, size, format, ap) >= (int)size)
    {
        buffer[size - 3] = '.';
        buffer[size - 2] = '.';
    }
#if __MSDOS__
    cputs(buffer);
#elif _WIN32
    {
        DWORD count;
        WriteConsoleA(env.console, buffer, strlen(buffer), &count, NULL);
    }
#else
    fputs(buffer, stderr);
#endif
}

void ecc_env_print(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    eccenv_vprintc(format, ap);
    va_end(ap);
}

void ecc_env_printcolor(int color, int attribute, const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    eccenv_textc(color, attribute);
    eccenv_vprintc(format, ap);
    eccenv_textc(0, 0);
    va_end(ap);
}

void ecc_env_printerror(int typeLength, const char* type, const char* format, ...)
{
    va_list ap;

    ecc_env_printcolor(ECC_COLOR_RED, ECC_ENVATTR_BOLD, "%.*s", typeLength, type);
    ecc_env_print(": ");

    va_start(ap, format);
    eccenv_textc(0, ECC_ENVATTR_BOLD);
    eccenv_vprintc(format, ap);
    eccenv_textc(0, 0);
    va_end(ap);

    ecc_env_newline();
}

void ecc_env_printwarning(const char* format, ...)
{
    va_list ap;

    ecc_env_printcolor(ECC_COLOR_YELLOW, ECC_ENVATTR_BOLD, "Warning");
    ecc_env_print(": ");

    va_start(ap, format);
    eccenv_textc(0, ECC_ENVATTR_BOLD);
    eccenv_vprintc(format, ap);
    eccenv_textc(0, 0);
    va_end(ap);

    ecc_env_newline();
}

void ecc_env_newline()
{
#if __MSDOS__ || _WIN32
    putc('\r', stderr);
#endif
    putc('\n', stderr);
}

double ecc_env_currenttime()
{
#if _WIN32
    struct _timeb timebuffer;
    _ftime(&timebuffer);
    return timebuffer.time * 1000 + timebuffer.millitm;
#elif _DEFAULT_SOURCE || __APPLE__
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
#else
    return time(NULL) * 1000;
#endif
}
