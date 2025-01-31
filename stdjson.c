
/*
//  json.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

typedef struct eccjsonparser_t eccjsonparser_t;
typedef struct eccjsonstringify_t eccjsonstringify_t;

struct eccjsonparser_t
{
    eccstrbox_t text;
    const char* start;
    int line;
    /* reviver */
    ecccontext_t context;
    eccobjfunction_t* function;
    eccobject_t* arguments;
    const eccoperand_t* ops;
};

struct eccjsonstringify_t
{
    eccappbuf_t chars;
    char spaces[11];
    int level;
    eccobject_t* filter;
    /* replacer */
    ecccontext_t context;
    eccobjfunction_t* function;
    eccobject_t* arguments;
    const eccoperand_t* ops;
};

static eccvalue_t ecc_json_error(eccjsonparser_t *parse, int length, eccstrbuffer_t *chars);
static eccstrbox_t ecc_json_errorofline(eccjsonparser_t *parse);
static eccrune_t ecc_json_nextc(eccjsonparser_t *parse);
static eccstrbox_t ecc_json_parsestring(eccjsonparser_t *parse);
static eccvalue_t ecc_json_parseliteral(eccjsonparser_t *parse);
static eccvalue_t ecc_json_parseobject(eccjsonparser_t *parse);
static eccvalue_t ecc_json_parsearray(eccjsonparser_t *parse);
static eccvalue_t ecc_json_runparser(eccjsonparser_t *parse);
static eccvalue_t ecc_json_revive(eccjsonparser_t *parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value);
static eccvalue_t ecc_json_itermore(eccjsonparser_t *parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value);
static eccvalue_t ecc_objfnjson_parse(ecccontext_t *context);
static eccvalue_t ecc_json_replace(eccjsonstringify_t *stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value);
static int ecc_json_stringify(eccjsonstringify_t *stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value, int isArray, int addComa);
static eccvalue_t ecc_objfnjson_stringify(ecccontext_t *context);

const eccobjinterntype_t ECC_Type_Json = {
    .text = &ECC_String_JsonType,
};

static eccvalue_t ecc_json_error(eccjsonparser_t* parse, int length, eccstrbuffer_t* chars)
{
    return ecc_value_error(ecc_error_syntaxerror(ecc_strbox_make(parse->text.bytes + (length < 0 ? length : 0), abs(length)), chars));
}

static eccstrbox_t ecc_json_errorofline(eccjsonparser_t* parse)
{
    eccstrbox_t text = { 0 };
    text.bytes = parse->start;
    while(parse->text.length)
    {
        eccrune_t c = ecc_strbox_character(parse->text);
        if(c.codepoint == '\r' || c.codepoint == '\n')
            break;

        ecc_strbox_advance(&parse->text, 1);
    }
    text.length = (int32_t)(parse->text.bytes - parse->start);
    return text;
}

