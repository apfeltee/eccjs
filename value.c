//
//  value.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

#define valueMake(T)          \
    {                         \
        .type = T, .check = 1 \
    }

const eccvalue_t ECCValConstNone = { { 0 }, {}, 0, 0, 0 };
const eccvalue_t ECCValConstUndefined = valueMake(ECC_VALTYPE_UNDEFINED);
const eccvalue_t ECCValConstTrue = valueMake(ECC_VALTYPE_TRUE);
const eccvalue_t ECCValConstFalse = valueMake(ECC_VALTYPE_FALSE);
const eccvalue_t ECCValConstNull = valueMake(ECC_VALTYPE_NULL);

// MARK: - Static Members

// MARK: - Methods

// make

static eccvalue_t nsvaluefn_truth(int truth);
static eccvalue_t nsvaluefn_integer(int32_t integer);
static eccvalue_t nsvaluefn_binary(double binary);
static eccvalue_t nsvaluefn_buffer(const char buffer[7], uint8_t units);
static eccvalue_t nsvaluefn_key(eccindexkey_t key);
static eccvalue_t nsvaluefn_text(const ecctextstring_t* text);
static eccvalue_t nsvaluefn_chars(ecccharbuffer_t* chars);
static eccvalue_t nsvaluefn_object(eccobject_t*);
static eccvalue_t nsvaluefn_error(eccobjerror_t*);
static eccvalue_t nsvaluefn_string(eccobjstring_t*);
static eccvalue_t nsvaluefn_regexp(eccobjregexp_t*);
static eccvalue_t nsvaluefn_number(eccobjnumber_t*);
static eccvalue_t nsvaluefn_boolean(eccobjbool_t*);
static eccvalue_t nsvaluefn_date(eccobjdate_t*);
static eccvalue_t nsvaluefn_function(eccobjfunction_t*);
static eccvalue_t nsvaluefn_host(eccobject_t*);
static eccvalue_t nsvaluefn_reference(eccvalue_t*);
static int nsvaluefn_isPrimitive(eccvalue_t);
static int nsvaluefn_isBoolean(eccvalue_t);
static int nsvaluefn_isNumber(eccvalue_t);
static int nsvaluefn_isString(eccvalue_t);
static int nsvaluefn_isObject(eccvalue_t);
static int nsvaluefn_isDynamic(eccvalue_t);
static int nsvaluefn_isTrue(eccvalue_t);
static eccvalue_t nsvaluefn_toPrimitive(eccstate_t*, eccvalue_t, eccvalhint_t);
static eccvalue_t nsvaluefn_toBinary(eccstate_t*, eccvalue_t);
static eccvalue_t nsvaluefn_toInteger(eccstate_t*, eccvalue_t);
static eccvalue_t nsvaluefn_binaryToString(double binary, int base);
static eccvalue_t nsvaluefn_toString(eccstate_t*, eccvalue_t);
static int32_t nsvaluefn_stringLength(const eccvalue_t*);
static const char* nsvaluefn_stringBytes(const eccvalue_t*);
static ecctextstring_t nsvaluefn_textOf(const eccvalue_t* string);
static eccvalue_t nsvaluefn_toObject(eccstate_t*, eccvalue_t);
static eccvalue_t nsvaluefn_objectValue(eccobject_t*);
static int nsvaluefn_objectIsArray(eccobject_t*);
static eccvalue_t nsvaluefn_toType(eccvalue_t);
static eccvalue_t nsvaluefn_equals(eccstate_t*, eccvalue_t, eccvalue_t);
static eccvalue_t nsvaluefn_same(eccstate_t*, eccvalue_t, eccvalue_t);
static eccvalue_t nsvaluefn_add(eccstate_t*, eccvalue_t, eccvalue_t);
static eccvalue_t nsvaluefn_subtract(eccstate_t*, eccvalue_t, eccvalue_t);
static eccvalue_t nsvaluefn_less(eccstate_t*, eccvalue_t, eccvalue_t);
static eccvalue_t nsvaluefn_more(eccstate_t*, eccvalue_t, eccvalue_t);
static eccvalue_t nsvaluefn_lessOrEqual(eccstate_t*, eccvalue_t, eccvalue_t);
static eccvalue_t nsvaluefn_moreOrEqual(eccstate_t*, eccvalue_t, eccvalue_t);
static const char* nsvaluefn_typeName(eccvaltype_t);
static const char* nsvaluefn_maskName(eccvalmask_t);
static void nsvaluefn_dumpTo(eccvalue_t, FILE*);
const struct eccpseudonsvalue_t ECCNSValue =
{
    nsvaluefn_truth,
    nsvaluefn_integer,
    nsvaluefn_binary,
    nsvaluefn_buffer,
    nsvaluefn_key,
    nsvaluefn_text,
    nsvaluefn_chars,
    nsvaluefn_object,
    nsvaluefn_error,
    nsvaluefn_string,
    nsvaluefn_regexp,
    nsvaluefn_number,
    nsvaluefn_boolean,
    nsvaluefn_date,
    nsvaluefn_function,
    nsvaluefn_host,
    nsvaluefn_reference,
    nsvaluefn_isPrimitive,
    nsvaluefn_isBoolean,
    nsvaluefn_isNumber,
    nsvaluefn_isString,
    nsvaluefn_isObject,
    nsvaluefn_isDynamic,
    nsvaluefn_isTrue,
    nsvaluefn_toPrimitive,
    nsvaluefn_toBinary,
    nsvaluefn_toInteger,
    nsvaluefn_binaryToString,
    nsvaluefn_toString,
    nsvaluefn_stringLength,
    nsvaluefn_stringBytes,
    nsvaluefn_textOf,
    nsvaluefn_toObject,
    nsvaluefn_objectValue,
    nsvaluefn_objectIsArray,
    nsvaluefn_toType,
    nsvaluefn_equals,
    nsvaluefn_same,
    nsvaluefn_add,
    nsvaluefn_subtract,
    nsvaluefn_less,
    nsvaluefn_more,
    nsvaluefn_lessOrEqual,
    nsvaluefn_moreOrEqual,
    nsvaluefn_typeName,
    nsvaluefn_maskName,
    nsvaluefn_dumpTo,
    ECCValConstNone
};

