//
//  parser.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

// MARK: - Static Members

static eccoplist_t* eccprs_new(eccastparser_t*);
static eccoplist_t* eccprs_assignment(eccastparser_t*, int noIn);
static eccoplist_t* eccprs_expression(eccastparser_t*, int noIn);
static eccoplist_t* eccprs_statement(eccastparser_t*);
static eccoplist_t* eccprs_function(eccastparser_t*, int isDeclaration, int isGetter, int isSetter);
static eccoplist_t* eccprs_sourceElements(eccastparser_t*);


static eccastparser_t* nsparsrfn_createWithLexer(eccastlexer_t*);
static void nsparsrfn_destroy(eccastparser_t*);
static eccobjfunction_t* nsparsrfn_parseWithEnvironment(eccastparser_t* const, eccobject_t* environment, eccobject_t* global);
const struct eccpseudonsparser_t ECCNSParser = {
    nsparsrfn_createWithLexer,
    nsparsrfn_destroy,
    nsparsrfn_parseWithEnvironment,
    {}
};

static inline eccasttoktype_t eccprs_previewToken(eccastparser_t* self)
{
    return self->previewToken;
}

static inline eccasttoktype_t eccprs_nextToken(eccastparser_t* self)
{
    if(self->previewToken != ECC_TOK_ERROR)
    {
        self->previewToken = ECCNSLexer.nextToken(self->lexer);

        if(self->previewToken == ECC_TOK_ERROR)
            self->error = self->lexer->value.data.error;
    }
    return self->previewToken;
}

static void eccprs_parseError(eccastparser_t* self, eccobjerror_t* error)
{
    if(!self->error)
    {
        self->error = error;
        self->previewToken = ECC_TOK_ERROR;
    }
}

static void eccprs_syntaxError(eccastparser_t* self, ecctextstring_t text, ecccharbuffer_t* message)
{
    eccprs_parseError(self, ECCNSError.syntaxError(text, message));
}

static void eccprs_referenceError(eccastparser_t* self, ecctextstring_t text, ecccharbuffer_t* message)
{
    eccprs_parseError(self, ECCNSError.referenceError(text, message));
}

static eccoplist_t* eccprs_tokenError(eccastparser_t* self, const char* t)
{
    char b[4];

    if(!eccprs_previewToken(self) || eccprs_previewToken(self) >= ECC_TOK_ERROR)
        eccprs_syntaxError(self, self->lexer->text, ECCNSChars.create("expected %s, got %s", t, ECCNSLexer.tokenChars(eccprs_previewToken(self), b)));
    else
        eccprs_syntaxError(self, self->lexer->text, ECCNSChars.create("expected %s, got '%.*s'", t, self->lexer->text.length, self->lexer->text.bytes));

    return NULL;
}

static inline int eccprs_acceptToken(eccastparser_t* self, eccasttoktype_t token)
{
    if(eccprs_previewToken(self) != token)
        return 0;

    eccprs_nextToken(self);
    return 1;
}

static inline int eccprs_expectToken(eccastparser_t* self, eccasttoktype_t token)
{
    if(eccprs_previewToken(self) != token)
    {
        char b[4];
        const char* type = ECCNSLexer.tokenChars(token, b);
        eccprs_tokenError(self, type);
        return 0;
    }

    eccprs_nextToken(self);
    return 1;
}

// MARK: Depth

static void eccprs_pushDepth(eccastparser_t* self, eccindexkey_t key, char depth)
{
    self->depths = realloc(self->depths, (self->depthCount + 1) * sizeof(*self->depths));
    self->depths[self->depthCount].key = key;
    self->depths[self->depthCount].depth = depth;
    ++self->depthCount;
}

static void eccprs_popDepth(eccastparser_t* self)
{
    --self->depthCount;
}

// MARK: Expression

static eccoplist_t* eccprs_foldConstant(eccastparser_t* self, eccoplist_t* oplist)
{
    eccscriptcontext_t ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
    eccstate_t context = { oplist->ops, .ecc = &ecc };
    eccvalue_t value = context.ops->native(&context);
    ecctextstring_t text = ECCNSOpList.text(oplist);
    ECCNSOpList.destroy(oplist);
    return ECCNSOpList.create(ECCNSOperand.value, value, text);
}

static eccoplist_t* eccprs_useBinary(eccastparser_t* self, eccoplist_t* oplist, int add)
{
    if(oplist && oplist->ops[0].native == ECCNSOperand.value && (ECCNSValue.isNumber(oplist->ops[0].value) || !add))
    {
        eccscriptcontext_t ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
        eccstate_t context = { oplist->ops, .ecc = &ecc };
        oplist->ops[0].value = ECCNSValue.toBinary(&context, oplist->ops[0].value);
    }
    return oplist;
}

static eccoplist_t* eccprs_useInteger(eccastparser_t* self, eccoplist_t* oplist)
{
    if(oplist && oplist->ops[0].native == ECCNSOperand.value)
    {
        eccscriptcontext_t ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
        eccstate_t context = { oplist->ops, .ecc = &ecc };
        oplist->ops[0].value = ECCNSValue.toInteger(&context, oplist->ops[0].value);
    }
    return oplist;
}

static eccoplist_t* eccprs_expressionRef(eccastparser_t* self, eccoplist_t* oplist, const char* name)
{
    if(!oplist)
        return NULL;

    if(oplist->ops[0].native == ECCNSOperand.getLocal && oplist->count == 1)
    {
        if(oplist->ops[0].value.type == ECC_VALTYPE_KEY)
        {
            if(ECCNSKey.isEqual(oplist->ops[0].value.data.key, ECC_ConstKey_eval))
                eccprs_syntaxError(self, ECCNSOpList.text(oplist), ECCNSChars.create(name));
            else if(ECCNSKey.isEqual(oplist->ops[0].value.data.key, ECC_ConstKey_arguments))
                eccprs_syntaxError(self, ECCNSOpList.text(oplist), ECCNSChars.create(name));
        }

        oplist->ops[0].native = ECCNSOperand.getLocalRef;
    }
    else if(oplist->ops[0].native == ECCNSOperand.getMember)
        oplist->ops[0].native = ECCNSOperand.getMemberRef;
    else if(oplist->ops[0].native == ECCNSOperand.getProperty)
        oplist->ops[0].native = ECCNSOperand.getPropertyRef;
    else
        eccprs_referenceError(self, ECCNSOpList.text(oplist), ECCNSChars.create("%s", name));

    return oplist;
}

static void eccprs_semicolon(eccastparser_t* self)
{
    if(eccprs_previewToken(self) == ';')
    {
        eccprs_nextToken(self);
        return;
    }
    else if(self->lexer->didLineBreak || eccprs_previewToken(self) == '}' || eccprs_previewToken(self) == ECC_TOK_NO)
        return;

    eccprs_syntaxError(self, self->lexer->text, ECCNSChars.create("missing ; before statement"));
}

static eccoperand_t eccprs_identifier(eccastparser_t* self)
{
    eccvalue_t value = self->lexer->value;
    ecctextstring_t text = self->lexer->text;
    if(!eccprs_expectToken(self, ECC_TOK_IDENTIFIER))
        return (eccoperand_t){ 0 };

    return ECCNSOperand.make(ECCNSOperand.value, value, text);
}

static eccoplist_t* eccprs_array(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    uint32_t count = 0;
    ecctextstring_t text = self->lexer->text;

    eccprs_nextToken(self);

    do
    {
        while(eccprs_previewToken(self) == ',')
        {
            ++count;
            oplist = ECCNSOpList.append(oplist, ECCNSOperand.make(ECCNSOperand.value, ECCValConstNone, self->lexer->text));
            eccprs_nextToken(self);
        }

        if(eccprs_previewToken(self) == ']')
            break;

        ++count;
        oplist = ECCNSOpList.join(oplist, eccprs_assignment(self, 0));
    } while(eccprs_acceptToken(self, ','));

    text = ECCNSText.join(text, self->lexer->text);
    eccprs_expectToken(self, ']');

    return ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.array, ECCNSValue.integer(count), text), oplist);
}

