
/*
//  string.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"
#include "compat.h"

static void ecc_string_typefnmark(eccobject_t *object);
static void ecc_string_typefncapture(eccobject_t *object);
static void ecc_string_typefnfinalize(eccobject_t *object);
static eccvalue_t ecc_objfnstring_tostring(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_valueof(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_charat(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_charcodeat(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_concat(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_indexof(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_lastindexof(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_localecompare(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_match(ecccontext_t *context);
static void ecc_stringutil_replace(eccappbuf_t *chars, eccstrbox_t replace, eccstrbox_t before, eccstrbox_t match, eccstrbox_t after, int count, const char *dcap[]);
static eccvalue_t ecc_objfnstring_replace(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_search(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_slice(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_split(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_substring(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_tolowercase(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_touppercase(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_trim(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_constructor(ecccontext_t *context);
static eccvalue_t ecc_objfnstring_fromcharcode(ecccontext_t *context);


eccobject_t* ECC_Prototype_String = NULL;
eccobjfunction_t* ECC_CtorFunc_String = NULL;

const eccobjinterntype_t ECC_Type_String = {
    .text = &ECC_String_StringType,
    .fnmark = ecc_string_typefnmark,
    .fncapture = ecc_string_typefncapture,
    .fnfinalize = ecc_string_typefnfinalize,
};


static void ecc_string_typefnmark(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    ecc_mempool_markvalue(ecc_value_fromchars(self->sbuf));
}

static void ecc_string_typefncapture(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    ++self->sbuf->refcount;
}

static void ecc_string_typefnfinalize(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    --self->sbuf->refcount;
}

static eccvalue_t ecc_objfnstring_tostring(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_STRING);

    return ecc_value_fromchars(context->thisvalue.data.string->sbuf);
}

static eccvalue_t ecc_objfnstring_valueof(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_STRING);

    return ecc_value_fromchars(context->thisvalue.data.string->sbuf);
}

static eccvalue_t ecc_objfnstring_charat(ecccontext_t* context)
{
    int32_t index, length;
    const char* chars;
    eccstrbox_t text;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, context->thisvalue);
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    index = ecc_value_tointeger(context, ecc_context_argument(context, 0)).data.integer;

    text = ecc_string_textatindex(chars, length, index, 0);
    if(!text.length)
        return ecc_value_fromtext(&ECC_String_Empty);
    else
    {
        eccrune_t c = ecc_strbox_character(text);

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

            ecc_strbuf_writecodepoint(buffer, c.codepoint);
            return ecc_value_buffer(buffer, 3);
        }
    }
}

static eccvalue_t ecc_objfnstring_charcodeat(ecccontext_t* context)
{
    int32_t index, length;
    const char* chars;
    eccstrbox_t text;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, context->thisvalue);
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    index = ecc_value_tointeger(context, ecc_context_argument(context, 0)).data.integer;

    text = ecc_string_textatindex(chars, length, index, 0);
    if(!text.length)
        return ecc_value_fromfloat(ECC_CONST_NAN);
    else
    {
        eccrune_t c = ecc_strbox_character(text);

        if(c.codepoint < 0x010000)
            return ecc_value_fromfloat(c.codepoint);
        else
        {
            /* simulate 16-bit surrogate */

            c.codepoint -= 0x010000;
            if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
                return ecc_value_fromfloat(((c.codepoint >> 0) & 0x3ff) + 0xdc00);
            else
                return ecc_value_fromfloat(((c.codepoint >> 10) & 0x3ff) + 0xd800);
        }
    }
}

static eccvalue_t ecc_objfnstring_concat(ecccontext_t* context)
{
    eccappbuf_t chars;
    int32_t index, count;

    ecc_context_assertthiscoercibleprimitive(context);

    count = ecc_context_argumentcount(context);

    ecc_strbuf_beginappend(&chars);
    ecc_strbuf_appendvalue(&chars, context, ecc_context_this(context));
    for(index = 0; index < count; ++index)
        ecc_strbuf_appendvalue(&chars, context, ecc_context_argument(context, index));

    return ecc_strbuf_endappend(&chars);
}

