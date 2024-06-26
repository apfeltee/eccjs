//
//  global.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static void setup(void);
static void teardown(void);
static eccobjscriptfunction_t* create(void);
const struct eccpseudonsglobal_t io_libecc_Global = {
    setup,
    teardown,
    create,
    {}
};

const eccobjinterntype_t ECCObjTypeGlobal = {
    .text = &ECC_ConstString_GlobalType,
};

// MARK: - Static Members

static eccvalue_t eval(eccstate_t* context)
{
    eccvalue_t value;
    eccioinput_t* input;
    eccstate_t subContext = {
        .parent = context,
        .thisvalue = ECCNSValue.object(&context->ecc->global->environment),
        .ecc = context->ecc,
        .depth = context->depth + 1,
        .environment = ECCNSContext.environmentRoot(context->parent),
    };

    value = ECCNSContext.argument(context, 0);
    if(!ECCNSValue.isString(value) || !ECCNSValue.isPrimitive(value))
        return value;

    input = io_libecc_Input.createFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value), "(eval)");

    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_NO);
    ECCNSScript.evalInputWithContext(context->ecc, input, &subContext);

    return context->ecc->result;
}

static eccvalue_t parseInt(eccstate_t* context)
{
    eccvalue_t value;
    ecctextstring_t text;
    int32_t base;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    base = ECCNSValue.toInteger(context, ECCNSContext.argument(context, 1)).data.integer;
    text = ECCNSValue.textOf(&value);

    if(!base)
    {
        // prevent octal auto-detection

        if(text.length > 2 && text.bytes[0] == '-')
        {
            if(text.bytes[1] == '0' && tolower(text.bytes[2]) != 'x')
                base = 10;
        }
        else if(text.length > 1 && text.bytes[0] == '0' && tolower(text.bytes[1]) != 'x')
            base = 10;
    }

    return io_libecc_Lexer.scanInteger(text, base, ECC_LEXFLAG_SCANLAZY | (context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0));
}

static eccvalue_t parseFloat(eccstate_t* context)
{
    eccvalue_t value;
    ecctextstring_t text;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    text = ECCNSValue.textOf(&value);
    return io_libecc_Lexer.scanBinary(text, ECC_LEXFLAG_SCANLAZY | (context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0));
}

static eccvalue_t isFinite(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0));
    return ECCNSValue.truth(!isnan(value.data.binary) && !isinf(value.data.binary));
}

static eccvalue_t isNaN(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0));
    return ECCNSValue.truth(isnan(value.data.binary));
}

static eccvalue_t decodeExcept(eccstate_t* context, const char* exclude)
{
    char buffer[5], *b;
    eccvalue_t value;
    const char* bytes;
    uint16_t index = 0, count;
    eccappendbuffer_t chars;
    uint8_t byte;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    bytes = ECCNSValue.stringBytes(&value);
    count = ECCNSValue.stringLength(&value);

    io_libecc_Chars.beginAppend(&chars);

    while(index < count)
    {
        byte = bytes[index++];

        if(byte != '%')
        {
            io_libecc_Chars.append(&chars, "%c", byte);
        }
        else if(index + 2 > count || !isxdigit(bytes[index]) || !isxdigit(bytes[index + 1]))
        {
            goto error;
        }
        else
        {
            byte = io_libecc_Lexer.uint8Hex(bytes[index], bytes[index + 1]);
            index += 2;

            if(byte >= 0x80)
            {
                ecctextchar_t c;
                int continuation = (byte & 0xf8) == 0xf0 ? 3 : (byte & 0xf0) == 0xe0 ? 2 : (byte & 0xe0) == 0xc0 ? 1 : 0;

                if(!continuation || index + continuation * 3 > count)
                    goto error;

                b = buffer;
                (*b++) = byte;
                while(continuation--)
                {
                    if(bytes[index++] != '%' || !isxdigit(bytes[index]) || !isxdigit(bytes[index + 1]))
                        goto error;

                    byte = io_libecc_Lexer.uint8Hex(bytes[index], bytes[index + 1]);
                    index += 2;

                    if((byte & 0xc0) != 0x80)
                        goto error;

                    (*b++) = byte;
                }
                *b = '\0';

                c = ECCNSText.character(ECCNSText.make(buffer, (int32_t)(b - buffer)));
                io_libecc_Chars.appendCodepoint(&chars, c.codepoint);
            }
            else if(byte && exclude && strchr(exclude, byte))
                io_libecc_Chars.append(&chars, "%%%c%c", bytes[index - 2], bytes[index - 1]);
            else
                io_libecc_Chars.append(&chars, "%c", byte);
        }
    }

    return io_libecc_Chars.endAppend(&chars);

error:
    ECCNSContext.uriError(context, io_libecc_Chars.create("malformed URI"));
    return ECCValConstNull;
}