static eccoplist_t* eccprs_propertyAssignment(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    int isGetter = 0, isSetter = 0;

    if(eccprs_previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        if(ECCNSKey.isEqual(self->lexer->value.data.key, ECC_ConstKey_get))
        {
            eccprs_nextToken(self);
            if(eccprs_previewToken(self) == ':')
            {
                oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.key(ECC_ConstKey_get), self->lexer->text);
                goto skipProperty;
            }
            else
                isGetter = 1;
        }
        else if(ECCNSKey.isEqual(self->lexer->value.data.key, ECC_ConstKey_set))
        {
            eccprs_nextToken(self);
            if(eccprs_previewToken(self) == ':')
            {
                oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.key(ECC_ConstKey_set), self->lexer->text);
                goto skipProperty;
            }
            else
                isSetter = 1;
        }
    }

    if(eccprs_previewToken(self) == ECC_TOK_INTEGER)
        oplist = ECCNSOpList.create(ECCNSOperand.value, self->lexer->value, self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_BINARY)
        oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.key(ECCNSKey.makeWithText(self->lexer->text, 0)), self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_STRING)
    {
        uint32_t element = ECCNSLexer.scanElement(self->lexer->text);
        if(element < UINT32_MAX)
            oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.integer(element), self->lexer->text);
        else
            oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.key(ECCNSKey.makeWithText(self->lexer->text, 0)), self->lexer->text);
    }
    else if(eccprs_previewToken(self) == ECC_TOK_ESCAPEDSTRING)
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
            bufp = self->lexer->value.data.chars->bytes; 
            len = self->lexer->value.data.chars->length;
        #else
            bufp = self->lexer->value.data.buffer;
            len = strlen(bufp);
        #endif
        text = ECCNSText.make(bufp, len);
        element = ECCNSLexer.scanElement(text);
        if(element < UINT32_MAX)
        {
            oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.integer(element), self->lexer->text);
        }
        else
        {
            #if 0
                kkey = ECCNSKey.makeWithText(*self->lexer->value.data.text, 0);
            #else
                kkey = ECCNSKey.makeWithCString(bufp);
            #endif
            kval = ECCNSValue.key(kkey);
            oplist = ECCNSOpList.create(ECCNSOperand.value, kval, self->lexer->text);
        }
    }
    else if(eccprs_previewToken(self) == ECC_TOK_IDENTIFIER)
        oplist = ECCNSOpList.create(ECCNSOperand.value, self->lexer->value, self->lexer->text);
    else
    {
        eccprs_expectToken(self, ECC_TOK_IDENTIFIER);
        return NULL;
    }

    eccprs_nextToken(self);

    if(isGetter)
        return ECCNSOpList.join(oplist, eccprs_function(self, 0, 1, 0));
    else if(isSetter)
        return ECCNSOpList.join(oplist, eccprs_function(self, 0, 0, 1));

skipProperty:
    eccprs_expectToken(self, ':');
    return ECCNSOpList.join(oplist, eccprs_assignment(self, 0));
}

static eccoplist_t* eccprs_object(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    uint32_t count = 0;
    ecctextstring_t text = self->lexer->text;

    do
    {
        self->lexer->disallowKeyword = 1;
        eccprs_nextToken(self);
        self->lexer->disallowKeyword = 0;

        if(eccprs_previewToken(self) == '}')
            break;

        ++count;
        oplist = ECCNSOpList.join(oplist, eccprs_propertyAssignment(self));
    } while(eccprs_previewToken(self) == ',');

    text = ECCNSText.join(text, self->lexer->text);
    eccprs_expectToken(self, '}');

    return ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.object, ECCNSValue.integer(count), text), oplist);
}

static eccoplist_t* eccprs_primary(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;

    if(eccprs_previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        oplist = ECCNSOpList.create(ECCNSOperand.getLocal, self->lexer->value, self->lexer->text);

        if(ECCNSKey.isEqual(self->lexer->value.data.key, ECC_ConstKey_arguments))
            self->function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_NEEDHEAP;
    }
    else if(eccprs_previewToken(self) == ECC_TOK_STRING)
        oplist = ECCNSOpList.create(ECCNSOperand.text, ECCValConstUndefined, self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_ESCAPEDSTRING)
        oplist = ECCNSOpList.create(ECCNSOperand.value, self->lexer->value, self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_BINARY)
        oplist = ECCNSOpList.create(ECCNSOperand.value, self->lexer->value, self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_INTEGER)
    {
        if(self->preferInteger)
            oplist = ECCNSOpList.create(ECCNSOperand.value, self->lexer->value, self->lexer->text);
        else
            oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.binary(self->lexer->value.data.integer), self->lexer->text);
    }
    else if(eccprs_previewToken(self) == ECC_TOK_THIS)
        oplist = ECCNSOpList.create(ECCNSOperand.getThis, ECCValConstUndefined, self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_NULL)
        oplist = ECCNSOpList.create(ECCNSOperand.value, ECCValConstNull, self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_TRUE)
        oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.truth(1), self->lexer->text);
    else if(eccprs_previewToken(self) == ECC_TOK_FALSE)
        oplist = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.truth(0), self->lexer->text);
    else if(eccprs_previewToken(self) == '{')
        return eccprs_object(self);
    else if(eccprs_previewToken(self) == '[')
        return eccprs_array(self);
    else if(eccprs_acceptToken(self, '('))
    {
        oplist = eccprs_expression(self, 0);
        eccprs_expectToken(self, ')');
        return oplist;
    }
    else
    {
        if(self->lexer->text.bytes[0] == '/')
        {
            self->lexer->allowRegex = 1;
            self->lexer->offset -= self->lexer->text.length;
            eccprs_nextToken(self);
            self->lexer->allowRegex = 0;

            if(eccprs_previewToken(self) != ECC_TOK_REGEXP)
                eccprs_tokenError(self, "ECCNSRegExp");
        }

        if(eccprs_previewToken(self) == ECC_TOK_REGEXP)
            oplist = ECCNSOpList.create(ECCNSOperand.regexp, ECCValConstUndefined, self->lexer->text);
        else
            return NULL;
    }

    eccprs_nextToken(self);

    return oplist;
}

static eccoplist_t* eccprs_arguments(eccastparser_t* self, int* count)
{
    eccoplist_t *oplist = NULL, *argumentOps;
    *count = 0;
    if(eccprs_previewToken(self) != ')')
        do
        {
            argumentOps = eccprs_assignment(self, 0);
            if(!argumentOps)
                eccprs_tokenError(self, "expression");

            ++*count;
            oplist = ECCNSOpList.join(oplist, argumentOps);
        } while(eccprs_acceptToken(self, ','));

    return oplist;
}

static eccoplist_t* eccprs_member(eccastparser_t* self)
{
    eccoplist_t* oplist = eccprs_new(self);
    ecctextstring_t text;
    while(1)
    {
        if(eccprs_previewToken(self) == '.')
        {
            eccvalue_t value;

            self->lexer->disallowKeyword = 1;
            eccprs_nextToken(self);
            self->lexer->disallowKeyword = 0;

            value = self->lexer->value;
            text = ECCNSText.join(ECCNSOpList.text(oplist), self->lexer->text);
            if(!eccprs_expectToken(self, ECC_TOK_IDENTIFIER))
                return oplist;

            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.getMember, value, text), oplist);
        }
        else if(eccprs_acceptToken(self, '['))
        {
            oplist = ECCNSOpList.join(oplist, eccprs_expression(self, 0));
            text = ECCNSText.join(ECCNSOpList.text(oplist), self->lexer->text);
            if(!eccprs_expectToken(self, ']'))
                return oplist;

            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.getProperty, ECCValConstUndefined, text), oplist);
        }
        else
            break;
    }
    return oplist;
}

