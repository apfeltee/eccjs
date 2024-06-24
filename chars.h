//
//  chars.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "builtins.h"

typedef struct
{
    uint32_t value : 24;
} io_libecc_chars_length;

enum io_libecc_chars_Flags
{
    io_libecc_chars_mark = 1 << 0,
    io_libecc_chars_asciiOnly = 1 << 1,
};

struct io_libecc_chars_Append
{
    struct io_libecc_Chars* value;
    char buffer[9];
    uint8_t units;
};

struct io_libecc_Chars
{
    int32_t length;
    int16_t referenceCount;
    uint8_t flags;
    char bytes[1];
};

struct type_io_libecc_Chars
{
    struct io_libecc_Chars* (*createVA)(int32_t length, const char* format, va_list ap);
    struct io_libecc_Chars* (*create)(const char* format, ...);
    struct io_libecc_Chars* (*createSized)(int32_t length);
    struct io_libecc_Chars* (*createWithBytes)(int32_t length, const char* bytes);
    void (*beginAppend)(struct io_libecc_chars_Append*);
    void (*append)(struct io_libecc_chars_Append*, const char* format, ...);
    void (*appendCodepoint)(struct io_libecc_chars_Append*, uint32_t cp);
    void (*appendValue)(struct io_libecc_chars_Append*, struct io_libecc_Context* const context, struct io_libecc_Value value);
    void (*appendBinary)(struct io_libecc_chars_Append*, double binary, int base);
    void (*normalizeBinary)(struct io_libecc_chars_Append*);
    struct io_libecc_Value (*endAppend)(struct io_libecc_chars_Append*);
    void (*destroy)(struct io_libecc_Chars*);
    uint8_t (*codepointLength)(uint32_t cp);
    uint8_t (*writeCodepoint)(char*, uint32_t cp);
    const struct io_libecc_Chars identity;
} extern const io_libecc_Chars;
