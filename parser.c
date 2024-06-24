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

static struct io_libecc_OpList * new (struct io_libecc_Parser *);
static struct io_libecc_OpList * assignment (struct io_libecc_Parser *, int noIn);
static struct io_libecc_OpList * expression (struct io_libecc_Parser *, int noIn);
static struct io_libecc_OpList * statement (struct io_libecc_Parser *);
static struct io_libecc_OpList * function (struct io_libecc_Parser *, int isDeclaration, int isGetter, int isSetter);
static struct io_libecc_OpList * sourceElements (struct io_libecc_Parser *);


// MARK: Token

static struct io_libecc_Parser* createWithLexer(struct io_libecc_Lexer*);
static void destroy(struct io_libecc_Parser*);
static struct io_libecc_Function* parseWithEnvironment(struct io_libecc_Parser* const, struct eccobject_t* environment, struct eccobject_t* global);
const struct type_io_libecc_Parser io_libecc_Parser = {
    createWithLexer,
    destroy,
    parseWithEnvironment,
};

static inline
enum io_libecc_lexer_Token previewToken (struct io_libecc_Parser *self)
{
	return self->previewToken;
}

static inline
enum io_libecc_lexer_Token nextToken (struct io_libecc_Parser *self)
{
	if (self->previewToken != io_libecc_lexer_errorToken)
	{
		self->previewToken = io_libecc_Lexer.nextToken(self->lexer);
		
		if (self->previewToken == io_libecc_lexer_errorToken)
			self->error = self->lexer->value.data.error;
	}
	return self->previewToken;
}

static
void parseError (struct io_libecc_Parser *self, struct io_libecc_Error *error)
{
	if (!self->error)
	{
		self->error = error;
		self->previewToken = io_libecc_lexer_errorToken;
	}
}

static
void syntaxError (struct io_libecc_Parser *self, struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	parseError(self, io_libecc_Error.syntaxError(text, message));
}

static
void referenceError (struct io_libecc_Parser *self, struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	parseError(self, io_libecc_Error.referenceError(text, message));
}

static
struct io_libecc_OpList * tokenError (struct io_libecc_Parser *self, const char *t)
{
	char b[4];
	
	if (!previewToken(self) || previewToken(self) >= io_libecc_lexer_errorToken)
		syntaxError(self, self->lexer->text, io_libecc_Chars.create("expected %s, got %s", t, io_libecc_Lexer.tokenChars(previewToken(self), b)));
	else
		syntaxError(self, self->lexer->text, io_libecc_Chars.create("expected %s, got '%.*s'", t, self->lexer->text.length, self->lexer->text.bytes));
	
	return NULL;
}

static inline
int acceptToken (struct io_libecc_Parser *self, enum io_libecc_lexer_Token token)
{
	if (previewToken(self) != token)
		return 0;
	
	nextToken(self);
	return 1;
}

static inline
int expectToken (struct io_libecc_Parser *self, enum io_libecc_lexer_Token token)
{
	if (previewToken(self) != token)
	{
		
		char b[4];
		const char *type = io_libecc_Lexer.tokenChars(token, b);
		tokenError(self, type);
		return 0;
	}
	
	nextToken(self);
	return 1;
}


// MARK: Depth

static
void pushDepth (struct io_libecc_Parser *self, struct io_libecc_Key key, char depth)
{
	self->depths = realloc(self->depths, (self->depthCount + 1) * sizeof(*self->depths));
	self->depths[self->depthCount].key = key;
	self->depths[self->depthCount].depth = depth;
	++self->depthCount;
}

static
void popDepth (struct io_libecc_Parser *self)
{
	--self->depthCount;
}


// MARK: Expression

static
struct io_libecc_OpList * foldConstant (struct io_libecc_Parser *self, struct io_libecc_OpList * oplist)
{
	struct io_libecc_Ecc ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
	struct eccstate_t context = { oplist->ops, .ecc = &ecc };
	struct eccvalue_t value = context.ops->native(&context);
	struct io_libecc_Text text = io_libecc_OpList.text(oplist);
	io_libecc_OpList.destroy(oplist);
	return io_libecc_OpList.create(io_libecc_Op.value, value, text);
}

static
struct io_libecc_OpList * useBinary (struct io_libecc_Parser *self, struct io_libecc_OpList * oplist, int add)
{
	if (oplist && oplist->ops[0].native == io_libecc_Op.value && (io_libecc_Value.isNumber(oplist->ops[0].value) || !add))
	{
		struct io_libecc_Ecc ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
		struct eccstate_t context = { oplist->ops, .ecc = &ecc };
		oplist->ops[0].value = io_libecc_Value.toBinary(&context, oplist->ops[0].value);
	}
	return oplist;
}

static
struct io_libecc_OpList * useInteger (struct io_libecc_Parser *self, struct io_libecc_OpList * oplist)
{
	if (oplist && oplist->ops[0].native == io_libecc_Op.value)
	{
		struct io_libecc_Ecc ecc = { .sloppyMode = self->lexer->allowUnicodeOutsideLiteral };
		struct eccstate_t context = { oplist->ops, .ecc = &ecc };
		oplist->ops[0].value = io_libecc_Value.toInteger(&context, oplist->ops[0].value);
	}
	return oplist;
}

static
struct io_libecc_OpList * expressionRef (struct io_libecc_Parser *self, struct io_libecc_OpList *oplist, const char *name)
{
	if (!oplist)
		return NULL;
	
	if (oplist->ops[0].native == io_libecc_Op.getLocal && oplist->count == 1)
	{
		if (oplist->ops[0].value.type == io_libecc_value_keyType)
		{
			if (io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_eval))
				syntaxError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create(name));
			else if (io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_arguments))
				syntaxError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create(name));
		}
		
		oplist->ops[0].native = io_libecc_Op.getLocalRef;
	}
	else if (oplist->ops[0].native == io_libecc_Op.getMember)
		oplist->ops[0].native = io_libecc_Op.getMemberRef;
	else if (oplist->ops[0].native == io_libecc_Op.getProperty)
		oplist->ops[0].native = io_libecc_Op.getPropertyRef;
	else
		referenceError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("%s", name));
	
	return oplist;
}

static
void semicolon (struct io_libecc_Parser *self)
{
	if (previewToken(self) == ';')
	{
		nextToken(self);
		return;
	}
	else if (self->lexer->didLineBreak || previewToken(self) == '}' || previewToken(self) == io_libecc_lexer_noToken)
		return;
	
	syntaxError(self, self->lexer->text, io_libecc_Chars.create("missing ; before statement"));
}

static
struct io_libecc_Op identifier (struct io_libecc_Parser *self)
{
	struct eccvalue_t value = self->lexer->value;
	struct io_libecc_Text text = self->lexer->text;
	if (!expectToken(self, io_libecc_lexer_identifierToken))
		return (struct io_libecc_Op){ 0 };
	
	return io_libecc_Op.make(io_libecc_Op.value, value, text);
}

static
struct io_libecc_OpList * array (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	uint32_t count = 0;
	struct io_libecc_Text text = self->lexer->text;
	
	nextToken(self);
	
	do
	{
		while (previewToken(self) == ',')
		{
			++count;
			oplist = io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.value, io_libecc_value_none, self->lexer->text));
			nextToken(self);
		}
		
		if (previewToken(self) == ']')
			break;
		
		++count;
		oplist = io_libecc_OpList.join(oplist, assignment(self, 0));
	}
	while (acceptToken(self, ','));
	
	text = io_libecc_Text.join(text, self->lexer->text);
	expectToken(self, ']');
	
	return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.array, io_libecc_Value.integer(count), text), oplist);
}