static eccoplist_t* eccprs_new(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    ecctextstring_t text = self->lexer->text;

    if(eccprs_acceptToken(self, ECC_TOK_NEW))
    {
        int count = 0;
        oplist = eccprs_member(self);
        text = ECCNSText.join(text, ECCNSOpList.text(oplist));
        if(eccprs_acceptToken(self, '('))
        {
            oplist = ECCNSOpList.join(oplist, eccprs_arguments(self, &count));
            text = ECCNSText.join(text, self->lexer->text);
            eccprs_expectToken(self, ')');
        }
        return ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.construct, ECCNSValue.integer(count), text), oplist);
    }
    else if(eccprs_previewToken(self) == ECC_TOK_FUNCTION)
        return eccprs_function(self, 0, 0, 0);
    else
        return eccprs_primary(self);
}

static eccoplist_t* eccprs_leftHandSide(eccastparser_t* self)
{
    eccoplist_t* oplist = eccprs_new(self);
    ecctextstring_t text = ECCNSOpList.text(oplist);
    eccvalue_t value;

    while(1)
    {
        if(eccprs_previewToken(self) == '.')
        {
            if(!oplist)
            {
                eccprs_tokenError(self, "expression");
                return oplist;
            }

            self->lexer->disallowKeyword = 1;
            eccprs_nextToken(self);
            self->lexer->disallowKeyword = 0;

            value = self->lexer->value;
            text = ECCNSText.join(ECCNSOpList.text(oplist), self->lexer->text);
            if(!eccprs_expectToken(self, ECC_TOK_IDENTIFIER))
                return oplist;

            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.getMember, value, text), oplist);
        }
        else if(eccprs_acceptToken(self, '['))
        {
            oplist = ECCNSOpList.join(oplist, eccprs_expression(self, 0));
            text = ECCNSText.join(ECCNSOpList.text(oplist), self->lexer->text);
            if(!eccprs_expectToken(self, ']'))
                return oplist;

            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.getProperty, ECCValConstUndefined, text), oplist);
        }
        else if(eccprs_acceptToken(self, '('))
        {
            int count = 0;

            int isEval = oplist->count == 1 && oplist->ops[0].native == ECCNSOperand.getLocal && ECCNSKey.isEqual(oplist->ops[0].value.data.key, ECC_ConstKey_eval);
            if(isEval)
            {
                text = ECCNSText.join(ECCNSOpList.text(oplist), self->lexer->text);
                ECCNSOpList.destroy(oplist), oplist = NULL;
            }

            oplist = ECCNSOpList.join(oplist, eccprs_arguments(self, &count));
            text = ECCNSText.join(ECCNSText.join(text, ECCNSOpList.text(oplist)), self->lexer->text);

            if(isEval)
                oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.eval, ECCNSValue.integer(count), text), oplist);
            else if(oplist->ops->native == ECCNSOperand.getMember)
                oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.callMember, ECCNSValue.integer(count), text), oplist);
            else if(oplist->ops->native == ECCNSOperand.getProperty)
                oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.callProperty, ECCNSValue.integer(count), text), oplist);
            else
                oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.call, ECCNSValue.integer(count), text), oplist);

            if(!eccprs_expectToken(self, ')'))
                break;
        }
        else
            break;
    }
    return oplist;
}

static eccoplist_t* eccprs_postfix(eccastparser_t* self)
{
    eccoplist_t* oplist = eccprs_leftHandSide(self);
    ecctextstring_t text = self->lexer->text;

    if(!self->lexer->didLineBreak && eccprs_acceptToken(self, ECC_TOK_INCREMENT))
        oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.postIncrementRef, ECCValConstUndefined, ECCNSText.join(oplist->ops->text, text)),
                                          eccprs_expressionRef(self, oplist, "invalid increment operand"));
    if(!self->lexer->didLineBreak && eccprs_acceptToken(self, ECC_TOK_DECREMENT))
        oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.postDecrementRef, ECCValConstUndefined, ECCNSText.join(oplist->ops->text, text)),
                                          eccprs_expressionRef(self, oplist, "invalid decrement operand"));

    return oplist;
}

static eccoplist_t* eccprs_unary(eccastparser_t* self)
{
    eccoplist_t *oplist, *alt;
    ecctextstring_t text = self->lexer->text;
    eccnativefuncptr_t native;

    if(eccprs_acceptToken(self, ECC_TOK_DELETE))
    {
        oplist = eccprs_unary(self);

        if(oplist && oplist->ops[0].native == ECCNSOperand.getLocal)
        {
            if(self->strictMode)
                eccprs_syntaxError(self, ECCNSOpList.text(oplist), ECCNSChars.create("delete of an unqualified identifier"));

            oplist->ops->native = ECCNSOperand.deleteLocal;
        }
        else if(oplist && oplist->ops[0].native == ECCNSOperand.getMember)
            oplist->ops->native = ECCNSOperand.deleteMember;
        else if(oplist && oplist->ops[0].native == ECCNSOperand.getProperty)
            oplist->ops->native = ECCNSOperand.deleteProperty;
        else if(!self->strictMode && oplist)
            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.exchange, ECCValConstTrue, ECC_ConstString_Empty), oplist);
        else if(oplist)
            eccprs_referenceError(self, ECCNSOpList.text(oplist), ECCNSChars.create("invalid delete operand"));
        else
            eccprs_tokenError(self, "expression");

        return oplist;
    }
    else if(eccprs_acceptToken(self, ECC_TOK_VOID))
        native = ECCNSOperand.exchange, alt = eccprs_unary(self);
    else if(eccprs_acceptToken(self, ECC_TOK_TYPEOF))
    {
        native = ECCNSOperand.typeOf, alt = eccprs_unary(self);
        if(alt->ops->native == ECCNSOperand.getLocal)
            alt->ops->native = ECCNSOperand.getLocalRefOrNull;
    }
    else if(eccprs_acceptToken(self, ECC_TOK_INCREMENT))
        native = ECCNSOperand.incrementRef, alt = eccprs_expressionRef(self, eccprs_unary(self), "invalid increment operand");
    else if(eccprs_acceptToken(self, ECC_TOK_DECREMENT))
        native = ECCNSOperand.decrementRef, alt = eccprs_expressionRef(self, eccprs_unary(self), "invalid decrement operand");
    else if(eccprs_acceptToken(self, '+'))
        native = ECCNSOperand.positive, alt = eccprs_useBinary(self, eccprs_unary(self), 0);
    else if(eccprs_acceptToken(self, '-'))
        native = ECCNSOperand.negative, alt = eccprs_useBinary(self, eccprs_unary(self), 0);
    else if(eccprs_acceptToken(self, '~'))
        native = ECCNSOperand.invert, alt = eccprs_useInteger(self, eccprs_unary(self));
    else if(eccprs_acceptToken(self, '!'))
        native = ECCNSOperand.doLogicalNot, alt = eccprs_unary(self);
    else
        return eccprs_postfix(self);

    if(!alt)
        return eccprs_tokenError(self, "expression");

    oplist = ECCNSOpList.unshift(ECCNSOperand.make(native, ECCValConstUndefined, ECCNSText.join(text, alt->ops->text)), alt);

    if(oplist->ops[1].native == ECCNSOperand.value)
        return eccprs_foldConstant(self, oplist);
    else
        return oplist;
}

