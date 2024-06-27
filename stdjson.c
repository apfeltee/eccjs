//
//  json.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

typedef struct eccjsonparser_t eccjsonparser_t;
typedef struct eccjsonstringify_t eccjsonstringify_t;

struct eccjsonparser_t
{
    ecctextstring_t text;
    const char* start;
    int line;

    // reviver

    eccstate_t context;
    eccobjfunction_t* function;
    eccobject_t* arguments;
    const eccoperand_t* ops;
};

struct eccjsonstringify_t
{
    eccappendbuffer_t chars;
    char spaces[11];
    int level;
    eccobject_t* filter;

    // replacer

    eccstate_t context;
    eccobjfunction_t* function;
    eccobject_t* arguments;
    const eccoperand_t* ops;
};

static eccvalue_t eccjson_error(eccjsonparser_t *parse, int length, ecccharbuffer_t *chars);
static ecctextstring_t eccjson_errorOfLine(eccjsonparser_t *parse);
static ecctextchar_t eccjson_nextc(eccjsonparser_t *parse);
static ecctextstring_t eccjson_parsestring(eccjsonparser_t *parse);
static eccvalue_t eccjson_parseliteral(eccjsonparser_t *parse);
static eccvalue_t eccjson_parseobject(eccjsonparser_t *parse);
static eccvalue_t eccjson_parsearray(eccjsonparser_t *parse);
static eccvalue_t eccjson_runparser(eccjsonparser_t *parse);
static eccvalue_t eccjson_revive(eccjsonparser_t *parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value);
static eccvalue_t eccjson_itermore(eccjsonparser_t *parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value);
static eccvalue_t objjsonfn_parse(eccstate_t *context);
static eccvalue_t eccjson_replace(eccjsonstringify_t *stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value);
static int eccjson_stringify(eccjsonstringify_t *stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value, int isArray, int addComa);
static eccvalue_t objjsonfn_stringify(eccstate_t *context);
static void nsjsonfn_setup(void);
static void nsjsonfn_teardown(void);


const eccobjinterntype_t ECC_Type_Json = {
    .text = &ECC_ConstString_JsonType,
};

const struct eccpseudonsjson_t ECCNSJSON = {
    nsjsonfn_setup,
    nsjsonfn_teardown,
    {}
};

static eccvalue_t eccjson_error(eccjsonparser_t* parse, int length, ecccharbuffer_t* chars)
{
    return ECCNSValue.error(ECCNSError.syntaxError(ECCNSText.make(parse->text.bytes + (length < 0 ? length : 0), abs(length)), chars));
}

static ecctextstring_t eccjson_errorOfLine(eccjsonparser_t* parse)
{
    ecctextstring_t text = { 0 };
    text.bytes = parse->start;
    while(parse->text.length)
    {
        ecctextchar_t c = ECCNSText.character(parse->text);
        if(c.codepoint == '\r' || c.codepoint == '\n')
            break;

        ECCNSText.advance(&parse->text, 1);
    }
    text.length = (int32_t)(parse->text.bytes - parse->start);
    return text;
}

static ecctextchar_t eccjson_nextc(eccjsonparser_t* parse)
{
    ecctextchar_t c = { 0 };
    while(parse->text.length)
    {
        c = ECCNSText.nextCharacter(&parse->text);

        if(c.codepoint == '\r' && ECCNSText.character(parse->text).codepoint == '\n')
            ECCNSText.advance(&parse->text, 1);

        if(c.codepoint == '\r' || c.codepoint == '\n')
        {
            parse->start = parse->text.bytes;
            ++parse->line;
        }

        if(!isspace(c.codepoint))
            break;
    }
    return c;
}

static ecctextstring_t eccjson_parsestring(eccjsonparser_t* parse)
{
    ecctextstring_t text = { parse->text.bytes, 0, 0 };
    ecctextchar_t c;
    do
    {
        c = ECCNSText.nextCharacter(&parse->text);
        if(c.codepoint == '\\')
            ECCNSText.advance(&parse->text, 1);
    } while(c.codepoint != '\"' && parse->text.length);

    text.length = (int32_t)(parse->text.bytes - text.bytes - 1);
    return text;
}

