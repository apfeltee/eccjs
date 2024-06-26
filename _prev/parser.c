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

static eccoplist_t* new(eccastparser_t*);
static eccoplist_t* assignment(eccastparser_t*, int noIn);
static eccoplist_t* expression(eccastparser_t*, int noIn);
static eccoplist_t* statement(eccastparser_t*);
static eccoplist_t* function(eccastparser_t*, int isDeclaration, int isGetter, int isSetter);
static eccoplist_t* sourceElements(eccastparser_t*);

// MARK: Token

static eccastparser_t* createWithLexer(eccastlexer_t*);
static void destroy(eccastparser_t*);
static eccobjscriptfunction_t* parseWithEnvironment(eccastparser_t* const, eccobject_t* environment, eccobject_t* global);
const struct eccpseudonsparser_t io_libecc_Parser = {
    createWithLexer,
    destroy,
    parseWithEnvironment,
    {}
};

static inline eccasttoktype_t previewToken(eccastparser_t* self)
{
    return self->previewToken;
}

static inline eccasttoktype_t nextToken(eccastparser_t* self)
{
    if(self->previewToken != ECC_TOK_ERROR)
    {
        self->previewToken = io_libecc_Lexer.nextToken(self->lexer);

        if(self->previewToken == ECC_TOK_ERROR)
            self->error = self->lexer->parsedvalue.data.error;
    }
    return self->previewToken;
}

static void parseError(eccastparser_t* self, eccobjerror_t* error)
{
    if(!self->error)
    {
        self->error = error;
        self->previewToken = ECC_TOK_ERROR;
    }
}

static void syntaxError(eccastparser_t* self, ecctextstring_t text, ecccharbuffer_t* message)
{
    parseError(self, io_libecc_Error.syntaxError(text, message));
}

static void referenceError(eccastparser_t* self, ecctextstring_t text, ecccharbuffer_t* message)
{
    parseError(self, io_libecc_Error.referenceError(text, message));
}

static eccoplist_t* tokenError(eccastparser_t* self, const char* t)
{
    char b[4];

    if(!previewToken(self) || previewToken(self) >= ECC_TOK_ERROR)
        syntaxError(self, self->lexer->text, io_libecc_Chars.create("expected %s, got %s", t, io_libecc_Lexer.tokenChars(previewToken(self), b)));
    else
        syntaxError(self, self->lexer->text, io_libecc_Chars.create("expected %s, got '%.*s'", t, self->lexer->text.length, self->lexer->text.bytes));

    return NULL;
}

static inline int acceptToken(eccastparser_t* self, eccasttoktype_t token)
{
    if(previewToken(self) != token)
        return 0;

    nextToken(self);
    return 1;
}

static inline int expectToken(eccastparser_t* self, eccasttoktype_t token)
{
    if(previewToken(self) != token)
    {
        char b[4];
        const char* type = io_libecc_Lexer.tokenChars(token, b);
        tokenError(self, type);
        return 0;
    }

    nextToken(self);
    return 1;
}

// MARK: Depth

static void pushDepth(eccastparser_t* self, eccindexkey_t key, char depth)
{
    self->depths = realloc(self->depths, (self->depthCount + 1) * sizeof(*self->depths));
    self->depths[self->depthCount].key = key;
    self->depths[self->depthCount].depth = depth;
    ++self->depthCount;
}

static void popDepth(eccastparser_t* self)
{
    --self->depthCount;
}

// MARK: Expression

static eccoplist_t* foldConstant(eccastparser_t* self, eccoplist_t* oplist)
{
    eccscriptcontext_t ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
    eccstate_t context = { oplist->ops, .ecc = &ecc };
    eccvalue_t value = context.ops->native(&context);
    ecctextstring_t text = io_libecc_OpList.text(oplist);
    io_libecc_OpList.destroy(oplist);
    return io_libecc_OpList.create(io_libecc_Op.value, value, text);
}

static eccoplist_t* useBinary(eccastparser_t* self, eccoplist_t* oplist, int add)
{
    if(oplist && oplist->ops[0].native == io_libecc_Op.value && (ECCNSValue.isNumber(oplist->ops[0].value) || !add))
    {
        eccscriptcontext_t ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
        eccstate_t context = { oplist->ops, .ecc = &ecc };
        oplist->ops[0].value = ECCNSValue.toBinary(&context, oplist->ops[0].value);
    }
    return oplist;
}

static eccoplist_t* useInteger(eccastparser_t* self, eccoplist_t* oplist)
{
    if(oplist && oplist->ops[0].native == io_libecc_Op.value)
    {
        eccscriptcontext_t ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
        eccstate_t context = { oplist->ops, .ecc = &ecc };
        oplist->ops[0].value = ECCNSValue.toInteger(&context, oplist->ops[0].value);
    }
    return oplist;
}

static eccoplist_t* expressionRef(eccastparser_t* self, eccoplist_t* oplist, const char* name)
{
    if(!oplist)
        return NULL;

    if(oplist->ops[0].native == io_libecc_Op.getLocal && oplist->count == 1)
    {
        if(oplist->ops[0].value.type == ECC_VALTYPE_KEY)
        {
            if(io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_eval))
                syntaxError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create(name));
            else if(io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_arguments))
                syntaxError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create(name));
        }

        oplist->ops[0].native = io_libecc_Op.getLocalRef;
    }
    else if(oplist->ops[0].native == io_libecc_Op.getMember)
        oplist->ops[0].native = io_libecc_Op.getMemberRef;
    else if(oplist->ops[0].native == io_libecc_Op.getProperty)
        oplist->ops[0].native = io_libecc_Op.getPropertyRef;
    else
        referenceError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("%s", name));

    return oplist;
}

static void semicolon(eccastparser_t* self)
{
    if(previewToken(self) == ';')
    {
        nextToken(self);
        return;
    }
    else if(self->lexer->didLineBreak || previewToken(self) == '}' || previewToken(self) == ECC_TOK_NO)
        return;

    syntaxError(self, self->lexer->text, io_libecc_Chars.create("missing ; before statement"));
}

static eccoperand_t identifier(eccastparser_t* self)
{
    eccvalue_t value = self->lexer->parsedvalue;
    ecctextstring_t text = self->lexer->text;
    if(!expectToken(self, ECC_TOK_IDENTIFIER))
        return (eccoperand_t){ 0 };

    return io_libecc_Op.make(io_libecc_Op.value, value, text);
}

static eccoplist_t* array(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    uint32_t count = 0;
    ecctextstring_t text = self->lexer->text;

    nextToken(self);

    do
    {
        while(previewToken(self) == ',')
        {
            ++count;
            oplist = io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.value, ECCValConstNone, self->lexer->text));
            nextToken(self);
        }

        if(previewToken(self) == ']')
            break;

        ++count;
        oplist = io_libecc_OpList.join(oplist, assignment(self, 0));
    } while(acceptToken(self, ','));

    text = ECCNSText.join(text, self->lexer->text);
    expectToken(self, ']');

    return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.array, ECCNSValue.integer(count), text), oplist);
}

