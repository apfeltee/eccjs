//
//  text.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"


extern const struct io_libecc_Text io_libecc_text_undefined;
extern const struct io_libecc_Text io_libecc_text_null;
extern const struct io_libecc_Text io_libecc_text_false;
extern const struct io_libecc_Text io_libecc_text_true;
extern const struct io_libecc_Text io_libecc_text_boolean;
extern const struct io_libecc_Text io_libecc_text_number;
extern const struct io_libecc_Text io_libecc_text_string;
extern const struct io_libecc_Text io_libecc_text_object;
extern const struct io_libecc_Text io_libecc_text_function;
extern const struct io_libecc_Text io_libecc_text_zero;
extern const struct io_libecc_Text io_libecc_text_one;
extern const struct io_libecc_Text io_libecc_text_nan;
extern const struct io_libecc_Text io_libecc_text_infinity;
extern const struct io_libecc_Text io_libecc_text_negativeInfinity;
extern const struct io_libecc_Text io_libecc_text_nativeCode;
extern const struct io_libecc_Text io_libecc_text_empty;
extern const struct io_libecc_Text io_libecc_text_emptyRegExp;

extern const struct io_libecc_Text io_libecc_text_nullType;
extern const struct io_libecc_Text io_libecc_text_undefinedType;
extern const struct io_libecc_Text io_libecc_text_objectType;
extern const struct io_libecc_Text io_libecc_text_errorType;
extern const struct io_libecc_Text io_libecc_text_arrayType;
extern const struct io_libecc_Text io_libecc_text_stringType;
extern const struct io_libecc_Text io_libecc_text_regexpType;
extern const struct io_libecc_Text io_libecc_text_numberType;
extern const struct io_libecc_Text io_libecc_text_booleanType;
extern const struct io_libecc_Text io_libecc_text_dateType;
extern const struct io_libecc_Text io_libecc_text_functionType;
extern const struct io_libecc_Text io_libecc_text_argumentsType;
extern const struct io_libecc_Text io_libecc_text_mathType;
extern const struct io_libecc_Text io_libecc_text_jsonType;
extern const struct io_libecc_Text io_libecc_text_globalType;

extern const struct io_libecc_Text io_libecc_text_errorName;
extern const struct io_libecc_Text io_libecc_text_rangeErrorName;
extern const struct io_libecc_Text io_libecc_text_referenceErrorName;
extern const struct io_libecc_Text io_libecc_text_syntaxErrorName;
extern const struct io_libecc_Text io_libecc_text_typeErrorName;
extern const struct io_libecc_Text io_libecc_text_uriErrorName;
extern const struct io_libecc_Text io_libecc_text_inputErrorName;
extern const struct io_libecc_Text io_libecc_text_evalErrorName;

struct io_libecc_text_Char
{
    uint32_t codepoint;
    uint8_t units;
};

enum io_libecc_text_Flags
{
    io_libecc_text_breakFlag = 1 << 0,
};

struct io_libecc_Text
{
    const char* bytes;
    int32_t length;
    uint8_t flags;
};

struct type_io_libecc_Text
{
    struct io_libecc_Text (*make)(const char* bytes, int32_t length);
    struct io_libecc_Text (*join)(struct io_libecc_Text from, struct io_libecc_Text to);
    struct io_libecc_text_Char (*character)(struct io_libecc_Text);
    struct io_libecc_text_Char (*nextCharacter)(struct io_libecc_Text* text);
    struct io_libecc_text_Char (*prevCharacter)(struct io_libecc_Text* text);
    void (*advance)(struct io_libecc_Text* text, int32_t units);
    uint16_t (*toUTF16Length)(struct io_libecc_Text);
    uint16_t (*toUTF16)(struct io_libecc_Text, uint16_t* wbuffer);
    char* (*toLower)(struct io_libecc_Text, char* x2buffer);
    char* (*toUpper)(struct io_libecc_Text, char* x3buffer);
    int (*isSpace)(struct io_libecc_text_Char);
    int (*isDigit)(struct io_libecc_text_Char);
    int (*isWord)(struct io_libecc_text_Char);
    int (*isLineFeed)(struct io_libecc_text_Char);
    const struct io_libecc_Text identity;
} extern const io_libecc_Text;


