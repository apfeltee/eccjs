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

const eccvalue_t ECCValConstNone = { { 0 }, {}, (eccvaltype_t)0, 0, 0 };
const eccvalue_t ECCValConstUndefined = valueMake(ECC_VALTYPE_UNDEFINED);
const eccvalue_t ECCValConstTrue = valueMake(ECC_VALTYPE_TRUE);
const eccvalue_t ECCValConstFalse = valueMake(ECC_VALTYPE_FALSE);
const eccvalue_t ECCValConstNull = valueMake(ECC_VALTYPE_NULL);

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

eccvalue_t ecc_value_makevalue(eccvaltype_t t)
{
    eccvalue_t v;
    memset(&v, 0, sizeof(eccvalue_t));
    v.type = t;
    return v;
}

eccvalue_t ecc_value_truth(int truth)
{
    eccvalue_t v;
    v = ecc_value_makevalue(truth ? ECC_VALTYPE_TRUE : ECC_VALTYPE_FALSE);
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_integer(int32_t integer)
{
    eccvalue_t v;
    v = ecc_value_makevalue(ECC_VALTYPE_INTEGER);
    v.data.integer = integer;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_binary(double binary)
{
    eccvalue_t v;
    v = ecc_value_makevalue(ECC_VALTYPE_BINARY);
    v.data.binary = binary;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_buffer(const char b[7], uint8_t units)
{
    eccvalue_t value;
    value = ecc_value_makevalue(ECC_VALTYPE_BUFFER);
    value.check = 1;
    memcpy(value.data.buffer, b, units);
    value.data.buffer[7] = units;
    return value;
}

eccvalue_t ecc_value_key(eccindexkey_t key)
{
    eccvalue_t v;
    v = ecc_value_makevalue(ECC_VALTYPE_KEY);
    v.data.key = key;
    v.check = 0;
    return v;
}

eccvalue_t ecc_value_text(const ecctextstring_t* text)
{
    eccvalue_t v;
    v = ecc_value_makevalue(ECC_VALTYPE_TEXT);
    v.data.text = text;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_chars(ecccharbuffer_t* chars)
{
    eccvalue_t v;
    assert(chars);
    v = ecc_value_makevalue(ECC_VALTYPE_CHARS);
    v.data.chars = chars;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_object(eccobject_t* object)
{
    assert(object);
    eccvalue_t v;
    v = ecc_value_makevalue(ECC_VALTYPE_OBJECT);
    v.data.object = object;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_error(eccobjerror_t* error)
{
    eccvalue_t v;
    assert(error);
    v = ecc_value_makevalue(ECC_VALTYPE_ERROR);
    v.data.error = error;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_string(eccobjstring_t* string)
{
    eccvalue_t v;
    assert(string);
    v = ecc_value_makevalue(ECC_VALTYPE_STRING);
    v.data.string = string;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_regexp(eccobjregexp_t* regexp)
{
    eccvalue_t v;
    assert(regexp);
    v = ecc_value_makevalue(ECC_VALTYPE_REGEXP);
    v.data.regexp = regexp;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_number(eccobjnumber_t* number)
{
    eccvalue_t v;
    assert(number);
    v = ecc_value_makevalue(ECC_VALTYPE_NUMBER);
    v.data.number = number;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_boolean(eccobjbool_t* boolean)
{
    eccvalue_t v;
    assert(boolean);
    v= ecc_value_makevalue(ECC_VALTYPE_BOOLEAN);
    v.data.boolean = boolean;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_date(eccobjdate_t* date)
{
    eccvalue_t v;
    assert(date);
    v = ecc_value_makevalue(ECC_VALTYPE_DATE);
    v.data.date = date;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_function(eccobjfunction_t* function)
{
    eccvalue_t v;
    assert(function);
    v = ecc_value_makevalue(ECC_VALTYPE_FUNCTION);
    v.data.function = function;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_host(eccobject_t* object)
{
    assert(object);
    eccvalue_t v;
    v = ecc_value_makevalue(ECC_VALTYPE_HOST);
    v.data.object = object;
    v.check = 1;
    return v;
}

eccvalue_t ecc_value_reference(eccvalue_t* reference)
{
    assert(reference);
    eccvalue_t v;
    v = ecc_value_makevalue(ECC_VALTYPE_REFERENCE);
    v.data.reference = reference;
    v.check = 0;
    return v;
}

// check

int ecc_value_isprimitive(eccvalue_t value)
{
    return !(value.type & ECC_VALMASK_OBJECT);
}

int ecc_value_isboolean(eccvalue_t value)
{
    return value.type & ECC_VALMASK_BOOLEAN;
}

int ecc_value_isnumber(eccvalue_t value)
{
    return value.type & ECC_VALMASK_NUMBER;
}

int ecc_value_isstring(eccvalue_t value)
{
    return value.type & ECC_VALMASK_STRING;
}

int ecc_value_isobject(eccvalue_t value)
{
    return value.type & ECC_VALMASK_OBJECT;
}

int ecc_value_isdynamic(eccvalue_t value)
{
    return value.type & ECC_VALMASK_DYNAMIC;
}

int ecc_value_istrue(eccvalue_t value)
{
    if(value.type <= ECC_VALTYPE_UNDEFINED)
    {
        return 0;
    }
    if(value.type >= ECC_VALTYPE_TRUE)
    {
        return 1;
    }
    else if(value.type == ECC_VALTYPE_INTEGER)
    {
        return value.data.integer != 0;
    }
    else if(value.type == ECC_VALTYPE_BINARY)
    {
        return !isnan(value.data.binary) && value.data.binary != 0;
    }
    else if(ecc_value_isstring(value))
    {
        return ecc_value_stringlength(&value) > 0;
    }
    ecc_script_fatal("Invalid value type : %u", value.type);
    return 0;
}

// convert

eccvalue_t ecc_value_toprimitive(eccstate_t* context, eccvalue_t value, int hint)
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
        ecc_script_fatal("cannot use toPrimitive outside context");

    object = value.data.object;
    hint = hint ? hint : value.type == ECC_VALTYPE_DATE ? ECC_VALHINT_STRING : ECC_VALHINT_NUMBER;
    aKey = hint > 0 ? ECC_ConstKey_toString : ECC_ConstKey_valueOf;
    bKey = hint > 0 ? ECC_ConstKey_valueOf : ECC_ConstKey_toString;

    aFunction = ECCNSObject.getMember(context, object, aKey);
    if(aFunction.type == ECC_VALTYPE_FUNCTION)
    {
        result = ECCNSContext.callFunction(context, aFunction.data.function, value, 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        if(ecc_value_isprimitive(result))
            return result;
    }

    bFunction = ECCNSObject.getMember(context, object, bKey);
    if(bFunction.type == ECC_VALTYPE_FUNCTION)
    {
        result = ECCNSContext.callFunction(context, bFunction.data.function, value, 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        if(ecc_value_isprimitive(result))
            return result;
    }

    text = ECCNSContext.textSeek(context);
    if(context->textIndex != ECC_CTXINDEXTYPE_CALL && text.length)
        ECCNSContext.typeError(context, ECCNSChars.create("cannot convert '%.*s' to primitive", text.length, text.bytes));
    else
        ECCNSContext.typeError(context, ECCNSChars.create("cannot convert value to primitive"));
    return ECCValConstUndefined;
}

eccvalue_t ecc_value_tobinary(eccstate_t* context, eccvalue_t value)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_BINARY:
            return value;

        case ECC_VALTYPE_INTEGER:
            return ecc_value_binary(value.data.integer);

        case ECC_VALTYPE_NUMBER:
            return ecc_value_binary(value.data.number->value);

        case ECC_VALTYPE_NULL:
        case ECC_VALTYPE_FALSE:
            return ecc_value_binary(0);

        case ECC_VALTYPE_TRUE:
            return ecc_value_binary(1);

        case ECC_VALTYPE_BOOLEAN:
            return ecc_value_binary(value.data.boolean->truth ? 1 : 0);

        case ECC_VALTYPE_UNDEFINED:
            return ecc_value_binary(NAN);

        case ECC_VALTYPE_TEXT:
        {
            if(value.data.text == &ECC_ConstString_Zero)
                return ecc_value_binary(0);
            else if(value.data.text == &ECC_ConstString_One)
                return ecc_value_binary(1);
            else if(value.data.text == &ECC_ConstString_Nan)
                return ecc_value_binary(NAN);
            else if(value.data.text == &ECC_ConstString_Infinity)
                return ecc_value_binary(INFINITY);
            else if(value.data.text == &ECC_ConstString_NegativeInfinity)
                return ecc_value_binary(-INFINITY);
        }
            /* fallthrough */
        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_BUFFER:
            return ECCNSLexer.scanBinary(ecc_value_textof(&value), context && context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0);

        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_FUNCTION:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
            return ecc_value_tobinary(context, ecc_value_toprimitive(context, value, ECC_VALHINT_NUMBER));

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ecc_script_fatal("Invalid value type : %u", value.type);
    return ECCValConstUndefined;
}

eccvalue_t ecc_value_tointeger(eccstate_t* context, eccvalue_t value)
{
    const double modulus = (double)UINT32_MAX + 1;
    double binary = ecc_value_tobinary(context, value).data.binary;

    if(!binary || !isfinite(binary))
        return ecc_value_integer(0);

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
        return ecc_value_integer(binary - modulus);
    }
    return ecc_value_integer(binary);
}

eccvalue_t ecc_value_binarytostring(double binary, int base)
{
    eccappendbuffer_t chars;

    if(binary == 0)
        return ecc_value_text(&ECC_ConstString_Zero);
    else if(binary == 1)
        return ecc_value_text(&ECC_ConstString_One);
    else if(isnan(binary))
        return ecc_value_text(&ECC_ConstString_Nan);
    else if(isinf(binary))
    {
        if(binary < 0)
            return ecc_value_text(&ECC_ConstString_NegativeInfinity);
        else
            return ecc_value_text(&ECC_ConstString_Infinity);
    }

    ECCNSChars.beginAppend(&chars);
    ECCNSChars.appendBinary(&chars, binary, base);
    return ECCNSChars.endAppend(&chars);
}

eccvalue_t ecc_value_tostring(eccstate_t* context, eccvalue_t value)
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
            return ecc_value_text(ECCNSKey.textOf(value.data.key));
        }
        break;
        case ECC_VALTYPE_STRING:
        {
            return ecc_value_chars(value.data.string->value);
        }
        break;
        case ECC_VALTYPE_NULL:
            return ecc_value_text(&ECC_ConstString_Null);

        case ECC_VALTYPE_UNDEFINED:
            return ecc_value_text(&ECC_ConstString_Undefined);

        case ECC_VALTYPE_FALSE:
            return ecc_value_text(&ECC_ConstString_False);

        case ECC_VALTYPE_TRUE:
            return ecc_value_text(&ECC_ConstString_True);

        case ECC_VALTYPE_BOOLEAN:
            return value.data.boolean->truth ? ecc_value_text(&ECC_ConstString_True) : ecc_value_text(&ECC_ConstString_False);

        case ECC_VALTYPE_INTEGER:
            return ecc_value_binarytostring(value.data.integer, 10);

        case ECC_VALTYPE_NUMBER:
        {
            value.data.binary = value.data.number->value;
        }
            /* fallthrough */
        case ECC_VALTYPE_BINARY:
            return ecc_value_binarytostring(value.data.binary, 10);

        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_FUNCTION:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
            return ecc_value_tostring(context, ecc_value_toprimitive(context, value, ECC_VALHINT_STRING));

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ecc_script_fatal("Invalid value type : %u", value.type);
    return ECCValConstUndefined;
}

int32_t ecc_value_stringlength(const eccvalue_t* value)
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
            break;
    }
    return 0;
}

const char* ecc_value_stringbytes(const eccvalue_t* value)
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
            break;
    }
    return NULL;
}

ecctextstring_t ecc_value_textof(const eccvalue_t* value)
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
            break;
    }
    return ECC_ConstString_Empty;
}

eccvalue_t ecc_value_toobject(eccstate_t* context, eccvalue_t value)
{
    if(value.type >= ECC_VALTYPE_OBJECT)
        return value;

    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_BINARY:
            return ecc_value_number(ECCNSNumber.create(value.data.binary));

        case ECC_VALTYPE_INTEGER:
            return ecc_value_number(ECCNSNumber.create(value.data.integer));

        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
            return ecc_value_string(ECCNSString.create(ECCNSChars.createWithBytes(ecc_value_stringlength(&value), ecc_value_stringbytes(&value))));

        case ECC_VALTYPE_FALSE:
        case ECC_VALTYPE_TRUE:
            return ecc_value_boolean(ECCNSBool.create(value.type == ECC_VALTYPE_TRUE));

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
    ecc_script_fatal("Invalid value type : %u", value.type);
    return ECCValConstUndefined;
    error:
    {
        ecctextstring_t text = ECCNSContext.textSeek(context);
        if(context->textIndex != ECC_CTXINDEXTYPE_CALL && text.length)
            ECCNSContext.typeError(context, ECCNSChars.create("cannot convert '%.*s' to object", text.length, text.bytes));
        else
            ECCNSContext.typeError(context, ECCNSChars.create("cannot convert %s to object", ecc_value_typename(value.type)));
    }
    return ECCValConstUndefined;
}

eccvalue_t ecc_value_objectvalue(eccobject_t* object)
{
    if(!object)
        return ECCValConstUndefined;
    else if(object->type == &ECC_Type_Function)
        return ecc_value_function((eccobjfunction_t*)object);
    else if(object->type == &ECC_Type_String)
        return ecc_value_string((eccobjstring_t*)object);
    else if(object->type == &ECC_Type_Boolean)
        return ecc_value_boolean((eccobjbool_t*)object);
    else if(object->type == &ECC_Type_Number)
        return ecc_value_number((eccobjnumber_t*)object);
    else if(object->type == &ECC_Type_Date)
        return ecc_value_date((eccobjdate_t*)object);
    else if(object->type == &ECC_Type_Regexp)
        return ecc_value_regexp((eccobjregexp_t*)object);
    else if(object->type == &ECC_Type_Error)
        return ecc_value_error((eccobjerror_t*)object);
    else if(object->type == &ECC_Type_Object || object->type == &ECC_Type_Array || object->type == &ECC_Type_Arguments || object->type == &ECC_Type_Math)
        return ecc_value_object((eccobject_t*)object);
    return ecc_value_host(object);
}

int ecc_value_objectisarray(eccobject_t* object)
{
    return object->type == &ECC_Type_Array || object->type == &ECC_Type_Arguments;
}

eccvalue_t ecc_value_totype(eccvalue_t value)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_TRUE:
        case ECC_VALTYPE_FALSE:
            return ecc_value_text(&ECC_ConstString_Boolean);

        case ECC_VALTYPE_UNDEFINED:
            return ecc_value_text(&ECC_ConstString_Undefined);

        case ECC_VALTYPE_INTEGER:
        case ECC_VALTYPE_BINARY:
            return ecc_value_text(&ECC_ConstString_Number);

        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
            return ecc_value_text(&ECC_ConstString_String);

        case ECC_VALTYPE_NULL:
        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_NUMBER:
        case ECC_VALTYPE_BOOLEAN:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_HOST:
            return ecc_value_text(&ECC_ConstString_Object);

        case ECC_VALTYPE_FUNCTION:
            return ecc_value_text(&ECC_ConstString_Function);

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ecc_script_fatal("Invalid value type : %u", value.type);
    return ECCValConstUndefined;
}

eccvalue_t ecc_value_equals(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    if(ecc_value_isobject(a) && ecc_value_isobject(b))
        return ecc_value_truth(a.data.object == b.data.object);
    else if(((ecc_value_isstring(a) || ecc_value_isnumber(a)) && ecc_value_isobject(b)) || (ecc_value_isobject(a) && (ecc_value_isstring(b) || ecc_value_isnumber(b))))
    {
        a = ecc_value_toprimitive(context, a, ECC_VALHINT_AUTO);
        ECCNSContext.setTextIndex(context, ECC_CTXINDECTYPE_SAVEDINDEXALT);
        b = ecc_value_toprimitive(context, b, ECC_VALHINT_AUTO);

        return ecc_value_equals(context, a, b);
    }
    else if(ecc_value_isnumber(a) && ecc_value_isnumber(b))
        return ecc_value_truth(ecc_value_tobinary(context, a).data.binary == ecc_value_tobinary(context, b).data.binary);
    else if(ecc_value_isstring(a) && ecc_value_isstring(b))
    {
        int32_t aLength = ecc_value_stringlength(&a);
        int32_t bLength = ecc_value_stringlength(&b);
        if(aLength != bLength)
            return ECCValConstFalse;

        return ecc_value_truth(!memcmp(ecc_value_stringbytes(&a), ecc_value_stringbytes(&b), aLength));
    }
    else if(a.type == b.type)
        return ECCValConstTrue;
    else if(a.type == ECC_VALTYPE_NULL && b.type == ECC_VALTYPE_UNDEFINED)
        return ECCValConstTrue;
    else if(a.type == ECC_VALTYPE_UNDEFINED && b.type == ECC_VALTYPE_NULL)
        return ECCValConstTrue;
    else if(ecc_value_isnumber(a) && ecc_value_isstring(b))
        return ecc_value_equals(context, a, ecc_value_tobinary(context, b));
    else if(ecc_value_isstring(a) && ecc_value_isnumber(b))
        return ecc_value_equals(context, ecc_value_tobinary(context, a), b);
    else if(ecc_value_isboolean(a))
        return ecc_value_equals(context, ecc_value_tobinary(context, a), b);
    else if(ecc_value_isboolean(b))
        return ecc_value_equals(context, a, ecc_value_tobinary(context, b));

    return ECCValConstFalse;
}

eccvalue_t ecc_value_same(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    if(ecc_value_isobject(a) || ecc_value_isobject(b))
        return ecc_value_truth(ecc_value_isobject(a) && ecc_value_isobject(b) && a.data.object == b.data.object);
    else if(ecc_value_isnumber(a) && ecc_value_isnumber(b))
        return ecc_value_truth(ecc_value_tobinary(context, a).data.binary == ecc_value_tobinary(context, b).data.binary);
    else if(ecc_value_isstring(a) && ecc_value_isstring(b))
    {
        int32_t aLength = ecc_value_stringlength(&a);
        int32_t bLength = ecc_value_stringlength(&b);
        if(aLength != bLength)
            return ECCValConstFalse;

        return ecc_value_truth(!memcmp(ecc_value_stringbytes(&a), ecc_value_stringbytes(&b), aLength));
    }
    else if(a.type == b.type)
        return ECCValConstTrue;
    return ECCValConstFalse;
}

eccvalue_t ecc_value_add(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    if(!ecc_value_isnumber(a) || !ecc_value_isnumber(b))
    {
        a = ecc_value_toprimitive(context, a, ECC_VALHINT_AUTO);
        ECCNSContext.setTextIndex(context, ECC_CTXINDECTYPE_SAVEDINDEXALT);
        b = ecc_value_toprimitive(context, b, ECC_VALHINT_AUTO);

        if(ecc_value_isstring(a) || ecc_value_isstring(b))
        {
            eccappendbuffer_t chars;

            ECCNSChars.beginAppend(&chars);
            ECCNSChars.appendValue(&chars, context, a);
            ECCNSChars.appendValue(&chars, context, b);
            return ECCNSChars.endAppend(&chars);
        }
    }
    return ecc_value_binary(ecc_value_tobinary(context, a).data.binary + ecc_value_tobinary(context, b).data.binary);
}

eccvalue_t ecc_value_subtract(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    return ecc_value_binary(ecc_value_tobinary(context, a).data.binary - ecc_value_tobinary(context, b).data.binary);
}

eccvalue_t eccvalue_compare(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = ecc_value_toprimitive(context, a, ECC_VALHINT_NUMBER);
    ECCNSContext.setTextIndex(context, ECC_CTXINDECTYPE_SAVEDINDEXALT);
    b = ecc_value_toprimitive(context, b, ECC_VALHINT_NUMBER);

    if(ecc_value_isstring(a) && ecc_value_isstring(b))
    {
        int32_t aLength = ecc_value_stringlength(&a);
        int32_t bLength = ecc_value_stringlength(&b);

        if(aLength < bLength && !memcmp(ecc_value_stringbytes(&a), ecc_value_stringbytes(&b), aLength))
            return ECCValConstTrue;

        if(aLength > bLength && !memcmp(ecc_value_stringbytes(&a), ecc_value_stringbytes(&b), bLength))
            return ECCValConstFalse;

        return ecc_value_truth(memcmp(ecc_value_stringbytes(&a), ecc_value_stringbytes(&b), aLength) < 0);
    }
    a = ecc_value_tobinary(context, a);
    b = ecc_value_tobinary(context, b);
    if(isnan(a.data.binary) || isnan(b.data.binary))
        return ECCValConstUndefined;
    return ecc_value_truth(a.data.binary < b.data.binary);
}

eccvalue_t ecc_value_less(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, a, b);
    if(a.type == ECC_VALTYPE_UNDEFINED)
        return ECCValConstFalse;
    return a;
}

eccvalue_t ecc_value_more(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, b, a);
    if(a.type == ECC_VALTYPE_UNDEFINED)
        return ECCValConstFalse;
    return a;
}

eccvalue_t ecc_value_lessorequal(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, b, a);
    if(a.type == ECC_VALTYPE_UNDEFINED || a.type == ECC_VALTYPE_TRUE)
        return ECCValConstFalse;
    return ECCValConstTrue;
}

eccvalue_t ecc_value_moreorequal(eccstate_t* context, eccvalue_t a, eccvalue_t b)
{
    a = eccvalue_compare(context, a, b);
    if(a.type == ECC_VALTYPE_UNDEFINED || a.type == ECC_VALTYPE_TRUE)
        return ECCValConstFalse;
    return ECCValConstTrue;
}

const char* ecc_value_typename(int type)
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
    ecc_script_fatal("Invalid value type : %u", type);
    return "unknown";
}

const char* ecc_value_maskname(int mask)
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
    ecc_script_fatal("Invalid value mask : %u", mask);
    return "illegal";
}

void ecc_value_dumpto(eccvalue_t value, FILE* file)
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
            const ecctextstring_t text = ecc_value_textof(&value);
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
            ecc_value_dumpto(*value.data.reference, file);
            return;
        }
        break;
    }
}
