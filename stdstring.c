//
//  string.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static void eccstringtypefn_mark(eccobject_t* object);
static void eccstringtypefn_capture(eccobject_t* object);
static void eccstringtypefn_finalize(eccobject_t* object);

eccobject_t* ECC_Prototype_String = NULL;
eccobjfunction_t* ECC_CtorFunc_String = NULL;

const eccobjinterntype_t ECC_Type_String = {
    .text = &ECC_ConstString_StringType,
    .mark = eccstringtypefn_mark,
    .capture = eccstringtypefn_capture,
    .finalize = eccstringtypefn_finalize,
};

static void nsstringfn_setup(void);
static void nsstringfn_teardown(void);
static eccobjstring_t* nsstringfn_create(ecccharbuffer_t*);
static eccvalue_t nsstringfn_valueAtIndex(eccobjstring_t*, int32_t index);
static ecctextstring_t nsstringfn_textAtIndex(const char* chars, int32_t length, int32_t index, int enableReverse);
static int32_t nsstringfn_unitIndex(const char* chars, int32_t max, int32_t unit);
const struct eccpseudonsstring_t ECCNSString = {
    nsstringfn_setup, nsstringfn_teardown, nsstringfn_create, nsstringfn_valueAtIndex, nsstringfn_textAtIndex, nsstringfn_unitIndex,
    {}
};

static void eccstringtypefn_mark(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    ECCNSMemoryPool.markValue(ecc_value_chars(self->value));
}

static void eccstringtypefn_capture(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    ++self->value->referenceCount;
}

static void eccstringtypefn_finalize(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    --self->value->referenceCount;
}

// MARK: - Static Members

static eccvalue_t objstringfn_toString(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_STRING);

    return ecc_value_chars(context->thisvalue.data.string->value);
}

static eccvalue_t objstringfn_valueOf(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_STRING);

    return ecc_value_chars(context->thisvalue.data.string->value);
}

static eccvalue_t objstringfn_charAt(eccstate_t* context)
{
    int32_t index, length;
    const char* chars;
    ecctextstring_t text;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->thisvalue = ecc_value_tostring(context, context->thisvalue);
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    index = ecc_value_tointeger(context, ECCNSContext.argument(context, 0)).data.integer;

    text = nsstringfn_textAtIndex(chars, length, index, 0);
    if(!text.length)
        return ecc_value_text(&ECC_ConstString_Empty);
    else
    {
        ecctextchar_t c = ECCNSText.character(text);

        if(c.codepoint < 0x010000)
            return ecc_value_buffer(text.bytes, c.units);
        else
        {
            char buffer[7];

            /* simulate 16-bit surrogate */

            c.codepoint -= 0x010000;
            if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
                c.codepoint = ((c.codepoint >> 0) & 0x3ff) + 0xdc00;
            else
                c.codepoint = ((c.codepoint >> 10) & 0x3ff) + 0xd800;

            ECCNSChars.writeCodepoint(buffer, c.codepoint);
            return ecc_value_buffer(buffer, 3);
        }
    }
}

static eccvalue_t objstringfn_charCodeAt(eccstate_t* context)
{
    int32_t index, length;
    const char* chars;
    ecctextstring_t text;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->thisvalue = ecc_value_tostring(context, context->thisvalue);
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    index = ecc_value_tointeger(context, ECCNSContext.argument(context, 0)).data.integer;

    text = nsstringfn_textAtIndex(chars, length, index, 0);
    if(!text.length)
        return ecc_value_binary(NAN);
    else
    {
        ecctextchar_t c = ECCNSText.character(text);

        if(c.codepoint < 0x010000)
            return ecc_value_binary(c.codepoint);
        else
        {
            /* simulate 16-bit surrogate */

            c.codepoint -= 0x010000;
            if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
                return ecc_value_binary(((c.codepoint >> 0) & 0x3ff) + 0xdc00);
            else
                return ecc_value_binary(((c.codepoint >> 10) & 0x3ff) + 0xd800);
        }
    }
}

