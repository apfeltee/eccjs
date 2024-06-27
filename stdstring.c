//
//  string.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static void eccstringtypefn_mark(eccobject_t *object);
static void eccstringtypefn_capture(eccobject_t *object);
static void eccstringtypefn_finalize(eccobject_t *object);
static eccvalue_t objstringfn_toString(eccstate_t *context);
static eccvalue_t objstringfn_valueOf(eccstate_t *context);
static eccvalue_t objstringfn_charAt(eccstate_t *context);
static eccvalue_t objstringfn_charCodeAt(eccstate_t *context);
static eccvalue_t objstringfn_concat(eccstate_t *context);
static eccvalue_t objstringfn_indexOf(eccstate_t *context);
static eccvalue_t objstringfn_lastIndexOf(eccstate_t *context);
static eccvalue_t objstringfn_localeCompare(eccstate_t *context);
static eccvalue_t objstringfn_match(eccstate_t *context);
static void eccstring_replace(eccappendbuffer_t *chars, ecctextstring_t replace, ecctextstring_t before, ecctextstring_t match, ecctextstring_t after, int count, const char *dcap[]);
static eccvalue_t objstringfn_replace(eccstate_t *context);
static eccvalue_t objstringfn_search(eccstate_t *context);
static eccvalue_t objstringfn_slice(eccstate_t *context);
static eccvalue_t objstringfn_split(eccstate_t *context);
static eccvalue_t objstringfn_substring(eccstate_t *context);
static eccvalue_t objstringfn_toLowerCase(eccstate_t *context);
static eccvalue_t objstringfn_toUpperCase(eccstate_t *context);
static eccvalue_t objstringfn_trim(eccstate_t *context);
static eccvalue_t objstringfn_constructor(eccstate_t *context);
static eccvalue_t objstringfn_fromCharCode(eccstate_t *context);


eccobject_t* ECC_Prototype_String = NULL;
eccobjfunction_t* ECC_CtorFunc_String = NULL;

const eccobjinterntype_t ECC_Type_String = {
    .text = &ECC_ConstString_StringType,
    .mark = eccstringtypefn_mark,
    .capture = eccstringtypefn_capture,
    .finalize = eccstringtypefn_finalize,
};


static void eccstringtypefn_mark(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    ecc_mempool_markvalue(ecc_value_chars(self->value));
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
    ecc_context_assertthistype(context, ECC_VALTYPE_STRING);

    return ecc_value_chars(context->thisvalue.data.string->value);
}

static eccvalue_t objstringfn_valueOf(eccstate_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_STRING);

    return ecc_value_chars(context->thisvalue.data.string->value);
}

static eccvalue_t objstringfn_charAt(eccstate_t* context)
{
    int32_t index, length;
    const char* chars;
    ecctextstring_t text;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, context->thisvalue);
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    index = ecc_value_tointeger(context, ecc_context_argument(context, 0)).data.integer;

    text = ecc_string_textatindex(chars, length, index, 0);
    if(!text.length)
        return ecc_value_text(&ECC_ConstString_Empty);
    else
    {
        ecctextchar_t c = ecc_textbuf_character(text);

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

            ecc_charbuf_writecodepoint(buffer, c.codepoint);
            return ecc_value_buffer(buffer, 3);
        }
    }
}

static eccvalue_t objstringfn_charCodeAt(eccstate_t* context)
{
    int32_t index, length;
    const char* chars;
    ecctextstring_t text;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, context->thisvalue);
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    index = ecc_value_tointeger(context, ecc_context_argument(context, 0)).data.integer;

    text = ecc_string_textatindex(chars, length, index, 0);
    if(!text.length)
        return ecc_value_binary(NAN);
    else
    {
        ecctextchar_t c = ecc_textbuf_character(text);

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

    ecc_context_assertthiscoercibleprimitive(context);

    count = ecc_context_argumentcount(context);

    ecc_charbuf_beginappend(&chars);
    ecc_charbuf_appendvalue(&chars, context, ecc_context_this(context));
    for(index = 0; index < count; ++index)
        ecc_charbuf_appendvalue(&chars, context, ecc_context_argument(context, index));

    return ecc_charbuf_endappend(&chars);
}