static
struct io_libecc_OpList * propertyAssignment (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	int isGetter = 0, isSetter = 0;
	
	if (previewToken(self) == io_libecc_lexer_identifierToken)
	{
		if (io_libecc_Key.isEqual(self->lexer->value.data.key, io_libecc_key_get))
		{
			nextToken(self);
			if (previewToken(self) == ':')
			{
				oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.key(io_libecc_key_get), self->lexer->text);
				goto skipProperty;
			}
			else
				isGetter = 1;
		}
		else if (io_libecc_Key.isEqual(self->lexer->value.data.key, io_libecc_key_set))
		{
			nextToken(self);
			if (previewToken(self) == ':')
			{
				oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.key(io_libecc_key_set), self->lexer->text);
				goto skipProperty;
			}
			else
				isSetter = 1;
		}
	}
	
	if (previewToken(self) == io_libecc_lexer_integerToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->value, self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_binaryToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.key(io_libecc_Key.makeWithText(self->lexer->text, 0)), self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_stringToken)
	{
		uint32_t element = io_libecc_Lexer.scanElement(self->lexer->text);
		if (element < UINT32_MAX)
			oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.integer(element), self->lexer->text);
		else
			oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.key(io_libecc_Key.makeWithText(self->lexer->text, 0)), self->lexer->text);
	}
	else if (previewToken(self) == io_libecc_lexer_escapedStringToken)
	{
		struct io_libecc_Text text = io_libecc_Text.make(self->lexer->value.data.chars->bytes, self->lexer->value.data.chars->length);
		uint32_t element = io_libecc_Lexer.scanElement(text);
		if (element < UINT32_MAX)
			oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.integer(element), self->lexer->text);
		else
			oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.key(io_libecc_Key.makeWithText(*self->lexer->value.data.text, 0)), self->lexer->text);
	}
	else if (previewToken(self) == io_libecc_lexer_identifierToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->value, self->lexer->text);
	else
	{
		expectToken(self, io_libecc_lexer_identifierToken);
		return NULL;
	}
	
	nextToken(self);
	
	if (isGetter)
		return io_libecc_OpList.join(oplist, function(self, 0, 1, 0));
	else if (isSetter)
		return io_libecc_OpList.join(oplist, function(self, 0, 0, 1));
	
	skipProperty:
	expectToken(self, ':');
	return io_libecc_OpList.join(oplist, assignment(self, 0));
}

static
struct io_libecc_OpList * object (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	uint32_t count = 0;
	struct io_libecc_Text text = self->lexer->text;
	
	do
	{
		self->lexer->disallowKeyword = 1;
		nextToken(self);
		self->lexer->disallowKeyword = 0;
		
		if (previewToken(self) == '}')
			break;
		
		++count;
		oplist = io_libecc_OpList.join(oplist, propertyAssignment(self));
	}
	while (previewToken(self) == ',');
	
	text = io_libecc_Text.join(text, self->lexer->text);
	expectToken(self, '}');
	
	return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.object, io_libecc_Value.integer(count), text), oplist);
}

static
struct io_libecc_OpList * primary (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	
	if (previewToken(self) == io_libecc_lexer_identifierToken)
	{
		oplist = io_libecc_OpList.create(io_libecc_Op.getLocal, self->lexer->value, self->lexer->text);
		
		if (io_libecc_Key.isEqual(self->lexer->value.data.key, io_libecc_key_arguments))
			self->function->flags |= io_libecc_function_needArguments | io_libecc_function_needHeap;
	}
	else if (previewToken(self) == io_libecc_lexer_stringToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.text, io_libecc_value_undefined, self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_escapedStringToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->value, self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_binaryToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->value, self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_integerToken)
	{
		if (self->preferInteger)
			oplist = io_libecc_OpList.create(io_libecc_Op.value, self->lexer->value, self->lexer->text);
		else
			oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.binary(self->lexer->value.data.integer), self->lexer->text);
	}
	else if (previewToken(self) == io_libecc_lexer_thisToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.this, io_libecc_value_undefined, self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_nullToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_value_null, self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_trueToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.truth(1), self->lexer->text);
	else if (previewToken(self) == io_libecc_lexer_falseToken)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.truth(0), self->lexer->text);
	else if (previewToken(self) == '{')
		return object(self);
	else if (previewToken(self) == '[')
		return array(self);
	else if (acceptToken(self, '('))
	{
		oplist = expression(self, 0);
		expectToken(self, ')');
		return oplist;
	}
	else
	{
		if (self->lexer->text.bytes[0] == '/')
		{
			self->lexer->allowRegex = 1;
			self->lexer->offset -= self->lexer->text.length;
			nextToken(self);
			self->lexer->allowRegex = 0;
			
			if (previewToken(self) != io_libecc_lexer_regexpToken)
				tokenError(self, "io_libecc_RegExp");
		}
		
		if (previewToken(self) == io_libecc_lexer_regexpToken)
			oplist = io_libecc_OpList.create(io_libecc_Op.regexp, io_libecc_value_undefined, self->lexer->text);
		else
			return NULL;
	}
	
	nextToken(self);
	
	return oplist;
}

static
struct io_libecc_OpList * arguments (struct io_libecc_Parser *self, int *count)
{
	struct io_libecc_OpList *oplist = NULL, *argumentOps;
	*count = 0;
	if (previewToken(self) != ')')
		do
		{
			argumentOps = assignment(self, 0);
			if (!argumentOps)
				tokenError(self, "expression");
			
			++*count;
			oplist = io_libecc_OpList.join(oplist, argumentOps);
		} while (acceptToken(self, ','));
	
	return oplist;
}

static
struct io_libecc_OpList * member (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = new(self);
	struct io_libecc_Text text;
	while (1)
	{
		if (previewToken(self) == '.')
		{
			struct eccvalue_t value;
			
			self->lexer->disallowKeyword = 1;
			nextToken(self);
			self->lexer->disallowKeyword = 0;
			
			value = self->lexer->value;
			text = io_libecc_Text.join(io_libecc_OpList.text(oplist), self->lexer->text);
			if (!expectToken(self, io_libecc_lexer_identifierToken))
				return oplist;
			
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getMember, value, text), oplist);
		}
		else if (acceptToken(self, '['))
		{
			oplist = io_libecc_OpList.join(oplist, expression(self, 0));
			text = io_libecc_Text.join(io_libecc_OpList.text(oplist), self->lexer->text);
			if (!expectToken(self, ']'))
				return oplist;
			
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getProperty, io_libecc_value_undefined, text), oplist);
		}
		else
			break;
	}
	return oplist;
}

static
struct io_libecc_OpList * new (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	struct io_libecc_Text text = self->lexer->text;
	
	if (acceptToken(self, io_libecc_lexer_newToken))
	{
		int count = 0;
		oplist = member(self);
		text = io_libecc_Text.join(text, io_libecc_OpList.text(oplist));
		if (acceptToken(self, '('))
		{
			oplist = io_libecc_OpList.join(oplist, arguments(self, &count));
			text = io_libecc_Text.join(text, self->lexer->text);
			expectToken(self, ')');
		}
		return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.construct, io_libecc_Value.integer(count), text), oplist);
	}
	else if (previewToken(self) == io_libecc_lexer_functionToken)
		return function(self, 0, 0, 0);
	else
		return primary(self);
}

