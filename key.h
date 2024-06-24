//
//  key.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "text.h"

extern struct io_libecc_Key io_libecc_key_none;
extern struct io_libecc_Key io_libecc_key_prototype;
extern struct io_libecc_Key io_libecc_key_constructor;
extern struct io_libecc_Key io_libecc_key_length;
extern struct io_libecc_Key io_libecc_key_arguments;
extern struct io_libecc_Key io_libecc_key_callee;
extern struct io_libecc_Key io_libecc_key_name;
extern struct io_libecc_Key io_libecc_key_message;
extern struct io_libecc_Key io_libecc_key_toString;
extern struct io_libecc_Key io_libecc_key_valueOf;
extern struct io_libecc_Key io_libecc_key_eval;
extern struct io_libecc_Key io_libecc_key_value;
extern struct io_libecc_Key io_libecc_key_writable;
extern struct io_libecc_Key io_libecc_key_enumerable;
extern struct io_libecc_Key io_libecc_key_configurable;
extern struct io_libecc_Key io_libecc_key_get;
extern struct io_libecc_Key io_libecc_key_set;
extern struct io_libecc_Key io_libecc_key_join;
extern struct io_libecc_Key io_libecc_key_toISOString;
extern struct io_libecc_Key io_libecc_key_input;
extern struct io_libecc_Key io_libecc_key_index;
extern struct io_libecc_Key io_libecc_key_lastIndex;
extern struct io_libecc_Key io_libecc_key_global;
extern struct io_libecc_Key io_libecc_key_ignoreCase;
extern struct io_libecc_Key io_libecc_key_multiline;
extern struct io_libecc_Key io_libecc_key_source;


enum io_libecc_key_Flags
{
    io_libecc_key_copyOnCreate = (1 << 0),
};

struct io_libecc_Key
{
    union
    {
        uint8_t depth[4];
        uint32_t integer;
    } data;
};

struct type_io_libecc_Key
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Key (*makeWithCString)(const char* cString);
    struct io_libecc_Key (*makeWithText)(const struct io_libecc_Text text, enum io_libecc_key_Flags flags);
    struct io_libecc_Key (*search)(const struct io_libecc_Text text);
    int (*isEqual)(struct io_libecc_Key, struct io_libecc_Key);
    const struct io_libecc_Text* (*textOf)(struct io_libecc_Key);
    void (*dumpTo)(struct io_libecc_Key, FILE*);
    const struct io_libecc_Key identity;
} extern const io_libecc_Key;