static eccoplist_t* propertyAssignment(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    int isGetter = 0, isSetter = 0;

    if(previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        if(io_libecc_Key.isEqual(self->lexer->parsedvalue.data.key, io_libecc_key_get))
        {
            nextToken(self);
            if(previewToken(self) == ':')
            {
                oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.key(io_libecc_key_get), self->lexer->text);
                goto skipProperty;
            }
            else
                isGetter = 1;
        }
        else if(io_libecc_Key.isEqual(self->lexer->parsedvalue.data.key, io_libecc_key_set))
        {
            nextToken(self);
            if(previewToken(self) == ':')
            {
                oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.key(io_libecc_key_set), self->lexer->text);
                goto skipProperty;
            }
            else
                isSetter = 1;
        }
    }

    if(previewToken(self) == ECC_TOK_INTEGER)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->parsedvalue, self->lexer->text);
    else if(previewToken(self) == ECC_TOK_BINARY)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.key(io_libecc_Key.makeWithText(self->lexer->text, 0)), self->lexer->text);
    else if(previewToken(self) == ECC_TOK_STRING)
    {
        uint32_t element = io_libecc_Lexer.scanElement(self->lexer->text);
        if(element < UINT32_MAX)
            oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.integer(element), self->lexer->text);
        else
            oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.key(io_libecc_Key.makeWithText(self->lexer->text, 0)), self->lexer->text);
    }
    else if(previewToken(self) == ECC_TOK_ESCAPEDSTRING)
    {
        //if(&self->lexer->parsedvalue != &ECCValConstUndefined)
        
        {
            ecctextstring_t text = ECCNSText.make(self->lexer->parsedvalue.data.charbufdata->bytes, self->lexer->parsedvalue.data.charbufdata->length);
            uint32_t element = io_libecc_Lexer.scanElement(text);
            if(element < UINT32_MAX)
                oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.integer(element), self->lexer->text);
            else
                oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.key(io_libecc_Key.makeWithText(*self->lexer->parsedvalue.data.text, 0)), self->lexer->text);
        }
    }
    else if(previewToken(self) == ECC_TOK_IDENTIFIER)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->parsedvalue, self->lexer->text);
    else
    {
        expectToken(self, ECC_TOK_IDENTIFIER);
        return NULL;
    }

    nextToken(self);

    if(isGetter)
        return io_libecc_OpList.join(oplist, function(self, 0, 1, 0));
    else if(isSetter)
        return io_libecc_OpList.join(oplist, function(self, 0, 0, 1));

skipProperty:
    expectToken(self, ':');
    return io_libecc_OpList.join(oplist, assignment(self, 0));
}

static eccoplist_t* object(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    uint32_t count = 0;
    ecctextstring_t text = self->lexer->text;

    do
    {
        self->lexer->disallowKeyword = 1;
        nextToken(self);
        self->lexer->disallowKeyword = 0;

        if(previewToken(self) == '}')
            break;

        ++count;
        oplist = io_libecc_OpList.join(oplist, propertyAssignment(self));
    } while(previewToken(self) == ',');

    text = ECCNSText.join(text, self->lexer->text);
    expectToken(self, '}');

    return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.object, ECCNSValue.integer(count), text), oplist);
}

static eccoplist_t* primary(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;

    if(previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        oplist = io_libecc_OpList.create(io_libecc_Op.getLocal, self->lexer->parsedvalue, self->lexer->text);

        if(io_libecc_Key.isEqual(self->lexer->parsedvalue.data.key, io_libecc_key_arguments))
            self->function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_NEEDHEAP;
    }
    else if(previewToken(self) == ECC_TOK_STRING)
        oplist = io_libecc_OpList.create(io_libecc_Op.text, ECCValConstUndefined, self->lexer->text);
    else if(previewToken(self) == ECC_TOK_ESCAPEDSTRING)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->parsedvalue, self->lexer->text);
    else if(previewToken(self) == ECC_TOK_BINARY)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->parsedvalue, self->lexer->text);
    else if(previewToken(self) == ECC_TOK_INTEGER)
    {
        if(self->preferInteger)
            oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->parsedvalue, self->lexer->text);
        else
            oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.binary(self->lexer->parsedvalue.data.integer), self->lexer->text);
    }
    else if(previewToken(self) == ECC_TOK_THIS)
        oplist = io_libecc_OpList.create(io_libecc_Op.getThis, ECCValConstUndefined, self->lexer->text);
    else if(previewToken(self) == ECC_TOK_NULL)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCValConstNull, self->lexer->text);
    else if(previewToken(self) == ECC_TOK_TRUE)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.truth(1), self->lexer->text);
    else if(previewToken(self) == ECC_TOK_FALSE)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.truth(0), self->lexer->text);
    else if(previewToken(self) == '{')
        return object(self);
    else if(previewToken(self) == '[')
        return array(self);
    else if(acceptToken(self, '('))
    {
        oplist = expression(self, 0);
        expectToken(self, ')');
        return oplist;
    }
    else
    {
        if(self->lexer->text.bytes[0] == '/')
        {
            self->lexer->allowRegex = 1;
            self->lexer->offset -= self->lexer->text.length;
            nextToken(self);
            self->lexer->allowRegex = 0;

            if(previewToken(self) != ECC_TOK_REGEXP)
                tokenError(self, "io_libecc_RegExp");
        }

        if(previewToken(self) == ECC_TOK_REGEXP)
            oplist = io_libecc_OpList.create(io_libecc_Op.regexp, ECCValConstUndefined, self->lexer->text);
        else
            return NULL;
    }

    nextToken(self);

    return oplist;
}

static eccoplist_t* arguments(eccastparser_t* self, int* count)
{
    eccoplist_t *oplist = NULL, *argumentOps;
    *count = 0;
    if(previewToken(self) != ')')
        do
        {
            argumentOps = assignment(self, 0);
            if(!argumentOps)
                tokenError(self, "expression");

            ++*count;
            oplist = io_libecc_OpList.join(oplist, argumentOps);
        } while(acceptToken(self, ','));

    return oplist;
}

static eccoplist_t* member(eccastparser_t* self)
{
    eccoplist_t* oplist = new(self);
    ecctextstring_t text;
    while(1)
    {
        if(previewToken(self) == '.')
        {
            eccvalue_t value;

            self->lexer->disallowKeyword = 1;
            nextToken(self);
            self->lexer->disallowKeyword = 0;

            value = self->lexer->parsedvalue;
            text = ECCNSText.join(io_libecc_OpList.text(oplist), self->lexer->text);
            if(!expectToken(self, ECC_TOK_IDENTIFIER))
                return oplist;

            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getMember, value, text), oplist);
        }
        else if(acceptToken(self, '['))
        {
            oplist = io_libecc_OpList.join(oplist, expression(self, 0));
            text = ECCNSText.join(io_libecc_OpList.text(oplist), self->lexer->text);
            if(!expectToken(self, ']'))
                return oplist;

            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getProperty, ECCValConstUndefined, text), oplist);
        }
        else
            break;
    }
    return oplist;
}

static eccoplist_t* new(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    ecctextstring_t text = self->lexer->text;

    if(acceptToken(self, ECC_TOK_NEW))
    {
        int count = 0;
        oplist = member(self);
        text = ECCNSText.join(text, io_libecc_OpList.text(oplist));
        if(acceptToken(self, '('))
        {
            oplist = io_libecc_OpList.join(oplist, arguments(self, &count));
            text = ECCNSText.join(text, self->lexer->text);
            expectToken(self, ')');
        }
        return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.construct, ECCNSValue.integer(count), text), oplist);
    }
    else if(previewToken(self) == ECC_TOK_FUNCTION)
        return function(self, 0, 0, 0);
    else
        return primary(self);
}

