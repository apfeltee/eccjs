
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




typedef struct eccstate_t eccstate_t;
typedef struct eccobject_t eccobject_t;
typedef struct eccvalue_t eccvalue_t;
typedef struct ecctextchar_t ecctextchar_t;
typedef struct ecctextstring_t ecctextstring_t;
typedef struct eccastlexer_t eccastlexer_t;
typedef struct eccastparser_t eccastparser_t;

typedef enum eccasttoktype_t eccasttoktype_t;
typedef enum eccvaltype_t eccvaltype_t;
typedef enum io_libecc_object_Flags io_libecc_object_Flags;
typedef enum io_libecc_regexp_Options io_libecc_regexp_Options;
typedef enum io_libecc_function_Flags io_libecc_function_Flags;

typedef struct eccioinput_t eccioinput_t;
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

typedef eccvalue_t (*io_libecc_native_io_libecc_Function)(eccstate_t* const context);

struct ecctextchar_t
{
    uint32_t codepoint;
    uint8_t units;
};

enum ecctextflags_t
{
    ECC_TEXTFLAG_BREAKFLAG = 1 << 0,
};

struct ecctextstring_t
{
    const char* bytes;
    int32_t length;
    uint8_t flags;
};

struct type_io_libecc_Text
{
    ecctextstring_t (*make)(const char* bytes, int32_t length);
    ecctextstring_t (*join)(ecctextstring_t from, ecctextstring_t to);
    ecctextchar_t (*character)(ecctextstring_t);
    ecctextchar_t (*nextCharacter)(ecctextstring_t* text);
    ecctextchar_t (*prevCharacter)(ecctextstring_t* text);
    void (*advance)(ecctextstring_t* text, int32_t units);
    uint16_t (*toUTF16Length)(ecctextstring_t);
    uint16_t (*toUTF16)(ecctextstring_t, uint16_t* wbuffer);
    char* (*toLower)(ecctextstring_t, char* x2buffer);
    char* (*toUpper)(ecctextstring_t, char* x3buffer);
    int (*isSpace)(ecctextchar_t);
    int (*isDigit)(ecctextchar_t);
    int (*isWord)(ecctextchar_t);
    int (*isLineFeed)(ecctextchar_t);
    const ecctextstring_t identity;
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
    struct io_libecc_Key (*makeWithText)(const ecctextstring_t text, enum io_libecc_key_Flags flags);
    struct io_libecc_Key (*search)(const ecctextstring_t text);
    int (*isEqual)(struct io_libecc_Key, struct io_libecc_Key);
    const ecctextstring_t* (*textOf)(struct io_libecc_Key);
    void (*dumpTo)(struct io_libecc_Key, FILE*);
    const struct io_libecc_Key identity;
};

enum eccvaltype_t
{

    ECC_VALTYPE_NULL = -1 & ~0x79,
    ECC_VALTYPE_FALSE = -1 & ~0x59,
    ECC_VALTYPE_UNDEFINED = 0x00,

    ECC_VALTYPE_INTEGER = 0x08,
    ECC_VALTYPE_BINARY = 0x0a,

    ECC_VALTYPE_KEY = 0x10,
    ECC_VALTYPE_TEXT = 0x12,
    ECC_VALTYPE_CHARS = 0x13,
    ECC_VALTYPE_BUFFER = 0x14,

    ECC_VALTYPE_TRUE = 0x20,

    ECC_VALTYPE_OBJECT = 0x40,
    ECC_VALTYPE_ERROR = 0x41,
    ECC_VALTYPE_FUNCTION = 0x42,
    ECC_VALTYPE_REGEXP = 0x43,
    ECC_VALTYPE_DATE = 0x44,
    ECC_VALTYPE_NUMBER = 0x48,
    ECC_VALTYPE_STRING = 0x50,
    ECC_VALTYPE_BOOLEAN = 0x60,

    ECC_VALTYPE_HOST = 0x46,
    ECC_VALTYPE_REFERENCE = 0x47,
};

enum io_libecc_value_Mask
{
    ECC_VALMASK_NUMBER = 0x08,
    ECC_VALMASK_STRING = 0x10,
    ECC_VALMASK_BOOLEAN = 0x20,
    ECC_VALMASK_OBJECT = 0x40,
    ECC_VALMASK_DYNAMIC = 0x41,
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
        const ecctextstring_t* text;
        struct io_libecc_Chars* chars;
        eccobject_t* object;
        struct io_libecc_Error* error;
        struct io_libecc_String* string;
        struct io_libecc_RegExp* regexp;
        struct io_libecc_Number* number;
        struct io_libecc_Boolean* boolean;
        struct io_libecc_Date* date;
        struct io_libecc_Function* function;
        eccvalue_t* reference;
    } data;
    struct io_libecc_Key key;
    eccvaltype_t type;
    uint8_t flags;
    uint16_t check;
};

