//
//  parser.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

int ecc_astparse_previewtoken(eccastparser_t* self)
{
    return self->currenttoken;
}

int ecc_astparse_nexttoken(eccastparser_t* self)
{
    if(self->currenttoken != ECC_TOK_ERROR)
    {
        self->currenttoken = ecc_astlex_nexttoken(self->lexer);

        if(self->currenttoken == ECC_TOK_ERROR)
            self->error = self->lexer->tokenvalue.data.error;
    }
    return self->currenttoken;
}

void ecc_astparse_parseerror(eccastparser_t* self, eccobjerror_t* error)
{
    if(!self->error)
    {
        self->error = error;
        self->currenttoken = ECC_TOK_ERROR;
    }
}

void ecc_astparse_syntaxerror(eccastparser_t* self, ecctextstring_t text, eccstrbuffer_t* message)
{
    ecc_astparse_parseerror(self, ecc_error_syntaxerror(text, message));
}

void ecc_astparse_referenceerror(eccastparser_t* self, ecctextstring_t text, eccstrbuffer_t* message)
{
    ecc_astparse_parseerror(self, ecc_error_referenceerror(text, message));
}

eccoplist_t* ecc_astparse_tokenerror(eccastparser_t* self, const char* t)
{
    char b[4];

    if(!ecc_astparse_previewtoken(self) || ecc_astparse_previewtoken(self) >= ECC_TOK_ERROR)
        ecc_astparse_syntaxerror(self, self->lexer->text, ecc_strbuf_create("expected %s, got %s", t, ecc_astlex_tokenchars(ecc_astparse_previewtoken(self), b)));
    else
        ecc_astparse_syntaxerror(self, self->lexer->text, ecc_strbuf_create("expected %s, got '%.*s'", t, self->lexer->text.length, self->lexer->text.bytes));

    return NULL;
}

int ecc_astparse_accepttoken(eccastparser_t* self, int token)
{
    if(ecc_astparse_previewtoken(self) != token)
        return 0;

    ecc_astparse_nexttoken(self);
    return 1;
}

int ecc_astparse_expecttoken(eccastparser_t* self, int token)
{
    if(ecc_astparse_previewtoken(self) != token)
    {
        char b[4];
        const char* type = ecc_astlex_tokenchars(token, b);
        ecc_astparse_tokenerror(self, type);
        return 0;
    }

    ecc_astparse_nexttoken(self);
    return 1;
}

// MARK: Depth

void ecc_astparse_pushdepth(eccastparser_t* self, eccindexkey_t key, char depth)
{
    self->depthlistvals = (eccastdepths_t*)realloc(self->depthlistvals, (self->depthlistcount + 1) * sizeof(*self->depthlistvals));
    self->depthlistvals[self->depthlistcount].key = key;
    self->depthlistvals[self->depthlistcount].depth = depth;
    ++self->depthlistcount;
}

void ecc_astparse_popdepth(eccastparser_t* self)
{
    --self->depthlistcount;
}

// MARK: Expression

eccoplist_t* ecc_astparse_foldconstant(eccastparser_t* self, eccoplist_t* oplist)
{
    eccstate_t ecc = {};
    ecc.sloppyMode = self->lexer->allowUnicodeOutsideLiteral;
    ecccontext_t context = {};
    context.ops = oplist->ops;
    context.ecc = &ecc;
    eccvalue_t value = context.ops->native(&context);
    ecctextstring_t text = ecc_oplist_text(oplist);
    ecc_oplist_destroy(oplist);
    return ecc_oplist_create(ecc_oper_value, value, text);
}

eccoplist_t* ecc_astparse_usebinary(eccastparser_t* self, eccoplist_t* oplist, int add)
{
    ecccontext_t context = {};
    eccstate_t ecc = {};
    if(oplist && oplist->ops[0].native == ecc_oper_value && (ecc_value_isnumber(oplist->ops[0].value) || !add))
    {
        ecc.sloppyMode = self->lexer->allowUnicodeOutsideLiteral;
        context.ops = oplist->ops;
        context.ecc = &ecc;
        oplist->ops[0].value = ecc_value_tobinary(&context, oplist->ops[0].value);
    }
    return oplist;
}

eccoplist_t* ecc_astparse_useinteger(eccastparser_t* self, eccoplist_t* oplist)
{
    ecccontext_t context = {};
    eccstate_t ecc = {};
    if(oplist && oplist->ops[0].native == ecc_oper_value)
    {
        ecc.sloppyMode = self->lexer->allowUnicodeOutsideLiteral;
        context.ops = oplist->ops;
        context.ecc = &ecc;
        oplist->ops[0].value = ecc_value_tointeger(&context, oplist->ops[0].value);
    }
    return oplist;
}

eccoplist_t* ecc_astparse_expressionref(eccastparser_t* self, eccoplist_t* oplist, const char* name)
{
    if(!oplist)
        return NULL;

    if(oplist->ops[0].native == ecc_oper_getlocal && oplist->count == 1)
    {
        if(oplist->ops[0].value.type == ECC_VALTYPE_KEY)
        {
            if(ecc_keyidx_isequal(oplist->ops[0].value.data.key, ECC_ConstKey_eval))
                ecc_astparse_syntaxerror(self, ecc_oplist_text(oplist), ecc_strbuf_create(name));
            else if(ecc_keyidx_isequal(oplist->ops[0].value.data.key, ECC_ConstKey_arguments))
                ecc_astparse_syntaxerror(self, ecc_oplist_text(oplist), ecc_strbuf_create(name));
        }

        oplist->ops[0].native = ecc_oper_getlocalref;
    }
    else if(oplist->ops[0].native == ecc_oper_getmember)
        oplist->ops[0].native = ecc_oper_getmemberref;
    else if(oplist->ops[0].native == ecc_oper_getproperty)
        oplist->ops[0].native = ecc_oper_getpropertyref;
    else
        ecc_astparse_referenceerror(self, ecc_oplist_text(oplist), ecc_strbuf_create("%s", name));

    return oplist;
}

void ecc_astparse_semicolon(eccastparser_t* self)
{
    if(ecc_astparse_previewtoken(self) == ';')
    {
        ecc_astparse_nexttoken(self);
        return;
    }
    else if(self->lexer->didLineBreak || ecc_astparse_previewtoken(self) == '}' || ecc_astparse_previewtoken(self) == ECC_TOK_NO)
        return;

    ecc_astparse_syntaxerror(self, self->lexer->text, ecc_strbuf_create("missing ; before statement"));
}

eccoperand_t ecc_astparse_identifier(eccastparser_t* self)
{
    eccvalue_t value = self->lexer->tokenvalue;
    ecctextstring_t text = self->lexer->text;
    if(!ecc_astparse_expecttoken(self, ECC_TOK_IDENTIFIER))
        return (eccoperand_t){ 0 };

    return ecc_oper_make(ecc_oper_value, value, text);
}

eccoplist_t* ecc_astparse_array(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    uint32_t count = 0;
    ecctextstring_t text = self->lexer->text;

    ecc_astparse_nexttoken(self);

    do
    {
        while(ecc_astparse_previewtoken(self) == ',')
        {
            ++count;
            oplist = ecc_oplist_append(oplist, ecc_oper_make(ecc_oper_value, ECCValConstNone, self->lexer->text));
            ecc_astparse_nexttoken(self);
        }

        if(ecc_astparse_previewtoken(self) == ']')
            break;

        ++count;
        oplist = ecc_oplist_join(oplist, ecc_astparse_assignment(self, 0));
    } while(ecc_astparse_accepttoken(self, ','));

    text = ecc_textbuf_join(text, self->lexer->text);
    ecc_astparse_expecttoken(self, ']');

    return ecc_oplist_unshift(ecc_oper_make(ecc_oper_array, ecc_value_fromint(count), text), oplist);
}