static eccvalue_t objstringfn_indexOf(eccstate_t* context)
{
    ecctextstring_t text;
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
        ecc_textbuf_nextcharacter(&text);
        ++index;
    }

    while(text.length)
    {
        if(!memcmp(text.bytes, searchChars, searchLength))
            return ecc_value_integer(index);

        ++index;
        if(ecc_textbuf_nextcharacter(&text).codepoint > 0xffff)
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

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    search = ecc_value_tostring(context, ecc_context_argument(context, 0));
    searchChars = ecc_value_stringbytes(&search);
    searchLength = ecc_value_stringlength(&search);

    start = ecc_value_tobinary(context, ecc_context_argument(context, 1));
    index = ecc_string_unitindex(chars, length, length);
    if(!isnan(start.data.binary) && start.data.binary < index)
        index = start.data.binary < 0 ? 0 : start.data.binary;

    text = ecc_string_textatindex(chars, length, index, 0);
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
        if(ecc_textbuf_prevcharacter(&text).codepoint > 0xffff)
            --index;
    }

    return ecc_value_integer(-1);
}

static eccvalue_t objstringfn_localeCompare(eccstate_t* context)
{
    eccvalue_t that;
    ecctextstring_t a, b;

    ecc_context_assertthiscoercibleprimitive(context);

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    a = ecc_value_textof(&context->thisvalue);

    that = ecc_value_tostring(context, ecc_context_argument(context, 0));
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

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        regexp = ecc_regexp_createwith(context, value, ECCValConstUndefined);

    lastIndex = regexp->global ? ecc_value_integer(0) : ecc_value_tointeger(context, ecc_object_getmember(context, &regexp->object, ECC_ConstKey_lastIndex));

    ecc_object_putmember(context, &regexp->object, ECC_ConstKey_lastIndex, ecc_value_integer(0));

    if(lastIndex.data.integer >= 0)
    {
        const char* bytes = ecc_value_stringbytes(&context->thisvalue);
        int32_t length = ecc_value_stringlength(&context->thisvalue);
        ecctextstring_t text = ecc_string_textatindex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        eccobject_t* array = ecc_array_create();
        eccappendbuffer_t chars;
        uint32_t size = 0;

        do
        {
            eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, strindex, 0};

            if(ecc_regexp_matchwithstate(regexp, &state))
            {
                ecc_charbuf_beginappend(&chars);
                ecc_charbuf_append(&chars, "%.*s", capture[1] - capture[0], capture[0]);
                ecc_object_addelement(array, size++, ecc_charbuf_endappend(&chars), 0);

                if(!regexp->global)
                {
                    int32_t numindex, count;

                    for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                        {
                            ecc_charbuf_beginappend(&chars);
                            ecc_charbuf_append(&chars, "%.*s", capture[numindex * 2 + 1] - capture[numindex * 2], capture[numindex * 2]);
                            ecc_object_addelement(array, size++, ecc_charbuf_endappend(&chars), 0);
                        }
                        else
                            ecc_object_addelement(array, size++, ECCValConstUndefined, 0);
                    }
                    break;
                }

                if(capture[1] - text.bytes > 0)
                    ecc_textbuf_advance(&text, (int32_t)(capture[1] - text.bytes));
                else
                    ecc_textbuf_nextcharacter(&text);
            }
            else
                break;
        } while(text.length);

        if(size)
        {
            ecc_object_addmember(array, ECC_ConstKey_input, context->thisvalue, 0);
            ecc_object_addmember(array, ECC_ConstKey_index, ecc_value_integer(ecc_string_unitindex(bytes, length, (int32_t)(capture[0] - bytes))), 0);

            if(regexp->global)
                ecc_object_putmember(context, &regexp->object, ECC_ConstKey_lastIndex,
                                      ecc_value_integer(ecc_string_unitindex(bytes, length, (int32_t)(text.bytes - bytes))));

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
        c = ecc_textbuf_character(replace);
        if(c.codepoint == '$')
        {
            int index;
            ecc_textbuf_advance(&replace, 1);
            switch(ecc_textbuf_character(replace).codepoint)
            {
                case '$':
                    ecc_charbuf_append(chars, "$");
                    break;
                case '&':
                    ecc_charbuf_append(chars, "%.*s", match.length, match.bytes);
                    break;
                case '`':
                    ecc_charbuf_append(chars, "%.*s", before.length, before.bytes);
                    break;
                case '\'':
                    ecc_charbuf_append(chars, "%.*s", after.length, after.bytes);
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
                                ecc_textbuf_advance(&replace, 1);
                            }
                            if(dcap && index && index < count)
                            {
                                if(dcap[index * 2])
                                {
                                    ecc_charbuf_append(chars, "%.*s", dcap[index * 2 + 1] - dcap[index * 2], dcap[index * 2]);
                                }
                                else
                                {
                                    ecc_charbuf_append(chars, "");
                                }
                                break;
                            }
                        }
                    }
                    /* fallthrough  */
                default:
                    {
                        ecc_charbuf_append(chars, "$");
                    }
                    continue;
            }
        }
        else
        {
            ecc_charbuf_append(chars, "%.*s", c.units, replace.bytes);
        }
        ecc_textbuf_advance(&replace, c.units);
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
    ecc_context_assertthiscoercibleprimitive(context);
    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    bytes = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);
    text = ecc_textbuf_make(bytes, length);
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
        ecctextstring_t seek = text;

        ecc_charbuf_beginappend(&chars);
        do
        {
            eccrxstate_t state = { seek.bytes, text.bytes + text.length, capture, strindex, 0 };

            if(ecc_regexp_matchwithstate(regexp, &state))
            {
                ecc_charbuf_append(&chars, "%.*s", capture[0] - text.bytes, text.bytes);

                if(replace.type == ECC_VALTYPE_FUNCTION)
                {
                    eccobject_t* arguments = ecc_array_createsized(regexp->count + 2);
                    int32_t numindex, count;
                    eccvalue_t result;

                    for(numindex = 0, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                            arguments->element[numindex].value
                            = ecc_value_chars(ecc_charbuf_createwithbytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]));
                        else
                            arguments->element[numindex].value = ECCValConstUndefined;
                    }
                    arguments->element[regexp->count].value = ecc_value_integer(ecc_string_unitindex(bytes, length, (int32_t)(capture[0] - bytes)));
                    arguments->element[regexp->count + 1].value = context->thisvalue;

                    result = ecc_value_tostring(context, ecc_oper_callfunctionarguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
                    ecc_charbuf_append(&chars, "%.*s", ecc_value_stringlength(&result), ecc_value_stringbytes(&result));
                }
                else
                    eccstring_replace(&chars, ecc_textbuf_make(ecc_value_stringbytes(&replace), ecc_value_stringlength(&replace)),
                                ecc_textbuf_make(bytes, (int32_t)(capture[0] - bytes)), ecc_textbuf_make(capture[0], (int32_t)(capture[1] - capture[0])),
                                ecc_textbuf_make(capture[1], (int32_t)((bytes + length) - capture[1])), regexp->count, capture);

                ecc_textbuf_advance(&text, (int32_t)(state.capture[1] - text.bytes));

                seek = text;
                if(text.bytes == state.capture[1])
                    ecc_textbuf_nextcharacter(&seek);
            }
            else
                break;
        } while(text.length && regexp->global);

        ecc_charbuf_append(&chars, "%.*s", text.length, text.bytes);

        return ecc_charbuf_endappend(&chars);
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
            ecc_textbuf_nextcharacter(&text);
        }

        ecc_charbuf_beginappend(&chars);
        ecc_charbuf_append(&chars, "%.*s", text.bytes - bytes, bytes);

        if(replace.type == ECC_VALTYPE_FUNCTION)
        {
            eccobject_t* arguments = ecc_array_createsized(1 + 2);
            eccvalue_t result;

            arguments->element[0].value = ecc_value_chars(ecc_charbuf_createwithbytes(text.length, text.bytes));
            arguments->element[1].value = ecc_value_integer(ecc_string_unitindex(bytes, length, (int32_t)(text.bytes - bytes)));
            arguments->element[2].value = context->thisvalue;

            result = ecc_value_tostring(context, ecc_oper_callfunctionarguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
            ecc_charbuf_append(&chars, "%.*s", ecc_value_stringlength(&result), ecc_value_stringbytes(&result));
        }
        else
            eccstring_replace(&chars, ecc_textbuf_make(ecc_value_stringbytes(&replace), ecc_value_stringlength(&replace)),
                        ecc_textbuf_make(text.bytes, (int32_t)(text.bytes - bytes)), ecc_textbuf_make(text.bytes, text.length),
                        ecc_textbuf_make(text.bytes, (int32_t)(length - (text.bytes - bytes))), 0, NULL);

        ecc_charbuf_append(&chars, "%.*s", length - (text.bytes - bytes), text.bytes + text.length);

        return ecc_charbuf_endappend(&chars);
    }
}