static eccoplist_t* leftHandSide(eccastparser_t* self)
{
    eccoplist_t* oplist = new(self);
    ecctextstring_t text = io_libecc_OpList.text(oplist);
    eccvalue_t value;

    while(1)
    {
        if(previewToken(self) == '.')
        {
            if(!oplist)
            {
                tokenError(self, "expression");
                return oplist;
            }

            self->lexer->disallowKeyword = 1;
            nextToken(self);
            self->lexer->disallowKeyword = 0;

            value = self->lexer->parsedvalue;
            text = ECCNSText.join(io_libecc_OpList.text(oplist), self->lexer->text);
            if(!expectToken(self, ECC_TOK_IDENTIFIER))
                return oplist;

            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getMember, value, text), oplist);
        }
        else if(acceptToken(self, '['))
        {
            oplist = io_libecc_OpList.join(oplist, expression(self, 0));
            text = ECCNSText.join(io_libecc_OpList.text(oplist), self->lexer->text);
            if(!expectToken(self, ']'))
                return oplist;

            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getProperty, ECCValConstUndefined, text), oplist);
        }
        else if(acceptToken(self, '('))
        {
            int count = 0;

            int isEval = oplist->count == 1 && oplist->ops[0].native == io_libecc_Op.getLocal && io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_eval);
            if(isEval)
            {
                text = ECCNSText.join(io_libecc_OpList.text(oplist), self->lexer->text);
                io_libecc_OpList.destroy(oplist), oplist = NULL;
            }

            oplist = io_libecc_OpList.join(oplist, arguments(self, &count));
            text = ECCNSText.join(ECCNSText.join(text, io_libecc_OpList.text(oplist)), self->lexer->text);

            if(isEval)
                oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.eval, ECCNSValue.integer(count), text), oplist);
            else if(oplist->ops->native == io_libecc_Op.getMember)
                oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.callMember, ECCNSValue.integer(count), text), oplist);
            else if(oplist->ops->native == io_libecc_Op.getProperty)
                oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.callProperty, ECCNSValue.integer(count), text), oplist);
            else
                oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.call, ECCNSValue.integer(count), text), oplist);

            if(!expectToken(self, ')'))
                break;
        }
        else
            break;
    }
    return oplist;
}

static eccoplist_t* postfix(eccastparser_t* self)
{
    eccoplist_t* oplist = leftHandSide(self);
    ecctextstring_t text = self->lexer->text;

    if(!self->lexer->didLineBreak && acceptToken(self, ECC_TOK_INCREMENT))
        oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.postIncrementRef, ECCValConstUndefined, ECCNSText.join(oplist->ops->text, text)),
                                          expressionRef(self, oplist, "invalid increment operand"));
    if(!self->lexer->didLineBreak && acceptToken(self, ECC_TOK_DECREMENT))
        oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.postDecrementRef, ECCValConstUndefined, ECCNSText.join(oplist->ops->text, text)),
                                          expressionRef(self, oplist, "invalid decrement operand"));

    return oplist;
}

static eccoplist_t* unary(eccastparser_t* self)
{
    eccoplist_t *oplist, *alt;
    ecctextstring_t text = self->lexer->text;
    eccnativefuncptr_t native;

    if(acceptToken(self, ECC_TOK_DELETE))
    {
        oplist = unary(self);

        if(oplist && oplist->ops[0].native == io_libecc_Op.getLocal)
        {
            if(self->strictMode)
                syntaxError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("delete of an unqualified identifier"));

            oplist->ops->native = io_libecc_Op.deleteLocal;
        }
        else if(oplist && oplist->ops[0].native == io_libecc_Op.getMember)
            oplist->ops->native = io_libecc_Op.deleteMember;
        else if(oplist && oplist->ops[0].native == io_libecc_Op.getProperty)
            oplist->ops->native = io_libecc_Op.deleteProperty;
        else if(!self->strictMode && oplist)
            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.exchange, ECCValConstTrue, ECC_ConstString_Empty), oplist);
        else if(oplist)
            referenceError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("invalid delete operand"));
        else
            tokenError(self, "expression");

        return oplist;
    }
    else if(acceptToken(self, ECC_TOK_VOID))
        native = io_libecc_Op.exchange, alt = unary(self);
    else if(acceptToken(self, ECC_TOK_TYPEOF))
    {
        native = io_libecc_Op.typeOf, alt = unary(self);
        if(alt->ops->native == io_libecc_Op.getLocal)
            alt->ops->native = io_libecc_Op.getLocalRefOrNull;
    }
    else if(acceptToken(self, ECC_TOK_INCREMENT))
        native = io_libecc_Op.incrementRef, alt = expressionRef(self, unary(self), "invalid increment operand");
    else if(acceptToken(self, ECC_TOK_DECREMENT))
        native = io_libecc_Op.decrementRef, alt = expressionRef(self, unary(self), "invalid decrement operand");
    else if(acceptToken(self, '+'))
        native = io_libecc_Op.positive, alt = useBinary(self, unary(self), 0);
    else if(acceptToken(self, '-'))
        native = io_libecc_Op.negative, alt = useBinary(self, unary(self), 0);
    else if(acceptToken(self, '~'))
        native = io_libecc_Op.invert, alt = useInteger(self, unary(self));
    else if(acceptToken(self, '!'))
        native = io_libecc_Op.doLogicalNot, alt = unary(self);
    else
        return postfix(self);

    if(!alt)
        return tokenError(self, "expression");

    oplist = io_libecc_OpList.unshift(io_libecc_Op.make(native, ECCValConstUndefined, ECCNSText.join(text, alt->ops->text)), alt);

    if(oplist->ops[1].native == io_libecc_Op.value)
        return foldConstant(self, oplist);
    else
        return oplist;
}