eccoplist_t* ecc_astparse_propertyassignment(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    int isGetter = 0, isSetter = 0;

    if(ecc_astparse_previewtoken(self) == ECC_TOK_IDENTIFIER)
    {
        if(ecc_keyidx_isequal(self->lexer->tokenvalue.data.key, ECC_ConstKey_get))
        {
            ecc_astparse_nexttoken(self);
            if(ecc_astparse_previewtoken(self) == ':')
            {
                oplist = ecc_oplist_create(ecc_oper_value, ecc_value_fromkey(ECC_ConstKey_get), self->lexer->text);
                goto skipProperty;
            }
            else
                isGetter = 1;
        }
        else if(ecc_keyidx_isequal(self->lexer->tokenvalue.data.key, ECC_ConstKey_set))
        {
            ecc_astparse_nexttoken(self);
            if(ecc_astparse_previewtoken(self) == ':')
            {
                oplist = ecc_oplist_create(ecc_oper_value, ecc_value_fromkey(ECC_ConstKey_set), self->lexer->text);
                goto skipProperty;
            }
            else
                isSetter = 1;
        }
    }

    if(ecc_astparse_previewtoken(self) == ECC_TOK_INTEGER)
        oplist = ecc_oplist_create(ecc_oper_value, self->lexer->tokenvalue, self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_BINARY)
        oplist = ecc_oplist_create(ecc_oper_value, ecc_value_fromkey(ecc_keyidx_makewithtext(self->lexer->text, 0)), self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_STRING)
    {
        uint32_t element = ecc_astlex_scanelement(self->lexer->text);
        if(element < UINT32_MAX)
            oplist = ecc_oplist_create(ecc_oper_value, ecc_value_fromint(element), self->lexer->text);
        else
            oplist = ecc_oplist_create(ecc_oper_value, ecc_value_fromkey(ecc_keyidx_makewithtext(self->lexer->text, 0)), self->lexer->text);
    }
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_ESCAPEDSTRING)
    {
        /*
        * TODO: escapes in strings in object literals are ***BROKEN***
        */
        size_t len;
        uint32_t element;
        const char* bufp;
        eccvalue_t kval;
        eccindexkey_t kkey;
        ecctextstring_t text;
        #if 0
            bufp = self->lexer->tokenvalue.data.chars->bytes; 
            len = self->lexer->tokenvalue.data.chars->length;
        #else
            bufp = self->lexer->tokenvalue.data.buffer;
            len = strlen(bufp);
        #endif
        text = ecc_textbuf_make(bufp, len);
        element = ecc_astlex_scanelement(text);
        if(element < UINT32_MAX)
        {
            oplist = ecc_oplist_create(ecc_oper_value, ecc_value_fromint(element), self->lexer->text);
        }
        else
        {
            #if 0
                kkey = ecc_keyidx_makewithtext(*self->lexer->tokenvalue.data.text, 0);
            #else
                kkey = ecc_keyidx_makewithcstring(bufp);
            #endif
            kval = ecc_value_fromkey(kkey);
            oplist = ecc_oplist_create(ecc_oper_value, kval, self->lexer->text);
        }
    }
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_IDENTIFIER)
        oplist = ecc_oplist_create(ecc_oper_value, self->lexer->tokenvalue, self->lexer->text);
    else
    {
        ecc_astparse_expecttoken(self, ECC_TOK_IDENTIFIER);
        return NULL;
    }

    ecc_astparse_nexttoken(self);

    if(isGetter)
        return ecc_oplist_join(oplist, ecc_astparse_function(self, 0, 1, 0));
    else if(isSetter)
        return ecc_oplist_join(oplist, ecc_astparse_function(self, 0, 0, 1));

skipProperty:
    ecc_astparse_expecttoken(self, ':');
    return ecc_oplist_join(oplist, ecc_astparse_assignment(self, 0));
}

eccoplist_t* ecc_astparse_object(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    uint32_t count = 0;
    ecctextstring_t text = self->lexer->text;

    do
    {
        self->lexer->disallowKeyword = 1;
        ecc_astparse_nexttoken(self);
        self->lexer->disallowKeyword = 0;

        if(ecc_astparse_previewtoken(self) == '}')
            break;

        ++count;
        oplist = ecc_oplist_join(oplist, ecc_astparse_propertyassignment(self));
    } while(ecc_astparse_previewtoken(self) == ',');

    text = ecc_textbuf_join(text, self->lexer->text);
    ecc_astparse_expecttoken(self, '}');

    return ecc_oplist_unshift(ecc_oper_make(ecc_oper_object, ecc_value_fromint(count), text), oplist);
}

eccoplist_t* ecc_astparse_primary(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;

    if(ecc_astparse_previewtoken(self) == ECC_TOK_IDENTIFIER)
    {
        oplist = ecc_oplist_create(ecc_oper_getlocal, self->lexer->tokenvalue, self->lexer->text);

        if(ecc_keyidx_isequal(self->lexer->tokenvalue.data.key, ECC_ConstKey_arguments))
            self->function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_NEEDHEAP;
    }
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_STRING)
        oplist = ecc_oplist_create(ecc_oper_text, ECCValConstUndefined, self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_ESCAPEDSTRING)
        oplist = ecc_oplist_create(ecc_oper_value, self->lexer->tokenvalue, self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_BINARY)
        oplist = ecc_oplist_create(ecc_oper_value, self->lexer->tokenvalue, self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_INTEGER)
    {
        if(self->preferInteger)
            oplist = ecc_oplist_create(ecc_oper_value, self->lexer->tokenvalue, self->lexer->text);
        else
            oplist = ecc_oplist_create(ecc_oper_value, ecc_value_fromfloat(self->lexer->tokenvalue.data.integer), self->lexer->text);
    }
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_THIS)
        oplist = ecc_oplist_create(ecc_oper_getthis, ECCValConstUndefined, self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_NULL)
        oplist = ecc_oplist_create(ecc_oper_value, ECCValConstNull, self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_TRUE)
        oplist = ecc_oplist_create(ecc_oper_value, ecc_value_truth(1), self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_FALSE)
        oplist = ecc_oplist_create(ecc_oper_value, ecc_value_truth(0), self->lexer->text);
    else if(ecc_astparse_previewtoken(self) == '{')
        return ecc_astparse_object(self);
    else if(ecc_astparse_previewtoken(self) == '[')
        return ecc_astparse_array(self);
    else if(ecc_astparse_accepttoken(self, '('))
    {
        oplist = ecc_astparse_expression(self, 0);
        ecc_astparse_expecttoken(self, ')');
        return oplist;
    }
    else
    {
        if(self->lexer->text.bytes[0] == '/')
        {
            self->lexer->allowRegex = 1;
            self->lexer->offset -= self->lexer->text.length;
            ecc_astparse_nexttoken(self);
            self->lexer->allowRegex = 0;

            if(ecc_astparse_previewtoken(self) != ECC_TOK_REGEXP)
                ecc_astparse_tokenerror(self, "ECCNSRegExp");
        }

        if(ecc_astparse_previewtoken(self) == ECC_TOK_REGEXP)
            oplist = ecc_oplist_create(ecc_oper_regexp, ECCValConstUndefined, self->lexer->text);
        else
            return NULL;
    }

    ecc_astparse_nexttoken(self);

    return oplist;
}

eccoplist_t* ecc_astparse_arguments(eccastparser_t* self, int* count)
{
    eccoplist_t *oplist = NULL, *argumentOps;
    *count = 0;
    if(ecc_astparse_previewtoken(self) != ')')
        do
        {
            argumentOps = ecc_astparse_assignment(self, 0);
            if(!argumentOps)
                ecc_astparse_tokenerror(self, "expression");

            ++*count;
            oplist = ecc_oplist_join(oplist, argumentOps);
        } while(ecc_astparse_accepttoken(self, ','));

    return oplist;
}

eccoplist_t* ecc_astparse_member(eccastparser_t* self)
{
    eccoplist_t* oplist = ecc_astparse_new(self);
    ecctextstring_t text;
    while(1)
    {
        if(ecc_astparse_previewtoken(self) == '.')
        {
            eccvalue_t value;

            self->lexer->disallowKeyword = 1;
            ecc_astparse_nexttoken(self);
            self->lexer->disallowKeyword = 0;

            value = self->lexer->tokenvalue;
            text = ecc_textbuf_join(ecc_oplist_text(oplist), self->lexer->text);
            if(!ecc_astparse_expecttoken(self, ECC_TOK_IDENTIFIER))
                return oplist;

            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_getmember, value, text), oplist);
        }
        else if(ecc_astparse_accepttoken(self, '['))
        {
            oplist = ecc_oplist_join(oplist, ecc_astparse_expression(self, 0));
            text = ecc_textbuf_join(ecc_oplist_text(oplist), self->lexer->text);
            if(!ecc_astparse_expecttoken(self, ']'))
                return oplist;

            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_getproperty, ECCValConstUndefined, text), oplist);
        }
        else
            break;
    }
    return oplist;
}

