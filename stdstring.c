//
//  string.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static void mark(eccobject_t* object);
static void capture(eccobject_t* object);
static void finalize(eccobject_t* object);

eccobject_t* io_libecc_string_prototype = NULL;
eccobjscriptfunction_t* io_libecc_string_constructor = NULL;

const eccobjinterntype_t io_libecc_string_type = {
    .text = &ECC_ConstString_StringType,
    .mark = mark,
    .capture = capture,
    .finalize = finalize,
};

static void setup(void);
static void teardown(void);
static eccobjstring_t* create(ecccharbuffer_t*);
static eccvalue_t valueAtIndex(eccobjstring_t*, int32_t index);
static ecctextstring_t textAtIndex(const char* chars, int32_t length, int32_t index, int enableReverse);
static int32_t unitIndex(const char* chars, int32_t max, int32_t unit);
const struct type_io_libecc_String io_libecc_String = {
    setup, teardown, create, valueAtIndex, textAtIndex, unitIndex,
    {}
};

static void mark(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    io_libecc_Pool.markValue(ECCNSValue.chars(self->value));
}

static void capture(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    ++self->value->referenceCount;
}

static void finalize(eccobject_t* object)
{
    eccobjstring_t* self = (eccobjstring_t*)object;

    --self->value->referenceCount;
}

// MARK: - Static Members

static eccvalue_t toString(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_STRING);

    return ECCNSValue.chars(context->this.data.string->value);
}

static eccvalue_t valueOf(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_STRING);

    return ECCNSValue.chars(context->this.data.string->value);
}

static eccvalue_t charAt(eccstate_t* context)
{
    int32_t index, length;
    const char* chars;
    ecctextstring_t text;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, context->this);
    chars = ECCNSValue.stringBytes(&context->this);
    length = ECCNSValue.stringLength(&context->this);
    index = ECCNSValue.toInteger(context, ECCNSContext.argument(context, 0)).data.integer;

    text = textAtIndex(chars, length, index, 0);
    if(!text.length)
        return ECCNSValue.text(&ECC_ConstString_Empty);
    else
    {
        ecctextchar_t c = ECCNSText.character(text);

        if(c.codepoint < 0x010000)
            return ECCNSValue.buffer(text.bytes, c.units);
        else
        {
            char buffer[7];

            /* simulate 16-bit surrogate */

            c.codepoint -= 0x010000;
            if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
                c.codepoint = ((c.codepoint >> 0) & 0x3ff) + 0xdc00;
            else
                c.codepoint = ((c.codepoint >> 10) & 0x3ff) + 0xd800;

            io_libecc_Chars.writeCodepoint(buffer, c.codepoint);
            return ECCNSValue.buffer(buffer, 3);
        }
    }
}

static eccvalue_t charCodeAt(eccstate_t* context)
{
    int32_t index, length;
    const char* chars;
    ecctextstring_t text;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, context->this);
    chars = ECCNSValue.stringBytes(&context->this);
    length = ECCNSValue.stringLength(&context->this);
    index = ECCNSValue.toInteger(context, ECCNSContext.argument(context, 0)).data.integer;

    text = textAtIndex(chars, length, index, 0);
    if(!text.length)
        return ECCNSValue.binary(NAN);
    else
    {
        ecctextchar_t c = ECCNSText.character(text);

        if(c.codepoint < 0x010000)
            return ECCNSValue.binary(c.codepoint);
        else
        {
            /* simulate 16-bit surrogate */

            c.codepoint -= 0x010000;
            if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
                return ECCNSValue.binary(((c.codepoint >> 0) & 0x3ff) + 0xdc00);
            else
                return ECCNSValue.binary(((c.codepoint >> 10) & 0x3ff) + 0xd800);
        }
    }
}

static eccvalue_t concat(eccstate_t* context)
{
    eccappendbuffer_t chars;
    int32_t index, count;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    count = ECCNSContext.argumentCount(context);

    io_libecc_Chars.beginAppend(&chars);
    io_libecc_Chars.appendValue(&chars, context, ECCNSContext.this(context));
    for(index = 0; index < count; ++index)
        io_libecc_Chars.appendValue(&chars, context, ECCNSContext.argument(context, index));

    return io_libecc_Chars.endAppend(&chars);
}