static
struct io_libecc_OpList * leftHandSide (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = new(self);
	struct io_libecc_Text text = io_libecc_OpList.text(oplist);
	struct eccvalue_t value;
	
	while (1)
	{
		if (previewToken(self) == '.')
		{
			if (!oplist)
			{
				tokenError(self, "expression");
				return oplist;
			}
			
			self->lexer->disallowKeyword = 1;
			nextToken(self);
			self->lexer->disallowKeyword = 0;
			
			value = self->lexer->value;
			text = io_libecc_Text.join(io_libecc_OpList.text(oplist), self->lexer->text);
			if (!expectToken(self, io_libecc_lexer_identifierToken))
				return oplist;
			
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getMember, value, text), oplist);
		}
		else if (acceptToken(self, '['))
		{
			oplist = io_libecc_OpList.join(oplist, expression(self, 0));
			text = io_libecc_Text.join(io_libecc_OpList.text(oplist), self->lexer->text);
			if (!expectToken(self, ']'))
				return oplist;
			
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getProperty, io_libecc_value_undefined, text), oplist);
		}
		else if (acceptToken(self, '('))
		{
			int count = 0;
			
			int isEval = oplist->count == 1 && oplist->ops[0].native == io_libecc_Op.getLocal && io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_eval);
			if (isEval)
			{
				text = io_libecc_Text.join(io_libecc_OpList.text(oplist), self->lexer->text);
				io_libecc_OpList.destroy(oplist), oplist = NULL;
			}
			
			oplist = io_libecc_OpList.join(oplist, arguments(self, &count));
			text = io_libecc_Text.join(io_libecc_Text.join(text, io_libecc_OpList.text(oplist)), self->lexer->text);
			
			if (isEval)
				oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.eval, io_libecc_Value.integer(count), text), oplist);
			else if (oplist->ops->native == io_libecc_Op.getMember)
				oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.callMember, io_libecc_Value.integer(count), text), oplist);
			else if (oplist->ops->native == io_libecc_Op.getProperty)
				oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.callProperty, io_libecc_Value.integer(count), text), oplist);
			else
				oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.call, io_libecc_Value.integer(count), text), oplist);
			
			if (!expectToken(self, ')'))
				break;
		}
		else
			break;
	}
	return oplist;
}

static
struct io_libecc_OpList * postfix (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = leftHandSide(self);
	struct io_libecc_Text text = self->lexer->text;
	
	if (!self->lexer->didLineBreak && acceptToken(self, io_libecc_lexer_incrementToken))
		oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.postIncrementRef, io_libecc_value_undefined, io_libecc_Text.join(oplist->ops->text, text)), expressionRef(self, oplist, "invalid increment operand"));
	if (!self->lexer->didLineBreak && acceptToken(self, io_libecc_lexer_decrementToken))
		oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.postDecrementRef, io_libecc_value_undefined, io_libecc_Text.join(oplist->ops->text, text)), expressionRef(self, oplist, "invalid decrement operand"));
	
	return oplist;
}

static
struct io_libecc_OpList * unary (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist, *alt;
	struct io_libecc_Text text = self->lexer->text;
	io_libecc_native_io_libecc_Function native;
	
	if (acceptToken(self, io_libecc_lexer_deleteToken))
	{
		oplist = unary(self);
		
		if (oplist && oplist->ops[0].native == io_libecc_Op.getLocal)
		{
			if (self->strictMode)
				syntaxError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("delete of an unqualified identifier"));
			
			oplist->ops->native = io_libecc_Op.deleteLocal;
		}
		else if (oplist && oplist->ops[0].native == io_libecc_Op.getMember)
			oplist->ops->native = io_libecc_Op.deleteMember;
		else if (oplist && oplist->ops[0].native == io_libecc_Op.getProperty)
			oplist->ops->native = io_libecc_Op.deleteProperty;
		else if (!self->strictMode && oplist)
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.exchange, io_libecc_value_true, io_libecc_text_empty), oplist);
		else if (oplist)
			referenceError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("invalid delete operand"));
		else
			tokenError(self, "expression");
		
		return oplist;
	}
	else if (acceptToken(self, io_libecc_lexer_voidToken))
		native = io_libecc_Op.exchange, alt = unary(self);
	else if (acceptToken(self, io_libecc_lexer_typeofToken))
	{
		native = io_libecc_Op.typeOf, alt = unary(self);
		if (alt->ops->native == io_libecc_Op.getLocal)
			alt->ops->native = io_libecc_Op.getLocalRefOrNull;
	}
	else if (acceptToken(self, io_libecc_lexer_incrementToken))
		native = io_libecc_Op.incrementRef, alt = expressionRef(self, unary(self), "invalid increment operand");
	else if (acceptToken(self, io_libecc_lexer_decrementToken))
		native = io_libecc_Op.decrementRef, alt = expressionRef(self, unary(self), "invalid decrement operand");
	else if (acceptToken(self, '+'))
		native = io_libecc_Op.positive, alt = useBinary(self, unary(self), 0);
	else if (acceptToken(self, '-'))
		native = io_libecc_Op.negative, alt = useBinary(self, unary(self), 0);
	else if (acceptToken(self, '~'))
		native = io_libecc_Op.invert, alt = useInteger(self, unary(self));
	else if (acceptToken(self, '!'))
		native = io_libecc_Op.not, alt = unary(self);
	else
		return postfix(self);
	
	if (!alt)
		return tokenError(self, "expression");
	
	oplist = io_libecc_OpList.unshift(io_libecc_Op.make(native, io_libecc_value_undefined, io_libecc_Text.join(text, alt->ops->text)), alt);
	
	if (oplist->ops[1].native == io_libecc_Op.value)
		return foldConstant(self, oplist);
	else
		return oplist;
}

static
struct io_libecc_OpList * multiplicative (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = unary(self), *alt;
	
	while (1)
	{
		io_libecc_native_io_libecc_Function native;
		
		if (previewToken(self) == '*')
			native = io_libecc_Op.multiply;
		else if (previewToken(self) == '/')
			native = io_libecc_Op.divide;
		else if (previewToken(self) == '%')
			native = io_libecc_Op.modulo;
		else
			return oplist;
		
		if (useBinary(self, oplist, 0))
		{
			nextToken(self);
			if ((alt = useBinary(self, unary(self), 0)))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, io_libecc_value_undefined, text), oplist, alt);
				
				if (oplist->ops[1].native == io_libecc_Op.value && oplist->ops[2].native == io_libecc_Op.value)
					oplist = foldConstant(self, oplist);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
}

static
struct io_libecc_OpList * additive (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = multiplicative(self), *alt;
	while (1)
	{
		io_libecc_native_io_libecc_Function native;
		
		if (previewToken(self) == '+')
			native = io_libecc_Op.add;
		else if (previewToken(self) == '-')
			native = io_libecc_Op.minus;
		else
			return oplist;
		
		if (useBinary(self, oplist, native == io_libecc_Op.add))
		{
			nextToken(self);
			if ((alt = useBinary(self, multiplicative(self), native == io_libecc_Op.add)))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, io_libecc_value_undefined, text), oplist, alt);
				
				if (oplist->ops[1].native == io_libecc_Op.value && oplist->ops[2].native == io_libecc_Op.value)
					oplist = foldConstant(self, oplist);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
}

static
struct io_libecc_OpList * shift (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = additive(self), *alt;
	while (1)
	{
		io_libecc_native_io_libecc_Function native;
		
		if (previewToken(self) == io_libecc_lexer_leftShiftToken)
			native = io_libecc_Op.leftShift;
		else if (previewToken(self) == io_libecc_lexer_rightShiftToken)
			native = io_libecc_Op.rightShift;
		else if (previewToken(self) == io_libecc_lexer_unsignedRightShiftToken)
			native = io_libecc_Op.unsignedRightShift;
		else
			return oplist;
		
		if (useInteger(self, oplist))
		{
			nextToken(self);
			if ((alt = useInteger(self, additive(self))))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, io_libecc_value_undefined, text), oplist, alt);
				
				if (oplist->ops[1].native == io_libecc_Op.value && oplist->ops[2].native == io_libecc_Op.value)
					oplist = foldConstant(self, oplist);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
}