eccoplist_t* ecc_astparse_new(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    ecctextstring_t text = self->lexer->text;

    if(ecc_astparse_accepttoken(self, ECC_TOK_NEW))
    {
        int count = 0;
        oplist = ecc_astparse_member(self);
        text = ecc_textbuf_join(text, ecc_oplist_text(oplist));
        if(ecc_astparse_accepttoken(self, '('))
        {
            oplist = ecc_oplist_join(oplist, ecc_astparse_arguments(self, &count));
            text = ecc_textbuf_join(text, self->lexer->text);
            ecc_astparse_expecttoken(self, ')');
        }
        return ecc_oplist_unshift(ecc_oper_make(ecc_oper_construct, ecc_value_fromint(count), text), oplist);
    }
    else if(ecc_astparse_previewtoken(self) == ECC_TOK_FUNCTION)
        return ecc_astparse_function(self, 0, 0, 0);
    else
        return ecc_astparse_primary(self);
}

eccoplist_t* ecc_astparse_lefthandside(eccastparser_t* self)
{
    eccoplist_t* oplist = ecc_astparse_new(self);
    ecctextstring_t text = ecc_oplist_text(oplist);
    eccvalue_t value;

    while(1)
    {
        if(ecc_astparse_previewtoken(self) == '.')
        {
            if(!oplist)
            {
                ecc_astparse_tokenerror(self, "expression");
                return oplist;
            }

            self->lexer->disallowKeyword = 1;
            ecc_astparse_nexttoken(self);
            self->lexer->disallowKeyword = 0;

            value = self->lexer->tokenvalue;
            text = ecc_textbuf_join(ecc_oplist_text(oplist), self->lexer->text);
            if(!ecc_astparse_expecttoken(self, ECC_TOK_IDENTIFIER))
                return oplist;

            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_getmember, value, text), oplist);
        }
        else if(ecc_astparse_accepttoken(self, '['))
        {
            oplist = ecc_oplist_join(oplist, ecc_astparse_expression(self, 0));
            text = ecc_textbuf_join(ecc_oplist_text(oplist), self->lexer->text);
            if(!ecc_astparse_expecttoken(self, ']'))
                return oplist;

            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_getproperty, ECCValConstUndefined, text), oplist);
        }
        else if(ecc_astparse_accepttoken(self, '('))
        {
            int count = 0;

            int isEval = oplist->count == 1 && oplist->ops[0].native == ecc_oper_getlocal && ecc_keyidx_isequal(oplist->ops[0].value.data.key, ECC_ConstKey_eval);
            if(isEval)
            {
                text = ecc_textbuf_join(ecc_oplist_text(oplist), self->lexer->text);
                ecc_oplist_destroy(oplist), oplist = NULL;
            }

            oplist = ecc_oplist_join(oplist, ecc_astparse_arguments(self, &count));
            text = ecc_textbuf_join(ecc_textbuf_join(text, ecc_oplist_text(oplist)), self->lexer->text);

            if(isEval)
                oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_eval, ecc_value_fromint(count), text), oplist);
            else if(oplist->ops->native == ecc_oper_getmember)
                oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_callmember, ecc_value_fromint(count), text), oplist);
            else if(oplist->ops->native == ecc_oper_getproperty)
                oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_callproperty, ecc_value_fromint(count), text), oplist);
            else
                oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_call, ecc_value_fromint(count), text), oplist);

            if(!ecc_astparse_expecttoken(self, ')'))
                break;
        }
        else
            break;
    }
    return oplist;
}

eccoplist_t* ecc_astparse_postfix(eccastparser_t* self)
{
    eccoplist_t* oplist = ecc_astparse_lefthandside(self);
    ecctextstring_t text = self->lexer->text;

    if(!self->lexer->didLineBreak && ecc_astparse_accepttoken(self, ECC_TOK_INCREMENT))
        oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_postincrementref, ECCValConstUndefined, ecc_textbuf_join(oplist->ops->text, text)),
                                          ecc_astparse_expressionref(self, oplist, "invalid increment operand"));
    if(!self->lexer->didLineBreak && ecc_astparse_accepttoken(self, ECC_TOK_DECREMENT))
        oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_postdecrementref, ECCValConstUndefined, ecc_textbuf_join(oplist->ops->text, text)),
                                          ecc_astparse_expressionref(self, oplist, "invalid decrement operand"));

    return oplist;
}

eccoplist_t* ecc_astparse_unary(eccastparser_t* self)
{
    eccoplist_t *oplist, *alt;
    ecctextstring_t text = self->lexer->text;
    eccnativefuncptr_t native;

    if(ecc_astparse_accepttoken(self, ECC_TOK_DELETE))
    {
        oplist = ecc_astparse_unary(self);

        if(oplist && oplist->ops[0].native == ecc_oper_getlocal)
        {
            if(self->strictMode)
                ecc_astparse_syntaxerror(self, ecc_oplist_text(oplist), ecc_strbuf_create("delete of an unqualified identifier"));

            oplist->ops->native = ecc_oper_deletelocal;
        }
        else if(oplist && oplist->ops[0].native == ecc_oper_getmember)
            oplist->ops->native = ecc_oper_deletemember;
        else if(oplist && oplist->ops[0].native == ecc_oper_getproperty)
            oplist->ops->native = ecc_oper_deleteproperty;
        else if(!self->strictMode && oplist)
            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_exchange, ECCValConstTrue, ECC_ConstString_Empty), oplist);
        else if(oplist)
            ecc_astparse_referenceerror(self, ecc_oplist_text(oplist), ecc_strbuf_create("invalid delete operand"));
        else
            ecc_astparse_tokenerror(self, "expression");

        return oplist;
    }
    else if(ecc_astparse_accepttoken(self, ECC_TOK_VOID))
        native = ecc_oper_exchange, alt = ecc_astparse_unary(self);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_TYPEOF))
    {
        native = ecc_oper_typeof, alt = ecc_astparse_unary(self);
        if(alt->ops->native == ecc_oper_getlocal)
            alt->ops->native = ecc_oper_getlocalrefornull;
    }
    else if(ecc_astparse_accepttoken(self, ECC_TOK_INCREMENT))
        native = ecc_oper_incrementref, alt = ecc_astparse_expressionref(self, ecc_astparse_unary(self), "invalid increment operand");
    else if(ecc_astparse_accepttoken(self, ECC_TOK_DECREMENT))
        native = ecc_oper_decrementref, alt = ecc_astparse_expressionref(self, ecc_astparse_unary(self), "invalid decrement operand");
    else if(ecc_astparse_accepttoken(self, '+'))
        native = ecc_oper_positive, alt = ecc_astparse_usebinary(self, ecc_astparse_unary(self), 0);
    else if(ecc_astparse_accepttoken(self, '-'))
        native = ecc_oper_negative, alt = ecc_astparse_usebinary(self, ecc_astparse_unary(self), 0);
    else if(ecc_astparse_accepttoken(self, '~'))
        native = ecc_oper_invert, alt = ecc_astparse_useinteger(self, ecc_astparse_unary(self));
    else if(ecc_astparse_accepttoken(self, '!'))
        native = ecc_oper_logicalnot, alt = ecc_astparse_unary(self);
    else
        return ecc_astparse_postfix(self);

    if(!alt)
        return ecc_astparse_tokenerror(self, "expression");

    oplist = ecc_oplist_unshift(ecc_oper_make(native, ECCValConstUndefined, ecc_textbuf_join(text, alt->ops->text)), alt);

    if(oplist->ops[1].native == ecc_oper_value)
        return ecc_astparse_foldconstant(self, oplist);
    else
        return oplist;
}