static eccvalue_t eccjson_parseliteral(eccjsonparser_t* parse)
{
    ecctextchar_t c = eccjson_nextc(parse);

    switch(c.codepoint)
    {
        case 't':
            if(!memcmp(parse->text.bytes, "rue", 3))
            {
                ECCNSText.advance(&parse->text, 3);
                return ECCValConstTrue;
            }
            break;

        case 'f':
            if(!memcmp(parse->text.bytes, "alse", 4))
            {
                ECCNSText.advance(&parse->text, 4);
                return ECCValConstFalse;
            }
            break;

        case 'n':
            if(!memcmp(parse->text.bytes, "ull", 3))
            {
                ECCNSText.advance(&parse->text, 3);
                return ECCValConstNull;
            }
            break;

        case '-':
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
            ecctextstring_t text = ECCNSText.make(parse->text.bytes - 1, 1);

            if(c.codepoint == '-')
                c = ECCNSText.nextCharacter(&parse->text);

            if(c.codepoint != '0')
                while(parse->text.length)
                {
                    c = ECCNSText.nextCharacter(&parse->text);
                    if(!isdigit(c.codepoint))
                        break;
                }
            else
                c = ECCNSText.nextCharacter(&parse->text);

            if(c.codepoint == '.')
            {
                while(parse->text.length)
                {
                    c = ECCNSText.nextCharacter(&parse->text);
                    if(!isdigit(c.codepoint))
                        break;
                }
            }

            if(c.codepoint == 'e' || c.codepoint == 'E')
            {
                c = ECCNSText.nextCharacter(&parse->text);
                if(c.codepoint == '+' || c.codepoint == '-')
                    c = ECCNSText.nextCharacter(&parse->text);

                while(parse->text.length)
                {
                    c = ECCNSText.nextCharacter(&parse->text);
                    if(!isdigit(c.codepoint))
                        break;
                }
            }

            ECCNSText.advance(&parse->text, -c.units);
            text.length = (int32_t)(parse->text.bytes - text.bytes);

            return ECCNSLexer.scanBinary(text, 0);
        }

        case '"':
        {
            ecctextstring_t text = eccjson_parsestring(parse);
            return ECCNSValue.chars(ECCNSChars.createWithBytes(text.length, text.bytes));
            break;
        }

        case '{':
            return eccjson_parseobject(parse);
            break;

        case '[':
            return eccjson_parsearray(parse);
            break;
    }
    return eccjson_error(parse, -c.units, ECCNSChars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
}