static eccvalue_t objstringfn_search(eccstate_t* context)
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
        ecctextstring_t text = ecc_string_textatindex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* index[regexp->count * 2];

        eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, index, 0 };

        if(ecc_regexp_matchwithstate(regexp, &state))
            return ecc_value_integer(ecc_string_unitindex(bytes, length, (int32_t)(capture[0] - bytes)));
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
        context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    from = ecc_context_argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED)
        start = ecc_textbuf_make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.binary == INFINITY)
        start = ecc_textbuf_make(chars + length, 0);
    else
        start = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, from).data.integer, 1);

    to = ecc_context_argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && (isnan(to.data.binary) || to.data.binary == INFINITY)))
        end = ecc_textbuf_make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && to.data.binary == -INFINITY)
        end = ecc_textbuf_make(chars, length);
    else
        end = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, to).data.integer, 1);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        headcp = ecc_textbuf_nextcharacter(&start).codepoint;

    length = (int32_t)(end.bytes - start.bytes);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        head = 3;

    if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        tail = 3;

    if(head + length + tail <= 0)
        return ecc_value_text(&ECC_ConstString_Empty);
    else
    {
        ecccharbuffer_t* result = ecc_charbuf_createsized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_charbuf_writecodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_charbuf_writecodepoint(result->bytes + head + length, (((ecc_textbuf_character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
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

    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);

    array = ecc_array_create();

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

            if(seek.length && ecc_regexp_matchwithstate(regexp, &state))
            {
                if(capture[1] <= text.bytes)
                {
                    ecc_textbuf_advance(&seek, 1);
                    continue;
                }

                element = ecc_charbuf_createwithbytes((int32_t)(capture[0] - text.bytes), text.bytes);
                ecc_object_addelement(array, size++, ecc_value_chars(element), 0);

                for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                {
                    if(size >= limit)
                        break;

                    if(capture[numindex * 2])
                    {
                        element = ecc_charbuf_createwithbytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]);
                        ecc_object_addelement(array, size++, ecc_value_chars(element), 0);
                    }
                    else
                        ecc_object_addelement(array, size++, ECCValConstUndefined, 0);
                }

                ecc_textbuf_advance(&text, (int32_t)(capture[1] - text.bytes));
                seek = text;
            }
            else
            {
                element = ecc_charbuf_createwithbytes(text.length, text.bytes);
                ecc_object_addelement(array, size++, ecc_value_chars(element), 0);
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

            c = ecc_textbuf_character(text);
            if(c.codepoint < 0x010000)
                ecc_object_addelement(array, size++, ecc_value_buffer(text.bytes, c.units), 0);
            else
            {
                char buffer[7];

                /* simulate 16-bit surrogate */

                ecc_charbuf_writecodepoint(buffer, (((c.codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
                ecc_object_addelement(array, size++, ecc_value_buffer(buffer, 3), 0);

                ecc_charbuf_writecodepoint(buffer, (((c.codepoint - 0x010000) >> 0) & 0x3ff) + 0xdc00);
                ecc_object_addelement(array, size++, ecc_value_buffer(buffer, 3), 0);
            }
            ecc_textbuf_advance(&text, c.units);
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
                element = ecc_charbuf_createsized(length);
                memcpy(element->bytes, text.bytes, length);
                ecc_object_addelement(array, size++, ecc_value_chars(element), 0);

                ecc_textbuf_advance(&text, length + separator.length);
                seek = text;
                continue;
            }
            ecc_textbuf_nextcharacter(&seek);
        }

        if(size < limit)
        {
            element = ecc_charbuf_createsized(text.length);
            memcpy(element->bytes, text.bytes, text.length);
            ecc_object_addelement(array, size++, ecc_value_chars(element), 0);
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

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    chars = ecc_value_stringbytes(&context->thisvalue);
    length = ecc_value_stringlength(&context->thisvalue);

    from = ecc_context_argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED || (from.type == ECC_VALTYPE_BINARY && (isnan(from.data.binary) || from.data.binary == -INFINITY)))
        start = ecc_textbuf_make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.binary == INFINITY)
        start = ecc_textbuf_make(chars + length, 0);
    else
        start = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, from).data.integer, 0);

    to = ecc_context_argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && to.data.binary == INFINITY))
        end = ecc_textbuf_make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && !isfinite(to.data.binary))
        end = ecc_textbuf_make(chars, length);
    else
        end = ecc_string_textatindex(chars, length, ecc_value_tointeger(context, to).data.integer, 0);

    if(start.bytes > end.bytes)
    {
        ecctextstring_t temp = start;
        start = end;
        end = temp;
    }

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        headcp = ecc_textbuf_nextcharacter(&start).codepoint;

    length = (int32_t)(end.bytes - start.bytes);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        head = 3;

    if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        tail = 3;

    if(head + length + tail <= 0)
        return ecc_value_text(&ECC_ConstString_Empty);
    else
    {
        ecccharbuffer_t* result = ecc_charbuf_createsized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_charbuf_writecodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            ecc_charbuf_writecodepoint(result->bytes + head + length, (((ecc_textbuf_character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
        }

        return ecc_value_chars(result);
    }
}

static eccvalue_t objstringfn_toLowerCase(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    text = ecc_value_textof(&context->thisvalue);
    {
        char buffer[text.length * 2];
        char* end = ecc_textbuf_tolower(text, buffer);
        chars = ecc_charbuf_createwithbytes((int32_t)(end - buffer), buffer);
    }

    return ecc_value_chars(chars);
}

static eccvalue_t objstringfn_toUpperCase(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text;

    context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));
    text = ecc_value_textof(&context->thisvalue);
    {
        char buffer[text.length * 3];
        char* end = ecc_textbuf_toupper(text, buffer);
        chars = ecc_charbuf_createwithbytes((int32_t)(end - buffer), buffer);
    }

    return ecc_value_chars(chars);
}

