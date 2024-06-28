//
//  context.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"



void ecc_context_rangeerror(ecccontext_t* self, eccstrbuffer_t* chars)
{
    ecc_context_throw(self, ecc_value_error(ecc_error_rangeerror(ecc_context_textseek(self), chars)));
}

void ecc_context_referenceerror(ecccontext_t* self, eccstrbuffer_t* chars)
{
    ecc_context_throw(self, ecc_value_error(ecc_error_referenceerror(ecc_context_textseek(self), chars)));
}

void ecc_context_syntaxerror(ecccontext_t* self, eccstrbuffer_t* chars)
{
    ecc_context_throw(self, ecc_value_error(ecc_error_syntaxerror(ecc_context_textseek(self), chars)));
}

void ecc_context_typeerror(ecccontext_t* self, eccstrbuffer_t* chars)
{
    ecc_context_throw(self, ecc_value_error(ecc_error_typeerror(ecc_context_textseek(self), chars)));
}

void ecc_context_urierror(ecccontext_t* self, eccstrbuffer_t* chars)
{
    ecc_context_throw(self, ecc_value_error(ecc_error_urierror(ecc_context_textseek(self), chars)));
}

void ecc_context_throw(ecccontext_t * self, eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_ERROR)
        self->ecc->text = value.data.error->text;

    if(self->ecc->printLastThrow && self->ecc->envCount == 1)
    {
        eccvalue_t name, message;
        name = ECCValConstUndefined;

        if(value.type == ECC_VALTYPE_ERROR)
        {
            name = ecc_value_tostring(self, ecc_object_getmember(self, value.data.object, ECC_ConstKey_name));
            message = ecc_value_tostring(self, ecc_object_getmember(self, value.data.object, ECC_ConstKey_message));
        }
        else
            message = ecc_value_tostring(self, value);

        if(name.type == ECC_VALTYPE_UNDEFINED)
            name = ecc_value_fromtext(&ECC_ConstString_ErrorName);

        ecc_env_newline();
        ecc_env_printerror(ecc_value_stringlength(&name), ecc_value_stringbytes(&name), "%.*s", ecc_value_stringlength(&message), ecc_value_stringbytes(&message));
        ecc_context_printbacktrace(self);
        ecc_script_printtextinput(self->ecc, self->ecc->text, 1);
    }

    ecc_script_jmpenv(self->ecc, value);
}

eccvalue_t ecc_context_callfunction(ecccontext_t* self, eccobjfunction_t* function, eccvalue_t thisval, int argumentCount, ...)
{
    eccvalue_t result;
    va_list ap;
    int offset = 0;

    if(argumentCount & ECC_CTXSPECIALTYPE_ASACCESSOR)
    {
        offset = ECC_CTXOFFSET_ACCESSOR;
    }

    va_start(ap, argumentCount);
    result = ecc_oper_callfunctionva(self, offset, function, thisval, argumentCount & ECC_CTXSPECIALTYPE_COUNTMASK, ap);
    va_end(ap);

    return result;
}

int ecc_context_argumentcount(ecccontext_t* self)
{
    if(self->environment->hashmap[2].hmapmapvalue.type == ECC_VALTYPE_OBJECT)
        return self->environment->hashmap[2].hmapmapvalue.data.object->elementCount;
    else
        return self->environment->hashmapCount - 3;
}

eccvalue_t ecc_context_argument(ecccontext_t* self, int argumentIndex)
{
    self->textIndex = argumentIndex + 4;

    if(self->environment->hashmap[2].hmapmapvalue.type == ECC_VALTYPE_OBJECT)
    {
        if(argumentIndex < (int)self->environment->hashmap[2].hmapmapvalue.data.object->elementCount)
            return self->environment->hashmap[2].hmapmapvalue.data.object->element[argumentIndex].hmapitemvalue;
    }
    else if(argumentIndex < (self->environment->hashmapCount - 3))
        return self->environment->hashmap[argumentIndex + 3].hmapmapvalue;

    return ECCValConstNone;
}

void ecc_context_replaceargument(ecccontext_t* self, int argumentIndex, eccvalue_t value)
{
    if(self->environment->hashmap[2].hmapmapvalue.type == ECC_VALTYPE_OBJECT)
    {
        if(argumentIndex < (int)self->environment->hashmap[2].hmapmapvalue.data.object->elementCount)
            self->environment->hashmap[2].hmapmapvalue.data.object->element[argumentIndex].hmapitemvalue = value;
    }
    else if(argumentIndex < self->environment->hashmapCount - 3)
        self->environment->hashmap[argumentIndex + 3].hmapmapvalue = value;
}

eccvalue_t ecc_context_this(ecccontext_t* self)
{
    self->textIndex = ECC_CTXINDEXTYPE_THIS;
    return self->thisvalue;
}

void ecc_context_assertthistype(ecccontext_t* self, int type)
{
    if(self->thisvalue.type != type)
    {
        ecc_context_settextindex(self, ECC_CTXINDEXTYPE_THIS);
        ecc_context_typeerror(self, ecc_strbuf_create("'this' is not a %s", ecc_value_typename(type)));
    }
}

void ecc_context_assertthismask(ecccontext_t* self, int mask)
{
    if(!(self->thisvalue.type & mask))
    {
        ecc_context_settextindex(self, ECC_CTXINDEXTYPE_THIS);
        ecc_context_typeerror(self, ecc_strbuf_create("'this' is not a %s", ecc_value_maskname(mask)));
    }
}

