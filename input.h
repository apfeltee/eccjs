//
//  input.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "value.h"
#include "env.h"

struct io_libecc_Input
{
    char name[
    4096
    ];
    uint32_t length;
    char* bytes;
    uint16_t lineCount;
    uint16_t lineCapacity;
    uint32_t* lines;
    struct io_libecc_Value* attached;
    uint16_t attachedCount;
};

struct type_io_libecc_Input
{
    struct io_libecc_Input* (*createFromFile)(const char* filename);
    struct io_libecc_Input* (*createFromBytes)(const char* bytes, uint32_t length, const char* name, ...);
    void (*destroy)(struct io_libecc_Input*);
    void (*printText)(struct io_libecc_Input*, struct io_libecc_Text text, int32_t ofLine, struct io_libecc_Text ofText, const char* ofInput, int fullLine);
    int32_t (*findLine)(struct io_libecc_Input*, struct io_libecc_Text text);
    struct io_libecc_Value (*attachValue)(struct io_libecc_Input*, struct io_libecc_Value value);
    const struct io_libecc_Input identity;
} extern const io_libecc_Input;
