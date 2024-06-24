//
//  parser.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "lexer.h"

struct io_libecc_Parser
{
    struct io_libecc_Lexer* lexer;
    enum io_libecc_lexer_Token previewToken;
    struct io_libecc_Error* error;

    struct
    {
        struct io_libecc_Key key;
        char depth;
    }* depths;

    uint16_t depthCount;
    struct io_libecc_Object* global;
    struct io_libecc_Function* function;
    uint16_t sourceDepth;
    int preferInteger;
    int strictMode;
    int reserveGlobalSlots;
};

struct type_io_libecc_Parser
{
    struct io_libecc_Parser* (*createWithLexer)(struct io_libecc_Lexer*);
    void (*destroy)(struct io_libecc_Parser*);
    struct io_libecc_Function* (*parseWithEnvironment)(struct io_libecc_Parser* const, struct io_libecc_Object* environment, struct io_libecc_Object* global);
    const struct io_libecc_Parser identity;
} extern const io_libecc_Parser;
