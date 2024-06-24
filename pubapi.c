//
//  ecc.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static struct io_libecc_Ecc* create(void);
static void destroy(struct io_libecc_Ecc*);
static void addValue(struct io_libecc_Ecc*, const char* name, eccvalue_t value, enum io_libecc_value_Flags);
static void addFunction(struct io_libecc_Ecc*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
static int evalInput(struct io_libecc_Ecc*, struct io_libecc_Input*, enum io_libecc_ecc_EvalFlags);
static void evalInputWithContext(struct io_libecc_Ecc*, struct io_libecc_Input*, eccstate_t* context);
static jmp_buf* pushEnv(struct io_libecc_Ecc*);
static void popEnv(struct io_libecc_Ecc*);
static void jmpEnv(struct io_libecc_Ecc*, eccvalue_t value) __attribute__((noreturn));
static void fatal(const char* format, ...) __attribute__((noreturn));
static struct io_libecc_Input* findInput(struct io_libecc_Ecc* self, ecctextstring_t text);
static void printTextInput(struct io_libecc_Ecc*, ecctextstring_t text, int fullLine);
static void garbageCollect(struct io_libecc_Ecc*);
const struct type_io_libecc_Ecc io_libecc_Ecc = {
    create, destroy, addValue, addFunction, evalInput, evalInputWithContext, pushEnv, popEnv, jmpEnv, fatal, findInput, printTextInput, garbageCollect,
};


static int instanceCount = 0;

// MARK: - Static Members

static
void addInput(struct io_libecc_Ecc *self, struct io_libecc_Input *input)
{
	self->inputs = realloc(self->inputs, sizeof(*self->inputs) * (self->inputCount + 1));
	self->inputs[self->inputCount++] = input;
}

// MARK: - Methods

uint32_t io_libecc_ecc_version = (0 << 24) | (1 << 16) | (0 << 0);

struct io_libecc_Ecc *create (void)
{
	struct io_libecc_Ecc *self;
	
	if (!instanceCount++)
	{
		io_libecc_Env.setup();
		io_libecc_Pool.setup();
		io_libecc_Key.setup();
		io_libecc_Global.setup();
	}
	
	self = malloc(sizeof(*self));
	*self = io_libecc_Ecc.identity;
	
	self->global = io_libecc_Global.create();
	self->maximumCallDepth = 512;
	
	return self;
}

void destroy (struct io_libecc_Ecc *self)
{
	assert(self);
	
	while (self->inputCount--)
		io_libecc_Input.destroy(self->inputs[self->inputCount]), self->inputs[self->inputCount] = NULL;
	
	free(self->inputs), self->inputs = NULL;
	free(self->envList), self->envList = NULL;
	free(self), self = NULL;
	
	if (!--instanceCount)
	{
		io_libecc_Global.teardown();
		io_libecc_Key.teardown();
		io_libecc_Pool.teardown();
		io_libecc_Env.teardown();
	}
}

void addFunction (struct io_libecc_Ecc *self, const char *name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags flags)
{
	assert(self);
	
	io_libecc_Function.addFunction(self->global, name, native, argumentCount, flags);
}

void addValue (struct io_libecc_Ecc *self, const char *name, eccvalue_t value, enum io_libecc_value_Flags flags)
{
	assert(self);
	
	io_libecc_Function.addValue(self->global, name, value, flags);
}

io_libecc_ecc_useframe
int evalInput (struct io_libecc_Ecc *self, struct io_libecc_Input *input, enum io_libecc_ecc_EvalFlags flags)
{
	volatile int result = EXIT_SUCCESS, trap = !self->envCount || flags & io_libecc_ecc_primitiveResult, catch = 0;
	eccstate_t context = {
		.environment = &self->global->environment,
		.this = ECCNSValue.object(&self->global->environment),
		.ecc = self,
		.strictMode = !(flags & io_libecc_ecc_sloppyMode),
	};
	
	if (!input)
		return EXIT_FAILURE;
	
	self->sloppyMode = flags & io_libecc_ecc_sloppyMode;
	
	if (trap)
	{
		self->printLastThrow = 1;
		catch = setjmp(*pushEnv(self));
	}
	
	if (catch)
		result = EXIT_FAILURE;
	else
		evalInputWithContext(self, input, &context);
	
	if (flags & io_libecc_ecc_primitiveResult)
	{
		io_libecc_Context.rewindStatement(&context);
		context.text = &context.ops->text;
		
		if ((flags & io_libecc_ecc_stringResult) == io_libecc_ecc_stringResult)
			self->result = ECCNSValue.toString(&context, self->result);
		else
			self->result = ECCNSValue.toPrimitive(&context, self->result, io_libecc_value_hintAuto);
	}
	
	if (trap)
	{
		popEnv(self);
		self->printLastThrow = 0;
	}
	
	return result;
}

void evalInputWithContext (struct io_libecc_Ecc *self, struct io_libecc_Input *input, eccstate_t *context)
{
	struct io_libecc_Lexer *lexer;
	struct io_libecc_Parser *parser;
	struct io_libecc_Function *function;
	
	assert(self);
	assert(self->envCount);
	
	addInput(self, input);
	
	lexer = io_libecc_Lexer.createWithInput(input);
	parser = io_libecc_Parser.createWithLexer(lexer);
	
	if (context->strictMode)
		parser->strictMode = 1;
	
	if (self->sloppyMode)
		lexer->allowUnicodeOutsideLiteral = 1;
	
	function = io_libecc_Parser.parseWithEnvironment(parser, context->environment, &self->global->environment);
	context->ops = function->oplist->ops;
	context->environment = &function->environment;
	
	io_libecc_Parser.destroy(parser), parser = NULL;
	
//	fprintf(stderr, "--- source:\n%.*s\n", input->length, input->bytes);
//	io_libecc_OpList.dumpTo(function->oplist, stderr);
	
	self->result = ECCValConstUndefined;
	
	context->ops->native(context);
}

jmp_buf * pushEnv(struct io_libecc_Ecc *self)
{
	if (self->envCount >= self->envCapacity)
	{
		uint16_t capacity = self->envCapacity? self->envCapacity * 2: 8;
		self->envList = realloc(self->envList, sizeof(*self->envList) * capacity);
		memset(self->envList + self->envCapacity, 0, sizeof(*self->envList) * (capacity - self->envCapacity));
		self->envCapacity = capacity;
	}
	return &self->envList[self->envCount++];
}

void popEnv(struct io_libecc_Ecc *self)
{
	assert(self->envCount);
	
	--self->envCount;
}

void jmpEnv (struct io_libecc_Ecc *self, eccvalue_t value)
{
	assert(self);
	assert(self->envCount);
	
	self->result = value;
	
	if (value.type == ECC_VALTYPE_ERROR)
		self->text = value.data.error->text;
	
	longjmp(self->envList[self->envCount - 1], 1);
}

void fatal (const char *format, ...)
{
	int16_t length;
	va_list ap;
	
	va_start(ap, format);
	length = vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	{
		const char type[] = "Fatal";
		char buffer[length + 1];
		
		va_start(ap, format);
		vsprintf(buffer, format, ap);
		va_end(ap);
		
		io_libecc_Env.printError(sizeof(type)-1, type, "%s", buffer);
	}
	
	exit(EXIT_FAILURE);
}

struct io_libecc_Input * findInput (struct io_libecc_Ecc *self, ecctextstring_t text)
{
	uint16_t i;
	
	for (i = 0; i < self->inputCount; ++i)
		if (text.bytes >= self->inputs[i]->bytes && text.bytes <= self->inputs[i]->bytes + self->inputs[i]->length)
			return self->inputs[i];
	
	return NULL;
}

void printTextInput (struct io_libecc_Ecc *self, ecctextstring_t text, int fullLine)
{
	int32_t ofLine;
	ecctextstring_t ofText;
	const char *ofInput;
	
	assert(self);
	
	if (text.bytes >= self->ofText.bytes && text.bytes < self->ofText.bytes + self->ofText.length)
	{
		ofLine = self->ofLine;
		ofText = self->ofText;
		ofInput = self->ofInput;
	}
	else
	{
		ofLine = 0;
		ofText = ECC_ConstString_Empty;
		ofInput = NULL;
	}
	
	io_libecc_Input.printText(findInput(self, text), text, ofLine, ofText, ofInput, fullLine);
}

void garbageCollect(struct io_libecc_Ecc *self)
{
	uint16_t index, count;
	
	io_libecc_Pool.unmarkAll();
	io_libecc_Pool.markValue(ECCNSValue.object(io_libecc_arguments_prototype));
	io_libecc_Pool.markValue(ECCNSValue.function(self->global));
	
	for (index = 0, count = self->inputCount; index < count; ++index)
	{
		struct io_libecc_Input *input = self->inputs[index];
		uint16_t a = input->attachedCount;
		
		while (a--)
			io_libecc_Pool.markValue(input->attached[a]);
	}
	
	io_libecc_Pool.collectUnmarked();
}