static eccoplist_t* eccprs_multiplicative(eccastparser_t* self)
{
    eccoplist_t *oplist = eccprs_unary(self), *alt;

    while(1)
    {
        eccnativefuncptr_t native;

        if(eccprs_previewToken(self) == '*')
            native = ECCNSOperand.multiply;
        else if(eccprs_previewToken(self) == '/')
            native = ECCNSOperand.divide;
        else if(eccprs_previewToken(self) == '%')
            native = ECCNSOperand.modulo;
        else
            return oplist;

        if(eccprs_useBinary(self, oplist, 0))
        {
            eccprs_nextToken(self);
            if((alt = eccprs_useBinary(self, eccprs_unary(self), 0)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == ECCNSOperand.value && oplist->ops[2].native == ECCNSOperand.value)
                    oplist = eccprs_foldConstant(self, oplist);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
}

static eccoplist_t* eccprs_additive(eccastparser_t* self)
{
    eccoplist_t *oplist = eccprs_multiplicative(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(eccprs_previewToken(self) == '+')
            native = ECCNSOperand.add;
        else if(eccprs_previewToken(self) == '-')
            native = ECCNSOperand.minus;
        else
            return oplist;

        if(eccprs_useBinary(self, oplist, native == ECCNSOperand.add))
        {
            eccprs_nextToken(self);
            if((alt = eccprs_useBinary(self, eccprs_multiplicative(self), native == ECCNSOperand.add)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == ECCNSOperand.value && oplist->ops[2].native == ECCNSOperand.value)
                    oplist = eccprs_foldConstant(self, oplist);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
}

static eccoplist_t* eccprs_shift(eccastparser_t* self)
{
    eccoplist_t *oplist = eccprs_additive(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(eccprs_previewToken(self) == ECC_TOK_LEFTSHIFT)
            native = ECCNSOperand.leftShift;
        else if(eccprs_previewToken(self) == ECC_TOK_RIGHTSHIFT)
            native = ECCNSOperand.rightShift;
        else if(eccprs_previewToken(self) == ECC_TOK_UNSIGNEDRIGHTSHIFT)
            native = ECCNSOperand.unsignedRightShift;
        else
            return oplist;

        if(eccprs_useInteger(self, oplist))
        {
            eccprs_nextToken(self);
            if((alt = eccprs_useInteger(self, eccprs_additive(self))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == ECCNSOperand.value && oplist->ops[2].native == ECCNSOperand.value)
                    oplist = eccprs_foldConstant(self, oplist);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
}

static eccoplist_t* eccprs_relational(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = eccprs_shift(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(eccprs_previewToken(self) == '<')
            native = ECCNSOperand.less;
        else if(eccprs_previewToken(self) == '>')
            native = ECCNSOperand.more;
        else if(eccprs_previewToken(self) == ECC_TOK_LESSOREQUAL)
            native = ECCNSOperand.lessOrEqual;
        else if(eccprs_previewToken(self) == ECC_TOK_MOREOREQUAL)
            native = ECCNSOperand.moreOrEqual;
        else if(eccprs_previewToken(self) == ECC_TOK_INSTANCEOF)
            native = ECCNSOperand.instanceOf;
        else if(!noIn && eccprs_previewToken(self) == ECC_TOK_IN)
            native = ECCNSOperand.in;
        else
            return oplist;

        if(oplist)
        {
            eccprs_nextToken(self);
            if((alt = eccprs_shift(self)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(native, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
}

static eccoplist_t* eccprs_equality(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = eccprs_relational(self, noIn), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(eccprs_previewToken(self) == ECC_TOK_EQUAL)
            native = ECCNSOperand.equal;
        else if(eccprs_previewToken(self) == ECC_TOK_NOTEQUAL)
            native = ECCNSOperand.notEqual;
        else if(eccprs_previewToken(self) == ECC_TOK_IDENTICAL)
            native = ECCNSOperand.identical;
        else if(eccprs_previewToken(self) == ECC_TOK_NOTIDENTICAL)
            native = ECCNSOperand.notIdentical;
        else
            return oplist;

        if(oplist)
        {
            eccprs_nextToken(self);
            if((alt = eccprs_relational(self, noIn)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(native, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
}

static eccoplist_t* eccprs_bitwiseAnd(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = eccprs_equality(self, noIn), *alt;
    while(eccprs_previewToken(self) == '&')
    {
        if(eccprs_useInteger(self, oplist))
        {
            eccprs_nextToken(self);
            if((alt = eccprs_useInteger(self, eccprs_equality(self, noIn))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(ECCNSOperand.bitwiseAnd, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* eccprs_bitwiseXor(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = eccprs_bitwiseAnd(self, noIn), *alt;
    while(eccprs_previewToken(self) == '^')
    {
        if(eccprs_useInteger(self, oplist))
        {
            eccprs_nextToken(self);
            if((alt = eccprs_useInteger(self, eccprs_bitwiseAnd(self, noIn))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(ECCNSOperand.bitwiseXor, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* eccprs_bitwiseOr(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = eccprs_bitwiseXor(self, noIn), *alt;
    while(eccprs_previewToken(self) == '|')
    {
        if(eccprs_useInteger(self, oplist))
        {
            eccprs_nextToken(self);
            if((alt = eccprs_useInteger(self, eccprs_bitwiseXor(self, noIn))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(ECCNSOperand.bitwiseOr, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            ECCNSOpList.destroy(oplist);
        }
        return eccprs_tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* eccprs_logicalAnd(eccastparser_t* self, int noIn)
{
    int32_t opCount;
    eccoplist_t *oplist = eccprs_bitwiseOr(self, noIn), *nextOp = NULL;

    while(eccprs_acceptToken(self, ECC_TOK_LOGICALAND))
        if(oplist && (nextOp = eccprs_bitwiseOr(self, noIn)))
        {
            opCount = nextOp->count;
            oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(ECCNSOperand.logicalAnd, ECCNSValue.integer(opCount), ECCNSOpList.text(oplist)), oplist, nextOp);
        }
        else
            eccprs_tokenError(self, "expression");

    return oplist;
}

static eccoplist_t* eccprs_logicalOr(eccastparser_t* self, int noIn)
{
    int32_t opCount;
    eccoplist_t *oplist = eccprs_logicalAnd(self, noIn), *nextOp = NULL;

    while(eccprs_acceptToken(self, ECC_TOK_LOGICALOR))
        if(oplist && (nextOp = eccprs_logicalAnd(self, noIn)))
        {
            opCount = nextOp->count;
            oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(ECCNSOperand.logicalOr, ECCNSValue.integer(opCount), ECCNSOpList.text(oplist)), oplist, nextOp);
        }
        else
            eccprs_tokenError(self, "expression");

    return oplist;
}

static eccoplist_t* eccprs_conditional(eccastparser_t* self, int noIn)
{
    eccoplist_t* oplist = eccprs_logicalOr(self, noIn);

    if(eccprs_acceptToken(self, '?'))
    {
        if(oplist)
        {
            eccoplist_t* trueOps = eccprs_assignment(self, 0);
            eccoplist_t* falseOps;

            if(!eccprs_expectToken(self, ':'))
                return oplist;

            falseOps = eccprs_assignment(self, noIn);

            trueOps = ECCNSOpList.append(trueOps, ECCNSOperand.make(ECCNSOperand.jump, ECCNSValue.integer(falseOps->count), ECCNSOpList.text(trueOps)));
            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.jumpIfNot, ECCNSValue.integer(trueOps->count), ECCNSOpList.text(oplist)), oplist);
            oplist = ECCNSOpList.join3(oplist, trueOps, falseOps);

            return oplist;
        }
        else
            eccprs_tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* eccprs_assignment(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = eccprs_conditional(self, noIn), *opassign = NULL;
    ecctextstring_t text = self->lexer->text;
    eccnativefuncptr_t native = NULL;

    if(eccprs_acceptToken(self, '='))
    {
        if(!oplist)
        {
            eccprs_syntaxError(self, text, ECCNSChars.create("expected expression, got '='"));
            return NULL;
        }
        else if(oplist->ops[0].native == ECCNSOperand.getLocal && oplist->count == 1)
        {
            if(ECCNSKey.isEqual(oplist->ops[0].value.data.key, ECC_ConstKey_eval))
                eccprs_syntaxError(self, text, ECCNSChars.create("can't assign to eval"));
            else if(ECCNSKey.isEqual(oplist->ops[0].value.data.key, ECC_ConstKey_arguments))
                eccprs_syntaxError(self, text, ECCNSChars.create("can't assign to arguments"));

            if(!self->strictMode && !ECCNSObject.member(&self->function->environment, oplist->ops[0].value.data.key, 0))
                ++self->reserveGlobalSlots;
            //				ECCNSObject.addMember(self->global, oplist->ops[0].value.data.key, ECCValConstNone, 0);

            oplist->ops->native = ECCNSOperand.setLocal;
        }
        else if(oplist->ops[0].native == ECCNSOperand.getMember)
            oplist->ops->native = ECCNSOperand.setMember;
        else if(oplist->ops[0].native == ECCNSOperand.getProperty)
            oplist->ops->native = ECCNSOperand.setProperty;
        else
            eccprs_referenceError(self, ECCNSOpList.text(oplist), ECCNSChars.create("invalid assignment left-hand side"));

        if((opassign = eccprs_assignment(self, noIn)))
        {
            oplist->ops->text = ECCNSText.join(oplist->ops->text, opassign->ops->text);
            return ECCNSOpList.join(oplist, opassign);
        }

        eccprs_tokenError(self, "expression");
    }
    else if(eccprs_acceptToken(self, ECC_TOK_MULTIPLYASSIGN))
        native = ECCNSOperand.multiplyAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_DIVIDEASSIGN))
        native = ECCNSOperand.divideAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_MODULOASSIGN))
        native = ECCNSOperand.moduloAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_ADDASSIGN))
        native = ECCNSOperand.addAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_MINUSASSIGN))
        native = ECCNSOperand.minusAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_LEFTSHIFTASSIGN))
        native = ECCNSOperand.leftShiftAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_RIGHTSHIFTASSIGN))
        native = ECCNSOperand.rightShiftAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN))
        native = ECCNSOperand.unsignedRightShiftAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_ANDASSIGN))
        native = ECCNSOperand.bitAndAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_XORASSIGN))
        native = ECCNSOperand.bitXorAssignRef;
    else if(eccprs_acceptToken(self, ECC_TOK_ORASSIGN))
        native = ECCNSOperand.bitOrAssignRef;
    else
        return oplist;

    if(oplist)
    {
        if((opassign = eccprs_assignment(self, noIn)))
            oplist->ops->text = ECCNSText.join(oplist->ops->text, opassign->ops->text);
        else
            eccprs_tokenError(self, "expression");

        return ECCNSOpList.unshiftJoin(ECCNSOperand.make(native, ECCValConstUndefined, oplist->ops->text),
                                            eccprs_expressionRef(self, oplist, "invalid assignment left-hand side"), opassign);
    }

    eccprs_syntaxError(self, text, ECCNSChars.create("expected expression, got '%.*s'", text.length, text.bytes));
    return NULL;
}

static eccoplist_t* eccprs_expression(eccastparser_t* self, int noIn)
{
    eccoplist_t* oplist = eccprs_assignment(self, noIn);
    while(eccprs_acceptToken(self, ','))
        oplist = ECCNSOpList.unshiftJoin(ECCNSOperand.make(ECCNSOperand.discard, ECCValConstUndefined, ECC_ConstString_Empty), oplist, eccprs_assignment(self, noIn));

    return oplist;
}

// MARK: Statements

static eccoplist_t* eccprs_statementList(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *statementOps = NULL, *discardOps = NULL;
    uint16_t discardCount = 0;

    while(eccprs_previewToken(self) != ECC_TOK_ERROR && eccprs_previewToken(self) != ECC_TOK_NO)
    {
        if(eccprs_previewToken(self) == ECC_TOK_FUNCTION)
            self->function->oplist = ECCNSOpList.join(self->function->oplist, eccprs_function(self, 1, 0, 0));
        else
        {
            if((statementOps = eccprs_statement(self)))
            {
                while(statementOps->count > 1 && statementOps->ops[0].native == ECCNSOperand.next)
                    ECCNSOpList.shift(statementOps);

                if(statementOps->count == 1 && statementOps->ops[0].native == ECCNSOperand.next)
                    ECCNSOpList.destroy(statementOps), statementOps = NULL;
                else
                {
                    if(statementOps->ops[0].native == ECCNSOperand.discard)
                    {
                        ++discardCount;
                        discardOps = ECCNSOpList.join(discardOps, ECCNSOpList.shift(statementOps));
                        statementOps = NULL;
                    }
                    else if(discardOps)
                    {
                        oplist = ECCNSOpList.joinDiscarded(oplist, discardCount, discardOps);
                        discardOps = NULL;
                        discardCount = 0;
                    }

                    oplist = ECCNSOpList.join(oplist, statementOps);
                }
            }
            else
                break;
        }
    }

    if(discardOps)
        oplist = ECCNSOpList.joinDiscarded(oplist, discardCount, discardOps);

    return oplist;
}

static eccoplist_t* eccprs_block(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    eccprs_expectToken(self, '{');
    if(eccprs_previewToken(self) == '}')
        oplist = ECCNSOpList.create(ECCNSOperand.next, ECCValConstUndefined, self->lexer->text);
    else
        oplist = eccprs_statementList(self);

    eccprs_expectToken(self, '}');
    return oplist;
}

static eccoplist_t* eccprs_variableDeclaration(eccastparser_t* self, int noIn)
{
    eccvalue_t value = self->lexer->value;
    ecctextstring_t text = self->lexer->text;
    if(!eccprs_expectToken(self, ECC_TOK_IDENTIFIER))
        return NULL;

    if(self->strictMode && ECCNSKey.isEqual(value.data.key, ECC_ConstKey_eval))
        eccprs_syntaxError(self, text, ECCNSChars.create("redefining eval is not allowed"));
    else if(self->strictMode && ECCNSKey.isEqual(value.data.key, ECC_ConstKey_arguments))
        eccprs_syntaxError(self, text, ECCNSChars.create("redefining arguments is not allowed"));

    if(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE || self->sourceDepth > 1)
        ECCNSObject.addMember(&self->function->environment, value.data.key, ECCValConstUndefined, ECC_VALFLAG_SEALED);
    else
        ECCNSObject.addMember(self->global, value.data.key, ECCValConstUndefined, ECC_VALFLAG_SEALED);

    if(eccprs_acceptToken(self, '='))
    {
        eccoplist_t* opassign = eccprs_assignment(self, noIn);

        if(opassign)
            return ECCNSOpList.unshiftJoin(ECCNSOperand.make(ECCNSOperand.discard, ECCValConstUndefined, ECC_ConstString_Empty),
                                                ECCNSOpList.create(ECCNSOperand.setLocal, value, ECCNSText.join(text, opassign->ops->text)), opassign);

        eccprs_tokenError(self, "expression");
        return NULL;
    }
    //	else if (!(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE) && self->sourceDepth <= 1)
    //		return ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.discard, ECCValConstUndefined, ECC_ConstString_Empty), ECCNSOpList.create(ECCNSOperand.createLocalRef, value, text));
    else
        return ECCNSOpList.create(ECCNSOperand.next, value, text);
}

static eccoplist_t* eccprs_variableDeclarationList(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = NULL, *varOps;
    do
    {
        varOps = eccprs_variableDeclaration(self, noIn);

        if(oplist && varOps && varOps->count == 1 && varOps->ops[0].native == ECCNSOperand.next)
            ECCNSOpList.destroy(varOps), varOps = NULL;
        else
            oplist = ECCNSOpList.join(oplist, varOps);
    } while(eccprs_acceptToken(self, ','));

    return oplist;
}

static eccoplist_t* eccprs_ifStatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *trueOps = NULL, *falseOps = NULL;
    eccprs_expectToken(self, '(');
    oplist = eccprs_expression(self, 0);
    eccprs_expectToken(self, ')');
    trueOps = eccprs_statement(self);
    if(!trueOps)
        trueOps = ECCNSOpList.appendNoop(NULL);

    if(eccprs_acceptToken(self, ECC_TOK_ELSE))
    {
        falseOps = eccprs_statement(self);
        if(falseOps)
            trueOps = ECCNSOpList.append(trueOps, ECCNSOperand.make(ECCNSOperand.jump, ECCNSValue.integer(falseOps->count), ECCNSOpList.text(trueOps)));
    }
    oplist = ECCNSOpList.unshiftJoin3(ECCNSOperand.make(ECCNSOperand.jumpIfNot, ECCNSValue.integer(trueOps->count), ECCNSOpList.text(oplist)), oplist,
                                           trueOps, falseOps);
    return oplist;
}

static eccoplist_t* eccprs_doStatement(eccastparser_t* self)
{
    eccoplist_t *oplist, *condition;

    eccprs_pushDepth(self, ECC_ConstKey_none, 2);
    oplist = eccprs_statement(self);
    eccprs_popDepth(self);

    eccprs_expectToken(self, ECC_TOK_WHILE);
    eccprs_expectToken(self, '(');
    condition = eccprs_expression(self, 0);
    eccprs_expectToken(self, ')');
    eccprs_acceptToken(self, ';');

    return ECCNSOpList.createLoop(NULL, condition, NULL, oplist, 1);
}

static eccoplist_t* eccprs_whileStatement(eccastparser_t* self)
{
    eccoplist_t *oplist, *condition;

    eccprs_expectToken(self, '(');
    condition = eccprs_expression(self, 0);
    eccprs_expectToken(self, ')');

    eccprs_pushDepth(self, ECC_ConstKey_none, 2);
    oplist = eccprs_statement(self);
    eccprs_popDepth(self);

    return ECCNSOpList.createLoop(NULL, condition, NULL, oplist, 0);
}

static eccoplist_t* eccprs_forStatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *condition = NULL, *increment = NULL, *body = NULL;

    eccprs_expectToken(self, '(');

    self->preferInteger = 1;

    if(eccprs_acceptToken(self, ECC_TOK_VAR))
        oplist = eccprs_variableDeclarationList(self, 1);
    else if(eccprs_previewToken(self) != ';')
    {
        oplist = eccprs_expression(self, 1);

        if(oplist)
            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.discard, ECCValConstUndefined, ECCNSOpList.text(oplist)), oplist);
    }

    if(oplist && eccprs_acceptToken(self, ECC_TOK_IN))
    {
        if(oplist->count == 2 && oplist->ops[0].native == ECCNSOperand.discard && oplist->ops[1].native == ECCNSOperand.getLocal)
        {
            if(!self->strictMode && !ECCNSObject.member(&self->function->environment, oplist->ops[1].value.data.key, 0))
                ++self->reserveGlobalSlots;

            oplist->ops[0].native = ECCNSOperand.iterateInRef;
            oplist->ops[1].native = ECCNSOperand.createLocalRef;
        }
        else if(oplist->count == 1 && oplist->ops[0].native == ECCNSOperand.next)
        {
            oplist->ops->native = ECCNSOperand.createLocalRef;
            oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.iterateInRef, ECCValConstUndefined, self->lexer->text), oplist);
        }
        else
            eccprs_referenceError(self, ECCNSOpList.text(oplist), ECCNSChars.create("invalid for/in left-hand side"));

        oplist = ECCNSOpList.join(oplist, eccprs_expression(self, 0));
        oplist->ops[0].text = ECCNSOpList.text(oplist);
        eccprs_expectToken(self, ')');

        self->preferInteger = 0;

        eccprs_pushDepth(self, ECC_ConstKey_none, 2);
        body = eccprs_statement(self);
        eccprs_popDepth(self);

        body = ECCNSOpList.appendNoop(body);
        return ECCNSOpList.join(ECCNSOpList.append(oplist, ECCNSOperand.make(ECCNSOperand.value, ECCNSValue.integer(body->count), self->lexer->text)), body);
    }
    else
    {
        eccprs_expectToken(self, ';');
        if(eccprs_previewToken(self) != ';')
            condition = eccprs_expression(self, 0);

        eccprs_expectToken(self, ';');
        if(eccprs_previewToken(self) != ')')
            increment = eccprs_expression(self, 0);

        eccprs_expectToken(self, ')');

        self->preferInteger = 0;

        eccprs_pushDepth(self, ECC_ConstKey_none, 2);
        body = eccprs_statement(self);
        eccprs_popDepth(self);

        return ECCNSOpList.createLoop(oplist, condition, increment, body, 0);
    }
}

static eccoplist_t* eccprs_continueStatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;
    eccindexkey_t label = ECC_ConstKey_none;
    ecctextstring_t labelText = self->lexer->text;
    uint16_t depth, lastestDepth, breaker = 0;

    if(!self->lexer->didLineBreak && eccprs_previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        label = self->lexer->value.data.key;
        eccprs_nextToken(self);
    }
    eccprs_semicolon(self);

    depth = self->depthCount;
    if(!depth)
        eccprs_syntaxError(self, text, ECCNSChars.create("continue must be inside loop"));

    lastestDepth = 0;
    while(depth--)
    {
        breaker += self->depths[depth].depth;
        if(self->depths[depth].depth)
            lastestDepth = self->depths[depth].depth;

        if(lastestDepth == 2)
            if(ECCNSKey.isEqual(label, ECC_ConstKey_none) || ECCNSKey.isEqual(label, self->depths[depth].key))
                return ECCNSOpList.create(ECCNSOperand.breaker, ECCNSValue.integer(breaker - 1), self->lexer->text);
    }
    eccprs_syntaxError(self, labelText, ECCNSChars.create("label not found"));
    return oplist;
}

static eccoplist_t* eccprs_breakStatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;
    eccindexkey_t label = ECC_ConstKey_none;
    ecctextstring_t labelText = self->lexer->text;
    uint16_t depth, breaker = 0;

    if(!self->lexer->didLineBreak && eccprs_previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        label = self->lexer->value.data.key;
        eccprs_nextToken(self);
    }
    eccprs_semicolon(self);

    depth = self->depthCount;
    if(!depth)
        eccprs_syntaxError(self, text, ECCNSChars.create("break must be inside loop or switch"));

    while(depth--)
    {
        breaker += self->depths[depth].depth;
        if(ECCNSKey.isEqual(label, ECC_ConstKey_none) || ECCNSKey.isEqual(label, self->depths[depth].key))
            return ECCNSOpList.create(ECCNSOperand.breaker, ECCNSValue.integer(breaker), self->lexer->text);
    }
    eccprs_syntaxError(self, labelText, ECCNSChars.create("label not found"));
    return oplist;
}

static eccoplist_t* eccprs_returnStatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;

    if(self->sourceDepth <= 1)
        eccprs_syntaxError(self, text, ECCNSChars.create("return not in function"));

    if(!self->lexer->didLineBreak && eccprs_previewToken(self) != ';' && eccprs_previewToken(self) != '}' && eccprs_previewToken(self) != ECC_TOK_NO)
        oplist = eccprs_expression(self, 0);

    eccprs_semicolon(self);

    if(!oplist)
        oplist = ECCNSOpList.create(ECCNSOperand.value, ECCValConstUndefined, ECCNSText.join(text, self->lexer->text));

    oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.result, ECCValConstUndefined, ECCNSText.join(text, oplist->ops->text)), oplist);
    return oplist;
}

static eccoplist_t* eccprs_switchStatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *conditionOps = NULL, *defaultOps = NULL;
    ecctextstring_t text = ECC_ConstString_Empty;
    uint32_t conditionCount = 0;

    eccprs_expectToken(self, '(');
    conditionOps = eccprs_expression(self, 0);
    eccprs_expectToken(self, ')');
    eccprs_expectToken(self, '{');
    eccprs_pushDepth(self, ECC_ConstKey_none, 1);

    while(eccprs_previewToken(self) != '}' && eccprs_previewToken(self) != ECC_TOK_ERROR && eccprs_previewToken(self) != ECC_TOK_NO)
    {
        text = self->lexer->text;

        if(eccprs_acceptToken(self, ECC_TOK_CASE))
        {
            conditionOps = ECCNSOpList.join(conditionOps, eccprs_expression(self, 0));
            conditionOps
            = ECCNSOpList.append(conditionOps, ECCNSOperand.make(ECCNSOperand.value, ECCNSValue.integer(2 + (oplist ? oplist->count : 0)), ECC_ConstString_Empty));
            ++conditionCount;
            eccprs_expectToken(self, ':');
            oplist = ECCNSOpList.join(oplist, eccprs_statementList(self));
        }
        else if(eccprs_acceptToken(self, ECC_TOK_DEFAULT))
        {
            if(!defaultOps)
            {
                defaultOps = ECCNSOpList.create(ECCNSOperand.jump, ECCNSValue.integer(1 + (oplist ? oplist->count : 0)), text);
                eccprs_expectToken(self, ':');
                oplist = ECCNSOpList.join(oplist, eccprs_statementList(self));
            }
            else
                eccprs_syntaxError(self, text, ECCNSChars.create("more than one switch default"));
        }
        else
            eccprs_syntaxError(self, text, ECCNSChars.create("invalid switch statement"));
    }

    if(!defaultOps)
        defaultOps = ECCNSOpList.create(ECCNSOperand.noop, ECCValConstNone, ECC_ConstString_Empty);

    oplist = ECCNSOpList.appendNoop(oplist);
    defaultOps = ECCNSOpList.append(defaultOps, ECCNSOperand.make(ECCNSOperand.jump, ECCNSValue.integer(oplist ? oplist->count : 0), ECC_ConstString_Empty));
    conditionOps = ECCNSOpList.unshiftJoin(
    ECCNSOperand.make(ECCNSOperand.switchOp, ECCNSValue.integer(conditionOps ? conditionOps->count : 0), ECC_ConstString_Empty), conditionOps, defaultOps);
    oplist = ECCNSOpList.join(conditionOps, oplist);

    eccprs_popDepth(self);
    eccprs_expectToken(self, '}');
    return oplist;
}

static eccoplist_t* eccprs_allStatement(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    ecctextstring_t text = self->lexer->text;

    if(eccprs_previewToken(self) == '{')
        return eccprs_block(self);
    else if(eccprs_acceptToken(self, ECC_TOK_VAR))
    {
        oplist = eccprs_variableDeclarationList(self, 0);
        eccprs_semicolon(self);
        return oplist;
    }
    else if(eccprs_acceptToken(self, ';'))
        return ECCNSOpList.create(ECCNSOperand.next, ECCValConstUndefined, text);
    else if(eccprs_acceptToken(self, ECC_TOK_IF))
        return eccprs_ifStatement(self);
    else if(eccprs_acceptToken(self, ECC_TOK_DO))
        return eccprs_doStatement(self);
    else if(eccprs_acceptToken(self, ECC_TOK_WHILE))
        return eccprs_whileStatement(self);
    else if(eccprs_acceptToken(self, ECC_TOK_FOR))
        return eccprs_forStatement(self);
    else if(eccprs_acceptToken(self, ECC_TOK_CONTINUE))
        return eccprs_continueStatement(self, text);
    else if(eccprs_acceptToken(self, ECC_TOK_BREAK))
        return eccprs_breakStatement(self, text);
    else if(eccprs_acceptToken(self, ECC_TOK_RETURN))
        return eccprs_returnStatement(self, text);
    else if(eccprs_acceptToken(self, ECC_TOK_WITH))
    {
        if(self->strictMode)
            eccprs_syntaxError(self, text, ECCNSChars.create("code may not contain 'with' statements"));

        oplist = eccprs_expression(self, 0);
        if(!oplist)
            eccprs_tokenError(self, "expression");

        oplist = ECCNSOpList.join(oplist, ECCNSOpList.appendNoop(eccprs_statement(self)));
        oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.with, ECCNSValue.integer(oplist->count), ECC_ConstString_Empty), oplist);

        return oplist;
    }
    else if(eccprs_acceptToken(self, ECC_TOK_SWITCH))
        return eccprs_switchStatement(self);
    else if(eccprs_acceptToken(self, ECC_TOK_THROW))
    {
        if(!self->lexer->didLineBreak && eccprs_previewToken(self) != ';' && eccprs_previewToken(self) != '}' && eccprs_previewToken(self) != ECC_TOK_NO)
            oplist = eccprs_expression(self, 0);

        if(!oplist)
            eccprs_syntaxError(self, text, ECCNSChars.create("throw statement is missing an expression"));

        eccprs_semicolon(self);
        return ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.doThrow, ECCValConstUndefined, ECCNSText.join(text, ECCNSOpList.text(oplist))), oplist);
    }
    else if(eccprs_acceptToken(self, ECC_TOK_TRY))
    {
        oplist = ECCNSOpList.appendNoop(eccprs_block(self));
        oplist = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.doTry, ECCNSValue.integer(oplist->count), text), oplist);

        if(eccprs_previewToken(self) != ECC_TOK_CATCH && eccprs_previewToken(self) != ECC_TOK_FINALLY)
            eccprs_tokenError(self, "catch or finally");

        if(eccprs_acceptToken(self, ECC_TOK_CATCH))
        {
            eccoperand_t identiferOp;
            eccoplist_t* catchOps;

            eccprs_expectToken(self, '(');
            if(eccprs_previewToken(self) != ECC_TOK_IDENTIFIER)
                eccprs_syntaxError(self, text, ECCNSChars.create("missing identifier in catch"));

            identiferOp = eccprs_identifier(self);
            eccprs_expectToken(self, ')');

            catchOps = eccprs_block(self);
            catchOps = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.pushEnvironment, ECCNSValue.key(identiferOp.value.data.key), text), catchOps);
            catchOps = ECCNSOpList.append(catchOps, ECCNSOperand.make(ECCNSOperand.popEnvironment, ECCValConstUndefined, text));
            catchOps = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.jump, ECCNSValue.integer(catchOps->count), text), catchOps);
            oplist = ECCNSOpList.join(oplist, catchOps);
        }
        else
            oplist = ECCNSOpList.append(ECCNSOpList.append(oplist, ECCNSOperand.make(ECCNSOperand.jump, ECCNSValue.integer(1), text)),
                                             ECCNSOperand.make(ECCNSOperand.noop, ECCValConstUndefined, text));

        if(eccprs_acceptToken(self, ECC_TOK_FINALLY))
            oplist = ECCNSOpList.join(oplist, eccprs_block(self));

        return ECCNSOpList.appendNoop(oplist);
    }
    else if(eccprs_acceptToken(self, ECC_TOK_DEBUGGER))
    {
        eccprs_semicolon(self);
        return ECCNSOpList.create(ECCNSOperand.debugger, ECCValConstUndefined, text);
    }
    else
    {
        uint32_t index;

        oplist = eccprs_expression(self, 0);
        if(!oplist)
            return NULL;

        if(oplist->ops[0].native == ECCNSOperand.getLocal && oplist->count == 1 && eccprs_acceptToken(self, ':'))
        {
            eccprs_pushDepth(self, oplist->ops[0].value.data.key, 0);
            free(oplist), oplist = NULL;
            oplist = eccprs_statement(self);
            eccprs_popDepth(self);
            return oplist;
        }

        eccprs_acceptToken(self, ';');

        index = oplist->count;
        while(index--)
            if(oplist->ops[index].native == ECCNSOperand.call)
                return ECCNSOpList.unshift(ECCNSOperand.make(self->sourceDepth <= 1 ? ECCNSOperand.autoreleaseExpression : ECCNSOperand.autoreleaseDiscard,
                                                                  ECCValConstUndefined, ECC_ConstString_Empty),
                                                oplist);

        return ECCNSOpList.unshift(
        ECCNSOperand.make(self->sourceDepth <= 1 ? ECCNSOperand.expression : ECCNSOperand.discard, ECCValConstUndefined, ECC_ConstString_Empty), oplist);
    }
}

