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

const eccobjinterntype_t ECC_Type_Global = {
    .text = &ECC_ConstString_GlobalType,
};

// MARK: - Static Members

static eccvalue_t globalsfn_eval(eccstate_t* context)
{
    eccvalue_t value;
    eccioinput_t* input;
    eccstate_t subContext = {};
    subContext.parent = context;
    subContext.thisvalue = ecc_value_object(&context->ecc->global->environment);
    subContext.ecc = context->ecc;
    subContext.depth = context->depth + 1;
    subContext.environment = ecc_context_environmentroot(context->parent);

    value = ecc_context_argument(context, 0);
    if(!ecc_value_isstring(value) || !ecc_value_isprimitive(value))
        return value;

    input = ecc_ioinput_createfrombytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value), "(eval)");

    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_NO);
    ecc_script_evalinputwithcontext(context->ecc, input, &subContext);

    return context->ecc->result;
}

static eccvalue_t globalsfn_parseInt(eccstate_t* context)
{
    eccvalue_t value;
    ecctextstring_t text;
    int32_t base;

    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    base = ecc_value_tointeger(context, ecc_context_argument(context, 1)).data.integer;
    text = ecc_value_textof(&value);

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

    return ecc_astlex_scaninteger(text, base, ECC_LEXFLAG_SCANLAZY | (context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0));
}

static eccvalue_t globalsfn_parseFloat(eccstate_t* context)
{
    eccvalue_t value;
    ecctextstring_t text;

    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    text = ecc_value_textof(&value);
    return ecc_astlex_scanbinary(text, ECC_LEXFLAG_SCANLAZY | (context->ecc->sloppyMode ? ECC_LEXFLAG_SCANSLOPPY : 0));
}

static eccvalue_t globalsfn_isFinite(eccstate_t* context)
{
    eccvalue_t value;

    value = ecc_value_tobinary(context, ecc_context_argument(context, 0));
    return ecc_value_truth(!isnan(value.data.binary) && !isinf(value.data.binary));
}

static eccvalue_t globalsfn_isNaN(eccstate_t* context)
{
    eccvalue_t value;

    value = ecc_value_tobinary(context, ecc_context_argument(context, 0));
    return ecc_value_truth(isnan(value.data.binary));
}

static eccvalue_t eccglobals_decodeExcept(eccstate_t* context, const char* exclude)
{
    uint16_t index;
    uint16_t count;
    uint8_t byte;
    int continuation;
    char buffer[5];
    char *b;
    const char* bytes;
    eccvalue_t value;
    eccappendbuffer_t chars;
    ecctextchar_t c;
    index = 0;
    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    bytes = ecc_value_stringbytes(&value);
    count = ecc_value_stringlength(&value);
    ecc_charbuf_beginappend(&chars);
    while(index < count)
    {
        byte = bytes[index++];
        if(byte != '%')
        {
            ecc_charbuf_append(&chars, "%c", byte);
        }
        else if(index + 2 > count || !isxdigit(bytes[index]) || !isxdigit(bytes[index + 1]))
        {
            goto error;
        }
        else
        {
            byte = ecc_astlex_uint8hex(bytes[index], bytes[index + 1]);
            index += 2;
            if(byte >= 0x80)
            {
                continuation = (byte & 0xf8) == 0xf0 ? 3 : (byte & 0xf0) == 0xe0 ? 2 : (byte & 0xe0) == 0xc0 ? 1 : 0;
                if(!continuation || index + continuation * 3 > count)
                {
                    goto error;
                }
                b = buffer;
                (*b++) = byte;
                while(continuation--)
                {
                    if(bytes[index++] != '%' || !isxdigit(bytes[index]) || !isxdigit(bytes[index + 1]))
                    {
                        goto error;
                    }
                    byte = ecc_astlex_uint8hex(bytes[index], bytes[index + 1]);
                    index += 2;
                    if((byte & 0xc0) != 0x80)
                    {
                        goto error;
                    }
                    (*b++) = byte;
                }
                *b = '\0';
                c = ecc_textbuf_character(ecc_textbuf_make(buffer, (int32_t)(b - buffer)));
                ecc_charbuf_appendcodepoint(&chars, c.codepoint);
            }
            else if(byte && exclude && strchr(exclude, byte))
            {
                ecc_charbuf_append(&chars, "%%%c%c", bytes[index - 2], bytes[index - 1]);
            }
            else
            {
                ecc_charbuf_append(&chars, "%c", byte);
            }
        }
    }
    return ecc_charbuf_endappend(&chars);
    error:
    {
        ecc_context_urierror(context, ecc_charbuf_create("malformed URI"));
    }
    return ECCValConstUndefined;
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
    eccvalue_t value;
    const char* bytes;
    uint16_t offset;
    uint16_t unit;
    uint16_t length;
    int needPair;
    ecccharbuffer_t* chars;
    ecctextstring_t text;
    ecctextchar_t c;
    static const char hex[] = "0123456789ABCDEF";
    offset = 0;
    needPair = 0;
    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    bytes = ecc_value_stringbytes(&value);
    length = ecc_value_stringlength(&value);
    text = ecc_textbuf_make(bytes, length);
    chars = ecc_charbuf_createsized(length * 3);
    while(text.length)
    {
        c = ecc_textbuf_character(text);
        if(c.codepoint < 0x80 && c.codepoint && strchr(exclude, c.codepoint))
            chars->bytes[offset++] = c.codepoint;
        else
        {
            if(c.codepoint >= 0xDC00 && c.codepoint <= 0xDFFF)
            {
                if(!needPair)
                {
                    goto error;
                }
            }
            else if(needPair)
            {
                goto error;
            }
            else if(c.codepoint >= 0xD800 && c.codepoint <= 0xDBFF)
            {
                needPair = 1;
            }
            for(unit = 0; unit < c.units; ++unit)
            {
                chars->bytes[offset++] = '%';
                chars->bytes[offset++] = hex[(uint8_t)text.bytes[unit] >> 4];
                chars->bytes[offset++] = hex[(uint8_t)text.bytes[unit] & 0xf];
            }
        }
        ecc_textbuf_advance(&text, c.units);
    }
    if(needPair)
    {
        goto error;
    }
    chars->length = offset;
    return ecc_value_chars(chars);
    error:
    {
        ecc_context_urierror(context, ecc_charbuf_create("malformed URI"));
    }
    return ECCValConstUndefined;
}

