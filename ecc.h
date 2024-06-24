
//
//  global.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if __GNUC__
    #define io_libecc_ecc_noreturn __attribute__((noreturn))
#else
    #define io_libecc_ecc_noreturn
#endif

#if __GNUC__ && _WIN32 && !_MSC_VER
    /* use ebp frame */
    #define io_libecc_ecc_useframe __attribute__((optimize("no-omit-frame-pointer")))
#else
    #define io_libecc_ecc_useframe
#endif

#if(__STDC_VERSION__ < 199901L)
    #ifdef __GNUC__
        #define inline __inline__
        #define restrict __restrict__
    #else
        #define inline static
        #define restrict
    #endif
#endif

#if(__unix__ && !__MSDOS__) || (defined(__APPLE__) && defined(__MACH__))
    /* don't use signal version of long jump */
    #undef setjmp
    #define setjmp _setjmp

    #undef longjmp
    #define longjmp _longjmp
#endif

#if _WIN32
    /* supports hex */
    #define strtod strtold
#endif






struct io_libecc_text_Char
{
    uint32_t codepoint;
    uint8_t units;
};

enum io_libecc_text_Flags
{
    io_libecc_text_breakFlag = 1 << 0,
};

struct io_libecc_Text
{
    const char* bytes;
    int32_t length;
    uint8_t flags;
};

struct type_io_libecc_Text
{
    struct io_libecc_Text (*make)(const char* bytes, int32_t length);
    struct io_libecc_Text (*join)(struct io_libecc_Text from, struct io_libecc_Text to);
    struct io_libecc_text_Char (*character)(struct io_libecc_Text);
    struct io_libecc_text_Char (*nextCharacter)(struct io_libecc_Text* text);
    struct io_libecc_text_Char (*prevCharacter)(struct io_libecc_Text* text);
    void (*advance)(struct io_libecc_Text* text, int32_t units);
    uint16_t (*toUTF16Length)(struct io_libecc_Text);
    uint16_t (*toUTF16)(struct io_libecc_Text, uint16_t* wbuffer);
    char* (*toLower)(struct io_libecc_Text, char* x2buffer);
    char* (*toUpper)(struct io_libecc_Text, char* x3buffer);
    int (*isSpace)(struct io_libecc_text_Char);
    int (*isDigit)(struct io_libecc_text_Char);
    int (*isWord)(struct io_libecc_text_Char);
    int (*isLineFeed)(struct io_libecc_text_Char);
    const struct io_libecc_Text identity;
};






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
};




struct eccstate_t;

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


struct eccvalue_t
{
    union
    {
        int32_t integer;
        double binary;
        char buffer[8];
        struct io_libecc_Key key;
        const struct io_libecc_Text* text;
        struct io_libecc_Chars* chars;
        struct eccobject_t* object;
        struct io_libecc_Error* error;
        struct io_libecc_String* string;
        struct io_libecc_RegExp* regexp;
        struct io_libecc_Number* number;
        struct io_libecc_Boolean* boolean;
        struct io_libecc_Date* date;
        struct io_libecc_Function* function;
        struct eccvalue_t* reference;
    } data;
    struct io_libecc_Key key;
    int8_t type;
    uint8_t flags;
    uint16_t check;
};

