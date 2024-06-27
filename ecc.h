
/*
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#pragma once

#include <stdbool.h>
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
typedef struct eccastdepths_t eccastdepths_t;
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
typedef struct eccobjfunction_t eccobjfunction_t;
typedef struct eccobjdate_t eccobjdate_t;
typedef struct eccobjerror_t eccobjerror_t;
typedef struct eccobjregexp_t eccobjregexp_t;
typedef struct eccobjnumber_t eccobjnumber_t;

typedef struct eccrxstate_t eccrxstate_t;

typedef union ecchashmap_t ecchashmap_t;
typedef union ecchashitem_t ecchashitem_t;

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

typedef eccvalue_t (*eccnativefuncptr_t)(eccstate_t* context);
typedef void (*ecctypefnmark_t)(eccobject_t*);
typedef void (*ecctypefncapture_t)(eccobject_t*);
typedef void (*ecctypefnfinalize_t)(eccobject_t*);
typedef int (*eccoperfncompareint_t)(int32_t, int32_t);
typedef int (*eccoperfnwontoverflow_t)(int32_t, int32_t);
typedef eccvalue_t (*eccoperfncomparevalue_t)(eccstate_t *, eccvalue_t, eccvalue_t);
typedef eccvalue_t (*eccoperfnvaluestep_t)(eccstate_t *, eccvalue_t, eccvalue_t);


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

struct eccindexkey_t
{
    union
    {
        uint8_t depth[4];
        uint32_t integer;
    } data;
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
        ecccharbuffer_t* chars;
        eccobject_t* object;
        eccobjerror_t* error;
        eccobjstring_t* string;
        eccobjregexp_t* regexp;
        eccobjnumber_t* number;
        eccobjbool_t* boolean;
        eccobjdate_t* date;
        eccobjfunction_t* function;
        eccvalue_t* reference;
    } data;

    eccindexkey_t key;
    eccvaltype_t type;
    uint8_t flags;
    uint16_t check;
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
    int textIndex;
    int16_t breaker;
    int16_t depth;
    int8_t construct : 1;
    int8_t argumentOffset : 3;
    int8_t strictMode : 1;
    int8_t inEnvironmentObject : 1;
};

struct eccappendbuffer_t
{
    ecccharbuffer_t* value;
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

struct eccobjinterntype_t
{
    const ecctextstring_t* text;

    ecctypefnmark_t mark;
    ecctypefncapture_t capture;
    ecctypefnfinalize_t finalize;

};


union ecchashitem_t
{
    eccvalue_t value;
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
    ecchashitem_t* element;
    ecchashmap_t* hashmap;
    uint32_t elementCount;
    uint32_t elementCapacity;
    uint16_t hashmapCount;
    uint16_t hashmapCapacity;
    int16_t referenceCount;
    uint8_t flags;
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

struct eccidentityarguments_t
{
    eccobject_t object;
};

struct eccidentityarray_t
{
    eccobject_t object;
};

struct eccobjbool_t
{
    eccobject_t object;
    int truth;
};

struct eccobjdate_t
{
    eccobject_t object;
    double ms;
};



struct eccobjerror_t
{
    eccobject_t object;
    ecctextstring_t text;
};

struct eccobjfunction_t
{
    eccobject_t object;
    eccobject_t environment;
    eccobject_t* refObject;
    eccoplist_t* oplist;
    eccobjfunction_t* pair;
    eccvalue_t boundThis;
    ecctextstring_t text;
    const char* name;
    int parameterCount;
    /*eccobjscriptfuncflags_t*/
    int flags;
};

struct eccobjnumber_t
{
    eccobject_t object;
    double value;
};

struct eccobjstring_t
{
    eccobject_t object;
    ecccharbuffer_t* value;
};

struct eccenvidentity_t
{
    char unused;
};

struct eccioinput_t
{
    char name[4096];
    uint32_t length;
    char* bytes;
    uint16_t lineCount;
    uint16_t lineCapacity;
    uint32_t* lines;
    eccvalue_t* attached;
    uint16_t attachedCount;
};


