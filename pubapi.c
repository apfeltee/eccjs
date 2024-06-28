
/*
//  ecc.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

static int instanceCount = 0;

void ecc_script_addinput(eccstate_t* self, eccioinput_t* input)
{
    size_t needed;
    eccioinput_t** tmp;
    needed = (sizeof(*self->inputs) * (self->inputCount + 1));
    tmp = (eccioinput_t**)realloc(self->inputs, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in addinput: failed to reallocate for %ld bytes\n", needed);
    }
    self->inputs = tmp;
    self->inputs[self->inputCount++] = input;
}

eccstate_t* ecc_script_create(void)
{
    eccstate_t* self;

    if(!instanceCount++)
    {
        ecc_env_setup();
        ecc_mempool_setup();
        ecc_keyidx_setup();
        ecc_globals_setup();
    }
    self = (eccstate_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccstate_t));

    self->globalfunc = ecc_globals_create();
    self->maximumCallDepth = 512;

    return self;
}

void ecc_script_destroy(eccstate_t* self)
{
    assert(self);

    while(self->inputCount--)
        ecc_ioinput_destroy(self->inputs[self->inputCount]), self->inputs[self->inputCount] = NULL;

    free(self->inputs), self->inputs = NULL;
    free(self->envList), self->envList = NULL;
    free(self), self = NULL;

    if(!--instanceCount)
    {
        ecc_globals_teardown();
        ecc_keyidx_teardown();
        ecc_mempool_teardown();
        ecc_env_teardown();
    }
}

void ecc_script_addfunction(eccstate_t* self, const char* name, const eccnativefuncptr_t native, int argumentCount, int flags)
{
    assert(self);

    ecc_function_addfunction(self->globalfunc, name, native, argumentCount, flags);
}

void ecc_script_addvalue(eccstate_t* self, const char* name, eccvalue_t value, int flags)
{
    assert(self);

    ecc_function_addvalue(self->globalfunc, name, value, flags);
}

int ecc_script_evalinput(eccstate_t* self, eccioinput_t* input, int flags)
{
    int result = EXIT_SUCCESS, trap = !self->envCount || flags & ECC_SCRIPTEVAL_PRIMITIVERESULT, catchpos = 0;
    ecccontext_t context = {};
    context.execenv = &self->globalfunc->funcenv;
    context.thisvalue = ecc_value_object(&self->globalfunc->funcenv);
    context.ecc = self;
    context.strictMode = !(flags & ECC_SCRIPTEVAL_SLOPPYMODE);

    if(!input)
        return EXIT_FAILURE;

    self->sloppyMode = flags & ECC_SCRIPTEVAL_SLOPPYMODE;

    if(trap)
    {
        self->printLastThrow = 1;
        catchpos = setjmp(*ecc_script_pushenv(self));
    }

    if(catchpos)
        result = EXIT_FAILURE;
    else
        ecc_script_evalinputwithcontext(self, input, &context);

    if(flags & ECC_SCRIPTEVAL_PRIMITIVERESULT)
    {
        ecc_context_rewindstatement(&context);
        context.text = &context.ops->text;

        if((flags & ECC_SCRIPTEVAL_STRINGRESULT) == ECC_SCRIPTEVAL_STRINGRESULT)
            self->result = ecc_value_tostring(&context, self->result);
        else
            self->result = ecc_value_toprimitive(&context, self->result, ECC_VALHINT_AUTO);
    }

    if(trap)
    {
        ecc_script_popenv(self);
        self->printLastThrow = 0;
    }

    return result;
}

void ecc_script_evalinputwithcontext(eccstate_t* self, eccioinput_t* input, ecccontext_t* context)
{
    eccastlexer_t* lexer;
    eccastparser_t* parser;
    eccobjfunction_t* function;

    assert(self);
    assert(self->envCount);

    ecc_script_addinput(self, input);

    lexer = ecc_astlex_createwithinput(input);
    parser = ecc_astparse_createwithlexer(lexer);

    if(context->strictMode)
        parser->strictMode = 1;

    if(self->sloppyMode)
        lexer->permitutfoutsidelit = 1;

    function = ecc_astparse_parsewithenvironment(parser, context->execenv, &self->globalfunc->funcenv);
    context->ops = function->oplist->ops;
    context->execenv = &function->funcenv;

    ecc_astparse_destroy(parser), parser = NULL;
    /*
    fprintf(stderr, "--- source:\n%.*s\n", input->length, input->bytes);
    ecc_oplist_dumpto(function->oplist, stderr);
    */
    self->result = ECCValConstUndefined;

    context->ops->native(context);
}

jmp_buf* ecc_script_pushenv(eccstate_t* self)
{
    size_t needed;
    uint32_t capacity;
    jmp_buf* tmp;
    if(self->envCount >= self->envCapacity)
    {
        capacity = self->envCapacity ? self->envCapacity * 2 : 8;
        needed = (sizeof(*self->envList) * capacity);
        tmp = (jmp_buf*)realloc(self->envList, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in pushenv: failed to reallocate for %ld bytes\n", needed);
        }
        self->envList = tmp;
        memset(self->envList + self->envCapacity, 0, sizeof(*self->envList) * (capacity - self->envCapacity));
        self->envCapacity = capacity;
    }
    return &self->envList[self->envCount++];
}

void ecc_script_popenv(eccstate_t* self)
{
    assert(self->envCount);

    --self->envCount;
}

void ecc_script_jmpenv(eccstate_t* self, eccvalue_t value)
{
    assert(self);
    assert(self->envCount);

    self->result = value;

    if(value.type == ECC_VALTYPE_ERROR)
        self->text = value.data.error->text;

    longjmp(self->envList[self->envCount - 1], 1);
}

void ecc_script_fatal(const char* format, ...)
{
    int32_t length;
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

        ecc_env_printerror(sizeof(type) - 1, type, "%s", buffer);
    }

    exit(EXIT_FAILURE);
}

eccioinput_t* ecc_script_findinput(eccstate_t* self, ecctextstring_t text)
{
    uint32_t i;

    for(i = 0; i < self->inputCount; ++i)
        if(text.bytes >= self->inputs[i]->bytes && text.bytes <= self->inputs[i]->bytes + self->inputs[i]->length)
            return self->inputs[i];

    return NULL;
}

void ecc_script_printtextinput(eccstate_t* self, ecctextstring_t text, int fullLine)
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
        ofText = ECC_String_Empty;
        ofInput = NULL;
    }

    ecc_ioinput_printtext(ecc_script_findinput(self, text), text, ofLine, ofText, ofInput, fullLine);
}

void ecc_script_garbagecollect(eccstate_t* self)
{
    uint32_t index, count;

    ecc_mempool_unmarkall();
    ecc_mempool_markvalue(ecc_value_object(ECC_Prototype_Arguments));
    ecc_mempool_markvalue(ecc_value_function(self->globalfunc));

    for(index = 0, count = self->inputCount; index < count; ++index)
    {
        eccioinput_t* input = self->inputs[index];
        uint32_t a = input->attachedCount;

        while(a--)
            ecc_mempool_markvalue(input->attached[a]);
    }

    ecc_mempool_collectunmarked();
}
