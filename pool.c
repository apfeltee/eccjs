
/*
//  pool.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

static eccmempool_t* self = NULL;

void ecc_mempool_markobject(eccobject_t* object)
{
    uint32_t index, count;

    if(object->flags & ECC_OBJFLAG_MARK)
        return;

    object->flags |= ECC_OBJFLAG_MARK;

    if(object->prototype)
        ecc_mempool_markobject(object->prototype);

    for(index = 0, count = object->hmapitemcount; index < count; ++index)
        if(object->hmapitemitems[index].hmapitemvalue.check == 1)
            ecc_mempool_markvalue(object->hmapitemitems[index].hmapitemvalue);

    for(index = 2, count = object->hmapmapcount; index < count; ++index)
        if(object->hmapmapitems[index].hmapmapvalue.check == 1)
            ecc_mempool_markvalue(object->hmapmapitems[index].hmapmapvalue);

    if(object->type->fnmark)
        object->type->fnmark(object);
}

void ecc_mempool_markchars(eccstrbuffer_t* chars)
{
    if(chars->flags & ECC_CHARBUFFLAG_MARK)
        return;

    chars->flags |= ECC_CHARBUFFLAG_MARK;
}

void ecc_mempool_setup(void)
{
    assert(!self);
    self = (eccmempool_t*)malloc(sizeof(*self));
    memset(self,0, sizeof(eccmempool_t));
}

void ecc_mempool_teardown(void)
{
    assert(self);

    ecc_mempool_unmarkall();
    ecc_mempool_collectunmarked();

    free(self->funclistvals), self->funclistvals = NULL;
    free(self->objlistvals), self->objlistvals = NULL;
    free(self->sbuflistvals), self->sbuflistvals = NULL;

    free(self), self = NULL;
}

void ecc_mempool_addfunction(eccobjfunction_t* function)
{
    size_t needed;
    eccobjfunction_t** tmp;
    assert(function);
    if(self->funclistcount >= self->funclistcapacity)
    {
        self->funclistcapacity = self->funclistcapacity ? self->funclistcapacity * 2 : 8;
        needed = (self->funclistcapacity * sizeof(*self->funclistvals));
        tmp = (eccobjfunction_t**)realloc(self->funclistvals, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in addfunction: failed to reallocate for %ld bytes\n", needed);
        }
        self->funclistvals = tmp;
        memset(self->funclistvals + self->funclistcount, 0, sizeof(*self->funclistvals) * (self->funclistcapacity - self->funclistcount));
    }

    self->funclistvals[self->funclistcount++] = function;
}

void ecc_mempool_addobject(eccobject_t* object)
{
    size_t needed;
    eccobject_t** tmp;
    assert(object);
    /*
    fprintf(stderr, " > add %p %u\n", object, self->objlistcount);
    ecc_object_dumpto(object, stderr);
    */
    if(self->objlistcount >= self->objlistcapacity)
    {
        self->objlistcapacity = self->objlistcapacity ? self->objlistcapacity * 2 : 8;
        needed = (self->objlistcapacity * sizeof(*self->objlistvals));
        tmp = (eccobject_t**)realloc(self->objlistvals, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in addobject: failed to reallocate for %ld bytes\n", needed);
        }
        self->objlistvals = tmp;
        memset(self->objlistvals + self->objlistcount, 0, sizeof(*self->objlistvals) * (self->objlistcapacity - self->objlistcount));
    }
    self->objlistvals[self->objlistcount++] = object;
}

void ecc_mempool_addchars(eccstrbuffer_t* chars)
{
    size_t needed;
    eccstrbuffer_t** tmp;
    assert(chars);
    if(self->sbuflistcount >= self->sbuflistcapacity)
    {
        self->sbuflistcapacity = self->sbuflistcapacity ? self->sbuflistcapacity * 2 : 8;
        needed = (self->sbuflistcapacity * sizeof(*self->sbuflistvals));
        tmp = (eccstrbuffer_t**)realloc(self->sbuflistvals, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in addchars: failed to reallocate for %ld bytes\n", needed);
        }
        self->sbuflistvals = tmp;
        memset(self->sbuflistvals + self->sbuflistcount, 0, sizeof(*self->sbuflistvals) * (self->sbuflistcapacity - self->sbuflistcount));
    }

    self->sbuflistvals[self->sbuflistcount++] = chars;
}

void ecc_mempool_unmarkall(void)
{
    uint32_t index, count;

    for(index = 0, count = self->funclistcount; index < count; ++index)
    {
        self->funclistvals[index]->object.flags &= ~ECC_OBJFLAG_MARK;
        self->funclistvals[index]->funcenv.flags &= ~ECC_OBJFLAG_MARK;
    }

    for(index = 0, count = self->objlistcount; index < count; ++index)
        self->objlistvals[index]->flags &= ~ECC_OBJFLAG_MARK;

    for(index = 0, count = self->sbuflistcount; index < count; ++index)
        self->sbuflistvals[index]->flags &= ~ECC_CHARBUFFLAG_MARK;
}

void ecc_mempool_markvalue(eccvalue_t value)
{
    if(value.type >= ECC_VALTYPE_OBJECT)
        ecc_mempool_markobject(value.data.object);
    else if(value.type == ECC_VALTYPE_CHARS)
        ecc_mempool_markchars(value.data.chars);
}

void ecc_mempool_releaseobject(eccobject_t* object)
{
    if(object->refcount > 0 && !--object->refcount)
        ecc_mempool_cleanupobject(object);
}

eccvalue_t ecc_mempool_releasevalue(eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        --value.data.chars->refcount;
    if(value.type >= ECC_VALTYPE_OBJECT)
        ecc_mempool_releaseobject(value.data.object);

    return value;
}

