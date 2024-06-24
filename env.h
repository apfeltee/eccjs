//
//  env.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"

enum io_libecc_env_Color
{
    io_libecc_env_black = 30,
    io_libecc_env_red = 31,
    io_libecc_env_green = 32,
    io_libecc_env_yellow = 33,
    io_libecc_env_blue = 34,
    io_libecc_env_magenta = 35,
    io_libecc_env_cyan = 36,
    io_libecc_env_white = 37,
};

enum io_libecc_env_Attribute
{
    io_libecc_env_bold = 1,
    io_libecc_env_dim = 2,
    io_libecc_env_invisible = 8,
};

extern const int io_libecc_env_print_max;

struct io_libecc_Env
{
    struct io_libecc_env_Internal* internal;
};

struct type_io_libecc_Env
{
    void (*setup)(void);
    void (*teardown)(void);
    void (*print)(const char* format, ...);
    void (*printColor)(enum io_libecc_env_Color color, enum io_libecc_env_Attribute attribute, const char* format, ...);
    void (*printError)(int typeLength, const char* type, const char* format, ...);
    void (*printWarning)(const char* format, ...);
    void (*newline)(void);
    double (*currentTime)(void);
    const struct io_libecc_Env identity;
} extern const io_libecc_Env;