static eccvalue_t indexOf(eccstate_t* context)
{
    ecctextstring_t text;
    eccvalue_t search, start;
    int32_t index, length, searchLength;
    const char *chars, *searchChars;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));
    chars = ECCNSValue.stringBytes(&context->this);
    length = ECCNSValue.stringLength(&context->this);

    search = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    searchChars = ECCNSValue.stringBytes(&search);
    searchLength = ECCNSValue.stringLength(&search);
    start = ECCNSValue.toInteger(context, ECCNSContext.argument(context, 1));
    index = start.data.integer < 0 ? length + start.data.integer : start.data.integer;
    if(index < 0)
        index = 0;

    text = textAtIndex(chars, length, index, 0);
    if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
    {
        ECCNSText.nextCharacter(&text);
        ++index;
    }

    while(text.length)
    {
        if(!memcmp(text.bytes, searchChars, searchLength))
            return ECCNSValue.integer(index);

        ++index;
        if(ECCNSText.nextCharacter(&text).codepoint > 0xffff)
            ++index;
    }

    return ECCNSValue.integer(-1);
}

static eccvalue_t lastIndexOf(eccstate_t* context)
{
    ecctextstring_t text;
    eccvalue_t search, start;
    int32_t index, length, searchLength;
    const char *chars, *searchChars;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));
    chars = ECCNSValue.stringBytes(&context->this);
    length = ECCNSValue.stringLength(&context->this);

    search = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    searchChars = ECCNSValue.stringBytes(&search);
    searchLength = ECCNSValue.stringLength(&search);

    start = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 1));
    index = unitIndex(chars, length, length);
    if(!isnan(start.data.binary) && start.data.binary < index)
        index = start.data.binary < 0 ? 0 : start.data.binary;

    text = textAtIndex(chars, length, index, 0);
    if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
        --index;

    text.length = (int32_t)(text.bytes - chars);

    for(;;)
    {
        if(length - (text.bytes - chars) >= searchLength && !memcmp(text.bytes, searchChars, searchLength))
            return ECCNSValue.integer(index);

        if(!text.length)
            break;

        --index;
        if(ECCNSText.prevCharacter(&text).codepoint > 0xffff)
            --index;
    }

    return ECCNSValue.integer(-1);
}

static eccvalue_t localeCompare(eccstate_t* context)
{
    eccvalue_t that;
    ecctextstring_t a, b;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));
    a = ECCNSValue.textOf(&context->this);

    that = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    b = ECCNSValue.textOf(&that);

    if(a.length < b.length && !memcmp(a.bytes, b.bytes, a.length))
        return ECCNSValue.integer(-1);

    if(a.length > b.length && !memcmp(a.bytes, b.bytes, b.length))
        return ECCNSValue.integer(1);

    return ECCNSValue.integer(memcmp(a.bytes, b.bytes, a.length));
}

static eccvalue_t match(eccstate_t* context)
{
    eccobjregexp_t* regexp;
    eccvalue_t value, lastIndex;

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        regexp = io_libecc_RegExp.createWith(context, value, ECCValConstUndefined);

    lastIndex = regexp->global ? ECCNSValue.integer(0) : ECCNSValue.toInteger(context, ECCNSObject.getMember(context, &regexp->object, io_libecc_key_lastIndex));

    ECCNSObject.putMember(context, &regexp->object, io_libecc_key_lastIndex, ECCNSValue.integer(0));

    if(lastIndex.data.integer >= 0)
    {
        const char* bytes = ECCNSValue.stringBytes(&context->this);
        int32_t length = ECCNSValue.stringLength(&context->this);
        ecctextstring_t text = textAtIndex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        eccobject_t* array = io_libecc_Array.create();
        eccappendbuffer_t chars;
        uint32_t size = 0;

        do
        {
            eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, strindex, 0};

            if(io_libecc_RegExp.matchWithState(regexp, &state))
            {
                io_libecc_Chars.beginAppend(&chars);
                io_libecc_Chars.append(&chars, "%.*s", capture[1] - capture[0], capture[0]);
                ECCNSObject.addElement(array, size++, io_libecc_Chars.endAppend(&chars), 0);

                if(!regexp->global)
                {
                    int32_t numindex, count;

                    for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                        {
                            io_libecc_Chars.beginAppend(&chars);
                            io_libecc_Chars.append(&chars, "%.*s", capture[numindex * 2 + 1] - capture[numindex * 2], capture[numindex * 2]);
                            ECCNSObject.addElement(array, size++, io_libecc_Chars.endAppend(&chars), 0);
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
            ECCNSObject.addMember(array, io_libecc_key_input, context->this, 0);
            ECCNSObject.addMember(array, io_libecc_key_index, ECCNSValue.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(capture[0] - bytes))), 0);

            if(regexp->global)
                ECCNSObject.putMember(context, &regexp->object, io_libecc_key_lastIndex,
                                      ECCNSValue.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(text.bytes - bytes))));

            return ECCNSValue.object(array);
        }
    }
    return ECCValConstNull;
}