struct type_io_libecc_Value
{
    struct eccvalue_t (*truth)(int truth);
    struct eccvalue_t (*integer)(int32_t integer);
    struct eccvalue_t (*binary)(double binary);
    struct eccvalue_t (*buffer)(const char buffer[7], uint8_t units);
    struct eccvalue_t (*key)(struct io_libecc_Key key);
    struct eccvalue_t (*text)(const struct io_libecc_Text* text);
    struct eccvalue_t (*chars)(struct io_libecc_Chars* chars);
    struct eccvalue_t (*object)(struct eccobject_t*);
    struct eccvalue_t (*error)(struct io_libecc_Error*);
    struct eccvalue_t (*string)(struct io_libecc_String*);
    struct eccvalue_t (*regexp)(struct io_libecc_RegExp*);
    struct eccvalue_t (*number)(struct io_libecc_Number*);
    struct eccvalue_t (*boolean)(struct io_libecc_Boolean*);
    struct eccvalue_t (*date)(struct io_libecc_Date*);
    struct eccvalue_t (*function)(struct io_libecc_Function*);
    struct eccvalue_t (*host)(struct eccobject_t*);
    struct eccvalue_t (*reference)(struct eccvalue_t*);
    int (*isPrimitive)(struct eccvalue_t);
    int (*isBoolean)(struct eccvalue_t);
    int (*isNumber)(struct eccvalue_t);
    int (*isString)(struct eccvalue_t);
    int (*isObject)(struct eccvalue_t);
    int (*isDynamic)(struct eccvalue_t);
    int (*isTrue)(struct eccvalue_t);
    struct eccvalue_t (*toPrimitive)(struct eccstate_t* const, struct eccvalue_t, enum io_libecc_value_hintPrimitive);
    struct eccvalue_t (*toBinary)(struct eccstate_t* const, struct eccvalue_t);
    struct eccvalue_t (*toInteger)(struct eccstate_t* const, struct eccvalue_t);
    struct eccvalue_t (*binaryToString)(double binary, int base);
    struct eccvalue_t (*toString)(struct eccstate_t* const, struct eccvalue_t);
    int32_t (*stringLength)(const struct eccvalue_t*);
    const char* (*stringBytes)(const struct eccvalue_t*);
    struct io_libecc_Text (*textOf)(const struct eccvalue_t* string);
    struct eccvalue_t (*toObject)(struct eccstate_t* const, struct eccvalue_t);
    struct eccvalue_t (*objectValue)(struct eccobject_t*);
    int (*objectIsArray)(struct eccobject_t*);
    struct eccvalue_t (*toType)(struct eccvalue_t);
    struct eccvalue_t (*equals)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    struct eccvalue_t (*same)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    struct eccvalue_t (*add)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    struct eccvalue_t (*subtract)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    struct eccvalue_t (*less)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    struct eccvalue_t (*more)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    struct eccvalue_t (*lessOrEqual)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    struct eccvalue_t (*moreOrEqual)(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
    const char* (*typeName)(enum io_libecc_value_Type);
    const char* (*maskName)(enum io_libecc_value_Mask);
    void (*dumpTo)(struct eccvalue_t, FILE*);
    const struct eccvalue_t identity;
};


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

struct eccstate_t
{
    const struct io_libecc_Op* ops;
    struct eccobject_t* refObject;
    struct eccobject_t* environment;
    struct eccstate_t* parent;
    struct io_libecc_Ecc* ecc;
    struct eccvalue_t this;
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
    void (*rangeError)(struct eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*referenceError)(struct eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*syntaxError)(struct eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*typeError)(struct eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*uriError)(struct eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*throw)(struct eccstate_t* const, struct eccvalue_t) __attribute__((noreturn));
    struct eccvalue_t (*callFunction)(struct eccstate_t* const, struct io_libecc_Function* function, struct eccvalue_t this, int argumentCount, ...);
    int (*argumentCount)(struct eccstate_t* const);
    struct eccvalue_t (*argument)(struct eccstate_t* const, int argumentIndex);
    void (*replaceArgument)(struct eccstate_t* const, int argumentIndex, struct eccvalue_t value);
    struct eccvalue_t (*this)(struct eccstate_t* const);
    void (*assertThisType)(struct eccstate_t* const, enum io_libecc_value_Type);
    void (*assertThisMask)(struct eccstate_t* const, enum io_libecc_value_Mask);
    void (*assertThisCoerciblePrimitive)(struct eccstate_t* const);
    void (*setText)(struct eccstate_t* const, const struct io_libecc_Text* text);
    void (*setTexts)(struct eccstate_t* const, const struct io_libecc_Text* text, const struct io_libecc_Text* textAlt);
    void (*setTextIndex)(struct eccstate_t* const, enum io_libecc_context_Index index);
    void (*setTextIndexArgument)(struct eccstate_t* const, int argument);
    struct io_libecc_Text (*textSeek)(struct eccstate_t* const);
    void (*rewindStatement)(struct eccstate_t* const);
    void (*printBacktrace)(struct eccstate_t* const context);
    struct eccobject_t* (*environmentRoot)(struct eccstate_t* const context);
    const struct eccstate_t identity;
};

enum io_libecc_chars_Flags
{
    io_libecc_chars_mark = 1 << 0,
    io_libecc_chars_asciiOnly = 1 << 1,
};

struct io_libecc_chars_Append
{
    struct io_libecc_Chars* value;
    char buffer[9];
    uint8_t units;
};

struct io_libecc_Chars
{
    int32_t length;
    int16_t referenceCount;
    uint8_t flags;
    char bytes[1];
};

struct type_io_libecc_Chars
{
    struct io_libecc_Chars* (*createVA)(int32_t length, const char* format, va_list ap);
    struct io_libecc_Chars* (*create)(const char* format, ...);
    struct io_libecc_Chars* (*createSized)(int32_t length);
    struct io_libecc_Chars* (*createWithBytes)(int32_t length, const char* bytes);
    void (*beginAppend)(struct io_libecc_chars_Append*);
    void (*append)(struct io_libecc_chars_Append*, const char* format, ...);
    void (*appendCodepoint)(struct io_libecc_chars_Append*, uint32_t cp);
    void (*appendValue)(struct io_libecc_chars_Append*, struct eccstate_t* const context, struct eccvalue_t value);
    void (*appendBinary)(struct io_libecc_chars_Append*, double binary, int base);
    void (*normalizeBinary)(struct io_libecc_chars_Append*);
    struct eccvalue_t (*endAppend)(struct io_libecc_chars_Append*);
    void (*destroy)(struct io_libecc_Chars*);
    uint8_t (*codepointLength)(uint32_t cp);
    uint8_t (*writeCodepoint)(char*, uint32_t cp);
    const struct io_libecc_Chars identity;
};

typedef enum io_libecc_object_Flags io_libecc_object_Flags;
typedef enum io_libecc_regexp_Options io_libecc_regexp_Options;
typedef enum io_libecc_function_Flags io_libecc_function_Flags;
typedef struct io_libecc_regexp_Node io_libecc_regexp_Node;

typedef struct type_io_libecc_Math type_io_libecc_Math;
typedef struct type_io_libecc_Array type_io_libecc_Array;
typedef struct type_io_libecc_Object type_io_libecc_Object;
typedef struct type_io_libecc_RegExp type_io_libecc_RegExp;
typedef struct type_io_libecc_Arguments type_io_libecc_Arguments;
typedef struct type_io_libecc_Boolean type_io_libecc_Boolean;
typedef struct type_io_libecc_Date type_io_libecc_Date;
typedef struct type_io_libecc_Error type_io_libecc_Error;
typedef struct type_io_libecc_Function type_io_libecc_Function;
typedef struct type_io_libecc_JSON type_io_libecc_JSON;
typedef struct type_io_libecc_Number type_io_libecc_Number;
typedef struct type_io_libecc_String type_io_libecc_String;
typedef struct type_io_libecc_Global type_io_libecc_Global;

typedef struct eccvalue_t (*io_libecc_native_io_libecc_Function)(struct eccstate_t* const context);


struct io_libecc_object_Type
{
    const struct io_libecc_Text* text;