static eccoplist_t* eccprs_statement(eccastparser_t* self)
{
    eccoplist_t* oplist = eccprs_allStatement(self);
    if(oplist && oplist->count > 1)
        oplist->ops[oplist->ops[0].text.length ? 0 : 1].text.flags |= ECC_TEXTFLAG_BREAKFLAG;

    return oplist;
}

// MARK: ECCNSFunction

static eccoplist_t* eccprs_parameters(eccastparser_t* self, int* count)
{
    eccoperand_t op;
    *count = 0;
    if(eccprs_previewToken(self) != ')')
        do
        {
            ++*count;
            op = eccprs_identifier(self);

            if(op.value.data.key.data.integer)
            {
                if(self->strictMode && ECCNSKey.isEqual(op.value.data.key, ECC_ConstKey_eval))
                    eccprs_syntaxError(self, op.text, ECCNSChars.create("redefining eval is not allowed"));
                else if(self->strictMode && ECCNSKey.isEqual(op.value.data.key, ECC_ConstKey_arguments))
                    eccprs_syntaxError(self, op.text, ECCNSChars.create("redefining arguments is not allowed"));

                ECCNSObject.deleteMember(&self->function->environment, op.value.data.key);
                ECCNSObject.addMember(&self->function->environment, op.value.data.key, ECCValConstUndefined, ECC_VALFLAG_HIDDEN);
            }
        } while(eccprs_acceptToken(self, ','));

    return NULL;
}

