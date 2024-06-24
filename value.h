//
//  value.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "key.h"

struct io_libecc_Context;

enum io_libecc_value_Type
{

    io_libecc_value_nullType = -1 & ~0x79,
    io_libecc_value_falseType = -1 & ~0x59,
    io_libecc_value_undefinedType = 0x00,

    io_libecc_value_integerType = 0x08,
    io_libecc_value_binaryType = 0x0a,

    io_libecc_value_keyType = 0x10,
    io_libecc_value_textType = 0x12,
    io_libecc_value_charsType = 0x13,
    io_libecc_value_bufferType = 0x14,

    io_libecc_value_trueType = 0x20,

    io_libecc_value_objectType = 0x40,
    io_libecc_value_errorType = 0x41,
    io_libecc_value_functionType = 0x42,
    io_libecc_value_regexpType = 0x43,
    io_libecc_value_dateType = 0x44,
    io_libecc_value_numberType = 0x48,
    io_libecc_value_stringType = 0x50,
    io_libecc_value_booleanType = 0x60,

    io_libecc_value_hostType = 0x46,
    io_libecc_value_referenceType = 0x47,
};

enum io_libecc_value_Mask
{
    io_libecc_value_numberMask = 0x08,
    io_libecc_value_stringMask = 0x10,
    io_libecc_value_booleanMask = 0x20,
    io_libecc_value_objectMask = 0x40,
    io_libecc_value_dynamicMask = 0x41,
};

enum io_libecc_value_Flags
{
    io_libecc_value_readonly = 1 << 0,
    io_libecc_value_hidden = 1 << 1,
    io_libecc_value_sealed = 1 << 2,
    io_libecc_value_getter = 1 << 3,
    io_libecc_value_setter = 1 << 4,
    io_libecc_value_asOwn = 1 << 5,
    io_libecc_value_asData = 1 << 6,

    io_libecc_value_frozen = io_libecc_value_readonly | io_libecc_value_sealed,
    io_libecc_value_accessor = io_libecc_value_getter | io_libecc_value_setter,
};

enum io_libecc_value_hintPrimitive
{
    io_libecc_value_hintAuto = 0,
    io_libecc_value_hintString = 1,
    io_libecc_value_hintNumber = -1,
};

extern const struct io_libecc_Value io_libecc_value_none;
extern const struct io_libecc_Value io_libecc_value_undefined;
extern const struct io_libecc_Value io_libecc_value_true;
extern const struct io_libecc_Value io_libecc_value_false;
extern const struct io_libecc_Value io_libecc_value_null;

struct io_libecc_Value
{
    union
    {
        int32_t integer;
        double binary;
        char buffer[8];
        struct io_libecc_Key key;
        const struct io_libecc_Text* text;
        struct io_libecc_Chars* chars;
        struct io_libecc_Object* object;
        struct io_libecc_Error* error;
        struct io_libecc_String* string;
        struct io_libecc_RegExp* regexp;
        struct io_libecc_Number* number;
        struct io_libecc_Boolean* boolean;
        struct io_libecc_Date* date;
        struct io_libecc_Function* function;
        struct io_libecc_Value* reference;
    } data;
    struct io_libecc_Key key;
    int8_t type;
    uint8_t flags;
    uint16_t check;
};

struct type_io_libecc_Value
{
    struct io_libecc_Value (*truth)(int truth);
    struct io_libecc_Value (*integer)(int32_t integer);
    struct io_libecc_Value (*binary)(double binary);
    struct io_libecc_Value (*buffer)(const char buffer[7], uint8_t units);
    struct io_libecc_Value (*key)(struct io_libecc_Key key);
    struct io_libecc_Value (*text)(const struct io_libecc_Text* text);
    struct io_libecc_Value (*chars)(struct io_libecc_Chars* chars);
    struct io_libecc_Value (*object)(struct io_libecc_Object*);
    struct io_libecc_Value (*error)(struct io_libecc_Error*);
    struct io_libecc_Value (*string)(struct io_libecc_String*);
    struct io_libecc_Value (*regexp)(struct io_libecc_RegExp*);
    struct io_libecc_Value (*number)(struct io_libecc_Number*);
    struct io_libecc_Value (*boolean)(struct io_libecc_Boolean*);
    struct io_libecc_Value (*date)(struct io_libecc_Date*);
    struct io_libecc_Value (*function)(struct io_libecc_Function*);
    struct io_libecc_Value (*host)(struct io_libecc_Object*);
    struct io_libecc_Value (*reference)(struct io_libecc_Value*);
    int (*isPrimitive)(struct io_libecc_Value);
    int (*isBoolean)(struct io_libecc_Value);
    int (*isNumber)(struct io_libecc_Value);
    int (*isString)(struct io_libecc_Value);
    int (*isObject)(struct io_libecc_Value);
    int (*isDynamic)(struct io_libecc_Value);
    int (*isTrue)(struct io_libecc_Value);
    struct io_libecc_Value (*toPrimitive)(struct io_libecc_Context* const, struct io_libecc_Value, enum io_libecc_value_hintPrimitive);
    struct io_libecc_Value (*toBinary)(struct io_libecc_Context* const, struct io_libecc_Value);
    struct io_libecc_Value (*toInteger)(struct io_libecc_Context* const, struct io_libecc_Value);
    struct io_libecc_Value (*binaryToString)(double binary, int base);
    struct io_libecc_Value (*toString)(struct io_libecc_Context* const, struct io_libecc_Value);
    int32_t (*stringLength)(const struct io_libecc_Value*);
    const char* (*stringBytes)(const struct io_libecc_Value*);
    struct io_libecc_Text (*textOf)(const struct io_libecc_Value* string);
    struct io_libecc_Value (*toObject)(struct io_libecc_Context* const, struct io_libecc_Value);
    struct io_libecc_Value (*objectValue)(struct io_libecc_Object*);
    int (*objectIsArray)(struct io_libecc_Object*);
    struct io_libecc_Value (*toType)(struct io_libecc_Value);
    struct io_libecc_Value (*equals)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    struct io_libecc_Value (*same)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    struct io_libecc_Value (*add)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    struct io_libecc_Value (*subtract)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    struct io_libecc_Value (*less)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    struct io_libecc_Value (*more)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    struct io_libecc_Value (*lessOrEqual)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    struct io_libecc_Value (*moreOrEqual)(struct io_libecc_Context* const, struct io_libecc_Value, struct io_libecc_Value);
    const char* (*typeName)(enum io_libecc_value_Type);
    const char* (*maskName)(enum io_libecc_value_Mask);
    void (*dumpTo)(struct io_libecc_Value, FILE*);
    const struct io_libecc_Value identity;
} extern const io_libecc_Value;