/*
* 'floor*' lifted from musl-libc, because the builtins break during
* aggressive optimization (-march=native -ffast-math -Ofast...)
* -----
* fp_force_eval ensures that the input value is computed when that's
* otherwise unused.  To prevent the constant folding of the input
* expression, an additional fp_barrier may be needed or a compilation
* mode that does so (e.g. -frounding-math in gcc). Then it can be
* used to evaluate an expression for its fenv side-effects only.
*/

#ifndef fp_force_evalf
#define fp_force_evalf fp_force_evalf
void fp_force_evalf(float x)
{
    volatile float y;
    y = x;
}
#endif

#ifndef fp_force_eval
#define fp_force_eval fp_force_eval
void fp_force_eval(double x)
{
    volatile double y;
    y = x;
}
#endif

#ifndef fp_force_evall
#define fp_force_evall fp_force_evall
void fp_force_evall(long double x)
{
    volatile long double y;
    y = x;
}
#endif

#define FORCE_EVAL(x) \
    do \
    { \
        if(sizeof(x) == sizeof(float)) \
        { \
            fp_force_evalf(x);\
        } \
        else if (sizeof(x) == sizeof(double)) \
        { \
            fp_force_eval(x);\
        } \
        else \
        { \
            fp_force_evall(x);\
        } \
    } while (0)


#if FLT_EVAL_METHOD==0 || FLT_EVAL_METHOD==1
    #define EPS DBL_EPSILON
#elif FLT_EVAL_METHOD==2
    #define EPS LDBL_EPSILON
#endif
static const double_t toint = 1/EPS;

double eccutil_mathfloor(double x)
{
    int e;
    double_t y;
    union {
        double f;
        uint64_t i;
    } u = {x};
    e = u.i >> 52 & 0x7ff;
    if (e >= 0x3ff+52 || x == 0)
    {
        return x;
    }
    /* y = int(x) - x, where int(x) is an integer neighbor of x */
    if (u.i >> 63)
    {
        y = x - toint + toint - x;
    }
    else
    {
        y = x + toint - toint - x;
    }
    /* special case because of non-nearest rounding modes */
    if (e <= 0x3ff-1)
    {
        FORCE_EVAL(y);
        return u.i >> 63 ? -1 : 0;
    }
    if (y > 0)
    {
        return x + y - 1;
    }
    return x + y;
}