    void (*mark)(struct eccobject_t*);
    void (*capture)(struct eccobject_t*);
    void (*finalize)(struct eccobject_t*);
};

enum io_libecc_object_Flags
{
    io_libecc_object_mark = 1 << 0,
    io_libecc_object_sealed = 1 << 1,
};

struct eccobject_t
{
    struct eccobject_t* prototype;
    const struct io_libecc_object_Type* type;

    union io_libecc_object_Element
    {
        struct eccvalue_t value;
    }* element;

    union io_libecc_object_Hashmap
    {
        struct eccvalue_t value;
        uint16_t slot[16];
    }* hashmap;

    uint32_t elementCount;
    uint32_t elementCapacity;
    uint16_t hashmapCount;
    uint16_t hashmapCapacity;
    int16_t referenceCount;
    uint8_t flags;
};

struct type_io_libecc_Object
{
    void (*setup)(void);
    void (*teardown)(void);
    struct eccobject_t* (*create)(struct eccobject_t* prototype);
    struct eccobject_t* (*createSized)(struct eccobject_t* prototype, uint16_t size);
    struct eccobject_t* (*createTyped)(const struct io_libecc_object_Type* type);
    struct eccobject_t* (*initialize)(struct eccobject_t* restrict, struct eccobject_t* restrict prototype);
    struct eccobject_t* (*initializeSized)(struct eccobject_t* restrict, struct eccobject_t* restrict prototype, uint16_t size);
    struct eccobject_t* (*finalize)(struct eccobject_t*);
    struct eccobject_t* (*copy)(const struct eccobject_t* original);
    void (*destroy)(struct eccobject_t*);
    struct eccvalue_t (*getMember)(struct eccstate_t* const, struct eccobject_t*, struct io_libecc_Key key);
    struct eccvalue_t (*putMember)(struct eccstate_t* const, struct eccobject_t*, struct io_libecc_Key key, struct eccvalue_t);
    struct eccvalue_t* (*member)(struct eccobject_t*, struct io_libecc_Key key, enum io_libecc_value_Flags);
    struct eccvalue_t* (*addMember)(struct eccobject_t*, struct io_libecc_Key key, struct eccvalue_t, enum io_libecc_value_Flags);
    int (*deleteMember)(struct eccobject_t*, struct io_libecc_Key key);
    struct eccvalue_t (*getElement)(struct eccstate_t* const, struct eccobject_t*, uint32_t index);
    struct eccvalue_t (*putElement)(struct eccstate_t* const, struct eccobject_t*, uint32_t index, struct eccvalue_t);
    struct eccvalue_t* (*element)(struct eccobject_t*, uint32_t index, enum io_libecc_value_Flags);
    struct eccvalue_t* (*addElement)(struct eccobject_t*, uint32_t index, struct eccvalue_t, enum io_libecc_value_Flags);
    int (*deleteElement)(struct eccobject_t*, uint32_t index);
    struct eccvalue_t (*getProperty)(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t primitive);
    struct eccvalue_t (*putProperty)(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t primitive, struct eccvalue_t);
    struct eccvalue_t* (*property)(struct eccobject_t*, struct eccvalue_t primitive, enum io_libecc_value_Flags);
    struct eccvalue_t* (*addProperty)(struct eccobject_t*, struct eccvalue_t primitive, struct eccvalue_t, enum io_libecc_value_Flags);
    int (*deleteProperty)(struct eccobject_t*, struct eccvalue_t primitive);
    struct eccvalue_t (*putValue)(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t*, struct eccvalue_t);
    struct eccvalue_t (*getValue)(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t*);
    void (*packValue)(struct eccobject_t*);
    void (*stripMap)(struct eccobject_t*);
    void (*reserveSlots)(struct eccobject_t*, uint16_t slots);
    int (*resizeElement)(struct eccobject_t*, uint32_t size);
    void (*populateElementWithCList)(struct eccobject_t*, uint32_t count, const char* list[]);
    struct eccvalue_t (*toString)(struct eccstate_t* const);
    void (*dumpTo)(struct eccobject_t*, FILE* file);
    const struct eccobject_t identity;
};



struct io_libecc_regexp_State
{
    const char* const start;
    const char* const end;
    const char** capture;
    const char** index;
    int flags;
};


enum io_libecc_regexp_Options
{
    io_libecc_regexp_allowUnicodeFlags = 1 << 0,
};

struct io_libecc_RegExp
{
    struct eccobject_t object;
    struct io_libecc_Chars* pattern;
    struct io_libecc_Chars* source;
    struct io_libecc_regexp_Node* program;
    uint8_t count;
    uint8_t global : 1;
    uint8_t ignoreCase : 1;
    uint8_t multiline : 1;
};

struct type_io_libecc_RegExp
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_RegExp* (*create)(struct io_libecc_Chars* pattern, struct io_libecc_Error**, enum io_libecc_regexp_Options);
    struct io_libecc_RegExp* (*createWith)(struct eccstate_t* context, struct eccvalue_t pattern, struct eccvalue_t flags);
    int (*matchWithState)(struct io_libecc_RegExp*, struct io_libecc_regexp_State*);
    const struct io_libecc_RegExp identity;
};