static eccvalue_t ecc_objfnstring_indexof(ecccontext_t* context)
{
    eccstrbox_t text;
    eccvalue_t search, start;
    int32_t index, length, searchLength;
    const char *chars, *searchChars;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    search = ecc_value_tostring(context, ecc_context_argument(context, 0));
    searchChars = ecc_value_stringbytes(&search);
    searchLength = ecc_value_stringlength(&search);
    start = ecc_value_tointeger(context, ecc_context_argument(context, 1));
    index = start.data.integer < 0 ? length + start.data.integer : start.data.integer;
    if(index < 0)
        index = 0;

    text = ecc_string_textatindex(chars, length, index, 0);
    if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
    {
        ecc_strbox_nextcharacter(&text);
        ++index;
    }

    while(text.length)
    {
        if(!memcmp(text.bytes, searchChars, searchLength))
            return ecc_value_fromint(index);

        ++index;
        if(ecc_strbox_nextcharacter(&text).codepoint > 0xffff)
            ++index;
    }

    return ecc_value_fromint(-1);
}

static eccvalue_t ecc_objfnstring_lastindexof(ecccontext_t* context)
{
    eccstrbox_t text;
    eccvalue_t search, start;
    int32_t index, length, searchLength;
    const char *chars, *searchChars;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    search = ecc_value_tostring(context, ecc_context_argument(context, 0));
    searchChars = ecc_value_stringbytes(&search);
    searchLength = ecc_value_stringlength(&search);

    start = ecc_value_tobinary(context, ecc_context_argument(context, 1));
    index = ecc_string_unitindex(chars, length, length);
    if(!isnan(start.data.valnumfloat) && start.data.valnumfloat < index)
        index = start.data.valnumfloat < 0 ? 0 : start.data.valnumfloat;

    text = ecc_string_textatindex(chars, length, index, 0);
    if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
        --index;

    text.length = (int32_t)(text.bytes - chars);

    for(;;)
    {
        if(length - (text.bytes - chars) >= searchLength && !memcmp(text.bytes, searchChars, searchLength))
            return ecc_value_fromint(index);

        if(!text.length)
            break;

        --index;
        if(ecc_strbox_prevcharacter(&text).codepoint > 0xffff)
            --index;
    }

    return ecc_value_fromint(-1);
}

static eccvalue_t ecc_objfnstring_localecompare(ecccontext_t* context)
{
    eccvalue_t that;
    eccstrbox_t a, b;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    a = ecc_value_textof(&context->thisvalue);

    that = ecc_value_tostring(context, ecc_context_argument(context, 0));
    b = ecc_value_textof(&that);

    if(a.length < b.length && !memcmp(a.bytes, b.bytes, a.length))
        return ecc_value_fromint(-1);

    if(a.length > b.length && !memcmp(a.bytes, b.bytes, b.length))
        return ecc_value_fromint(1);

    return ecc_value_fromint(memcmp(a.bytes, b.bytes, a.length));
}