static eccvalue_t objstringfn_trim(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text, last;
    ecctextchar_t c;

    if(!ecc_value_isstring(context->thisvalue))
        context->thisvalue = ecc_value_tostring(context, ecc_context_this(context));

    text = ecc_value_textof(&context->thisvalue);
    while(text.length)
    {
        c = ecc_textbuf_character(text);
        if(!ecc_textbuf_isspace(c))
            break;

        ecc_textbuf_advance(&text, c.units);
    }

    last = ecc_textbuf_make(text.bytes + text.length, text.length);
    while(last.length)
    {
        c = ecc_textbuf_prevcharacter(&last);
        if(!ecc_textbuf_isspace(c))
            break;

        text.length = last.length;
    }

    chars = ecc_charbuf_createwithbytes(text.length, text.bytes);

    return ecc_value_chars(chars);
}

static eccvalue_t objstringfn_constructor(eccstate_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        value = ecc_value_text(value.check == 1 ? &ECC_ConstString_Undefined : &ECC_ConstString_Empty);
    else
        value = ecc_value_tostring(context, value);

    if(context->construct)
        return ecc_value_string(ecc_string_create(ecc_charbuf_createwithbytes(ecc_value_stringlength(&value), ecc_value_stringbytes(&value))));
    else
        return value;
}