static eccvalue_t globalsfn_encodeURI(eccstate_t* context)
{
    static const char* encstrings = (
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "-_.!~*'();/?:@&=+$,#"
    );
    return eccglobals_encodeExpect(context, encstrings);
}

static eccvalue_t globalsfn_encodeURIComponent(eccstate_t* context)
{
    static const char* encstrings = (
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "-_.!~*'()"
    );
    return eccglobals_encodeExpect(context, encstrings);
}

static eccvalue_t globalsfn_escape(eccstate_t* context)
{
    eccvalue_t value;
    eccappendbuffer_t chars;
    ecctextstring_t text;
    ecctextchar_t c;
    static const char* exclude = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 @*_+-./";
    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    text = ecc_value_textof(&value);
    ecc_charbuf_beginappend(&chars);
    while(text.length)
    {
        c = ecc_textbuf_nextcharacter(&text);
        if(c.codepoint < 0x80 && c.codepoint && strchr(exclude, c.codepoint))
        {
            ecc_charbuf_append(&chars, "%c", c.codepoint);
        }
        else
        {
            if(c.codepoint <= 0xff)
            {
                ecc_charbuf_append(&chars, "%%%02X", c.codepoint);
            }
            else if(c.codepoint < 0x010000)
            {
                ecc_charbuf_append(&chars, "%%u%04X", c.codepoint);
            }
            else
            {
                c.codepoint -= 0x010000;
                ecc_charbuf_append(&chars, "%%u%04X", ((c.codepoint >> 10) & 0x3ff) + 0xd800);
                ecc_charbuf_append(&chars, "%%u%04X", ((c.codepoint >> 0) & 0x3ff) + 0xdc00);
            }
        }
    }
    return ecc_charbuf_endappend(&chars);
}

static eccvalue_t globalsfn_unescape(eccstate_t* context)
{
    eccvalue_t value;
    eccappendbuffer_t chars;
    ecctextstring_t text;
    ecctextchar_t c;

    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    text = ecc_value_textof(&value);

    ecc_charbuf_beginappend(&chars);

    while(text.length)
    {
        c = ecc_textbuf_nextcharacter(&text);

        if(c.codepoint == '%')
        {
            switch(ecc_textbuf_character(text).codepoint)
            {
                case '%':
                    ecc_charbuf_append(&chars, "%%");
                    break;

                case 'u':
                {
                    uint32_t cp = ecc_astlex_uint16hex(text.bytes[1], text.bytes[2], text.bytes[3], text.bytes[4]);

                    ecc_charbuf_appendcodepoint(&chars, cp);
                    ecc_textbuf_advance(&text, 5);
                    break;
                }

                default:
                {
                    uint32_t cp = ecc_astlex_uint8hex(text.bytes[0], text.bytes[1]);

                    ecc_charbuf_appendcodepoint(&chars, cp);
                    ecc_textbuf_advance(&text, 2);
                    break;
                }
            }
        }
        else
            ecc_charbuf_append(&chars, "%c", c.codepoint);
    }

    return ecc_charbuf_endappend(&chars);
}

