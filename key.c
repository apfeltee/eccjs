//
//  key.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "key.h"

#include "env.h"
#include "lexer.h"
#include "ecc.h"
#include "builtins.h"

// MARK: - Private

static struct io_libecc_Text *keyPool = NULL;
static uint16_t keyCount = 0;
static uint16_t keyCapacity = 0;

static char **charsList = NULL;
static uint16_t charsCount = 0;

struct io_libecc_Key io_libecc_key_none = {{{ 0 }}};

struct io_libecc_Key io_libecc_key_prototype;
struct io_libecc_Key io_libecc_key_constructor;
struct io_libecc_Key io_libecc_key_length;
struct io_libecc_Key io_libecc_key_arguments;
struct io_libecc_Key io_libecc_key_callee;
struct io_libecc_Key io_libecc_key_name;
struct io_libecc_Key io_libecc_key_message;
struct io_libecc_Key io_libecc_key_toString;
struct io_libecc_Key io_libecc_key_valueOf;
struct io_libecc_Key io_libecc_key_eval;
struct io_libecc_Key io_libecc_key_value;
struct io_libecc_Key io_libecc_key_writable;
struct io_libecc_Key io_libecc_key_enumerable;
struct io_libecc_Key io_libecc_key_configurable;
struct io_libecc_Key io_libecc_key_get;
struct io_libecc_Key io_libecc_key_set;
struct io_libecc_Key io_libecc_key_join;
struct io_libecc_Key io_libecc_key_toISOString;
struct io_libecc_Key io_libecc_key_input;
struct io_libecc_Key io_libecc_key_index;
struct io_libecc_Key io_libecc_key_lastIndex;
struct io_libecc_Key io_libecc_key_global;
struct io_libecc_Key io_libecc_key_ignoreCase;
struct io_libecc_Key io_libecc_key_multiline;
struct io_libecc_Key io_libecc_key_source;


// MARK: - Static Members

static void setup(void);
static void teardown(void);
static struct io_libecc_Key makeWithCString(const char* cString);
static struct io_libecc_Key makeWithText(const struct io_libecc_Text text, enum io_libecc_key_Flags flags);
static struct io_libecc_Key search(const struct io_libecc_Text text);
static int isEqual(struct io_libecc_Key, struct io_libecc_Key);
static const struct io_libecc_Text* textOf(struct io_libecc_Key);
static void dumpTo(struct io_libecc_Key, FILE*);
const struct type_io_libecc_Key io_libecc_Key = {
    setup, teardown, makeWithCString, makeWithText, search, isEqual, textOf, dumpTo,
};

static
struct io_libecc_Key makeWithNumber (uint16_t number)
{
	struct io_libecc_Key key;
	
	key.data.depth[0] = number >> 12 & 0xf;
	key.data.depth[1] = number >> 8 & 0xf;
	key.data.depth[2] = number >> 4 & 0xf;
	key.data.depth[3] = number >> 0 & 0xf;
	
	return key;
}

static
struct io_libecc_Key addWithText (const struct io_libecc_Text text, enum io_libecc_key_Flags flags)
{
	if (keyCount >= keyCapacity)
	{
		if (keyCapacity == UINT16_MAX)
			io_libecc_Ecc.fatal("No more identifier left");
		
		keyCapacity += 0xff;
		keyPool = realloc(keyPool, keyCapacity * sizeof(*keyPool));
	}
	
	if ((isdigit(text.bytes[0]) || text.bytes[0] == '-') && !isnan(io_libecc_Lexer.scanBinary(text, 0).data.binary))
		io_libecc_Env.printWarning("Creating identifier '%.*s'; %u identifier(s) left. Using array of length > 0x%x, or negative-integer/floating-point as property name is discouraged", text.length, text.bytes, UINT16_MAX - keyCount, io_libecc_object_ElementMax);
	
	if (flags & io_libecc_key_copyOnCreate)
	{
		char *chars = malloc(text.length + 1);
		memcpy(chars, text.bytes, text.length);
		chars[text.length] = '\0';
		charsList = realloc(charsList, sizeof(*charsList) * (charsCount + 1));
		charsList[charsCount++] = chars;
		keyPool[keyCount++] = io_libecc_Text.make(chars, text.length);
	}
	else
		keyPool[keyCount++] = text;
	
	return makeWithNumber(keyCount);
}

// MARK: - Methods

void setup (void)
{
    const char* cstr;
	if (!keyPool)
	{
        {
            cstr = "prototype";
            io_libecc_key_prototype = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "constructor";
            io_libecc_key_constructor = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "length";
            io_libecc_key_length = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "arguments";
            io_libecc_key_arguments = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "callee";
            io_libecc_key_callee = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "name";
            io_libecc_key_name = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "message";
            io_libecc_key_message = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toString";
            io_libecc_key_toString = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "valueOf";
            io_libecc_key_valueOf = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "eval";
            io_libecc_key_eval = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "value";
            io_libecc_key_value = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "writable";
            io_libecc_key_writable = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "enumerable";
            io_libecc_key_enumerable = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "configurable";
            io_libecc_key_configurable = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "get";
            io_libecc_key_get = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "set";
            io_libecc_key_set = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "join";
            io_libecc_key_join = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "toISOString";
            io_libecc_key_toISOString = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "input";
            io_libecc_key_input = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "index";
            io_libecc_key_index = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "lastIndex";
            io_libecc_key_lastIndex = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "global";
            io_libecc_key_global = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "ignoreCase";
            io_libecc_key_ignoreCase = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "multiline";
            io_libecc_key_multiline = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }
        {
            cstr = "source";
            io_libecc_key_source = addWithText(io_libecc_Text.make(cstr, strlen(cstr)), 0);
        }


	}
}

void teardown (void)
{
	while (charsCount)
		free(charsList[--charsCount]), charsList[charsCount] = NULL;
	
	free(charsList), charsList = NULL, charsCount = 0;
	free(keyPool), keyPool = NULL, keyCount = 0, keyCapacity = 0;
}

struct io_libecc_Key makeWithCString (const char *cString)
{
	return makeWithText(io_libecc_Text.make(cString, (uint16_t)strlen(cString)), 0);
}

struct io_libecc_Key makeWithText (const struct io_libecc_Text text, enum io_libecc_key_Flags flags)
{
	struct io_libecc_Key key = search(text);
	
	if (!key.data.integer)
		key = addWithText(text, flags);
	
	return key;
}

struct io_libecc_Key search (const struct io_libecc_Text text)
{
	uint16_t index = 0;
	
	for (index = 0; index < keyCount; ++index)
	{
		if (text.length == keyPool[index].length && memcmp(keyPool[index].bytes, text.bytes, text.length) == 0)
		{
			return makeWithNumber(index + 1);
		}
	}
	return makeWithNumber(0);
}

int isEqual (struct io_libecc_Key self, struct io_libecc_Key to)
{
	return self.data.integer == to.data.integer;
}

const struct io_libecc_Text *textOf (struct io_libecc_Key key)
{
	uint16_t number = key.data.depth[0] << 12 | key.data.depth[1] << 8 | key.data.depth[2] << 4 | key.data.depth[3];
	if (number)
		return &keyPool[number - 1];
	else
		return &io_libecc_text_empty;
}

void dumpTo (struct io_libecc_Key key, FILE *file)
{
	const struct io_libecc_Text *text = textOf(key);
	fprintf(file, "%.*s", (int)text->length, text->bytes);
}