static eccvalue_t eccjson_parseobject(eccjsonparser_t* parse)
{
    eccobject_t* object = ECCNSObject.create(ECC_Prototype_Object);
    ecctextchar_t c;
    eccvalue_t value;
    eccindexkey_t key;

    c = eccjson_nextc(parse);
    if(c.codepoint != '}')
        for(;;)
        {
            if(c.codepoint != '"')
                return eccjson_error(parse, -c.units, ECCNSChars.create("expect property name"));

            key = ECCNSKey.makeWithText(eccjson_parsestring(parse), ECC_INDEXFLAG_COPYONCREATE);

            c = eccjson_nextc(parse);
            if(c.codepoint != ':')
                return eccjson_error(parse, -c.units, ECCNSChars.create("expect colon"));

            value = eccjson_parseliteral(parse);
            if(value.type == ECC_VALTYPE_ERROR)
                return value;

            ECCNSObject.addMember(object, key, value, 0);

            c = eccjson_nextc(parse);

            if(c.codepoint == '}')
                break;
            else if(c.codepoint == ',')
                c = eccjson_nextc(parse);
            else
                return eccjson_error(parse, -c.units, ECCNSChars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
        }

    return ECCNSValue.object(object);
}

static eccvalue_t eccjson_parsearray(eccjsonparser_t* parse)
{
    eccobject_t* object = ECCNSArray.create();
    ecctextchar_t c;
    eccvalue_t value;

    for(;;)
    {
        value = eccjson_parseliteral(parse);
        if(value.type == ECC_VALTYPE_ERROR)
            return value;

        ECCNSObject.addElement(object, object->elementCount, value, 0);

        c = eccjson_nextc(parse);

        if(c.codepoint == ',')
            continue;

        if(c.codepoint == ']')
            break;

        return eccjson_error(parse, -c.units, ECCNSChars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
    }
    return ECCNSValue.object(object);
}

static eccvalue_t eccjson_runparser(eccjsonparser_t* parse)
{
    ecctextchar_t c = eccjson_nextc(parse);

    if(c.codepoint == '{')
        return eccjson_parseobject(parse);
    else if(c.codepoint == '[')
        return eccjson_parsearray(parse);

    return eccjson_error(parse, -c.units, ECCNSChars.create("expect { or ["));
}

// MARK: - Static Members

static eccvalue_t eccjson_revive(eccjsonparser_t* parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value)
{
    uint16_t hashmapCount;

    hashmapCount = parse->context.environment->hashmapCount;
    switch(hashmapCount)
    {
        default:
            {
                memcpy(parse->context.environment->hashmap + 5, parse->function->environment.hashmap, sizeof(*parse->context.environment->hashmap) * (hashmapCount - 5));
            }
            /* fallthrough */
        case 5:
            {
                parse->context.environment->hashmap[3 + 1].value = value;
            }
            /* fallthrough */
        case 4:
            {
                parse->context.environment->hashmap[3 + 0].value = property;
            }
            /* fallthrough */
        case 3:
            break;
        case 2:
        case 1:
        case 0:
            assert(0);
            break;
    }

    parse->context.ops = parse->ops;
    parse->context.thisvalue = thisval;
    parse->arguments->element[0].value = property;
    parse->arguments->element[1].value = value;
    return parse->context.ops->native(&parse->context);
}

static eccvalue_t eccjson_itermore(eccjsonparser_t* parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value)
{
    uint32_t index, count;
    eccappendbuffer_t chars;

    if(ECCNSValue.isObject(value))
    {
        eccobject_t* object = value.data.object;

        for(index = 0, count = object->elementCount < io_libecc_object_ElementMax ? object->elementCount : io_libecc_object_ElementMax; index < count; ++index)
        {
            if(object->element[index].value.check == 1)
            {
                ECCNSChars.beginAppend(&chars);
                ECCNSChars.append(&chars, "%d", index);
                object->element[index].value = eccjson_itermore(parse, thisval, ECCNSChars.endAppend(&chars), object->element[index].value);
            }
        }

        for(index = 2; index < object->hashmapCount; ++index)
        {
            if(object->hashmap[index].value.check == 1)
                object->hashmap[index].value = eccjson_itermore(parse, thisval, ECCNSValue.key(object->hashmap[index].value.key), object->hashmap[index].value);
        }
    }
    return eccjson_revive(parse, thisval, property, value);
}

static eccvalue_t objjsonfn_parse(eccstate_t* context)
{
    eccvalue_t value, reviver, result;
    eccjsonparser_t parse = {
		.context = {
			.parent = context,
			.ecc = context->ecc,
			.depth = context->depth + 1,
			.textIndex = ECC_CTXINDEXTYPE_CALL,
		},
	};

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    reviver = ECCNSContext.argument(context, 1);

    parse.text = ECCNSText.make(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value));
    parse.start = parse.text.bytes;
    parse.line = 1;
    parse.function = reviver.type == ECC_VALTYPE_FUNCTION ? reviver.data.function : NULL;
    parse.ops = parse.function ? parse.function->oplist->ops : NULL;

    result = eccjson_runparser(&parse);

    if(result.type != ECC_VALTYPE_ERROR && parse.text.length)
    {
        ecctextchar_t c = ECCNSText.character(parse.text);
        result = eccjson_error(&parse, c.units, ECCNSChars.create("unexpected '%.*s'", c.units, parse.text.bytes));
    }

    if(result.type == ECC_VALTYPE_ERROR)
    {
        ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_NO);
        context->ecc->ofLine = parse.line;
        context->ecc->ofText = eccjson_errorOfLine(&parse);
        context->ecc->ofInput = "(parse)";
        ECCNSContext.doThrow(context, result);
    }

    if(parse.function && parse.function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ECCNSObject.copy(&parse.function->environment);

        parse.context.environment = environment;
        parse.arguments = ECCNSArguments.createSized(2);
        ++parse.arguments->referenceCount;

        environment->hashmap[2].value = ECCNSValue.object(parse.arguments);

        result = eccjson_itermore(&parse, result, ECCNSValue.text(&ECC_ConstString_Empty), result);
    }
    else if(parse.function)
    {
        eccobject_t environment = parse.function->environment;
        eccobject_t arguments = ECCNSObject.identity;
        ecchashmap_t hashmap[parse.function->environment.hashmapCapacity];
        eccobjelement_t element[2];

        memcpy(hashmap, parse.function->environment.hashmap, sizeof(hashmap));
        parse.context.environment = &environment;
        parse.arguments = &arguments;

        arguments.element = element;
        arguments.elementCount = 2;
        environment.hashmap = hashmap;
        environment.hashmap[2].value = ECCNSValue.object(&arguments);

        result = eccjson_itermore(&parse, result, ECCNSValue.text(&ECC_ConstString_Empty), result);
    }
    return result;
}

