//
//  oplist.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "op.h"



struct io_libecc_OpList
{
    uint32_t count;
    struct io_libecc_Op* ops;
};

struct type_io_libecc_OpList
{
    struct io_libecc_OpList* (*create)(const io_libecc_native_io_libecc_Function native, struct io_libecc_Value value, struct io_libecc_Text text);
    void (*destroy)(struct io_libecc_OpList*);
    struct io_libecc_OpList* (*join)(struct io_libecc_OpList*, struct io_libecc_OpList*);
    struct io_libecc_OpList* (*join3)(struct io_libecc_OpList*, struct io_libecc_OpList*, struct io_libecc_OpList*);
    struct io_libecc_OpList* (*joinDiscarded)(struct io_libecc_OpList*, uint16_t n, struct io_libecc_OpList*);
    struct io_libecc_OpList* (*unshift)(struct io_libecc_Op op, struct io_libecc_OpList*);
    struct io_libecc_OpList* (*unshiftJoin)(struct io_libecc_Op op, struct io_libecc_OpList*, struct io_libecc_OpList*);
    struct io_libecc_OpList* (*unshiftJoin3)(struct io_libecc_Op op, struct io_libecc_OpList*, struct io_libecc_OpList*, struct io_libecc_OpList*);
    struct io_libecc_OpList* (*shift)(struct io_libecc_OpList*);
    struct io_libecc_OpList* (*append)(struct io_libecc_OpList*, struct io_libecc_Op op);
    struct io_libecc_OpList* (*appendNoop)(struct io_libecc_OpList*);
    struct io_libecc_OpList* (*createLoop)(
    struct io_libecc_OpList* initial, struct io_libecc_OpList* condition, struct io_libecc_OpList* step, struct io_libecc_OpList* body, int reverseCondition);
    void (*optimizeWithEnvironment)(struct io_libecc_OpList*, struct io_libecc_Object* environment, uint32_t index);
    void (*dumpTo)(struct io_libecc_OpList*, FILE* file);
    struct io_libecc_Text (*text)(struct io_libecc_OpList* oplist);
    const struct io_libecc_OpList identity;
} extern const io_libecc_OpList;
