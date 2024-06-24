//
//  lexer.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "builtins.h"
#include "input.h"

enum io_libecc_lexer_Token
{
    io_libecc_lexer_noToken = 0,
    io_libecc_lexer_errorToken = 128,
    io_libecc_lexer_nullToken,
    io_libecc_lexer_trueToken,
    io_libecc_lexer_falseToken,
    io_libecc_lexer_integerToken,
    io_libecc_lexer_binaryToken,
    io_libecc_lexer_stringToken,
    io_libecc_lexer_escapedStringToken,
    io_libecc_lexer_identifierToken,
    io_libecc_lexer_regexpToken,
    io_libecc_lexer_breakToken,
    io_libecc_lexer_caseToken,
    io_libecc_lexer_catchToken,
    io_libecc_lexer_continueToken,
    io_libecc_lexer_debuggerToken,
    io_libecc_lexer_defaultToken,
    io_libecc_lexer_deleteToken,
    io_libecc_lexer_doToken,
    io_libecc_lexer_elseToken,
    io_libecc_lexer_finallyToken,
    io_libecc_lexer_forToken,
    io_libecc_lexer_functionToken,
    io_libecc_lexer_ifToken,
    io_libecc_lexer_inToken,
    io_libecc_lexer_instanceofToken,
    io_libecc_lexer_newToken,
    io_libecc_lexer_returnToken,
    io_libecc_lexer_switchToken,
    io_libecc_lexer_thisToken,
    io_libecc_lexer_throwToken,
    io_libecc_lexer_tryToken,
    io_libecc_lexer_typeofToken,
    io_libecc_lexer_varToken,
    io_libecc_lexer_voidToken,
    io_libecc_lexer_withToken,
    io_libecc_lexer_whileToken,
    io_libecc_lexer_equalToken,
    io_libecc_lexer_notEqualToken,
    io_libecc_lexer_identicalToken,
    io_libecc_lexer_notIdenticalToken,
    io_libecc_lexer_leftShiftAssignToken,
    io_libecc_lexer_rightShiftAssignToken,
    io_libecc_lexer_unsignedRightShiftAssignToken,
    io_libecc_lexer_leftShiftToken,
    io_libecc_lexer_rightShiftToken,
    io_libecc_lexer_unsignedRightShiftToken,
    io_libecc_lexer_lessOrEqualToken,
    io_libecc_lexer_moreOrEqualToken,
    io_libecc_lexer_incrementToken,
    io_libecc_lexer_decrementToken,
    io_libecc_lexer_logicalAndToken,
    io_libecc_lexer_logicalOrToken,
    io_libecc_lexer_addAssignToken,
    io_libecc_lexer_minusAssignToken,
    io_libecc_lexer_multiplyAssignToken,
    io_libecc_lexer_divideAssignToken,
    io_libecc_lexer_moduloAssignToken,
    io_libecc_lexer_andAssignToken,
    io_libecc_lexer_orAssignToken,
    io_libecc_lexer_xorAssignToken,
};

enum io_libecc_lexer_ScanFlags
{
    io_libecc_lexer_scanLazy = 1 << 0,
    io_libecc_lexer_scanSloppy = 1 << 1,
};

struct io_libecc_Lexer
{
    struct io_libecc_Input* input;
    uint32_t offset;
    struct io_libecc_Value value;
    struct io_libecc_Text text;
    int didLineBreak;
    int allowRegex;
    int allowUnicodeOutsideLiteral;
    int disallowKeyword;
};

struct type_io_libecc_Lexer
{
    struct io_libecc_Lexer* (*createWithInput)(struct io_libecc_Input*);
    void (*destroy)(struct io_libecc_Lexer*);
    enum io_libecc_lexer_Token (*nextToken)(struct io_libecc_Lexer*);
    const char* (*tokenChars)(enum io_libecc_lexer_Token token, char buffer[4]);
    struct io_libecc_Value (*scanBinary)(struct io_libecc_Text text, enum io_libecc_lexer_ScanFlags);
    struct io_libecc_Value (*scanInteger)(struct io_libecc_Text text, int base, enum io_libecc_lexer_ScanFlags);
    uint32_t (*scanElement)(struct io_libecc_Text text);
    uint8_t (*uint8Hex)(char a, char b);
    uint16_t (*uint16Hex)(char a, char b, char c, char d);
    const struct io_libecc_Lexer identity;
} extern const io_libecc_Lexer;