static
struct io_libecc_OpList * relational (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = shift(self), *alt;
	while (1)
	{
		io_libecc_native_io_libecc_Function native;
		
		if (previewToken(self) == '<')
			native = io_libecc_Op.less;
		else if (previewToken(self) == '>')
			native = io_libecc_Op.more;
		else if (previewToken(self) == io_libecc_lexer_lessOrEqualToken)
			native = io_libecc_Op.lessOrEqual;
		else if (previewToken(self) == io_libecc_lexer_moreOrEqualToken)
			native = io_libecc_Op.moreOrEqual;
		else if (previewToken(self) == io_libecc_lexer_instanceofToken)
			native = io_libecc_Op.instanceOf;
		else if (!noIn && previewToken(self) == io_libecc_lexer_inToken)
			native = io_libecc_Op.in;
		else
			return oplist;
		
		if (oplist)
		{
			nextToken(self);
			if ((alt = shift(self)))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, io_libecc_value_undefined, text), oplist, alt);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
}

static
struct io_libecc_OpList * equality (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = relational(self, noIn), *alt;
	while (1)
	{
		io_libecc_native_io_libecc_Function native;
		
		if (previewToken(self) == io_libecc_lexer_equalToken)
			native = io_libecc_Op.equal;
		else if (previewToken(self) == io_libecc_lexer_notEqualToken)
			native = io_libecc_Op.notEqual;
		else if (previewToken(self) == io_libecc_lexer_identicalToken)
			native = io_libecc_Op.identical;
		else if (previewToken(self) == io_libecc_lexer_notIdenticalToken)
			native = io_libecc_Op.notIdentical;
		else
			return oplist;
		
		if (oplist)
		{
			nextToken(self);
			if ((alt = relational(self, noIn)))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, io_libecc_value_undefined, text), oplist, alt);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
}

static
struct io_libecc_OpList * bitwiseAnd (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = equality(self, noIn), *alt;
	while (previewToken(self) == '&')
	{
		if (useInteger(self, oplist))
		{
			nextToken(self);
			if ((alt = useInteger(self, equality(self, noIn))))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.bitwiseAnd, io_libecc_value_undefined, text), oplist, alt);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
	return oplist;
}

static
struct io_libecc_OpList * bitwiseXor (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = bitwiseAnd(self, noIn), *alt;
	while (previewToken(self) == '^')
	{
		if (useInteger(self, oplist))
		{
			nextToken(self);
			if ((alt = useInteger(self, bitwiseAnd(self, noIn))))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.bitwiseXor, io_libecc_value_undefined, text), oplist, alt);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
	return oplist;
}

static
struct io_libecc_OpList * bitwiseOr (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = bitwiseXor(self, noIn), *alt;
	while (previewToken(self) == '|')
	{
		if (useInteger(self, oplist))
		{
			nextToken(self);
			if ((alt = useInteger(self, bitwiseXor(self, noIn))))
			{
				struct io_libecc_Text text = io_libecc_Text.join(oplist->ops->text, alt->ops->text);
				oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.bitwiseOr, io_libecc_value_undefined, text), oplist, alt);
				
				continue;
			}
			io_libecc_OpList.destroy(oplist);
		}
		return tokenError(self, "expression");
	}
	return oplist;
}

static
struct io_libecc_OpList * logicalAnd (struct io_libecc_Parser *self, int noIn)
{
	int32_t opCount;
	struct io_libecc_OpList *oplist = bitwiseOr(self, noIn), *nextOp = NULL;
	
	while (acceptToken(self, io_libecc_lexer_logicalAndToken))
		if (oplist && (nextOp = bitwiseOr(self, noIn)))
		{
			opCount = nextOp->count;
			oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.logicalAnd, io_libecc_Value.integer(opCount), io_libecc_OpList.text(oplist)), oplist, nextOp);
		}
		else
			tokenError(self, "expression");
	
	return oplist;
}

static
struct io_libecc_OpList * logicalOr (struct io_libecc_Parser *self, int noIn)
{
	int32_t opCount;
	struct io_libecc_OpList *oplist = logicalAnd(self, noIn), *nextOp = NULL;
	
	while (acceptToken(self, io_libecc_lexer_logicalOrToken))
		if (oplist && (nextOp = logicalAnd(self, noIn)))
		{
			opCount = nextOp->count;
			oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.logicalOr, io_libecc_Value.integer(opCount), io_libecc_OpList.text(oplist)), oplist, nextOp);
		}
		else
			tokenError(self, "expression");
	
	return oplist;
}

static
struct io_libecc_OpList * conditional (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = logicalOr(self, noIn);
	
	if (acceptToken(self, '?'))
	{
		if (oplist)
		{
			struct io_libecc_OpList *trueOps = assignment(self, 0);
			struct io_libecc_OpList *falseOps;
			
			if (!expectToken(self, ':'))
				return oplist;
			
			falseOps = assignment(self, noIn);
			
			trueOps = io_libecc_OpList.append(trueOps, io_libecc_Op.make(io_libecc_Op.jump, io_libecc_Value.integer(falseOps->count), io_libecc_OpList.text(trueOps)));
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.jumpIfNot, io_libecc_Value.integer(trueOps->count), io_libecc_OpList.text(oplist)), oplist);
			oplist = io_libecc_OpList.join3(oplist, trueOps, falseOps);
			
			return oplist;
		}
		else
			tokenError(self, "expression");
	}
	return oplist;
}