static eccvalue_t objstringfn_concat(eccstate_t* context)
{
    eccappendbuffer_t chars;
    int32_t index, count;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    count = ECCNSContext.argumentCount(context);

    ECCNSChars.beginAppend(&chars);
    ECCNSChars.appendValue(&chars, context, ECCNSContext.getThis(context));
    for(index = 0; index < count; ++index)
        ECCNSChars.appendValue(&chars, context, ECCNSContext.argument(context, index));

    return ECCNSChars.endAppend(&chars);
}

static eccvalue_t objstringfn_indexOf(eccstate_t* context)
{
    ecctextstring_t text;
    eccvalue_t search, start;
    int32_t index, length, searchLength;
    const char *chars, *searchChars;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    search = ecc_value_tostring(context, ECCNSContext.argument(context, 0));
    searchChars = ecc_value_stringbytes(&search);
    searchLength = ecc_value_stringlength(&search);
    start = ecc_value_tointeger(context, ECCNSContext.argument(context, 1));
    index = start.data.integer < 0 ? length + start.data.integer : start.data.integer;
    if(index < 0)
        index = 0;

    text = nsstringfn_textAtIndex(chars, length, index, 0);
    if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
    {
        ECCNSText.nextCharacter(&text);
        ++index;
    }

    while(text.length)
    {
        if(!memcmp(text.bytes, searchChars, searchLength))
            return ecc_value_integer(index);

        ++index;
        if(ECCNSText.nextCharacter(&text).codepoint > 0xffff)
            ++index;
    }

    return ecc_value_integer(-1);
}

static eccvalue_t objstringfn_lastIndexOf(eccstate_t* context)
{
    ecctextstring_t text;
    eccvalue_t search, start;
    int32_t index, length, searchLength;
    const char *chars, *searchChars;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    search = ecc_value_tostring(context, ECCNSContext.argument(context, 0));
    searchChars = ecc_value_stringbytes(&search);
    searchLength = ecc_value_stringlength(&search);

    start = ecc_value_tobinary(context, ECCNSContext.argument(context, 1));
    index = nsstringfn_unitIndex(chars, length, length);
    if(!isnan(start.data.binary) && start.data.binary < index)
        index = start.data.binary < 0 ? 0 : start.data.binary;

    text = nsstringfn_textAtIndex(chars, length, index, 0);
    if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
        --index;

    text.length = (int32_t)(text.bytes - chars);

    for(;;)
    {
        if(length - (text.bytes - chars) >= searchLength && !memcmp(text.bytes, searchChars, searchLength))
            return ecc_value_integer(index);

        if(!text.length)
            break;

        --index;
        if(ECCNSText.prevCharacter(&text).codepoint > 0xffff)
            --index;
    }

    return ecc_value_integer(-1);
}

static eccvalue_t objstringfn_localeCompare(eccstate_t* context)
{
    eccvalue_t that;
    ecctextstring_t a, b;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));
    a = ecc_value_textof(&context->thisvalue);

    that = ecc_value_tostring(context, ECCNSContext.argument(context, 0));
    b = ecc_value_textof(&that);

    if(a.length < b.length && !memcmp(a.bytes, b.bytes, a.length))
        return ecc_value_integer(-1);

    if(a.length > b.length && !memcmp(a.bytes, b.bytes, b.length))
        return ecc_value_integer(1);

    return ecc_value_integer(memcmp(a.bytes, b.bytes, a.length));
}