static eccoplist_t* multiplicative(eccastparser_t* self)
{
    eccoplist_t *oplist = unary(self), *alt;

    while(1)
    {
        eccnativefuncptr_t native;

        if(previewToken(self) == '*')
            native = io_libecc_Op.multiply;
        else if(previewToken(self) == '/')
            native = io_libecc_Op.divide;
        else if(previewToken(self) == '%')
            native = io_libecc_Op.modulo;
        else
            return oplist;

        if(useBinary(self, oplist, 0))
        {
            nextToken(self);
            if((alt = useBinary(self, unary(self), 0)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == io_libecc_Op.value && oplist->ops[2].native == io_libecc_Op.value)
                    oplist = foldConstant(self, oplist);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
}

static eccoplist_t* additive(eccastparser_t* self)
{
    eccoplist_t *oplist = multiplicative(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(previewToken(self) == '+')
            native = io_libecc_Op.add;
        else if(previewToken(self) == '-')
            native = io_libecc_Op.minus;
        else
            return oplist;

        if(useBinary(self, oplist, native == io_libecc_Op.add))
        {
            nextToken(self);
            if((alt = useBinary(self, multiplicative(self), native == io_libecc_Op.add)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == io_libecc_Op.value && oplist->ops[2].native == io_libecc_Op.value)
                    oplist = foldConstant(self, oplist);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
}

static eccoplist_t* shift(eccastparser_t* self)
{
    eccoplist_t *oplist = additive(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(previewToken(self) == ECC_TOK_LEFTSHIFT)
            native = io_libecc_Op.leftShift;
        else if(previewToken(self) == ECC_TOK_RIGHTSHIFT)
            native = io_libecc_Op.rightShift;
        else if(previewToken(self) == ECC_TOK_UNSIGNEDRIGHTSHIFT)
            native = io_libecc_Op.unsignedRightShift;
        else
            return oplist;

        if(useInteger(self, oplist))
        {
            nextToken(self);
            if((alt = useInteger(self, additive(self))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, ECCValConstUndefined, text), oplist, alt);

                if(oplist->ops[1].native == io_libecc_Op.value && oplist->ops[2].native == io_libecc_Op.value)
                    oplist = foldConstant(self, oplist);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
}

static eccoplist_t* relational(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = shift(self), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(previewToken(self) == '<')
            native = io_libecc_Op.less;
        else if(previewToken(self) == '>')
            native = io_libecc_Op.more;
        else if(previewToken(self) == ECC_TOK_LESSOREQUAL)
            native = io_libecc_Op.lessOrEqual;
        else if(previewToken(self) == ECC_TOK_MOREOREQUAL)
            native = io_libecc_Op.moreOrEqual;
        else if(previewToken(self) == ECC_TOK_INSTANCEOF)
            native = io_libecc_Op.instanceOf;
        else if(!noIn && previewToken(self) == ECC_TOK_IN)
            native = io_libecc_Op.in;
        else
            return oplist;

        if(oplist)
        {
            nextToken(self);
            if((alt = shift(self)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
}

static eccoplist_t* equality(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = relational(self, noIn), *alt;
    while(1)
    {
        eccnativefuncptr_t native;

        if(previewToken(self) == ECC_TOK_EQUAL)
            native = io_libecc_Op.equal;
        else if(previewToken(self) == ECC_TOK_NOTEQUAL)
            native = io_libecc_Op.notEqual;
        else if(previewToken(self) == ECC_TOK_IDENTICAL)
            native = io_libecc_Op.identical;
        else if(previewToken(self) == ECC_TOK_NOTIDENTICAL)
            native = io_libecc_Op.notIdentical;
        else
            return oplist;

        if(oplist)
        {
            nextToken(self);
            if((alt = relational(self, noIn)))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
}

static eccoplist_t* bitwiseAnd(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = equality(self, noIn), *alt;
    while(previewToken(self) == '&')
    {
        if(useInteger(self, oplist))
        {
            nextToken(self);
            if((alt = useInteger(self, equality(self, noIn))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.bitwiseAnd, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* bitwiseXor(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = bitwiseAnd(self, noIn), *alt;
    while(previewToken(self) == '^')
    {
        if(useInteger(self, oplist))
        {
            nextToken(self);
            if((alt = useInteger(self, bitwiseAnd(self, noIn))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.bitwiseXor, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* bitwiseOr(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = bitwiseXor(self, noIn), *alt;
    while(previewToken(self) == '|')
    {
        if(useInteger(self, oplist))
        {
            nextToken(self);
            if((alt = useInteger(self, bitwiseXor(self, noIn))))
            {
                ecctextstring_t text = ECCNSText.join(oplist->ops->text, alt->ops->text);
                oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.bitwiseOr, ECCValConstUndefined, text), oplist, alt);

                continue;
            }
            io_libecc_OpList.destroy(oplist);
        }
        return tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* logicalAnd(eccastparser_t* self, int noIn)
{
    int32_t opCount;
    eccoplist_t *oplist = bitwiseOr(self, noIn), *nextOp = NULL;

    while(acceptToken(self, ECC_TOK_LOGICALAND))
        if(oplist && (nextOp = bitwiseOr(self, noIn)))
        {
            opCount = nextOp->count;
            oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.logicalAnd, ECCNSValue.integer(opCount), io_libecc_OpList.text(oplist)), oplist, nextOp);
        }
        else
            tokenError(self, "expression");

    return oplist;
}

static eccoplist_t* logicalOr(eccastparser_t* self, int noIn)
{
    int32_t opCount;
    eccoplist_t *oplist = logicalAnd(self, noIn), *nextOp = NULL;

    while(acceptToken(self, ECC_TOK_LOGICALOR))
        if(oplist && (nextOp = logicalAnd(self, noIn)))
        {
            opCount = nextOp->count;
            oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.logicalOr, ECCNSValue.integer(opCount), io_libecc_OpList.text(oplist)), oplist, nextOp);
        }
        else
            tokenError(self, "expression");

    return oplist;
}

static eccoplist_t* conditional(eccastparser_t* self, int noIn)
{
    eccoplist_t* oplist = logicalOr(self, noIn);

    if(acceptToken(self, '?'))
    {
        if(oplist)
        {
            eccoplist_t* trueOps = assignment(self, 0);
            eccoplist_t* falseOps;

            if(!expectToken(self, ':'))
                return oplist;

            falseOps = assignment(self, noIn);

            trueOps = io_libecc_OpList.append(trueOps, io_libecc_Op.make(io_libecc_Op.jump, ECCNSValue.integer(falseOps->count), io_libecc_OpList.text(trueOps)));
            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.jumpIfNot, ECCNSValue.integer(trueOps->count), io_libecc_OpList.text(oplist)), oplist);
            oplist = io_libecc_OpList.join3(oplist, trueOps, falseOps);

            return oplist;
        }
        else
            tokenError(self, "expression");
    }
    return oplist;
}

static eccoplist_t* assignment(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = conditional(self, noIn), *opassign = NULL;
    ecctextstring_t text = self->lexer->text;
    eccnativefuncptr_t native = NULL;

    if(acceptToken(self, '='))
    {
        if(!oplist)
        {
            syntaxError(self, text, io_libecc_Chars.create("expected expression, got '='"));
            return NULL;
        }
        else if(oplist->ops[0].native == io_libecc_Op.getLocal && oplist->count == 1)
        {
            if(io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_eval))
                syntaxError(self, text, io_libecc_Chars.create("can't assign to eval"));
            else if(io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_arguments))
                syntaxError(self, text, io_libecc_Chars.create("can't assign to arguments"));

            if(!self->strictMode && !ECCNSObject.member(&self->function->environment, oplist->ops[0].value.data.key, 0))
                ++self->reserveGlobalSlots;
            //				ECCNSObject.addMember(self->global, oplist->ops[0].value.data.key, ECCValConstNone, 0);

            oplist->ops->native = io_libecc_Op.setLocal;
        }
        else if(oplist->ops[0].native == io_libecc_Op.getMember)
            oplist->ops->native = io_libecc_Op.setMember;
        else if(oplist->ops[0].native == io_libecc_Op.getProperty)
            oplist->ops->native = io_libecc_Op.setProperty;
        else
            referenceError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("invalid assignment left-hand side"));

        if((opassign = assignment(self, noIn)))
        {
            oplist->ops->text = ECCNSText.join(oplist->ops->text, opassign->ops->text);
            return io_libecc_OpList.join(oplist, opassign);
        }

        tokenError(self, "expression");
    }
    else if(acceptToken(self, ECC_TOK_MULTIPLYASSIGN))
        native = io_libecc_Op.multiplyAssignRef;
    else if(acceptToken(self, ECC_TOK_DIVIDEASSIGN))
        native = io_libecc_Op.divideAssignRef;
    else if(acceptToken(self, ECC_TOK_MODULOASSIGN))
        native = io_libecc_Op.moduloAssignRef;
    else if(acceptToken(self, ECC_TOK_ADDASSIGN))
        native = io_libecc_Op.addAssignRef;
    else if(acceptToken(self, ECC_TOK_MINUSASSIGN))
        native = io_libecc_Op.minusAssignRef;
    else if(acceptToken(self, ECC_TOK_LEFTSHIFTASSIGN))
        native = io_libecc_Op.leftShiftAssignRef;
    else if(acceptToken(self, ECC_TOK_RIGHTSHIFTASSIGN))
        native = io_libecc_Op.rightShiftAssignRef;
    else if(acceptToken(self, ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN))
        native = io_libecc_Op.unsignedRightShiftAssignRef;
    else if(acceptToken(self, ECC_TOK_ANDASSIGN))
        native = io_libecc_Op.bitAndAssignRef;
    else if(acceptToken(self, ECC_TOK_XORASSIGN))
        native = io_libecc_Op.bitXorAssignRef;
    else if(acceptToken(self, ECC_TOK_ORASSIGN))
        native = io_libecc_Op.bitOrAssignRef;
    else
        return oplist;

    if(oplist)
    {
        if((opassign = assignment(self, noIn)))
            oplist->ops->text = ECCNSText.join(oplist->ops->text, opassign->ops->text);
        else
            tokenError(self, "expression");

        return io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, ECCValConstUndefined, oplist->ops->text),
                                            expressionRef(self, oplist, "invalid assignment left-hand side"), opassign);
    }

    syntaxError(self, text, io_libecc_Chars.create("expected expression, got '%.*s'", text.length, text.bytes));
    return NULL;
}

static eccoplist_t* expression(eccastparser_t* self, int noIn)
{
    eccoplist_t* oplist = assignment(self, noIn);
    while(acceptToken(self, ','))
        oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.discard, ECCValConstUndefined, ECC_ConstString_Empty), oplist, assignment(self, noIn));

    return oplist;
}

// MARK: Statements

static eccoplist_t* statementList(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *statementOps = NULL, *discardOps = NULL;
    uint16_t discardCount = 0;

    while(previewToken(self) != ECC_TOK_ERROR && previewToken(self) != ECC_TOK_NO)
    {
        if(previewToken(self) == ECC_TOK_FUNCTION)
            self->function->oplist = io_libecc_OpList.join(self->function->oplist, function(self, 1, 0, 0));
        else
        {
            if((statementOps = statement(self)))
            {
                while(statementOps->count > 1 && statementOps->ops[0].native == io_libecc_Op.next)
                    io_libecc_OpList.shift(statementOps);

                if(statementOps->count == 1 && statementOps->ops[0].native == io_libecc_Op.next)
                    io_libecc_OpList.destroy(statementOps), statementOps = NULL;
                else
                {
                    if(statementOps->ops[0].native == io_libecc_Op.discard)
                    {
                        ++discardCount;
                        discardOps = io_libecc_OpList.join(discardOps, io_libecc_OpList.shift(statementOps));
                        statementOps = NULL;
                    }
                    else if(discardOps)
                    {
                        oplist = io_libecc_OpList.joinDiscarded(oplist, discardCount, discardOps);
                        discardOps = NULL;
                        discardCount = 0;
                    }

                    oplist = io_libecc_OpList.join(oplist, statementOps);
                }
            }
            else
                break;
        }
    }

    if(discardOps)
        oplist = io_libecc_OpList.joinDiscarded(oplist, discardCount, discardOps);

    return oplist;
}

static eccoplist_t* block(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    expectToken(self, '{');
    if(previewToken(self) == '}')
        oplist = io_libecc_OpList.create(io_libecc_Op.next, ECCValConstUndefined, self->lexer->text);
    else
        oplist = statementList(self);

    expectToken(self, '}');
    return oplist;
}

static eccoplist_t* variableDeclaration(eccastparser_t* self, int noIn)
{
    eccvalue_t value = self->lexer->parsedvalue;
    ecctextstring_t text = self->lexer->text;
    if(!expectToken(self, ECC_TOK_IDENTIFIER))
        return NULL;

    if(self->strictMode && io_libecc_Key.isEqual(value.data.key, io_libecc_key_eval))
        syntaxError(self, text, io_libecc_Chars.create("redefining eval is not allowed"));
    else if(self->strictMode && io_libecc_Key.isEqual(value.data.key, io_libecc_key_arguments))
        syntaxError(self, text, io_libecc_Chars.create("redefining arguments is not allowed"));

    if(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE || self->sourceDepth > 1)
        ECCNSObject.addMember(&self->function->environment, value.data.key, ECCValConstUndefined, ECC_VALFLAG_SEALED);
    else
        ECCNSObject.addMember(self->global, value.data.key, ECCValConstUndefined, ECC_VALFLAG_SEALED);

    if(acceptToken(self, '='))
    {
        eccoplist_t* opassign = assignment(self, noIn);

        if(opassign)
            return io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.discard, ECCValConstUndefined, ECC_ConstString_Empty),
                                                io_libecc_OpList.create(io_libecc_Op.setLocal, value, ECCNSText.join(text, opassign->ops->text)), opassign);

        tokenError(self, "expression");
        return NULL;
    }
    //	else if (!(self->function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE) && self->sourceDepth <= 1)
    //		return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.discard, ECCValConstUndefined, ECC_ConstString_Empty), io_libecc_OpList.create(io_libecc_Op.createLocalRef, value, text));
    else
        return io_libecc_OpList.create(io_libecc_Op.next, value, text);
}

static eccoplist_t* variableDeclarationList(eccastparser_t* self, int noIn)
{
    eccoplist_t *oplist = NULL, *varOps;
    do
    {
        varOps = variableDeclaration(self, noIn);

        if(oplist && varOps && varOps->count == 1 && varOps->ops[0].native == io_libecc_Op.next)
            io_libecc_OpList.destroy(varOps), varOps = NULL;
        else
            oplist = io_libecc_OpList.join(oplist, varOps);
    } while(acceptToken(self, ','));

    return oplist;
}

static eccoplist_t* ifStatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *trueOps = NULL, *falseOps = NULL;
    expectToken(self, '(');
    oplist = expression(self, 0);
    expectToken(self, ')');
    trueOps = statement(self);
    if(!trueOps)
        trueOps = io_libecc_OpList.appendNoop(NULL);

    if(acceptToken(self, ECC_TOK_ELSE))
    {
        falseOps = statement(self);
        if(falseOps)
            trueOps = io_libecc_OpList.append(trueOps, io_libecc_Op.make(io_libecc_Op.jump, ECCNSValue.integer(falseOps->count), io_libecc_OpList.text(trueOps)));
    }
    oplist = io_libecc_OpList.unshiftJoin3(io_libecc_Op.make(io_libecc_Op.jumpIfNot, ECCNSValue.integer(trueOps->count), io_libecc_OpList.text(oplist)), oplist,
                                           trueOps, falseOps);
    return oplist;
}

static eccoplist_t* doStatement(eccastparser_t* self)
{
    eccoplist_t *oplist, *condition;

    pushDepth(self, io_libecc_key_none, 2);
    oplist = statement(self);
    popDepth(self);

    expectToken(self, ECC_TOK_WHILE);
    expectToken(self, '(');
    condition = expression(self, 0);
    expectToken(self, ')');
    acceptToken(self, ';');

    return io_libecc_OpList.createLoop(NULL, condition, NULL, oplist, 1);
}

static eccoplist_t* whileStatement(eccastparser_t* self)
{
    eccoplist_t *oplist, *condition;

    expectToken(self, '(');
    condition = expression(self, 0);
    expectToken(self, ')');

    pushDepth(self, io_libecc_key_none, 2);
    oplist = statement(self);
    popDepth(self);

    return io_libecc_OpList.createLoop(NULL, condition, NULL, oplist, 0);
}

static eccoplist_t* forStatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *condition = NULL, *increment = NULL, *body = NULL;

    expectToken(self, '(');

    self->preferInteger = 1;

    if(acceptToken(self, ECC_TOK_VAR))
        oplist = variableDeclarationList(self, 1);
    else if(previewToken(self) != ';')
    {
        oplist = expression(self, 1);

        if(oplist)
            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.discard, ECCValConstUndefined, io_libecc_OpList.text(oplist)), oplist);
    }

    if(oplist && acceptToken(self, ECC_TOK_IN))
    {
        if(oplist->count == 2 && oplist->ops[0].native == io_libecc_Op.discard && oplist->ops[1].native == io_libecc_Op.getLocal)
        {
            if(!self->strictMode && !ECCNSObject.member(&self->function->environment, oplist->ops[1].value.data.key, 0))
                ++self->reserveGlobalSlots;

            oplist->ops[0].native = io_libecc_Op.iterateInRef;
            oplist->ops[1].native = io_libecc_Op.createLocalRef;
        }
        else if(oplist->count == 1 && oplist->ops[0].native == io_libecc_Op.next)
        {
            oplist->ops->native = io_libecc_Op.createLocalRef;
            oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.iterateInRef, ECCValConstUndefined, self->lexer->text), oplist);
        }
        else
            referenceError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("invalid for/in left-hand side"));

        oplist = io_libecc_OpList.join(oplist, expression(self, 0));
        oplist->ops[0].text = io_libecc_OpList.text(oplist);
        expectToken(self, ')');

        self->preferInteger = 0;

        pushDepth(self, io_libecc_key_none, 2);
        body = statement(self);
        popDepth(self);

        body = io_libecc_OpList.appendNoop(body);
        return io_libecc_OpList.join(io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.value, ECCNSValue.integer(body->count), self->lexer->text)), body);
    }
    else
    {
        expectToken(self, ';');
        if(previewToken(self) != ';')
            condition = expression(self, 0);

        expectToken(self, ';');
        if(previewToken(self) != ')')
            increment = expression(self, 0);

        expectToken(self, ')');

        self->preferInteger = 0;

        pushDepth(self, io_libecc_key_none, 2);
        body = statement(self);
        popDepth(self);

        return io_libecc_OpList.createLoop(oplist, condition, increment, body, 0);
    }
}