static eccvalue_t ecc_objfnstring_match(ecccontext_t* context)
{
    eccobjregexp_t* regexp;
    eccvalue_t value, lastIndex;

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        regexp = ecc_regexp_createwith(context, value, ECCValConstUndefined);

    lastIndex = regexp->isflagglobal ? ecc_value_fromint(0) : ecc_value_tointeger(context, ecc_object_getmember(context, &regexp->object, ECC_ConstKey_lastIndex));

    ecc_object_putmember(context, &regexp->object, ECC_ConstKey_lastIndex, ecc_value_fromint(0));

    if(lastIndex.data.integer >= 0)
    {
        const char* bytes = ecc_value_stringbytes(&context->thisvalue);
        int32_t length = ecc_value_stringlength(&context->thisvalue);
        eccstrbox_t text = ecc_string_textatindex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        eccobject_t* array = ecc_array_create();
        eccappbuf_t chars;
        uint32_t size = 0;

        do
        {
            eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, strindex, 0};

            if(ecc_regexp_matchwithstate(regexp, &state))
            {
                ecc_strbuf_beginappend(&chars);
                ecc_strbuf_append(&chars, "%.*s", capture[1] - capture[0], capture[0]);
                ecc_object_addelement(array, size++, ecc_strbuf_endappend(&chars), 0);

                if(!regexp->isflagglobal)
                {
                    int32_t numindex, count;

                    for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                        {
                            ecc_strbuf_beginappend(&chars);
                            ecc_strbuf_append(&chars, "%.*s", capture[numindex * 2 + 1] - capture[numindex * 2], capture[numindex * 2]);
                            ecc_object_addelement(array, size++, ecc_strbuf_endappend(&chars), 0);
                        }
                        else
                            ecc_object_addelement(array, size++, ECCValConstUndefined, 0);
                    }
                    break;
                }

                if(capture[1] - text.bytes > 0)
                    ecc_strbox_advance(&text, (int32_t)(capture[1] - text.bytes));
                else
                    ecc_strbox_nextcharacter(&text);
            }
            else
                break;
        } while(text.length);

        if(size)
        {
            ecc_object_addmember(array, ECC_ConstKey_input, context->thisvalue, 0);
            ecc_object_addmember(array, ECC_ConstKey_index, ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(capture[0] - bytes))), 0);

            if(regexp->isflagglobal)
                ecc_object_putmember(context, &regexp->object, ECC_ConstKey_lastIndex,
                                      ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(text.bytes - bytes))));

            return ecc_value_object(array);
        }
    }
    return ECCValConstNull;
}

static void
ecc_stringutil_replace(eccappbuf_t* chars, eccstrbox_t replace, eccstrbox_t before, eccstrbox_t match, eccstrbox_t after, int count, const char* dcap[])
{
    eccrune_t c;

    while(replace.length)
    {
        c = ecc_strbox_character(replace);
        if(c.codepoint == '$')
        {
            int index;
            ecc_strbox_advance(&replace, 1);
            switch(ecc_strbox_character(replace).codepoint)
            {
                case '$':
                    ecc_strbuf_append(chars, "$");
                    break;
                case '&':
                    ecc_strbuf_append(chars, "%.*s", match.length, match.bytes);
                    break;
                case '`':
                    ecc_strbuf_append(chars, "%.*s", before.length, before.bytes);
                    break;
                case '\'':
                    ecc_strbuf_append(chars, "%.*s", after.length, after.bytes);
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
                                ecc_strbox_advance(&replace, 1);
                            }
                            if(dcap && index && index < count)
                            {
                                if(dcap[index * 2])
                                {
                                    ecc_strbuf_append(chars, "%.*s", dcap[index * 2 + 1] - dcap[index * 2], dcap[index * 2]);
                                }
                                else
                                {
                                    ecc_strbuf_append(chars, "");
                                }
                                break;
                            }
                        }
                    }
                    /* fallthrough  */
                default:
                    {
                        ecc_strbuf_append(chars, "$");
                    }
                    continue;
            }
        }
        else
        {
            ecc_strbuf_append(chars, "%.*s", c.units, replace.bytes);
        }
        ecc_strbox_advance(&replace, c.units);
    }
}

