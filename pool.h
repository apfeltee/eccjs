//
//  pool.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "builtins.h"


struct io_libecc_Pool
{
    struct io_libecc_Function** functionList;
    uint32_t functionCount;
    uint32_t functionCapacity;
    struct io_libecc_Object** objectList;
    uint32_t objectCount;
    uint32_t objectCapacity;
    struct io_libecc_Chars** charsList;
    uint32_t charsCount;
    uint32_t charsCapacity;
};

struct type_io_libecc_Pool
{
    void (*setup)(void);
    void (*teardown)(void);
    void (*addFunction)(struct io_libecc_Function* function);
    void (*addObject)(struct io_libecc_Object* object);
    void (*addChars)(struct io_libecc_Chars* chars);
    void (*unmarkAll)(void);
    void (*markValue)(struct io_libecc_Value value);
    void (*markObject)(struct io_libecc_Object* object);
    void (*collectUnmarked)(void);
    void (*collectUnreferencedFromIndices)(uint32_t indices[3]);
    void (*unreferenceFromIndices)(uint32_t indices[3]);
    void (*getIndices)(uint32_t indices[3]);
    const struct io_libecc_Pool identity;
} extern const io_libecc_Pool;