struct io_libecc_Arguments
{
    struct eccobject_t object;
};

struct type_io_libecc_Arguments
{
    void (*setup)(void);
    void (*teardown)(void);
    struct eccobject_t* (*createSized)(uint32_t size);
    struct eccobject_t* (*createWithCList)(int count, const char* list[]);
    const struct io_libecc_Arguments identity;
};


struct io_libecc_Array
{
    struct eccobject_t object;
};

struct type_io_libecc_Array
{
    void (*setup)(void);
    void (*teardown)(void);
    struct eccobject_t* (*create)(void);
    struct eccobject_t* (*createSized)(uint32_t size);
    const struct io_libecc_Array identity;
};



struct io_libecc_Boolean
{
    struct eccobject_t object;
    int truth;
};

struct type_io_libecc_Boolean
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Boolean* (*create)(int);
    const struct io_libecc_Boolean identity;
};




struct io_libecc_Date
{
    struct eccobject_t object;
    double ms;
};

struct type_io_libecc_Date
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Date* (*create)(double);
    const struct io_libecc_Date identity;
};



struct io_libecc_Error
{
    struct eccobject_t object;
    struct io_libecc_Text text;
};

struct type_io_libecc_Error
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Error* (*error)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*rangeError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*referenceError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*syntaxError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*typeError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*uriError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    void (*destroy)(struct io_libecc_Error*);
    const struct io_libecc_Error identity;
};





enum io_libecc_function_Flags
{
    io_libecc_function_needHeap = 1 << 1,
    io_libecc_function_needArguments = 1 << 2,
    io_libecc_function_useBoundThis = 1 << 3,
    io_libecc_function_strictMode = 1 << 4,
};


struct io_libecc_Function
{
    struct eccobject_t object;
    struct eccobject_t environment;
    struct eccobject_t* refObject;
    struct io_libecc_OpList* oplist;
    struct io_libecc_Function* pair;
    struct eccvalue_t boundThis;
    struct io_libecc_Text text;
    const char* name;
    int parameterCount;
    enum io_libecc_function_Flags flags;
};

struct type_io_libecc_Function
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Function* (*create)(struct eccobject_t* environment);
    struct io_libecc_Function* (*createSized)(struct eccobject_t* environment, uint32_t size);
    struct io_libecc_Function* (*createWithNative)(const io_libecc_native_io_libecc_Function native, int parameterCount);
    struct io_libecc_Function* (*copy)(struct io_libecc_Function* original);
    void (*destroy)(struct io_libecc_Function*);
    void (*addMember)(struct io_libecc_Function*, const char* name, struct eccvalue_t value, enum io_libecc_value_Flags);
    void (*addValue)(struct io_libecc_Function*, const char* name, struct eccvalue_t value, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addMethod)(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addFunction)(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addToObject)(struct eccobject_t* object, const char* name, const io_libecc_native_io_libecc_Function native, int parameterCount, enum io_libecc_value_Flags);
    void (*linkPrototype)(struct io_libecc_Function*, struct eccvalue_t prototype, enum io_libecc_value_Flags);
    void (*setupBuiltinObject)(struct io_libecc_Function**,
                               const io_libecc_native_io_libecc_Function,
                               int parameterCount,
                               struct eccobject_t**,
                               struct eccvalue_t prototype,
                               const struct io_libecc_object_Type* type);
    struct eccvalue_t (*accessor)(const io_libecc_native_io_libecc_Function getter, const io_libecc_native_io_libecc_Function setter);
    const struct io_libecc_Function identity;
};

struct io_libecc_JSON
{
    char empty;
};

struct type_io_libecc_JSON
{
    void (*setup)(void);
    void (*teardown)(void);
    const struct io_libecc_JSON identity;
};



struct io_libecc_Math
{
    char empty;
};

struct type_io_libecc_Math
{
    void (*setup)(void);
    void (*teardown)(void);
    const struct io_libecc_Math identity;
};



struct io_libecc_Number
{
    struct eccobject_t object;
    double value;
};

struct type_io_libecc_Number
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Number* (*create)(double);
    const struct io_libecc_Number identity;
};


struct io_libecc_String
{
    struct eccobject_t object;
    struct io_libecc_Chars* value;
};

struct type_io_libecc_String
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_String* (*create)(struct io_libecc_Chars*);
    struct eccvalue_t (*valueAtIndex)(struct io_libecc_String*, int32_t index);
    struct io_libecc_Text (*textAtIndex)(const char* chars, int32_t length, int32_t index, int enableReverse);
    int32_t (*unitIndex)(const char* chars, int32_t max, int32_t unit);
    const struct io_libecc_String identity;
};