static eccvalue_t ecc_objfnstring_replace(ecccontext_t* context)
{
    eccobjregexp_t* regexp = NULL;
    eccappbuf_t chars;
    eccvalue_t value, replace;
    eccstrbox_t text;
    const char *bytes, *searchBytes;
    int32_t length, searchLength;
    ecc_context_assertthiscoercibleprimitive(context);
    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    bytes = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    text = ecc_strbox_make(bytes, length);
    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        value = ecc_value_tostring(context, value);
    replace = ecc_context_argument(context, 1);
    if(replace.type != ECC_VALTYPE_FUNCTION)
        replace = ecc_value_tostring(context, replace);
    if(regexp)
    {
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        eccstrbox_t seek = text;

        ecc_strbuf_beginappend(&chars);
        do
        {
            eccrxstate_t state = { seek.bytes, text.bytes + text.length, capture, strindex, 0 };

            if(ecc_regexp_matchwithstate(regexp, &state))
            {
                ecc_strbuf_append(&chars, "%.*s", capture[0] - text.bytes, text.bytes);

                if(replace.type == ECC_VALTYPE_FUNCTION)
                {
                    eccobject_t* arguments = ecc_array_createsized(regexp->count + 2);
                    int32_t numindex, count;
                    eccvalue_t result;

                    for(numindex = 0, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                            arguments->hmapitemitems[numindex].hmapitemvalue
                            = ecc_value_fromchars(ecc_strbuf_createwithbytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]));
                        else
                            arguments->hmapitemitems[numindex].hmapitemvalue = ECCValConstUndefined;
                    }
                    arguments->hmapitemitems[regexp->count].hmapitemvalue = ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(capture[0] - bytes)));
                    arguments->hmapitemitems[regexp->count + 1].hmapitemvalue = context->thisvalue;

                    result = ecc_value_tostring(context, ecc_oper_callfunctionarguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
                    ecc_strbuf_append(&chars, "%.*s", ecc_value_stringlength(&result), ecc_value_stringbytes(&result));
                }
                else
                    ecc_stringutil_replace(&chars, ecc_strbox_make(ecc_value_stringbytes(&replace), ecc_value_stringlength(&replace)),
                                ecc_strbox_make(bytes, (int32_t)(capture[0] - bytes)), ecc_strbox_make(capture[0], (int32_t)(capture[1] - capture[0])),
                                ecc_strbox_make(capture[1], (int32_t)((bytes + length) - capture[1])), regexp->count, capture);

                ecc_strbox_advance(&text, (int32_t)(state.capture[1] - text.bytes));

                seek = text;
                if(text.bytes == state.capture[1])
                    ecc_strbox_nextcharacter(&seek);
            }
            else
                break;
        } while(text.length && regexp->isflagglobal);

        ecc_strbuf_append(&chars, "%.*s", text.length, text.bytes);

        return ecc_strbuf_endappend(&chars);
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
            ecc_strbox_nextcharacter(&text);
        }

        ecc_strbuf_beginappend(&chars);
        ecc_strbuf_append(&chars, "%.*s", text.bytes - bytes, bytes);

        if(replace.type == ECC_VALTYPE_FUNCTION)
        {
            eccobject_t* arguments = ecc_array_createsized(1 + 2);
            eccvalue_t result;

            arguments->hmapitemitems[0].hmapitemvalue = ecc_value_fromchars(ecc_strbuf_createwithbytes(text.length, text.bytes));
            arguments->hmapitemitems[1].hmapitemvalue = ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(text.bytes - bytes)));
            arguments->hmapitemitems[2].hmapitemvalue = context->thisvalue;

            result = ecc_value_tostring(context, ecc_oper_callfunctionarguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
            ecc_strbuf_append(&chars, "%.*s", ecc_value_stringlength(&result), ecc_value_stringbytes(&result));
        }
        else
            ecc_stringutil_replace(&chars, ecc_strbox_make(ecc_value_stringbytes(&replace), ecc_value_stringlength(&replace)),
                        ecc_strbox_make(text.bytes, (int32_t)(text.bytes - bytes)), ecc_strbox_make(text.bytes, text.length),
                        ecc_strbox_make(text.bytes, (int32_t)(length - (text.bytes - bytes))), 0, NULL);

        ecc_strbuf_append(&chars, "%.*s", length - (text.bytes - bytes), text.bytes + text.length);

        return ecc_strbuf_endappend(&chars);
    }
}