struct type_io_libecc_Value
{
    eccvalue_t (*truth)(int truth);
    eccvalue_t (*integer)(int32_t integer);
    eccvalue_t (*binary)(double binary);
    eccvalue_t (*buffer)(const char buffer[7], uint8_t units);
    eccvalue_t (*key)(struct io_libecc_Key key);
    eccvalue_t (*text)(const ecctextstring_t* text);
    eccvalue_t (*chars)(struct io_libecc_Chars* chars);
    eccvalue_t (*object)(eccobject_t*);
    eccvalue_t (*error)(struct io_libecc_Error*);
    eccvalue_t (*string)(struct io_libecc_String*);
    eccvalue_t (*regexp)(struct io_libecc_RegExp*);
    eccvalue_t (*number)(struct io_libecc_Number*);
    eccvalue_t (*boolean)(struct io_libecc_Boolean*);
    eccvalue_t (*date)(struct io_libecc_Date*);
    eccvalue_t (*function)(struct io_libecc_Function*);
    eccvalue_t (*host)(eccobject_t*);
    eccvalue_t (*reference)(eccvalue_t*);
    int (*isPrimitive)(eccvalue_t);
    int (*isBoolean)(eccvalue_t);
    int (*isNumber)(eccvalue_t);
    int (*isString)(eccvalue_t);
    int (*isObject)(eccvalue_t);
    int (*isDynamic)(eccvalue_t);
    int (*isTrue)(eccvalue_t);
    eccvalue_t (*toPrimitive)(eccstate_t* const, eccvalue_t, enum io_libecc_value_hintPrimitive);
    eccvalue_t (*toBinary)(eccstate_t* const, eccvalue_t);
    eccvalue_t (*toInteger)(eccstate_t* const, eccvalue_t);
    eccvalue_t (*binaryToString)(double binary, int base);
    eccvalue_t (*toString)(eccstate_t* const, eccvalue_t);
    int32_t (*stringLength)(const eccvalue_t*);
    const char* (*stringBytes)(const eccvalue_t*);
    ecctextstring_t (*textOf)(const eccvalue_t* string);
    eccvalue_t (*toObject)(eccstate_t* const, eccvalue_t);
    eccvalue_t (*objectValue)(eccobject_t*);
    int (*objectIsArray)(eccobject_t*);
    eccvalue_t (*toType)(eccvalue_t);
    eccvalue_t (*equals)(eccstate_t* const, eccvalue_t, eccvalue_t);
    eccvalue_t (*same)(eccstate_t* const, eccvalue_t, eccvalue_t);
    eccvalue_t (*add)(eccstate_t* const, eccvalue_t, eccvalue_t);
    eccvalue_t (*subtract)(eccstate_t* const, eccvalue_t, eccvalue_t);
    eccvalue_t (*less)(eccstate_t* const, eccvalue_t, eccvalue_t);
    eccvalue_t (*more)(eccstate_t* const, eccvalue_t, eccvalue_t);
    eccvalue_t (*lessOrEqual)(eccstate_t* const, eccvalue_t, eccvalue_t);
    eccvalue_t (*moreOrEqual)(eccstate_t* const, eccvalue_t, eccvalue_t);
    const char* (*typeName)(eccvaltype_t);
    const char* (*maskName)(enum io_libecc_value_Mask);
    void (*dumpTo)(eccvalue_t, FILE*);
    const eccvalue_t identity;
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
    eccobject_t* refObject;
    eccobject_t* environment;
    eccstate_t* parent;
    struct ECCNSScript* ecc;
    eccvalue_t this;
    const ecctextstring_t* text;
    const ecctextstring_t* textAlt;
    const ecctextstring_t* textCall;
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
    void (*rangeError)(eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*referenceError)(eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*syntaxError)(eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*typeError)(eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*uriError)(eccstate_t* const, struct io_libecc_Chars*) __attribute__((noreturn));
    void (*throw)(eccstate_t* const, eccvalue_t) __attribute__((noreturn));
    eccvalue_t (*callFunction)(eccstate_t* const, struct io_libecc_Function* function, eccvalue_t this, int argumentCount, ...);
    int (*argumentCount)(eccstate_t* const);
    eccvalue_t (*argument)(eccstate_t* const, int argumentIndex);
    void (*replaceArgument)(eccstate_t* const, int argumentIndex, eccvalue_t value);
    eccvalue_t (*this)(eccstate_t* const);
    void (*assertThisType)(eccstate_t* const, eccvaltype_t);
    void (*assertThisMask)(eccstate_t* const, enum io_libecc_value_Mask);
    void (*assertThisCoerciblePrimitive)(eccstate_t* const);
    void (*setText)(eccstate_t* const, const ecctextstring_t* text);
    void (*setTexts)(eccstate_t* const, const ecctextstring_t* text, const ecctextstring_t* textAlt);
    void (*setTextIndex)(eccstate_t* const, enum io_libecc_context_Index index);
    void (*setTextIndexArgument)(eccstate_t* const, int argument);
    ecctextstring_t (*textSeek)(eccstate_t* const);
    void (*rewindStatement)(eccstate_t* const);
    void (*printBacktrace)(eccstate_t* const context);
    eccobject_t* (*environmentRoot)(eccstate_t* const context);
    const eccstate_t identity;
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
    void (*appendValue)(struct io_libecc_chars_Append*, eccstate_t* const context, eccvalue_t value);
    void (*appendBinary)(struct io_libecc_chars_Append*, double binary, int base);
    void (*normalizeBinary)(struct io_libecc_chars_Append*);
    eccvalue_t (*endAppend)(struct io_libecc_chars_Append*);
    void (*destroy)(struct io_libecc_Chars*);
    uint8_t (*codepointLength)(uint32_t cp);
    uint8_t (*writeCodepoint)(char*, uint32_t cp);
    const struct io_libecc_Chars identity;
};