struct type_io_libecc_Global
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Function* (*create)(void);
    const struct {} identity;
};




#include <stdbool.h>

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
};



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
    struct eccvalue_t* attached;
    uint16_t attachedCount;
};

struct type_io_libecc_Input
{
    struct io_libecc_Input* (*createFromFile)(const char* filename);
    struct io_libecc_Input* (*createFromBytes)(const char* bytes, uint32_t length, const char* name, ...);
    void (*destroy)(struct io_libecc_Input*);
    void (*printText)(struct io_libecc_Input*, struct io_libecc_Text text, int32_t ofLine, struct io_libecc_Text ofText, const char* ofInput, int fullLine);
    int32_t (*findLine)(struct io_libecc_Input*, struct io_libecc_Text text);
    struct eccvalue_t (*attachValue)(struct io_libecc_Input*, struct eccvalue_t value);
    const struct io_libecc_Input identity;
};



enum io_libecc_ecc_EvalFlags
{
    io_libecc_ecc_sloppyMode = 0x1,
    io_libecc_ecc_primitiveResult = 0x2,
    io_libecc_ecc_stringResult = 0x6,
};


struct io_libecc_Ecc
{
    jmp_buf* envList;
    uint16_t envCount;
    uint16_t envCapacity;
    struct io_libecc_Function* global;
    struct eccvalue_t result;
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
    void (*addValue)(struct io_libecc_Ecc*, const char* name, struct eccvalue_t value, enum io_libecc_value_Flags);
    void (*addFunction)(struct io_libecc_Ecc*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    int (*evalInput)(struct io_libecc_Ecc*, struct io_libecc_Input*, enum io_libecc_ecc_EvalFlags);
    void (*evalInputWithContext)(struct io_libecc_Ecc*, struct io_libecc_Input*, struct eccstate_t* context);
    jmp_buf* (*pushEnv)(struct io_libecc_Ecc*);
    void (*popEnv)(struct io_libecc_Ecc*);
    void (*jmpEnv)(struct io_libecc_Ecc*, struct eccvalue_t value) __attribute__((noreturn));
    void (*fatal)(const char* format, ...) __attribute__((noreturn));
    struct io_libecc_Input* (*findInput)(struct io_libecc_Ecc* self, struct io_libecc_Text text);
    void (*printTextInput)(struct io_libecc_Ecc*, struct io_libecc_Text text, int fullLine);
    void (*garbageCollect)(struct io_libecc_Ecc*);
    const struct io_libecc_Ecc identity;
};



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
    struct eccvalue_t value;
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
    struct eccvalue_t (*scanBinary)(struct io_libecc_Text text, enum io_libecc_lexer_ScanFlags);
    struct eccvalue_t (*scanInteger)(struct io_libecc_Text text, int base, enum io_libecc_lexer_ScanFlags);
    uint32_t (*scanElement)(struct io_libecc_Text text);
    uint8_t (*uint8Hex)(char a, char b);
    uint16_t (*uint16Hex)(char a, char b, char c, char d);
    const struct io_libecc_Lexer identity;
};



struct io_libecc_Op
{
    io_libecc_native_io_libecc_Function native;
    struct eccvalue_t value;
    struct io_libecc_Text text;
};