static eccoplist_t* eccprs_function(eccastparser_t* self, int isDeclaration, int isGetter, int isSetter)
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
        eccprs_expectToken(self, ECC_TOK_FUNCTION);

        if(eccprs_previewToken(self) == ECC_TOK_IDENTIFIER)
        {
            identifierOp = eccprs_identifier(self);

            if(self->strictMode && ECCNSKey.isEqual(identifierOp.value.data.key, ECC_ConstKey_eval))
                eccprs_syntaxError(self, identifierOp.text, ECCNSChars.create("redefining eval is not allowed"));
            else if(self->strictMode && ECCNSKey.isEqual(identifierOp.value.data.key, ECC_ConstKey_arguments))
                eccprs_syntaxError(self, identifierOp.text, ECCNSChars.create("redefining arguments is not allowed"));
        }
        else if(isDeclaration)
        {
            eccprs_syntaxError(self, self->lexer->text, ECCNSChars.create("function statement requires a name"));
            return NULL;
        }
    }

    parentFunction = self->function;
    parentFunction->flags |= ECC_SCRIPTFUNCFLAG_NEEDHEAP;

    function = ECCNSFunction.create(&self->function->environment);

    arguments = (ecchashmap_t*)ECCNSObject.addMember(&function->environment, ECC_ConstKey_arguments, ECCValConstUndefined, 0);
    slot = arguments - function->environment.hashmap;

    self->function = function;
    text = self->lexer->text;
    eccprs_expectToken(self, '(');
    textParameter = self->lexer->text;
    oplist = ECCNSOpList.join(oplist, eccprs_parameters(self, &parameterCount));

    function->environment.hashmap[slot].value = ECCValConstUndefined;
    function->environment.hashmap[slot].value.key = ECC_ConstKey_arguments;
    function->environment.hashmap[slot].value.flags |= ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED;

    if(isGetter && parameterCount != 0)
        eccprs_syntaxError(self, ECCNSText.make(textParameter.bytes, (int32_t)(self->lexer->text.bytes - textParameter.bytes)),
                    ECCNSChars.create("getter functions must have no arguments"));
    else if(isSetter && parameterCount != 1)
        eccprs_syntaxError(self, ECCNSText.make(self->lexer->text.bytes, 0), ECCNSChars.create("setter functions must have one argument"));

    eccprs_expectToken(self, ')');
    eccprs_expectToken(self, '{');

    if(parentFunction->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE)
        self->function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    oplist = ECCNSOpList.join(oplist, eccprs_sourceElements(self));
    text.length = (int32_t)(self->lexer->text.bytes - text.bytes) + 1;
    eccprs_expectToken(self, '}');
    self->function = parentFunction;

    function->oplist = oplist;
    function->text = text;
    function->parameterCount = parameterCount;

    ECCNSObject.addMember(&function->object, ECC_ConstKey_length, ECCNSValue.integer(parameterCount), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    value = ECCNSValue.function(function);

    if(isDeclaration)
    {
        if(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE || self->sourceDepth > 1)
            ECCNSObject.addMember(&parentFunction->environment, identifierOp.value.data.key, ECCValConstUndefined, ECC_VALFLAG_HIDDEN);
        else
            ECCNSObject.addMember(self->global, identifierOp.value.data.key, ECCValConstUndefined, ECC_VALFLAG_HIDDEN);
    }
    else if(identifierOp.value.type != ECC_VALTYPE_UNDEFINED && !isGetter && !isSetter)
    {
        ECCNSObject.addMember(&function->environment, identifierOp.value.data.key, value, ECC_VALFLAG_HIDDEN);
        ECCNSObject.packValue(&function->environment);
    }

    if(isGetter)
        value.flags |= ECC_VALFLAG_GETTER;
    else if(isSetter)
        value.flags |= ECC_VALFLAG_SETTER;

    if(isDeclaration)
        return ECCNSOpList.append(ECCNSOpList.create(ECCNSOperand.setLocal, identifierOp.value, ECC_ConstString_Empty),
                                       ECCNSOperand.make(ECCNSOperand.function, value, text));
    else
        return ECCNSOpList.create(ECCNSOperand.function, value, text);
}