struct eccscriptcontext_t
{
    jmp_buf* envList;
    uint16_t envCount;
    uint16_t envCapacity;
    eccobjfunction_t* global;
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


struct eccoperand_t
{
    eccnativefuncptr_t native;
    eccvalue_t value;
    ecctextstring_t text;
};


struct eccoplist_t
{
    uint32_t count;
    eccoperand_t* ops;
};


struct eccastdepths_t
{
    eccindexkey_t key;
    char depth;
};

struct eccastparser_t
{
    eccastlexer_t* lexer;
    int previewToken;
    eccobjerror_t* error;
    eccastdepths_t* depths;
    uint16_t depthCount;
    eccobject_t* global;
    eccobjfunction_t* function;
    uint16_t sourceDepth;
    int preferInteger;
    int strictMode;
    int reserveGlobalSlots;
};

struct eccmempool_t
{
    eccobjfunction_t** functionList;
    uint32_t functionCount;
    uint32_t functionCapacity;
    eccobject_t** objectList;
    uint32_t objectCount;
    uint32_t objectCapacity;
    ecccharbuffer_t** charsList;
    uint32_t charsCount;
    uint32_t charsCapacity;
};

extern const int io_libecc_env_print_max;
extern const uint32_t io_libecc_object_ElementMax;
extern uint32_t io_libecc_ecc_version;

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

extern eccindexkey_t ECC_ConstKey_none;
extern eccindexkey_t ECC_ConstKey_prototype;
extern eccindexkey_t ECC_ConstKey_constructor;
extern eccindexkey_t ECC_ConstKey_length;
extern eccindexkey_t ECC_ConstKey_arguments;
extern eccindexkey_t ECC_ConstKey_callee;
extern eccindexkey_t ECC_ConstKey_name;
extern eccindexkey_t ECC_ConstKey_message;
extern eccindexkey_t ECC_ConstKey_toString;
extern eccindexkey_t ECC_ConstKey_valueOf;
extern eccindexkey_t ECC_ConstKey_eval;
extern eccindexkey_t ECC_ConstKey_value;
extern eccindexkey_t ECC_ConstKey_writable;
extern eccindexkey_t ECC_ConstKey_enumerable;
extern eccindexkey_t ECC_ConstKey_configurable;
extern eccindexkey_t ECC_ConstKey_get;
extern eccindexkey_t ECC_ConstKey_set;
extern eccindexkey_t ECC_ConstKey_join;
extern eccindexkey_t ECC_ConstKey_toISOString;
extern eccindexkey_t ECC_ConstKey_input;
extern eccindexkey_t ECC_ConstKey_index;
extern eccindexkey_t ECC_ConstKey_lastIndex;
extern eccindexkey_t ECC_ConstKey_global;
extern eccindexkey_t ECC_ConstKey_ignoreCase;
extern eccindexkey_t ECC_ConstKey_multiline;
extern eccindexkey_t ECC_ConstKey_source;

extern const eccvalue_t ECCValConstNone;
extern const eccvalue_t ECCValConstUndefined;
extern const eccvalue_t ECCValConstTrue;
extern const eccvalue_t ECCValConstFalse;
extern const eccvalue_t ECCValConstNull;

extern eccobject_t* ECC_Prototype_Object;
extern eccobject_t* ECC_Prototype_Regexp;
extern eccobject_t* ECC_Prototype_Arguments;
extern eccobject_t* ECC_Prototype_Array;
extern eccobject_t* ECC_Prototype_Boolean;
extern eccobject_t* ECC_Prototype_Date;
extern eccobject_t* ECC_Prototype_Error;
extern eccobject_t* ECC_Prototype_Function;
extern eccobject_t* ECC_Prototype_JSONObject;
extern eccobject_t* ECC_Prototype_MathObject;
extern eccobject_t* ECC_Prototype_Number;
extern eccobject_t* ECC_Prototype_String;
extern eccobject_t* ECC_Prototype_ErrorRangeError;
extern eccobject_t* ECC_Prototype_ErrorReferenceError;
extern eccobject_t* ECC_Prototype_ErrorSyntaxError;
extern eccobject_t* ECC_Prototype_ErrorTypeError;
extern eccobject_t* ECC_Prototype_ErrorUriError;
extern eccobject_t* ECC_Prototype_ErrorEvalError;

extern eccobjfunction_t* ECC_CtorFunc_String;
extern eccobjfunction_t* ECC_CtorFunc_Number;
extern eccobjfunction_t* ECC_CtorFunc_Function;
extern eccobjfunction_t* ECC_CtorFunc_Object;
extern eccobjfunction_t* ECC_CtorFunc_Regexp;
extern eccobjfunction_t* ECC_CtorFunc_Array;
extern eccobjfunction_t* ECC_CtorFunc_Boolean;
extern eccobjfunction_t* ECC_CtorFunc_Date;
extern eccobjfunction_t* ECC_CtorFunc_Error;
extern eccobjfunction_t* ECC_CtorFunc_ErrorRangeError;
extern eccobjfunction_t* ECC_CtorFunc_ErrorReferenceError;
extern eccobjfunction_t* ECC_CtorFunc_ErrorSyntaxError;
extern eccobjfunction_t* ECC_CtorFunc_ErrorTypeError;
extern eccobjfunction_t* ECC_CtorFunc_ErrorUriError;
extern eccobjfunction_t* ECC_CtorFunc_ErrorEvalError;

extern const eccobjinterntype_t ECC_Type_Regexp;
extern const eccobjinterntype_t ECC_Type_Arguments;
extern const eccobjinterntype_t ECC_Type_Array;
extern const eccobjinterntype_t ECC_Type_Boolean;
extern const eccobjinterntype_t ECC_Type_Date;
extern const eccobjinterntype_t ECC_Type_Error;
extern const eccobjinterntype_t ECC_Type_Function;
extern const eccobjinterntype_t ECC_Type_Json;
extern const eccobjinterntype_t ECC_Type_Math;
extern const eccobjinterntype_t ECC_Type_Number;
extern const eccobjinterntype_t ECC_Type_String;
extern const eccobjinterntype_t ECC_Type_Global;
extern const eccobjinterntype_t ECC_Type_Object;

eccioinput_t* ecc_ioinput_create(void);
eccioinput_t* ecc_ioinput_createfromfile(const char* filename);
eccioinput_t* ecc_ioinput_createfrombytes(const char* bytes, uint32_t length, const char* name, ...);
void ecc_ioinput_destroy(eccioinput_t*);
void ecc_ioinput_printtext(eccioinput_t*, ecctextstring_t text, int32_t ofLine, ecctextstring_t ofText, const char* ofInput, int fullLine);
int32_t ecc_ioinput_findline(eccioinput_t*, ecctextstring_t text);
eccvalue_t ecc_ioinput_attachvalue(eccioinput_t*, eccvalue_t value);

void ecc_args_setup(void);
void ecc_args_teardown(void);
eccobject_t *ecc_args_createsized(uint32_t size);
eccobject_t *ecc_args_createwithclist(int count, const char *list[]);

void ecc_number_setup(void);
void ecc_number_teardown(void);
eccobjnumber_t *ecc_number_create(double binary);
void ecc_bool_setup(void);
void ecc_bool_teardown(void);
eccobjbool_t *ecc_bool_create(int truth);
void ecc_date_setup(void);
void ecc_date_teardown(void);
eccobjdate_t *ecc_date_create(double ms);
void ecc_json_setup(void);
void ecc_json_teardown(void);
void ecc_libmath_teardown(void);
void ecc_libmath_setup(void);


eccvalue_t ecc_value_truth(int truth);
eccvalue_t ecc_value_integer(int32_t integer);
eccvalue_t ecc_value_binary(double binary);
eccvalue_t ecc_value_buffer(const char buffer[7], uint8_t units);
eccvalue_t ecc_value_key(eccindexkey_t key);
eccvalue_t ecc_value_text(const ecctextstring_t* text);
eccvalue_t ecc_value_chars(ecccharbuffer_t* chars);
eccvalue_t ecc_value_object(eccobject_t*);
eccvalue_t ecc_value_error(eccobjerror_t*);
eccvalue_t ecc_value_string(eccobjstring_t*);
eccvalue_t ecc_value_regexp(eccobjregexp_t*);
eccvalue_t ecc_value_number(eccobjnumber_t*);
eccvalue_t ecc_value_boolean(eccobjbool_t*);
eccvalue_t ecc_value_date(eccobjdate_t*);
eccvalue_t ecc_value_function(eccobjfunction_t*);
eccvalue_t ecc_value_host(eccobject_t*);
eccvalue_t ecc_value_reference(eccvalue_t*);
int ecc_value_isprimitive(eccvalue_t);
int ecc_value_isboolean(eccvalue_t);
int ecc_value_isnumber(eccvalue_t);
int ecc_value_isstring(eccvalue_t);
int ecc_value_isobject(eccvalue_t);
int ecc_value_isdynamic(eccvalue_t);
int ecc_value_istrue(eccvalue_t);
eccvalue_t ecc_value_toprimitive(eccstate_t*, eccvalue_t, int);
eccvalue_t ecc_value_tobinary(eccstate_t*, eccvalue_t);
eccvalue_t ecc_value_tointeger(eccstate_t*, eccvalue_t);
eccvalue_t ecc_value_binarytostring(double binary, int base);
eccvalue_t ecc_value_tostring(eccstate_t*, eccvalue_t);
int32_t ecc_value_stringlength(const eccvalue_t*);
const char* ecc_value_stringbytes(const eccvalue_t*);
ecctextstring_t ecc_value_textof(const eccvalue_t* string);
eccvalue_t ecc_value_toobject(eccstate_t*, eccvalue_t);
eccvalue_t ecc_value_objectvalue(eccobject_t*);
int ecc_value_objectisarray(eccobject_t*);
eccvalue_t ecc_value_totype(eccvalue_t);
eccvalue_t ecc_value_equals(eccstate_t*, eccvalue_t, eccvalue_t);
eccvalue_t ecc_value_same(eccstate_t*, eccvalue_t, eccvalue_t);
eccvalue_t ecc_value_add(eccstate_t*, eccvalue_t, eccvalue_t);
eccvalue_t ecc_value_subtract(eccstate_t*, eccvalue_t, eccvalue_t);
eccvalue_t ecc_value_less(eccstate_t*, eccvalue_t, eccvalue_t);
eccvalue_t ecc_value_more(eccstate_t*, eccvalue_t, eccvalue_t);
eccvalue_t ecc_value_lessorequal(eccstate_t*, eccvalue_t, eccvalue_t);
eccvalue_t ecc_value_moreorequal(eccstate_t*, eccvalue_t, eccvalue_t);
const char* ecc_value_typename(int);
const char* ecc_value_maskname(int);
void ecc_value_dumpto(eccvalue_t, FILE*);

eccscriptcontext_t* ecc_script_create(void);
void ecc_script_destroy(eccscriptcontext_t*);
void ecc_script_addvalue(eccscriptcontext_t*, const char* name, eccvalue_t value, int);
void ecc_script_addfunction(eccscriptcontext_t*, const char* name, const eccnativefuncptr_t native, int argumentCount, int);
int ecc_script_evalinput(eccscriptcontext_t*, eccioinput_t*, int);
void ecc_script_evalinputwithcontext(eccscriptcontext_t*, eccioinput_t*, eccstate_t* context);
jmp_buf* ecc_script_pushenv(eccscriptcontext_t*);
void ecc_script_popenv(eccscriptcontext_t*);
void ecc_script_jmpenv(eccscriptcontext_t*, eccvalue_t value);
void ecc_script_fatal(const char* format, ...);
eccioinput_t* ecc_script_findinput(eccscriptcontext_t* self, ecctextstring_t text);
void ecc_script_printtextinput(eccscriptcontext_t*, ecctextstring_t text, int fullLine);
void ecc_script_garbagecollect(eccscriptcontext_t*);

void ecc_globals_setup(void);
void ecc_globals_teardown(void);
eccobjfunction_t *ecc_globals_create(void);


int ecc_array_valueisarray(eccvalue_t value);
uint32_t ecc_array_getlengtharrayval(eccvalue_t value);
uint32_t ecc_array_getlength(eccstate_t *context, eccobject_t *object);
void ecc_array_objectresize(eccstate_t *context, eccobject_t *object, uint32_t length);
void ecc_array_valueappendfromelement(eccstate_t *context, eccvalue_t value, eccobject_t *object, uint32_t *element);
eccvalue_t ecc_array_tochars(eccstate_t *context, eccvalue_t thisval, ecctextstring_t separator);
int ecc_array_gcd(int m, int n);
void ecc_array_rotate(eccobject_t *object, eccstate_t *context, uint32_t first, uint32_t half, uint32_t last);
void ecc_array_sortinplace(eccstate_t *context, eccobject_t *object, eccobjfunction_t *function, int first, int last);
void ecc_array_setup(void);
void ecc_array_teardown(void);
eccobject_t *ecc_array_create(void);
eccobject_t *ecc_array_createsized(uint32_t size);

void ecc_string_setup(void);
void ecc_string_teardown(void);
eccobjstring_t *ecc_string_create(ecccharbuffer_t *chars);
eccvalue_t ecc_string_valueatindex(eccobjstring_t *self, int32_t index);
ecctextstring_t ecc_string_textatindex(const char *chars, int32_t length, int32_t position, int enableReverse);
int32_t ecc_string_unitindex(const char *chars, int32_t max, int32_t unit);


void ecc_regexp_setup(void);
void ecc_regexp_teardown(void);
eccobjregexp_t *ecc_regexp_create(ecccharbuffer_t *s, eccobjerror_t **error, int options);
eccobjregexp_t *ecc_regexp_createwith(eccstate_t *context, eccvalue_t pattern, eccvalue_t flags);
int ecc_regexp_matchwithstate(eccobjregexp_t *self, eccrxstate_t *state);

eccvalue_t ecc_oper_dotrapop(eccstate_t *context, int offset);
eccvalue_t ecc_oper_retain(eccvalue_t value);
eccvalue_t ecc_oper_release(eccvalue_t value);
int ecc_oper_intlessthan(int32_t a, int32_t b);
int ecc_oper_intlessequal(int32_t a, int32_t b);
int ecc_oper_intgreaterthan(int32_t a, int32_t b);
int ecc_oper_intgreaterequal(int32_t a, int32_t b);
int ecc_oper_testintegerwontofpos(int32_t a, int32_t positive);
int ecc_oper_testintwontofneg(int32_t a, int32_t negative);
eccoperand_t ecc_oper_make(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text);
const char *ecc_oper_tochars(const eccnativefuncptr_t native);
eccvalue_t ecc_oper_nextopvalue(eccstate_t *context);
eccvalue_t ecc_oper_replacerefvalue(eccvalue_t *ref, eccvalue_t value);
eccvalue_t ecc_oper_callops(eccstate_t *context, eccobject_t *environment);
eccvalue_t ecc_oper_callvalue(eccstate_t *context, eccvalue_t value, eccvalue_t thisval, int32_t argumentCount, int construct, const ecctextstring_t *textCall);
eccvalue_t ecc_oper_callopsrelease(eccstate_t *context, eccobject_t *environment);
void ecc_oper_makeenvwithargs(eccobject_t *environment, eccobject_t *arguments, int32_t parameterCount);
void ecc_oper_makeenvandargswithva(eccobject_t *environment, int32_t parameterCount, int32_t argumentCount, va_list ap);
void ecc_oper_populateenvwithva(eccobject_t *environment, int32_t parameterCount, int32_t argumentCount, va_list ap);
void ecc_oper_makestackenvandargswithops(eccstate_t *context, eccobject_t *environment, eccobject_t *arguments, int32_t parameterCount, int32_t argumentCount);
void ecc_oper_makeenvandargswithops(eccstate_t *context, eccobject_t *environment, eccobject_t *arguments, int32_t parameterCount, int32_t argumentCount);
void ecc_oper_populateenvwithops(eccstate_t *context, eccobject_t *environment, int32_t parameterCount, int32_t argumentCount);
eccvalue_t ecc_oper_callfunctionarguments(eccstate_t *context, int offset, eccobjfunction_t *function, eccvalue_t thisval, eccobject_t *arguments);
eccvalue_t ecc_oper_callfunctionva(eccstate_t *context, int offset, eccobjfunction_t *function, eccvalue_t thisval, int argumentCount, va_list ap);
eccvalue_t ecc_oper_callfunction(eccstate_t *context, eccobjfunction_t *const function, eccvalue_t thisval, int32_t argumentCount, int construct);
eccvalue_t ecc_oper_construct(eccstate_t *context);
eccvalue_t ecc_oper_call(eccstate_t *context);
eccvalue_t ecc_oper_eval(eccstate_t *context);
eccvalue_t ecc_oper_noop(eccstate_t *context);
eccvalue_t ecc_oper_value(eccstate_t *context);
eccvalue_t ecc_oper_valueconstref(eccstate_t *context);
eccvalue_t ecc_oper_text(eccstate_t *context);
eccvalue_t ecc_oper_regexp(eccstate_t *context);
eccvalue_t ecc_oper_function(eccstate_t *context);
eccvalue_t ecc_oper_object(eccstate_t *context);
eccvalue_t ecc_oper_array(eccstate_t *context);
eccvalue_t ecc_oper_getthis(eccstate_t *context);
eccvalue_t *ecc_oper_localref(eccstate_t *context, eccindexkey_t key, const ecctextstring_t *text, int required);
eccvalue_t ecc_oper_createlocalref(eccstate_t *context);
eccvalue_t ecc_oper_getlocalrefornull(eccstate_t *context);
eccvalue_t ecc_oper_getlocalref(eccstate_t *context);
eccvalue_t ecc_oper_getlocal(eccstate_t *context);
eccvalue_t ecc_oper_setlocal(eccstate_t *context);
eccvalue_t ecc_oper_deletelocal(eccstate_t *context);
eccvalue_t ecc_oper_getlocalslotref(eccstate_t *context);
eccvalue_t ecc_oper_getlocalslot(eccstate_t *context);
eccvalue_t ecc_oper_setlocalslot(eccstate_t *context);
eccvalue_t ecc_oper_deletelocalslot(eccstate_t *context);
eccvalue_t ecc_oper_getparentslotref(eccstate_t *context);
eccvalue_t ecc_oper_getparentslot(eccstate_t *context);
eccvalue_t ecc_oper_setparentslot(eccstate_t *context);
eccvalue_t ecc_oper_deleteparentslot(eccstate_t *context);
void ecc_oper_prepareobject(eccstate_t *context, eccvalue_t *object);
eccvalue_t ecc_oper_getmemberref(eccstate_t *context);
eccvalue_t ecc_oper_getmember(eccstate_t *context);
eccvalue_t ecc_oper_setmember(eccstate_t *context);
eccvalue_t ecc_oper_callmember(eccstate_t *context);
eccvalue_t ecc_oper_deletemember(eccstate_t *context);
void ecc_oper_prepareobjectproperty(eccstate_t *context, eccvalue_t *object, eccvalue_t *property);
eccvalue_t ecc_oper_getpropertyref(eccstate_t *context);
eccvalue_t ecc_oper_getproperty(eccstate_t *context);
eccvalue_t ecc_oper_setproperty(eccstate_t *context);
eccvalue_t ecc_oper_callproperty(eccstate_t *context);
eccvalue_t ecc_oper_deleteproperty(eccstate_t *context);
eccvalue_t ecc_oper_pushenvironment(eccstate_t *context);
eccvalue_t ecc_oper_popenvironment(eccstate_t *context);
eccvalue_t ecc_oper_exchange(eccstate_t *context);
eccvalue_t ecc_oper_typeof(eccstate_t *context);
eccvalue_t ecc_oper_equal(eccstate_t *context);
eccvalue_t ecc_oper_notequal(eccstate_t *context);
eccvalue_t ecc_oper_identical(eccstate_t *context);
eccvalue_t ecc_oper_notidentical(eccstate_t *context);
eccvalue_t ecc_oper_less(eccstate_t *context);
eccvalue_t ecc_oper_lessorequal(eccstate_t *context);
eccvalue_t ecc_oper_more(eccstate_t *context);
eccvalue_t ecc_oper_moreorequal(eccstate_t *context);
eccvalue_t ecc_oper_instanceof(eccstate_t *context);
eccvalue_t ecc_oper_in(eccstate_t *context);
eccvalue_t ecc_oper_add(eccstate_t *context);
eccvalue_t ecc_oper_minus(eccstate_t *context);
eccvalue_t ecc_oper_multiply(eccstate_t *context);
eccvalue_t ecc_oper_divide(eccstate_t *context);
eccvalue_t ecc_oper_modulo(eccstate_t *context);
eccvalue_t ecc_oper_leftshift(eccstate_t *context);
eccvalue_t ecc_oper_rightshift(eccstate_t *context);
eccvalue_t ecc_oper_unsignedrightshift(eccstate_t *context);
eccvalue_t ecc_oper_bitwiseand(eccstate_t *context);
eccvalue_t ecc_oper_bitwisexor(eccstate_t *context);
eccvalue_t ecc_oper_bitwiseor(eccstate_t *context);
eccvalue_t ecc_oper_logicaland(eccstate_t *context);
eccvalue_t ecc_oper_logicalor(eccstate_t *context);
eccvalue_t ecc_oper_positive(eccstate_t *context);
eccvalue_t ecc_oper_negative(eccstate_t *context);
eccvalue_t ecc_oper_invert(eccstate_t *context);
eccvalue_t ecc_oper_logicalnot(eccstate_t *context);
eccvalue_t ecc_oper_incrementref(eccstate_t *context);
eccvalue_t ecc_oper_decrementref(eccstate_t *context);
eccvalue_t ecc_oper_postincrementref(eccstate_t *context);
eccvalue_t ecc_oper_postdecrementref(eccstate_t *context);
eccvalue_t ecc_oper_addassignref(eccstate_t *context);
eccvalue_t ecc_oper_minusassignref(eccstate_t *context);
eccvalue_t ecc_oper_multiplyassignref(eccstate_t *context);
eccvalue_t ecc_oper_divideassignref(eccstate_t *context);
eccvalue_t ecc_oper_moduloassignref(eccstate_t *context);
eccvalue_t ecc_oper_leftshiftassignref(eccstate_t *context);
eccvalue_t ecc_oper_rightshiftassignref(eccstate_t *context);
eccvalue_t ecc_oper_unsignedrightshiftassignref(eccstate_t *context);
eccvalue_t ecc_oper_bitandassignref(eccstate_t *context);
eccvalue_t ecc_oper_bitxorassignref(eccstate_t *context);
eccvalue_t ecc_oper_bitorassignref(eccstate_t *context);
eccvalue_t ecc_oper_debugger(eccstate_t *context);
eccvalue_t ecc_oper_try(eccstate_t *context);
eccvalue_t ecc_oper_throw(eccstate_t *context);
eccvalue_t ecc_oper_with(eccstate_t *context);
eccvalue_t ecc_oper_next(eccstate_t *context);
eccvalue_t ecc_oper_nextif(eccstate_t *context);
eccvalue_t ecc_oper_autoreleaseexpression(eccstate_t *context);
eccvalue_t ecc_oper_autoreleasediscard(eccstate_t *context);
eccvalue_t ecc_oper_expression(eccstate_t *context);
eccvalue_t ecc_oper_discard(eccstate_t *context);
eccvalue_t ecc_oper_discardn(eccstate_t *context);
eccvalue_t ecc_oper_jump(eccstate_t *context);
eccvalue_t ecc_oper_jumpif(eccstate_t *context);
eccvalue_t ecc_oper_jumpifnot(eccstate_t *context);
eccvalue_t ecc_oper_result(eccstate_t *context);
eccvalue_t ecc_oper_repopulate(eccstate_t *context);
eccvalue_t ecc_oper_resultvoid(eccstate_t *context);
eccvalue_t ecc_oper_switchop(eccstate_t *context);
eccvalue_t ecc_oper_breaker(eccstate_t *context);
eccvalue_t ecc_oper_iterate(eccstate_t *context);

eccvalue_t ecc_oper_iterateintegerref(eccstate_t *context, eccoperfncompareint_t, eccoperfnwontoverflow_t wontOverflow, eccoperfncomparevalue_t compareValue, eccoperfnvaluestep_t valueStep);
eccvalue_t ecc_oper_iteratelessref(eccstate_t *context);
eccvalue_t ecc_oper_iteratelessorequalref(eccstate_t *context);
eccvalue_t ecc_oper_iteratemoreref(eccstate_t *context);
eccvalue_t ecc_oper_iteratemoreorequalref(eccstate_t *context);
eccvalue_t ecc_oper_iterateinref(eccstate_t *context);

eccastlexer_t* ecc_astlex_createwithinput(eccioinput_t*);
void ecc_astlex_destroy(eccastlexer_t*);
int ecc_astlex_nexttoken(eccastlexer_t*);
const char* ecc_astlex_tokenchars(int token, char buffer[4]);
eccvalue_t ecc_astlex_scanbinary(ecctextstring_t text, int);
eccvalue_t ecc_astlex_scaninteger(ecctextstring_t text, int base, int);
uint32_t ecc_astlex_scanelement(ecctextstring_t text);
uint8_t ecc_astlex_uint8hex(char a, char b);
uint16_t ecc_astlex_uint16hex(char a, char b, char c, char d);



eccoplist_t* ecc_astparse_new(eccastparser_t*);
eccoplist_t* ecc_astparse_assignment(eccastparser_t*, int noIn);
eccoplist_t* ecc_astparse_expression(eccastparser_t*, int noIn);
eccoplist_t* ecc_astparse_statement(eccastparser_t*);
eccoplist_t* ecc_astparse_function(eccastparser_t*, int isDeclaration, int isGetter, int isSetter);
eccoplist_t* ecc_astparse_sourceelements(eccastparser_t*);
eccastparser_t* ecc_astparse_createwithlexer(eccastlexer_t*);
void ecc_astparse_destroy(eccastparser_t*);
eccobjfunction_t* ecc_astparse_parsewithenvironment(eccastparser_t* const, eccobject_t* environment, eccobject_t* global);


/* ../context.c */
static __inline __uint16_t __bswap_16(__uint16_t __bsx);
static __inline __uint32_t __bswap_32(__uint32_t __bsx);
static __inline __uint16_t __uint16_identity(__uint16_t __x);
static __inline __uint32_t __uint32_identity(__uint32_t __x);
static __inline __uint64_t __uint64_identity(__uint64_t __x);
void ecc_context_rangeerror(eccstate_t *self, ecccharbuffer_t *chars);
void ecc_context_referenceerror(eccstate_t *self, ecccharbuffer_t *chars);
void ecc_context_syntaxerror(eccstate_t *self, ecccharbuffer_t *chars);
void ecc_context_typeerror(eccstate_t *self, ecccharbuffer_t *chars);
void ecc_context_urierror(eccstate_t *self, ecccharbuffer_t *chars);
void ecc_context_throw(eccstate_t *self, eccvalue_t value);
eccvalue_t ecc_context_callfunction(eccstate_t *self, eccobjfunction_t *function, eccvalue_t thisval, int argumentCount, ...);
int ecc_context_argumentcount(eccstate_t *self);
eccvalue_t ecc_context_argument(eccstate_t *self, int argumentIndex);
void ecc_context_replaceargument(eccstate_t *self, int argumentIndex, eccvalue_t value);
eccvalue_t ecc_context_this(eccstate_t *self);
void ecc_context_assertthistype(eccstate_t *self, int type);
void ecc_context_assertthismask(eccstate_t *self, int mask);
void ecc_context_assertthiscoercibleprimitive(eccstate_t *self);
void ecc_context_settext(eccstate_t *self, const ecctextstring_t *text);
void ecc_context_settexts(eccstate_t *self, const ecctextstring_t *text, const ecctextstring_t *textAlt);
void ecc_context_settextindex(eccstate_t *self, int index);
void ecc_context_settextindexargument(eccstate_t *self, int argument);
ecctextstring_t ecc_context_textseek(eccstate_t *self);
void ecc_context_rewindstatement(eccstate_t *context);
void ecc_context_printbacktrace(eccstate_t *context);
eccobject_t *ecc_context_environmentroot(eccstate_t *context);



ecccharbuffer_t* ecc_charbuf_createva(int32_t length, const char* format, va_list ap);
ecccharbuffer_t* ecc_charbuf_create(const char* format, ...);
ecccharbuffer_t* ecc_charbuf_createsized(int32_t length);
ecccharbuffer_t* ecc_charbuf_createwithbytes(int32_t length, const char* bytes);
void ecc_charbuf_beginappend(eccappendbuffer_t*);
void ecc_charbuf_append(eccappendbuffer_t*, const char* format, ...);
void ecc_charbuf_appendcodepoint(eccappendbuffer_t*, uint32_t cp);
void ecc_charbuf_appendvalue(eccappendbuffer_t*, eccstate_t* context, eccvalue_t value);
void ecc_charbuf_appendbinary(eccappendbuffer_t*, double binary, int base);
void ecc_charbuf_normalizebinary(eccappendbuffer_t*);
eccvalue_t ecc_charbuf_endappend(eccappendbuffer_t*);
void ecc_charbuf_destroy(ecccharbuffer_t*);
uint8_t ecc_charbuf_codepointlength(uint32_t cp);
uint8_t ecc_charbuf_writecodepoint(char*, uint32_t cp);



ecctextstring_t ecc_textbuf_make(const char* bytes, int32_t length);
ecctextstring_t ecc_textbuf_join(ecctextstring_t from, ecctextstring_t to);
ecctextchar_t ecc_textbuf_character(ecctextstring_t);
ecctextchar_t ecc_textbuf_nextcharacter(ecctextstring_t* text);
ecctextchar_t ecc_textbuf_prevcharacter(ecctextstring_t* text);
void ecc_textbuf_advance(ecctextstring_t* text, int32_t units);
uint16_t ecc_textbuf_toutf16length(ecctextstring_t);
uint16_t ecc_textbuf_toutf16(ecctextstring_t, uint16_t* wbuffer);
char* ecc_textbuf_tolower(ecctextstring_t, char* x2buffer);
char* ecc_textbuf_toupper(ecctextstring_t, char* x3buffer);
int ecc_textbuf_isspace(ecctextchar_t);
int ecc_textbuf_isdigit(ecctextchar_t);
int ecc_textbuf_isword(ecctextchar_t);
int ecc_textbuf_islinefeed(ecctextchar_t);



void ecc_object_setup(void);
void ecc_object_teardown(void);
eccobject_t* ecc_object_create(eccobject_t* prototype);
eccobject_t* ecc_object_createsized(eccobject_t* prototype, uint16_t size);
eccobject_t* ecc_object_createtyped(const eccobjinterntype_t* type);
eccobject_t* ecc_object_initialize(eccobject_t*, eccobject_t* prototype);
eccobject_t* ecc_object_initializesized(eccobject_t*, eccobject_t* prototype, uint16_t size);
eccobject_t* ecc_object_finalize(eccobject_t*);
eccobject_t* ecc_object_copy(const eccobject_t* original);
void ecc_object_destroy(eccobject_t*);
eccvalue_t ecc_object_getmember(eccstate_t*, eccobject_t*, eccindexkey_t key);
eccvalue_t ecc_object_putmember(eccstate_t*, eccobject_t*, eccindexkey_t key, eccvalue_t);
eccvalue_t* ecc_object_member(eccobject_t*, eccindexkey_t key, int);
eccvalue_t* ecc_object_addmember(eccobject_t*, eccindexkey_t key, eccvalue_t, int);
int ecc_object_deletemember(eccobject_t*, eccindexkey_t key);
eccvalue_t ecc_object_getelement(eccstate_t*, eccobject_t*, uint32_t index);
eccvalue_t ecc_object_putelement(eccstate_t*, eccobject_t*, uint32_t index, eccvalue_t);
eccvalue_t* ecc_object_element(eccobject_t*, uint32_t index, int);
eccvalue_t* ecc_object_addelement(eccobject_t*, uint32_t index, eccvalue_t, int);
int ecc_object_deleteelement(eccobject_t*, uint32_t index);
eccvalue_t ecc_object_getproperty(eccstate_t*, eccobject_t*, eccvalue_t primitive);
eccvalue_t ecc_object_putproperty(eccstate_t*, eccobject_t*, eccvalue_t primitive, eccvalue_t);
eccvalue_t* ecc_object_property(eccobject_t*, eccvalue_t primitive, int);
eccvalue_t* ecc_object_addproperty(eccobject_t*, eccvalue_t primitive, eccvalue_t, int);
int ecc_object_deleteproperty(eccobject_t*, eccvalue_t primitive);
eccvalue_t ecc_object_putvalue(eccstate_t*, eccobject_t*, eccvalue_t*, eccvalue_t);
eccvalue_t ecc_object_getvalue(eccstate_t*, eccobject_t*, eccvalue_t*);
void ecc_object_packvalue(eccobject_t*);
void ecc_object_stripmap(eccobject_t*);
void ecc_object_reserveslots(eccobject_t*, uint16_t slots);
int ecc_object_resizeelement(eccobject_t*, uint32_t size);
void ecc_object_populateelementwithclist(eccobject_t*, uint32_t count, const char* list[]);
eccvalue_t ecc_object_tostringfn(eccstate_t*);
void ecc_object_dumpto(eccobject_t*, FILE* file);



eccoplist_t* ecc_oplist_create(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text);
void ecc_oplist_destroy(eccoplist_t*);
eccoplist_t* ecc_oplist_join(eccoplist_t*, eccoplist_t*);
eccoplist_t* ecc_oplist_join3(eccoplist_t*, eccoplist_t*, eccoplist_t*);
eccoplist_t* ecc_oplist_joindiscarded(eccoplist_t*, uint16_t n, eccoplist_t*);
eccoplist_t* ecc_oplist_unshift(eccoperand_t op, eccoplist_t*);
eccoplist_t* ecc_oplist_unshiftjoin(eccoperand_t op, eccoplist_t*, eccoplist_t*);
eccoplist_t* ecc_oplist_unshiftjoin3(eccoperand_t op, eccoplist_t*, eccoplist_t*, eccoplist_t*);
eccoplist_t* ecc_oplist_shift(eccoplist_t*);
eccoplist_t* ecc_oplist_append(eccoplist_t*, eccoperand_t op);
eccoplist_t* ecc_oplist_appendnoop(eccoplist_t*);
eccoplist_t* ecc_oplist_createloop(eccoplist_t* initial, eccoplist_t* condition, eccoplist_t* step, eccoplist_t* body, int reverseCondition);
void ecc_oplist_optimizewithenvironment(eccoplist_t*, eccobject_t* environment, uint32_t index);
void ecc_oplist_dumpto(eccoplist_t*, FILE* file);
ecctextstring_t ecc_oplist_text(eccoplist_t* oplist);



void eccfunction_capture(eccobject_t *object);
void eccfunction_mark(eccobject_t *object);
eccvalue_t eccfunction_toChars(eccstate_t *context, eccvalue_t value);
void ecc_function_setup(void);
void ecc_function_teardown(void);
eccobjfunction_t *ecc_function_create(eccobject_t *environment);
eccobjfunction_t *ecc_function_createsized(eccobject_t *environment, uint32_t size);
eccobjfunction_t *ecc_function_createwithnative(const eccnativefuncptr_t native, int parameterCount);
eccobjfunction_t *ecc_function_copy(eccobjfunction_t *original);
void ecc_function_destroy(eccobjfunction_t *self);
void ecc_function_addmember(eccobjfunction_t *self, const char *name, eccvalue_t value, int flags);
eccobjfunction_t *ecc_function_addmethod(eccobjfunction_t *self, const char *name, const eccnativefuncptr_t native, int parameterCount, int flags);
void ecc_function_addvalue(eccobjfunction_t *self, const char *name, eccvalue_t value, int flags);
eccobjfunction_t *ecc_function_addfunction(eccobjfunction_t *self, const char *name, const eccnativefuncptr_t native, int parameterCount, int flags);
eccobjfunction_t *ecc_function_addto(eccobject_t *object, const char *name, const eccnativefuncptr_t native, int parameterCount, int flags);
void ecc_function_linkprototype(eccobjfunction_t *self, eccvalue_t prototype, int flags);
void ecc_function_setupbuiltinobject(eccobjfunction_t **constructor, const eccnativefuncptr_t native, int parameterCount, eccobject_t **prototype, eccvalue_t prototypeValue, const eccobjinterntype_t *type);
eccvalue_t ecc_function_accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter);


