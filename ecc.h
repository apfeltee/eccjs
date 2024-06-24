
//
//  ecc.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include <stdbool.h>
#include "compatibility.h"
#include "builtins.h"
#include "input.h"

enum io_libecc_ecc_EvalFlags
{
    io_libecc_ecc_sloppyMode = 0x1,
    io_libecc_ecc_primitiveResult = 0x2,
    io_libecc_ecc_stringResult = 0x6,
};

extern uint32_t io_libecc_ecc_version;

struct io_libecc_Ecc
{
    jmp_buf* envList;
    uint16_t envCount;
    uint16_t envCapacity;
    struct io_libecc_Function* global;
    struct io_libecc_Value result;
    struct io_libecc_Text text;
    int32_t ofLine;
    struct io_libecc_Text ofText;
    const char* ofInput;
    struct io_libecc_Input** inputs;
    uint16_t inputCount;
    int16_t maximumCallDepth;
    unsigned printLastThrow : 1;
    unsigned sloppyMode : 1;
};

struct type_io_libecc_Ecc
{
    struct io_libecc_Ecc* (*create)(void);
    void (*destroy)(struct io_libecc_Ecc*);
    void (*addValue)(struct io_libecc_Ecc*, const char* name, struct io_libecc_Value value, enum io_libecc_value_Flags);
    void (*addFunction)(struct io_libecc_Ecc*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    int (*evalInput)(struct io_libecc_Ecc*, struct io_libecc_Input*, enum io_libecc_ecc_EvalFlags);
    void (*evalInputWithContext)(struct io_libecc_Ecc*, struct io_libecc_Input*, struct io_libecc_Context* context);
    jmp_buf* (*pushEnv)(struct io_libecc_Ecc*);
    void (*popEnv)(struct io_libecc_Ecc*);
    void (*jmpEnv)(struct io_libecc_Ecc*, struct io_libecc_Value value) __attribute__((noreturn));
    void (*fatal)(const char* format, ...) __attribute__((noreturn));
    struct io_libecc_Input* (*findInput)(struct io_libecc_Ecc* self, struct io_libecc_Text text);
    void (*printTextInput)(struct io_libecc_Ecc*, struct io_libecc_Text text, int fullLine);
    void (*garbageCollect)(struct io_libecc_Ecc*);
    const struct io_libecc_Ecc identity;
} extern const io_libecc_Ecc;