static eccvalue_t decodeURI(eccstate_t* context)
{
    return decodeExcept(context, ";/?:@&=+$,#");
}

static eccvalue_t decodeURIComponent(eccstate_t* context)
{
    return decodeExcept(context, NULL);
}

static eccvalue_t encodeExpect(eccstate_t* context, const char* exclude)
{
    const char hex[] = "0123456789ABCDEF";
    eccvalue_t value;
    const char* bytes;
    uint16_t offset = 0, unit, length;
    ecccharbuffer_t* chars;
    ecctextstring_t text;
    ecctextchar_t c;
    int needPair = 0;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    bytes = ECCNSValue.stringBytes(&value);
    length = ECCNSValue.stringLength(&value);
    text = ECCNSText.make(bytes, length);

    chars = io_libecc_Chars.createSized(length * 3);

    while(text.length)
    {
        c = ECCNSText.character(text);

        if(c.codepoint < 0x80 && c.codepoint && strchr(exclude, c.codepoint))
            chars->bytes[offset++] = c.codepoint;
        else
        {
            if(c.codepoint >= 0xDC00 && c.codepoint <= 0xDFFF)
            {
                if(!needPair)
                    goto error;
            }
            else if(needPair)
                goto error;
            else if(c.codepoint >= 0xD800 && c.codepoint <= 0xDBFF)
                needPair = 1;

            for(unit = 0; unit < c.units; ++unit)
            {
                chars->bytes[offset++] = '%';
                chars->bytes[offset++] = hex[(uint8_t)text.bytes[unit] >> 4];
                chars->bytes[offset++] = hex[(uint8_t)text.bytes[unit] & 0xf];
            }
        }

        ECCNSText.advance(&text, c.units);
    }

    if(needPair)
        goto error;

    chars->length = offset;
    return ECCNSValue.chars(chars);

error:
    ECCNSContext.uriError(context, io_libecc_Chars.create("malformed URI"));
    return ECCValConstNull;
}

static eccvalue_t encodeURI(eccstate_t* context)
{
    return encodeExpect(context, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()"
                                 ";/?:@&=+$,#");
}