eccvalue_t ecc_mempool_retainvalue(eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        ++value.data.chars->refcount;
    if(value.type >= ECC_VALTYPE_OBJECT)
    {
        ++value.data.object->refcount;
        if(!(value.data.object->flags & ECC_OBJFLAG_MARK))
        {
            value.data.object->flags |= ECC_OBJFLAG_MARK;
            ecc_mempool_captureobject(value.data.object);
        }
    }
    return value;
}

void ecc_mempool_cleanupobject(eccobject_t* object)
{
    eccvalue_t value;

    if(object->prototype && object->prototype->refcount)
        --object->prototype->refcount;

    if(object->hmapitemcount)
        while(object->hmapitemcount--)
            if((value = object->hmapitemitems[object->hmapitemcount].hmapitemvalue).check == 1)
                ecc_mempool_releasevalue(value);

    if(object->hmapmapcount)
        while(object->hmapmapcount--)
            if((value = object->hmapmapitems[object->hmapmapcount].hmapmapvalue).check == 1)
                ecc_mempool_releasevalue(value);
}

void ecc_mempool_captureobject(eccobject_t* object)
{
    uint32_t index, count;
    ecchashitem_t* element;
    ecchashmap_t* hashmap;

    if(object->prototype)
    {
        ++object->prototype->refcount;
        if(!(object->prototype->flags & ECC_OBJFLAG_MARK))
        {
            object->prototype->flags |= ECC_OBJFLAG_MARK;
            ecc_mempool_captureobject(object->prototype);
        }
    }

    count = object->hmapitemcount < object->hmapitemcapacity ? object->hmapitemcount : object->hmapitemcapacity;
    for(index = 0; index < count; ++index)
    {
        element = object->hmapitemitems + index;
        if(element->hmapitemvalue.check == 1)
            ecc_mempool_retainvalue(element->hmapitemvalue);
    }

    count = object->hmapmapcount;
    for(index = 2; index < count; ++index)
    {
        hashmap = object->hmapmapitems + index;
        if(hashmap->hmapmapvalue.check == 1)
            ecc_mempool_retainvalue(hashmap->hmapmapvalue);
    }

    if(object->type->fncapture)
        object->type->fncapture(object);
}

void ecc_mempool_collectunmarked(void)
{
    uint32_t index;

    /* finalize & destroy */

    index = self->funclistcount;
    while(index--)
        if(!(self->funclistvals[index]->object.flags & ECC_OBJFLAG_MARK) && !(self->funclistvals[index]->funcenv.flags & ECC_OBJFLAG_MARK))
        {
            ecc_function_destroy(self->funclistvals[index]);
            self->funclistvals[index] = self->funclistvals[--self->funclistcount];
        }

    index = self->objlistcount;
    while(index--)
        if(!(self->objlistvals[index]->flags & ECC_OBJFLAG_MARK))
        {
            ecc_object_finalize(self->objlistvals[index]);
            ecc_object_destroy(self->objlistvals[index]);
            self->objlistvals[index] = self->objlistvals[--self->objlistcount];
        }

    index = self->sbuflistcount;
    while(index--)
        if(!(self->sbuflistvals[index]->flags & ECC_CHARBUFFLAG_MARK))
        {
            ecc_strbuf_destroy(self->sbuflistvals[index]);
            self->sbuflistvals[index] = self->sbuflistvals[--self->sbuflistcount];
        }
}

void ecc_mempool_collectunreferencedfromindices(uint32_t indices[3])
{
    uint32_t index;

    /* prepare */

    index = self->objlistcount;
    while(index-- > indices[1])
        if(self->objlistvals[index]->refcount <= 0)
            ecc_mempool_cleanupobject(self->objlistvals[index]);

    index = self->objlistcount;
    while(index-- > indices[1])
        if(self->objlistvals[index]->refcount > 0 && !(self->objlistvals[index]->flags & ECC_OBJFLAG_MARK))
        {
            self->objlistvals[index]->flags |= ECC_OBJFLAG_MARK;
            ecc_mempool_captureobject(self->objlistvals[index]);
        }

    /* destroy */

    index = self->funclistcount;
    while(index-- > indices[0])
        if(!self->funclistvals[index]->object.refcount && !self->funclistvals[index]->funcenv.refcount)
        {
            ecc_function_destroy(self->funclistvals[index]);
            self->funclistvals[index] = self->funclistvals[--self->funclistcount];
        }

    index = self->objlistcount;
    while(index-- > indices[1])
        if(self->objlistvals[index]->refcount <= 0)
        {
            ecc_object_finalize(self->objlistvals[index]);
            ecc_object_destroy(self->objlistvals[index]);
            self->objlistvals[index] = self->objlistvals[--self->objlistcount];
        }

    index = self->sbuflistcount;
    while(index-- > indices[2])
        if(self->sbuflistvals[index]->refcount <= 0)
        {
            ecc_strbuf_destroy(self->sbuflistvals[index]);
            self->sbuflistvals[index] = self->sbuflistvals[--self->sbuflistcount];
        }
}

void ecc_mempool_unreferencefromindices(uint32_t indices[3])
{
    uint32_t index;

    index = self->funclistcount;
    while(index-- > indices[0])
    {
        --self->funclistvals[index]->object.refcount;
        --self->funclistvals[index]->funcenv.refcount;
    }

    index = self->objlistcount;
    while(index-- > indices[1])
        --self->objlistvals[index]->refcount;

    index = self->sbuflistcount;
    while(index-- > indices[2])
        --self->sbuflistvals[index]->refcount;
}

void ecc_mempool_getindices(uint32_t indices[3])
{
    indices[0] = self->funclistcount;
    indices[1] = self->objlistcount;
    indices[2] = self->sbuflistcount;
}