static
struct io_libecc_OpList * assignment (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = conditional(self, noIn), *opassign = NULL;
	struct io_libecc_Text text = self->lexer->text;
	io_libecc_native_io_libecc_Function native = NULL;
	
	if (acceptToken(self, '='))
	{
		if (!oplist)
		{
			syntaxError(self, text, io_libecc_Chars.create("expected expression, got '='"));
			return NULL;
		}
		else if (oplist->ops[0].native == io_libecc_Op.getLocal && oplist->count == 1)
		{
			if (io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_eval))
				syntaxError(self, text, io_libecc_Chars.create("can't assign to eval"));
			else if (io_libecc_Key.isEqual(oplist->ops[0].value.data.key, io_libecc_key_arguments))
				syntaxError(self, text, io_libecc_Chars.create("can't assign to arguments"));
			
			if (!self->strictMode && !io_libecc_Object.member(&self->function->environment, oplist->ops[0].value.data.key, 0))
				++self->reserveGlobalSlots;
//				io_libecc_Object.addMember(self->global, oplist->ops[0].value.data.key, io_libecc_value_none, 0);
			
			oplist->ops->native = io_libecc_Op.setLocal;
		}
		else if (oplist->ops[0].native == io_libecc_Op.getMember)
			oplist->ops->native = io_libecc_Op.setMember;
		else if (oplist->ops[0].native == io_libecc_Op.getProperty)
			oplist->ops->native = io_libecc_Op.setProperty;
		else
			referenceError(self, io_libecc_OpList.text(oplist), io_libecc_Chars.create("invalid assignment left-hand side"));
		
		if (( opassign = assignment(self, noIn) ))
		{
			oplist->ops->text = io_libecc_Text.join(oplist->ops->text, opassign->ops->text);
			return io_libecc_OpList.join(oplist, opassign);
		}
		
		tokenError(self, "expression");
	}
	else if (acceptToken(self, io_libecc_lexer_multiplyAssignToken))
		native = io_libecc_Op.multiplyAssignRef;
	else if (acceptToken(self, io_libecc_lexer_divideAssignToken))
		native = io_libecc_Op.divideAssignRef;
	else if (acceptToken(self, io_libecc_lexer_moduloAssignToken))
		native = io_libecc_Op.moduloAssignRef;
	else if (acceptToken(self, io_libecc_lexer_addAssignToken))
		native = io_libecc_Op.addAssignRef;
	else if (acceptToken(self, io_libecc_lexer_minusAssignToken))
		native = io_libecc_Op.minusAssignRef;
	else if (acceptToken(self, io_libecc_lexer_leftShiftAssignToken))
		native = io_libecc_Op.leftShiftAssignRef;
	else if (acceptToken(self, io_libecc_lexer_rightShiftAssignToken))
		native = io_libecc_Op.rightShiftAssignRef;
	else if (acceptToken(self, io_libecc_lexer_unsignedRightShiftAssignToken))
		native = io_libecc_Op.unsignedRightShiftAssignRef;
	else if (acceptToken(self, io_libecc_lexer_andAssignToken))
		native = io_libecc_Op.bitAndAssignRef;
	else if (acceptToken(self, io_libecc_lexer_xorAssignToken))
		native = io_libecc_Op.bitXorAssignRef;
	else if (acceptToken(self, io_libecc_lexer_orAssignToken))
		native = io_libecc_Op.bitOrAssignRef;
	else
		return oplist;
	
	if (oplist)
	{
		if (( opassign = assignment(self, noIn) ))
			oplist->ops->text = io_libecc_Text.join(oplist->ops->text, opassign->ops->text);
		else
			tokenError(self, "expression");
		
		return io_libecc_OpList.unshiftJoin(io_libecc_Op.make(native, io_libecc_value_undefined, oplist->ops->text), expressionRef(self, oplist, "invalid assignment left-hand side"), opassign);
	}
	
	syntaxError(self, text, io_libecc_Chars.create("expected expression, got '%.*s'", text.length, text.bytes));
	return NULL;
}

static
struct io_libecc_OpList * expression (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = assignment(self, noIn);
	while (acceptToken(self, ','))
		oplist = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.discard, io_libecc_value_undefined, io_libecc_text_empty), oplist, assignment(self, noIn));
	
	return oplist;
}


// MARK: Statements

static
struct io_libecc_OpList * statementList (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL, *statementOps = NULL, *discardOps = NULL;
	uint16_t discardCount = 0;
	
	while (previewToken(self) != io_libecc_lexer_errorToken && previewToken(self) != io_libecc_lexer_noToken)
	{
		if (previewToken(self) == io_libecc_lexer_functionToken)
			self->function->oplist = io_libecc_OpList.join(self->function->oplist, function(self, 1, 0, 0));
		else
		{
			if (( statementOps = statement(self) ))
			{
				while (statementOps->count > 1 && statementOps->ops[0].native == io_libecc_Op.next)
					io_libecc_OpList.shift(statementOps);
				
				if (statementOps->count == 1 && statementOps->ops[0].native == io_libecc_Op.next)
					io_libecc_OpList.destroy(statementOps), statementOps = NULL;
				else
				{
					if (statementOps->ops[0].native == io_libecc_Op.discard)
					{
						++discardCount;
						discardOps = io_libecc_OpList.join(discardOps, io_libecc_OpList.shift(statementOps));
						statementOps = NULL;
					}
					else if (discardOps)
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
	
	if (discardOps)
		oplist = io_libecc_OpList.joinDiscarded(oplist, discardCount, discardOps);
	
	return oplist;
}

static
struct io_libecc_OpList * block (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	expectToken(self, '{');
	if (previewToken(self) == '}')
		oplist = io_libecc_OpList.create(io_libecc_Op.next, io_libecc_value_undefined, self->lexer->text);
	else
		oplist = statementList(self);
	
	expectToken(self, '}');
	return oplist;
}

static
struct io_libecc_OpList * variableDeclaration (struct io_libecc_Parser *self, int noIn)
{
	struct eccvalue_t value = self->lexer->value;
	struct io_libecc_Text text = self->lexer->text;
	if (!expectToken(self, io_libecc_lexer_identifierToken))
		return NULL;
	
	if (self->strictMode && io_libecc_Key.isEqual(value.data.key, io_libecc_key_eval))
		syntaxError(self, text, io_libecc_Chars.create("redefining eval is not allowed"));
	else if (self->strictMode && io_libecc_Key.isEqual(value.data.key, io_libecc_key_arguments))
		syntaxError(self, text, io_libecc_Chars.create("redefining arguments is not allowed"));
	
	if (self->function->flags & io_libecc_function_strictMode || self->sourceDepth > 1)
		io_libecc_Object.addMember(&self->function->environment, value.data.key, io_libecc_value_undefined, io_libecc_value_sealed);
	else
		io_libecc_Object.addMember(self->global, value.data.key, io_libecc_value_undefined, io_libecc_value_sealed);
	
	if (acceptToken(self, '='))
	{
		struct io_libecc_OpList *opassign = assignment(self, noIn);
		
		if (opassign)
			return io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.discard, io_libecc_value_undefined, io_libecc_text_empty), io_libecc_OpList.create(io_libecc_Op.setLocal, value, io_libecc_Text.join(text, opassign->ops->text)), opassign);
		
		tokenError(self, "expression");
		return NULL;
	}
//	else if (!(self->function->flags & io_libecc_function_strictMode) && self->sourceDepth <= 1)
//		return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.discard, io_libecc_value_undefined, io_libecc_text_empty), io_libecc_OpList.create(io_libecc_Op.createLocalRef, value, text));
	else
		return io_libecc_OpList.create(io_libecc_Op.next, value, text);
}

static
struct io_libecc_OpList * variableDeclarationList (struct io_libecc_Parser *self, int noIn)
{
	struct io_libecc_OpList *oplist = NULL, *varOps;
	do
	{
		varOps = variableDeclaration(self, noIn);
		
		if (oplist && varOps && varOps->count == 1 && varOps->ops[0].native == io_libecc_Op.next)
			io_libecc_OpList.destroy(varOps), varOps = NULL;
		else
			oplist = io_libecc_OpList.join(oplist, varOps);
	}
	while (acceptToken(self, ','));
	
	return oplist;
}

static
struct io_libecc_OpList * ifStatement (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL, *trueOps = NULL, *falseOps = NULL;
	expectToken(self, '(');
	oplist = expression(self, 0);
	expectToken(self, ')');
	trueOps = statement(self);
	if (!trueOps)
		trueOps = io_libecc_OpList.appendNoop(NULL);
	
	if (acceptToken(self, io_libecc_lexer_elseToken))
	{
		falseOps = statement(self);
		if (falseOps)
			trueOps = io_libecc_OpList.append(trueOps, io_libecc_Op.make(io_libecc_Op.jump, io_libecc_Value.integer(falseOps->count), io_libecc_OpList.text(trueOps)));
	}
	oplist = io_libecc_OpList.unshiftJoin3(io_libecc_Op.make(io_libecc_Op.jumpIfNot, io_libecc_Value.integer(trueOps->count), io_libecc_OpList.text(oplist)), oplist, trueOps, falseOps);
	return oplist;
}

static
struct io_libecc_OpList * doStatement (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist, *condition;
	
	pushDepth(self, io_libecc_key_none, 2);
	oplist = statement(self);
	popDepth(self);
	
	expectToken(self, io_libecc_lexer_whileToken);
	expectToken(self, '(');
	condition = expression(self, 0);
	expectToken(self, ')');
	acceptToken(self, ';');
	
	return io_libecc_OpList.createLoop(NULL, condition, NULL, oplist, 1);
}

static
struct io_libecc_OpList * whileStatement (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist, *condition;
	
	expectToken(self, '(');
	condition = expression(self, 0);
	expectToken(self, ')');
	
	pushDepth(self, io_libecc_key_none, 2);
	oplist = statement(self);
	popDepth(self);
	
	return io_libecc_OpList.createLoop(NULL, condition, NULL, oplist, 0);
}

static
struct io_libecc_OpList * forStatement (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL, *condition = NULL, *increment = NULL, *body = NULL;
	
	expectToken(self, '(');
	
	self->preferInteger = 1;
	
	if (acceptToken(self, io_libecc_lexer_varToken))
		oplist = variableDeclarationList(self, 1);
	else if (previewToken(self) != ';')
	{
		oplist = expression(self, 1);
		
		if (oplist)
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.discard, io_libecc_value_undefined, io_libecc_OpList.text(oplist)), oplist);
	}
	
	if (oplist && acceptToken(self, io_libecc_lexer_inToken))
	{
		if (oplist->count == 2 && oplist->ops[0].native == io_libecc_Op.discard && oplist->ops[1].native == io_libecc_Op.getLocal)
		{
			if (!self->strictMode && !io_libecc_Object.member(&self->function->environment, oplist->ops[1].value.data.key, 0))
				++self->reserveGlobalSlots;
			
			oplist->ops[0].native = io_libecc_Op.iterateInRef;
			oplist->ops[1].native = io_libecc_Op.createLocalRef;
		}
		else if (oplist->count == 1 && oplist->ops[0].native == io_libecc_Op.next)
		{
			oplist->ops->native = io_libecc_Op.createLocalRef;
			oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.iterateInRef, io_libecc_value_undefined, self->lexer->text), oplist);
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
		return io_libecc_OpList.join(io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.value, io_libecc_Value.integer(body->count), self->lexer->text)), body);
	}
	else
	{
		expectToken(self, ';');
		if (previewToken(self) != ';')
			condition = expression(self, 0);
		
		expectToken(self, ';');
		if (previewToken(self) != ')')
			increment = expression(self, 0);
		
		expectToken(self, ')');
		
		self->preferInteger = 0;
		
		pushDepth(self, io_libecc_key_none, 2);
		body = statement(self);
		popDepth(self);
		
		return io_libecc_OpList.createLoop(oplist, condition, increment, body, 0);
	}
}