eccoplist_t* ecc_astparse_multiplicative(eccastparser_t* self)
{
    eccoplist_t *oplist = ecc_astparse_unary(self), *alt;

    while(1)
    {
        eccnativefuncptr_t native;

        if(ecc_astparse_previewtoken(self) == '*')
            native = ecc_oper_multiply;
        else if(ecc_astparse_previewtoken(self) == '/')
            native = ecc_oper_divide;
        else if(ecc_astparse_previewtoken(self) == '%')
            native = ecc_oper_modulo;
        else
            return oplist;

        if(ecc_astparse_usebinary(self, oplist, 0))
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_usebinary(self, ecc_astparse_unary(self), 0)))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == ecc_oper_value && oplist->ops[2].native == ecc_oper_value)
                    oplist = ecc_astparse_foldconstant(self, oplist);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
}

eccoplist_t* ecc_astparse_additive(eccastparser_t* self)
{
    eccoplist_t *oplist = ecc_astparse_multiplicative(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(ecc_astparse_previewtoken(self) == '+')
            native = ecc_oper_add;
        else if(ecc_astparse_previewtoken(self) == '-')
            native = ecc_oper_minus;
        else
            return oplist;

        if(ecc_astparse_usebinary(self, oplist, native == ecc_oper_add))
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_usebinary(self, ecc_astparse_multiplicative(self), native == ecc_oper_add)))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == ecc_oper_value && oplist->ops[2].native == ecc_oper_value)
                    oplist = ecc_astparse_foldconstant(self, oplist);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
}

eccoplist_t* ecc_astparse_shift(eccastparser_t* self)
{
    eccoplist_t *oplist = ecc_astparse_additive(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(ecc_astparse_previewtoken(self) == ECC_TOK_LEFTSHIFT)
            native = ecc_oper_leftshift;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_RIGHTSHIFT)
            native = ecc_oper_rightshift;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_UNSIGNEDRIGHTSHIFT)
            native = ecc_oper_unsignedrightshift;
        else
            return oplist;

        if(ecc_astparse_useinteger(self, oplist))
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_useinteger(self, ecc_astparse_additive(self))))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == ecc_oper_value && oplist->ops[2].native == ecc_oper_value)
                    oplist = ecc_astparse_foldconstant(self, oplist);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
}

eccoplist_t* ecc_astparse_relational(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = ecc_astparse_shift(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(ecc_astparse_previewtoken(self) == '<')
            native = ecc_oper_less;
        else if(ecc_astparse_previewtoken(self) == '>')
            native = ecc_oper_more;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_LESSOREQUAL)
            native = ecc_oper_lessorequal;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_MOREOREQUAL)
            native = ecc_oper_moreorequal;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_INSTANCEOF)
            native = ecc_oper_instanceof;
        else if(!noIn && ecc_astparse_previewtoken(self) == ECC_TOK_IN)
            native = ecc_oper_in;
        else
            return oplist;

        if(oplist)
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_shift(self)))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(native, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
}

eccoplist_t* ecc_astparse_equality(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = ecc_astparse_relational(self, noIn), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(ecc_astparse_previewtoken(self) == ECC_TOK_EQUAL)
            native = ecc_oper_equal;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_NOTEQUAL)
            native = ecc_oper_notequal;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_IDENTICAL)
            native = ecc_oper_identical;
        else if(ecc_astparse_previewtoken(self) == ECC_TOK_NOTIDENTICAL)
            native = ecc_oper_notidentical;
        else
            return oplist;

        if(oplist)
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_relational(self, noIn)))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(native, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
}

eccoplist_t* ecc_astparse_bitwiseand(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = ecc_astparse_equality(self, noIn), *alt;
    while(ecc_astparse_previewtoken(self) == '&')
    {
        if(ecc_astparse_useinteger(self, oplist))
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_useinteger(self, ecc_astparse_equality(self, noIn))))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(ecc_oper_bitwiseand, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
    return oplist;
}

eccoplist_t* ecc_astparse_bitwisexor(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = ecc_astparse_bitwiseand(self, noIn), *alt;
    while(ecc_astparse_previewtoken(self) == '^')
    {
        if(ecc_astparse_useinteger(self, oplist))
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_useinteger(self, ecc_astparse_bitwiseand(self, noIn))))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(ecc_oper_bitwisexor, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
    return oplist;
}

eccoplist_t* ecc_astparse_bitwiseor(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = ecc_astparse_bitwisexor(self, noIn), *alt;
    while(ecc_astparse_previewtoken(self) == '|')
    {
        if(ecc_astparse_useinteger(self, oplist))
        {
            ecc_astparse_nexttoken(self);
            if((alt = ecc_astparse_useinteger(self, ecc_astparse_bitwisexor(self, noIn))))
            {
                ecctextstring_t text = ecc_textbuf_join(oplist->ops->text, alt->ops->text);
                oplist = ecc_oplist_unshiftjoin(ecc_oper_make(ecc_oper_bitwiseor, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ecc_oplist_destroy(oplist);
        }
        return ecc_astparse_tokenerror(self, "expression");
    }
    return oplist;
}

eccoplist_t* ecc_astparse_logicaland(eccastparser_t* self, int noIn)
{
    int32_t opCount;
    eccoplist_t *oplist = ecc_astparse_bitwiseor(self, noIn), *nextOp = NULL;

    while(ecc_astparse_accepttoken(self, ECC_TOK_LOGICALAND))
        if(oplist && (nextOp = ecc_astparse_bitwiseor(self, noIn)))
        {
            opCount = nextOp->count;
            oplist = ecc_oplist_unshiftjoin(ecc_oper_make(ecc_oper_logicaland, ecc_value_fromint(opCount), ecc_oplist_text(oplist)), oplist, nextOp);
        }
        else
            ecc_astparse_tokenerror(self, "expression");

    return oplist;
}

eccoplist_t* ecc_astparse_logicalor(eccastparser_t* self, int noIn)
{
    int32_t opCount;
    eccoplist_t *oplist = ecc_astparse_logicaland(self, noIn), *nextOp = NULL;

    while(ecc_astparse_accepttoken(self, ECC_TOK_LOGICALOR))
        if(oplist && (nextOp = ecc_astparse_logicaland(self, noIn)))
        {
            opCount = nextOp->count;
            oplist = ecc_oplist_unshiftjoin(ecc_oper_make(ecc_oper_logicalor, ecc_value_fromint(opCount), ecc_oplist_text(oplist)), oplist, nextOp);
        }
        else
            ecc_astparse_tokenerror(self, "expression");

    return oplist;
}

eccoplist_t* ecc_astparse_conditional(eccastparser_t* self, int noIn)
{
    eccoplist_t* oplist = ecc_astparse_logicalor(self, noIn);

    if(ecc_astparse_accepttoken(self, '?'))
    {
        if(oplist)
        {
            eccoplist_t* trueOps = ecc_astparse_assignment(self, 0);
            eccoplist_t* falseOps;

            if(!ecc_astparse_expecttoken(self, ':'))
                return oplist;

            falseOps = ecc_astparse_assignment(self, noIn);

            trueOps = ecc_oplist_append(trueOps, ecc_oper_make(ecc_oper_jump, ecc_value_fromint(falseOps->count), ecc_oplist_text(trueOps)));
            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_jumpifnot, ecc_value_fromint(trueOps->count), ecc_oplist_text(oplist)), oplist);
            oplist = ecc_oplist_join3(oplist, trueOps, falseOps);

            return oplist;
        }
        else
            ecc_astparse_tokenerror(self, "expression");
    }
    return oplist;
}

eccoplist_t* ecc_astparse_assignment(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = ecc_astparse_conditional(self, noIn), *opassign = NULL;
    ecctextstring_t text = self->lexer->text;
    eccnativefuncptr_t native = NULL;

    if(ecc_astparse_accepttoken(self, '='))
    {
        if(!oplist)
        {
            ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("expected expression, got '='"));
            return NULL;
        }
        else if(oplist->ops[0].native == ecc_oper_getlocal && oplist->count == 1)
        {
            if(ecc_keyidx_isequal(oplist->ops[0].value.data.key, ECC_ConstKey_eval))
                ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("can't assign to eval"));
            else if(ecc_keyidx_isequal(oplist->ops[0].value.data.key, ECC_ConstKey_arguments))
                ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("can't assign to arguments"));

            if(!self->strictMode && !ecc_object_member(&self->function->environment, oplist->ops[0].value.data.key, 0))
                ++self->reserveGlobalSlots;
            //				ecc_object_addmember(self->global, oplist->ops[0].value.data.key, ECCValConstNone, 0);

            oplist->ops->native = ecc_oper_setlocal;
        }
        else if(oplist->ops[0].native == ecc_oper_getmember)
            oplist->ops->native = ecc_oper_setmember;
        else if(oplist->ops[0].native == ecc_oper_getproperty)
            oplist->ops->native = ecc_oper_setproperty;
        else
            ecc_astparse_referenceerror(self, ecc_oplist_text(oplist), ecc_strbuf_create("invalid assignment left-hand side"));

        if((opassign = ecc_astparse_assignment(self, noIn)))
        {
            oplist->ops->text = ecc_textbuf_join(oplist->ops->text, opassign->ops->text);
            return ecc_oplist_join(oplist, opassign);
        }

        ecc_astparse_tokenerror(self, "expression");
    }
    else if(ecc_astparse_accepttoken(self, ECC_TOK_MULTIPLYASSIGN))
        native = ecc_oper_multiplyassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_DIVIDEASSIGN))
        native = ecc_oper_divideassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_MODULOASSIGN))
        native = ecc_oper_moduloassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_ADDASSIGN))
        native = ecc_oper_addassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_MINUSASSIGN))
        native = ecc_oper_minusassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_LEFTSHIFTASSIGN))
        native = ecc_oper_leftshiftassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_RIGHTSHIFTASSIGN))
        native = ecc_oper_rightshiftassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN))
        native = ecc_oper_unsignedrightshiftassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_ANDASSIGN))
        native = ecc_oper_bitandassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_XORASSIGN))
        native = ecc_oper_bitxorassignref;
    else if(ecc_astparse_accepttoken(self, ECC_TOK_ORASSIGN))
        native = ecc_oper_bitorassignref;
    else
        return oplist;

    if(oplist)
    {
        if((opassign = ecc_astparse_assignment(self, noIn)))
            oplist->ops->text = ecc_textbuf_join(oplist->ops->text, opassign->ops->text);
        else
            ecc_astparse_tokenerror(self, "expression");

        return ecc_oplist_unshiftjoin(ecc_oper_make(native, ECCValConstUndefined, oplist->ops->text),
                                            ecc_astparse_expressionref(self, oplist, "invalid assignment left-hand side"), opassign);
    }

    ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("expected expression, got '%.*s'", text.length, text.bytes));
    return NULL;
}