static void
replaceText(eccappendbuffer_t* chars, ecctextstring_t replace, ecctextstring_t before, ecctextstring_t match, ecctextstring_t after, int count, const char* capture[])
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
                    io_libecc_Chars.append(chars, "$");
                    break;

                case '&':
                    io_libecc_Chars.append(chars, "%.*s", match.length, match.bytes);
                    break;

                case '`':
                    io_libecc_Chars.append(chars, "%.*s", before.length, before.bytes);
                    break;

                case '\'':
                    io_libecc_Chars.append(chars, "%.*s", after.length, after.bytes);
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
                            if(capture && index && index < count)
                            {
                                if(capture[index * 2])
                                {
                                    io_libecc_Chars.append(chars, "%.*s", capture[index * 2 + 1] - capture[index * 2], capture[index * 2]);
                                }
                                else
                                {
                                    io_libecc_Chars.append(chars, "");
                                }
                                break;
                            }
                        }
                    }
                    /* fallthrough  */
                default:
                    {
                        io_libecc_Chars.append(chars, "$");
                    }
                    continue;
            }
        }
        else
        {
            io_libecc_Chars.append(chars, "%.*s", c.units, replace.bytes);
        }
        ECCNSText.advance(&replace, c.units);
    }
}

static eccvalue_t replace(eccstate_t* context)
{
    eccobjregexp_t* regexp = NULL;
    eccappendbuffer_t chars;
    eccvalue_t value, replace;
    ecctextstring_t text;
    const char *bytes, *searchBytes;
    int32_t length, searchLength;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));
    bytes = ECCNSValue.stringBytes(&context->this);
    length = ECCNSValue.stringLength(&context->this);
    text = ECCNSText.make(bytes, length);

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        value = ECCNSValue.toString(context, value);

    replace = ECCNSContext.argument(context, 1);
    if(replace.type != ECC_VALTYPE_FUNCTION)
        replace = ECCNSValue.toString(context, replace);

    if(regexp)
    {
        const char* capture[regexp->count * 2];
        const char* strindex[regexp->count * 2];
        ecctextstring_t seek = text;

        io_libecc_Chars.beginAppend(&chars);
        do
        {
            eccrxstate_t state = { seek.bytes, text.bytes + text.length, capture, strindex, 0 };

            if(io_libecc_RegExp.matchWithState(regexp, &state))
            {
                io_libecc_Chars.append(&chars, "%.*s", capture[0] - text.bytes, text.bytes);

                if(replace.type == ECC_VALTYPE_FUNCTION)
                {
                    eccobject_t* arguments = io_libecc_Array.createSized(regexp->count + 2);
                    int32_t numindex, count;
                    eccvalue_t result;

                    for(numindex = 0, count = regexp->count; numindex < count; ++numindex)
                    {
                        if(capture[numindex * 2])
                            arguments->element[numindex].value
                            = ECCNSValue.chars(io_libecc_Chars.createWithBytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]));
                        else
                            arguments->element[numindex].value = ECCValConstUndefined;
                    }
                    arguments->element[regexp->count].value = ECCNSValue.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(capture[0] - bytes)));
                    arguments->element[regexp->count + 1].value = context->this;

                    result = ECCNSValue.toString(context, io_libecc_Op.callFunctionArguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
                    io_libecc_Chars.append(&chars, "%.*s", ECCNSValue.stringLength(&result), ECCNSValue.stringBytes(&result));
                }
                else
                    replaceText(&chars, ECCNSText.make(ECCNSValue.stringBytes(&replace), ECCNSValue.stringLength(&replace)),
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

        io_libecc_Chars.append(&chars, "%.*s", text.length, text.bytes);

        return io_libecc_Chars.endAppend(&chars);
    }
    else
    {
        searchBytes = ECCNSValue.stringBytes(&value);
        searchLength = ECCNSValue.stringLength(&value);

        for(;;)
        {
            if(!text.length)
                return context->this;

            if(!memcmp(text.bytes, searchBytes, searchLength))
            {
                text.length = searchLength;
                break;
            }
            ECCNSText.nextCharacter(&text);
        }

        io_libecc_Chars.beginAppend(&chars);
        io_libecc_Chars.append(&chars, "%.*s", text.bytes - bytes, bytes);

        if(replace.type == ECC_VALTYPE_FUNCTION)
        {
            eccobject_t* arguments = io_libecc_Array.createSized(1 + 2);
            eccvalue_t result;

            arguments->element[0].value = ECCNSValue.chars(io_libecc_Chars.createWithBytes(text.length, text.bytes));
            arguments->element[1].value = ECCNSValue.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(text.bytes - bytes)));
            arguments->element[2].value = context->this;

            result = ECCNSValue.toString(context, io_libecc_Op.callFunctionArguments(context, 0, replace.data.function, ECCValConstUndefined, arguments));
            io_libecc_Chars.append(&chars, "%.*s", ECCNSValue.stringLength(&result), ECCNSValue.stringBytes(&result));
        }
        else
            replaceText(&chars, ECCNSText.make(ECCNSValue.stringBytes(&replace), ECCNSValue.stringLength(&replace)),
                        ECCNSText.make(text.bytes, (int32_t)(text.bytes - bytes)), ECCNSText.make(text.bytes, text.length),
                        ECCNSText.make(text.bytes, (int32_t)(length - (text.bytes - bytes))), 0, NULL);

        io_libecc_Chars.append(&chars, "%.*s", length - (text.bytes - bytes), text.bytes + text.length);

        return io_libecc_Chars.endAppend(&chars);
    }
}