static
struct io_libecc_OpList * continueStatement (struct io_libecc_Parser *self, struct io_libecc_Text text)
{
	struct io_libecc_OpList *oplist = NULL;
	struct io_libecc_Key label = io_libecc_key_none;
	struct io_libecc_Text labelText = self->lexer->text;
	uint16_t depth, lastestDepth, breaker = 0;
	
	if (!self->lexer->didLineBreak && previewToken(self) == io_libecc_lexer_identifierToken)
	{
		label = self->lexer->value.data.key;
		nextToken(self);
	}
	semicolon(self);
	
	depth = self->depthCount;
	if (!depth)
		syntaxError(self, text, io_libecc_Chars.create("continue must be inside loop"));
	
	lastestDepth = 0;
	while (depth--)
	{
		breaker += self->depths[depth].depth;
		if (self->depths[depth].depth)
			lastestDepth = self->depths[depth].depth;
		
		if (lastestDepth == 2)
			if (io_libecc_Key.isEqual(label, io_libecc_key_none) || io_libecc_Key.isEqual(label, self->depths[depth].key))
				return io_libecc_OpList.create(io_libecc_Op.breaker, io_libecc_Value.integer(breaker - 1), self->lexer->text);
	}
	syntaxError(self, labelText, io_libecc_Chars.create("label not found"));
	return oplist;
}

static
struct io_libecc_OpList * breakStatement (struct io_libecc_Parser *self, struct io_libecc_Text text)
{
	struct io_libecc_OpList *oplist = NULL;
	struct io_libecc_Key label = io_libecc_key_none;
	struct io_libecc_Text labelText = self->lexer->text;
	uint16_t depth, breaker = 0;
	
	if (!self->lexer->didLineBreak && previewToken(self) == io_libecc_lexer_identifierToken)
	{
		label = self->lexer->value.data.key;
		nextToken(self);
	}
	semicolon(self);
	
	depth = self->depthCount;
	if (!depth)
		syntaxError(self, text, io_libecc_Chars.create("break must be inside loop or switch"));
	
	while (depth--)
	{
		breaker += self->depths[depth].depth;
		if (io_libecc_Key.isEqual(label, io_libecc_key_none) || io_libecc_Key.isEqual(label, self->depths[depth].key))
			return io_libecc_OpList.create(io_libecc_Op.breaker, io_libecc_Value.integer(breaker), self->lexer->text);
	}
	syntaxError(self, labelText, io_libecc_Chars.create("label not found"));
	return oplist;
}

static
struct io_libecc_OpList * returnStatement (struct io_libecc_Parser *self, struct io_libecc_Text text)
{
	struct io_libecc_OpList *oplist = NULL;
	
	if (self->sourceDepth <= 1)
		syntaxError(self, text, io_libecc_Chars.create("return not in function"));
	
	if (!self->lexer->didLineBreak && previewToken(self) != ';' && previewToken(self) != '}' && previewToken(self) != io_libecc_lexer_noToken)
		oplist = expression(self, 0);
	
	semicolon(self);
	
	if (!oplist)
		oplist = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_value_undefined, io_libecc_Text.join(text, self->lexer->text));
	
	oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.result, io_libecc_value_undefined, io_libecc_Text.join(text, oplist->ops->text)), oplist);
	return oplist;
}

static
struct io_libecc_OpList * switchStatement (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL, *conditionOps = NULL, *defaultOps = NULL;
	struct io_libecc_Text text = io_libecc_text_empty;
	uint32_t conditionCount = 0;
	
	expectToken(self, '(');
	conditionOps = expression(self, 0);
	expectToken(self, ')');
	expectToken(self, '{');
	pushDepth(self, io_libecc_key_none, 1);
	
	while (previewToken(self) != '}' && previewToken(self) != io_libecc_lexer_errorToken && previewToken(self) != io_libecc_lexer_noToken)
	{
		text = self->lexer->text;
		
		if (acceptToken(self, io_libecc_lexer_caseToken))
		{
			conditionOps = io_libecc_OpList.join(conditionOps, expression(self, 0));
			conditionOps = io_libecc_OpList.append(conditionOps, io_libecc_Op.make(io_libecc_Op.value, io_libecc_Value.integer(2 + (oplist? oplist->count: 0)), io_libecc_text_empty));
			++conditionCount;
			expectToken(self, ':');
			oplist = io_libecc_OpList.join(oplist, statementList(self));
		}
		else if (acceptToken(self, io_libecc_lexer_defaultToken))
		{
			if (!defaultOps)
			{
				defaultOps = io_libecc_OpList.create(io_libecc_Op.jump, io_libecc_Value.integer(1 + (oplist? oplist->count: 0)), text);
				expectToken(self, ':');
				oplist = io_libecc_OpList.join(oplist, statementList(self));
			}
			else
				syntaxError(self, text, io_libecc_Chars.create("more than one switch default"));
		}
		else
			syntaxError(self, text, io_libecc_Chars.create("invalid switch statement"));
	}
	
	if (!defaultOps)
		defaultOps = io_libecc_OpList.create(io_libecc_Op.noop, io_libecc_value_none, io_libecc_text_empty);
	
	oplist = io_libecc_OpList.appendNoop(oplist);
	defaultOps = io_libecc_OpList.append(defaultOps, io_libecc_Op.make(io_libecc_Op.jump, io_libecc_Value.integer(oplist? oplist->count : 0), io_libecc_text_empty));
	conditionOps = io_libecc_OpList.unshiftJoin(io_libecc_Op.make(io_libecc_Op.switchOp, io_libecc_Value.integer(conditionOps? conditionOps->count: 0), io_libecc_text_empty), conditionOps, defaultOps);
	oplist = io_libecc_OpList.join(conditionOps, oplist);
	
	popDepth(self);
	expectToken(self, '}');
	return oplist;
}

