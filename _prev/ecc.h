
/*
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

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

#if(__STDC_VERSION__ < 199901L)
    #ifdef __GNUC__
        #define inline __inline__
    #else
        #define inline static
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


struct io_libecc_JSON;
struct io_libecc_Math;

struct eccpseudonschars_t;
struct eccpseudonscontext_t;
struct eccpseudonsenv_t;
struct eccpseudonsinput_t;
struct eccpseudonskey_t;
struct eccpseudonslexer_t;
struct eccpseudonsop_t;
struct eccpseudonsoplist_t;
struct eccpseudonsparser_t;
struct eccpseudonspool_t;
struct eccpseudonsecc_t;
struct eccpseudonsarguments_t;
struct eccpseudonsarray_t;
struct eccpseudonsboolean_t;
struct eccpseudonsdate_t;
struct eccpseudonserror_t;
struct eccpseudonsfunction_t;
struct eccpseudonsglobal_t;
struct eccpseudonsjson_t;
struct eccpseudonsmath_t;
struct eccpseudonsnumber_t;
struct eccpseudonsobject_t;
struct eccpseudonsregexp_t;
struct eccpseudonsstring_t;
struct eccpseudonstext_t;
struct eccpseudonsvalue_t;


enum ecctextflags_t
{
    ECC_TEXTFLAG_BREAKFLAG = 1 << 0,
};


enum eccindexflags_t
{
    ECC_INDEXFLAG_COPYONCREATE = (1 << 0),
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

enum eccvalmask_t
{
    ECC_VALMASK_NUMBER = 0x08,
    ECC_VALMASK_STRING = 0x10,
    ECC_VALMASK_BOOLEAN = 0x20,
    ECC_VALMASK_OBJECT = 0x40,
    ECC_VALMASK_DYNAMIC = 0x41,
};

enum eccvalflag_t
{
    ECC_VALFLAG_READONLY = 1 << 0,
    ECC_VALFLAG_HIDDEN = 1 << 1,
    ECC_VALFLAG_SEALED = 1 << 2,
    ECC_VALFLAG_GETTER = 1 << 3,
    ECC_VALFLAG_SETTER = 1 << 4,
    ECC_VALFLAG_ASOWN = 1 << 5,
    ECC_VALFLAG_ASDATA = 1 << 6,

    ECC_VALFLAG_FROZEN = ECC_VALFLAG_READONLY | ECC_VALFLAG_SEALED,
    ECC_VALFLAG_ACCESSOR = ECC_VALFLAG_GETTER | ECC_VALFLAG_SETTER,
};

enum eccvalhint_t
{
    ECC_VALHINT_AUTO = 0,
    ECC_VALHINT_STRING = 1,
    ECC_VALHINT_NUMBER = -1,
};


enum eccctxindextype_t
{
    ECC_CTXINDECTYPE_SAVEDINDEXALT = -2,
    ECC_CTXINDEXTYPE_SAVED = -1,
    ECC_CTXINDEXTYPE_NO = 0,
    ECC_CTXINDEXTYPE_CALL = 1,
    ECC_CTXINDEXTYPE_FUNC = 2,
    ECC_CTXINDEXTYPE_THIS = 3,
};

enum eccctxoffsettype_t
{
    ECC_CTXOFFSET_ACCESSOR = -1,
    ECC_CTXOFFSET_CALL = 1,
    ECC_CTXOFFSET_APPLY = 2,
};

enum eccctxspecialtype_t
{
    ECC_CTXSPECIALTYPE_COUNTMASK = 0x7f,
    ECC_CTXSPECIALTYPE_ASACCESSOR = 1 << 8,
};


enum ecccharbufflags_t
{
    ECC_CHARBUFFLAG_MARK = 1 << 0,
    ECC_CHARBUFFLAG_ASCIIONLY = 1 << 1,
};


enum eccobjflags_t
{
    ECC_OBJFLAG_MARK = 1 << 0,
    ECC_OBJFLAG_SEALED = 1 << 1,
};

enum eccrxoptions_t
{
    ECC_REGEXOPT_ALLOWUNICODEFLAGS = 1 << 0,
};

enum eccobjscriptfuncflags_t
{
    ECC_SCRIPTFUNCFLAG_NEEDHEAP = 1 << 1,
    ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS = 1 << 2,
    ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS = 1 << 3,
    ECC_SCRIPTFUNCFLAG_STRICTMODE = 1 << 4,
};

enum eccenvcolor_t
{
    ECC_COLOR_BLACK = 30,
    ECC_COLOR_RED = 31,
    ECC_COLOR_GREEN = 32,
    ECC_COLOR_YELLOW = 33,
    ECC_COLOR_BLUE = 34,
    ECC_COLOR_MAGENTA = 35,
    ECC_COLOR_CYAN = 36,
    ECC_COLOR_WHITE = 37,
};

enum eccenvattribute_t
{
    ECC_ENVATTR_BOLD = 1,
    ECC_ENVATTR_DIM = 2,
    ECC_ENVATTR_INVISIBLE = 8,
};


enum eccscriptevalflags_t
{
    ECC_SCRIPTEVAL_SLOPPYMODE = 0x1,
    ECC_SCRIPTEVAL_PRIMITIVERESULT = 0x2,
    ECC_SCRIPTEVAL_STRINGRESULT = 0x6,
};

enum eccasttoktype_t
{
    ECC_TOK_NO = 0,
    ECC_TOK_ERROR = 128,
    ECC_TOK_NULL,
    ECC_TOK_TRUE,
    ECC_TOK_FALSE,
    ECC_TOK_INTEGER,
    ECC_TOK_BINARY,
    ECC_TOK_STRING,
    ECC_TOK_ESCAPEDSTRING,
    ECC_TOK_IDENTIFIER,
    ECC_TOK_REGEXP,
    ECC_TOK_BREAK,
    ECC_TOK_CASE,
    ECC_TOK_CATCH,
    ECC_TOK_CONTINUE,
    ECC_TOK_DEBUGGER,
    ECC_TOK_DEFAULT,
    ECC_TOK_DELETE,
    ECC_TOK_DO,
    ECC_TOK_ELSE,
    ECC_TOK_FINALLY,
    ECC_TOK_FOR,
    ECC_TOK_FUNCTION,
    ECC_TOK_IF,
    ECC_TOK_IN,
    ECC_TOK_INSTANCEOF,
    ECC_TOK_NEW,
    ECC_TOK_RETURN,
    ECC_TOK_SWITCH,
    ECC_TOK_THIS,
    ECC_TOK_THROW,
    ECC_TOK_TRY,
    ECC_TOK_TYPEOF,
    ECC_TOK_VAR,
    ECC_TOK_VOID,
    ECC_TOK_WITH,
    ECC_TOK_WHILE,
    ECC_TOK_EQUAL,
    ECC_TOK_NOTEQUAL,
    ECC_TOK_IDENTICAL,
    ECC_TOK_NOTIDENTICAL,
    ECC_TOK_LEFTSHIFTASSIGN,
    ECC_TOK_RIGHTSHIFTASSIGN,
    ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN,
    ECC_TOK_LEFTSHIFT,
    ECC_TOK_RIGHTSHIFT,
    ECC_TOK_UNSIGNEDRIGHTSHIFT,
    ECC_TOK_LESSOREQUAL,
    ECC_TOK_MOREOREQUAL,
    ECC_TOK_INCREMENT,
    ECC_TOK_DECREMENT,
    ECC_TOK_LOGICALAND,
    ECC_TOK_LOGICALOR,
    ECC_TOK_ADDASSIGN,
    ECC_TOK_MINUSASSIGN,
    ECC_TOK_MULTIPLYASSIGN,
    ECC_TOK_DIVIDEASSIGN,
    ECC_TOK_MODULOASSIGN,
    ECC_TOK_ANDASSIGN,
    ECC_TOK_ORASSIGN,
    ECC_TOK_XORASSIGN,
};

enum eccastlexflags_t
{
    ECC_LEXFLAG_SCANLAZY = 1 << 0,
    ECC_LEXFLAG_SCANSLOPPY = 1 << 1,
};

typedef struct eccstate_t eccstate_t;
typedef struct eccobject_t eccobject_t;
typedef struct eccvalue_t eccvalue_t;
typedef struct ecctextchar_t ecctextchar_t;
typedef struct ecctextstring_t ecctextstring_t;
typedef struct eccastlexer_t eccastlexer_t;
typedef struct eccastparser_t eccastparser_t;
typedef struct eccscriptcontext_t eccscriptcontext_t;
typedef struct eccoperand_t eccoperand_t;
typedef struct eccindexkey_t eccindexkey_t;
typedef struct eccmempool_t eccmempool_t;
typedef struct eccappendbuffer_t eccappendbuffer_t;

typedef struct eccoplist_t eccoplist_t;

typedef struct eccregexnode_t eccregexnode_t;

typedef struct ecccharbuffer_t ecccharbuffer_t;
typedef struct eccobjinterntype_t eccobjinterntype_t;
typedef struct eccobjbool_t eccobjbool_t;
typedef struct eccobjstring_t eccobjstring_t;
typedef struct eccobjscriptfunction_t eccobjscriptfunction_t;
typedef struct eccobjdate_t eccobjdate_t;
typedef struct eccobjerror_t eccobjerror_t;
typedef struct eccobjregexp_t eccobjregexp_t;
typedef struct eccobjnumber_t eccobjnumber_t;

typedef struct eccrxstate_t eccrxstate_t;

typedef union ecchashmap_t ecchashmap_t;
typedef union eccobjelement_t eccobjelement_t;

typedef enum eccastlexflags_t eccastlexflags_t;
typedef enum eccscriptevalflags_t eccscriptevalflags_t;
typedef enum eccenvattribute_t eccenvattribute_t;
typedef enum eccenvcolor_t eccenvcolor_t;
typedef enum eccobjflags_t eccobjflags_t;
typedef enum ecccharbufflags_t ecccharbufflags_t;
typedef enum eccctxoffsettype_t eccctxoffsettype_t;
typedef enum eccctxspecialtype_t eccctxspecialtype_t;
typedef enum eccctxindextype_t eccctxindextype_t;
typedef enum eccindexflags_t eccindexflags_t;
typedef enum eccrxoptions_t eccrxoptions_t;
typedef enum eccasttoktype_t eccasttoktype_t;
typedef enum eccvaltype_t eccvaltype_t;
typedef enum eccobjscriptfuncflags_t eccobjscriptfuncflags_t;
typedef enum eccvalflag_t eccvalflag_t;
typedef enum eccvalmask_t eccvalmask_t;
typedef enum eccvalhint_t eccvalhint_t;

typedef struct eccioinput_t eccioinput_t;

typedef struct eccpseudonsmath_t eccpseudonsmath_t;
typedef struct eccpseudonsarray_t eccpseudonsarray_t;
typedef struct eccpseudonsobject_t eccpseudonsobject_t;
typedef struct eccpseudonsregexp_t eccpseudonsregexp_t;
typedef struct eccpseudonsarguments_t eccpseudonsarguments_t;
typedef struct eccpseudonsboolean_t eccpseudonsboolean_t;
typedef struct eccpseudonsdate_t eccpseudonsdate_t;
typedef struct eccpseudonserror_t eccpseudonserror_t;
typedef struct eccpseudonsfunction_t eccpseudonsfunction_t;
typedef struct eccpseudonsjson_t eccpseudonsjson_t;
typedef struct eccpseudonsnumber_t eccpseudonsnumber_t;
typedef struct eccpseudonsstring_t eccpseudonsstring_t;
typedef struct eccpseudonsglobal_t eccpseudonsglobal_t;

typedef eccvalue_t (*eccnativefuncptr_t)(eccstate_t* context);

struct ecctextchar_t
{
    uint32_t codepoint;
    uint8_t units;
};


struct ecctextstring_t
{
    const char* bytes;
    int32_t length;
    uint8_t flags;
};

struct eccpseudonstext_t
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


struct eccindexkey_t
{
    union
    {
        uint8_t depth[4];
        uint32_t integer;
    } data;
};

struct eccpseudonskey_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccindexkey_t (*makeWithCString)(const char* cString);
    eccindexkey_t (*makeWithText)(const ecctextstring_t text, eccindexflags_t flags);
    eccindexkey_t (*search)(const ecctextstring_t text);
    int (*isEqual)(eccindexkey_t, eccindexkey_t);
    const ecctextstring_t* (*textOf)(eccindexkey_t);
    void (*dumpTo)(eccindexkey_t, FILE*);
    const eccindexkey_t identity;
};


struct eccvalue_t
{
    union
    {
        int32_t integer;
        double binary;
        char buffer[8];
        eccindexkey_t key;
        const ecctextstring_t* text;
        ecccharbuffer_t* charbufdata;
        eccobject_t* object;
        eccobjerror_t* error;
        eccobjstring_t* string;
        eccobjregexp_t* regexp;
        eccobjnumber_t* number;
        eccobjbool_t* boolean;
        eccobjdate_t* date;
        eccobjscriptfunction_t* function;
        eccvalue_t* reference;
    } data;

    eccindexkey_t key;
    eccvaltype_t type;
    uint8_t flags;
    uint16_t check;
};

struct eccpseudonsvalue_t
{
    eccvalue_t (*truth)(int truth);
    eccvalue_t (*integer)(int32_t integer);
    eccvalue_t (*binary)(double binary);
    eccvalue_t (*buffer)(const char buffer[7], uint8_t units);
    eccvalue_t (*key)(eccindexkey_t key);
    eccvalue_t (*text)(const ecctextstring_t* text);
    eccvalue_t (*chars)(ecccharbuffer_t* chars);
    eccvalue_t (*object)(eccobject_t*);
    eccvalue_t (*error)(eccobjerror_t*);
    eccvalue_t (*string)(eccobjstring_t*);
    eccvalue_t (*regexp)(eccobjregexp_t*);
    eccvalue_t (*number)(eccobjnumber_t*);
    eccvalue_t (*boolean)(eccobjbool_t*);
    eccvalue_t (*date)(eccobjdate_t*);
    eccvalue_t (*function)(eccobjscriptfunction_t*);
    eccvalue_t (*host)(eccobject_t*);
    eccvalue_t (*reference)(eccvalue_t*);
    int (*isPrimitive)(eccvalue_t);
    int (*isBoolean)(eccvalue_t);
    int (*isNumber)(eccvalue_t);
    int (*isString)(eccvalue_t);
    int (*isObject)(eccvalue_t);
    int (*isDynamic)(eccvalue_t);
    int (*isTrue)(eccvalue_t);
    eccvalue_t (*toPrimitive)(eccstate_t*, eccvalue_t, eccvalhint_t);
    eccvalue_t (*toBinary)(eccstate_t*, eccvalue_t);
    eccvalue_t (*toInteger)(eccstate_t*, eccvalue_t);
    eccvalue_t (*binaryToString)(double binary, int base);
    eccvalue_t (*toString)(eccstate_t*, eccvalue_t);
    int32_t (*stringLength)(const eccvalue_t*);
    const char* (*stringBytes)(const eccvalue_t*);
    ecctextstring_t (*textOf)(const eccvalue_t* string);
    eccvalue_t (*toObject)(eccstate_t*, eccvalue_t);
    eccvalue_t (*objectValue)(eccobject_t*);
    int (*objectIsArray)(eccobject_t*);
    eccvalue_t (*toType)(eccvalue_t);
    eccvalue_t (*equals)(eccstate_t*, eccvalue_t, eccvalue_t);
    eccvalue_t (*same)(eccstate_t*, eccvalue_t, eccvalue_t);
    eccvalue_t (*add)(eccstate_t*, eccvalue_t, eccvalue_t);
    eccvalue_t (*subtract)(eccstate_t*, eccvalue_t, eccvalue_t);
    eccvalue_t (*less)(eccstate_t*, eccvalue_t, eccvalue_t);
    eccvalue_t (*more)(eccstate_t*, eccvalue_t, eccvalue_t);
    eccvalue_t (*lessOrEqual)(eccstate_t*, eccvalue_t, eccvalue_t);
    eccvalue_t (*moreOrEqual)(eccstate_t*, eccvalue_t, eccvalue_t);
    const char* (*typeName)(eccvaltype_t);
    const char* (*maskName)(eccvalmask_t);
    void (*dumpTo)(eccvalue_t, FILE*);
    const eccvalue_t identity;
};


struct eccstate_t
{
    const eccoperand_t* ops;
    eccobject_t* refObject;
    eccobject_t* environment;
    eccstate_t* parent;
    eccscriptcontext_t* ecc;
    eccvalue_t thisvalue;
    const ecctextstring_t* text;
    const ecctextstring_t* textAlt;
    const ecctextstring_t* textCall;
    eccctxindextype_t textIndex;
    int16_t breaker;
    int16_t depth;
    int8_t construct : 1;
    int8_t argumentOffset : 3;
    int8_t strictMode : 1;
    int8_t inEnvironmentObject : 1;
};

struct eccpseudonscontext_t
{
    void (*rangeError)(eccstate_t*, ecccharbuffer_t*);
    void (*referenceError)(eccstate_t*, ecccharbuffer_t*);
    void (*syntaxError)(eccstate_t*, ecccharbuffer_t*);
    void (*typeError)(eccstate_t*, ecccharbuffer_t*);
    void (*uriError)(eccstate_t*, ecccharbuffer_t*);
    void (*doThrow)(eccstate_t*, eccvalue_t);
    eccvalue_t (*callFunction)(eccstate_t*, eccobjscriptfunction_t* function, eccvalue_t thisval, int argumentCount, ...);
    int (*argumentCount)(eccstate_t*);
    eccvalue_t (*argument)(eccstate_t*, int argumentIndex);
    void (*replaceArgument)(eccstate_t*, int argumentIndex, eccvalue_t value);
    eccvalue_t (*getThis)(eccstate_t*);
    void (*assertThisType)(eccstate_t*, eccvaltype_t);
    void (*assertThisMask)(eccstate_t*, eccvalmask_t);
    void (*assertThisCoerciblePrimitive)(eccstate_t*);
    void (*setText)(eccstate_t*, const ecctextstring_t* text);
    void (*setTexts)(eccstate_t*, const ecctextstring_t* text, const ecctextstring_t* textAlt);
    void (*setTextIndex)(eccstate_t*, eccctxindextype_t index);
    void (*setTextIndexArgument)(eccstate_t*, int argument);
    ecctextstring_t (*textSeek)(eccstate_t*);
    void (*rewindStatement)(eccstate_t*);
    void (*printBacktrace)(eccstate_t* context);
    eccobject_t* (*environmentRoot)(eccstate_t* context);
    const eccstate_t identity;
};


struct eccappendbuffer_t
{
    ecccharbuffer_t* charbufvalue;
    char buffer[9];
    uint8_t units;
};

struct ecccharbuffer_t
{
    int32_t length;
    int16_t referenceCount;
    uint8_t flags;
    char bytes[1];
};

struct eccpseudonschars_t
{
    ecccharbuffer_t* (*createVA)(int32_t length, const char* format, va_list ap);
    ecccharbuffer_t* (*create)(const char* format, ...);
    ecccharbuffer_t* (*createSized)(int32_t length);
    ecccharbuffer_t* (*createWithBytes)(int32_t length, const char* bytes);
    void (*beginAppend)(eccappendbuffer_t*);
    void (*append)(eccappendbuffer_t*, const char* format, ...);
    void (*appendCodepoint)(eccappendbuffer_t*, uint32_t cp);
    void (*appendValue)(eccappendbuffer_t*, eccstate_t* context, eccvalue_t value);
    void (*appendBinary)(eccappendbuffer_t*, double binary, int base);
    void (*normalizeBinary)(eccappendbuffer_t*);
    eccvalue_t (*endAppend)(eccappendbuffer_t*);
    void (*destroy)(ecccharbuffer_t*);
    uint8_t (*codepointLength)(uint32_t cp);
    uint8_t (*writeCodepoint)(char*, uint32_t cp);
    const ecccharbuffer_t identity;
};

struct eccobjinterntype_t
{
    const ecctextstring_t* text;
    void (*mark)(eccobject_t*);
    void (*capture)(eccobject_t*);
    void (*finalize)(eccobject_t*);
};


union eccobjelement_t
{
    eccvalue_t elemvalue;
};

union ecchashmap_t
{
    eccvalue_t value;
    uint16_t slot[16];
};

struct eccobject_t
{
    eccobject_t* prototype;
    const eccobjinterntype_t* type;
    eccobjelement_t* element;
    ecchashmap_t* hashmap;
    uint32_t elementCount;
    uint32_t elementCapacity;
    uint16_t hashmapCount;
    uint16_t hashmapCapacity;
    int16_t referenceCount;
    uint8_t flags;
};

struct eccpseudonsobject_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobject_t* (*create)(eccobject_t* prototype);
    eccobject_t* (*createSized)(eccobject_t* prototype, uint16_t size);
    eccobject_t* (*createTyped)(const eccobjinterntype_t* type);
    eccobject_t* (*initialize)(eccobject_t*, eccobject_t* prototype);
    eccobject_t* (*initializeSized)(eccobject_t*, eccobject_t* prototype, uint16_t size);
    eccobject_t* (*finalize)(eccobject_t*);
    eccobject_t* (*copy)(const eccobject_t* original);
    void (*destroy)(eccobject_t*);
    eccvalue_t (*getMember)(eccstate_t*, eccobject_t*, eccindexkey_t key);
    eccvalue_t (*putMember)(eccstate_t*, eccobject_t*, eccindexkey_t key, eccvalue_t);
    eccvalue_t* (*member)(eccobject_t*, eccindexkey_t key, eccvalflag_t);
    eccvalue_t* (*addMember)(eccobject_t*, eccindexkey_t key, eccvalue_t, eccvalflag_t);
    int (*deleteMember)(eccobject_t*, eccindexkey_t key);
    eccvalue_t (*getElement)(eccstate_t*, eccobject_t*, uint32_t index);
    eccvalue_t (*putElement)(eccstate_t*, eccobject_t*, uint32_t index, eccvalue_t);
    eccvalue_t* (*element)(eccobject_t*, uint32_t index, eccvalflag_t);
    eccvalue_t* (*addElement)(eccobject_t*, uint32_t index, eccvalue_t, eccvalflag_t);
    int (*deleteElement)(eccobject_t*, uint32_t index);
    eccvalue_t (*getProperty)(eccstate_t*, eccobject_t*, eccvalue_t primitive);
    eccvalue_t (*putProperty)(eccstate_t*, eccobject_t*, eccvalue_t primitive, eccvalue_t);
    eccvalue_t* (*property)(eccobject_t*, eccvalue_t primitive, eccvalflag_t);
    eccvalue_t* (*addProperty)(eccobject_t*, eccvalue_t primitive, eccvalue_t, eccvalflag_t);
    int (*deleteProperty)(eccobject_t*, eccvalue_t primitive);
    eccvalue_t (*putValue)(eccstate_t*, eccobject_t*, eccvalue_t*, eccvalue_t);
    eccvalue_t (*getValue)(eccstate_t*, eccobject_t*, eccvalue_t*);
    void (*packValue)(eccobject_t*);
    void (*stripMap)(eccobject_t*);
    void (*reserveSlots)(eccobject_t*, uint16_t slots);
    int (*resizeElement)(eccobject_t*, uint32_t size);
    void (*populateElementWithCList)(eccobject_t*, uint32_t count, const char* list[]);
    eccvalue_t (*toString)(eccstate_t*);
    void (*dumpTo)(eccobject_t*, FILE* file);
    const eccobject_t identity;
};

struct eccrxstate_t
{
    const char* const start;
    const char* const end;
    const char** capture;
    const char** index;
    int flags;
};


struct eccobjregexp_t
{
    eccobject_t object;
    ecccharbuffer_t* pattern;
    ecccharbuffer_t* source;
    eccregexnode_t* program;
    uint8_t count;
    uint8_t global : 1;
    uint8_t ignoreCase : 1;
    uint8_t multiline : 1;
};

struct eccpseudonsregexp_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjregexp_t* (*create)(ecccharbuffer_t* pattern, eccobjerror_t**, eccrxoptions_t);
    eccobjregexp_t* (*createWith)(eccstate_t* context, eccvalue_t pattern, eccvalue_t flags);
    int (*matchWithState)(eccobjregexp_t*, eccrxstate_t*);
    const eccobjregexp_t identity;
};

struct eccidentityarguments_t
{
    eccobject_t object;
};

struct eccpseudonsarguments_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobject_t* (*createSized)(uint32_t size);
    eccobject_t* (*createWithCList)(int count, const char* list[]);
    const struct eccidentityarguments_t identity;
};

struct eccidentityarray_t
{
    eccobject_t object;
};

struct eccpseudonsarray_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobject_t* (*create)(void);
    eccobject_t* (*createSized)(uint32_t size);
    const struct eccidentityarray_t identity;
};

struct eccobjbool_t
{
    eccobject_t object;
    int truth;
};

struct eccpseudonsboolean_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjbool_t* (*create)(int);
    const eccobjbool_t identity;
};

struct eccobjdate_t
{
    eccobject_t object;
    double ms;
};

struct eccpseudonsdate_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjdate_t* (*create)(double);
    const eccobjdate_t identity;
};

struct eccobjerror_t
{
    eccobject_t object;
    ecctextstring_t text;
};

struct eccpseudonserror_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjerror_t* (*error)(ecctextstring_t, ecccharbuffer_t* message);
    eccobjerror_t* (*rangeError)(ecctextstring_t, ecccharbuffer_t* message);
    eccobjerror_t* (*referenceError)(ecctextstring_t, ecccharbuffer_t* message);
    eccobjerror_t* (*syntaxError)(ecctextstring_t, ecccharbuffer_t* message);
    eccobjerror_t* (*typeError)(ecctextstring_t, ecccharbuffer_t* message);
    eccobjerror_t* (*uriError)(ecctextstring_t, ecccharbuffer_t* message);
    void (*destroy)(eccobjerror_t*);
    const eccobjerror_t identity;
};


struct eccobjscriptfunction_t
{
    eccobject_t object;
    eccobject_t environment;
    eccobject_t* refObject;
    eccoplist_t* oplist;
    eccobjscriptfunction_t* pair;
    eccvalue_t boundThis;
    ecctextstring_t text;
    const char* name;
    int parameterCount;
    eccobjscriptfuncflags_t flags;
};

struct eccpseudonsfunction_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjscriptfunction_t* (*create)(eccobject_t* environment);
    eccobjscriptfunction_t* (*createSized)(eccobject_t* environment, uint32_t size);
    eccobjscriptfunction_t* (*createWithNative)(const eccnativefuncptr_t native, int parameterCount);
    eccobjscriptfunction_t* (*copy)(eccobjscriptfunction_t* original);
    void (*destroy)(eccobjscriptfunction_t*);
    void (*addMember)(eccobjscriptfunction_t*, const char* name, eccvalue_t value, eccvalflag_t);
    void (*addValue)(eccobjscriptfunction_t*, const char* name, eccvalue_t value, eccvalflag_t);
    eccobjscriptfunction_t* (*addMethod)(eccobjscriptfunction_t*, const char* name, const eccnativefuncptr_t native, int argumentCount, eccvalflag_t);
    eccobjscriptfunction_t* (*addFunction)(eccobjscriptfunction_t*, const char* name, const eccnativefuncptr_t native, int argumentCount, eccvalflag_t);
    eccobjscriptfunction_t* (*addToObject)(eccobject_t* object, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t);
    void (*linkPrototype)(eccobjscriptfunction_t*, eccvalue_t prototype, eccvalflag_t);
    void (*setupBuiltinObject)(eccobjscriptfunction_t**, const eccnativefuncptr_t, int parameterCount, eccobject_t**, eccvalue_t prototype, const eccobjinterntype_t* type);
    eccvalue_t (*accessor)(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter);
    const eccobjscriptfunction_t identity;
};

struct io_libecc_JSON
{
    char empty;
};

struct eccpseudonsjson_t
{
    void (*setup)(void);
    void (*teardown)(void);
    const struct io_libecc_JSON identity;
};

struct io_libecc_Math
{
    char empty;
};

struct eccpseudonsmath_t
{
    void (*setup)(void);
    void (*teardown)(void);
    const struct io_libecc_Math identity;
};

struct eccobjnumber_t
{
    eccobject_t object;
    double value;
};

struct eccpseudonsnumber_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjnumber_t* (*create)(double);
    const eccobjnumber_t identity;
};

struct eccobjstring_t
{
    eccobject_t object;
    ecccharbuffer_t* value;
};

struct eccpseudonsstring_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjstring_t* (*create)(ecccharbuffer_t*);
    eccvalue_t (*valueAtIndex)(eccobjstring_t*, int32_t index);
    ecctextstring_t (*textAtIndex)(const char* chars, int32_t length, int32_t index, int enableReverse);
    int32_t (*unitIndex)(const char* chars, int32_t max, int32_t unit);
    const eccobjstring_t identity;
};

struct eccpseudonsglobal_t
{
    void (*setup)(void);
    void (*teardown)(void);
    eccobjscriptfunction_t* (*create)(void);

    const struct
    {
    } identity;
};

#include <stdbool.h>

struct eccenvidentity_t
{
};

struct eccpseudonsenv_t
{
    void (*setup)(void);
    void (*teardown)(void);
    void (*print)(const char* format, ...);
    void (*printColor)(eccenvcolor_t color, eccenvattribute_t attribute, const char* format, ...);
    void (*printError)(int typeLength, const char* type, const char* format, ...);
    void (*printWarning)(const char* format, ...);
    void (*newline)(void);
    double (*currentTime)(void);
    const struct eccenvidentity_t identity;
};

struct eccioinput_t
{
    char name[4096];
    uint32_t length;
    char* bytes;
    uint64_t lineCount;
    uint64_t lineCapacity;
    uint32_t* lines;
    eccvalue_t* attached;
    uint64_t attachedCount;
};

struct eccpseudonsinput_t
{
    eccioinput_t* (*createFromFile)(const char* filename);
    eccioinput_t* (*createFromBytes)(const char* bytes, uint32_t length, const char* name, ...);
    void (*destroy)(eccioinput_t*);
    void (*printText)(eccioinput_t*, ecctextstring_t text, int32_t ofLine, ecctextstring_t ofText, const char* ofInput, int fullLine);
    int32_t (*findLine)(eccioinput_t*, ecctextstring_t text);
    eccvalue_t (*attachValue)(eccioinput_t*, eccvalue_t value);
    const eccioinput_t identity;
};


struct eccscriptcontext_t
{
    jmp_buf* envList;
    uint16_t envCount;
    uint16_t envCapacity;
    eccobjscriptfunction_t* global;
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

struct eccpseudonsecc_t
{
    eccscriptcontext_t* (*create)(void);
    void (*destroy)(eccscriptcontext_t*);
    void (*addValue)(eccscriptcontext_t*, const char* name, eccvalue_t value, eccvalflag_t);
    void (*addFunction)(eccscriptcontext_t*, const char* name, const eccnativefuncptr_t native, int argumentCount, eccvalflag_t);
    int (*evalInput)(eccscriptcontext_t*, eccioinput_t*, eccscriptevalflags_t);
    void (*evalInputWithContext)(eccscriptcontext_t*, eccioinput_t*, eccstate_t* context);
    jmp_buf* (*pushEnv)(eccscriptcontext_t*);
    void (*popEnv)(eccscriptcontext_t*);
    void (*jmpEnv)(eccscriptcontext_t*, eccvalue_t value);
    void (*fatal)(const char* format, ...);
    eccioinput_t* (*findInput)(eccscriptcontext_t* self, ecctextstring_t text);
    void (*printTextInput)(eccscriptcontext_t*, ecctextstring_t text, int fullLine);
    void (*garbageCollect)(eccscriptcontext_t*);
    const eccscriptcontext_t identity;
};


struct eccastlexer_t
{
    eccioinput_t* input;
    uint32_t offset;
    eccvalue_t parsedvalue;
    ecctextstring_t text;
    int didLineBreak;
    int allowRegex;
    int allowUnicodeOutsideLiteral;
    int disallowKeyword;
};

struct eccpseudonslexer_t
{
    eccastlexer_t* (*createWithInput)(eccioinput_t*);
    void (*destroy)(eccastlexer_t*);
    eccasttoktype_t (*nextToken)(eccastlexer_t*);
    const char* (*tokenChars)(eccasttoktype_t token, char buffer[4]);
    eccvalue_t (*scanBinary)(ecctextstring_t text, eccastlexflags_t);
    eccvalue_t (*scanInteger)(ecctextstring_t text, int base, eccastlexflags_t);
    uint32_t (*scanElement)(ecctextstring_t text);
    uint8_t (*uint8Hex)(char a, char b);
    uint16_t (*uint16Hex)(char a, char b, char c, char d);
    const eccastlexer_t identity;
};

struct eccoperand_t
{
    eccnativefuncptr_t native;
    eccvalue_t value;
    ecctextstring_t text;
};

struct eccpseudonsop_t
{
    eccoperand_t (*make)(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text);
    const char* (*toChars)(const eccnativefuncptr_t native);
    eccvalue_t (*callFunctionArguments)(eccstate_t*, eccctxoffsettype_t, eccobjscriptfunction_t* function, eccvalue_t thisval, eccobject_t* arguments);
    eccvalue_t (*callFunctionVA)(eccstate_t*, eccctxoffsettype_t, eccobjscriptfunction_t* function, eccvalue_t thisval, int argumentCount, va_list ap);
    eccvalue_t (*noop)(eccstate_t*);
    eccvalue_t (*value)(eccstate_t*);
    eccvalue_t (*valueConstRef)(eccstate_t*);
    eccvalue_t (*text)(eccstate_t*);
    eccvalue_t (*function)(eccstate_t*);
    eccvalue_t (*object)(eccstate_t*);
    eccvalue_t (*array)(eccstate_t*);
    eccvalue_t (*regexp)(eccstate_t*);
    eccvalue_t (*getThis)(eccstate_t*);
    eccvalue_t (*createLocalRef)(eccstate_t*);
    eccvalue_t (*getLocalRefOrNull)(eccstate_t*);
    eccvalue_t (*getLocalRef)(eccstate_t*);
    eccvalue_t (*getLocal)(eccstate_t*);
    eccvalue_t (*setLocal)(eccstate_t*);
    eccvalue_t (*deleteLocal)(eccstate_t*);
    eccvalue_t (*getLocalSlotRef)(eccstate_t*);
    eccvalue_t (*getLocalSlot)(eccstate_t*);
    eccvalue_t (*setLocalSlot)(eccstate_t*);
    eccvalue_t (*deleteLocalSlot)(eccstate_t*);
    eccvalue_t (*getParentSlotRef)(eccstate_t*);
    eccvalue_t (*getParentSlot)(eccstate_t*);
    eccvalue_t (*setParentSlot)(eccstate_t*);
    eccvalue_t (*deleteParentSlot)(eccstate_t*);
    eccvalue_t (*getMemberRef)(eccstate_t*);
    eccvalue_t (*getMember)(eccstate_t*);
    eccvalue_t (*setMember)(eccstate_t*);
    eccvalue_t (*callMember)(eccstate_t*);
    eccvalue_t (*deleteMember)(eccstate_t*);
    eccvalue_t (*getPropertyRef)(eccstate_t*);
    eccvalue_t (*getProperty)(eccstate_t*);
    eccvalue_t (*setProperty)(eccstate_t*);
    eccvalue_t (*callProperty)(eccstate_t*);
    eccvalue_t (*deleteProperty)(eccstate_t*);
    eccvalue_t (*pushEnvironment)(eccstate_t*);
    eccvalue_t (*popEnvironment)(eccstate_t*);
    eccvalue_t (*exchange)(eccstate_t*);
    eccvalue_t (*typeOf)(eccstate_t*);
    eccvalue_t (*equal)(eccstate_t*);
    eccvalue_t (*notEqual)(eccstate_t*);
    eccvalue_t (*identical)(eccstate_t*);
    eccvalue_t (*notIdentical)(eccstate_t*);
    eccvalue_t (*less)(eccstate_t*);
    eccvalue_t (*lessOrEqual)(eccstate_t*);
    eccvalue_t (*more)(eccstate_t*);
    eccvalue_t (*moreOrEqual)(eccstate_t*);
    eccvalue_t (*instanceOf)(eccstate_t*);
    eccvalue_t (*in)(eccstate_t*);
    eccvalue_t (*add)(eccstate_t*);
    eccvalue_t (*minus)(eccstate_t*);
    eccvalue_t (*multiply)(eccstate_t*);
    eccvalue_t (*divide)(eccstate_t*);
    eccvalue_t (*modulo)(eccstate_t*);
    eccvalue_t (*leftShift)(eccstate_t*);
    eccvalue_t (*rightShift)(eccstate_t*);
    eccvalue_t (*unsignedRightShift)(eccstate_t*);
    eccvalue_t (*bitwiseAnd)(eccstate_t*);
    eccvalue_t (*bitwiseXor)(eccstate_t*);
    eccvalue_t (*bitwiseOr)(eccstate_t*);
    eccvalue_t (*logicalAnd)(eccstate_t*);
    eccvalue_t (*logicalOr)(eccstate_t*);
    eccvalue_t (*positive)(eccstate_t*);
    eccvalue_t (*negative)(eccstate_t*);
    eccvalue_t (*invert)(eccstate_t*);
    eccvalue_t (*doLogicalNot)(eccstate_t*);
    eccvalue_t (*construct)(eccstate_t*);
    eccvalue_t (*call)(eccstate_t*);
    eccvalue_t (*eval)(eccstate_t*);
    eccvalue_t (*incrementRef)(eccstate_t*);
    eccvalue_t (*decrementRef)(eccstate_t*);
    eccvalue_t (*postIncrementRef)(eccstate_t*);
    eccvalue_t (*postDecrementRef)(eccstate_t*);
    eccvalue_t (*addAssignRef)(eccstate_t*);
    eccvalue_t (*minusAssignRef)(eccstate_t*);
    eccvalue_t (*multiplyAssignRef)(eccstate_t*);
    eccvalue_t (*divideAssignRef)(eccstate_t*);
    eccvalue_t (*moduloAssignRef)(eccstate_t*);
    eccvalue_t (*leftShiftAssignRef)(eccstate_t*);
    eccvalue_t (*rightShiftAssignRef)(eccstate_t*);
    eccvalue_t (*unsignedRightShiftAssignRef)(eccstate_t*);
    eccvalue_t (*bitAndAssignRef)(eccstate_t*);
    eccvalue_t (*bitXorAssignRef)(eccstate_t*);
    eccvalue_t (*bitOrAssignRef)(eccstate_t*);
    eccvalue_t (*debugger)(eccstate_t*);
    eccvalue_t (*doTry)(eccstate_t*);
    eccvalue_t (*doThrow)(eccstate_t*);
    eccvalue_t (*with)(eccstate_t*);
    eccvalue_t (*next)(eccstate_t*);
    eccvalue_t (*nextIf)(eccstate_t*);
    eccvalue_t (*autoreleaseExpression)(eccstate_t*);
    eccvalue_t (*autoreleaseDiscard)(eccstate_t*);
    eccvalue_t (*expression)(eccstate_t*);
    eccvalue_t (*discard)(eccstate_t*);
    eccvalue_t (*discardN)(eccstate_t*);
    eccvalue_t (*jump)(eccstate_t*);
    eccvalue_t (*jumpIf)(eccstate_t*);
    eccvalue_t (*jumpIfNot)(eccstate_t*);
    eccvalue_t (*repopulate)(eccstate_t*);
    eccvalue_t (*result)(eccstate_t*);
    eccvalue_t (*resultVoid)(eccstate_t*);
    eccvalue_t (*switchOp)(eccstate_t*);
    eccvalue_t (*breaker)(eccstate_t*);
    eccvalue_t (*iterate)(eccstate_t*);
    eccvalue_t (*iterateLessRef)(eccstate_t*);
    eccvalue_t (*iterateMoreRef)(eccstate_t*);
    eccvalue_t (*iterateLessOrEqualRef)(eccstate_t*);
    eccvalue_t (*iterateMoreOrEqualRef)(eccstate_t*);
    eccvalue_t (*iterateInRef)(eccstate_t*);
    const eccoperand_t identity;
};

struct eccoplist_t
{
    uint32_t count;
    eccoperand_t* ops;
};

struct eccpseudonsoplist_t
{
    eccoplist_t* (*create)(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text);
    void (*destroy)(eccoplist_t*);
    eccoplist_t* (*join)(eccoplist_t*, eccoplist_t*);
    eccoplist_t* (*join3)(eccoplist_t*, eccoplist_t*, eccoplist_t*);
    eccoplist_t* (*joinDiscarded)(eccoplist_t*, uint16_t n, eccoplist_t*);
    eccoplist_t* (*unshift)(eccoperand_t op, eccoplist_t*);
    eccoplist_t* (*unshiftJoin)(eccoperand_t op, eccoplist_t*, eccoplist_t*);
    eccoplist_t* (*unshiftJoin3)(eccoperand_t op, eccoplist_t*, eccoplist_t*, eccoplist_t*);
    eccoplist_t* (*shift)(eccoplist_t*);
    eccoplist_t* (*append)(eccoplist_t*, eccoperand_t op);
    eccoplist_t* (*appendNoop)(eccoplist_t*);
    eccoplist_t* (*createLoop)(eccoplist_t* initial, eccoplist_t* condition, eccoplist_t* step, eccoplist_t* body, int reverseCondition);
    void (*optimizeWithEnvironment)(eccoplist_t*, eccobject_t* environment, uint32_t index);
    void (*dumpTo)(eccoplist_t*, FILE* file);
    ecctextstring_t (*text)(eccoplist_t* oplist);
    const eccoplist_t identity;
};

struct eccastparser_t
{
    eccastlexer_t* lexer;
    eccasttoktype_t previewToken;
    eccobjerror_t* error;

    struct
    {
        eccindexkey_t key;
        char depth;
    }* depths;

    uint16_t depthCount;
    eccobject_t* global;
    eccobjscriptfunction_t* function;
    uint16_t sourceDepth;
    int preferInteger;
    int strictMode;
    int reserveGlobalSlots;
};

struct eccpseudonsparser_t
{
    eccastparser_t* (*createWithLexer)(eccastlexer_t*);
    void (*destroy)(eccastparser_t*);
    eccobjscriptfunction_t* (*parseWithEnvironment)(eccastparser_t* const, eccobject_t* environment, eccobject_t* global);
    const eccastparser_t identity;
};

struct eccmempool_t
{
    eccobjscriptfunction_t** functionList;
    uint32_t functionCount;
    uint32_t functionCapacity;
    eccobject_t** objectList;
    uint32_t objectCount;
    uint32_t objectCapacity;
    ecccharbuffer_t** charsList;
    uint32_t charsCount;
    uint32_t charsCapacity;
};

struct eccpseudonspool_t
{
    void (*setup)(void);
    void (*teardown)(void);
    void (*addFunction)(eccobjscriptfunction_t* function);
    void (*addObject)(eccobject_t* object);
    void (*addChars)(ecccharbuffer_t* chars);
    void (*unmarkAll)(void);
    void (*markValue)(eccvalue_t value);
    void (*markObject)(eccobject_t* object);
    void (*collectUnmarked)(void);
    void (*collectUnreferencedFromIndices)(uint32_t indices[3]);
    void (*unreferenceFromIndices)(uint32_t indices[3]);
    void (*getIndices)(uint32_t indices[3]);
    const eccmempool_t identity;
};

extern const int io_libecc_env_print_max;
extern uint32_t io_libecc_ecc_version;
extern const uint32_t io_libecc_object_ElementMax;

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

extern eccindexkey_t io_libecc_key_none;
extern eccindexkey_t io_libecc_key_prototype;
extern eccindexkey_t io_libecc_key_constructor;
extern eccindexkey_t io_libecc_key_length;
extern eccindexkey_t io_libecc_key_arguments;
extern eccindexkey_t io_libecc_key_callee;
extern eccindexkey_t io_libecc_key_name;
extern eccindexkey_t io_libecc_key_message;
extern eccindexkey_t io_libecc_key_toString;
extern eccindexkey_t io_libecc_key_valueOf;
extern eccindexkey_t io_libecc_key_eval;
extern eccindexkey_t io_libecc_key_value;
extern eccindexkey_t io_libecc_key_writable;
extern eccindexkey_t io_libecc_key_enumerable;
extern eccindexkey_t io_libecc_key_configurable;
extern eccindexkey_t io_libecc_key_get;
extern eccindexkey_t io_libecc_key_set;
extern eccindexkey_t io_libecc_key_join;
extern eccindexkey_t io_libecc_key_toISOString;
extern eccindexkey_t io_libecc_key_input;
extern eccindexkey_t io_libecc_key_index;
extern eccindexkey_t io_libecc_key_lastIndex;
extern eccindexkey_t io_libecc_key_global;
extern eccindexkey_t io_libecc_key_ignoreCase;
extern eccindexkey_t io_libecc_key_multiline;
extern eccindexkey_t io_libecc_key_source;

extern const eccvalue_t ECCValConstNone;
extern const eccvalue_t ECCValConstUndefined;
extern const eccvalue_t ECCValConstTrue;
extern const eccvalue_t ECCValConstFalse;
extern const eccvalue_t ECCValConstNull;

extern eccobject_t* io_libecc_object_prototype;
extern eccobject_t* io_libecc_regexp_prototype;
extern eccobject_t* io_libecc_arguments_prototype;
extern eccobject_t* io_libecc_array_prototype;
extern eccobject_t* io_libecc_boolean_prototype;
extern eccobject_t* io_libecc_date_prototype;
extern eccobject_t* io_libecc_error_prototype;
extern eccobject_t* io_libecc_error_rangePrototype;
extern eccobject_t* io_libecc_error_referencePrototype;
extern eccobject_t* io_libecc_error_syntaxPrototype;
extern eccobject_t* io_libecc_error_typePrototype;
extern eccobject_t* io_libecc_error_uriPrototype;
extern eccobject_t* io_libecc_error_evalPrototype;
extern eccobject_t* io_libecc_string_prototype;
extern eccobject_t* io_libecc_function_prototype;
extern eccobject_t* io_libecc_json_object;
extern eccobject_t* io_libecc_math_object;
extern eccobject_t* io_libecc_number_prototype;

extern eccobjscriptfunction_t* io_libecc_date_constructor;
extern eccobjscriptfunction_t* io_libecc_error_constructor;
extern eccobjscriptfunction_t* io_libecc_error_rangeConstructor;
extern eccobjscriptfunction_t* io_libecc_error_referenceConstructor;
extern eccobjscriptfunction_t* io_libecc_error_syntaxConstructor;
extern eccobjscriptfunction_t* io_libecc_error_typeConstructor;
extern eccobjscriptfunction_t* io_libecc_error_uriConstructor;
extern eccobjscriptfunction_t* io_libecc_error_evalConstructor;
extern eccobjscriptfunction_t* io_libecc_function_constructor;
extern eccobjscriptfunction_t* io_libecc_number_constructor;
extern eccobjscriptfunction_t* io_libecc_string_constructor;
extern eccobjscriptfunction_t* io_libecc_object_constructor;
extern eccobjscriptfunction_t* io_libecc_regexp_constructor;
extern eccobjscriptfunction_t* io_libecc_array_constructor;
extern eccobjscriptfunction_t* io_libecc_boolean_constructor;

extern const eccobjinterntype_t ECCObjTypeRegexp;
extern const eccobjinterntype_t ECCObjTypeArguments;
extern const eccobjinterntype_t ECCObjTypeArray;
extern const eccobjinterntype_t ECCObjTypeBoolean;
extern const eccobjinterntype_t ECCObjTypeDate;
extern const eccobjinterntype_t ECCObjTypeError;
extern const eccobjinterntype_t ECCObjTypeFunction;
extern const eccobjinterntype_t ECCObjTypeObject;
extern const eccobjinterntype_t ECCObjTypeJson;
extern const eccobjinterntype_t ECCObjTypeMath;
extern const eccobjinterntype_t ECCObjTypeNumber;
extern const eccobjinterntype_t ECCObjTypeString;
extern const eccobjinterntype_t ECCObjTypeGlobal;

/* 'namespaces' */
extern const struct eccpseudonsvalue_t ECCNSValue;
extern const struct eccpseudonsecc_t ECCNSScript;
extern const struct eccpseudonsenv_t ECCNSEnv;
extern const struct eccpseudonstext_t ECCNSText;
extern const struct eccpseudonscontext_t ECCNSContext;
extern const struct eccpseudonsarguments_t ECCNSArguments;
extern const struct eccpseudonsobject_t ECCNSObject;

extern const struct eccpseudonskey_t io_libecc_Key;
extern const struct eccpseudonschars_t io_libecc_Chars;
extern const struct eccpseudonsregexp_t io_libecc_RegExp;
extern const struct eccpseudonsarray_t io_libecc_Array;
extern const struct eccpseudonsboolean_t io_libecc_Boolean;
extern const struct eccpseudonsdate_t io_libecc_Date;
extern const struct eccpseudonserror_t io_libecc_Error;
extern const struct eccpseudonsfunction_t io_libecc_Function;
extern const struct eccpseudonsjson_t io_libecc_JSON;
extern const struct eccpseudonsmath_t io_libecc_Math;
extern const struct eccpseudonsnumber_t io_libecc_Number;
extern const struct eccpseudonsstring_t io_libecc_String;
extern const struct eccpseudonsglobal_t io_libecc_Global;
extern const struct eccpseudonsinput_t io_libecc_Input;
extern const struct eccpseudonslexer_t io_libecc_Lexer;
extern const struct eccpseudonsop_t io_libecc_Op;
extern const struct eccpseudonsoplist_t io_libecc_OpList;
extern const struct eccpseudonsparser_t io_libecc_Parser;
extern const struct eccpseudonspool_t io_libecc_Pool;