eccvalue_t nsvaluefn_truth(int truth)
{
    return (eccvalue_t){
        .type = truth ? ECC_VALTYPE_TRUE : ECC_VALTYPE_FALSE,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_integer(int32_t integer)
{
    return (eccvalue_t){
        .data = { .integer = integer },
        .type = ECC_VALTYPE_INTEGER,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_binary(double binary)
{
    return (eccvalue_t){
        .data = { .binary = binary },
        .type = ECC_VALTYPE_BINARY,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_buffer(const char b[7], uint8_t units)
{
    eccvalue_t value = {
        .type = ECC_VALTYPE_BUFFER,
        .check = 1,
    };
    memcpy(value.data.buffer, b, units);
    value.data.buffer[7] = units;
    return value;
}

eccvalue_t nsvaluefn_key(eccindexkey_t key)
{
    return (eccvalue_t){
        .data = { .key = key },
        .type = ECC_VALTYPE_KEY,
        .check = 0,
    };
}

eccvalue_t nsvaluefn_text(const ecctextstring_t* text)
{
    return (eccvalue_t){
        .data = { .text = text },
        .type = ECC_VALTYPE_TEXT,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_chars(ecccharbuffer_t* chars)
{
    assert(chars);

    return (eccvalue_t){
        .data = { .chars = chars },
        .type = ECC_VALTYPE_CHARS,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_object(eccobject_t* object)
{
    assert(object);

    return (eccvalue_t){
        .data = { .object = object },
        .type = ECC_VALTYPE_OBJECT,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_error(eccobjerror_t* error)
{
    assert(error);

    return (eccvalue_t){
        .data = { .error = error },
        .type = ECC_VALTYPE_ERROR,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_string(eccobjstring_t* string)
{
    assert(string);

    return (eccvalue_t){
        .data = { .string = string },
        .type = ECC_VALTYPE_STRING,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_regexp(eccobjregexp_t* regexp)
{
    assert(regexp);

    return (eccvalue_t){
        .data = { .regexp = regexp },
        .type = ECC_VALTYPE_REGEXP,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_number(eccobjnumber_t* number)
{
    assert(number);

    return (eccvalue_t){
        .data = { .number = number },
        .type = ECC_VALTYPE_NUMBER,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_boolean(eccobjbool_t* boolean)
{
    assert(boolean);

    return (eccvalue_t){
        .data = { .boolean = boolean },
        .type = ECC_VALTYPE_BOOLEAN,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_date(eccobjdate_t* date)
{
    assert(date);

    return (eccvalue_t){
        .data = { .date = date },
        .type = ECC_VALTYPE_DATE,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_function(eccobjfunction_t* function)
{
    assert(function);

    return (eccvalue_t){
        .data = { .function = function },
        .type = ECC_VALTYPE_FUNCTION,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_host(eccobject_t* object)
{
    assert(object);

    return (eccvalue_t){
        .data = { .object = object },
        .type = ECC_VALTYPE_HOST,
        .check = 1,
    };
}

eccvalue_t nsvaluefn_reference(eccvalue_t* reference)
{
    assert(reference);

    return (eccvalue_t){
        .data = { .reference = reference },
        .type = ECC_VALTYPE_REFERENCE,
        .check = 0,
    };
}

// check

int nsvaluefn_isPrimitive(eccvalue_t value)
{
    return !(value.type & ECC_VALMASK_OBJECT);
}

int nsvaluefn_isBoolean(eccvalue_t value)
{
    return value.type & ECC_VALMASK_BOOLEAN;
}

int nsvaluefn_isNumber(eccvalue_t value)
{
    return value.type & ECC_VALMASK_NUMBER;
}

int nsvaluefn_isString(eccvalue_t value)
{
    return value.type & ECC_VALMASK_STRING;
}

int nsvaluefn_isObject(eccvalue_t value)
{
    return value.type & ECC_VALMASK_OBJECT;
}

int nsvaluefn_isDynamic(eccvalue_t value)
{
    return value.type & ECC_VALMASK_DYNAMIC;
}

int nsvaluefn_isTrue(eccvalue_t value)
{
    if(value.type <= ECC_VALTYPE_UNDEFINED)
        return 0;
    if(value.type >= ECC_VALTYPE_TRUE)
        return 1;
    else if(value.type == ECC_VALTYPE_INTEGER)
        return value.data.integer != 0;
    else if(value.type == ECC_VALTYPE_BINARY)
        return !isnan(value.data.binary) && value.data.binary != 0;
    else if(nsvaluefn_isString(value))
        return nsvaluefn_stringLength(&value) > 0;

    ECCNSScript.fatal("Invalid value type : %u", value.type);
}

// convert

eccvalue_t nsvaluefn_toPrimitive(eccstate_t* context, eccvalue_t value, eccvalhint_t hint)
{
    eccobject_t* object;
    eccindexkey_t aKey;
    eccindexkey_t bKey;
    eccvalue_t aFunction;
    eccvalue_t bFunction;
    eccvalue_t result;
    ecctextstring_t text;

    if(value.type < ECC_VALTYPE_OBJECT)
        return value;

    if(!context)
        ECCNSScript.fatal("cannot use toPrimitive outside context");

    object = value.data.object;
    hint = hint ? hint : value.type == ECC_VALTYPE_DATE ? ECC_VALHINT_STRING : ECC_VALHINT_NUMBER;
    aKey = hint > 0 ? ECC_ConstKey_toString : ECC_ConstKey_valueOf;
    bKey = hint > 0 ? ECC_ConstKey_valueOf : ECC_ConstKey_toString;

    aFunction = ECCNSObject.getMember(context, object, aKey);
    if(aFunction.type == ECC_VALTYPE_FUNCTION)
    {
        result = ECCNSContext.callFunction(context, aFunction.data.function, value, 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        if(nsvaluefn_isPrimitive(result))
            return result;
    }

    bFunction = ECCNSObject.getMember(context, object, bKey);
    if(bFunction.type == ECC_VALTYPE_FUNCTION)
    {
        result = ECCNSContext.callFunction(context, bFunction.data.function, value, 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        if(nsvaluefn_isPrimitive(result))
            return result;
    }

    text = ECCNSContext.textSeek(context);
    if(context->textIndex != ECC_CTXINDEXTYPE_CALL && text.length)
        ECCNSContext.typeError(context, ECCNSChars.create("cannot convert '%.*s' to primitive", text.length, text.bytes));
    else
        ECCNSContext.typeError(context, ECCNSChars.create("cannot convert value to primitive"));
}

eccvalue_t nsvaluefn_toBinary(eccstate_t* context, eccvalue_t value)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_BINARY:
            return value;

        case ECC_VALTYPE_INTEGER:
            return nsvaluefn_binary(value.data.integer);

        case ECC_VALTYPE_NUMBER:
            return nsvaluefn_binary(value.data.number->value);

        case ECC_VALTYPE_NULL:
        case ECC_VALTYPE_FALSE:
            return nsvaluefn_binary(0);

        case ECC_VALTYPE_TRUE:
            return nsvaluefn_binary(1);

        case ECC_VALTYPE_BOOLEAN:
            return nsvaluefn_binary(value.data.boolean->truth ? 1 : 0);

        case ECC_VALTYPE_UNDEFINED:
            return nsvaluefn_binary(NAN);

        case ECC_VALTYPE_TEXT:
        {
            if(value.data.text == &ECC_ConstString_Zero)
                return nsvaluefn_binary(0);
            else if(value.data.text == &ECC_ConstString_One)
                return nsvaluefn_binary(1);
            else if(value.data.text == &ECC_ConstString_Nan)
                return nsvaluefn_binary(NAN);
            else if(value.data.text == &ECC_ConstString_Infinity)
                return nsvaluefn_binary(INFINITY);
            else if(value.data.text == &ECC_ConstString_NegativeInfinity)
                return nsvaluefn_binary(-INFINITY);
        }
            /* fallthrough */
        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_BUFFER:
            return ECCNSLexer.scanBinary(nsvaluefn_textOf(&value), context && context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0);

        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_FUNCTION:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
            return nsvaluefn_toBinary(context, nsvaluefn_toPrimitive(context, value, ECC_VALHINT_NUMBER));

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ECCNSScript.fatal("Invalid value type : %u", value.type);
}

eccvalue_t nsvaluefn_toInteger(eccstate_t* context, eccvalue_t value)
{
    const double modulus = (double)UINT32_MAX + 1;
    double binary = nsvaluefn_toBinary(context, value).data.binary;

    if(!binary || !isfinite(binary))
        return nsvaluefn_integer(0);

    binary = fmod(binary, modulus);
    if(binary >= 0)
    {
        binary = eccutil_mathfloor(binary);
    }
    else
    {
        binary = ceil(binary) + modulus;
    }
    if(binary > INT32_MAX)
    {
        return nsvaluefn_integer(binary - modulus);
    }
    return nsvaluefn_integer(binary);
}

eccvalue_t nsvaluefn_binaryToString(double binary, int base)
{
    eccappendbuffer_t chars;

    if(binary == 0)
        return nsvaluefn_text(&ECC_ConstString_Zero);
    else if(binary == 1)
        return nsvaluefn_text(&ECC_ConstString_One);
    else if(isnan(binary))
        return nsvaluefn_text(&ECC_ConstString_Nan);
    else if(isinf(binary))
    {
        if(binary < 0)
            return nsvaluefn_text(&ECC_ConstString_NegativeInfinity);
        else
            return nsvaluefn_text(&ECC_ConstString_Infinity);
    }

    ECCNSChars.beginAppend(&chars);
    ECCNSChars.appendBinary(&chars, binary, base);
    return ECCNSChars.endAppend(&chars);
}

eccvalue_t nsvaluefn_toString(eccstate_t* context, eccvalue_t value)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
        {
            return value;
        }
        break;
        case ECC_VALTYPE_KEY:
        {
            return nsvaluefn_text(ECCNSKey.textOf(value.data.key));
        }
        break;
        case ECC_VALTYPE_STRING:
        {
            return nsvaluefn_chars(value.data.string->value);
        }
        break;
        case ECC_VALTYPE_NULL:
            return nsvaluefn_text(&ECC_ConstString_Null);

        case ECC_VALTYPE_UNDEFINED:
            return nsvaluefn_text(&ECC_ConstString_Undefined);

        case ECC_VALTYPE_FALSE:
            return nsvaluefn_text(&ECC_ConstString_False);

        case ECC_VALTYPE_TRUE:
            return nsvaluefn_text(&ECC_ConstString_True);

        case ECC_VALTYPE_BOOLEAN:
            return value.data.boolean->truth ? nsvaluefn_text(&ECC_ConstString_True) : nsvaluefn_text(&ECC_ConstString_False);

        case ECC_VALTYPE_INTEGER:
            return nsvaluefn_binaryToString(value.data.integer, 10);

        case ECC_VALTYPE_NUMBER:
        {
            value.data.binary = value.data.number->value;
        }
            /* fallthrough */
        case ECC_VALTYPE_BINARY:
            return nsvaluefn_binaryToString(value.data.binary, 10);

        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_FUNCTION:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
            return nsvaluefn_toString(context, nsvaluefn_toPrimitive(context, value, ECC_VALHINT_STRING));

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ECCNSScript.fatal("Invalid value type : %u", value.type);
}

int32_t nsvaluefn_stringLength(const eccvalue_t* value)
{
    switch(value->type)
    {
        case ECC_VALTYPE_CHARS:
            return value->data.chars->length;

        case ECC_VALTYPE_TEXT:
            return value->data.text->length;

        case ECC_VALTYPE_STRING:
            return value->data.string->value->length;

        case ECC_VALTYPE_BUFFER:
            return value->data.buffer[7];

        default:
            return 0;
    }
}

const char* nsvaluefn_stringBytes(const eccvalue_t* value)
{
    switch(value->type)
    {
        case ECC_VALTYPE_CHARS:
            return value->data.chars->bytes;

        case ECC_VALTYPE_TEXT:
            return value->data.text->bytes;

        case ECC_VALTYPE_STRING:
            return value->data.string->value->bytes;

        case ECC_VALTYPE_BUFFER:
            return value->data.buffer;

        default:
            return NULL;
    }
}

ecctextstring_t nsvaluefn_textOf(const eccvalue_t* value)
{
    switch(value->type)
    {
        case ECC_VALTYPE_CHARS:
            return ECCNSText.make(value->data.chars->bytes, value->data.chars->length);

        case ECC_VALTYPE_TEXT:
            return *value->data.text;

        case ECC_VALTYPE_STRING:
            return ECCNSText.make(value->data.string->value->bytes, value->data.string->value->length);

        case ECC_VALTYPE_KEY:
            return *ECCNSKey.textOf(value->data.key);

        case ECC_VALTYPE_BUFFER:
            return ECCNSText.make(value->data.buffer, value->data.buffer[7]);

        default:
            return ECC_ConstString_Empty;
    }
}

eccvalue_t nsvaluefn_toObject(eccstate_t* context, eccvalue_t value)
{
    if(value.type >= ECC_VALTYPE_OBJECT)
        return value;

    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_BINARY:
            return nsvaluefn_number(ECCNSNumber.create(value.data.binary));

        case ECC_VALTYPE_INTEGER:
            return nsvaluefn_number(ECCNSNumber.create(value.data.integer));

        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
            return nsvaluefn_string(ECCNSString.create(ECCNSChars.createWithBytes(nsvaluefn_stringLength(&value), nsvaluefn_stringBytes(&value))));

        case ECC_VALTYPE_FALSE:
        case ECC_VALTYPE_TRUE:
            return nsvaluefn_boolean(ECCNSBool.create(value.type == ECC_VALTYPE_TRUE));

        case ECC_VALTYPE_NULL:
            goto error;

        case ECC_VALTYPE_UNDEFINED:
            goto error;

        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_REFERENCE:
        case ECC_VALTYPE_FUNCTION:
        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_NUMBER:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_BOOLEAN:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
            break;
    }
    ECCNSScript.fatal("Invalid value type : %u", value.type);

error:
{
    ecctextstring_t text = ECCNSContext.textSeek(context);

    if(context->textIndex != ECC_CTXINDEXTYPE_CALL && text.length)
        ECCNSContext.typeError(context, ECCNSChars.create("cannot convert '%.*s' to object", text.length, text.bytes));
    else
        ECCNSContext.typeError(context, ECCNSChars.create("cannot convert %s to object", nsvaluefn_typeName(value.type)));
}
}

eccvalue_t nsvaluefn_objectValue(eccobject_t* object)
{
    if(!object)
        return ECCValConstUndefined;
    else if(object->type == &ECC_Type_Function)
        return ECCNSValue.function((eccobjfunction_t*)object);
    else if(object->type == &ECC_Type_String)
        return ECCNSValue.string((eccobjstring_t*)object);
    else if(object->type == &ECC_Type_Boolean)
        return ECCNSValue.boolean((eccobjbool_t*)object);
    else if(object->type == &ECC_Type_Number)
        return ECCNSValue.number((eccobjnumber_t*)object);
    else if(object->type == &ECC_Type_Date)
        return ECCNSValue.date((eccobjdate_t*)object);
    else if(object->type == &ECC_Type_Regexp)
        return ECCNSValue.regexp((eccobjregexp_t*)object);
    else if(object->type == &ECC_Type_Error)
        return ECCNSValue.error((eccobjerror_t*)object);
    else if(object->type == &ECC_Type_Object || object->type == &ECC_Type_Array || object->type == &ECC_Type_Arguments || object->type == &ECC_Type_Math)
        return ECCNSValue.object((eccobject_t*)object);
    else
        return ECCNSValue.host(object);
}

int nsvaluefn_objectIsArray(eccobject_t* object)
{
    return object->type == &ECC_Type_Array || object->type == &ECC_Type_Arguments;
}

eccvalue_t nsvaluefn_toType(eccvalue_t value)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_TRUE:
        case ECC_VALTYPE_FALSE:
            return nsvaluefn_text(&ECC_ConstString_Boolean);

        case ECC_VALTYPE_UNDEFINED:
            return nsvaluefn_text(&ECC_ConstString_Undefined);

        case ECC_VALTYPE_INTEGER:
        case ECC_VALTYPE_BINARY:
            return nsvaluefn_text(&ECC_ConstString_Number);

        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
            return nsvaluefn_text(&ECC_ConstString_String);

        case ECC_VALTYPE_NULL:
        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_NUMBER:
        case ECC_VALTYPE_BOOLEAN:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
            return nsvaluefn_text(&ECC_ConstString_Object);

        case ECC_VALTYPE_FUNCTION:
            return nsvaluefn_text(&ECC_ConstString_Function);

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ECCNSScript.fatal("Invalid value type : %u", value.type);
}

eccvalue_t nsvaluefn_equals(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    if(nsvaluefn_isObject(a) && nsvaluefn_isObject(b))
        return nsvaluefn_truth(a.data.object == b.data.object);
    else if(((nsvaluefn_isString(a) || nsvaluefn_isNumber(a)) && nsvaluefn_isObject(b)) || (nsvaluefn_isObject(a) && (nsvaluefn_isString(b) || nsvaluefn_isNumber(b))))
    {
        a = nsvaluefn_toPrimitive(context, a, ECC_VALHINT_AUTO);
        ECCNSContext.setTextIndex(context, ECC_CTXINDECTYPE_SAVEDINDEXALT);
        b = nsvaluefn_toPrimitive(context, b, ECC_VALHINT_AUTO);

        return nsvaluefn_equals(context, a, b);
    }
    else if(nsvaluefn_isNumber(a) && nsvaluefn_isNumber(b))
        return nsvaluefn_truth(nsvaluefn_toBinary(context, a).data.binary == nsvaluefn_toBinary(context, b).data.binary);
    else if(nsvaluefn_isString(a) && nsvaluefn_isString(b))
    {
        int32_t aLength = nsvaluefn_stringLength(&a);
        int32_t bLength = nsvaluefn_stringLength(&b);
        if(aLength != bLength)
            return ECCValConstFalse;

        return nsvaluefn_truth(!memcmp(nsvaluefn_stringBytes(&a), nsvaluefn_stringBytes(&b), aLength));
    }
    else if(a.type == b.type)
        return ECCValConstTrue;
    else if(a.type == ECC_VALTYPE_NULL && b.type == ECC_VALTYPE_UNDEFINED)
        return ECCValConstTrue;
    else if(a.type == ECC_VALTYPE_UNDEFINED && b.type == ECC_VALTYPE_NULL)
        return ECCValConstTrue;
    else if(nsvaluefn_isNumber(a) && nsvaluefn_isString(b))
        return nsvaluefn_equals(context, a, nsvaluefn_toBinary(context, b));
    else if(nsvaluefn_isString(a) && nsvaluefn_isNumber(b))
        return nsvaluefn_equals(context, nsvaluefn_toBinary(context, a), b);
    else if(nsvaluefn_isBoolean(a))
        return nsvaluefn_equals(context, nsvaluefn_toBinary(context, a), b);
    else if(nsvaluefn_isBoolean(b))
        return nsvaluefn_equals(context, a, nsvaluefn_toBinary(context, b));

    return ECCValConstFalse;
}

eccvalue_t nsvaluefn_same(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    if(nsvaluefn_isObject(a) || nsvaluefn_isObject(b))
        return nsvaluefn_truth(nsvaluefn_isObject(a) && nsvaluefn_isObject(b) && a.data.object == b.data.object);
    else if(nsvaluefn_isNumber(a) && nsvaluefn_isNumber(b))
        return nsvaluefn_truth(nsvaluefn_toBinary(context, a).data.binary == nsvaluefn_toBinary(context, b).data.binary);
    else if(nsvaluefn_isString(a) && nsvaluefn_isString(b))
    {
        int32_t aLength = nsvaluefn_stringLength(&a);
        int32_t bLength = nsvaluefn_stringLength(&b);
        if(aLength != bLength)
            return ECCValConstFalse;

        return nsvaluefn_truth(!memcmp(nsvaluefn_stringBytes(&a), nsvaluefn_stringBytes(&b), aLength));
    }
    else if(a.type == b.type)
        return ECCValConstTrue;

    return ECCValConstFalse;
}

eccvalue_t nsvaluefn_add(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    if(!nsvaluefn_isNumber(a) || !nsvaluefn_isNumber(b))
    {
        a = nsvaluefn_toPrimitive(context, a, ECC_VALHINT_AUTO);
        ECCNSContext.setTextIndex(context, ECC_CTXINDECTYPE_SAVEDINDEXALT);
        b = nsvaluefn_toPrimitive(context, b, ECC_VALHINT_AUTO);

        if(nsvaluefn_isString(a) || nsvaluefn_isString(b))
        {
            eccappendbuffer_t chars;

            ECCNSChars.beginAppend(&chars);
            ECCNSChars.appendValue(&chars, context, a);
            ECCNSChars.appendValue(&chars, context, b);
            return ECCNSChars.endAppend(&chars);
        }
    }
    return nsvaluefn_binary(nsvaluefn_toBinary(context, a).data.binary + nsvaluefn_toBinary(context, b).data.binary);
}

eccvalue_t nsvaluefn_subtract(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    return nsvaluefn_binary(nsvaluefn_toBinary(context, a).data.binary - nsvaluefn_toBinary(context, b).data.binary);
}

static eccvalue_t eccvalue_compare(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = nsvaluefn_toPrimitive(context, a, ECC_VALHINT_NUMBER);
    ECCNSContext.setTextIndex(context, ECC_CTXINDECTYPE_SAVEDINDEXALT);
    b = nsvaluefn_toPrimitive(context, b, ECC_VALHINT_NUMBER);

    if(nsvaluefn_isString(a) && nsvaluefn_isString(b))
    {
        int32_t aLength = nsvaluefn_stringLength(&a);
        int32_t bLength = nsvaluefn_stringLength(&b);

        if(aLength < bLength && !memcmp(nsvaluefn_stringBytes(&a), nsvaluefn_stringBytes(&b), aLength))
            return ECCValConstTrue;

        if(aLength > bLength && !memcmp(nsvaluefn_stringBytes(&a), nsvaluefn_stringBytes(&b), bLength))
            return ECCValConstFalse;

        return nsvaluefn_truth(memcmp(nsvaluefn_stringBytes(&a), nsvaluefn_stringBytes(&b), aLength) < 0);
    }
    a = nsvaluefn_toBinary(context, a);
    b = nsvaluefn_toBinary(context, b);
    if(isnan(a.data.binary) || isnan(b.data.binary))
        return ECCValConstUndefined;
    return nsvaluefn_truth(a.data.binary < b.data.binary);
}

eccvalue_t nsvaluefn_less(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, a, b);
    if(a.type == ECC_VALTYPE_UNDEFINED)
        return ECCValConstFalse;
    return a;
}

eccvalue_t nsvaluefn_more(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, b, a);
    if(a.type == ECC_VALTYPE_UNDEFINED)
        return ECCValConstFalse;
    return a;
}

eccvalue_t nsvaluefn_lessOrEqual(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, b, a);
    if(a.type == ECC_VALTYPE_UNDEFINED || a.type == ECC_VALTYPE_TRUE)
        return ECCValConstFalse;
    return ECCValConstTrue;
}

eccvalue_t nsvaluefn_moreOrEqual(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, a, b);
    if(a.type == ECC_VALTYPE_UNDEFINED || a.type == ECC_VALTYPE_TRUE)
        return ECCValConstFalse;
    return ECCValConstTrue;
}

const char* nsvaluefn_typeName(eccvaltype_t type)
{
    switch(type)
    {
        case ECC_VALTYPE_NULL:
            return "null";

        case ECC_VALTYPE_UNDEFINED:
            return "undefined";

        case ECC_VALTYPE_FALSE:
        case ECC_VALTYPE_TRUE:
            return "boolean";

        case ECC_VALTYPE_INTEGER:
        case ECC_VALTYPE_BINARY:
            return "number";

        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
            return "string";
        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_HOST:
            return "object";
        case ECC_VALTYPE_ERROR:
            return "error";
        case ECC_VALTYPE_FUNCTION:
            return "function";
        case ECC_VALTYPE_DATE:
            return "date";
        case ECC_VALTYPE_NUMBER:
            return "number";
        case ECC_VALTYPE_STRING:
            return "string";
        case ECC_VALTYPE_BOOLEAN:
            return "boolean";
        case ECC_VALTYPE_REGEXP:
            return "regexp";
        case ECC_VALTYPE_REFERENCE:
            break;
        default:
            break;
    }
    ECCNSScript.fatal("Invalid value type : %u", type);
}

const char* nsvaluefn_maskName(eccvalmask_t mask)
{
    switch(mask)
    {
        case ECC_VALMASK_NUMBER:
            return "number";
        case ECC_VALMASK_STRING:
            return "string";
        case ECC_VALMASK_BOOLEAN:
            return "boolean";
        case ECC_VALMASK_OBJECT:
            return "object";
        case ECC_VALMASK_DYNAMIC:
            return "dynamic";
        default:
            break;
    }
    ECCNSScript.fatal("Invalid value mask : %u", mask);
}

void nsvaluefn_dumpTo(eccvalue_t value, FILE* file)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_NULL:
        {
            fputs("null", file);
            return;
        }
        break;
        case ECC_VALTYPE_UNDEFINED:
        {
            fputs("undefined", file);
            return;
        }
        break;
        case ECC_VALTYPE_FALSE:
        {
            fputs("false", file);
            return;
        }
        break;
        case ECC_VALTYPE_TRUE:
        {
            fputs("true", file);
            return;
        }
        break;
        case ECC_VALTYPE_BOOLEAN:
        {
            fputs(value.data.boolean->truth ? "true" : "false", file);
            return;
        }
        break;
        case ECC_VALTYPE_INTEGER:
        {
            fprintf(file, "%d", (int)value.data.integer);
            return;
        }
        break;
        case ECC_VALTYPE_NUMBER:
        {
            value.data.binary = value.data.number->value;
        }
            /* fallthrough */
        case ECC_VALTYPE_BINARY:
        {
            fprintf(file, "%g", value.data.binary);
            return;
        }
        break;
        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_BUFFER:
        {
            const ecctextstring_t text = nsvaluefn_textOf(&value);
            //putc('\'', file);
            fwrite(text.bytes, sizeof(char), text.length, file);
            //putc('\'', file);
            return;
        }
        break;

        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
        {
            ECCNSObject.dumpTo(value.data.object, file);
            return;
        }
        break;
        case ECC_VALTYPE_FUNCTION:
        {
            fwrite(value.data.function->text.bytes, sizeof(char), value.data.function->text.length, file);
            return;
        }
        break;
        case ECC_VALTYPE_REFERENCE:
        {
            fputs("-> ", file);
            nsvaluefn_dumpTo(*value.data.reference, file);
            return;
        }
        break;
    }
}