static
struct io_libecc_OpList * allStatement (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	struct io_libecc_Text text = self->lexer->text;
	
	if (previewToken(self) == '{')
		return block(self);
	else if (acceptToken(self, io_libecc_lexer_varToken))
	{
		oplist = variableDeclarationList(self, 0);
		semicolon(self);
		return oplist;
	}
	else if (acceptToken(self, ';'))
		return io_libecc_OpList.create(io_libecc_Op.next, io_libecc_value_undefined, text);
	else if (acceptToken(self, io_libecc_lexer_ifToken))
		return ifStatement(self);
	else if (acceptToken(self, io_libecc_lexer_doToken))
		return doStatement(self);
	else if (acceptToken(self, io_libecc_lexer_whileToken))
		return whileStatement(self);
	else if (acceptToken(self, io_libecc_lexer_forToken))
		return forStatement(self);
	else if (acceptToken(self, io_libecc_lexer_continueToken))
		return continueStatement(self, text);
	else if (acceptToken(self, io_libecc_lexer_breakToken))
		return breakStatement(self, text);
	else if (acceptToken(self, io_libecc_lexer_returnToken))
		return returnStatement(self, text);
	else if (acceptToken(self, io_libecc_lexer_withToken))
	{
		if (self->strictMode)
			syntaxError(self, text, io_libecc_Chars.create("code may not contain 'with' statements"));
		
		oplist = expression(self, 0);
		if (!oplist)
			tokenError(self, "expression");
		
		oplist = io_libecc_OpList.join(oplist, io_libecc_OpList.appendNoop(statement(self)));
		oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.with, io_libecc_Value.integer(oplist->count), io_libecc_text_empty), oplist);
		
		return oplist;
	}
	else if (acceptToken(self, io_libecc_lexer_switchToken))
		return switchStatement(self);
	else if (acceptToken(self, io_libecc_lexer_throwToken))
	{
		if (!self->lexer->didLineBreak && previewToken(self) != ';' && previewToken(self) != '}' && previewToken(self) != io_libecc_lexer_noToken)
			oplist = expression(self, 0);
		
		if (!oplist)
			syntaxError(self, text, io_libecc_Chars.create("throw statement is missing an expression"));
		
		semicolon(self);
		return io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.throw, io_libecc_value_undefined, io_libecc_Text.join(text, io_libecc_OpList.text(oplist))), oplist);
	}
	else if (acceptToken(self, io_libecc_lexer_tryToken))
	{
		oplist = io_libecc_OpList.appendNoop(block(self));
		oplist = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.try, io_libecc_Value.integer(oplist->count), text), oplist);
		
		if (previewToken(self) != io_libecc_lexer_catchToken && previewToken(self) != io_libecc_lexer_finallyToken)
			tokenError(self, "catch or finally");
		
		if (acceptToken(self, io_libecc_lexer_catchToken))
		{
			struct io_libecc_Op identiferOp;
			struct io_libecc_OpList *catchOps;
			
			expectToken(self, '(');
			if (previewToken(self) != io_libecc_lexer_identifierToken)
				syntaxError(self, text, io_libecc_Chars.create("missing identifier in catch"));
			
			identiferOp = identifier(self);
			expectToken(self, ')');
			
			catchOps = block(self);
			catchOps = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.pushEnvironment, io_libecc_Value.key(identiferOp.value.data.key), text), catchOps);
			catchOps = io_libecc_OpList.append(catchOps, io_libecc_Op.make(io_libecc_Op.popEnvironment, io_libecc_value_undefined, text));
			catchOps = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.jump, io_libecc_Value.integer(catchOps->count), text), catchOps);
			oplist = io_libecc_OpList.join(oplist, catchOps);
		}
		else
			oplist = io_libecc_OpList.append(io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.jump, io_libecc_Value.integer(1), text)), io_libecc_Op.make(io_libecc_Op.noop, io_libecc_value_undefined, text));
		
		if (acceptToken(self, io_libecc_lexer_finallyToken))
			oplist = io_libecc_OpList.join(oplist, block(self));
		
		return io_libecc_OpList.appendNoop(oplist);
	}
	else if (acceptToken(self, io_libecc_lexer_debuggerToken))
	{
		semicolon(self);
		return io_libecc_OpList.create(io_libecc_Op.debugger, io_libecc_value_undefined, text);
	}
	else
	{
		uint32_t index;
		
		oplist = expression(self, 0);
		if (!oplist)
			return NULL;
		
		if (oplist->ops[0].native == io_libecc_Op.getLocal && oplist->count == 1 && acceptToken(self, ':'))
		{
			pushDepth(self, oplist->ops[0].value.data.key, 0);
			free(oplist), oplist = NULL;
			oplist = statement(self);
			popDepth(self);
			return oplist;
		}
		
		acceptToken(self, ';');
		
		index = oplist->count;
		while (index--)
			if (oplist->ops[index].native == io_libecc_Op.call)
				return io_libecc_OpList.unshift(io_libecc_Op.make(self->sourceDepth <=1 ? io_libecc_Op.autoreleaseExpression: io_libecc_Op.autoreleaseDiscard, io_libecc_value_undefined, io_libecc_text_empty), oplist);
		
		return io_libecc_OpList.unshift(io_libecc_Op.make(self->sourceDepth <=1 ? io_libecc_Op.expression: io_libecc_Op.discard, io_libecc_value_undefined, io_libecc_text_empty), oplist);
	}
}

static
struct io_libecc_OpList * statement (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = allStatement(self);
	if (oplist && oplist->count > 1)
		oplist->ops[oplist->ops[0].text.length? 0: 1].text.flags |= io_libecc_text_breakFlag;
	
	return oplist;
}

// MARK: io_libecc_Function

static
struct io_libecc_OpList * parameters (struct io_libecc_Parser *self, int *count)
{
	struct io_libecc_Op op;
	*count = 0;
	if (previewToken(self) != ')')
		do
		{
			++*count;
			op = identifier(self);
			
			if (op.value.data.key.data.integer)
			{
				if (self->strictMode && io_libecc_Key.isEqual(op.value.data.key, io_libecc_key_eval))
					syntaxError(self, op.text, io_libecc_Chars.create("redefining eval is not allowed"));
				else if (self->strictMode && io_libecc_Key.isEqual(op.value.data.key, io_libecc_key_arguments))
					syntaxError(self, op.text, io_libecc_Chars.create("redefining arguments is not allowed"));
				
				io_libecc_Object.deleteMember(&self->function->environment, op.value.data.key);
				io_libecc_Object.addMember(&self->function->environment, op.value.data.key, io_libecc_value_undefined, io_libecc_value_hidden);
			}
		} while (acceptToken(self, ','));
	
	return NULL;
}

static
struct io_libecc_OpList * function (struct io_libecc_Parser *self, int isDeclaration, int isGetter, int isSetter)
{
	struct eccvalue_t value;
	struct io_libecc_Text text, textParameter;
	
	struct io_libecc_OpList *oplist = NULL;
	int parameterCount = 0;
	