static eccoplist_t* continueStatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;
    eccindexkey_t label = io_libecc_key_none;
    ecctextstring_t labelText = self->lexer->text;
    uint16_t depth, lastestDepth, breaker = 0;

    if(!self->lexer->didLineBreak && previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        label = self->lexer->parsedvalue.data.key;
        nextToken(self);
    }
    semicolon(self);

    depth = self->depthCount;
    if(!depth)
        syntaxError(self, text, io_libecc_Chars.create("continue must be inside loop"));

    lastestDepth = 0;
    while(depth--)
    {
        breaker += self->depths[depth].depth;
        if(self->depths[depth].depth)
            lastestDepth = self->depths[depth].depth;

        if(lastestDepth == 2)
            if(io_libecc_Key.isEqual(label, io_libecc_key_none) || io_libecc_Key.isEqual(label, self->depths[depth].key))
                return io_libecc_OpList.create(io_libecc_Op.breaker, ECCNSValue.integer(breaker - 1), self->lexer->text);
    }
    syntaxError(self, labelText, io_libecc_Chars.create("label not found"));
    return oplist;
}

static eccoplist_t* breakStatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;
    eccindexkey_t label = io_libecc_key_none;
    ecctextstring_t labelText = self->lexer->text;
    uint16_t depth, breaker = 0;

    if(!self->lexer->didLineBreak && previewToken(self) == ECC_TOK_IDENTIFIER)
    {
        label = self->lexer->parsedvalue.data.key;
        nextToken(self);
    }
    semicolon(self);

    depth = self->depthCount;
    if(!depth)
        syntaxError(self, text, io_libecc_Chars.create("break must be inside loop or switch"));

    while(depth--)
    {
        breaker += self->depths[depth].depth;
        if(io_libecc_Key.isEqual(label, io_libecc_key_none) || io_libecc_Key.isEqual(label, self->depths[depth].key))
            return io_libecc_OpList.create(io_libecc_Op.breaker, ECCNSValue.integer(breaker), self->lexer->text);
    }
    syntaxError(self, labelText, io_libecc_Chars.create("label not found"));
    return oplist;
}