static eccvalue_t objstringfn_match(eccstate_t* context)
{
    eccobjregexp_t* regexp;
    eccvalue_t value, lastIndex;

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        regexp = ECCNSRegExp.createWith(context, value, ECCValConstUndefined);

    lastIndex = regexp->global ? ecc_value_integer(0) : ecc_value_tointeger(context, ECCNSObject.getMember(context, &regexp->object, ECC_ConstKey_lastIndex));

    ECCNSObject.putMember(context, &regexp->object, ECC_ConstKey_lastIndex, ecc_value_integer(0));

    if(lastIndex.data.integer >= 0)
    {
        const char* bytes = ecc_value_stringbytes(&context->thisvalue);
        int32_t length = ecc_value_stringlength(&context->thisvalue);
        ecctextstring_t text = nsstringfn_textAtIndex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        eccobject_t* array = ECCNSArray.create();
        eccappendbuffer_t chars;
        uint32_t size = 0;

        do
        {
            eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, strindex, 0};

            if(ECCNSRegExp.matchWithState(regexp, &state))
            {
                ECCNSChars.beginAppend(&chars);
                ECCNSChars.append(&chars, "%.*s", capture[1] - capture[0], capture[0]);
                ECCNSObject.addElement(array, size++, ECCNSChars.endAppend(&chars), 0);

                if(!regexp->global)
                {
                    int32_t numindex, count;

                    for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                        {
                            ECCNSChars.beginAppend(&chars);
                            ECCNSChars.append(&chars, "%.*s", capture[numindex * 2 + 1] - capture[numindex * 2], capture[numindex * 2]);
                            ECCNSObject.addElement(array, size++, ECCNSChars.endAppend(&chars), 0);
                        }
                        else
                            ECCNSObject.addElement(array, size++, ECCValConstUndefined, 0);
                    }
                    break;
                }

                if(capture[1] - text.bytes > 0)
                    ECCNSText.advance(&text, (int32_t)(capture[1] - text.bytes));
                else
                    ECCNSText.nextCharacter(&text);
            }
            else
                break;
        } while(text.length);

        if(size)
        {
            ECCNSObject.addMember(array, ECC_ConstKey_input, context->thisvalue, 0);
            ECCNSObject.addMember(array, ECC_ConstKey_index, ecc_value_integer(ECCNSString.unitIndex(bytes, length, (int32_t)(capture[0] - bytes))), 0);

            if(regexp->global)
                ECCNSObject.putMember(context, &regexp->object, ECC_ConstKey_lastIndex,
                                      ecc_value_integer(ECCNSString.unitIndex(bytes, length, (int32_t)(text.bytes - bytes))));

            return ecc_value_object(array);
        }
    }
    return ECCValConstNull;
}

static void
eccstring_replace(eccappendbuffer_t* chars, ecctextstring_t replace, ecctextstring_t before, ecctextstring_t match, ecctextstring_t after, int count, const char* dcap[])
{
    ecctextchar_t c;

    while(replace.length)
    {
        c = ECCNSText.character(replace);
        if(c.codepoint == '$')
        {
            int index;
            ECCNSText.advance(&replace, 1);
            switch(ECCNSText.character(replace).codepoint)
            {
                case '$':
                    ECCNSChars.append(chars, "$");
                    break;
                case '&':
                    ECCNSChars.append(chars, "%.*s", match.length, match.bytes);
                    break;
                case '`':
                    ECCNSChars.append(chars, "%.*s", before.length, before.bytes);
                    break;
                case '\'':
                    ECCNSChars.append(chars, "%.*s", after.length, after.bytes);
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    {
                        index = replace.bytes[0] - '0';
                        if(index < count)
                        {
                            if(isdigit(replace.bytes[1]) && index * 10 <= count)
                            {
                                index = index * 10 + replace.bytes[1] - '0';
                                ECCNSText.advance(&replace, 1);
                            }
                            if(dcap && index && index < count)
                            {
                                if(dcap[index * 2])
                                {
                                    ECCNSChars.append(chars, "%.*s", dcap[index * 2 + 1] - dcap[index * 2], dcap[index * 2]);
                                }
                                else
                                {
                                    ECCNSChars.append(chars, "");
                                }
                                break;
                            }
                        }
                    }
                    /* fallthrough  */
                default:
                    {
                        ECCNSChars.append(chars, "$");
                    }
                    continue;
            }
        }
        else
        {
            ECCNSChars.append(chars, "%.*s", c.units, replace.bytes);
        }
        ECCNSText.advance(&replace, c.units);
    }
}

