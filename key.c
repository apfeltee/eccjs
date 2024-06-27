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

eccindexkey_t ECC_ConstKey_none = { { { 0 } } };

eccindexkey_t ECC_ConstKey_prototype;
eccindexkey_t ECC_ConstKey_constructor;
eccindexkey_t ECC_ConstKey_length;
eccindexkey_t ECC_ConstKey_arguments;
eccindexkey_t ECC_ConstKey_callee;
eccindexkey_t ECC_ConstKey_name;
eccindexkey_t ECC_ConstKey_message;
eccindexkey_t ECC_ConstKey_toString;
eccindexkey_t ECC_ConstKey_valueOf;
eccindexkey_t ECC_ConstKey_eval;
eccindexkey_t ECC_ConstKey_value;
eccindexkey_t ECC_ConstKey_writable;
eccindexkey_t ECC_ConstKey_enumerable;
eccindexkey_t ECC_ConstKey_configurable;
eccindexkey_t ECC_ConstKey_get;
eccindexkey_t ECC_ConstKey_set;
eccindexkey_t ECC_ConstKey_join;
eccindexkey_t ECC_ConstKey_toISOString;
eccindexkey_t ECC_ConstKey_input;
eccindexkey_t ECC_ConstKey_index;
eccindexkey_t ECC_ConstKey_lastIndex;
eccindexkey_t ECC_ConstKey_global;
eccindexkey_t ECC_ConstKey_ignoreCase;
eccindexkey_t ECC_ConstKey_multiline;
eccindexkey_t ECC_ConstKey_source;

// MARK: - Static Members

static void nskeyfn_setup(void);
static void nskeyfn_teardown(void);
static eccindexkey_t nskeyfn_makeWithCString(const char* cString);
static eccindexkey_t nskeyfn_makeWithText(const ecctextstring_t text, eccindexflags_t flags);
static eccindexkey_t nskeyfn_search(const ecctextstring_t text);
static int nskeyfn_isEqual(eccindexkey_t, eccindexkey_t);
static const ecctextstring_t* nskeyfn_textOf(eccindexkey_t);
static void nskeyfn_dumpTo(eccindexkey_t, FILE*);
const struct eccpseudonskey_t ECCNSKey = {
    nskeyfn_setup, nskeyfn_teardown, nskeyfn_makeWithCString, nskeyfn_makeWithText, nskeyfn_search, nskeyfn_isEqual, nskeyfn_textOf, nskeyfn_dumpTo,
    {}
};

static eccindexkey_t ecckey_makeWithNumber(uint16_t number)
{
    eccindexkey_t key;

    key.data.depth[0] = number >> 12 & 0xf;
    key.data.depth[1] = number >> 8 & 0xf;
    key.data.depth[2] = number >> 4 & 0xf;
    key.data.depth[3] = number >> 0 & 0xf;

    return key;
}

static eccindexkey_t ecckey_addWithText(const ecctextstring_t text, eccindexflags_t flags)
{
    if(keyCount >= keyCapacity)
    {
        if(keyCapacity == UINT16_MAX)
            ECCNSScript.fatal("No more identifier left");

        keyCapacity += 0xff;
        keyPool = realloc(keyPool, keyCapacity * sizeof(*keyPool));
    }
    /*
    if((isdigit(text.bytes[0]) || text.bytes[0] == '-') && !isnan(ECCNSLexer.scanBinary(text, 0).data.binary))
    {
        ECCNSEnv.printWarning("Creating identifier '%.*s'; %u identifier(s) left. Using array of length > 0x%x, or negative-integer/floating-point as property name is discouraged",
                              text.length, text.bytes, UINT16_MAX - keyCount, io_libecc_object_ElementMax);
    }
    */
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

    return ecckey_makeWithNumber(keyCount);
}

// MARK: - Methods

void nskeyfn_setup(void)
{
    const char* cstr;
    if(!keyPool)
    {
        {
            cstr = "prototype";
            ECC_ConstKey_prototype = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "constructor";
            ECC_ConstKey_constructor = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "length";
            ECC_ConstKey_length = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "arguments";
            ECC_ConstKey_arguments = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "callee";
            ECC_ConstKey_callee = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "name";
            ECC_ConstKey_name = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "message";
            ECC_ConstKey_message = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toString";
            ECC_ConstKey_toString = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "valueOf";
            ECC_ConstKey_valueOf = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "eval";
            ECC_ConstKey_eval = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "value";
            ECC_ConstKey_value = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "writable";
            ECC_ConstKey_writable = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "enumerable";
            ECC_ConstKey_enumerable = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "configurable";
            ECC_ConstKey_configurable = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "get";
            ECC_ConstKey_get = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "set";
            ECC_ConstKey_set = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "join";
            ECC_ConstKey_join = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toISOString";
            ECC_ConstKey_toISOString = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "input";
            ECC_ConstKey_input = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "index";
            ECC_ConstKey_index = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "lastIndex";
            ECC_ConstKey_lastIndex = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "global";
            ECC_ConstKey_global = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "ignoreCase";
            ECC_ConstKey_ignoreCase = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "multiline";
            ECC_ConstKey_multiline = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "source";
            ECC_ConstKey_source = ecckey_addWithText(ECCNSText.make(cstr, strlen(cstr)), 0);
        }
    }
}

void nskeyfn_teardown(void)
{
    while(charsCount)
        free(charsList[--charsCount]), charsList[charsCount] = NULL;

    free(charsList), charsList = NULL, charsCount = 0;
    free(keyPool), keyPool = NULL, keyCount = 0, keyCapacity = 0;
}

eccindexkey_t nskeyfn_makeWithCString(const char* cString)
{
    return nskeyfn_makeWithText(ECCNSText.make(cString, (uint16_t)strlen(cString)), 0);
}

eccindexkey_t nskeyfn_makeWithText(const ecctextstring_t text, eccindexflags_t flags)
{
    eccindexkey_t key = nskeyfn_search(text);

    if(!key.data.integer)
        key = ecckey_addWithText(text, flags);

    return key;
}

eccindexkey_t nskeyfn_search(const ecctextstring_t text)
{
    uint16_t index = 0;

    for(index = 0; index < keyCount; ++index)
    {
        if(text.length == keyPool[index].length && memcmp(keyPool[index].bytes, text.bytes, text.length) == 0)
        {
            return ecckey_makeWithNumber(index + 1);
        }
    }
    return ecckey_makeWithNumber(0);
}

int nskeyfn_isEqual(eccindexkey_t self, eccindexkey_t to)
{
    return self.data.integer == to.data.integer;
}

const ecctextstring_t* nskeyfn_textOf(eccindexkey_t key)
{
    uint16_t number = key.data.depth[0] << 12 | key.data.depth[1] << 8 | key.data.depth[2] << 4 | key.data.depth[3];
    if(number)
        return &keyPool[number - 1];
    else
        return &ECC_ConstString_Empty;
}

void nskeyfn_dumpTo(eccindexkey_t key, FILE* file)
{
    const ecctextstring_t* text = nskeyfn_textOf(key);
    fprintf(file, "%.*s", (int)text->length, text->bytes);
}