static eccvalue_t ecc_objfnstring_search(ecccontext_t* context)
{
    eccobjregexp_t* regexp;
    eccvalue_t value;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        regexp = ecc_regexp_createwith(context, value, ECCValConstUndefined);

    {
        const char* bytes = ecc_value_stringbytes(&context->thisvalue);
        int32_t length = ecc_value_stringlength(&context->thisvalue);
        eccstrbox_t text = ecc_string_textatindex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* index[regexp->count * 2];

        eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, index, 0 };

        if(ecc_regexp_matchwithstate(regexp, &state))
            return ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(capture[0] - bytes)));
    }
    return ecc_value_fromint(-1);
}

static eccvalue_t ecc_objfnstring_slice(ecccontext_t* context)
{
    eccvalue_t from, to;
    eccstrbox_t start, end;
    const char* chars;
    int32_t length;
    uint16_t head = 0, tail = 0;
    uint32_t headcp = 0;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    from = ecc_context_argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED)
        start = ecc_strbox_make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.valnumfloat == ECC_CONST_INFINITY)
        start = ecc_strbox_make(chars + length, 0);
    else
        start = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, from).data.integer, 1);

    to = ecc_context_argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && (isnan(to.data.valnumfloat) || to.data.valnumfloat == ECC_CONST_INFINITY)))
        end = ecc_strbox_make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && to.data.valnumfloat == -ECC_CONST_INFINITY)
        end = ecc_strbox_make(chars, length);
    else
        end = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, to).data.integer, 1);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        headcp = ecc_strbox_nextcharacter(&start).codepoint;

    length = (int32_t)(end.bytes - start.bytes);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        head = 3;

    if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        tail = 3;

    if(head + length + tail <= 0)
        return ecc_value_fromtext(&ECC_String_Empty);
    else
    {
        eccstrbuffer_t* result = ecc_strbuf_createsized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_strbuf_writecodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_strbuf_writecodepoint(result->bytes + head + length, (((ecc_strbox_character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
        }

        return ecc_value_fromchars(result);
    }
}

static eccvalue_t ecc_objfnstring_split(ecccontext_t* context)
{
    eccvalue_t separatorValue, limitValue;
    eccobjregexp_t* regexp = NULL;
    eccobject_t* array;
    eccstrbuffer_t* element;
    eccstrbox_t text, separator = { 0 };
    uint32_t size = 0, limit = UINT32_MAX;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    text = ecc_value_textof(&context->thisvalue);

    limitValue = ecc_context_argument(context, 1);
    if(limitValue.type != ECC_VALTYPE_UNDEFINED)
    {
        limit = ecc_value_tointeger(context, limitValue).data.integer;
        if(!limit)
            return ecc_value_object(ecc_array_createsized(0));
    }

    separatorValue = ecc_context_argument(context, 0);
    if(separatorValue.type == ECC_VALTYPE_UNDEFINED)
    {
        eccobject_t* subarr = ecc_array_createsized(1);
        subarr->hmapitemitems[0].hmapitemvalue = context->thisvalue;
        return ecc_value_object(subarr);
    }
    else if(separatorValue.type == ECC_VALTYPE_REGEXP)
        regexp = separatorValue.data.regexp;
    else
    {
        separatorValue = ecc_value_tostring(context, separatorValue);
        separator = ecc_value_textof(&separatorValue);
    }

    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);

    array = ecc_array_create();

    if(regexp)
    {
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        eccstrbox_t seek = text;

        for(;;)
        {
            eccrxstate_t state = { seek.bytes, seek.bytes + seek.length, capture, strindex, 0};
            int32_t numindex, count;

            if(size >= limit)
                break;

            if(seek.length && ecc_regexp_matchwithstate(regexp, &state))
            {
                if(capture[1] <= text.bytes)
                {
                    ecc_strbox_advance(&seek, 1);
                    continue;
                }

                element = ecc_strbuf_createwithbytes((int32_t)(capture[0] - text.bytes), text.bytes);
                ecc_object_addelement(array, size++, ecc_value_fromchars(element), 0);

                for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                {
                    if(size >= limit)
                        break;

                    if(capture[numindex * 2])
                    {
                        element = ecc_strbuf_createwithbytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]);
                        ecc_object_addelement(array, size++, ecc_value_fromchars(element), 0);
                    }
                    else
                        ecc_object_addelement(array, size++, ECCValConstUndefined, 0);
                }

                ecc_strbox_advance(&text, (int32_t)(capture[1] - text.bytes));
                seek = text;
            }
            else
            {
                element = ecc_strbuf_createwithbytes(text.length, text.bytes);
                ecc_object_addelement(array, size++, ecc_value_fromchars(element), 0);
                break;
            }
        }
        return ecc_value_object(array);
    }
    else if(!separator.length)
    {
        eccrune_t c;

        while(text.length)
        {
            if(size >= limit)
                break;

            c = ecc_strbox_character(text);
            if(c.codepoint < 0x010000)
                ecc_object_addelement(array, size++, ecc_value_buffer(text.bytes, c.units), 0);
            else
            {
                char buffer[7];

                /* simulate 16-bit surrogate */

                ecc_strbuf_writecodepoint(buffer, (((c.codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
                ecc_object_addelement(array, size++, ecc_value_buffer(buffer, 3), 0);

                ecc_strbuf_writecodepoint(buffer, (((c.codepoint - 0x010000) >> 0) & 0x3ff) + 0xdc00);
                ecc_object_addelement(array, size++, ecc_value_buffer(buffer, 3), 0);
            }
            ecc_strbox_advance(&text, c.units);
        }

        return ecc_value_object(array);
    }
    else
    {
        eccstrbox_t seek = text;
        int32_t length;

        while(seek.length >= separator.length)
        {
            if(size >= limit)
                break;

            if(!memcmp(seek.bytes, separator.bytes, separator.length))
            {
                length = (int32_t)(seek.bytes - text.bytes);
                element = ecc_strbuf_createsized(length);
                memcpy(element->bytes, text.bytes, length);
                ecc_object_addelement(array, size++, ecc_value_fromchars(element), 0);

                ecc_strbox_advance(&text, length + separator.length);
                seek = text;
                continue;
            }
            ecc_strbox_nextcharacter(&seek);
        }

        if(size < limit)
        {
            element = ecc_strbuf_createsized(text.length);
            memcpy(element->bytes, text.bytes, text.length);
            ecc_object_addelement(array, size++, ecc_value_fromchars(element), 0);
        }
    }

    return ecc_value_object(array);
}

static eccvalue_t ecc_objfnstring_substring(ecccontext_t* context)
{
    eccvalue_t from, to;
    eccstrbox_t start, end;
    const char* chars;
    int32_t length, head = 0, tail = 0;
    uint32_t headcp = 0;

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    from = ecc_context_argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED || (from.type == ECC_VALTYPE_BINARY && (isnan(from.data.valnumfloat) || from.data.valnumfloat == -ECC_CONST_INFINITY)))
        start = ecc_strbox_make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.valnumfloat == ECC_CONST_INFINITY)
        start = ecc_strbox_make(chars + length, 0);
    else
        start = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, from).data.integer, 0);

    to = ecc_context_argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && to.data.valnumfloat == ECC_CONST_INFINITY))
        end = ecc_strbox_make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && !isfinite(to.data.valnumfloat))
        end = ecc_strbox_make(chars, length);
    else
        end = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, to).data.integer, 0);

    if(start.bytes > end.bytes)
    {
        eccstrbox_t temp = start;
        start = end;
        end = temp;
    }

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        headcp = ecc_strbox_nextcharacter(&start).codepoint;

    length = (int32_t)(end.bytes - start.bytes);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        head = 3;

    if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        tail = 3;

    if(head + length + tail <= 0)
        return ecc_value_fromtext(&ECC_String_Empty);
    else
    {
        eccstrbuffer_t* result = ecc_strbuf_createsized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_strbuf_writecodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_strbuf_writecodepoint(result->bytes + head + length, (((ecc_strbox_character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
        }

        return ecc_value_fromchars(result);
    }
}

static eccvalue_t ecc_objfnstring_tolowercase(ecccontext_t* context)
{
    eccstrbuffer_t* chars;
    eccstrbox_t text;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    text = ecc_value_textof(&context->thisvalue);
    {
        char buffer[text.length * 2];
        char* end = ecc_strbox_tolower(text, buffer);
        chars = ecc_strbuf_createwithbytes((int32_t)(end - buffer), buffer);
    }

    return ecc_value_fromchars(chars);
}

static eccvalue_t ecc_objfnstring_touppercase(ecccontext_t* context)
{
    eccstrbuffer_t* chars;
    eccstrbox_t text;

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    text = ecc_value_textof(&context->thisvalue);
    {
        char buffer[text.length * 3];
        char* end = ecc_strbox_toupper(text, buffer);
        chars = ecc_strbuf_createwithbytes((int32_t)(end - buffer), buffer);
    }

    return ecc_value_fromchars(chars);
}

static eccvalue_t ecc_objfnstring_trim(ecccontext_t* context)
{
    eccstrbuffer_t* chars;
    eccstrbox_t text, last;
    eccrune_t c;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    text = ecc_value_textof(&context->thisvalue);
    while(text.length)
    {
        c = ecc_strbox_character(text);
        if(!ecc_strbox_isspace(c))
            break;

        ecc_strbox_advance(&text, c.units);
    }

    last = ecc_strbox_make(text.bytes + text.length, text.length);
    while(last.length)
    {
        c = ecc_strbox_prevcharacter(&last);
        if(!ecc_strbox_isspace(c))
            break;

        text.length = last.length;
    }

    chars = ecc_strbuf_createwithbytes(text.length, text.bytes);

    return ecc_value_fromchars(chars);
}

static eccvalue_t ecc_objfnstring_constructor(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        value = ecc_value_fromtext(value.check == 1 ? &ECC_String_Undefined : &ECC_String_Empty);
    else
        value = ecc_value_tostring(context, value);

    if(context->construct)
        return ecc_value_string(ecc_string_create(ecc_strbuf_createwithbytes(ecc_value_stringlength(&value), ecc_value_stringbytes(&value))));
    else
        return value;
}

static eccvalue_t ecc_objfnstring_fromcharcode(ecccontext_t* context)
{
    eccappbuf_t chars;
    int32_t index, count;

    count = ecc_context_argumentcount(context);

    ecc_strbuf_beginappend(&chars);

    for(index = 0; index < count; ++index)
        ecc_strbuf_appendcodepoint(&chars, (uint16_t)ecc_value_tointeger(context, ecc_context_argument(context, index)).data.integer);

    return ecc_strbuf_endappend(&chars);
}

void ecc_string_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_String, ecc_objfnstring_constructor, 1, &ECC_Prototype_String,
                                          ecc_value_string(ecc_string_create(ecc_strbuf_createsized(0))), &ECC_Type_String);

    ecc_function_addmethod(ECC_CtorFunc_String, "fromCharCode", ecc_objfnstring_fromcharcode, -1, h);

    ecc_function_addto(ECC_Prototype_String, "toString", ecc_objfnstring_tostring, 0, h);
    ecc_function_addto(ECC_Prototype_String, "valueOf", ecc_objfnstring_valueof, 0, h);
    ecc_function_addto(ECC_Prototype_String, "charAt", ecc_objfnstring_charat, 1, h);
    ecc_function_addto(ECC_Prototype_String, "charCodeAt", ecc_objfnstring_charcodeat, 1, h);
    ecc_function_addto(ECC_Prototype_String, "concat", ecc_objfnstring_concat, -1, h);
    ecc_function_addto(ECC_Prototype_String, "indexOf", ecc_objfnstring_indexof, -1, h);
    ecc_function_addto(ECC_Prototype_String, "lastIndexOf", ecc_objfnstring_lastindexof, -1, h);
    ecc_function_addto(ECC_Prototype_String, "localeCompare", ecc_objfnstring_localecompare, 1, h);
    ecc_function_addto(ECC_Prototype_String, "match", ecc_objfnstring_match, 1, h);
    ecc_function_addto(ECC_Prototype_String, "replace", ecc_objfnstring_replace, 2, h);
    ecc_function_addto(ECC_Prototype_String, "search", ecc_objfnstring_search, 1, h);
    ecc_function_addto(ECC_Prototype_String, "slice", ecc_objfnstring_slice, 2, h);
    ecc_function_addto(ECC_Prototype_String, "split", ecc_objfnstring_split, 2, h);
    ecc_function_addto(ECC_Prototype_String, "substring", ecc_objfnstring_substring, 2, h);
    ecc_function_addto(ECC_Prototype_String, "toLowerCase", ecc_objfnstring_tolowercase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "toLocaleLowerCase", ecc_objfnstring_tolowercase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "toUpperCase", ecc_objfnstring_touppercase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "toLocaleUpperCase", ecc_objfnstring_touppercase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "trim", ecc_objfnstring_trim, 0, h);
}

