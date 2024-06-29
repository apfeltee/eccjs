
/*
//  context.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

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
            name = ecc_value_fromtext(&ECC_String_ErrorName);

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
    if(self->execenv->hmapmapitems[2].hmapmapvalue.type == ECC_VALTYPE_OBJECT)
        return self->execenv->hmapmapitems[2].hmapmapvalue.data.object->hmapitemcount;
    else
        return self->execenv->hmapmapcount - 3;
}

eccvalue_t ecc_context_argument(ecccontext_t* self, int argumentIndex)
{
    self->ctxtextindex = argumentIndex + 4;

    if(self->execenv->hmapmapitems[2].hmapmapvalue.type == ECC_VALTYPE_OBJECT)
    {
        if(argumentIndex < (int)self->execenv->hmapmapitems[2].hmapmapvalue.data.object->hmapitemcount)
            return self->execenv->hmapmapitems[2].hmapmapvalue.data.object->hmapitemitems[argumentIndex].hmapitemvalue;
    }
    else if(argumentIndex < (self->execenv->hmapmapcount - 3))
        return self->execenv->hmapmapitems[argumentIndex + 3].hmapmapvalue;

    return ECCValConstNone;
}

void ecc_context_replaceargument(ecccontext_t* self, int argumentIndex, eccvalue_t value)
{
    if(self->execenv->hmapmapitems[2].hmapmapvalue.type == ECC_VALTYPE_OBJECT)
    {
        if(argumentIndex < (int)self->execenv->hmapmapitems[2].hmapmapvalue.data.object->hmapitemcount)
            self->execenv->hmapmapitems[2].hmapmapvalue.data.object->hmapitemitems[argumentIndex].hmapitemvalue = value;
    }
    else if(argumentIndex < self->execenv->hmapmapcount - 3)
        self->execenv->hmapmapitems[argumentIndex + 3].hmapmapvalue = value;
}

eccvalue_t ecc_context_this(ecccontext_t* self)
{
    self->ctxtextindex = ECC_CTXINDEXTYPE_THIS;
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

void ecc_context_settext(ecccontext_t* self, const eccstrbox_t* text)
{
    self->ctxtextindex = ECC_CTXINDEXTYPE_SAVED;
    self->ctxtextfirst = text;
}

void ecc_context_settexts(ecccontext_t* self, const eccstrbox_t* text, const eccstrbox_t* textAlt)
{
    self->ctxtextindex = ECC_CTXINDEXTYPE_SAVED;
    self->ctxtextfirst = text;
    self->ctxtextalt = textAlt;
}

void ecc_context_settextindex(ecccontext_t* self, int index)
{
    self->ctxtextindex = index;
}

void ecc_context_settextindexargument(ecccontext_t* self, int argument)
{
    self->ctxtextindex = argument + 4;
}

eccstrbox_t ecc_context_textseek(ecccontext_t* self)
{
    const char* bytes;
    ecccontext_t seek = *self;
    uint32_t breakArray = 0, argumentCount = 0;
    eccstrbox_t callText;
    int index;
    int isAccessor = 0;

    assert(self);
    assert(self->ecc);

    index = self->ctxtextindex;

    if(index == ECC_CTXINDEXTYPE_SAVED)
        return *self->ctxtextfirst;

    if(index == ECC_CTXINDECTYPE_SAVEDINDEXALT)
        return *self->ctxtextalt;

    while(seek.ops->text.bytes == ECC_String_NativeCode.bytes)
    {
        if(!seek.parent)
            return seek.ops->text;

        isAccessor = seek.argoffset == ECC_CTXOFFSET_ACCESSOR;

        if(seek.argoffset > 0 && index >= ECC_CTXINDEXTYPE_THIS)
        {
            ++index;
            ++argumentCount;
            breakArray <<= 1;

            if(seek.argoffset == ECC_CTXOFFSET_APPLY)
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
        while(seek.ops->text.bytes != seek.ctxtextcall->bytes || seek.ops->text.length != seek.ctxtextcall->length)
            --seek.ops;

        argumentCount += seek.ops->opvalue.data.integer;
        callText = seek.ops->text;

        /* func */
        if(index-- > ECC_CTXINDEXTYPE_CALL)
            ++seek.ops;

        /* this */
        if(index-- > ECC_CTXINDEXTYPE_CALL && (seek.ops + 1)->text.bytes <= seek.ops->text.bytes)
            ++seek.ops;

        /* arguments */
        while(index-- > ECC_CTXINDEXTYPE_CALL)
        {
            if(!argumentCount--)
                return ecc_strbox_make(callText.bytes + callText.length - 1, 0);

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

            if(frame.argoffset == ECC_CTXOFFSET_CALL || frame.argoffset == ECC_CTXOFFSET_APPLY)
                skip = 2;
            else if(frame.ctxtextindex > ECC_CTXINDEXTYPE_NO && frame.ops->text.bytes == ECC_String_NativeCode.bytes)
                skip = 1;

            frame = *frame.parent;
        }

        if(skip <= 0 && frame.ops->text.bytes != ECC_String_NativeCode.bytes)
        {
            ecc_context_rewindstatement(&frame);
            if(frame.ops->text.length)
                ecc_script_printtextinput(frame.ecc, frame.ops->text, 0);
        }
    }
}

eccobject_t* ecc_context_environmentroot(ecccontext_t* context)
{
    eccobject_t* environment = context->isstrictmode ? context->execenv : &context->ecc->globalfunc->funcenv;

    if(context->isstrictmode)
        while(environment->prototype && environment->prototype != &context->ecc->globalfunc->funcenv)
            environment = environment->prototype;

    return environment;
}