struct type_io_libecc_Op
{
    struct io_libecc_Op (*make)(const io_libecc_native_io_libecc_Function native, struct eccvalue_t value, struct io_libecc_Text text);
    const char* (*toChars)(const io_libecc_native_io_libecc_Function native);
    struct eccvalue_t (*callFunctionArguments)(struct eccstate_t* const,
                                                    enum io_libecc_context_Offset,
                                                    struct io_libecc_Function* function,
                                                    struct eccvalue_t this,
                                                    struct eccobject_t* arguments);
    struct eccvalue_t (*callFunctionVA)(
    struct eccstate_t* const, enum io_libecc_context_Offset, struct io_libecc_Function* function, struct eccvalue_t this, int argumentCount, va_list ap);
    struct eccvalue_t (*noop)(struct eccstate_t* const);
    struct eccvalue_t (*value)(struct eccstate_t* const);
    struct eccvalue_t (*valueConstRef)(struct eccstate_t* const);
    struct eccvalue_t (*text)(struct eccstate_t* const);
    struct eccvalue_t (*function)(struct eccstate_t* const);
    struct eccvalue_t (*object)(struct eccstate_t* const);
    struct eccvalue_t (*array)(struct eccstate_t* const);
    struct eccvalue_t (*regexp)(struct eccstate_t* const);
    struct eccvalue_t (*this)(struct eccstate_t* const);
    struct eccvalue_t (*createLocalRef)(struct eccstate_t* const);
    struct eccvalue_t (*getLocalRefOrNull)(struct eccstate_t* const);
    struct eccvalue_t (*getLocalRef)(struct eccstate_t* const);
    struct eccvalue_t (*getLocal)(struct eccstate_t* const);
    struct eccvalue_t (*setLocal)(struct eccstate_t* const);
    struct eccvalue_t (*deleteLocal)(struct eccstate_t* const);
    struct eccvalue_t (*getLocalSlotRef)(struct eccstate_t* const);
    struct eccvalue_t (*getLocalSlot)(struct eccstate_t* const);
    struct eccvalue_t (*setLocalSlot)(struct eccstate_t* const);
    struct eccvalue_t (*deleteLocalSlot)(struct eccstate_t* const);
    struct eccvalue_t (*getParentSlotRef)(struct eccstate_t* const);
    struct eccvalue_t (*getParentSlot)(struct eccstate_t* const);
    struct eccvalue_t (*setParentSlot)(struct eccstate_t* const);
    struct eccvalue_t (*deleteParentSlot)(struct eccstate_t* const);
    struct eccvalue_t (*getMemberRef)(struct eccstate_t* const);
    struct eccvalue_t (*getMember)(struct eccstate_t* const);
    struct eccvalue_t (*setMember)(struct eccstate_t* const);
    struct eccvalue_t (*callMember)(struct eccstate_t* const);
    struct eccvalue_t (*deleteMember)(struct eccstate_t* const);
    struct eccvalue_t (*getPropertyRef)(struct eccstate_t* const);
    struct eccvalue_t (*getProperty)(struct eccstate_t* const);
    struct eccvalue_t (*setProperty)(struct eccstate_t* const);
    struct eccvalue_t (*callProperty)(struct eccstate_t* const);
    struct eccvalue_t (*deleteProperty)(struct eccstate_t* const);
    struct eccvalue_t (*pushEnvironment)(struct eccstate_t* const);
    struct eccvalue_t (*popEnvironment)(struct eccstate_t* const);
    struct eccvalue_t (*exchange)(struct eccstate_t* const);
    struct eccvalue_t (*typeOf)(struct eccstate_t* const);
    struct eccvalue_t (*equal)(struct eccstate_t* const);
    struct eccvalue_t (*notEqual)(struct eccstate_t* const);
    struct eccvalue_t (*identical)(struct eccstate_t* const);
    struct eccvalue_t (*notIdentical)(struct eccstate_t* const);
    struct eccvalue_t (*less)(struct eccstate_t* const);
    struct eccvalue_t (*lessOrEqual)(struct eccstate_t* const);
    struct eccvalue_t (*more)(struct eccstate_t* const);
    struct eccvalue_t (*moreOrEqual)(struct eccstate_t* const);
    struct eccvalue_t (*instanceOf)(struct eccstate_t* const);
    struct eccvalue_t (*in)(struct eccstate_t* const);
    struct eccvalue_t (*add)(struct eccstate_t* const);
    struct eccvalue_t (*minus)(struct eccstate_t* const);
    struct eccvalue_t (*multiply)(struct eccstate_t* const);
    struct eccvalue_t (*divide)(struct eccstate_t* const);
    struct eccvalue_t (*modulo)(struct eccstate_t* const);
    struct eccvalue_t (*leftShift)(struct eccstate_t* const);
    struct eccvalue_t (*rightShift)(struct eccstate_t* const);
    struct eccvalue_t (*unsignedRightShift)(struct eccstate_t* const);
    struct eccvalue_t (*bitwiseAnd)(struct eccstate_t* const);
    struct eccvalue_t (*bitwiseXor)(struct eccstate_t* const);
    struct eccvalue_t (*bitwiseOr)(struct eccstate_t* const);
    struct eccvalue_t (*logicalAnd)(struct eccstate_t* const);
    struct eccvalue_t (*logicalOr)(struct eccstate_t* const);
    struct eccvalue_t (*positive)(struct eccstate_t* const);
    struct eccvalue_t (*negative)(struct eccstate_t* const);
    struct eccvalue_t (*invert)(struct eccstate_t* const);
    struct eccvalue_t (*not )(struct eccstate_t* const);
    struct eccvalue_t (*construct)(struct eccstate_t* const);
    struct eccvalue_t (*call)(struct eccstate_t* const);
    struct eccvalue_t (*eval)(struct eccstate_t* const);
    struct eccvalue_t (*incrementRef)(struct eccstate_t* const);
    struct eccvalue_t (*decrementRef)(struct eccstate_t* const);
    struct eccvalue_t (*postIncrementRef)(struct eccstate_t* const);
    struct eccvalue_t (*postDecrementRef)(struct eccstate_t* const);
    struct eccvalue_t (*addAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*minusAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*multiplyAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*divideAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*moduloAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*leftShiftAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*rightShiftAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*unsignedRightShiftAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*bitAndAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*bitXorAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*bitOrAssignRef)(struct eccstate_t* const);
    struct eccvalue_t (*debugger)(struct eccstate_t* const);
    struct eccvalue_t (*try)(struct eccstate_t* const);
    struct eccvalue_t (*throw)(struct eccstate_t* const);
    struct eccvalue_t (*with)(struct eccstate_t* const);
    struct eccvalue_t (*next)(struct eccstate_t* const);
    struct eccvalue_t (*nextIf)(struct eccstate_t* const);
    struct eccvalue_t (*autoreleaseExpression)(struct eccstate_t* const);
    struct eccvalue_t (*autoreleaseDiscard)(struct eccstate_t* const);
    struct eccvalue_t (*expression)(struct eccstate_t* const);
    struct eccvalue_t (*discard)(struct eccstate_t* const);
    struct eccvalue_t (*discardN)(struct eccstate_t* const);
    struct eccvalue_t (*jump)(struct eccstate_t* const);
    struct eccvalue_t (*jumpIf)(struct eccstate_t* const);
    struct eccvalue_t (*jumpIfNot)(struct eccstate_t* const);
    struct eccvalue_t (*repopulate)(struct eccstate_t* const);
    struct eccvalue_t (*result)(struct eccstate_t* const);
    struct eccvalue_t (*resultVoid)(struct eccstate_t* const);
    struct eccvalue_t (*switchOp)(struct eccstate_t* const);
    struct eccvalue_t (*breaker)(struct eccstate_t* const);
    struct eccvalue_t (*iterate)(struct eccstate_t* const);
    struct eccvalue_t (*iterateLessRef)(struct eccstate_t* const);
    struct eccvalue_t (*iterateMoreRef)(struct eccstate_t* const);
    struct eccvalue_t (*iterateLessOrEqualRef)(struct eccstate_t* const);
    struct eccvalue_t (*iterateMoreOrEqualRef)(struct eccstate_t* const);
    struct eccvalue_t (*iterateInRef)(struct eccstate_t* const);
    const struct io_libecc_Op identity;
};