void ecc_string_teardown(void)
{
    ECC_Prototype_String = NULL;
    ECC_CtorFunc_String = NULL;
}

eccobjstring_t* ecc_string_create(eccstrbuffer_t* chars)
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;
    uint32_t length;
    eccobjstring_t* self;
    self = (eccobjstring_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccobjstring_t));
    ecc_mempool_addobject(&self->object);
    ecc_object_initialize(&self->object, ECC_Prototype_String);
    length = ecc_string_unitindex(chars->bytes, chars->length, chars->length);
    ecc_object_addmember(&self->object, ECC_ConstKey_length, ecc_value_fromint(length), r | h | s);
    self->sbuf = chars;
    if(length == (uint32_t)chars->length)
    {
        chars->flags |= ECC_CHARBUFFLAG_ASCIIONLY;
    }
    return self;
}

eccvalue_t ecc_string_valueatindex(eccobjstring_t* self, int32_t index)
{
    eccrune_t c;
    eccstrbox_t text;

    text = ecc_string_textatindex(self->sbuf->bytes, self->sbuf->length, index, 0);
    c = ecc_strbox_character(text);

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

            ecc_strbuf_writecodepoint(buffer, c.codepoint);
            return ecc_value_buffer(buffer, 3);
        }
    }
}

eccstrbox_t ecc_string_textatindex(const char* chars, int32_t length, int32_t position, int enableReverse)
{
    eccstrbox_t text = ecc_strbox_make(chars, length), prev;
    eccrune_t c;

    if(position >= 0)
    {
        while(position-- > 0)
        {
            prev = text;
            c = ecc_strbox_nextcharacter(&text);

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
            c = ecc_strbox_prevcharacter(&text);

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

int32_t ecc_string_unitindex(const char* chars, int32_t max, int32_t unit)
{
    eccstrbox_t text = ecc_strbox_make(chars, max);
    int32_t position = 0;
    eccrune_t c;

    while(unit > 0)
    {
        if(text.length)
        {
            ++position;
            c = ecc_strbox_nextcharacter(&text);
            unit -= c.units;

            if(c.codepoint > 0xffff) /* simulate 16-bit surrogate */
                ++position;
        }
    }

    return position;
}