// MARK: Source

static eccoplist_t* eccprs_sourceElements(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;

    ++self->sourceDepth;

    self->function->oplist = NULL;

    if(eccprs_previewToken(self) == ECC_TOK_STRING && self->lexer->text.length == 10 && !memcmp("use strict", self->lexer->text.bytes, 10))
        self->function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    oplist = eccprs_statementList(self);

    if(self->sourceDepth <= 1)
        oplist = ECCNSOpList.appendNoop(oplist);
    else
        oplist = ECCNSOpList.append(oplist, ECCNSOperand.make(ECCNSOperand.resultVoid, ECCValConstUndefined, ECC_ConstString_Empty));

    if(self->function->oplist)
        self->function->oplist = ECCNSOpList.joinDiscarded(NULL, self->function->oplist->count / 2, self->function->oplist);

    oplist = ECCNSOpList.join(self->function->oplist, oplist);

    oplist->ops->text.flags |= ECC_TEXTFLAG_BREAKFLAG;
    if(oplist->count > 1)
        oplist->ops[1].text.flags |= ECC_TEXTFLAG_BREAKFLAG;

    ECCNSObject.packValue(&self->function->environment);

    --self->sourceDepth;

    return oplist;
}

// MARK: - Methods

eccastparser_t* nsparsrfn_createWithLexer(eccastlexer_t* lexer)
{
    eccastparser_t* self = malloc(sizeof(*self));
    *self = ECCNSParser.identity;

    self->lexer = lexer;

    return self;
}

