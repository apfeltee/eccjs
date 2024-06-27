//
//  global.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static eccvalue_t globalsfn_eval(eccstate_t *context);
static eccvalue_t globalsfn_parseInt(eccstate_t *context);
static eccvalue_t globalsfn_parseFloat(eccstate_t *context);
static eccvalue_t globalsfn_isFinite(eccstate_t *context);
static eccvalue_t globalsfn_isNaN(eccstate_t *context);
static eccvalue_t eccglobals_decodeExcept(eccstate_t *context, const char *exclude);
static eccvalue_t globalsfn_decodeURI(eccstate_t *context);
static eccvalue_t globalsfn_decodeURIComponent(eccstate_t *context);
static eccvalue_t eccglobals_encodeExpect(eccstate_t *context, const char *exclude);
static eccvalue_t globalsfn_encodeURI(eccstate_t *context);
static eccvalue_t globalsfn_encodeURIComponent(eccstate_t *context);
static eccvalue_t globalsfn_escape(eccstate_t *context);
static eccvalue_t globalsfn_unescape(eccstate_t *context);
static void nsglobalfn_setup(void);
static void nsglobalfn_teardown(void);
static eccobjfunction_t *nsglobalfn_create(void);


const struct eccpseudonsglobal_t ECCNSGlobal = {
    nsglobalfn_setup,
    nsglobalfn_teardown,
    nsglobalfn_create,
    {}
};

const eccobjinterntype_t ECC_Type_Global = {
    .text = &ECC_ConstString_GlobalType,
};

// MARK: - Static Members

static eccvalue_t globalsfn_eval(eccstate_t* context)
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

    input = ECCNSInput.createFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value), "(eval)");

    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_NO);
    ECCNSScript.evalInputWithContext(context->ecc, input, &subContext);

    return context->ecc->result;
}

static eccvalue_t globalsfn_parseInt(eccstate_t* context)
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

    return ECCNSLexer.scanInteger(text, base, ECC_LEXFLAG_SCANLAZY | (context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0));
}

static eccvalue_t globalsfn_parseFloat(eccstate_t* context)
{
    eccvalue_t value;
    ecctextstring_t text;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    text = ECCNSValue.textOf(&value);
    return ECCNSLexer.scanBinary(text, ECC_LEXFLAG_SCANLAZY | (context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0));
}

static eccvalue_t globalsfn_isFinite(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0));
    return ECCNSValue.truth(!isnan(value.data.binary) && !isinf(value.data.binary));
}

static eccvalue_t globalsfn_isNaN(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0));
    return ECCNSValue.truth(isnan(value.data.binary));
}

static eccvalue_t eccglobals_decodeExcept(eccstate_t* context, const char* exclude)
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

    ECCNSChars.beginAppend(&chars);

    while(index < count)
    {
        byte = bytes[index++];

        if(byte != '%')
            ECCNSChars.append(&chars, "%c", byte);
        else if(index + 2 > count || !isxdigit(bytes[index]) || !isxdigit(bytes[index + 1]))
            goto error;
        else
        {
            byte = ECCNSLexer.uint8Hex(bytes[index], bytes[index + 1]);
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

                    byte = ECCNSLexer.uint8Hex(bytes[index], bytes[index + 1]);
                    index += 2;

                    if((byte & 0xc0) != 0x80)
                        goto error;

                    (*b++) = byte;
                }
                *b = '\0';

                c = ECCNSText.character(ECCNSText.make(buffer, (int32_t)(b - buffer)));
                ECCNSChars.appendCodepoint(&chars, c.codepoint);
            }
            else if(byte && exclude && strchr(exclude, byte))
                ECCNSChars.append(&chars, "%%%c%c", bytes[index - 2], bytes[index - 1]);
            else
                ECCNSChars.append(&chars, "%c", byte);
        }
    }

    return ECCNSChars.endAppend(&chars);

error:
    ECCNSContext.uriError(context, ECCNSChars.create("malformed URI"));
}

static eccvalue_t globalsfn_decodeURI(eccstate_t* context)
{
    return eccglobals_decodeExcept(context, ";/?:@&=+$,#");
}