static eccvalue_t objstringfn_replace(eccstate_t* context)
{
    eccobjregexp_t* regexp = NULL;
    eccappendbuffer_t chars;
    eccvalue_t value, replace;
    ecctextstring_t text;
    const char *bytes, *searchBytes;
    int32_t length, searchLength;
    ECCNSContext.assertThisCoerciblePrimitive(context);
    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));
    bytes = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    text = ECCNSText.make(bytes, length);
    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        value = ecc_value_tostring(context, value);
    replace = ECCNSContext.argument(context, 1);
    if(replace.type != ECC_VALTYPE_FUNCTION)
        replace = ecc_value_tostring(context, replace);
    if(regexp)
    {
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        ecctextstring_t seek = text;

        ECCNSChars.beginAppend(&chars);
        do
        {
            eccrxstate_t state = { seek.bytes, text.bytes + text.length, capture, strindex, 0 };

            if(ECCNSRegExp.matchWithState(regexp, &state))
            {
                ECCNSChars.append(&chars, "%.*s", capture[0] - text.bytes, text.bytes);

                if(replace.type == ECC_VALTYPE_FUNCTION)
                {
                    eccobject_t* arguments = ECCNSArray.createSized(regexp->count + 2);
                    int32_t numindex, count;
                    eccvalue_t result;

                    for(numindex = 0, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                            arguments->element[numindex].value
                            = ecc_value_chars(ECCNSChars.createWithBytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]));
                        else
                            arguments->element[numindex].value = ECCValConstUndefined;
                    }
                    arguments->element[regexp->count].value = ecc_value_integer(ECCNSString.unitIndex(bytes, length, (int32_t)(capture[0] - bytes)));
                    arguments->element[regexp->count + 1].value = context->thisvalue;

                    result = ecc_value_tostring(context, ECCNSOperand.callFunctionArguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
                    ECCNSChars.append(&chars, "%.*s", ecc_value_stringlength(&result), ecc_value_stringbytes(&result));
                }
                else
                    eccstring_replace(&chars, ECCNSText.make(ecc_value_stringbytes(&replace), ecc_value_stringlength(&replace)),
                                ECCNSText.make(bytes, (int32_t)(capture[0] - bytes)), ECCNSText.make(capture[0], (int32_t)(capture[1] - capture[0])),
                                ECCNSText.make(capture[1], (int32_t)((bytes + length) - capture[1])), regexp->count, capture);

                ECCNSText.advance(&text, (int32_t)(state.capture[1] - text.bytes));

                seek = text;
                if(text.bytes == state.capture[1])
                    ECCNSText.nextCharacter(&seek);
            }
            else
                break;
        } while(text.length && regexp->global);

        ECCNSChars.append(&chars, "%.*s", text.length, text.bytes);

        return ECCNSChars.endAppend(&chars);
    }
    else
    {
        searchBytes = ecc_value_stringbytes(&value);
        searchLength = ecc_value_stringlength(&value);

        for(;;)
        {
            if(!text.length)
                return context->thisvalue;

            if(!memcmp(text.bytes, searchBytes, searchLength))
            {
                text.length = searchLength;
                break;
            }
            ECCNSText.nextCharacter(&text);
        }

        ECCNSChars.beginAppend(&chars);
        ECCNSChars.append(&chars, "%.*s", text.bytes - bytes, bytes);

        if(replace.type == ECC_VALTYPE_FUNCTION)
        {
            eccobject_t* arguments = ECCNSArray.createSized(1 + 2);
            eccvalue_t result;

            arguments->element[0].value = ecc_value_chars(ECCNSChars.createWithBytes(text.length, text.bytes));
            arguments->element[1].value = ecc_value_integer(ECCNSString.unitIndex(bytes, length, (int32_t)(text.bytes - bytes)));
            arguments->element[2].value = context->thisvalue;

            result = ecc_value_tostring(context, ECCNSOperand.callFunctionArguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
            ECCNSChars.append(&chars, "%.*s", ecc_value_stringlength(&result), ecc_value_stringbytes(&result));
        }
        else
            eccstring_replace(&chars, ECCNSText.make(ecc_value_stringbytes(&replace), ecc_value_stringlength(&replace)),
                        ECCNSText.make(text.bytes, (int32_t)(text.bytes - bytes)), ECCNSText.make(text.bytes, text.length),
                        ECCNSText.make(text.bytes, (int32_t)(length - (text.bytes - bytes))), 0, NULL);

        ECCNSChars.append(&chars, "%.*s", length - (text.bytes - bytes), text.bytes + text.length);

        return ECCNSChars.endAppend(&chars);
    }
}

