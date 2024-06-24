//
//  context.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "value.h"

enum io_libecc_context_Index
{
    io_libecc_context_savedIndexAlt = -2,
    io_libecc_context_savedIndex = -1,
    io_libecc_context_noIndex = 0,
    io_libecc_context_callIndex = 1,
    io_libecc_context_funcIndex = 2,
    io_libecc_context_thisIndex = 3,
};

enum io_libecc_context_Offset
{
    io_libecc_context_accessorOffset = -1,
    io_libecc_context_callOffset = 1,
    io_libecc_context_applyOffset = 2,
};

enum io_libecc_context_Special
{
    io_libecc_context_countMask = 0x7f,
    io_libecc_context_asAccessor = 1 << 8,
};

struct io_libecc_Context
{
    const struct io_libecc_Op* ops;
    struct io_libecc_Object* refObject;
    struct io_libecc_Object* environment;
    struct io_libecc_Context* parent;
    struct io_libecc_Ecc* ecc;
    struct io_libecc_Value this;
    const struct io_libecc_Text* text;
    const struct io_libecc_Text* textAlt;
    const struct io_libecc_Text* textCall;
    enum io_libecc_context_Index textIndex;
    int16_t breaker;
    int16_t depth;
    int8_t construct : 1;
    int8_t argumentOffset : 3;
    int8_t strictMode : 1;
    int8_t inEnvironmentObject : 1;
};

struct type_io_libecc_Context
{
    void (*rangeError)(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*referenceError)(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*syntaxError)(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*typeError)(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*uriError)(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*throw)(struct io_libecc_Context* const, struct io_libecc_Value) __attribute__((noreturn));
    struct io_libecc_Value (*callFunction)(struct io_libecc_Context* const, struct io_libecc_Function* function, struct io_libecc_Value this, int argumentCount, ...);
    int (*argumentCount)(struct io_libecc_Context* const);
    struct io_libecc_Value (*argument)(struct io_libecc_Context* const, int argumentIndex);
    void (*replaceArgument)(struct io_libecc_Context* const, int argumentIndex, struct io_libecc_Value value);
    struct io_libecc_Value (*this)(struct io_libecc_Context* const);
    void (*assertThisType)(struct io_libecc_Context* const, enum io_libecc_value_Type);
    void (*assertThisMask)(struct io_libecc_Context* const, enum io_libecc_value_Mask);
    void (*assertThisCoerciblePrimitive)(struct io_libecc_Context* const);
    void (*setText)(struct io_libecc_Context* const, const struct io_libecc_Text* text);
    void (*setTexts)(struct io_libecc_Context* const, const struct io_libecc_Text* text, const struct io_libecc_Text* textAlt);
    void (*setTextIndex)(struct io_libecc_Context* const, enum io_libecc_context_Index index);
    void (*setTextIndexArgument)(struct io_libecc_Context* const, int argument);
    struct io_libecc_Text (*textSeek)(struct io_libecc_Context* const);
    void (*rewindStatement)(struct io_libecc_Context* const);
    void (*printBacktrace)(struct io_libecc_Context* const context);
    struct io_libecc_Object* (*environmentRoot)(struct io_libecc_Context* const context);
    const struct io_libecc_Context identity;
} extern const io_libecc_Context;