void ecc_context_assertthiscoercibleprimitive(ecccontext_t* self)
{
    if(self->thisvalue.type == ECC_VALTYPE_UNDEFINED || self->thisvalue.type == ECC_VALTYPE_NULL)
    {
        ecc_context_settextindex(self, ECC_CTXINDEXTYPE_THIS);
        ecc_context_typeerror(self, ecc_strbuf_create("'this' cannot be null or undefined"));
    }
}

void ecc_context_settext(ecccontext_t* self, const ecctextstring_t* text)
{
    self->textIndex = ECC_CTXINDEXTYPE_SAVED;
    self->text = text;
}

void ecc_context_settexts(ecccontext_t* self, const ecctextstring_t* text, const ecctextstring_t* textAlt)
{
    self->textIndex = ECC_CTXINDEXTYPE_SAVED;
    self->text = text;
    self->textAlt = textAlt;
}

void ecc_context_settextindex(ecccontext_t* self, int index)
{
    self->textIndex = index;
}

void ecc_context_settextindexargument(ecccontext_t* self, int argument)
{
    self->textIndex = argument + 4;
}

ecctextstring_t ecc_context_textseek(ecccontext_t* self)
{
    const char* bytes;
    ecccontext_t seek = *self;
    uint32_t breakArray = 0, argumentCount = 0;
    ecctextstring_t callText;
    int index;
    int isAccessor = 0;

    assert(self);
    assert(self->ecc);

    index = self->textIndex;

    if(index == ECC_CTXINDEXTYPE_SAVED)
        return *self->text;

    if(index == ECC_CTXINDECTYPE_SAVEDINDEXALT)
        return *self->textAlt;

    while(seek.ops->text.bytes == ECC_ConstString_NativeCode.bytes)
    {
        if(!seek.parent)
            return seek.ops->text;

        isAccessor = seek.argumentOffset == ECC_CTXOFFSET_ACCESSOR;

        if(seek.argumentOffset > 0 && index >= ECC_CTXINDEXTYPE_THIS)
        {
            ++index;
            ++argumentCount;
            breakArray <<= 1;

            if(seek.argumentOffset == ECC_CTXOFFSET_APPLY)
                breakArray |= 2;
        }
        seek = *seek.parent;
    }

    if(seek.ops->native == ecc_oper_noop)
        --seek.ops;

    if(isAccessor)
    {
        if(index > ECC_CTXINDEXTYPE_THIS)
            ecc_context_rewindstatement(&seek);
    }
    else if(index > ECC_CTXINDEXTYPE_NO)
    {
        while(seek.ops->text.bytes != seek.textCall->bytes || seek.ops->text.length != seek.textCall->length)
            --seek.ops;

        argumentCount += seek.ops->value.data.integer;
        callText = seek.ops->text;

        // func
        if(index-- > ECC_CTXINDEXTYPE_CALL)
            ++seek.ops;

        // this
        if(index-- > ECC_CTXINDEXTYPE_CALL && (seek.ops + 1)->text.bytes <= seek.ops->text.bytes)
            ++seek.ops;

        // arguments
        while(index-- > ECC_CTXINDEXTYPE_CALL)
        {
            if(!argumentCount--)
                return ecc_textbuf_make(callText.bytes + callText.length - 1, 0);

            bytes = seek.ops->text.bytes + seek.ops->text.length;
            while(bytes > seek.ops->text.bytes && seek.ops->text.bytes)
                ++seek.ops;

            if(breakArray & 0x1 && seek.ops->native == ecc_oper_array)
                ++seek.ops;

            breakArray >>= 1;
        }
    }

    return seek.ops->text;
}

void ecc_context_rewindstatement(ecccontext_t* context)
{
    while(!(context->ops->text.flags & ECC_TEXTFLAG_BREAKFLAG))
        --context->ops;
}

void ecc_context_printbacktrace(ecccontext_t* context)
{
    int depth = context->depth, count, skip;
    ecccontext_t frame;

    if(depth > 12)
    {
        ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "...");
        fprintf(stderr, " (%d more)\n", depth - 12);
        depth = 12;
    }

    while(depth > 0)
    {
        count = depth--;
        frame = *context;
        skip = 0;

        while(count--)
        {
            --skip;

            if(frame.argumentOffset == ECC_CTXOFFSET_CALL || frame.argumentOffset == ECC_CTXOFFSET_APPLY)
                skip = 2;
            else if(frame.textIndex > ECC_CTXINDEXTYPE_NO && frame.ops->text.bytes == ECC_ConstString_NativeCode.bytes)
                skip = 1;

            frame = *frame.parent;
        }

        if(skip <= 0 && frame.ops->text.bytes != ECC_ConstString_NativeCode.bytes)
        {
            ecc_context_rewindstatement(&frame);
            if(frame.ops->text.length)
                ecc_script_printtextinput(frame.ecc, frame.ops->text, 0);
        }
    }
}

eccobject_t* ecc_context_environmentroot(ecccontext_t* context)
{
    eccobject_t* environment = context->strictMode ? context->environment : &context->ecc->global->environment;

    if(context->strictMode)
        while(environment->prototype && environment->prototype != &context->ecc->global->environment)
            environment = environment->prototype;

    return environment;
}