struct io_libecc_object_Type
{
    const ecctextstring_t* text;
    void (*mark)(eccobject_t*);
    void (*capture)(eccobject_t*);
    void (*finalize)(eccobject_t*);
};

enum io_libecc_object_Flags
{
    io_libecc_object_mark = 1 << 0,
    io_libecc_object_sealed = 1 << 1,
};

union io_libecc_object_Element
{
    eccvalue_t value;
};

union io_libecc_object_Hashmap
{
    eccvalue_t value;
    uint16_t slot[16];
};

struct eccobject_t
{
    eccobject_t* prototype;
    const struct io_libecc_object_Type* type;
    union io_libecc_object_Element* element;
    union io_libecc_object_Hashmap* hashmap;
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
    eccobject_t* (*create)(eccobject_t* prototype);
    eccobject_t* (*createSized)(eccobject_t* prototype, uint16_t size);
    eccobject_t* (*createTyped)(const struct io_libecc_object_Type* type);
    eccobject_t* (*initialize)(eccobject_t* restrict, eccobject_t* restrict prototype);
    eccobject_t* (*initializeSized)(eccobject_t* restrict, eccobject_t* restrict prototype, uint16_t size);
    eccobject_t* (*finalize)(eccobject_t*);
    eccobject_t* (*copy)(const eccobject_t* original);
    void (*destroy)(eccobject_t*);
    eccvalue_t (*getMember)(eccstate_t* const, eccobject_t*, struct io_libecc_Key key);
    eccvalue_t (*putMember)(eccstate_t* const, eccobject_t*, struct io_libecc_Key key, eccvalue_t);
    eccvalue_t* (*member)(eccobject_t*, struct io_libecc_Key key, enum io_libecc_value_Flags);
    eccvalue_t* (*addMember)(eccobject_t*, struct io_libecc_Key key, eccvalue_t, enum io_libecc_value_Flags);
    int (*deleteMember)(eccobject_t*, struct io_libecc_Key key);
    eccvalue_t (*getElement)(eccstate_t* const, eccobject_t*, uint32_t index);
    eccvalue_t (*putElement)(eccstate_t* const, eccobject_t*, uint32_t index, eccvalue_t);
    eccvalue_t* (*element)(eccobject_t*, uint32_t index, enum io_libecc_value_Flags);
    eccvalue_t* (*addElement)(eccobject_t*, uint32_t index, eccvalue_t, enum io_libecc_value_Flags);
    int (*deleteElement)(eccobject_t*, uint32_t index);
    eccvalue_t (*getProperty)(eccstate_t* const, eccobject_t*, eccvalue_t primitive);
    eccvalue_t (*putProperty)(eccstate_t* const, eccobject_t*, eccvalue_t primitive, eccvalue_t);
    eccvalue_t* (*property)(eccobject_t*, eccvalue_t primitive, enum io_libecc_value_Flags);
    eccvalue_t* (*addProperty)(eccobject_t*, eccvalue_t primitive, eccvalue_t, enum io_libecc_value_Flags);
    int (*deleteProperty)(eccobject_t*, eccvalue_t primitive);
    eccvalue_t (*putValue)(eccstate_t* const, eccobject_t*, eccvalue_t*, eccvalue_t);
    eccvalue_t (*getValue)(eccstate_t* const, eccobject_t*, eccvalue_t*);
    void (*packValue)(eccobject_t*);
    void (*stripMap)(eccobject_t*);
    void (*reserveSlots)(eccobject_t*, uint16_t slots);
    int (*resizeElement)(eccobject_t*, uint32_t size);
    void (*populateElementWithCList)(eccobject_t*, uint32_t count, const char* list[]);
    eccvalue_t (*toString)(eccstate_t* const);
    void (*dumpTo)(eccobject_t*, FILE* file);
    const eccobject_t identity;
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
    eccobject_t object;
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
    struct io_libecc_RegExp* (*createWith)(eccstate_t* context, eccvalue_t pattern, eccvalue_t flags);
    int (*matchWithState)(struct io_libecc_RegExp*, struct io_libecc_regexp_State*);
    const struct io_libecc_RegExp identity;
};