static eccvalue_t objstringfn_search(eccstate_t* context)
{
    eccobjregexp_t* regexp;
    eccvalue_t value;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        regexp = ECCNSRegExp.createWith(context, value, ECCValConstUndefined);

    {
        const char* bytes = ecc_value_stringbytes(&context->thisvalue);
        int32_t length = ecc_value_stringlength(&context->thisvalue);
        ecctextstring_t text = nsstringfn_textAtIndex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* index[regexp->count * 2];

        eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, index, 0 };

        if(ECCNSRegExp.matchWithState(regexp, &state))
            return ecc_value_integer(nsstringfn_unitIndex(bytes, length, (int32_t)(capture[0] - bytes)));
    }
    return ecc_value_integer(-1);
}

static eccvalue_t objstringfn_slice(eccstate_t* context)
{
    eccvalue_t from, to;
    ecctextstring_t start, end;
    const char* chars;
    int32_t length;
    uint16_t head = 0, tail = 0;
    uint32_t headcp = 0;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));

    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    from = ECCNSContext.argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED)
        start = ECCNSText.make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.binary == INFINITY)
        start = ECCNSText.make(chars + length, 0);
    else
        start = nsstringfn_textAtIndex(chars, length, ecc_value_tointeger(context, from).data.integer, 1);

    to = ECCNSContext.argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && (isnan(to.data.binary) || to.data.binary == INFINITY)))
        end = ECCNSText.make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && to.data.binary == -INFINITY)
        end = ECCNSText.make(chars, length);
    else
        end = nsstringfn_textAtIndex(chars, length, ecc_value_tointeger(context, to).data.integer, 1);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        headcp = ECCNSText.nextCharacter(&start).codepoint;

    length = (int32_t)(end.bytes - start.bytes);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        head = 3;

    if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        tail = 3;

    if(head + length + tail <= 0)
        return ecc_value_text(&ECC_ConstString_Empty);
    else
    {
        ecccharbuffer_t* result = ECCNSChars.createSized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ECCNSChars.writeCodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ECCNSChars.writeCodepoint(result->bytes + head + length, (((ECCNSText.character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
        }

        return ecc_value_chars(result);
    }
}

static eccvalue_t objstringfn_split(eccstate_t* context)
{
    eccvalue_t separatorValue, limitValue;
    eccobjregexp_t* regexp = NULL;
    eccobject_t* array;
    ecccharbuffer_t* element;
    ecctextstring_t text, separator = { 0 };
    uint32_t size = 0, limit = UINT32_MAX;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));
    text = ecc_value_textof(&context->thisvalue);

    limitValue = ECCNSContext.argument(context, 1);
    if(limitValue.type != ECC_VALTYPE_UNDEFINED)
    {
        limit = ecc_value_tointeger(context, limitValue).data.integer;
        if(!limit)
            return ecc_value_object(ECCNSArray.createSized(0));
    }

    separatorValue = ECCNSContext.argument(context, 0);
    if(separatorValue.type == ECC_VALTYPE_UNDEFINED)
    {
        eccobject_t* subarr = ECCNSArray.createSized(1);
        subarr->element[0].value = context->thisvalue;
        return ecc_value_object(subarr);
    }
    else if(separatorValue.type == ECC_VALTYPE_REGEXP)
        regexp = separatorValue.data.regexp;
    else
    {
        separatorValue = ecc_value_tostring(context, separatorValue);
        separator = ecc_value_textof(&separatorValue);
    }

    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);

    array = ECCNSArray.create();

    if(regexp)
    {
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        ecctextstring_t seek = text;

        for(;;)
        {
            eccrxstate_t state = { seek.bytes, seek.bytes + seek.length, capture, strindex, 0};
            int32_t numindex, count;

            if(size >= limit)
                break;

            if(seek.length && ECCNSRegExp.matchWithState(regexp, &state))
            {
                if(capture[1] <= text.bytes)
                {
                    ECCNSText.advance(&seek, 1);
                    continue;
                }

                element = ECCNSChars.createWithBytes((int32_t)(capture[0] - text.bytes), text.bytes);
                ECCNSObject.addElement(array, size++, ecc_value_chars(element), 0);

                for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                {
                    if(size >= limit)
                        break;

                    if(capture[numindex * 2])
                    {
                        element = ECCNSChars.createWithBytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]);
                        ECCNSObject.addElement(array, size++, ecc_value_chars(element), 0);
                    }
                    else
                        ECCNSObject.addElement(array, size++, ECCValConstUndefined, 0);
                }

                ECCNSText.advance(&text, (int32_t)(capture[1] - text.bytes));
                seek = text;
            }
            else
            {
                element = ECCNSChars.createWithBytes(text.length, text.bytes);
                ECCNSObject.addElement(array, size++, ecc_value_chars(element), 0);
                break;
            }
        }
        return ecc_value_object(array);
    }
    else if(!separator.length)
    {
        ecctextchar_t c;

        while(text.length)
        {
            if(size >= limit)
                break;

            c = ECCNSText.character(text);
            if(c.codepoint < 0x010000)
                ECCNSObject.addElement(array, size++, ecc_value_buffer(text.bytes, c.units), 0);
            else
            {
                char buffer[7];

                /* simulate 16-bit surrogate */

                ECCNSChars.writeCodepoint(buffer, (((c.codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
                ECCNSObject.addElement(array, size++, ecc_value_buffer(buffer, 3), 0);

                ECCNSChars.writeCodepoint(buffer, (((c.codepoint - 0x010000) >> 0) & 0x3ff) + 0xdc00);
                ECCNSObject.addElement(array, size++, ecc_value_buffer(buffer, 3), 0);
            }
            ECCNSText.advance(&text, c.units);
        }

        return ecc_value_object(array);
    }
    else
    {
        ecctextstring_t seek = text;
        int32_t length;

        while(seek.length >= separator.length)
        {
            if(size >= limit)
                break;

            if(!memcmp(seek.bytes, separator.bytes, separator.length))
            {
                length = (int32_t)(seek.bytes - text.bytes);
                element = ECCNSChars.createSized(length);
                memcpy(element->bytes, text.bytes, length);
                ECCNSObject.addElement(array, size++, ecc_value_chars(element), 0);

                ECCNSText.advance(&text, length + separator.length);
                seek = text;
                continue;
            }
            ECCNSText.nextCharacter(&seek);
        }

        if(size < limit)
        {
            element = ECCNSChars.createSized(text.length);
            memcpy(element->bytes, text.bytes, text.length);
            ECCNSObject.addElement(array, size++, ecc_value_chars(element), 0);
        }
    }

    return ecc_value_object(array);
}

static eccvalue_t objstringfn_substring(eccstate_t* context)
{
    eccvalue_t from, to;
    ecctextstring_t start, end;
    const char* chars;
    int32_t length, head = 0, tail = 0;
    uint32_t headcp = 0;

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    from = ECCNSContext.argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED || (from.type == ECC_VALTYPE_BINARY && (isnan(from.data.binary) || from.data.binary == -INFINITY)))
        start = ECCNSText.make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.binary == INFINITY)
        start = ECCNSText.make(chars + length, 0);
    else
        start = nsstringfn_textAtIndex(chars, length, ecc_value_tointeger(context, from).data.integer, 0);

    to = ECCNSContext.argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && to.data.binary == INFINITY))
        end = ECCNSText.make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && !isfinite(to.data.binary))
        end = ECCNSText.make(chars, length);
    else
        end = nsstringfn_textAtIndex(chars, length, ecc_value_tointeger(context, to).data.integer, 0);

    if(start.bytes > end.bytes)
    {
        ecctextstring_t temp = start;
        start = end;
        end = temp;
    }

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        headcp = ECCNSText.nextCharacter(&start).codepoint;

    length = (int32_t)(end.bytes - start.bytes);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        head = 3;

    if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        tail = 3;

    if(head + length + tail <= 0)
        return ecc_value_text(&ECC_ConstString_Empty);
    else
    {
        ecccharbuffer_t* result = ECCNSChars.createSized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ECCNSChars.writeCodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ECCNSChars.writeCodepoint(result->bytes + head + length, (((ECCNSText.character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
        }

        return ecc_value_chars(result);
    }
}

static eccvalue_t objstringfn_toLowerCase(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));

    text = ecc_value_textof(&context->thisvalue);
    {
        char buffer[text.length * 2];
        char* end = ECCNSText.toLower(text, buffer);
        chars = ECCNSChars.createWithBytes((int32_t)(end - buffer), buffer);
    }

    return ecc_value_chars(chars);
}