void ecc_keyidx_setup(void);
void ecc_keyidx_teardown(void);
eccindexkey_t ecc_keyidx_makewithcstring(const char* cString);
eccindexkey_t ecc_keyidx_makewithtext(const ecctextstring_t text, int flags);
eccindexkey_t ecc_keyidx_search(const ecctextstring_t text);
int ecc_keyidx_isequal(eccindexkey_t, eccindexkey_t);
const ecctextstring_t* ecc_keyidx_textof(eccindexkey_t);
void ecc_keyidx_dumpto(eccindexkey_t, FILE*);

void ecc_mempool_markobject(eccobject_t *object);
void ecc_mempool_markchars(ecccharbuffer_t *chars);
void ecc_mempool_setup(void);
void ecc_mempool_teardown(void);
void ecc_mempool_addfunction(eccobjfunction_t *function);
void ecc_mempool_addobject(eccobject_t *object);
void ecc_mempool_addchars(ecccharbuffer_t *chars);
void ecc_mempool_unmarkall(void);
void ecc_mempool_markvalue(eccvalue_t value);
void ecc_mempool_releaseobject(eccobject_t *object);
eccvalue_t ecc_mempool_releasevalue(eccvalue_t value);
eccvalue_t ecc_mempool_retainvalue(eccvalue_t value);
void ecc_mempool_cleanupobject(eccobject_t *object);
void ecc_mempool_captureobject(eccobject_t *object);
void ecc_mempool_collectunmarked(void);
void ecc_mempool_collectunreferencedfromindices(uint32_t indices[3]);
void ecc_mempool_unreferencefromindices(uint32_t indices[3]);
void ecc_mempool_getindices(uint32_t indices[3]);



