
/*
//  key.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/
#include "ecc.h"


static ecctextstring_t* g_storedkeylist = NULL;
static uint32_t g_storedkeycount = 0;
static uint32_t g_storedkeycapacity = 0;

static char** g_storedcharslist = NULL;
static uint32_t g_storedcharscount = 0;

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

eccindexkey_t ecc_keyidx_makewithnumber(uint32_t number)
{
    eccindexkey_t key;
    key.data.depth[0] = number >> 12 & 0xf;
    key.data.depth[1] = number >> 8 & 0xf;
    key.data.depth[2] = number >> 4 & 0xf;
    key.data.depth[3] = number >> 0 & 0xf;
    return key;
}

eccindexkey_t ecc_keyidx_addwithtext(const ecctextstring_t text, int flags)
{
    size_t needed;
    char* chars;
    char** tmp;
    ecctextstring_t* ettmp;
    if(g_storedkeycount >= g_storedkeycapacity)
    {
        /*
        if(g_storedkeycapacity == UINT16_MAX)
        {
            ecc_script_fatal("No more identifier left");
        }
        */
        g_storedkeycapacity += 0xff;
        needed = (g_storedkeycapacity * sizeof(*g_storedkeylist));
        ettmp = (ecctextstring_t*)realloc(g_storedkeylist, needed);
        if(ettmp == NULL)
        {
            fprintf(stderr, "in addwithtext: failed to reallocate for %ld bytes\n", needed);
        }
        g_storedkeylist = ettmp;

    }
    /*
    if((isdigit(text.bytes[0]) || text.bytes[0] == '-') && !isnan(ecc_astlex_scanbinary(text, 0).data.valnumfloat))
    {
        ecc_env_printwarning("Creating identifier '%.*s'; %u identifier(s) left. Using array of length > 0x%x, or negative-integer/floating-point as property name is discouraged",
                              text.length, text.bytes, UINT16_MAX - g_storedkeycount, ECC_CONF_MAXELEMENTS);
    }
    */
    if(flags & ECC_INDEXFLAG_COPYONCREATE)
    {
        chars = (char*)malloc(text.length + 1);
        memcpy(chars, text.bytes, text.length);
        chars[text.length] = '\0';
        needed = (sizeof(*g_storedcharslist) * (g_storedcharscount + 1));
        tmp = (char**)realloc(g_storedcharslist, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in addwithtext: failed to reallocate for %ld bytes\n", needed);
        }
        g_storedcharslist = tmp;
        g_storedcharslist[g_storedcharscount++] = chars;
        g_storedkeylist[g_storedkeycount++] = ecc_textbuf_make(chars, text.length);
    }
    else
    {
        g_storedkeylist[g_storedkeycount++] = text;
    }
    return ecc_keyidx_makewithnumber(g_storedkeycount);
}

void ecc_keyidx_setup(void)
{
    const char* cstr;
    if(!g_storedkeylist)
    {
        {
            cstr = "prototype";
            ECC_ConstKey_prototype = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "constructor";
            ECC_ConstKey_constructor = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "length";
            ECC_ConstKey_length = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "arguments";
            ECC_ConstKey_arguments = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "callee";
            ECC_ConstKey_callee = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "name";
            ECC_ConstKey_name = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "message";
            ECC_ConstKey_message = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toString";
            ECC_ConstKey_toString = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "valueOf";
            ECC_ConstKey_valueOf = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "eval";
            ECC_ConstKey_eval = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "value";
            ECC_ConstKey_value = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "writable";
            ECC_ConstKey_writable = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "enumerable";
            ECC_ConstKey_enumerable = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "configurable";
            ECC_ConstKey_configurable = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "get";
            ECC_ConstKey_get = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "set";
            ECC_ConstKey_set = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "join";
            ECC_ConstKey_join = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toISOString";
            ECC_ConstKey_toISOString = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "input";
            ECC_ConstKey_input = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "index";
            ECC_ConstKey_index = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "lastIndex";
            ECC_ConstKey_lastIndex = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "global";
            ECC_ConstKey_global = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "ignoreCase";
            ECC_ConstKey_ignoreCase = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "multiline";
            ECC_ConstKey_multiline = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "source";
            ECC_ConstKey_source = ecc_keyidx_addwithtext(ecc_textbuf_make(cstr, strlen(cstr)), 0);
        }
    }
}

void ecc_keyidx_teardown(void)
{
    while(g_storedcharscount)
    {
        free(g_storedcharslist[--g_storedcharscount]);
        g_storedcharslist[g_storedcharscount] = NULL;
    }
    free(g_storedcharslist);
    g_storedcharslist = NULL;
    g_storedcharscount = 0;
    free(g_storedkeylist);
    g_storedkeylist = NULL;
    g_storedkeycount = 0;
    g_storedkeycapacity = 0;
}

eccindexkey_t ecc_keyidx_makewithcstring(const char* cString)
{
    return ecc_keyidx_makewithtext(ecc_textbuf_make(cString, (uint32_t)strlen(cString)), 0);
}

eccindexkey_t ecc_keyidx_makewithtext(const ecctextstring_t text, int flags)
{
    eccindexkey_t key = ecc_keyidx_search(text);
    if(!key.data.integer)
    {
        key = ecc_keyidx_addwithtext(text, flags);
    }
    return key;
}

eccindexkey_t ecc_keyidx_search(const ecctextstring_t text)
{
    uint32_t index;
    index = 0;
    for(index = 0; index < g_storedkeycount; ++index)
    {
        if(text.length == g_storedkeylist[index].length && memcmp(g_storedkeylist[index].bytes, text.bytes, text.length) == 0)
        {
            return ecc_keyidx_makewithnumber(index + 1);
        }
    }
    return ecc_keyidx_makewithnumber(0);
}

int ecc_keyidx_isequal(eccindexkey_t self, eccindexkey_t to)
{
    return self.data.integer == to.data.integer;
}

const ecctextstring_t* ecc_keyidx_textof(eccindexkey_t key)
{
    uint32_t number;
    number = ((key.data.depth[0] << 12) | (key.data.depth[1] << 8) | (key.data.depth[2] << 4) | key.data.depth[3]);
    if(number)
    {
        return &g_storedkeylist[number - 1];
    }
    return &ECC_String_Empty;
}

void ecc_keyidx_dumpto(eccindexkey_t key, FILE* file)
{
    const ecctextstring_t* text = ecc_keyidx_textof(key);
    fprintf(file, "%.*s", (int)text->length, text->bytes);
}