static eccvalue_t eccjson_replace(eccjsonstringify_t* stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value)
{
    uint16_t hashmapCount;

    hashmapCount = stringify->context.environment->hashmapCount;
    switch(hashmapCount)
    {
        default:
            {
                memcpy(stringify->context.environment->hashmap + 5, stringify->function->environment.hashmap,
                   sizeof(*stringify->context.environment->hashmap) * (hashmapCount - 5));
            }
            /* fallthrough */
        case 5:
            {
                stringify->context.environment->hashmap[3 + 1].value = value;
            }
            /* fallthrough */
        case 4:
            {
                stringify->context.environment->hashmap[3 + 0].value = property;
            }
            /* fallthrough */
        case 3:
            break;
        case 2:
        case 1:
        case 0:
            assert(0);
            break;
    }

    stringify->context.ops = stringify->ops;
    stringify->context.thisvalue = thisval;
    stringify->arguments->element[0].value = property;
    stringify->arguments->element[1].value = value;
    return stringify->context.ops->native(&stringify->context);
}

static int eccjson_stringify(eccjsonstringify_t* stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value, int isArray, int addComa)
{
    uint32_t index, count;

    if(stringify->function)
        value = eccjson_replace(stringify, thisval, property, value);

    if(!isArray)
    {
        if(value.type == ECC_VALTYPE_UNDEFINED)
            return 0;

        if(stringify->filter)
        {
            eccobject_t* object = stringify->filter;
            int found = 0;

            for(index = 0, count = object->elementCount < io_libecc_object_ElementMax ? object->elementCount : io_libecc_object_ElementMax; index < count; ++index)
            {
                if(object->element[index].value.check == 1)
                {
                    if(ECCNSValue.isTrue(ECCNSValue.equals(&stringify->context, property, object->element[index].value)))
                    {
                        found = 1;
                        break;
                    }
                }
            }

            if(!found)
                return 0;
        }
    }

    if(addComa)
        ECCNSChars.append(&stringify->chars, ",%s", strlen(stringify->spaces) ? "\n" : "");

    for(index = 0, count = stringify->level; index < count; ++index)
        ECCNSChars.append(&stringify->chars, "%s", stringify->spaces);

    if(!isArray)
        ECCNSChars.append(&stringify->chars, "\"%.*s\":%s", ECCNSValue.stringLength(&property), ECCNSValue.stringBytes(&property),
                               strlen(stringify->spaces) ? " " : "");

    if(value.type == ECC_VALTYPE_FUNCTION || value.type == ECC_VALTYPE_UNDEFINED)
        ECCNSChars.append(&stringify->chars, "null");
    else if(ECCNSValue.isObject(value))
    {
        eccobject_t* object = value.data.object;
        int subisarr = ECCNSValue.objectIsArray(object);
        eccappendbuffer_t chars;
        const ecctextstring_t* strprop;
        int hasValue = 0;

        ECCNSChars.append(&stringify->chars, "%s%s", subisarr ? "[" : "{", strlen(stringify->spaces) ? "\n" : "");
        ++stringify->level;

        for(index = 0, count = object->elementCount < io_libecc_object_ElementMax ? object->elementCount : io_libecc_object_ElementMax; index < count; ++index)
        {
            if(object->element[index].value.check == 1)
            {
                ECCNSChars.beginAppend(&chars);
                ECCNSChars.append(&chars, "%d", index);
                hasValue |= eccjson_stringify(stringify, value, ECCNSChars.endAppend(&chars), object->element[index].value, subisarr, hasValue);
            }
        }

        if(!subisarr)
        {
            for(index = 0; index < object->hashmapCount; ++index)
            {
                if(object->hashmap[index].value.check == 1)
                {
                    strprop = ECCNSKey.textOf(object->hashmap[index].value.key);
                    hasValue |= eccjson_stringify(stringify, value, ECCNSValue.text(strprop), object->hashmap[index].value, subisarr, hasValue);
                }
            }
        }

        ECCNSChars.append(&stringify->chars, "%s", strlen(stringify->spaces) ? "\n" : "");

        --stringify->level;
        for(index = 0, count = stringify->level; index < count; ++index)
            ECCNSChars.append(&stringify->chars, "%s", stringify->spaces);

        ECCNSChars.append(&stringify->chars, "%s", subisarr ? "]" : "}");
    }
    else
        ECCNSChars.appendValue(&stringify->chars, &stringify->context, value);

    return 1;
}