void ecc_env_setup(void);
void ecc_env_teardown(void);
void eccenv_textc(int c, int a);
void eccenv_vprintc(const char *format, va_list ap);
void ecc_env_print(const char *format, ...);
void ecc_env_printcolor(int color, int attribute, const char *format, ...);
void ecc_env_printerror(int typeLength, const char *type, const char *format, ...);
void ecc_env_printwarning(const char *format, ...);
void ecc_env_newline(void);
double ecc_env_currenttime(void);


ecccharbuffer_t *ecc_error_messagevalue(eccstate_t *context, eccvalue_t value);
eccvalue_t ecc_error_tochars(eccstate_t *context, eccvalue_t value);
eccobjerror_t *ecc_error_create(eccobject_t *errorPrototype, ecctextstring_t text, ecccharbuffer_t *message);
void ecc_error_setupbo(eccobjfunction_t **pctor, const eccnativefuncptr_t native, int paramcnt, eccobject_t **prototype, const ecctextstring_t *name);
void ecc_error_setup(void);
void ecc_error_teardown(void);
eccobjerror_t *ecc_error_error(ecctextstring_t text, ecccharbuffer_t *message);
eccobjerror_t *ecc_error_rangeerror(ecctextstring_t text, ecccharbuffer_t *message);
eccobjerror_t *ecc_error_referenceerror(ecctextstring_t text, ecccharbuffer_t *message);
eccobjerror_t *ecc_error_syntaxerror(ecctextstring_t text, ecccharbuffer_t *message);
eccobjerror_t *ecc_error_typeerror(ecctextstring_t text, ecccharbuffer_t *message);
eccobjerror_t *ecc_error_urierror(ecctextstring_t text, ecccharbuffer_t *message);
eccobjerror_t *ecc_error_evalerror(ecctextstring_t text, ecccharbuffer_t *message);
void ecc_error_destroy(eccobjerror_t *self);