static eccoplist_t* returnStatement(eccastparser_t* self, ecctextstring_t text)
{
    eccoplist_t* oplist = NULL;

    if(self->sourceDepth <= 1)
        syntaxError(self, text, io_libecc_Chars.create("return not in function"));

    if(!self->lexer->didLineBreak && previewToken(self) != ';' && previewToken(self) != '}' && previewToken(self) != ECC_TOK_NO)
        oplist = expression(self, 0);

    semicolon(self);

    if(!oplist)
        oplist = io_libecc_OpList.create(io_libecc_Op.value, ECCValConstUndefined, ECCNSText.join(text, self->lexer->text));

    oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.result, ECCValConstUndefined, ECCNSText.join(text, oplist->ops->text)), oplist);
    return oplist;
}

static eccoplist_t* switchStatement(eccastparser_t* self)
{
    eccoplist_t *oplist = NULL, *conditionOps = NULL, *defaultOps = NULL;
    ecctextstring_t text = ECC_ConstString_Empty;
    uint32_t conditionCount = 0;

    expectToken(self, '(');
    conditionOps = expression(self, 0);
    expectToken(self, ')');
    expectToken(self, '{');
    pushDepth(self, io_libecc_key_none, 1);

    while(previewToken(self) != '}' && previewToken(self) != ECC_TOK_ERROR && previewToken(self) != ECC_TOK_NO)
    {
        text = self->lexer->text;

        if(acceptToken(self, ECC_TOK_CASE))
        {
            conditionOps = io_libecc_OpList.join(conditionOps, expression(self, 0));
            conditionOps
            = io_libecc_OpList.append(conditionOps, io_libecc_Op.make(io_libecc_Op.value, ECCNSValue.integer(2 + (oplist ? oplist->count : 0)), ECC_ConstString_Empty));
            ++conditionCount;
            expectToken(self, ':');
            oplist = io_libecc_OpList.join(oplist, statementList(self));
        }
        else if(acceptToken(self, ECC_TOK_DEFAULT))
        {
            if(!defaultOps)
            {
                defaultOps = io_libecc_OpList.create(io_libecc_Op.jump, ECCNSValue.integer(1 + (oplist ? oplist->count : 0)), text);
                expectToken(self, ':');
                oplist = io_libecc_OpList.join(oplist, statementList(self));
            }
            else
                syntaxError(self, text, io_libecc_Chars.create("more than one switch default"));
        }
        else
            syntaxError(self, text, io_libecc_Chars.create("invalid switch statement"));
    }

    if(!defaultOps)
        defaultOps = io_libecc_OpList.create(io_libecc_Op.noop, ECCValConstNone, ECC_ConstString_Empty);

    oplist = io_libecc_OpList.appendNoop(oplist);
    defaultOps = io_libecc_OpList.append(defaultOps, io_libecc_Op.make(io_libecc_Op.jump, ECCNSValue.integer(oplist ? oplist->count : 0), ECC_ConstString_Empty));
    conditionOps = io_libecc_OpList.unshiftJoin(
    io_libecc_Op.make(io_libecc_Op.switchOp, ECCNSValue.integer(conditionOps ? conditionOps->count : 0), ECC_ConstString_Empty), conditionOps, defaultOps);
    oplist = io_libecc_OpList.join(conditionOps, oplist);

    popDepth(self);
    expectToken(self, '}');
    return oplist;
}