struct io_libecc_OpList
{
    uint32_t count;
    struct io_libecc_Op* ops;
};

struct type_io_libecc_OpList
{
    struct io_libecc_OpList* (*create)(const io_libecc_native_io_libecc_Function native, struct eccvalue_t value, struct io_libecc_Text text);
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
    void (*optimizeWithEnvironment)(struct io_libecc_OpList*, struct eccobject_t* environment, uint32_t index);
    void (*dumpTo)(struct io_libecc_OpList*, FILE* file);
    struct io_libecc_Text (*text)(struct io_libecc_OpList* oplist);
    const struct io_libecc_OpList identity;
};


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
    struct eccobject_t* global;
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
    struct io_libecc_Function* (*parseWithEnvironment)(struct io_libecc_Parser* const, struct eccobject_t* environment, struct eccobject_t* global);
    const struct io_libecc_Parser identity;
};


struct io_libecc_Pool
{
    struct io_libecc_Function** functionList;
    uint32_t functionCount;
    uint32_t functionCapacity;
    struct eccobject_t** objectList;
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
    void (*addObject)(struct eccobject_t* object);
    void (*addChars)(struct io_libecc_Chars* chars);
    void (*unmarkAll)(void);
    void (*markValue)(struct eccvalue_t value);
    void (*markObject)(struct eccobject_t* object);
    void (*collectUnmarked)(void);
    void (*collectUnreferencedFromIndices)(uint32_t indices[3]);
    void (*unreferenceFromIndices)(uint32_t indices[3]);
    void (*getIndices)(uint32_t indices[3]);
    const struct io_libecc_Pool identity;
};