static eccvalue_t objstringfn_toUpperCase(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text;

    context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));
    text = ecc_value_textof(&context->thisvalue);
    {
        char buffer[text.length * 3];
        char* end = ECCNSText.toUpper(text, buffer);
        chars = ECCNSChars.createWithBytes((int32_t)(end - buffer), buffer);
    }

    return ecc_value_chars(chars);
}

static eccvalue_t objstringfn_trim(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text, last;
    ecctextchar_t c;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ECCNSContext.getThis(context));

    text = ecc_value_textof(&context->thisvalue);
    while(text.length)
    {
        c = ECCNSText.character(text);
        if(!ECCNSText.isSpace(c))
            break;

        ECCNSText.advance(&text, c.units);
    }

    last = ECCNSText.make(text.bytes + text.length, text.length);
    while(last.length)
    {
        c = ECCNSText.prevCharacter(&last);
        if(!ECCNSText.isSpace(c))
            break;

        text.length = last.length;
    }

    chars = ECCNSChars.createWithBytes(text.length, text.bytes);

    return ecc_value_chars(chars);
}

static eccvalue_t objstringfn_constructor(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        value = ecc_value_text(value.check == 1 ? &ECC_ConstString_Undefined : &ECC_ConstString_Empty);
    else
        value = ecc_value_tostring(context, value);

    if(context->construct)
        return ecc_value_string(nsstringfn_create(ECCNSChars.createWithBytes(ecc_value_stringlength(&value), ecc_value_stringbytes(&value))));
    else
        return value;
}