struct eccargumentsidentity_t
{
    eccobject_t object;
};

struct type_io_libecc_Arguments
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobject_t* (*createSized)(uint32_t size);
    eccobject_t* (*createWithCList)(int count, const char* list[]);
    const struct eccargumentsidentity_t identity;
};


struct io_libecc_Array
{
    eccobject_t object;
};

struct type_io_libecc_Array
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobject_t* (*create)(void);
    eccobject_t* (*createSized)(uint32_t size);
    const struct io_libecc_Array identity;
};



struct io_libecc_Boolean
{
    eccobject_t object;
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
    eccobject_t object;
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
    eccobject_t object;
    ecctextstring_t text;
};

struct type_io_libecc_Error
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Error* (*error)(ecctextstring_t, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*rangeError)(ecctextstring_t, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*referenceError)(ecctextstring_t, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*syntaxError)(ecctextstring_t, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*typeError)(ecctextstring_t, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*uriError)(ecctextstring_t, struct io_libecc_Chars* message);
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
    eccobject_t object;
    eccobject_t environment;
    eccobject_t* refObject;
    struct io_libecc_OpList* oplist;
    struct io_libecc_Function* pair;
    eccvalue_t boundThis;
    ecctextstring_t text;
    const char* name;
    int parameterCount;
    enum io_libecc_function_Flags flags;
};

struct type_io_libecc_Function
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Function* (*create)(eccobject_t* environment);
    struct io_libecc_Function* (*createSized)(eccobject_t* environment, uint32_t size);
    struct io_libecc_Function* (*createWithNative)(const io_libecc_native_io_libecc_Function native, int parameterCount);
    struct io_libecc_Function* (*copy)(struct io_libecc_Function* original);
    void (*destroy)(struct io_libecc_Function*);
    void (*addMember)(struct io_libecc_Function*, const char* name, eccvalue_t value, enum io_libecc_value_Flags);
    void (*addValue)(struct io_libecc_Function*, const char* name, eccvalue_t value, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addMethod)(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addFunction)(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addToObject)(eccobject_t* object, const char* name, const io_libecc_native_io_libecc_Function native, int parameterCount, enum io_libecc_value_Flags);
    void (*linkPrototype)(struct io_libecc_Function*, eccvalue_t prototype, enum io_libecc_value_Flags);
    void (*setupBuiltinObject)(struct io_libecc_Function**,
                               const io_libecc_native_io_libecc_Function,
                               int parameterCount,
                               eccobject_t**,
                               eccvalue_t prototype,
                               const struct io_libecc_object_Type* type);
    eccvalue_t (*accessor)(const io_libecc_native_io_libecc_Function getter, const io_libecc_native_io_libecc_Function setter);
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
    eccobject_t object;
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
    eccobject_t object;
    struct io_libecc_Chars* value;
};

struct type_io_libecc_String
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_String* (*create)(struct io_libecc_Chars*);
    eccvalue_t (*valueAtIndex)(struct io_libecc_String*, int32_t index);
    ecctextstring_t (*textAtIndex)(const char* chars, int32_t length, int32_t index, int enableReverse);
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


struct ECCNSEnv
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
    const struct ECCNSEnv identity;
};



struct eccioinput_t
{
    char name[
    4096
    ];
    uint32_t length;
    char* bytes;
    uint16_t lineCount;
    uint16_t lineCapacity;
    uint32_t* lines;
    eccvalue_t* attached;
    uint16_t attachedCount;
};

struct type_io_libecc_Input
{
    eccioinput_t* (*createFromFile)(const char* filename);
    eccioinput_t* (*createFromBytes)(const char* bytes, uint32_t length, const char* name, ...);
    void (*destroy)(eccioinput_t*);
    void (*printText)(eccioinput_t*, ecctextstring_t text, int32_t ofLine, ecctextstring_t ofText, const char* ofInput, int fullLine);
    int32_t (*findLine)(eccioinput_t*, ecctextstring_t text);
    eccvalue_t (*attachValue)(eccioinput_t*, eccvalue_t value);
    const eccioinput_t identity;
};



enum io_libecc_ecc_EvalFlags
{
    io_libecc_ecc_sloppyMode = 0x1,
    io_libecc_ecc_primitiveResult = 0x2,
    io_libecc_ecc_stringResult = 0x6,
};


struct ECCNSScript
{
    jmp_buf* envList;
    uint16_t envCount;
    uint16_t envCapacity;
    struct io_libecc_Function* global;
    eccvalue_t result;
    ecctextstring_t text;
    int32_t ofLine;
    ecctextstring_t ofText;
    const char* ofInput;
    eccioinput_t** inputs;
    uint16_t inputCount;
    int16_t maximumCallDepth;
    unsigned printLastThrow : 1;
    unsigned sloppyMode : 1;
};