static eccvalue_t objjsonfn_stringify(eccstate_t* context)
{
    eccvalue_t value, replacer, space;
    eccjsonstringify_t stringify = {
		.context = {
			.parent = context,
			.ecc = context->ecc,
			.depth = context->depth + 1,
			.textIndex = ECC_CTXINDEXTYPE_CALL,
		},
		.spaces = { 0 },
	};

    value = ECCNSContext.argument(context, 0);
    replacer = ECCNSContext.argument(context, 1);
    space = ECCNSContext.argument(context, 2);

    stringify.filter = replacer.type == ECC_VALTYPE_OBJECT && replacer.data.object->type == &ECC_Type_Array ? replacer.data.object : NULL;
    stringify.function = replacer.type == ECC_VALTYPE_FUNCTION ? replacer.data.function : NULL;
    stringify.ops = stringify.function ? stringify.function->oplist->ops : NULL;

    if(ECCNSValue.isString(space))
        snprintf(stringify.spaces, sizeof(stringify.spaces), "%.*s", (int)ECCNSValue.stringLength(&space), ECCNSValue.stringBytes(&space));
    else if(ECCNSValue.isNumber(space))
    {
        int i = ECCNSValue.toInteger(context, space).data.integer;

        if(i < 0)
            i = 0;

        if(i > 10)
            i = 10;

        while(i--)
            strcat(stringify.spaces, " ");
    }

    ECCNSChars.beginAppend(&stringify.chars);

    if(stringify.function && stringify.function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ECCNSObject.copy(&stringify.function->environment);

        stringify.context.environment = environment;
        stringify.arguments = ECCNSArguments.createSized(2);
        ++stringify.arguments->referenceCount;

        environment->hashmap[2].value = ECCNSValue.object(stringify.arguments);

        eccjson_stringify(&stringify, value, ECCNSValue.text(&ECC_ConstString_Empty), value, 1, 0);
    }
    else if(stringify.function)
    {
        eccobject_t environment = stringify.function->environment;
        eccobject_t arguments = ECCNSObject.identity;
        ecchashmap_t hashmap[stringify.function->environment.hashmapCapacity];
        eccobjelement_t element[2];

        memcpy(hashmap, stringify.function->environment.hashmap, sizeof(hashmap));
        stringify.context.environment = &environment;
        stringify.arguments = &arguments;

        arguments.element = element;
        arguments.elementCount = 2;
        environment.hashmap = hashmap;
        environment.hashmap[2].value = ECCNSValue.object(&arguments);

        eccjson_stringify(&stringify, value, ECCNSValue.text(&ECC_ConstString_Empty), value, 1, 0);
    }
    else
        eccjson_stringify(&stringify, value, ECCNSValue.text(&ECC_ConstString_Empty), value, 1, 0);

    return ECCNSChars.endAppend(&stringify.chars);
}

// MARK: - Public

eccobject_t* ECC_Prototype_JSONObject = NULL;

static void nsjsonfn_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ECC_Prototype_JSONObject = ECCNSObject.createTyped(&ECC_Type_Json);

    ECCNSFunction.addToObject(ECC_Prototype_JSONObject, "parse", objjsonfn_parse, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_JSONObject, "stringify", objjsonfn_stringify, -1, h);
}

static void nsjsonfn_teardown(void)
{
    ECC_Prototype_JSONObject = NULL;
}
