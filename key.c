//
//  key.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static ecctextstring_t* keyPool = NULL;
static uint16_t keyCount = 0;
static uint16_t keyCapacity = 0;

static char** charsList = NULL;
static uint16_t charsCount = 0;

eccindexkey_t io_libecc_key_none = { { { 0 } } };

eccindexkey_t io_libecc_key_prototype;
eccindexkey_t io_libecc_key_constructor;
eccindexkey_t io_libecc_key_length;
eccindexkey_t io_libecc_key_arguments;
eccindexkey_t io_libecc_key_callee;
eccindexkey_t io_libecc_key_name;
eccindexkey_t io_libecc_key_message;
eccindexkey_t io_libecc_key_toString;
eccindexkey_t io_libecc_key_valueOf;
eccindexkey_t io_libecc_key_eval;
eccindexkey_t io_libecc_key_value;
eccindexkey_t io_libecc_key_writable;
eccindexkey_t io_libecc_key_enumerable;
eccindexkey_t io_libecc_key_configurable;
eccindexkey_t io_libecc_key_get;
eccindexkey_t io_libecc_key_set;
eccindexkey_t io_libecc_key_join;
eccindexkey_t io_libecc_key_toISOString;
eccindexkey_t io_libecc_key_input;
eccindexkey_t io_libecc_key_index;
eccindexkey_t io_libecc_key_lastIndex;
eccindexkey_t io_libecc_key_global;
eccindexkey_t io_libecc_key_ignoreCase;
eccindexkey_t io_libecc_key_multiline;
eccindexkey_t io_libecc_key_source;

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static eccindexkey_t makeWithCString(const char* cString);
static eccindexkey_t makeWithText(const ecctextstring_t text, eccindexflags_t flags);
static eccindexkey_t search(const ecctextstring_t text);
static int isEqual(eccindexkey_t, eccindexkey_t);
static const ecctextstring_t* textOf(eccindexkey_t);
static void dumpTo(eccindexkey_t, FILE*);
const struct eccpseudonskey_t io_libecc_Key = {
    setup, teardown, makeWithCString, makeWithText, search, isEqual, textOf, dumpTo,
    {}
};

static eccindexkey_t makeWithNumber(uint16_t number)
{
    eccindexkey_t key;

    key.data.depth[0] = number >> 12 & 0xf;
    key.data.depth[1] = number >> 8 & 0xf;
    key.data.depth[2] = number >> 4 & 0xf;
    key.data.depth[3] = number >> 0 & 0xf;

    return key;
}

static eccindexkey_t addWithText(const ecctextstring_t text, eccindexflags_t flags)
{
    if(keyCount >= keyCapacity)
    {
        if(keyCapacity == UINT16_MAX)
            ECCNSScript.fatal("No more identifier left");

        keyCapacity += 0xff;
        keyPool = realloc(keyPool, keyCapacity * sizeof(*keyPool));
    }

    if((isdigit(text.bytes[0]) || text.bytes[0] == '-') && !isnan(io_libecc_Lexer.scanBinary(text, 0).data.binary))
        ECCNSEnv.printWarning("Creating identifier '%.*s'; %u identifier(s) left. Using array of length > 0x%x, or negative-integer/floating-point as property name is discouraged",
                              text.length, text.bytes, UINT16_MAX - keyCount, io_libecc_object_ElementMax);

    if(flags & ECC_INDEXFLAG_COPYONCREATE)
    {
        char* chars = malloc(text.length + 1);
        memcpy(chars, text.bytes, text.length);
        chars[text.length] = '\0';
        charsList = realloc(charsList, sizeof(*charsList) * (charsCount + 1));
        charsList[charsCount++] = chars;
        keyPool[keyCount++] = ECCNSText.make(chars, text.length);
    }
    else
        keyPool[keyCount++] = text;

    return makeWithNumber(keyCount);
}

// MARK: - Methods

void setup(void)
{
    const char* cstr;
    if(!keyPool)
    {
        {
            cstr = "prototype";
            io_libecc_key_prototype = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "constructor";
            io_libecc_key_constructor = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "length";
            io_libecc_key_length = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "arguments";
            io_libecc_key_arguments = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "callee";
            io_libecc_key_callee = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "name";
            io_libecc_key_name = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "message";
            io_libecc_key_message = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toString";
            io_libecc_key_toString = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "valueOf";
            io_libecc_key_valueOf = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "eval";
            io_libecc_key_eval = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "value";
            io_libecc_key_value = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "writable";
            io_libecc_key_writable = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "enumerable";
            io_libecc_key_enumerable = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "configurable";
            io_libecc_key_configurable = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "get";
            io_libecc_key_get = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "set";
            io_libecc_key_set = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "join";
            io_libecc_key_join = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toISOString";
            io_libecc_key_toISOString = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "input";
            io_libecc_key_input = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "index";
            io_libecc_key_index = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "lastIndex";
            io_libecc_key_lastIndex = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "global";
            io_libecc_key_global = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "ignoreCase";
            io_libecc_key_ignoreCase = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "multiline";
            io_libecc_key_multiline = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "source";
            io_libecc_key_source = addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
    }
}

void teardown(void)
{
    while(charsCount)
        free(charsList[--charsCount]), charsList[charsCount] = NULL;

    free(charsList), charsList = NULL, charsCount = 0;
    free(keyPool), keyPool = NULL, keyCount = 0, keyCapacity = 0;
}

eccindexkey_t makeWithCString(const char* cString)
{
    return makeWithText(ECCNSText.make(cString, (uint16_t)strlen(cString)), 0);
}

eccindexkey_t makeWithText(const ecctextstring_t text, eccindexflags_t flags)
{
    eccindexkey_t key = search(text);

    if(!key.data.integer)
        key = addWithText(text, flags);

    return key;
}

eccindexkey_t search(const ecctextstring_t text)
{
    uint16_t index = 0;

    for(index = 0; index < keyCount; ++index)
    {
        if(text.length == keyPool[index].length && memcmp(keyPool[index].bytes, text.bytes, text.length) == 0)
        {
            return makeWithNumber(index + 1);
        }
    }
    return makeWithNumber(0);
}

int isEqual(eccindexkey_t self, eccindexkey_t to)
{
    return self.data.integer == to.data.integer;
}

const ecctextstring_t* textOf(eccindexkey_t key)
{
    uint16_t number = key.data.depth[0] << 12 | key.data.depth[1] << 8 | key.data.depth[2] << 4 | key.data.depth[3];
    if(number)
        return &keyPool[number - 1];
    else
        return &ECC_ConstString_Empty;
}

void dumpTo(eccindexkey_t key, FILE* file)
{
    const ecctextstring_t* text = textOf(key);
    fprintf(file, "%.*s", (int)text->length, text->bytes);
}