static eccvalue_t objstringfn_fromCharCode(eccstate_t* context)
{
    eccappendbuffer_t chars;
    int32_t index, count;

    count = ECCNSContext.argumentCount(context);

    ECCNSChars.beginAppend(&chars);

    for(index = 0; index < count; ++index)
        ECCNSChars.appendCodepoint(&chars, (uint16_t)ecc_value_tointeger(context, ECCNSContext.argument(context, index)).data.integer);

    return ECCNSChars.endAppend(&chars);
}

static void nsstringfn_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_String, objstringfn_constructor, 1, &ECC_Prototype_String,
                                          ecc_value_string(nsstringfn_create(ECCNSChars.createSized(0))), &ECC_Type_String);

    ECCNSFunction.addMethod(ECC_CtorFunc_String, "fromCharCode", objstringfn_fromCharCode, -1, h);

    ECCNSFunction.addToObject(ECC_Prototype_String, "toString", objstringfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "valueOf", objstringfn_valueOf, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "charAt", objstringfn_charAt, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "charCodeAt", objstringfn_charCodeAt, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "concat", objstringfn_concat, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "indexOf", objstringfn_indexOf, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "lastIndexOf", objstringfn_lastIndexOf, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "localeCompare", objstringfn_localeCompare, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "match", objstringfn_match, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "replace", objstringfn_replace, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "search", objstringfn_search, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "slice", objstringfn_slice, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "split", objstringfn_split, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "substring", objstringfn_substring, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "toLowerCase", objstringfn_toLowerCase, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "toLocaleLowerCase", objstringfn_toLowerCase, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "toUpperCase", objstringfn_toUpperCase, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "toLocaleUpperCase", objstringfn_toUpperCase, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_String, "trim", objstringfn_trim, 0, h);
}