struct type_io_libecc_Ecc
{
    struct ECCNSScript* (*create)(void);
    void (*destroy)(struct ECCNSScript*);
    void (*addValue)(struct ECCNSScript*, const char* name, eccvalue_t value, enum io_libecc_value_Flags);
    void (*addFunction)(struct ECCNSScript*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    int (*evalInput)(struct ECCNSScript*, eccioinput_t*, enum io_libecc_ecc_EvalFlags);
    void (*evalInputWithContext)(struct ECCNSScript*, eccioinput_t*, eccstate_t* context);
    jmp_buf* (*pushEnv)(struct ECCNSScript*);
    void (*popEnv)(struct ECCNSScript*);
    void (*jmpEnv)(struct ECCNSScript*, eccvalue_t value) __attribute__((noreturn));
    void (*fatal)(const char* format, ...) __attribute__((noreturn));
    eccioinput_t* (*findInput)(struct ECCNSScript* self, ecctextstring_t text);
    void (*printTextInput)(struct ECCNSScript*, ecctextstring_t text, int fullLine);
    void (*garbageCollect)(struct ECCNSScript*);
    const struct ECCNSScript identity;
};



enum eccasttoktype_t
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

struct eccastlexer_t
{
    eccioinput_t* input;
    uint32_t offset;
    eccvalue_t value;
    ecctextstring_t text;
    int didLineBreak;
    int allowRegex;
    int allowUnicodeOutsideLiteral;
    int disallowKeyword;
};

struct type_io_libecc_Lexer
{
    eccastlexer_t* (*createWithInput)(eccioinput_t*);
    void (*destroy)(eccastlexer_t*);
    eccasttoktype_t (*nextToken)(eccastlexer_t*);
    const char* (*tokenChars)(eccasttoktype_t token, char buffer[4]);
    eccvalue_t (*scanBinary)(ecctextstring_t text, enum io_libecc_lexer_ScanFlags);
    eccvalue_t (*scanInteger)(ecctextstring_t text, int base, enum io_libecc_lexer_ScanFlags);
    uint32_t (*scanElement)(ecctextstring_t text);
    uint8_t (*uint8Hex)(char a, char b);
    uint16_t (*uint16Hex)(char a, char b, char c, char d);
    const eccastlexer_t identity;
};



struct io_libecc_Op
{
    io_libecc_native_io_libecc_Function native;
    eccvalue_t value;
    ecctextstring_t text;
};

struct type_io_libecc_Op
{
    struct io_libecc_Op (*make)(const io_libecc_native_io_libecc_Function native, eccvalue_t value, ecctextstring_t text);
    const char* (*toChars)(const io_libecc_native_io_libecc_Function native);
    eccvalue_t (*callFunctionArguments)(eccstate_t* const,
                                                    enum io_libecc_context_Offset,
                                                    struct io_libecc_Function* function,
                                                    eccvalue_t this,
                                                    eccobject_t* arguments);
    eccvalue_t (*callFunctionVA)(
    eccstate_t* const, enum io_libecc_context_Offset, struct io_libecc_Function* function, eccvalue_t this, int argumentCount, va_list ap);
    eccvalue_t (*noop)(eccstate_t* const);
    eccvalue_t (*value)(eccstate_t* const);
    eccvalue_t (*valueConstRef)(eccstate_t* const);
    eccvalue_t (*text)(eccstate_t* const);
    eccvalue_t (*function)(eccstate_t* const);
    eccvalue_t (*object)(eccstate_t* const);
    eccvalue_t (*array)(eccstate_t* const);
    eccvalue_t (*regexp)(eccstate_t* const);
    eccvalue_t (*this)(eccstate_t* const);
    eccvalue_t (*createLocalRef)(eccstate_t* const);
    eccvalue_t (*getLocalRefOrNull)(eccstate_t* const);
    eccvalue_t (*getLocalRef)(eccstate_t* const);
    eccvalue_t (*getLocal)(eccstate_t* const);
    eccvalue_t (*setLocal)(eccstate_t* const);
    eccvalue_t (*deleteLocal)(eccstate_t* const);
    eccvalue_t (*getLocalSlotRef)(eccstate_t* const);
    eccvalue_t (*getLocalSlot)(eccstate_t* const);
    eccvalue_t (*setLocalSlot)(eccstate_t* const);
    eccvalue_t (*deleteLocalSlot)(eccstate_t* const);
    eccvalue_t (*getParentSlotRef)(eccstate_t* const);
    eccvalue_t (*getParentSlot)(eccstate_t* const);
    eccvalue_t (*setParentSlot)(eccstate_t* const);
    eccvalue_t (*deleteParentSlot)(eccstate_t* const);
    eccvalue_t (*getMemberRef)(eccstate_t* const);
    eccvalue_t (*getMember)(eccstate_t* const);
    eccvalue_t (*setMember)(eccstate_t* const);
    eccvalue_t (*callMember)(eccstate_t* const);
    eccvalue_t (*deleteMember)(eccstate_t* const);
    eccvalue_t (*getPropertyRef)(eccstate_t* const);
    eccvalue_t (*getProperty)(eccstate_t* const);
    eccvalue_t (*setProperty)(eccstate_t* const);
    eccvalue_t (*callProperty)(eccstate_t* const);
    eccvalue_t (*deleteProperty)(eccstate_t* const);
    eccvalue_t (*pushEnvironment)(eccstate_t* const);
    eccvalue_t (*popEnvironment)(eccstate_t* const);
    eccvalue_t (*exchange)(eccstate_t* const);
    eccvalue_t (*typeOf)(eccstate_t* const);
    eccvalue_t (*equal)(eccstate_t* const);
    eccvalue_t (*notEqual)(eccstate_t* const);
    eccvalue_t (*identical)(eccstate_t* const);
    eccvalue_t (*notIdentical)(eccstate_t* const);
    eccvalue_t (*less)(eccstate_t* const);
    eccvalue_t (*lessOrEqual)(eccstate_t* const);
    eccvalue_t (*more)(eccstate_t* const);
    eccvalue_t (*moreOrEqual)(eccstate_t* const);
    eccvalue_t (*instanceOf)(eccstate_t* const);
    eccvalue_t (*in)(eccstate_t* const);
    eccvalue_t (*add)(eccstate_t* const);
    eccvalue_t (*minus)(eccstate_t* const);
    eccvalue_t (*multiply)(eccstate_t* const);
    eccvalue_t (*divide)(eccstate_t* const);
    eccvalue_t (*modulo)(eccstate_t* const);
    eccvalue_t (*leftShift)(eccstate_t* const);
    eccvalue_t (*rightShift)(eccstate_t* const);
    eccvalue_t (*unsignedRightShift)(eccstate_t* const);
    eccvalue_t (*bitwiseAnd)(eccstate_t* const);
    eccvalue_t (*bitwiseXor)(eccstate_t* const);
    eccvalue_t (*bitwiseOr)(eccstate_t* const);
    eccvalue_t (*logicalAnd)(eccstate_t* const);
    eccvalue_t (*logicalOr)(eccstate_t* const);
    eccvalue_t (*positive)(eccstate_t* const);
    eccvalue_t (*negative)(eccstate_t* const);
    eccvalue_t (*invert)(eccstate_t* const);
    eccvalue_t (*not )(eccstate_t* const);
    eccvalue_t (*construct)(eccstate_t* const);
    eccvalue_t (*call)(eccstate_t* const);
    eccvalue_t (*eval)(eccstate_t* const);
    eccvalue_t (*incrementRef)(eccstate_t* const);
    eccvalue_t (*decrementRef)(eccstate_t* const);
    eccvalue_t (*postIncrementRef)(eccstate_t* const);
    eccvalue_t (*postDecrementRef)(eccstate_t* const);
    eccvalue_t (*addAssignRef)(eccstate_t* const);
    eccvalue_t (*minusAssignRef)(eccstate_t* const);
    eccvalue_t (*multiplyAssignRef)(eccstate_t* const);
    eccvalue_t (*divideAssignRef)(eccstate_t* const);
    eccvalue_t (*moduloAssignRef)(eccstate_t* const);
    eccvalue_t (*leftShiftAssignRef)(eccstate_t* const);
    eccvalue_t (*rightShiftAssignRef)(eccstate_t* const);
    eccvalue_t (*unsignedRightShiftAssignRef)(eccstate_t* const);
    eccvalue_t (*bitAndAssignRef)(eccstate_t* const);
    eccvalue_t (*bitXorAssignRef)(eccstate_t* const);
    eccvalue_t (*bitOrAssignRef)(eccstate_t* const);
    eccvalue_t (*debugger)(eccstate_t* const);
    eccvalue_t (*try)(eccstate_t* const);
    eccvalue_t (*throw)(eccstate_t* const);
    eccvalue_t (*with)(eccstate_t* const);
    eccvalue_t (*next)(eccstate_t* const);
    eccvalue_t (*nextIf)(eccstate_t* const);
    eccvalue_t (*autoreleaseExpression)(eccstate_t* const);
    eccvalue_t (*autoreleaseDiscard)(eccstate_t* const);
    eccvalue_t (*expression)(eccstate_t* const);
    eccvalue_t (*discard)(eccstate_t* const);
    eccvalue_t (*discardN)(eccstate_t* const);
    eccvalue_t (*jump)(eccstate_t* const);
    eccvalue_t (*jumpIf)(eccstate_t* const);
    eccvalue_t (*jumpIfNot)(eccstate_t* const);
    eccvalue_t (*repopulate)(eccstate_t* const);
    eccvalue_t (*result)(eccstate_t* const);
    eccvalue_t (*resultVoid)(eccstate_t* const);
    eccvalue_t (*switchOp)(eccstate_t* const);
    eccvalue_t (*breaker)(eccstate_t* const);
    eccvalue_t (*iterate)(eccstate_t* const);
    eccvalue_t (*iterateLessRef)(eccstate_t* const);
    eccvalue_t (*iterateMoreRef)(eccstate_t* const);
    eccvalue_t (*iterateLessOrEqualRef)(eccstate_t* const);
    eccvalue_t (*iterateMoreOrEqualRef)(eccstate_t* const);
    eccvalue_t (*iterateInRef)(eccstate_t* const);
    const struct io_libecc_Op identity;
};


struct io_libecc_OpList
{
    uint32_t count;
    struct io_libecc_Op* ops;
};

struct type_io_libecc_OpList
{
    struct io_libecc_OpList* (*create)(const io_libecc_native_io_libecc_Function native, eccvalue_t value, ecctextstring_t text);
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
    void (*optimizeWithEnvironment)(struct io_libecc_OpList*, eccobject_t* environment, uint32_t index);
    void (*dumpTo)(struct io_libecc_OpList*, FILE* file);
    ecctextstring_t (*text)(struct io_libecc_OpList* oplist);
    const struct io_libecc_OpList identity;
};


struct eccastparser_t
{
    eccastlexer_t* lexer;
    eccasttoktype_t previewToken;
    struct io_libecc_Error* error;

