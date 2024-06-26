//
//  ecc.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static eccscriptcontext_t* create(void);
static void destroy(eccscriptcontext_t*);
static void addValue(eccscriptcontext_t*, const char* name, eccvalue_t value, eccvalflag_t);
static void addFunction(eccscriptcontext_t*, const char* name, const eccnativefuncptr_t native, int argumentCount, eccvalflag_t);
static int evalInput(eccscriptcontext_t*, eccioinput_t*, eccscriptevalflags_t);
static void evalInputWithContext(eccscriptcontext_t*, eccioinput_t*, eccstate_t* context);
static jmp_buf* pushEnv(eccscriptcontext_t*);
static void popEnv(eccscriptcontext_t*);
static void jmpEnv(eccscriptcontext_t*, eccvalue_t value);
static void fatal(const char* format, ...);
static eccioinput_t* findInput(eccscriptcontext_t* self, ecctextstring_t text);
static void printTextInput(eccscriptcontext_t*, ecctextstring_t text, int fullLine);
static void garbageCollect(eccscriptcontext_t*);
const struct eccpseudonsecc_t ECCNSScript = {
    create, destroy, addValue, addFunction, evalInput, evalInputWithContext, pushEnv, popEnv, jmpEnv, fatal, findInput, printTextInput, garbageCollect,
    {}
};

static int instanceCount = 0;

// MARK: - Static Members

static void addInput(eccscriptcontext_t* self, eccioinput_t* input)
{
    self->inputs = realloc(self->inputs, sizeof(*self->inputs) * (self->inputCount + 1));
    self->inputs[self->inputCount++] = input;
}

// MARK: - Methods

uint32_t io_libecc_ecc_version = (0 << 24) | (1 << 16) | (0 << 0);

eccscriptcontext_t* create(void)
{
    eccscriptcontext_t* self;

    if(!instanceCount++)
    {
        ECCNSEnv.setup();
        io_libecc_Pool.setup();
        io_libecc_Key.setup();
        io_libecc_Global.setup();
    }

    self = malloc(sizeof(*self));
    *self = ECCNSScript.identity;

    self->global = io_libecc_Global.create();
    self->maximumCallDepth = 512;

    return self;
}

void destroy(eccscriptcontext_t* self)
{
    assert(self);

    while(self->inputCount--)
        io_libecc_Input.destroy(self->inputs[self->inputCount]), self->inputs[self->inputCount] = NULL;

    free(self->inputs), self->inputs = NULL;
    free(self->envList), self->envList = NULL;
    free(self), self = NULL;

    if(!--instanceCount)
    {
        io_libecc_Global.teardown();
        io_libecc_Key.teardown();
        io_libecc_Pool.teardown();
        ECCNSEnv.teardown();
    }
}

void addFunction(eccscriptcontext_t* self, const char* name, const eccnativefuncptr_t native, int argumentCount, eccvalflag_t flags)
{
    assert(self);

    io_libecc_Function.addFunction(self->global, name, native, argumentCount, flags);
}

void addValue(eccscriptcontext_t* self, const char* name, eccvalue_t value, eccvalflag_t flags)
{
    assert(self);

    io_libecc_Function.addValue(self->global, name, value, flags);
}

int evalInput(eccscriptcontext_t* self, eccioinput_t* input, eccscriptevalflags_t flags)
{
    volatile int result = EXIT_SUCCESS, trap = !self->envCount || flags & ECC_SCRIPTEVAL_PRIMITIVERESULT, catch = 0;
    eccstate_t context = {
        .environment = &self->global->environment,
        .thisvalue = ECCNSValue.object(&self->global->environment),
        .ecc = self,
        .strictMode = !(flags & ECC_SCRIPTEVAL_SLOPPYMODE),
    };

    if(!input)
        return EXIT_FAILURE;

    self->sloppyMode = flags & ECC_SCRIPTEVAL_SLOPPYMODE;

    if(trap)
    {
        self->printLastThrow = 1;
        catch = setjmp(*pushEnv(self));
    }

    if(catch)
        result = EXIT_FAILURE;
    else
        evalInputWithContext(self, input, &context);

    if(flags & ECC_SCRIPTEVAL_PRIMITIVERESULT)
    {
        ECCNSContext.rewindStatement(&context);
        context.text = &context.ops->text;

        if((flags & ECC_SCRIPTEVAL_STRINGRESULT) == ECC_SCRIPTEVAL_STRINGRESULT)
            self->result = ECCNSValue.toString(&context, self->result);
        else
            self->result = ECCNSValue.toPrimitive(&context, self->result, ECC_VALHINT_AUTO);
    }

    if(trap)
    {
        popEnv(self);
        self->printLastThrow = 0;
    }

    return result;
}

void evalInputWithContext(eccscriptcontext_t* self, eccioinput_t* input, eccstate_t* context)
{
    eccastlexer_t* lexer;
    eccastparser_t* parser;
    eccobjscriptfunction_t* function;

    assert(self);
    assert(self->envCount);

    addInput(self, input);

    lexer = io_libecc_Lexer.createWithInput(input);
    parser = io_libecc_Parser.createWithLexer(lexer);

    if(context->strictMode)
        parser->strictMode = 1;

    if(self->sloppyMode)
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

jmp_buf* pushEnv(eccscriptcontext_t* self)
{
    if(self->envCount >= self->envCapacity)
    {
        uint16_t capacity = self->envCapacity ? self->envCapacity * 2 : 8;
        self->envList = realloc(self->envList, sizeof(*self->envList) * capacity);
        memset(self->envList + self->envCapacity, 0, sizeof(*self->envList) * (capacity - self->envCapacity));
        self->envCapacity = capacity;
    }
    return &self->envList[self->envCount++];
}

void popEnv(eccscriptcontext_t* self)
{
    assert(self->envCount);

    --self->envCount;
}

void jmpEnv(eccscriptcontext_t* self, eccvalue_t value)
{
    assert(self);
    assert(self->envCount);

    self->result = value;

    if(value.type == ECC_VALTYPE_ERROR)
        self->text = value.data.error->text;

    longjmp(self->envList[self->envCount - 1], 1);
}

void fatal(const char* format, ...)
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

        ECCNSEnv.printError(sizeof(type) - 1, type, "%s", buffer);
    }

    exit(EXIT_FAILURE);
}

eccioinput_t* findInput(eccscriptcontext_t* self, ecctextstring_t text)
{
    uint16_t i;

    for(i = 0; i < self->inputCount; ++i)
        if(text.bytes >= self->inputs[i]->bytes && text.bytes <= self->inputs[i]->bytes + self->inputs[i]->length)
            return self->inputs[i];

    return NULL;
}

void printTextInput(eccscriptcontext_t* self, ecctextstring_t text, int fullLine)
{
    int32_t ofLine;
    ecctextstring_t ofText;
    const char* ofInput;

    assert(self);

    if(text.bytes >= self->ofText.bytes && text.bytes < self->ofText.bytes + self->ofText.length)
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

void garbageCollect(eccscriptcontext_t* self)
{
    uint16_t index, count;

    io_libecc_Pool.unmarkAll();
    io_libecc_Pool.markValue(ECCNSValue.object(io_libecc_arguments_prototype));
    io_libecc_Pool.markValue(ECCNSValue.function(self->global));

    for(index = 0, count = self->inputCount; index < count; ++index)
    {
        eccioinput_t* input = self->inputs[index];
        uint16_t a = input->attachedCount;

        while(a--)
            io_libecc_Pool.markValue(input->attached[a]);
    }

    io_libecc_Pool.collectUnmarked();
}