static eccvalue_t search(eccstate_t* context)
{
    eccobjregexp_t* regexp;
    eccvalue_t value;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_REGEXP)
        regexp = value.data.regexp;
    else
        regexp = io_libecc_RegExp.createWith(context, value, ECCValConstUndefined);

    {
        const char* bytes = ECCNSValue.stringBytes(&context->this);
        int32_t length = ECCNSValue.stringLength(&context->this);
        ecctextstring_t text = textAtIndex(bytes, length, 0, 0);
        const char* capture[regexp->count * 2];
        const char* index[regexp->count * 2];

        eccrxstate_t state = { text.bytes, text.bytes + text.length, capture, index, 0 };

        if(io_libecc_RegExp.matchWithState(regexp, &state))
            return ECCNSValue.integer(unitIndex(bytes, length, (int32_t)(capture[0] - bytes)));
    }
    return ECCNSValue.integer(-1);
}

static eccvalue_t slice(eccstate_t* context)
{
    eccvalue_t from, to;
    ecctextstring_t start, end;
    const char* chars;
    int32_t length;
    uint16_t head = 0, tail = 0;
    uint32_t headcp = 0;

    if(!ECCNSValue.isString(context->this))
        context->this = ECCNSValue.toString(context, ECCNSContext.this(context));

    chars = ECCNSValue.stringBytes(&context->this);
    length = ECCNSValue.stringLength(&context->this);

    from = ECCNSContext.argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED)
        start = ECCNSText.make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.binary == INFINITY)
        start = ECCNSText.make(chars + length, 0);
    else
        start = textAtIndex(chars, length, ECCNSValue.toInteger(context, from).data.integer, 1);

    to = ECCNSContext.argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && (isnan(to.data.binary) || to.data.binary == INFINITY)))
        end = ECCNSText.make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && to.data.binary == -INFINITY)
        end = ECCNSText.make(chars, length);
    else
        end = textAtIndex(chars, length, ECCNSValue.toInteger(context, to).data.integer, 1);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        headcp = ECCNSText.nextCharacter(&start).codepoint;

    length = (int32_t)(end.bytes - start.bytes);

    if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        head = 3;

    if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        tail = 3;

    if(head + length + tail <= 0)
        return ECCNSValue.text(&ECC_ConstString_Empty);
    else
    {
        ecccharbuffer_t* result = io_libecc_Chars.createSized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            io_libecc_Chars.writeCodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            io_libecc_Chars.writeCodepoint(result->bytes + head + length, (((ECCNSText.character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
        }

        return ECCNSValue.chars(result);
    }
}

static eccvalue_t split(eccstate_t* context)
{
    eccvalue_t separatorValue, limitValue;
    eccobjregexp_t* regexp = NULL;
    eccobject_t* array;
    ecccharbuffer_t* element;
    ecctextstring_t text, separator = { 0 };
    uint32_t size = 0, limit = UINT32_MAX;

    ECCNSContext.assertThisCoerciblePrimitive(context);

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));
    text = ECCNSValue.textOf(&context->this);

    limitValue = ECCNSContext.argument(context, 1);
    if(limitValue.type != ECC_VALTYPE_UNDEFINED)
    {
        limit = ECCNSValue.toInteger(context, limitValue).data.integer;
        if(!limit)
            return ECCNSValue.object(io_libecc_Array.createSized(0));
    }

    separatorValue = ECCNSContext.argument(context, 0);
    if(separatorValue.type == ECC_VALTYPE_UNDEFINED)
    {
        eccobject_t* subarr = io_libecc_Array.createSized(1);
        subarr->element[0].value = context->this;
        return ECCNSValue.object(subarr);
    }
    else if(separatorValue.type == ECC_VALTYPE_REGEXP)
        regexp = separatorValue.data.regexp;
    else
    {
        separatorValue = ECCNSValue.toString(context, separatorValue);
        separator = ECCNSValue.textOf(&separatorValue);
    }

    ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);

    array = io_libecc_Array.create();

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

            if(seek.length && io_libecc_RegExp.matchWithState(regexp, &state))
            {
                if(capture[1] <= text.bytes)
                {
                    ECCNSText.advance(&seek, 1);
                    continue;
                }

                element = io_libecc_Chars.createWithBytes((int32_t)(capture[0] - text.bytes), text.bytes);
                ECCNSObject.addElement(array, size++, ECCNSValue.chars(element), 0);

                for(numindex = 1, count = regexp->count; numindex < count; ++numindex)
                {
                    if(size >= limit)
                        break;

                    if(capture[numindex * 2])
                    {
                        element = io_libecc_Chars.createWithBytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]);
                        ECCNSObject.addElement(array, size++, ECCNSValue.chars(element), 0);
                    }
                    else
                        ECCNSObject.addElement(array, size++, ECCValConstUndefined, 0);
                }

                ECCNSText.advance(&text, (int32_t)(capture[1] - text.bytes));
                seek = text;
            }
            else
            {
                element = io_libecc_Chars.createWithBytes(text.length, text.bytes);
                ECCNSObject.addElement(array, size++, ECCNSValue.chars(element), 0);
                break;
            }
        }
        return ECCNSValue.object(array);
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
                ECCNSObject.addElement(array, size++, ECCNSValue.buffer(text.bytes, c.units), 0);
            else
            {
                char buffer[7];

                /* simulate 16-bit surrogate */

                io_libecc_Chars.writeCodepoint(buffer, (((c.codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
                ECCNSObject.addElement(array, size++, ECCNSValue.buffer(buffer, 3), 0);

                io_libecc_Chars.writeCodepoint(buffer, (((c.codepoint - 0x010000) >> 0) & 0x3ff) + 0xdc00);
                ECCNSObject.addElement(array, size++, ECCNSValue.buffer(buffer, 3), 0);
            }
            ECCNSText.advance(&text, c.units);
        }

        return ECCNSValue.object(array);
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
                element = io_libecc_Chars.createSized(length);
                memcpy(element->bytes, text.bytes, length);
                ECCNSObject.addElement(array, size++, ECCNSValue.chars(element), 0);

                ECCNSText.advance(&text, length + separator.length);
                seek = text;
                continue;
            }
            ECCNSText.nextCharacter(&seek);
        }

        if(size < limit)
        {
            element = io_libecc_Chars.createSized(text.length);
            memcpy(element->bytes, text.bytes, text.length);
            ECCNSObject.addElement(array, size++, ECCNSValue.chars(element), 0);
        }
    }

    return ECCNSValue.object(array);
}