static void nsstringfn_teardown(void)
{
    ECC_Prototype_String = NULL;
    ECC_CtorFunc_String = NULL;
}

static eccobjstring_t* nsstringfn_create(ecccharbuffer_t* chars)
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;
    uint32_t length;
    eccobjstring_t* self;
    self = (eccobjstring_t*)malloc(sizeof(*self));
    *self = ECCNSString.identity;
    ECCNSMemoryPool.addObject(&self->object);
    ECCNSObject.initialize(&self->object, ECC_Prototype_String);
    length = nsstringfn_unitIndex(chars->bytes, chars->length, chars->length);
    ECCNSObject.addMember(&self->object, ECC_ConstKey_length, ecc_value_integer(length), r | h | s);
    self->value = chars;
    if(length == (uint32_t)chars->length)
    {
        chars->flags |= ECC_CHARBUFFLAG_ASCIIONLY;
    }
    return self;
}

static eccvalue_t nsstringfn_valueAtIndex(eccobjstring_t* self, int32_t index)
{
    ecctextchar_t c;
    ecctextstring_t text;

    text = nsstringfn_textAtIndex(self->value->bytes, self->value->length, index, 0);
    c = ECCNSText.character(text);

    if(c.units <= 0)
        return ECCValConstUndefined;
    else
    {
        if(c.codepoint < 0x010000)
            return ecc_value_buffer(text.bytes, c.units);
        else
        {
            char buffer[7];

            /* simulate 16-bit surrogate */

            c.codepoint -= 0x010000;
            if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
                c.codepoint = ((c.codepoint >> 0) & 0x3ff) + 0xdc00;
            else
                c.codepoint = ((c.codepoint >> 10) & 0x3ff) + 0xd800;

            ECCNSChars.writeCodepoint(buffer, c.codepoint);
            return ecc_value_buffer(buffer, 3);
        }
    }
}

static ecctextstring_t nsstringfn_textAtIndex(const char* chars, int32_t length, int32_t position, int enableReverse)
{
    ecctextstring_t text = ECCNSText.make(chars, length), prev;
    ecctextchar_t c;

    if(position >= 0)
    {
        while(position-- > 0)
        {
            prev = text;
            c = ECCNSText.nextCharacter(&text);

            if(c.codepoint > 0xffff && !position--)
            {
                /* simulate 16-bit surrogate */
                text = prev;
                text.flags = ECC_TEXTFLAG_BREAKFLAG;
            }
        }
    }
    else if(enableReverse)
    {
        text.bytes += length;

        while(position++ < 0)
        {
            c = ECCNSText.prevCharacter(&text);

            if(c.codepoint > 0xffff && position++ >= 0)
            {
                /* simulate 16-bit surrogate */
                text.flags = ECC_TEXTFLAG_BREAKFLAG;
            }
        }

        text.length = length - (int32_t)(text.bytes - chars);
    }
    else
        text.length = 0;

    return text;
}

static int32_t nsstringfn_unitIndex(const char* chars, int32_t max, int32_t unit)
{
    ecctextstring_t text = ECCNSText.make(chars, max);
    int32_t position = 0;
    ecctextchar_t c;

    while(unit > 0)
    {
        if(text.length)
        {
            ++position;
            c = ECCNSText.nextCharacter(&text);
            unit -= c.units;

            if(c.codepoint > 0xffff) /* simulate 16-bit surrogate */
                ++position;
        }
    }

    return position;
}