eccoplist_t* ecc_astparse_expression(eccastparser_t* self, int noIn)
{
    eccoplist_t* oplist = ecc_astparse_assignment(self, noIn);
    while(ecc_astparse_accepttoken(self, ','))
        oplist = ecc_oplist_unshiftjoin(ecc_oper_make(ecc_oper_discard, ECCValConstUndefined, ECC_ConstString_Empty), oplist, ecc_astparse_assignment(self, noIn));

    return oplist;
}

// MARK: Statements

eccoplist_t* ecc_astparse_statementlist(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *statementOps = NULL, *discardOps = NULL;
    uint16_t discardCount = 0;

    while(ecc_astparse_previewtoken(self) != ECC_TOK_ERROR && ecc_astparse_previewtoken(self) != ECC_TOK_NO)
    {
        if(ecc_astparse_previewtoken(self) == ECC_TOK_FUNCTION)
            self->function->oplist = ecc_oplist_join(self->function->oplist, ecc_astparse_function(self, 1, 0, 0));
        else
        {
            if((statementOps = ecc_astparse_statement(self)))
            {
                while(statementOps->count > 1 && statementOps->ops[0].native == ecc_oper_next)
                    ecc_oplist_shift(statementOps);

                if(statementOps->count == 1 && statementOps->ops[0].native == ecc_oper_next)
                    ecc_oplist_destroy(statementOps), statementOps = NULL;
                else
                {
                    if(statementOps->ops[0].native == ecc_oper_discard)
                    {
                        ++discardCount;
                        discardOps = ecc_oplist_join(discardOps, ecc_oplist_shift(statementOps));
                        statementOps = NULL;
                    }
                    else if(discardOps)
                    {
                        oplist = ecc_oplist_joindiscarded(oplist, discardCount, discardOps);
                        discardOps = NULL;
                        discardCount = 0;
                    }

                    oplist = ecc_oplist_join(oplist, statementOps);
                }
            }
            else
                break;
        }
    }

    if(discardOps)
        oplist = ecc_oplist_joindiscarded(oplist, discardCount, discardOps);

    return oplist;
}

eccoplist_t* ecc_astparse_block(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    ecc_astparse_expecttoken(self, '{');
    if(ecc_astparse_previewtoken(self) == '}')
        oplist = ecc_oplist_create(ecc_oper_next, ECCValConstUndefined, self->lexer->text);
    else
        oplist = ecc_astparse_statementlist(self);

    ecc_astparse_expecttoken(self, '}');
    return oplist;
}

eccoplist_t* ecc_astparse_variabledeclaration(eccastparser_t* self, int noIn)
{
    eccvalue_t value = self->lexer->tokenvalue;
    ecctextstring_t text = self->lexer->text;
    if(!ecc_astparse_expecttoken(self, ECC_TOK_IDENTIFIER))
        return NULL;

    if(self->strictMode && ecc_keyidx_isequal(value.data.key, ECC_ConstKey_eval))
        ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("redefining eval is not allowed"));
    else if(self->strictMode && ecc_keyidx_isequal(value.data.key, ECC_ConstKey_arguments))
        ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("redefining arguments is not allowed"));

    if(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE || self->sourcedepth > 1)
        ecc_object_addmember(&self->function->environment, value.data.key, ECCValConstUndefined, ECC_VALFLAG_SEALED);
    else
        ecc_object_addmember(self->global, value.data.key, ECCValConstUndefined, ECC_VALFLAG_SEALED);

    if(ecc_astparse_accepttoken(self, '='))
    {
        eccoplist_t* opassign = ecc_astparse_assignment(self, noIn);

        if(opassign)
            return ecc_oplist_unshiftjoin(ecc_oper_make(ecc_oper_discard, ECCValConstUndefined, ECC_ConstString_Empty),
                                                ecc_oplist_create(ecc_oper_setlocal, value, ecc_textbuf_join(text, opassign->ops->text)), opassign);

        ecc_astparse_tokenerror(self, "expression");
        return NULL;
    }
    //	else if (!(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE) && self->sourcedepth <= 1)
    //		return ecc_oplist_unshift(ecc_oper_make(ecc_oper_discard, ECCValConstUndefined, ECC_ConstString_Empty), ecc_oplist_create(ecc_oper_createlocalref, value, text));
    else
        return ecc_oplist_create(ecc_oper_next, value, text);
}

eccoplist_t* ecc_astparse_variabledeclarationlist(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = NULL, *varOps;
    do
    {
        varOps = ecc_astparse_variabledeclaration(self, noIn);

        if(oplist && varOps && varOps->count == 1 && varOps->ops[0].native == ecc_oper_next)
            ecc_oplist_destroy(varOps), varOps = NULL;
        else
            oplist = ecc_oplist_join(oplist, varOps);
    } while(ecc_astparse_accepttoken(self, ','));

    return oplist;
}

eccoplist_t* ecc_astparse_ifstatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *trueOps = NULL, *falseOps = NULL;
    ecc_astparse_expecttoken(self, '(');
    oplist = ecc_astparse_expression(self, 0);
    ecc_astparse_expecttoken(self, ')');
    trueOps = ecc_astparse_statement(self);
    if(!trueOps)
        trueOps = ecc_oplist_appendnoop(NULL);

    if(ecc_astparse_accepttoken(self, ECC_TOK_ELSE))
    {
        falseOps = ecc_astparse_statement(self);
        if(falseOps)
            trueOps = ecc_oplist_append(trueOps, ecc_oper_make(ecc_oper_jump, ecc_value_fromint(falseOps->count), ecc_oplist_text(trueOps)));
    }
    oplist = ecc_oplist_unshiftjoin3(ecc_oper_make(ecc_oper_jumpifnot, ecc_value_fromint(trueOps->count), ecc_oplist_text(oplist)), oplist,
                                           trueOps, falseOps);
    return oplist;
}