static eccvalue_t globalsfn_decodeURIComponent(eccstate_t* context)
{
    return eccglobals_decodeExcept(context, NULL);
}

static eccvalue_t eccglobals_encodeExpect(eccstate_t* context, const char* exclude)
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

    chars = ECCNSChars.createSized(length * 3);

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
    ECCNSContext.uriError(context, ECCNSChars.create("malformed URI"));
}

static eccvalue_t globalsfn_encodeURI(eccstate_t* context)
{
    return eccglobals_encodeExpect(context, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()"
                                 ";/?:@&=+$,#");
}

static eccvalue_t globalsfn_encodeURIComponent(eccstate_t* context)
{
    return eccglobals_encodeExpect(context, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()");
}

static eccvalue_t globalsfn_escape(eccstate_t* context)
{
    const char* exclude = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 @*_+-./";
    eccvalue_t value;
    eccappendbuffer_t chars;
    ecctextstring_t text;
    ecctextchar_t c;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    text = ECCNSValue.textOf(&value);

    ECCNSChars.beginAppend(&chars);

    while(text.length)
    {
        c = ECCNSText.nextCharacter(&text);

        if(c.codepoint < 0x80 && c.codepoint && strchr(exclude, c.codepoint))
            ECCNSChars.append(&chars, "%c", c.codepoint);
        else
        {
            if(c.codepoint <= 0xff)
                ECCNSChars.append(&chars, "%%%02X", c.codepoint);
            else if(c.codepoint < 0x010000)
                ECCNSChars.append(&chars, "%%u%04X", c.codepoint);
            else
            {
                c.codepoint -= 0x010000;
                ECCNSChars.append(&chars, "%%u%04X", ((c.codepoint >> 10) & 0x3ff) + 0xd800);
                ECCNSChars.append(&chars, "%%u%04X", ((c.codepoint >> 0) & 0x3ff) + 0xdc00);
            }
        }
    }

    return ECCNSChars.endAppend(&chars);
}

static eccvalue_t globalsfn_unescape(eccstate_t* context)
{
    eccvalue_t value;
    eccappendbuffer_t chars;
    ecctextstring_t text;
    ecctextchar_t c;

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    text = ECCNSValue.textOf(&value);

    ECCNSChars.beginAppend(&chars);

    while(text.length)
    {
        c = ECCNSText.nextCharacter(&text);

        if(c.codepoint == '%')
        {
            switch(ECCNSText.character(text).codepoint)
            {
                case '%':
                    ECCNSChars.append(&chars, "%%");
                    break;

                case 'u':
                {
                    uint32_t cp = ECCNSLexer.uint16Hex(text.bytes[1], text.bytes[2], text.bytes[3], text.bytes[4]);

                    ECCNSChars.appendCodepoint(&chars, cp);
                    ECCNSText.advance(&text, 5);
                    break;
                }

                default:
                {
                    uint32_t cp = ECCNSLexer.uint8Hex(text.bytes[0], text.bytes[1]);

                    ECCNSChars.appendCodepoint(&chars, cp);
                    ECCNSText.advance(&text, 2);
                    break;
                }
            }
        }
        else
            ECCNSChars.append(&chars, "%c", c.codepoint);
    }

    return ECCNSChars.endAppend(&chars);
}

// MARK: - Methods

static void nsglobalfn_setup(void)
{
    ECC_Prototype_Function = ECC_Prototype_Object = ECCNSObject.create(NULL);

    ECCNSFunction.setup();
    ECCNSObject.setup();
    ECCNSString.setup();
    ECCNSError.setup();
    ECCNSArray.setup();
    ECCNSDate.setup();
    io_libecc_Math.setup();
    ECCNSNumber.setup();
    ECCNSBool.setup();
    ECCNSRegExp.setup();
    ECCNSJSON.setup();
    ECCNSArguments.setup();
}

static void nsglobalfn_teardown(void)
{
    ECCNSFunction.teardown();
    ECCNSObject.teardown();
    ECCNSString.teardown();
    ECCNSError.teardown();
    ECCNSArray.teardown();
    ECCNSDate.teardown();
    io_libecc_Math.teardown();
    ECCNSNumber.teardown();
    ECCNSBool.teardown();
    ECCNSRegExp.teardown();
    ECCNSJSON.teardown();
    ECCNSArguments.teardown();
}

static eccobjfunction_t* nsglobalfn_create(void)
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    eccobjfunction_t* self = ECCNSFunction.create(ECC_Prototype_Object);
    self->environment.type = &ECC_Type_Global;

    ECCNSFunction.addValue(self, "NaN", ECCNSValue.binary(NAN), r | h | s);
    ECCNSFunction.addValue(self, "Infinity", ECCNSValue.binary(INFINITY), r | h | s);
    ECCNSFunction.addValue(self, "undefined", ECCValConstUndefined, r | h | s);

    ECCNSFunction.addFunction(self, "eval", globalsfn_eval, 1, h);
    ECCNSFunction.addFunction(self, "escape", globalsfn_escape, 1, h);
    ECCNSFunction.addFunction(self, "unescape", globalsfn_unescape, 1, h);
    ECCNSFunction.addFunction(self, "parseInt", globalsfn_parseInt, 2, h);
    ECCNSFunction.addFunction(self, "parseFloat", globalsfn_parseFloat, 1, h);
    ECCNSFunction.addFunction(self, "isNaN", globalsfn_isNaN, 1, h);
    ECCNSFunction.addFunction(self, "isFinite", globalsfn_isFinite, 1, h);
    ECCNSFunction.addFunction(self, "decodeURI", globalsfn_decodeURI, 1, h);
    ECCNSFunction.addFunction(self, "decodeURIComponent", globalsfn_decodeURIComponent, 1, h);
    ECCNSFunction.addFunction(self, "encodeURI", globalsfn_encodeURI, 1, h);
    ECCNSFunction.addFunction(self, "encodeURIComponent", globalsfn_encodeURIComponent, 1, h);
    ECCNSFunction.addValue(self, "Object", ECCNSValue.function(ECC_CtorFunc_Object), h);
    ECCNSFunction.addValue(self, "Function", ECCNSValue.function(ECC_CtorFunc_Function), h);
    ECCNSFunction.addValue(self, "Array", ECCNSValue.function(ECC_CtorFunc_Array), h);
    ECCNSFunction.addValue(self, "String", ECCNSValue.function(ECC_CtorFunc_String), h);
    ECCNSFunction.addValue(self, "Boolean", ECCNSValue.function(ECC_CtorFunc_Boolean), h);
    ECCNSFunction.addValue(self, "Number", ECCNSValue.function(ECC_CtorFunc_Number), h);
    ECCNSFunction.addValue(self, "Date", ECCNSValue.function(ECC_CtorFunc_Date), h);
    ECCNSFunction.addValue(self, "RegExp", ECCNSValue.function(ECC_CtorFunc_Regexp), h);
    ECCNSFunction.addValue(self, "Error", ECCNSValue.function(ECC_CtorFunc_Error), h);
    ECCNSFunction.addValue(self, "RangeError", ECCNSValue.function(ECC_CtorFunc_ErrorRangeError), h);
    ECCNSFunction.addValue(self, "ReferenceError", ECCNSValue.function(ECC_CtorFunc_ErrorReferenceError), h);
    ECCNSFunction.addValue(self, "SyntaxError", ECCNSValue.function(ECC_CtorFunc_ErrorSyntaxError), h);
    ECCNSFunction.addValue(self, "TypeError", ECCNSValue.function(ECC_CtorFunc_ErrorTypeError), h);
    ECCNSFunction.addValue(self, "URIError", ECCNSValue.function(ECC_CtorFunc_ErrorUriError), h);
    ECCNSFunction.addValue(self, "EvalError", ECCNSValue.function(ECC_CtorFunc_ErrorEvalError), h);
    ECCNSFunction.addValue(self, "Math", ECCNSValue.object(ECC_Prototype_MathObject), h);
    ECCNSFunction.addValue(self, "JSON", ECCNSValue.object(ECC_Prototype_JSONObject), h);

    return self;
}