static eccvalue_t objstringfn_fromCharCode(eccstate_t* context)
{
    eccappendbuffer_t chars;
    int32_t index, count;

    count = ecc_context_argumentcount(context);

    ecc_charbuf_beginappend(&chars);

    for(index = 0; index < count; ++index)
        ecc_charbuf_appendcodepoint(&chars, (uint16_t)ecc_value_tointeger(context, ecc_context_argument(context, index)).data.integer);

    return ecc_charbuf_endappend(&chars);
}

void ecc_string_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_String, objstringfn_constructor, 1, &ECC_Prototype_String,
                                          ecc_value_string(ecc_string_create(ecc_charbuf_createsized(0))), &ECC_Type_String);

    ecc_function_addmethod(ECC_CtorFunc_String, "fromCharCode", objstringfn_fromCharCode, -1, h);

    ecc_function_addto(ECC_Prototype_String, "toString", objstringfn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_String, "valueOf", objstringfn_valueOf, 0, h);
    ecc_function_addto(ECC_Prototype_String, "charAt", objstringfn_charAt, 1, h);
    ecc_function_addto(ECC_Prototype_String, "charCodeAt", objstringfn_charCodeAt, 1, h);
    ecc_function_addto(ECC_Prototype_String, "concat", objstringfn_concat, -1, h);
    ecc_function_addto(ECC_Prototype_String, "indexOf", objstringfn_indexOf, -1, h);
    ecc_function_addto(ECC_Prototype_String, "lastIndexOf", objstringfn_lastIndexOf, -1, h);
    ecc_function_addto(ECC_Prototype_String, "localeCompare", objstringfn_localeCompare, 1, h);
    ecc_function_addto(ECC_Prototype_String, "match", objstringfn_match, 1, h);
    ecc_function_addto(ECC_Prototype_String, "replace", objstringfn_replace, 2, h);
    ecc_function_addto(ECC_Prototype_String, "search", objstringfn_search, 1, h);
    ecc_function_addto(ECC_Prototype_String, "slice", objstringfn_slice, 2, h);
    ecc_function_addto(ECC_Prototype_String, "split", objstringfn_split, 2, h);
    ecc_function_addto(ECC_Prototype_String, "substring", objstringfn_substring, 2, h);
    ecc_function_addto(ECC_Prototype_String, "toLowerCase", objstringfn_toLowerCase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "toLocaleLowerCase", objstringfn_toLowerCase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "toUpperCase", objstringfn_toUpperCase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "toLocaleUpperCase", objstringfn_toUpperCase, 0, h);
    ecc_function_addto(ECC_Prototype_String, "trim", objstringfn_trim, 0, h);
}