eccoplist_t* ecc_astparse_dostatement(eccastparser_t* self)
{
    eccoplist_t *oplist, *condition;

    ecc_astparse_pushdepth(self, ECC_ConstKey_none, 2);
    oplist = ecc_astparse_statement(self);
    ecc_astparse_popdepth(self);

    ecc_astparse_expecttoken(self, ECC_TOK_WHILE);
    ecc_astparse_expecttoken(self, '(');
    condition = ecc_astparse_expression(self, 0);
    ecc_astparse_expecttoken(self, ')');
    ecc_astparse_accepttoken(self, ';');

    return ecc_oplist_createloop(NULL, condition, NULL, oplist, 1);
}

eccoplist_t* ecc_astparse_whilestatement(eccastparser_t* self)
{
    eccoplist_t *oplist, *condition;

    ecc_astparse_expecttoken(self, '(');
    condition = ecc_astparse_expression(self, 0);
    ecc_astparse_expecttoken(self, ')');

    ecc_astparse_pushdepth(self, ECC_ConstKey_none, 2);
    oplist = ecc_astparse_statement(self);
    ecc_astparse_popdepth(self);

    return ecc_oplist_createloop(NULL, condition, NULL, oplist, 0);
}

eccoplist_t* ecc_astparse_forstatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *condition = NULL, *increment = NULL, *body = NULL;

    ecc_astparse_expecttoken(self, '(');

    self->preferInteger = 1;

    if(ecc_astparse_accepttoken(self, ECC_TOK_VAR))
        oplist = ecc_astparse_variabledeclarationlist(self, 1);
    else if(ecc_astparse_previewtoken(self) != ';')
    {
        oplist = ecc_astparse_expression(self, 1);

        if(oplist)
            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_discard, ECCValConstUndefined, ecc_oplist_text(oplist)), oplist);
    }

    if(oplist && ecc_astparse_accepttoken(self, ECC_TOK_IN))
    {
        if(oplist->count == 2 && oplist->ops[0].native == ecc_oper_discard && oplist->ops[1].native == ecc_oper_getlocal)
        {
            if(!self->strictMode && !ecc_object_member(&self->function->environment, oplist->ops[1].value.data.key, 0))
                ++self->reserveGlobalSlots;

            oplist->ops[0].native = ecc_oper_iterateinref;
            oplist->ops[1].native = ecc_oper_createlocalref;
        }
        else if(oplist->count == 1 && oplist->ops[0].native == ecc_oper_next)
        {
            oplist->ops->native = ecc_oper_createlocalref;
            oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_iterateinref, ECCValConstUndefined, self->lexer->text), oplist);
        }
        else
            ecc_astparse_referenceerror(self, ecc_oplist_text(oplist), ecc_strbuf_create("invalid for/in left-hand side"));

        oplist = ecc_oplist_join(oplist, ecc_astparse_expression(self, 0));
        oplist->ops[0].text = ecc_oplist_text(oplist);
        ecc_astparse_expecttoken(self, ')');

        self->preferInteger = 0;

        ecc_astparse_pushdepth(self, ECC_ConstKey_none, 2);
        body = ecc_astparse_statement(self);
        ecc_astparse_popdepth(self);

        body = ecc_oplist_appendnoop(body);
        return ecc_oplist_join(ecc_oplist_append(oplist, ecc_oper_make(ecc_oper_value, ecc_value_fromint(body->count), self->lexer->text)), body);
    }
    else
    {
        ecc_astparse_expecttoken(self, ';');
        if(ecc_astparse_previewtoken(self) != ';')
            condition = ecc_astparse_expression(self, 0);

        ecc_astparse_expecttoken(self, ';');
        if(ecc_astparse_previewtoken(self) != ')')
            increment = ecc_astparse_expression(self, 0);

        ecc_astparse_expecttoken(self, ')');

        self->preferInteger = 0;

        ecc_astparse_pushdepth(self, ECC_ConstKey_none, 2);
        body = ecc_astparse_statement(self);
        ecc_astparse_popdepth(self);

        return ecc_oplist_createloop(oplist, condition, increment, body, 0);
    }
}

eccoplist_t* ecc_astparse_continuestatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;
    eccindexkey_t label = ECC_ConstKey_none;
    ecctextstring_t labelText = self->lexer->text;
    uint16_t depth, lastestDepth, breaker = 0;

    if(!self->lexer->didLineBreak && ecc_astparse_previewtoken(self) == ECC_TOK_IDENTIFIER)
    {
        label = self->lexer->tokenvalue.data.key;
        ecc_astparse_nexttoken(self);
    }
    ecc_astparse_semicolon(self);

    depth = self->depthlistcount;
    if(!depth)
        ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("continue must be inside loop"));

    lastestDepth = 0;
    while(depth--)
    {
        breaker += self->depthlistvals[depth].depth;
        if(self->depthlistvals[depth].depth)
            lastestDepth = self->depthlistvals[depth].depth;

        if(lastestDepth == 2)
            if(ecc_keyidx_isequal(label, ECC_ConstKey_none) || ecc_keyidx_isequal(label, self->depthlistvals[depth].key))
                return ecc_oplist_create(ecc_oper_breaker, ecc_value_fromint(breaker - 1), self->lexer->text);
    }
    ecc_astparse_syntaxerror(self, labelText, ecc_strbuf_create("label not found"));
    return oplist;
}

eccoplist_t* ecc_astparse_breakstatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;
    eccindexkey_t label = ECC_ConstKey_none;
    ecctextstring_t labelText = self->lexer->text;
    uint16_t depth, breaker = 0;

    if(!self->lexer->didLineBreak && ecc_astparse_previewtoken(self) == ECC_TOK_IDENTIFIER)
    {
        label = self->lexer->tokenvalue.data.key;
        ecc_astparse_nexttoken(self);
    }
    ecc_astparse_semicolon(self);

    depth = self->depthlistcount;
    if(!depth)
        ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("break must be inside loop or switch"));

    while(depth--)
    {
        breaker += self->depthlistvals[depth].depth;
        if(ecc_keyidx_isequal(label, ECC_ConstKey_none) || ecc_keyidx_isequal(label, self->depthlistvals[depth].key))
            return ecc_oplist_create(ecc_oper_breaker, ecc_value_fromint(breaker), self->lexer->text);
    }
    ecc_astparse_syntaxerror(self, labelText, ecc_strbuf_create("label not found"));
    return oplist;
}

eccoplist_t* ecc_astparse_returnstatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;

    if(self->sourcedepth <= 1)
        ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("return not in function"));

    if(!self->lexer->didLineBreak && ecc_astparse_previewtoken(self) != ';' && ecc_astparse_previewtoken(self) != '}' && ecc_astparse_previewtoken(self) != ECC_TOK_NO)
        oplist = ecc_astparse_expression(self, 0);

    ecc_astparse_semicolon(self);

    if(!oplist)
        oplist = ecc_oplist_create(ecc_oper_value, ECCValConstUndefined, ecc_textbuf_join(text, self->lexer->text));

    oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_result, ECCValConstUndefined, ecc_textbuf_join(text, oplist->ops->text)), oplist);
    return oplist;
}