void nsparsrfn_destroy(eccastparser_t* self)
{
    assert(self);

    ECCNSLexer.destroy(self->lexer), self->lexer = NULL;
    free(self->depths), self->depths = NULL;
    free(self), self = NULL;
}

eccobjfunction_t* nsparsrfn_parseWithEnvironment(eccastparser_t* const self, eccobject_t* environment, eccobject_t* global)
{
    eccobjfunction_t* function;
    eccoplist_t* oplist;

    assert(self);

    function = ECCNSFunction.create(environment);
    self->function = function;
    self->global = global;
    self->reserveGlobalSlots = 0;
    if(self->strictMode)
        function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    eccprs_nextToken(self);
    oplist = eccprs_sourceElements(self);
    ECCNSOpList.optimizeWithEnvironment(oplist, &function->environment, 0);

    ECCNSObject.reserveSlots(global, self->reserveGlobalSlots);

    if(self->error)
    {
        eccoperand_t errorOps[] = {
            { ECCNSOperand.doThrow, ECCValConstUndefined, self->error->text },
            { ECCNSOperand.value, ECCNSValue.error(self->error), {} },
        };
        errorOps->text.flags |= ECC_TEXTFLAG_BREAKFLAG;

        ECCNSOpList.destroy(oplist), oplist = NULL;
        oplist = malloc(sizeof(*oplist));
        oplist->ops = malloc(sizeof(errorOps));
        oplist->count = sizeof(errorOps) / sizeof(*errorOps);
        memcpy(oplist->ops, errorOps, sizeof(errorOps));
    }

    function->oplist = oplist;
    return function;
}