static eccvalue_t substring(eccstate_t* context)
{
    eccvalue_t from, to;
    ecctextstring_t start, end;
    const char* chars;
    int32_t length, head = 0, tail = 0;
    uint32_t headcp = 0;

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));
    chars = ECCNSValue.stringBytes(&context->this);
    length = ECCNSValue.stringLength(&context->this);

    from = ECCNSContext.argument(context, 0);
    if(from.type == ECC_VALTYPE_UNDEFINED || (from.type == ECC_VALTYPE_BINARY && (isnan(from.data.binary) || from.data.binary == -INFINITY)))
        start = ECCNSText.make(chars, length);
    else if(from.type == ECC_VALTYPE_BINARY && from.data.binary == INFINITY)
        start = ECCNSText.make(chars + length, 0);
    else
        start = textAtIndex(chars, length, ECCNSValue.toInteger(context, from).data.integer, 0);

    to = ECCNSContext.argument(context, 1);
    if(to.type == ECC_VALTYPE_UNDEFINED || (to.type == ECC_VALTYPE_BINARY && to.data.binary == INFINITY))
        end = ECCNSText.make(chars + length, 0);
    else if(to.type == ECC_VALTYPE_BINARY && !isfinite(to.data.binary))
        end = ECCNSText.make(chars, length);
    else
        end = textAtIndex(chars, length, ECCNSValue.toInteger(context, to).data.integer, 0);

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
        return ECCNSValue.text(&ECC_ConstString_Empty);
    else
    {
        ecccharbuffer_t* result = io_libecc_Chars.createSized(length + head + tail);

        if(start.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            io_libecc_Chars.writeCodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
        }

        if(length > 0)
            memcpy(result->bytes + head, start.bytes, length);

        if(end.flags & ECC_TEXTFLAG_BREAKFLAG)
        {
            /* simulate 16-bit surrogate */
            io_libecc_Chars.writeCodepoint(result->bytes + head + length, (((ECCNSText.character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
        }

        return ECCNSValue.chars(result);
    }
}

static eccvalue_t toLowerCase(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text;

    if(!ECCNSValue.isString(context->this))
        context->this = ECCNSValue.toString(context, ECCNSContext.this(context));

    text = ECCNSValue.textOf(&context->this);
    {
        char buffer[text.length * 2];
        char* end = ECCNSText.toLower(text, buffer);
        chars = io_libecc_Chars.createWithBytes((int32_t)(end - buffer), buffer);
    }

    return ECCNSValue.chars(chars);
}

static eccvalue_t toUpperCase(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text;

    context->this = ECCNSValue.toString(context, ECCNSContext.this(context));
    text = ECCNSValue.textOf(&context->this);
    {
        char buffer[text.length * 3];
        char* end = ECCNSText.toUpper(text, buffer);
        chars = io_libecc_Chars.createWithBytes((int32_t)(end - buffer), buffer);
    }

    return ECCNSValue.chars(chars);
}

static eccvalue_t trim(eccstate_t* context)
{
    ecccharbuffer_t* chars;
    ecctextstring_t text, last;
    ecctextchar_t c;

    if(!ECCNSValue.isString(context->this))
        context->this = ECCNSValue.toString(context, ECCNSContext.this(context));

    text = ECCNSValue.textOf(&context->this);
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

    chars = io_libecc_Chars.createWithBytes(text.length, text.bytes);

    return ECCNSValue.chars(chars);
}

static eccvalue_t constructor(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        value = ECCNSValue.text(value.check == 1 ? &ECC_ConstString_Undefined : &ECC_ConstString_Empty);
    else
        value = ECCNSValue.toString(context, value);

    if(context->construct)
        return ECCNSValue.string(create(io_libecc_Chars.createWithBytes(ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value))));
    else
        return value;
}

static eccvalue_t fromCharCode(eccstate_t* context)
{
    eccappendbuffer_t chars;
    int32_t index, count;

    count = ECCNSContext.argumentCount(context);

    io_libecc_Chars.beginAppend(&chars);

    for(index = 0; index < count; ++index)
        io_libecc_Chars.appendCodepoint(&chars, (uint16_t)ECCNSValue.toInteger(context, ECCNSContext.argument(context, index)).data.integer);

    return io_libecc_Chars.endAppend(&chars);
}

// MARK: - Methods

void setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    io_libecc_Function.setupBuiltinObject(&io_libecc_string_constructor, constructor, 1, &io_libecc_string_prototype,
                                          ECCNSValue.string(create(io_libecc_Chars.createSized(0))), &io_libecc_string_type);

    io_libecc_Function.addMethod(io_libecc_string_constructor, "fromCharCode", fromCharCode, -1, h);

    io_libecc_Function.addToObject(io_libecc_string_prototype, "toString", toString, 0, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "valueOf", valueOf, 0, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "charAt", charAt, 1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "charCodeAt", charCodeAt, 1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "concat", concat, -1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "indexOf", indexOf, -1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "lastIndexOf", lastIndexOf, -1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "localeCompare", localeCompare, 1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "match", match, 1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "replace", replace, 2, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "search", search, 1, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "slice", slice, 2, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "split", split, 2, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "substring", substring, 2, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "toLowerCase", toLowerCase, 0, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "toLocaleLowerCase", toLowerCase, 0, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "toUpperCase", toUpperCase, 0, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "toLocaleUpperCase", toUpperCase, 0, h);
    io_libecc_Function.addToObject(io_libecc_string_prototype, "trim", trim, 0, h);
}

void teardown(void)
{
    io_libecc_string_prototype = NULL;
    io_libecc_string_constructor = NULL;
}

eccobjstring_t* create(ecccharbuffer_t* chars)
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;
    uint32_t length;
    eccobjstring_t* self = malloc(sizeof(*self));
    *self = io_libecc_String.identity;
    io_libecc_Pool.addObject(&self->object);
    ECCNSObject.initialize(&self->object, io_libecc_string_prototype);
    length = unitIndex(chars->bytes, chars->length, chars->length);
    ECCNSObject.addMember(&self->object, io_libecc_key_length, ECCNSValue.integer(length), r | h | s);
    self->value = chars;
    if(length == (uint32_t)chars->length)
    {
        chars->flags |= io_libecc_chars_asciiOnly;
    }
    return self;
}

eccvalue_t valueAtIndex(eccobjstring_t* self, int32_t index)
{
    ecctextchar_t c;
    ecctextstring_t text;

    text = textAtIndex(self->value->bytes, self->value->length, index, 0);
    c = ECCNSText.character(text);

    if(c.units <= 0)
        return ECCValConstUndefined;
    else
    {
        if(c.codepoint < 0x010000)
            return ECCNSValue.buffer(text.bytes, c.units);
        else
        {
            char buffer[7];

            /* simulate 16-bit surrogate */

            c.codepoint -= 0x010000;
            if(text.flags & ECC_TEXTFLAG_BREAKFLAG)
                c.codepoint = ((c.codepoint >> 0) & 0x3ff) + 0xdc00;
            else
                c.codepoint = ((c.codepoint >> 10) & 0x3ff) + 0xd800;

            io_libecc_Chars.writeCodepoint(buffer, c.codepoint);
            return ECCNSValue.buffer(buffer, 3);
        }
    }
}

ecctextstring_t textAtIndex(const char* chars, int32_t length, int32_t position, int enableReverse)
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

int32_t unitIndex(const char* chars, int32_t max, int32_t unit)
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