eccoplist_t* ecc_astparse_switchstatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *conditionOps = NULL, *defaultOps = NULL;
    ecctextstring_t text = ECC_ConstString_Empty;
    uint32_t conditionCount = 0;

    ecc_astparse_expecttoken(self, '(');
    conditionOps = ecc_astparse_expression(self, 0);
    ecc_astparse_expecttoken(self, ')');
    ecc_astparse_expecttoken(self, '{');
    ecc_astparse_pushdepth(self, ECC_ConstKey_none, 1);

    while(ecc_astparse_previewtoken(self) != '}' && ecc_astparse_previewtoken(self) != ECC_TOK_ERROR && ecc_astparse_previewtoken(self) != ECC_TOK_NO)
    {
        text = self->lexer->text;

        if(ecc_astparse_accepttoken(self, ECC_TOK_CASE))
        {
            conditionOps = ecc_oplist_join(conditionOps, ecc_astparse_expression(self, 0));
            conditionOps
            = ecc_oplist_append(conditionOps, ecc_oper_make(ecc_oper_value, ecc_value_fromint(2 + (oplist ? oplist->count : 0)), ECC_ConstString_Empty));
            ++conditionCount;
            ecc_astparse_expecttoken(self, ':');
            oplist = ecc_oplist_join(oplist, ecc_astparse_statementlist(self));
        }
        else if(ecc_astparse_accepttoken(self, ECC_TOK_DEFAULT))
        {
            if(!defaultOps)
            {
                defaultOps = ecc_oplist_create(ecc_oper_jump, ecc_value_fromint(1 + (oplist ? oplist->count : 0)), text);
                ecc_astparse_expecttoken(self, ':');
                oplist = ecc_oplist_join(oplist, ecc_astparse_statementlist(self));
            }
            else
                ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("more than one switch default"));
        }
        else
            ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("invalid switch statement"));
    }

    if(!defaultOps)
        defaultOps = ecc_oplist_create(ecc_oper_noop, ECCValConstNone, ECC_ConstString_Empty);

    oplist = ecc_oplist_appendnoop(oplist);
    defaultOps = ecc_oplist_append(defaultOps, ecc_oper_make(ecc_oper_jump, ecc_value_fromint(oplist ? oplist->count : 0), ECC_ConstString_Empty));
    conditionOps = ecc_oplist_unshiftjoin(
    ecc_oper_make(ecc_oper_switchop, ecc_value_fromint(conditionOps ? conditionOps->count : 0), ECC_ConstString_Empty), conditionOps, defaultOps);
    oplist = ecc_oplist_join(conditionOps, oplist);

    ecc_astparse_popdepth(self);
    ecc_astparse_expecttoken(self, '}');
    return oplist;
}

eccoplist_t* ecc_astparse_allstatement(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    ecctextstring_t text = self->lexer->text;

    if(ecc_astparse_previewtoken(self) == '{')
        return ecc_astparse_block(self);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_VAR))
    {
        oplist = ecc_astparse_variabledeclarationlist(self, 0);
        ecc_astparse_semicolon(self);
        return oplist;
    }
    else if(ecc_astparse_accepttoken(self, ';'))
        return ecc_oplist_create(ecc_oper_next, ECCValConstUndefined, text);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_IF))
        return ecc_astparse_ifstatement(self);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_DO))
        return ecc_astparse_dostatement(self);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_WHILE))
        return ecc_astparse_whilestatement(self);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_FOR))
        return ecc_astparse_forstatement(self);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_CONTINUE))
        return ecc_astparse_continuestatement(self, text);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_BREAK))
        return ecc_astparse_breakstatement(self, text);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_RETURN))
        return ecc_astparse_returnstatement(self, text);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_WITH))
    {
        if(self->strictMode)
            ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("code may not contain 'with' statements"));

        oplist = ecc_astparse_expression(self, 0);
        if(!oplist)
            ecc_astparse_tokenerror(self, "expression");

        oplist = ecc_oplist_join(oplist, ecc_oplist_appendnoop(ecc_astparse_statement(self)));
        oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_with, ecc_value_fromint(oplist->count), ECC_ConstString_Empty), oplist);

        return oplist;
    }
    else if(ecc_astparse_accepttoken(self, ECC_TOK_SWITCH))
        return ecc_astparse_switchstatement(self);
    else if(ecc_astparse_accepttoken(self, ECC_TOK_THROW))
    {
        if(!self->lexer->didLineBreak && ecc_astparse_previewtoken(self) != ';' && ecc_astparse_previewtoken(self) != '}' && ecc_astparse_previewtoken(self) != ECC_TOK_NO)
            oplist = ecc_astparse_expression(self, 0);

        if(!oplist)
            ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("throw statement is missing an expression"));

        ecc_astparse_semicolon(self);
        return ecc_oplist_unshift(ecc_oper_make(ecc_oper_throw, ECCValConstUndefined, ecc_textbuf_join(text, ecc_oplist_text(oplist))), oplist);
    }
    else if(ecc_astparse_accepttoken(self, ECC_TOK_TRY))
    {
        oplist = ecc_oplist_appendnoop(ecc_astparse_block(self));
        oplist = ecc_oplist_unshift(ecc_oper_make(ecc_oper_try, ecc_value_fromint(oplist->count), text), oplist);

        if(ecc_astparse_previewtoken(self) != ECC_TOK_CATCH && ecc_astparse_previewtoken(self) != ECC_TOK_FINALLY)
            ecc_astparse_tokenerror(self, "catch or finally");

        if(ecc_astparse_accepttoken(self, ECC_TOK_CATCH))
        {
            eccoperand_t identiferOp;
            eccoplist_t* catchOps;

            ecc_astparse_expecttoken(self, '(');
            if(ecc_astparse_previewtoken(self) != ECC_TOK_IDENTIFIER)
                ecc_astparse_syntaxerror(self, text, ecc_strbuf_create("missing identifier in catch"));

            identiferOp = ecc_astparse_identifier(self);
            ecc_astparse_expecttoken(self, ')');

            catchOps = ecc_astparse_block(self);
            catchOps = ecc_oplist_unshift(ecc_oper_make(ecc_oper_pushenvironment, ecc_value_fromkey(identiferOp.value.data.key), text), catchOps);
            catchOps = ecc_oplist_append(catchOps, ecc_oper_make(ecc_oper_popenvironment, ECCValConstUndefined, text));
            catchOps = ecc_oplist_unshift(ecc_oper_make(ecc_oper_jump, ecc_value_fromint(catchOps->count), text), catchOps);
            oplist = ecc_oplist_join(oplist, catchOps);
        }
        else
            oplist = ecc_oplist_append(ecc_oplist_append(oplist, ecc_oper_make(ecc_oper_jump, ecc_value_fromint(1), text)),
                                             ecc_oper_make(ecc_oper_noop, ECCValConstUndefined, text));

        if(ecc_astparse_accepttoken(self, ECC_TOK_FINALLY))
            oplist = ecc_oplist_join(oplist, ecc_astparse_block(self));

        return ecc_oplist_appendnoop(oplist);
    }
    else if(ecc_astparse_accepttoken(self, ECC_TOK_DEBUGGER))
    {
        ecc_astparse_semicolon(self);
        return ecc_oplist_create(ecc_oper_debugger, ECCValConstUndefined, text);
    }
    else
    {
        uint32_t index;

        oplist = ecc_astparse_expression(self, 0);
        if(!oplist)
            return NULL;

        if(oplist->ops[0].native == ecc_oper_getlocal && oplist->count == 1 && ecc_astparse_accepttoken(self, ':'))
        {
            ecc_astparse_pushdepth(self, oplist->ops[0].value.data.key, 0);
            free(oplist), oplist = NULL;
            oplist = ecc_astparse_statement(self);
            ecc_astparse_popdepth(self);
            return oplist;
        }

        ecc_astparse_accepttoken(self, ';');

        index = oplist->count;
        while(index--)
            if(oplist->ops[index].native == ecc_oper_call)
                return ecc_oplist_unshift(ecc_oper_make(self->sourcedepth <= 1 ? ecc_oper_autoreleaseexpression : ecc_oper_autoreleasediscard,
                                                                  ECCValConstUndefined, ECC_ConstString_Empty),
                                                oplist);

        return ecc_oplist_unshift(
        ecc_oper_make(self->sourcedepth <= 1 ? ecc_oper_expression : ecc_oper_discard, ECCValConstUndefined, ECC_ConstString_Empty), oplist);
    }
}

eccoplist_t* ecc_astparse_statement(eccastparser_t* self)
{
    eccoplist_t* oplist = ecc_astparse_allstatement(self);
    if(oplist && oplist->count > 1)
        oplist->ops[oplist->ops[0].text.length ? 0 : 1].text.flags |= ECC_TEXTFLAG_BREAKFLAG;

    return oplist;
}

// MARK: ECCNSFunction

eccoplist_t* ecc_astparse_parameters(eccastparser_t* self, int* count)
{
    eccoperand_t op;
    *count = 0;
    if(ecc_astparse_previewtoken(self) != ')')
        do
        {
            ++*count;
            op = ecc_astparse_identifier(self);

            if(op.value.data.key.data.integer)
            {
                if(self->strictMode && ecc_keyidx_isequal(op.value.data.key, ECC_ConstKey_eval))
                    ecc_astparse_syntaxerror(self, op.text, ecc_strbuf_create("redefining eval is not allowed"));
                else if(self->strictMode && ecc_keyidx_isequal(op.value.data.key, ECC_ConstKey_arguments))
                    ecc_astparse_syntaxerror(self, op.text, ecc_strbuf_create("redefining arguments is not allowed"));

                ecc_object_deletemember(&self->function->environment, op.value.data.key);
                ecc_object_addmember(&self->function->environment, op.value.data.key, ECCValConstUndefined, ECC_VALFLAG_HIDDEN);
            }
        } while(ecc_astparse_accepttoken(self, ','));

    return NULL;
}