static eccoplist_t* allStatement(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;
    ecctextstring_t text = self->lexer->text;

    if(previewToken(self) == '{')
        return block(self);
    else if(acceptToken(self, ECC_TOK_VAR))
    {
        oplist = variableDeclarationList(self, 0);
        semicolon(self);
        return oplist;
    }
    else if(acceptToken(self, ';'))
        return io_libecc_OpList.create(io_libecc_Op.next, ECCValConstUndefined, text);
    else if(acceptToken(self, ECC_TOK_IF))
        return ifStatement(self);
    else if(acceptToken(self, ECC_TOK_DO))
        return doStatement(self);
    else if(acceptToken(self, ECC_TOK_WHILE))
        return whileStatement(self);
    else if(acceptToken(self, ECC_TOK_FOR))
        return forStatement(self);
    else if(acceptToken(self, ECC_TOK_CONTINUE))
        return continueStatement(self, text);
    else if(acceptToken(self, ECC_TOK_BREAK))
        return breakStatement(self, text);
    else if(acceptToken(self, ECC_TOK_RETURN))
        return returnStatement(self, text);
    else if(acceptToken(self, ECC_TOK_WITH))
    {
        if(self->strictMode)
            syntaxError(self, text, io_libecc_Chars.create("code may not contain 'with' statements"));

        oplist = expression(self, 0);
        if(!oplist)
            tokenError(self, "expression");

        oplist = io_libecc_OpList.join(oplist, io_libecc_OpList.appendNoop(statement(self)));
        oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.with, ECCNSValue.integer(oplist->count), ECC_ConstString_Empty), oplist);

        return oplist;
    }
    else if(acceptToken(self, ECC_TOK_SWITCH))
        return switchStatement(self);
    else if(acceptToken(self, ECC_TOK_THROW))
    {
        if(!self->lexer->didLineBreak && previewToken(self) != ';' && previewToken(self) != '}' && previewToken(self) != ECC_TOK_NO)
            oplist = expression(self, 0);

        if(!oplist)
            syntaxError(self, text, io_libecc_Chars.create("throw statement is missing an expression"));

        semicolon(self);
        return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.doThrow, ECCValConstUndefined, ECCNSText.join(text, io_libecc_OpList.text(oplist))), oplist);
    }
    else if(acceptToken(self, ECC_TOK_TRY))
    {
        oplist = io_libecc_OpList.appendNoop(block(self));
        oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.doTry, ECCNSValue.integer(oplist->count), text), oplist);

        if(previewToken(self) != ECC_TOK_CATCH && previewToken(self) != ECC_TOK_FINALLY)
            tokenError(self, "catch or finally");

        if(acceptToken(self, ECC_TOK_CATCH))
        {
            eccoperand_t identiferOp;
            eccoplist_t* catchOps;

            expectToken(self, '(');
            if(previewToken(self) != ECC_TOK_IDENTIFIER)
                syntaxError(self, text, io_libecc_Chars.create("missing identifier in catch"));

            identiferOp = identifier(self);
            expectToken(self, ')');

            catchOps = block(self);
            catchOps = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.pushEnvironment, ECCNSValue.key(identiferOp.value.data.key), text), catchOps);
            catchOps = io_libecc_OpList.append(catchOps, io_libecc_Op.make(io_libecc_Op.popEnvironment, ECCValConstUndefined, text));
            catchOps = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.jump, ECCNSValue.integer(catchOps->count), text), catchOps);
            oplist = io_libecc_OpList.join(oplist, catchOps);
        }
        else
            oplist = io_libecc_OpList.append(io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.jump, ECCNSValue.integer(1), text)),
                                             io_libecc_Op.make(io_libecc_Op.noop, ECCValConstUndefined, text));

        if(acceptToken(self, ECC_TOK_FINALLY))
            oplist = io_libecc_OpList.join(oplist, block(self));

        return io_libecc_OpList.appendNoop(oplist);
    }
    else if(acceptToken(self, ECC_TOK_DEBUGGER))
    {
        semicolon(self);
        return io_libecc_OpList.create(io_libecc_Op.debugger, ECCValConstUndefined, text);
    }
    else
    {
        uint32_t index;

        oplist = expression(self, 0);
        if(!oplist)
            return NULL;

        if(oplist->ops[0].native == io_libecc_Op.getLocal && oplist->count == 1 && acceptToken(self, ':'))
        {
            pushDepth(self, oplist->ops[0].value.data.key, 0);
            free(oplist), oplist = NULL;
            oplist = statement(self);
            popDepth(self);
            return oplist;
        }

        acceptToken(self, ';');

        index = oplist->count;
        while(index--)
            if(oplist->ops[index].native == io_libecc_Op.call)
                return io_libecc_OpList.unshift(io_libecc_Op.make(self->sourceDepth <= 1 ? io_libecc_Op.autoreleaseExpression : io_libecc_Op.autoreleaseDiscard,
                                                                  ECCValConstUndefined, ECC_ConstString_Empty),
                                                oplist);

        return io_libecc_OpList.unshift(
        io_libecc_Op.make(self->sourceDepth <= 1 ? io_libecc_Op.expression : io_libecc_Op.discard, ECCValConstUndefined, ECC_ConstString_Empty), oplist);
    }
}

static eccoplist_t* statement(eccastparser_t* self)
{
    eccoplist_t* oplist = allStatement(self);
    if(oplist && oplist->count > 1)
        oplist->ops[oplist->ops[0].text.length ? 0 : 1].text.flags |= ECC_TEXTFLAG_BREAKFLAG;

    return oplist;
}

// MARK: io_libecc_Function