static eccvalue_t encodeURIComponent(eccstate_t* context)
{
    return encodeExpect(context, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()");
}

static eccvalue_t escape(eccstate_t* context)
{
    const char* exclude = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 @*_+-./";
    eccvalue_t value;
    eccappendbuffer_t chars;
    ecctextstring_t text;
    ecctextchar_t c;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    text = ECCNSValue.textOf(&value);

    io_libecc_Chars.beginAppend(&chars);

    while(text.length)
    {
        c = ECCNSText.nextCharacter(&text);

        if(c.codepoint < 0x80 && c.codepoint && strchr(exclude, c.codepoint))
            io_libecc_Chars.append(&chars, "%c", c.codepoint);
        else
        {
            if(c.codepoint <= 0xff)
                io_libecc_Chars.append(&chars, "%%%02X", c.codepoint);
            else if(c.codepoint < 0x010000)
                io_libecc_Chars.append(&chars, "%%u%04X", c.codepoint);
            else
            {
                c.codepoint -= 0x010000;
                io_libecc_Chars.append(&chars, "%%u%04X", ((c.codepoint >> 10) & 0x3ff) + 0xd800);
                io_libecc_Chars.append(&chars, "%%u%04X", ((c.codepoint >> 0) & 0x3ff) + 0xdc00);
            }
        }
    }

    return io_libecc_Chars.endAppend(&chars);
}

static eccvalue_t unescape(eccstate_t* context)
{
    eccvalue_t value;
    eccappendbuffer_t chars;
    ecctextstring_t text;
    ecctextchar_t c;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    text = ECCNSValue.textOf(&value);

    io_libecc_Chars.beginAppend(&chars);

    while(text.length)
    {
        c = ECCNSText.nextCharacter(&text);

        if(c.codepoint == '%')
        {
            switch(ECCNSText.character(text).codepoint)
            {
                case '%':
                    io_libecc_Chars.append(&chars, "%%");
                    break;

                case 'u':
                {
                    uint32_t cp = io_libecc_Lexer.uint16Hex(text.bytes[1], text.bytes[2], text.bytes[3], text.bytes[4]);

                    io_libecc_Chars.appendCodepoint(&chars, cp);
                    ECCNSText.advance(&text, 5);
                    break;
                }

                default:
                {
                    uint32_t cp = io_libecc_Lexer.uint8Hex(text.bytes[0], text.bytes[1]);

                    io_libecc_Chars.appendCodepoint(&chars, cp);
                    ECCNSText.advance(&text, 2);
                    break;
                }
            }
        }
        else
            io_libecc_Chars.append(&chars, "%c", c.codepoint);
    }

    return io_libecc_Chars.endAppend(&chars);
}

// MARK: - Methods

void setup(void)
{
    io_libecc_function_prototype = io_libecc_object_prototype = ECCNSObject.create(NULL);

    io_libecc_Function.setup();
    ECCNSObject.setup();
    io_libecc_String.setup();
    io_libecc_Error.setup();
    io_libecc_Array.setup();
    io_libecc_Date.setup();
    io_libecc_Math.setup();
    io_libecc_Number.setup();
    io_libecc_Boolean.setup();
    io_libecc_RegExp.setup();
    io_libecc_JSON.setup();
    ECCNSArguments.setup();
}

void teardown(void)
{
    io_libecc_Function.teardown();
    ECCNSObject.teardown();
    io_libecc_String.teardown();
    io_libecc_Error.teardown();
    io_libecc_Array.teardown();
    io_libecc_Date.teardown();
    io_libecc_Math.teardown();
    io_libecc_Number.teardown();
    io_libecc_Boolean.teardown();
    io_libecc_RegExp.teardown();
    io_libecc_JSON.teardown();
    ECCNSArguments.teardown();
}

eccobjscriptfunction_t* create(void)
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    eccobjscriptfunction_t* self = io_libecc_Function.create(io_libecc_object_prototype);
    self->environment.type = &ECCObjTypeGlobal;

    io_libecc_Function.addValue(self, "NaN", ECCNSValue.binary(NAN), r | h | s);
    io_libecc_Function.addValue(self, "Infinity", ECCNSValue.binary(INFINITY), r | h | s);
    io_libecc_Function.addValue(self, "undefined", ECCValConstUndefined, r | h | s);

    io_libecc_Function.addFunction(self, "eval", eval, 1, h);
    io_libecc_Function.addFunction(self, "escape", escape, 1, h);
    io_libecc_Function.addFunction(self, "unescape", unescape, 1, h);
    io_libecc_Function.addFunction(self, "parseInt", parseInt, 2, h);
    io_libecc_Function.addFunction(self, "parseFloat", parseFloat, 1, h);
    io_libecc_Function.addFunction(self, "isNaN", isNaN, 1, h);
    io_libecc_Function.addFunction(self, "isFinite", isFinite, 1, h);
    io_libecc_Function.addFunction(self, "decodeURI", decodeURI, 1, h);
    io_libecc_Function.addFunction(self, "decodeURIComponent", decodeURIComponent, 1, h);
    io_libecc_Function.addFunction(self, "encodeURI", encodeURI, 1, h);
    io_libecc_Function.addFunction(self, "encodeURIComponent", encodeURIComponent, 1, h);
    io_libecc_Function.addValue(self, "Object", ECCNSValue.function(io_libecc_object_constructor), h);
    io_libecc_Function.addValue(self, "Function", ECCNSValue.function(io_libecc_function_constructor), h);
    io_libecc_Function.addValue(self, "Array", ECCNSValue.function(io_libecc_array_constructor), h);
    io_libecc_Function.addValue(self, "String", ECCNSValue.function(io_libecc_string_constructor), h);
    io_libecc_Function.addValue(self, "Boolean", ECCNSValue.function(io_libecc_boolean_constructor), h);
    io_libecc_Function.addValue(self, "Number", ECCNSValue.function(io_libecc_number_constructor), h);
    io_libecc_Function.addValue(self, "Date", ECCNSValue.function(io_libecc_date_constructor), h);
    io_libecc_Function.addValue(self, "RegExp", ECCNSValue.function(io_libecc_regexp_constructor), h);
    io_libecc_Function.addValue(self, "Error", ECCNSValue.function(io_libecc_error_constructor), h);
    io_libecc_Function.addValue(self, "RangeError", ECCNSValue.function(io_libecc_error_rangeConstructor), h);
    io_libecc_Function.addValue(self, "ReferenceError", ECCNSValue.function(io_libecc_error_referenceConstructor), h);
    io_libecc_Function.addValue(self, "SyntaxError", ECCNSValue.function(io_libecc_error_syntaxConstructor), h);
    io_libecc_Function.addValue(self, "TypeError", ECCNSValue.function(io_libecc_error_typeConstructor), h);
    io_libecc_Function.addValue(self, "URIError", ECCNSValue.function(io_libecc_error_uriConstructor), h);
    io_libecc_Function.addValue(self, "EvalError", ECCNSValue.function(io_libecc_error_evalConstructor), h);
    io_libecc_Function.addValue(self, "Math", ECCNSValue.object(io_libecc_math_object), h);
    io_libecc_Function.addValue(self, "JSON", ECCNSValue.object(io_libecc_json_object), h);

    return self;
}