eccoplist_t* ecc_astparse_function(eccastparser_t* self, int isDeclaration, int isGetter, int isSetter)
{
    eccvalue_t value;
    ecctextstring_t text, textParameter;

    eccoplist_t* oplist = NULL;
    int parameterCount = 0;

    eccoperand_t identifierOp = { 0, ECCValConstUndefined, {}};
    eccobjfunction_t* parentFunction;
    eccobjfunction_t* function;
    ecchashmap_t* arguments;
    uint16_t slot;

    if(!isGetter && !isSetter)
    {
        ecc_astparse_expecttoken(self, ECC_TOK_FUNCTION);

        if(ecc_astparse_previewtoken(self) == ECC_TOK_IDENTIFIER)
        {
            identifierOp = ecc_astparse_identifier(self);

            if(self->strictMode && ecc_keyidx_isequal(identifierOp.value.data.key, ECC_ConstKey_eval))
                ecc_astparse_syntaxerror(self, identifierOp.text, ecc_strbuf_create("redefining eval is not allowed"));
            else if(self->strictMode && ecc_keyidx_isequal(identifierOp.value.data.key, ECC_ConstKey_arguments))
                ecc_astparse_syntaxerror(self, identifierOp.text, ecc_strbuf_create("redefining arguments is not allowed"));
        }
        else if(isDeclaration)
        {
            ecc_astparse_syntaxerror(self, self->lexer->text, ecc_strbuf_create("function statement requires a name"));
            return NULL;
        }
    }

    parentFunction = self->function;
    parentFunction->flags |= ECC_SCRIPTFUNCFLAG_NEEDHEAP;

    function = ecc_function_create(&self->function->environment);

    arguments = (ecchashmap_t*)ecc_object_addmember(&function->environment, ECC_ConstKey_arguments, ECCValConstUndefined, 0);
    slot = arguments - function->environment.hashmap;

    self->function = function;
    text = self->lexer->text;
    ecc_astparse_expecttoken(self, '(');
    textParameter = self->lexer->text;
    oplist = ecc_oplist_join(oplist, ecc_astparse_parameters(self, &parameterCount));

    function->environment.hashmap[slot].hmapmapvalue = ECCValConstUndefined;
    function->environment.hashmap[slot].hmapmapvalue.key = ECC_ConstKey_arguments;
    function->environment.hashmap[slot].hmapmapvalue.flags |= ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED;

    if(isGetter && parameterCount != 0)
        ecc_astparse_syntaxerror(self, ecc_textbuf_make(textParameter.bytes, (int32_t)(self->lexer->text.bytes - textParameter.bytes)),
                    ecc_strbuf_create("getter functions must have no arguments"));
    else if(isSetter && parameterCount != 1)
        ecc_astparse_syntaxerror(self, ecc_textbuf_make(self->lexer->text.bytes, 0), ecc_strbuf_create("setter functions must have one argument"));

    ecc_astparse_expecttoken(self, ')');
    ecc_astparse_expecttoken(self, '{');

    if(parentFunction->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE)
        self->function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    oplist = ecc_oplist_join(oplist, ecc_astparse_sourceelements(self));
    text.length = (int32_t)(self->lexer->text.bytes - text.bytes) + 1;
    ecc_astparse_expecttoken(self, '}');
    self->function = parentFunction;

    function->oplist = oplist;
    function->text = text;
    function->parameterCount = parameterCount;

    ecc_object_addmember(&function->object, ECC_ConstKey_length, ecc_value_fromint(parameterCount), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    value = ecc_value_function(function);

    if(isDeclaration)
    {
        if(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE || self->sourcedepth > 1)
            ecc_object_addmember(&parentFunction->environment, identifierOp.value.data.key, ECCValConstUndefined, ECC_VALFLAG_HIDDEN);
        else
            ecc_object_addmember(self->global, identifierOp.value.data.key, ECCValConstUndefined, ECC_VALFLAG_HIDDEN);
    }
    else if(identifierOp.value.type != ECC_VALTYPE_UNDEFINED && !isGetter && !isSetter)
    {
        ecc_object_addmember(&function->environment, identifierOp.value.data.key, value, ECC_VALFLAG_HIDDEN);
        ecc_object_packvalue(&function->environment);
    }

    if(isGetter)
        value.flags |= ECC_VALFLAG_GETTER;
    else if(isSetter)
        value.flags |= ECC_VALFLAG_SETTER;

    if(isDeclaration)
        return ecc_oplist_append(ecc_oplist_create(ecc_oper_setlocal, identifierOp.value, ECC_ConstString_Empty),
                                       ecc_oper_make(ecc_oper_function, value, text));
    else
        return ecc_oplist_create(ecc_oper_function, value, text);
}

// MARK: Source

eccoplist_t* ecc_astparse_sourceelements(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;

    ++self->sourcedepth;

    self->function->oplist = NULL;

    if(ecc_astparse_previewtoken(self) == ECC_TOK_STRING && self->lexer->text.length == 10 && !memcmp("use strict", self->lexer->text.bytes, 10))
        self->function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    oplist = ecc_astparse_statementlist(self);

    if(self->sourcedepth <= 1)
        oplist = ecc_oplist_appendnoop(oplist);
    else
        oplist = ecc_oplist_append(oplist, ecc_oper_make(ecc_oper_resultvoid, ECCValConstUndefined, ECC_ConstString_Empty));

    if(self->function->oplist)
        self->function->oplist = ecc_oplist_joindiscarded(NULL, self->function->oplist->count / 2, self->function->oplist);

    oplist = ecc_oplist_join(self->function->oplist, oplist);

    oplist->ops->text.flags |= ECC_TEXTFLAG_BREAKFLAG;
    if(oplist->count > 1)
        oplist->ops[1].text.flags |= ECC_TEXTFLAG_BREAKFLAG;

    ecc_object_packvalue(&self->function->environment);

    --self->sourcedepth;

    return oplist;
}

// MARK: - Methods

eccastparser_t* ecc_astparse_createwithlexer(eccastlexer_t* lexer)
{
    eccastparser_t* self = (eccastparser_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccastparser_t));
    self->lexer = lexer;

    return self;
}

void ecc_astparse_destroy(eccastparser_t* self)
{
    assert(self);

    ecc_astlex_destroy(self->lexer), self->lexer = NULL;
    free(self->depthlistvals), self->depthlistvals = NULL;
    free(self), self = NULL;
}

eccobjfunction_t* ecc_astparse_parsewithenvironment(eccastparser_t* const self, eccobject_t* environment, eccobject_t* global)
{
    eccobjfunction_t* function;
    eccoplist_t* oplist;

    assert(self);

    function = ecc_function_create(environment);
    self->function = function;
    self->global = global;
    self->reserveGlobalSlots = 0;
    if(self->strictMode)
        function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    ecc_astparse_nexttoken(self);
    oplist = ecc_astparse_sourceelements(self);
    ecc_oplist_optimizewithenvironment(oplist, &function->environment, 0);

    ecc_object_reserveslots(global, self->reserveGlobalSlots);

    if(self->error)
    {
        eccoperand_t errorOps[] = {
            { ecc_oper_throw, ECCValConstUndefined, self->error->text },
            { ecc_oper_value, ecc_value_error(self->error), {} },
        };
        errorOps->text.flags |= ECC_TEXTFLAG_BREAKFLAG;

        ecc_oplist_destroy(oplist), oplist = NULL;
        oplist = (eccoplist_t*)malloc(sizeof(*oplist));
        oplist->ops = (eccoperand_t*)malloc(sizeof(errorOps));
        oplist->count = sizeof(errorOps) / sizeof(*errorOps);
        memcpy(oplist->ops, errorOps, sizeof(errorOps));
    }

    function->oplist = oplist;
    return function;
}