extern const struct io_libecc_Text io_libecc_text_undefined;
extern const struct io_libecc_Text io_libecc_text_null;
extern const struct io_libecc_Text io_libecc_text_false;
extern const struct io_libecc_Text io_libecc_text_true;
extern const struct io_libecc_Text io_libecc_text_boolean;
extern const struct io_libecc_Text io_libecc_text_number;
extern const struct io_libecc_Text io_libecc_text_string;
extern const struct io_libecc_Text io_libecc_text_object;
extern const struct io_libecc_Text io_libecc_text_function;
extern const struct io_libecc_Text io_libecc_text_zero;
extern const struct io_libecc_Text io_libecc_text_one;
extern const struct io_libecc_Text io_libecc_text_nan;
extern const struct io_libecc_Text io_libecc_text_infinity;
extern const struct io_libecc_Text io_libecc_text_negativeInfinity;
extern const struct io_libecc_Text io_libecc_text_nativeCode;
extern const struct io_libecc_Text io_libecc_text_empty;
extern const struct io_libecc_Text io_libecc_text_emptyRegExp;
extern const struct io_libecc_Text io_libecc_text_nullType;
extern const struct io_libecc_Text io_libecc_text_undefinedType;
extern const struct io_libecc_Text io_libecc_text_objectType;
extern const struct io_libecc_Text io_libecc_text_errorType;
extern const struct io_libecc_Text io_libecc_text_arrayType;
extern const struct io_libecc_Text io_libecc_text_stringType;
extern const struct io_libecc_Text io_libecc_text_regexpType;
extern const struct io_libecc_Text io_libecc_text_numberType;
extern const struct io_libecc_Text io_libecc_text_booleanType;
extern const struct io_libecc_Text io_libecc_text_dateType;
extern const struct io_libecc_Text io_libecc_text_functionType;
extern const struct io_libecc_Text io_libecc_text_argumentsType;
extern const struct io_libecc_Text io_libecc_text_mathType;
extern const struct io_libecc_Text io_libecc_text_jsonType;
extern const struct io_libecc_Text io_libecc_text_globalType;
extern const struct io_libecc_Text io_libecc_text_errorName;
extern const struct io_libecc_Text io_libecc_text_rangeErrorName;
extern const struct io_libecc_Text io_libecc_text_referenceErrorName;
extern const struct io_libecc_Text io_libecc_text_syntaxErrorName;
extern const struct io_libecc_Text io_libecc_text_typeErrorName;
extern const struct io_libecc_Text io_libecc_text_uriErrorName;
extern const struct io_libecc_Text io_libecc_text_inputErrorName;
extern const struct io_libecc_Text io_libecc_text_evalErrorName;
extern const struct type_io_libecc_Text io_libecc_Text;
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
extern const struct type_io_libecc_Key io_libecc_Key;
extern const struct eccvalue_t io_libecc_value_none;
extern const struct eccvalue_t io_libecc_value_undefined;
extern const struct eccvalue_t io_libecc_value_true;
extern const struct eccvalue_t io_libecc_value_false;
extern const struct eccvalue_t io_libecc_value_null;
extern const struct type_io_libecc_Value io_libecc_Value;
extern const struct type_io_libecc_Context io_libecc_Context;
extern const struct type_io_libecc_Chars io_libecc_Chars;
extern const struct type_io_libecc_Object io_libecc_Object;
extern const struct type_io_libecc_RegExp io_libecc_RegExp;
extern const struct type_io_libecc_Arguments io_libecc_Arguments;
extern const struct type_io_libecc_Array io_libecc_Array;
extern const struct type_io_libecc_Boolean io_libecc_Boolean;
extern const struct type_io_libecc_Date io_libecc_Date;
extern const struct type_io_libecc_Error io_libecc_Error;
extern const struct type_io_libecc_Function io_libecc_Function;
extern const struct type_io_libecc_JSON io_libecc_JSON;
extern const struct type_io_libecc_Math io_libecc_Math;
extern const struct type_io_libecc_Number io_libecc_Number;
extern const struct type_io_libecc_String io_libecc_String;
extern const struct type_io_libecc_Global io_libecc_Global;
extern const uint32_t io_libecc_object_ElementMax;
extern struct eccobject_t* io_libecc_object_prototype;
extern struct io_libecc_Function* io_libecc_object_constructor;
extern const struct io_libecc_object_Type io_libecc_object_type;
extern struct eccobject_t* io_libecc_regexp_prototype;
extern struct io_libecc_Function* io_libecc_regexp_constructor;
extern const struct io_libecc_object_Type io_libecc_regexp_type;
extern struct eccobject_t* io_libecc_arguments_prototype;
extern const struct io_libecc_object_Type io_libecc_arguments_type;
extern struct eccobject_t* io_libecc_array_prototype;
extern struct io_libecc_Function* io_libecc_array_constructor;
extern const struct io_libecc_object_Type io_libecc_array_type;
extern struct eccobject_t* io_libecc_boolean_prototype;
extern struct io_libecc_Function* io_libecc_boolean_constructor;
extern const struct io_libecc_object_Type io_libecc_boolean_type;
extern struct eccobject_t* io_libecc_date_prototype;
extern struct io_libecc_Function* io_libecc_date_constructor;
extern const struct io_libecc_object_Type io_libecc_date_type;
extern struct eccobject_t* io_libecc_error_prototype;
extern struct eccobject_t* io_libecc_error_rangePrototype;
extern struct eccobject_t* io_libecc_error_referencePrototype;
extern struct eccobject_t* io_libecc_error_syntaxPrototype;
extern struct eccobject_t* io_libecc_error_typePrototype;
extern struct eccobject_t* io_libecc_error_uriPrototype;
extern struct eccobject_t* io_libecc_error_evalPrototype;
extern struct io_libecc_Function* io_libecc_error_constructor;
extern struct io_libecc_Function* io_libecc_error_rangeConstructor;
extern struct io_libecc_Function* io_libecc_error_referenceConstructor;
extern struct io_libecc_Function* io_libecc_error_syntaxConstructor;
extern struct io_libecc_Function* io_libecc_error_typeConstructor;
extern struct io_libecc_Function* io_libecc_error_uriConstructor;
extern struct io_libecc_Function* io_libecc_error_evalConstructor;
extern const struct io_libecc_object_Type io_libecc_error_type;
extern struct eccobject_t* io_libecc_function_prototype;
extern struct io_libecc_Function* io_libecc_function_constructor;
extern const struct io_libecc_object_Type io_libecc_function_type;
extern struct eccobject_t* io_libecc_json_object;
extern const struct io_libecc_object_Type io_libecc_json_type;
extern struct eccobject_t* io_libecc_math_object;
extern const struct io_libecc_object_Type io_libecc_math_type;
extern struct eccobject_t* io_libecc_number_prototype;
extern struct io_libecc_Function* io_libecc_number_constructor;
extern const struct io_libecc_object_Type io_libecc_number_type;
extern struct eccobject_t* io_libecc_string_prototype;
extern struct io_libecc_Function* io_libecc_string_constructor;
extern const struct io_libecc_object_Type io_libecc_string_type;
extern const struct io_libecc_object_Type io_libecc_global_type;
extern const int io_libecc_env_print_max;
extern const struct type_io_libecc_Env io_libecc_Env;
extern const struct type_io_libecc_Input io_libecc_Input;
extern uint32_t io_libecc_ecc_version;
extern const struct type_io_libecc_Ecc io_libecc_Ecc;
extern const struct type_io_libecc_Lexer io_libecc_Lexer;
extern const struct type_io_libecc_Op io_libecc_Op;
extern const struct type_io_libecc_OpList io_libecc_OpList;
extern const struct type_io_libecc_Parser io_libecc_Parser;
extern const struct type_io_libecc_Pool io_libecc_Pool;