static eccrune_t ecc_json_nextc(eccjsonparser_t* parse)
{
    eccrune_t c = { 0 };
    while(parse->text.length)
    {
        c = ecc_strbox_nextcharacter(&parse->text);

        if(c.codepoint == '\r' && ecc_strbox_character(parse->text).codepoint == '\n')
            ecc_strbox_advance(&parse->text, 1);

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

static eccstrbox_t ecc_json_parsestring(eccjsonparser_t* parse)
{
    eccstrbox_t text = { parse->text.bytes, 0, 0 };
    eccrune_t c;
    do
    {
        c = ecc_strbox_nextcharacter(&parse->text);
        if(c.codepoint == '\\')
            ecc_strbox_advance(&parse->text, 1);
    } while(c.codepoint != '\"' && parse->text.length);

    text.length = (int32_t)(parse->text.bytes - text.bytes - 1);
    return text;
}

static eccvalue_t ecc_json_parseliteral(eccjsonparser_t* parse)
{
    eccrune_t c = ecc_json_nextc(parse);

    switch(c.codepoint)
    {
        case 't':
            if(!memcmp(parse->text.bytes, "rue", 3))
            {
                ecc_strbox_advance(&parse->text, 3);
                return ECCValConstTrue;
            }
            break;

        case 'f':
            if(!memcmp(parse->text.bytes, "alse", 4))
            {
                ecc_strbox_advance(&parse->text, 4);
                return ECCValConstFalse;
            }
            break;

        case 'n':
            if(!memcmp(parse->text.bytes, "ull", 3))
            {
                ecc_strbox_advance(&parse->text, 3);
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
            eccstrbox_t text = ecc_strbox_make(parse->text.bytes - 1, 1);

            if(c.codepoint == '-')
                c = ecc_strbox_nextcharacter(&parse->text);

            if(c.codepoint != '0')
                while(parse->text.length)
                {
                    c = ecc_strbox_nextcharacter(&parse->text);
                    if(!isdigit(c.codepoint))
                        break;
                }
            else
                c = ecc_strbox_nextcharacter(&parse->text);

            if(c.codepoint == '.')
            {
                while(parse->text.length)
                {
                    c = ecc_strbox_nextcharacter(&parse->text);
                    if(!isdigit(c.codepoint))
                        break;
                }
            }

            if(c.codepoint == 'e' || c.codepoint == 'E')
            {
                c = ecc_strbox_nextcharacter(&parse->text);
                if(c.codepoint == '+' || c.codepoint == '-')
                    c = ecc_strbox_nextcharacter(&parse->text);

                while(parse->text.length)
                {
                    c = ecc_strbox_nextcharacter(&parse->text);
                    if(!isdigit(c.codepoint))
                        break;
                }
            }

            ecc_strbox_advance(&parse->text, -c.units);
            text.length = (int32_t)(parse->text.bytes - text.bytes);

            return ecc_astlex_scanbinary(text, 0);
        }

        case '"':
        {
            eccstrbox_t text = ecc_json_parsestring(parse);
            return ecc_value_fromchars(ecc_strbuf_createwithbytes(text.length, text.bytes));
            break;
        }

        case '{':
            return ecc_json_parseobject(parse);
            break;

        case '[':
            return ecc_json_parsearray(parse);
            break;
    }
    return ecc_json_error(parse, -c.units, ecc_strbuf_create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
}

static eccvalue_t ecc_json_parseobject(eccjsonparser_t* parse)
{
    eccobject_t* object = ecc_object_create(ECC_Prototype_Object);
    eccrune_t c;
    eccvalue_t value;
    eccindexkey_t key;

    c = ecc_json_nextc(parse);
    if(c.codepoint != '}')
        for(;;)
        {
            if(c.codepoint != '"')
                return ecc_json_error(parse, -c.units, ecc_strbuf_create("expect property name"));

            key = ecc_keyidx_makewithtext(ecc_json_parsestring(parse), ECC_INDEXFLAG_COPYONCREATE);

            c = ecc_json_nextc(parse);
            if(c.codepoint != ':')
                return ecc_json_error(parse, -c.units, ecc_strbuf_create("expect colon"));

            value = ecc_json_parseliteral(parse);
            if(value.type == ECC_VALTYPE_ERROR)
                return value;

            ecc_object_addmember(object, key, value, 0);

            c = ecc_json_nextc(parse);

            if(c.codepoint == '}')
                break;
            else if(c.codepoint == ',')
                c = ecc_json_nextc(parse);
            else
                return ecc_json_error(parse, -c.units, ecc_strbuf_create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
        }

    return ecc_value_object(object);
}

static eccvalue_t ecc_json_parsearray(eccjsonparser_t* parse)
{
    eccobject_t* object = ecc_array_create();
    eccrune_t c;
    eccvalue_t value;

    for(;;)
    {
        value = ecc_json_parseliteral(parse);
        if(value.type == ECC_VALTYPE_ERROR)
            return value;

        ecc_object_addelement(object, object->hmapitemcount, value, 0);

        c = ecc_json_nextc(parse);

        if(c.codepoint == ',')
            continue;

        if(c.codepoint == ']')
            break;

        return ecc_json_error(parse, -c.units, ecc_strbuf_create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
    }
    return ecc_value_object(object);
}

static eccvalue_t ecc_json_runparser(eccjsonparser_t* parse)
{
    eccrune_t c = ecc_json_nextc(parse);

    if(c.codepoint == '{')
        return ecc_json_parseobject(parse);
    else if(c.codepoint == '[')
        return ecc_json_parsearray(parse);

    return ecc_json_error(parse, -c.units, ecc_strbuf_create("expect { or ["));
}

static eccvalue_t ecc_json_revive(eccjsonparser_t* parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value)
{
    uint32_t hashmapCount;

    hashmapCount = parse->context.execenv->hmapmapcount;
    switch(hashmapCount)
    {
        default:
            {
                memcpy(parse->context.execenv->hmapmapitems + 5, parse->function->funcenv.hmapmapitems, sizeof(*parse->context.execenv->hmapmapitems) * (hashmapCount - 5));
            }
            /* fallthrough */
        case 5:
            {
                parse->context.execenv->hmapmapitems[3 + 1].hmapmapvalue = value;
            }
            /* fallthrough */
        case 4:
            {
                parse->context.execenv->hmapmapitems[3 + 0].hmapmapvalue = property;
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
    parse->arguments->hmapitemitems[0].hmapitemvalue = property;
    parse->arguments->hmapitemitems[1].hmapitemvalue = value;
    return parse->context.ops->native(&parse->context);
}

static eccvalue_t ecc_json_itermore(eccjsonparser_t* parse, eccvalue_t thisval, eccvalue_t property, eccvalue_t value)
{
    uint32_t index, count;
    eccappbuf_t chars;

    if(ecc_value_isobject(value))
    {
        eccobject_t* object = value.data.object;

        for(index = 0, count = object->hmapitemcount < ECC_CONF_MAXELEMENTS ? object->hmapitemcount : ECC_CONF_MAXELEMENTS; index < count; ++index)
        {
            if(object->hmapitemitems[index].hmapitemvalue.check == 1)
            {
                ecc_strbuf_beginappend(&chars);
                ecc_strbuf_append(&chars, "%d", index);
                object->hmapitemitems[index].hmapitemvalue = ecc_json_itermore(parse, thisval, ecc_strbuf_endappend(&chars), object->hmapitemitems[index].hmapitemvalue);
            }
        }

        for(index = 2; index < object->hmapmapcount; ++index)
        {
            if(object->hmapmapitems[index].hmapmapvalue.check == 1)
                object->hmapmapitems[index].hmapmapvalue = ecc_json_itermore(parse, thisval, ecc_value_fromkey(object->hmapmapitems[index].hmapmapvalue.key), object->hmapmapitems[index].hmapmapvalue);
        }
    }
    return ecc_json_revive(parse, thisval, property, value);
}

static eccvalue_t ecc_objfnjson_parse(ecccontext_t* context)
{
    eccvalue_t value, reviver, result;
    eccjsonparser_t parse = {};
    parse.context.parent = context;
    parse.context.ecc = context->ecc;
    parse.context.depth = context->depth + 1;
    parse.context.ctxtextindex = ECC_CTXINDEXTYPE_CALL;
    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    reviver = ecc_context_argument(context, 1);
    parse.text = ecc_strbox_make(ecc_value_stringbytes(&value), ecc_value_stringlength(&value));
    parse.start = parse.text.bytes;
    parse.line = 1;
    parse.function = reviver.type == ECC_VALTYPE_FUNCTION ? reviver.data.function : NULL;
    parse.ops = parse.function ? parse.function->oplist->ops : NULL;
    result = ecc_json_runparser(&parse);
    if(result.type != ECC_VALTYPE_ERROR && parse.text.length)
    {
        eccrune_t c = ecc_strbox_character(parse.text);
        result = ecc_json_error(&parse, c.units, ecc_strbuf_create("unexpected '%.*s'", c.units, parse.text.bytes));
    }
    if(result.type == ECC_VALTYPE_ERROR)
    {
        ecc_context_settextindex(context, ECC_CTXINDEXTYPE_NO);
        context->ecc->ofLine = parse.line;
        context->ecc->ofText = ecc_json_errorofline(&parse);
        context->ecc->ofInput = "(parse)";
        ecc_context_throw(context, result);
    }

    if(parse.function && parse.function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ecc_object_copy(&parse.function->funcenv);
        parse.context.execenv = environment;
        parse.arguments = ecc_args_createsized(2);
        ++parse.arguments->refcount;
        environment->hmapmapitems[2].hmapmapvalue = ecc_value_object(parse.arguments);
        result = ecc_json_itermore(&parse, result, ecc_value_fromtext(&ECC_String_Empty), result);
    }
    else if(parse.function)
    {
        eccobject_t environment = parse.function->funcenv;
        eccobject_t arguments;
        memset(&arguments, 0, sizeof(eccobject_t));
        ecchashmap_t hashmap[parse.function->funcenv.hmapmapcapacity];
        ecchashitem_t element[2];
        memcpy(hashmap, parse.function->funcenv.hmapmapitems, sizeof(hashmap));
        parse.context.execenv = &environment;
        parse.arguments = &arguments;
        arguments.hmapitemitems = element;
        arguments.hmapitemcount = 2;
        environment.hmapmapitems = hashmap;
        environment.hmapmapitems[2].hmapmapvalue = ecc_value_object(&arguments);
        result = ecc_json_itermore(&parse, result, ecc_value_fromtext(&ECC_String_Empty), result);
    }
    return result;
}

static eccvalue_t ecc_json_replace(eccjsonstringify_t* stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value)
{
    uint32_t hashmapCount;

    hashmapCount = stringify->context.execenv->hmapmapcount;
    switch(hashmapCount)
    {
        default:
            {
                memcpy(stringify->context.execenv->hmapmapitems + 5, stringify->function->funcenv.hmapmapitems,
                   sizeof(*stringify->context.execenv->hmapmapitems) * (hashmapCount - 5));
            }
            /* fallthrough */
        case 5:
            {
                stringify->context.execenv->hmapmapitems[3 + 1].hmapmapvalue = value;
            }
            /* fallthrough */
        case 4:
            {
                stringify->context.execenv->hmapmapitems[3 + 0].hmapmapvalue = property;
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
    stringify->arguments->hmapitemitems[0].hmapitemvalue = property;
    stringify->arguments->hmapitemitems[1].hmapitemvalue = value;
    return stringify->context.ops->native(&stringify->context);
}

static int ecc_json_stringify(eccjsonstringify_t* stringify, eccvalue_t thisval, eccvalue_t property, eccvalue_t value, int isArray, int addComa)
{
    uint32_t index, count;

    if(stringify->function)
        value = ecc_json_replace(stringify, thisval, property, value);

    if(!isArray)
    {
        if(value.type == ECC_VALTYPE_UNDEFINED)
            return 0;

        if(stringify->filter)
        {
            eccobject_t* object = stringify->filter;
            int found = 0;

            for(index = 0, count = object->hmapitemcount < ECC_CONF_MAXELEMENTS ? object->hmapitemcount : ECC_CONF_MAXELEMENTS; index < count; ++index)
            {
                if(object->hmapitemitems[index].hmapitemvalue.check == 1)
                {
                    if(ecc_value_istrue(ecc_value_equals(&stringify->context, property, object->hmapitemitems[index].hmapitemvalue)))
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
        ecc_strbuf_append(&stringify->chars, ",%s", strlen(stringify->spaces) ? "\n" : "");

    for(index = 0, count = stringify->level; index < count; ++index)
        ecc_strbuf_append(&stringify->chars, "%s", stringify->spaces);

    if(!isArray)
        ecc_strbuf_append(&stringify->chars, "\"%.*s\":%s", ecc_value_stringlength(&property), ecc_value_stringbytes(&property),
                               strlen(stringify->spaces) ? " " : "");

    if(value.type == ECC_VALTYPE_FUNCTION || value.type == ECC_VALTYPE_UNDEFINED)
        ecc_strbuf_append(&stringify->chars, "null");
    else if(ecc_value_isobject(value))
    {
        eccobject_t* object = value.data.object;
        int subisarr = ecc_value_objectisarray(object);
        eccappbuf_t chars;
        const eccstrbox_t* strprop;
        int hasValue = 0;

        ecc_strbuf_append(&stringify->chars, "%s%s", subisarr ? "[" : "{", strlen(stringify->spaces) ? "\n" : "");
        ++stringify->level;

        for(index = 0, count = object->hmapitemcount < ECC_CONF_MAXELEMENTS ? object->hmapitemcount : ECC_CONF_MAXELEMENTS; index < count; ++index)
        {
            if(object->hmapitemitems[index].hmapitemvalue.check == 1)
            {
                ecc_strbuf_beginappend(&chars);
                ecc_strbuf_append(&chars, "%d", index);
                hasValue |= ecc_json_stringify(stringify, value, ecc_strbuf_endappend(&chars), object->hmapitemitems[index].hmapitemvalue, subisarr, hasValue);
            }
        }

        if(!subisarr)
        {
            for(index = 0; index < object->hmapmapcount; ++index)
            {
                if(object->hmapmapitems[index].hmapmapvalue.check == 1)
                {
                    strprop = ecc_keyidx_textof(object->hmapmapitems[index].hmapmapvalue.key);
                    hasValue |= ecc_json_stringify(stringify, value, ecc_value_fromtext(strprop), object->hmapmapitems[index].hmapmapvalue, subisarr, hasValue);
                }
            }
        }

        ecc_strbuf_append(&stringify->chars, "%s", strlen(stringify->spaces) ? "\n" : "");

        --stringify->level;
        for(index = 0, count = stringify->level; index < count; ++index)
            ecc_strbuf_append(&stringify->chars, "%s", stringify->spaces);

        ecc_strbuf_append(&stringify->chars, "%s", subisarr ? "]" : "}");
    }
    else
        ecc_strbuf_appendvalue(&stringify->chars, &stringify->context, value);

    return 1;
}

static eccvalue_t ecc_objfnjson_stringify(ecccontext_t* context)
{
    eccvalue_t value, replacer, space;
    eccjsonstringify_t stringify = {};
    stringify.context.parent = context;
    stringify.context.ecc = context->ecc;
    stringify.context.depth = context->depth + 1;
    stringify.context.ctxtextindex = ECC_CTXINDEXTYPE_CALL;
    memset(stringify.spaces, 0, sizeof(stringify.spaces ));

    value = ecc_context_argument(context, 0);
    replacer = ecc_context_argument(context, 1);
    space = ecc_context_argument(context, 2);

    stringify.filter = replacer.type == ECC_VALTYPE_OBJECT && replacer.data.object->type == &ECC_Type_Array ? replacer.data.object : NULL;
    stringify.function = replacer.type == ECC_VALTYPE_FUNCTION ? replacer.data.function : NULL;
    stringify.ops = stringify.function ? stringify.function->oplist->ops : NULL;

    if(ecc_value_isstring(space))
        snprintf(stringify.spaces, sizeof(stringify.spaces), "%.*s", (int)ecc_value_stringlength(&space), ecc_value_stringbytes(&space));
    else if(ecc_value_isnumber(space))
    {
        int i = ecc_value_tointeger(context, space).data.integer;

        if(i < 0)
            i = 0;

        if(i > 10)
            i = 10;

        while(i--)
            strcat(stringify.spaces, " ");
    }

    ecc_strbuf_beginappend(&stringify.chars);

    if(stringify.function && stringify.function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ecc_object_copy(&stringify.function->funcenv);

        stringify.context.execenv = environment;
        stringify.arguments = ecc_args_createsized(2);
        ++stringify.arguments->refcount;

        environment->hmapmapitems[2].hmapmapvalue = ecc_value_object(stringify.arguments);

        ecc_json_stringify(&stringify, value, ecc_value_fromtext(&ECC_String_Empty), value, 1, 0);
    }
    else if(stringify.function)
    {
        eccobject_t environment = stringify.function->funcenv;
        eccobject_t arguments;
        memset(&arguments, 0, sizeof(eccobject_t));
        ecchashmap_t hashmap[stringify.function->funcenv.hmapmapcapacity];
        ecchashitem_t element[2];

        memcpy(hashmap, stringify.function->funcenv.hmapmapitems, sizeof(hashmap));
        stringify.context.execenv = &environment;
        stringify.arguments = &arguments;

        arguments.hmapitemitems = element;
        arguments.hmapitemcount = 2;
        environment.hmapmapitems = hashmap;
        environment.hmapmapitems[2].hmapmapvalue = ecc_value_object(&arguments);

        ecc_json_stringify(&stringify, value, ecc_value_fromtext(&ECC_String_Empty), value, 1, 0);
    }
    else
        ecc_json_stringify(&stringify, value, ecc_value_fromtext(&ECC_String_Empty), value, 1, 0);

    return ecc_strbuf_endappend(&stringify.chars);
}

eccobject_t* ECC_Prototype_JSONObject = NULL;

void ecc_json_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ECC_Prototype_JSONObject = ecc_object_createtyped(&ECC_Type_Json);

    ecc_function_addto(ECC_Prototype_JSONObject, "parse", ecc_objfnjson_parse, -1, h);
    ecc_function_addto(ECC_Prototype_JSONObject, "stringify", ecc_objfnjson_stringify, -1, h);
}

void ecc_json_teardown(void)
{
    ECC_Prototype_JSONObject = NULL;
}