	struct io_libecc_Op identifierOp = { 0, io_libecc_value_undefined };
	struct io_libecc_Function *parentFunction;
	struct io_libecc_Function *function;
	union io_libecc_object_Hashmap *arguments;
	uint16_t slot;
	
	if (!isGetter && !isSetter)
	{
		expectToken(self, io_libecc_lexer_functionToken);
		
		if (previewToken(self) == io_libecc_lexer_identifierToken)
		{
			identifierOp = identifier(self);
			
			if (self->strictMode && io_libecc_Key.isEqual(identifierOp.value.data.key, io_libecc_key_eval))
				syntaxError(self, identifierOp.text, io_libecc_Chars.create("redefining eval is not allowed"));
			else if (self->strictMode && io_libecc_Key.isEqual(identifierOp.value.data.key, io_libecc_key_arguments))
				syntaxError(self, identifierOp.text, io_libecc_Chars.create("redefining arguments is not allowed"));
		}
		else if (isDeclaration)
		{
			syntaxError(self, self->lexer->text, io_libecc_Chars.create("function statement requires a name"));
			return NULL;
		}
	}
	
	parentFunction = self->function;
	parentFunction->flags |= io_libecc_function_needHeap;
	
	function = io_libecc_Function.create(&self->function->environment);
	
	arguments = (union io_libecc_object_Hashmap *)io_libecc_Object.addMember(&function->environment, io_libecc_key_arguments, io_libecc_value_undefined, 0);
	slot = arguments - function->environment.hashmap;
	
	self->function = function;
	text = self->lexer->text;
	expectToken(self, '(');
	textParameter = self->lexer->text;
	oplist = io_libecc_OpList.join(oplist, parameters(self, &parameterCount));
	
	function->environment.hashmap[slot].value = io_libecc_value_undefined;
	function->environment.hashmap[slot].value.key = io_libecc_key_arguments;
	function->environment.hashmap[slot].value.flags |= io_libecc_value_hidden | io_libecc_value_sealed;
	
	if (isGetter && parameterCount != 0)
		syntaxError(self, io_libecc_Text.make(textParameter.bytes, (int32_t)(self->lexer->text.bytes - textParameter.bytes)), io_libecc_Chars.create("getter functions must have no arguments"));
	else if (isSetter && parameterCount != 1)
		syntaxError(self, io_libecc_Text.make(self->lexer->text.bytes, 0), io_libecc_Chars.create("setter functions must have one argument"));
	
	expectToken(self, ')');
	expectToken(self, '{');
	
	if (parentFunction->flags & io_libecc_function_strictMode)
		self->function->flags |= io_libecc_function_strictMode;
	
	oplist = io_libecc_OpList.join(oplist, sourceElements(self));
	text.length = (int32_t)(self->lexer->text.bytes - text.bytes) + 1;
	expectToken(self, '}');
	self->function = parentFunction;
	
	function->oplist = oplist;
	function->text = text;
	function->parameterCount = parameterCount;
	
	io_libecc_Object.addMember(&function->object, io_libecc_key_length, io_libecc_Value.integer(parameterCount), io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	
	value = io_libecc_Value.function(function);
	
	if (isDeclaration)
	{
		if (self->function->flags & io_libecc_function_strictMode || self->sourceDepth > 1)
			io_libecc_Object.addMember(&parentFunction->environment, identifierOp.value.data.key, io_libecc_value_undefined, io_libecc_value_hidden);
		else
			io_libecc_Object.addMember(self->global, identifierOp.value.data.key, io_libecc_value_undefined, io_libecc_value_hidden);
	}
	else if (identifierOp.value.type != io_libecc_value_undefinedType && !isGetter && !isSetter)
	{
		io_libecc_Object.addMember(&function->environment, identifierOp.value.data.key, value, io_libecc_value_hidden);
		io_libecc_Object.packValue(&function->environment);
	}
	
	if (isGetter)
		value.flags |= io_libecc_value_getter;
	else if (isSetter)
		value.flags |= io_libecc_value_setter;
	
	if (isDeclaration)
		return io_libecc_OpList.append(io_libecc_OpList.create(io_libecc_Op.setLocal, identifierOp.value, io_libecc_text_empty), io_libecc_Op.make(io_libecc_Op.function, value, text));
	else
		return io_libecc_OpList.create(io_libecc_Op.function, value, text);
}


// MARK: Source

static
struct io_libecc_OpList * sourceElements (struct io_libecc_Parser *self)
{
	struct io_libecc_OpList *oplist = NULL;
	
	++self->sourceDepth;
	
	self->function->oplist = NULL;
	
	if (previewToken(self) == io_libecc_lexer_stringToken
		&& self->lexer->text.length == 10
		&& !memcmp("use strict", self->lexer->text.bytes, 10)
		)
		self->function->flags |= io_libecc_function_strictMode;
	
	oplist = statementList(self);
	
	if (self->sourceDepth <= 1)
		oplist = io_libecc_OpList.appendNoop(oplist);
	else
		oplist = io_libecc_OpList.append(oplist, io_libecc_Op.make(io_libecc_Op.resultVoid, io_libecc_value_undefined, io_libecc_text_empty));
	
	if (self->function->oplist)
		self->function->oplist = io_libecc_OpList.joinDiscarded(NULL, self->function->oplist->count / 2, self->function->oplist);
	
	oplist = io_libecc_OpList.join(self->function->oplist, oplist);
	
	oplist->ops->text.flags |= io_libecc_text_breakFlag;
	if (oplist->count > 1)
		oplist->ops[1].text.flags |= io_libecc_text_breakFlag;
	
	io_libecc_Object.packValue(&self->function->environment);
	
	--self->sourceDepth;
	
	return oplist;
}


// MARK: - Methods

struct io_libecc_Parser * createWithLexer (struct io_libecc_Lexer *lexer)
{
	struct io_libecc_Parser *self = malloc(sizeof(*self));
	*self = io_libecc_Parser.identity;
	
	self->lexer = lexer;
	
	return self;
}

void destroy (struct io_libecc_Parser *self)
{
	assert(self);
	
	io_libecc_Lexer.destroy(self->lexer), self->lexer = NULL;
	free(self->depths), self->depths = NULL;
	free(self), self = NULL;
}

struct io_libecc_Function * parseWithEnvironment (struct io_libecc_Parser * const self, struct eccobject_t *environment, struct eccobject_t *global)
{
	struct io_libecc_Function *function;
	struct io_libecc_OpList *oplist;
	
	assert(self);
	
	function = io_libecc_Function.create(environment);
	self->function = function;
	self->global = global;
	self->reserveGlobalSlots = 0;
	if (self->strictMode)
		function->flags |= io_libecc_function_strictMode;
	
	nextToken(self);
	oplist = sourceElements(self);
	io_libecc_OpList.optimizeWithEnvironment(oplist, &function->environment, 0);
	
	io_libecc_Object.reserveSlots(global, self->reserveGlobalSlots);
	
	if (self->error)
	{
		struct io_libecc_Op errorOps[] = {
			{ io_libecc_Op.throw, io_libecc_value_undefined, self->error->text },
			{ io_libecc_Op.value, io_libecc_Value.error(self->error) },
		};
		errorOps->text.flags |= io_libecc_text_breakFlag;
		
		io_libecc_OpList.destroy(oplist), oplist = NULL;
		oplist = malloc(sizeof(*oplist));
		oplist->ops = malloc(sizeof(errorOps));
		oplist->count = sizeof(errorOps) / sizeof(*errorOps);
		memcpy(oplist->ops, errorOps, sizeof(errorOps));
	}
	
	function->oplist = oplist;
	return function;
}