static eccoplist_t* parameters(eccastparser_t* self, int* count)
{
    eccoperand_t op;
    *count = 0;
    if(previewToken(self) != ')')
        do
        {
            ++*count;
            op = identifier(self);

            if(op.value.data.key.data.integer)
            {
                if(self->strictMode && io_libecc_Key.isEqual(op.value.data.key, io_libecc_key_eval))
                    syntaxError(self, op.text, io_libecc_Chars.create("redefining eval is not allowed"));
                else if(self->strictMode && io_libecc_Key.isEqual(op.value.data.key, io_libecc_key_arguments))
                    syntaxError(self, op.text, io_libecc_Chars.create("redefining arguments is not allowed"));

                ECCNSObject.deleteMember(&self->function->environment, op.value.data.key);
                ECCNSObject.addMember(&self->function->environment, op.value.data.key, ECCValConstUndefined, ECC_VALFLAG_HIDDEN);
            }
        } while(acceptToken(self, ','));

    return NULL;
}

static eccoplist_t* function(eccastparser_t* self, int isDeclaration, int isGetter, int isSetter)
{
    eccvalue_t value;
    ecctextstring_t text, textParameter;

    eccoplist_t* oplist = NULL;
    int parameterCount = 0;

    eccoperand_t identifierOp = { 0, ECCValConstUndefined };
    eccobjscriptfunction_t* parentFunction;
    eccobjscriptfunction_t* function;
    ecchashmap_t* arguments;
    uint16_t slot;

    if(!isGetter && !isSetter)
    {
        expectToken(self, ECC_TOK_FUNCTION);

        if(previewToken(self) == ECC_TOK_IDENTIFIER)
        {
            identifierOp = identifier(self);

            if(self->strictMode && io_libecc_Key.isEqual(identifierOp.value.data.key, io_libecc_key_eval))
                syntaxError(self, identifierOp.text, io_libecc_Chars.create("redefining eval is not allowed"));
            else if(self->strictMode && io_libecc_Key.isEqual(identifierOp.value.data.key, io_libecc_key_arguments))
                syntaxError(self, identifierOp.text, io_libecc_Chars.create("redefining arguments is not allowed"));
        }
        else if(isDeclaration)
        {
            syntaxError(self, self->lexer->text, io_libecc_Chars.create("function statement requires a name"));
            return NULL;
        }
    }

    parentFunction = self->function;
    parentFunction->flags |= ECC_SCRIPTFUNCFLAG_NEEDHEAP;

    function = io_libecc_Function.create(&self->function->environment);

    arguments = (ecchashmap_t*)ECCNSObject.addMember(&function->environment, io_libecc_key_arguments, ECCValConstUndefined, 0);
    slot = arguments - function->environment.hashmap;

    self->function = function;
    text = self->lexer->text;
    expectToken(self, '(');
    textParameter = self->lexer->text;
    oplist = io_libecc_OpList.join(oplist, parameters(self, &parameterCount));

    function->environment.hashmap[slot].value = ECCValConstUndefined;
    function->environment.hashmap[slot].value.key = io_libecc_key_arguments;
    function->environment.hashmap[slot].value.flags |= ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED;

    if(isGetter && parameterCount != 0)
        syntaxError(self, ECCNSText.make(textParameter.bytes, (int32_t)(self->lexer->text.bytes - textParameter.bytes)),
                    io_libecc_Chars.create("getter functions must have no arguments"));
    else if(isSetter && parameterCount != 1)
        syntaxError(self, ECCNSText.make(self->lexer->text.bytes, 0), io_libecc_Chars.create("setter functions must have one argument"));

    expectToken(self, ')');
    expectToken(self, '{');

    if(parentFunction->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE)
        self->function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    oplist = io_libecc_OpList.join(oplist, sourceElements(self));
    text.length = (int32_t)(self->lexer->text.bytes - text.bytes) + 1;
    expectToken(self, '}');
    self->function = parentFunction;

    function->oplist = oplist;
    function->text = text;
    function->parameterCount = parameterCount;

    ECCNSObject.addMember(&function->object, io_libecc_key_length, ECCNSValue.integer(parameterCount), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

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
        return io_libecc_OpList.append(io_libecc_OpList.create(io_libecc_Op.setLocal, identifierOp.value, ECC_ConstString_Empty),
                                       io_libecc_Op.make(io_libecc_Op.function, value, text));
    else
        return io_libecc_OpList.create(io_libecc_Op.function, value, text);
}

// MARK: Source

static eccoplist_t* sourceElements(eccastparser_t* self)
{
    eccoplist_t* oplist = NULL;

    ++self->sourceDepth;

    self->function->oplist = NULL;

    if(previewToken(self) == ECC_TOK_STRING && self->lexer->text.length == 10 && !memcmp("use strict", self->lexer->text.bytes, 10))
        self->function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    oplist = statementList(self);

    if(self->sourceDepth <= 1)
        oplist = io_libecc_OpList.appendNoop(oplist);
    else
        oplist = io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.resultVoid, ECCValConstUndefined, ECC_ConstString_Empty));

    if(self->function->oplist)
        self->function->oplist = io_libecc_OpList.joinDiscarded(NULL, self->function->oplist->count / 2, self->function->oplist);

    oplist = io_libecc_OpList.join(self->function->oplist, oplist);

    oplist->ops->text.flags |= ECC_TEXTFLAG_BREAKFLAG;
    if(oplist->count > 1)
        oplist->ops[1].text.flags |= ECC_TEXTFLAG_BREAKFLAG;

    ECCNSObject.packValue(&self->function->environment);

    --self->sourceDepth;

    return oplist;
}

// MARK: - Methods

eccastparser_t* createWithLexer(eccastlexer_t* lexer)
{
    eccastparser_t* self = malloc(sizeof(*self));
    *self = io_libecc_Parser.identity;

    self->lexer = lexer;

    return self;
}

void destroy(eccastparser_t* self)
{
    assert(self);

    io_libecc_Lexer.destroy(self->lexer), self->lexer = NULL;
    free(self->depths), self->depths = NULL;
    free(self), self = NULL;
}

eccobjscriptfunction_t* parseWithEnvironment(eccastparser_t* const self, eccobject_t* environment, eccobject_t* global)
{
    eccobjscriptfunction_t* function;
    eccoplist_t* oplist;

    assert(self);

    function = io_libecc_Function.create(environment);
    self->function = function;
    self->global = global;
    self->reserveGlobalSlots = 0;
    if(self->strictMode)
        function->flags |= ECC_SCRIPTFUNCFLAG_STRICTMODE;

    nextToken(self);
    oplist = sourceElements(self);
    io_libecc_OpList.optimizeWithEnvironment(oplist, &function->environment, 0);

    ECCNSObject.reserveSlots(global, self->reserveGlobalSlots);

    if(self->error)
    {
        eccoperand_t errorOps[] = {
            { io_libecc_Op.doThrow, ECCValConstUndefined, self->error->text },
            { io_libecc_Op.value, ECCNSValue.error(self->error) },
        };
        errorOps->text.flags |= ECC_TEXTFLAG_BREAKFLAG;

        io_libecc_OpList.destroy(oplist), oplist = NULL;
        oplist = malloc(sizeof(*oplist));
        oplist->ops = malloc(sizeof(errorOps));
        oplist->count = sizeof(errorOps) / sizeof(*errorOps);
        memcpy(oplist->ops, errorOps, sizeof(errorOps));
    }

    function->oplist = oplist;
    return function;
}