// MARK: - Methods

void ecc_globals_setup(void)
{
    ECC_Prototype_Function = ECC_Prototype_Object = ecc_object_create(NULL);

    ecc_function_setup();
    ecc_object_setup();
    ecc_string_setup();
    ecc_error_setup();
    ecc_array_setup();
    ecc_date_setup();
    ecc_libmath_setup();
    ecc_number_setup();
    ecc_bool_setup();
    ecc_regexp_setup();
    ecc_json_setup();
    ecc_args_setup();
}

void ecc_globals_teardown(void)
{
    ecc_function_teardown();
    ecc_object_teardown();
    ecc_string_teardown();
    ecc_error_teardown();
    ecc_array_teardown();
    ecc_date_teardown();
    ecc_libmath_teardown();
    ecc_number_teardown();
    ecc_bool_teardown();
    ecc_regexp_teardown();
    ecc_json_teardown();
    ecc_args_teardown();
}

eccobjfunction_t* ecc_globals_create(void)
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    eccobjfunction_t* self = ecc_function_create(ECC_Prototype_Object);
    self->environment.type = &ECC_Type_Global;

    ecc_function_addvalue(self, "NaN", ecc_value_binary(NAN), r | h | s);
    ecc_function_addvalue(self, "Infinity", ecc_value_binary(INFINITY), r | h | s);
    ecc_function_addvalue(self, "undefined", ECCValConstUndefined, r | h | s);

    ecc_function_addfunction(self, "eval", globalsfn_eval, 1, h);
    ecc_function_addfunction(self, "escape", globalsfn_escape, 1, h);
    ecc_function_addfunction(self, "unescape", globalsfn_unescape, 1, h);
    ecc_function_addfunction(self, "parseInt", globalsfn_parseInt, 2, h);
    ecc_function_addfunction(self, "parseFloat", globalsfn_parseFloat, 1, h);
    ecc_function_addfunction(self, "isNaN", globalsfn_isNaN, 1, h);
    ecc_function_addfunction(self, "isFinite", globalsfn_isFinite, 1, h);
    ecc_function_addfunction(self, "decodeURI", globalsfn_decodeURI, 1, h);
    ecc_function_addfunction(self, "decodeURIComponent", globalsfn_decodeURIComponent, 1, h);
    ecc_function_addfunction(self, "encodeURI", globalsfn_encodeURI, 1, h);
    ecc_function_addfunction(self, "encodeURIComponent", globalsfn_encodeURIComponent, 1, h);
    ecc_function_addvalue(self, "Object", ecc_value_function(ECC_CtorFunc_Object), h);
    ecc_function_addvalue(self, "Function", ecc_value_function(ECC_CtorFunc_Function), h);
    ecc_function_addvalue(self, "Array", ecc_value_function(ECC_CtorFunc_Array), h);
    ecc_function_addvalue(self, "String", ecc_value_function(ECC_CtorFunc_String), h);
    ecc_function_addvalue(self, "Boolean", ecc_value_function(ECC_CtorFunc_Boolean), h);
    ecc_function_addvalue(self, "Number", ecc_value_function(ECC_CtorFunc_Number), h);
    ecc_function_addvalue(self, "Date", ecc_value_function(ECC_CtorFunc_Date), h);
    ecc_function_addvalue(self, "RegExp", ecc_value_function(ECC_CtorFunc_Regexp), h);
    ecc_function_addvalue(self, "Error", ecc_value_function(ECC_CtorFunc_Error), h);
    ecc_function_addvalue(self, "RangeError", ecc_value_function(ECC_CtorFunc_ErrorRangeError), h);
    ecc_function_addvalue(self, "ReferenceError", ecc_value_function(ECC_CtorFunc_ErrorReferenceError), h);
    ecc_function_addvalue(self, "SyntaxError", ecc_value_function(ECC_CtorFunc_ErrorSyntaxError), h);
    ecc_function_addvalue(self, "TypeError", ecc_value_function(ECC_CtorFunc_ErrorTypeError), h);
    ecc_function_addvalue(self, "URIError", ecc_value_function(ECC_CtorFunc_ErrorUriError), h);
    ecc_function_addvalue(self, "EvalError", ecc_value_function(ECC_CtorFunc_ErrorEvalError), h);
    ecc_function_addvalue(self, "Math", ecc_value_object(ECC_Prototype_MathObject), h);
    ecc_function_addvalue(self, "JSON", ecc_value_object(ECC_Prototype_JSONObject), h);

    return self;
}