    struct
    {
        struct io_libecc_Key key;
        char depth;
    }* depths;

    uint16_t depthCount;
    eccobject_t* global;
    struct io_libecc_Function* function;
    uint16_t sourceDepth;
    int preferInteger;
    int strictMode;
    int reserveGlobalSlots;
};

struct type_io_libecc_Parser
{
    eccastparser_t* (*createWithLexer)(eccastlexer_t*);
    void (*destroy)(eccastparser_t*);
    struct io_libecc_Function* (*parseWithEnvironment)(eccastparser_t* const, eccobject_t* environment, eccobject_t* global);
    const eccastparser_t identity;
};


struct io_libecc_Pool
{
    struct io_libecc_Function** functionList;
    uint32_t functionCount;
    uint32_t functionCapacity;
    eccobject_t** objectList;
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
    void (*addObject)(eccobject_t* object);
    void (*addChars)(struct io_libecc_Chars* chars);
    void (*unmarkAll)(void);
    void (*markValue)(eccvalue_t value);
    void (*markObject)(eccobject_t* object);
    void (*collectUnmarked)(void);
    void (*collectUnreferencedFromIndices)(uint32_t indices[3]);
    void (*unreferenceFromIndices)(uint32_t indices[3]);
    void (*getIndices)(uint32_t indices[3]);
    const struct io_libecc_Pool identity;
};

extern const ecctextstring_t ECC_ConstString_Undefined;
extern const ecctextstring_t ECC_ConstString_Null;
extern const ecctextstring_t ECC_ConstString_False;
extern const ecctextstring_t ECC_ConstString_True;
extern const ecctextstring_t ECC_ConstString_Boolean;
extern const ecctextstring_t ECC_ConstString_Number;
extern const ecctextstring_t ECC_ConstString_String;
extern const ecctextstring_t ECC_ConstString_Object;
extern const ecctextstring_t ECC_ConstString_Function;
extern const ecctextstring_t ECC_ConstString_Zero;
extern const ecctextstring_t ECC_ConstString_One;
extern const ecctextstring_t ECC_ConstString_Nan;
extern const ecctextstring_t ECC_ConstString_Infinity;
extern const ecctextstring_t ECC_ConstString_NegativeInfinity;
extern const ecctextstring_t ECC_ConstString_NativeCode;
extern const ecctextstring_t ECC_ConstString_Empty;
extern const ecctextstring_t ECC_ConstString_EmptyRegExp;
extern const ecctextstring_t ECC_ConstString_NullType;
extern const ecctextstring_t ECC_ConstString_UndefinedType;
extern const ecctextstring_t ECC_ConstString_ObjectType;
extern const ecctextstring_t ECC_ConstString_ErrorType;
extern const ecctextstring_t ECC_ConstString_ArrayType;
extern const ecctextstring_t ECC_ConstString_StringType;
extern const ecctextstring_t ECC_ConstString_RegexpType;
extern const ecctextstring_t ECC_ConstString_NumberType;
extern const ecctextstring_t ECC_ConstString_BooleanType;
extern const ecctextstring_t ECC_ConstString_DateType;
extern const ecctextstring_t ECC_ConstString_FunctionType;
extern const ecctextstring_t ECC_ConstString_ArgumentsType;
extern const ecctextstring_t ECC_ConstString_MathType;
extern const ecctextstring_t ECC_ConstString_JsonType;
extern const ecctextstring_t ECC_ConstString_GlobalType;
extern const ecctextstring_t ECC_ConstString_ErrorName;
extern const ecctextstring_t ECC_ConstString_RangeErrorName;
extern const ecctextstring_t ECC_ConstString_ReferenceErrorName;
extern const ecctextstring_t ECC_ConstString_SyntaxErrorName;
extern const ecctextstring_t ECC_ConstString_TypeErrorName;
extern const ecctextstring_t ECC_ConstString_UriErrorName;
extern const ecctextstring_t ECC_ConstString_InputErrorName;
extern const ecctextstring_t ECC_ConstString_EvalErrorName;
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
extern const eccvalue_t ECCValConstNone;
extern const eccvalue_t ECCValConstUndefined;
extern const eccvalue_t ECCValConstTrue;
extern const eccvalue_t ECCValConstFalse;
extern const eccvalue_t ECCValConstNull;


extern const uint32_t io_libecc_object_ElementMax;
extern eccobject_t* io_libecc_object_prototype;
extern struct io_libecc_Function* io_libecc_object_constructor;
extern const struct io_libecc_object_Type io_libecc_object_type;
extern eccobject_t* io_libecc_regexp_prototype;
extern struct io_libecc_Function* io_libecc_regexp_constructor;
extern const struct io_libecc_object_Type io_libecc_regexp_type;
extern eccobject_t* io_libecc_arguments_prototype;
extern const struct io_libecc_object_Type io_libecc_arguments_type;
extern eccobject_t* io_libecc_array_prototype;
extern struct io_libecc_Function* io_libecc_array_constructor;
extern const struct io_libecc_object_Type io_libecc_array_type;
extern eccobject_t* io_libecc_boolean_prototype;
extern struct io_libecc_Function* io_libecc_boolean_constructor;
extern const struct io_libecc_object_Type io_libecc_boolean_type;
extern eccobject_t* io_libecc_date_prototype;
extern struct io_libecc_Function* io_libecc_date_constructor;
extern const struct io_libecc_object_Type io_libecc_date_type;
extern eccobject_t* io_libecc_error_prototype;
extern eccobject_t* io_libecc_error_rangePrototype;
extern eccobject_t* io_libecc_error_referencePrototype;
extern eccobject_t* io_libecc_error_syntaxPrototype;
extern eccobject_t* io_libecc_error_typePrototype;
extern eccobject_t* io_libecc_error_uriPrototype;
extern eccobject_t* io_libecc_error_evalPrototype;
extern struct io_libecc_Function* io_libecc_error_constructor;
extern struct io_libecc_Function* io_libecc_error_rangeConstructor;
extern struct io_libecc_Function* io_libecc_error_referenceConstructor;
extern struct io_libecc_Function* io_libecc_error_syntaxConstructor;
extern struct io_libecc_Function* io_libecc_error_typeConstructor;
extern struct io_libecc_Function* io_libecc_error_uriConstructor;
extern struct io_libecc_Function* io_libecc_error_evalConstructor;
extern const struct io_libecc_object_Type io_libecc_error_type;
extern eccobject_t* io_libecc_function_prototype;
extern struct io_libecc_Function* io_libecc_function_constructor;
extern const struct io_libecc_object_Type io_libecc_function_type;
extern eccobject_t* io_libecc_json_object;
extern const struct io_libecc_object_Type io_libecc_json_type;
extern eccobject_t* io_libecc_math_object;
extern const struct io_libecc_object_Type io_libecc_math_type;
extern eccobject_t* io_libecc_number_prototype;
extern struct io_libecc_Function* io_libecc_number_constructor;
extern const struct io_libecc_object_Type io_libecc_number_type;
extern eccobject_t* io_libecc_string_prototype;
extern struct io_libecc_Function* io_libecc_string_constructor;
extern const struct io_libecc_object_Type io_libecc_string_type;
extern const struct io_libecc_object_Type io_libecc_global_type;
extern const int io_libecc_env_print_max;

extern uint32_t io_libecc_ecc_version;

/* 'namespaces' */
extern const struct type_io_libecc_Value ECCNSValue;
extern const struct type_io_libecc_Ecc ECCNSScript;
extern const struct type_io_libecc_Env ECCNSEnv;
extern const struct type_io_libecc_Text ECCNSText;
extern const struct type_io_libecc_Context ECCNSContext;
extern const struct type_io_libecc_Arguments ECCNSArguments;
extern const struct type_io_libecc_Object ECCNSObject;


extern const struct type_io_libecc_Key io_libecc_Key;
extern const struct type_io_libecc_Chars io_libecc_Chars;
extern const struct type_io_libecc_RegExp io_libecc_RegExp;
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
extern const struct type_io_libecc_Input io_libecc_Input;
extern const struct type_io_libecc_Lexer io_libecc_Lexer;
extern const struct type_io_libecc_Op io_libecc_Op;
extern const struct type_io_libecc_OpList io_libecc_OpList;
extern const struct type_io_libecc_Parser io_libecc_Parser;
extern const struct type_io_libecc_Pool io_libecc_Pool;