void ecc_string_teardown(void)
{
    ECC_Prototype_String = NULL;
    ECC_CtorFunc_String = NULL;
}

eccobjstring_t* ecc_string_create(ecccharbuffer_t* chars)
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
    ecc_object_addmember(&self->object, ECC_ConstKey_length, ecc_value_integer(length), r | h | s);
    self->value = chars;
    if(length == (uint32_t)chars->length)
    {
        chars->flags |= ECC_CHARBUFFLAG_ASCIIONLY;
    }
    return self;
}

eccvalue_t ecc_string_valueatindex(eccobjstring_t* self, int32_t index)
{
    ecctextchar_t c;
    ecctextstring_t text;

    text = ecc_string_textatindex(self->value->bytes, self->value->length, index, 0);
    c = ecc_textbuf_character(text);

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

            ecc_charbuf_writecodepoint(buffer, c.codepoint);
            return ecc_value_buffer(buffer, 3);
        }
    }
}

ecctextstring_t ecc_string_textatindex(const char* chars, int32_t length, int32_t position, int enableReverse)
{
    ecctextstring_t text = ecc_textbuf_make(chars, length), prev;
    ecctextchar_t c;

    if(position >= 0)
    {
        while(position-- > 0)
        {
            prev = text;
            c = ecc_textbuf_nextcharacter(&text);

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
            c = ecc_textbuf_prevcharacter(&text);

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
    ecctextstring_t text = ecc_textbuf_make(chars, max);
    int32_t position = 0;
    ecctextchar_t c;

    while(unit > 0)
    {
        if(text.length)
        {
            ++position;
            c = ecc_textbuf_nextcharacter(&text);
            unit -= c.units;

            if(c.codepoint > 0xffff) /* simulate 16-bit surrogate */
                ++position;
        }
    }

    return position;
}
