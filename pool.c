//
//  pool.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static void markValue(eccvalue_t value);
static void cleanupObject(eccobject_t* object);

static eccmempool_t* self = NULL;

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static void addFunction(eccobjfunction_t* function);
static void addObject(eccobject_t* object);
static void addChars(ecccharbuffer_t* chars);
static void unmarkAll(void);
static void markValue(eccvalue_t value);
static void markObject(eccobject_t* object);
static void collectUnmarked(void);
static void collectUnreferencedFromIndices(uint32_t indices[3]);
static void unreferenceFromIndices(uint32_t indices[3]);
static void getIndices(uint32_t indices[3]);
const struct eccpseudonspool_t ECCNSMemoryPool = {
    setup,
    teardown,
    addFunction,
    addObject,
    addChars,
    unmarkAll,
    markValue,
    markObject,
    collectUnmarked,
    collectUnreferencedFromIndices,
    unreferenceFromIndices,
    getIndices,
    {}
};

void markObject(eccobject_t* object)
{
    uint32_t index, count;

    if(object->flags & ECC_OBJFLAG_MARK)
        return;

    object->flags |= ECC_OBJFLAG_MARK;

    if(object->prototype)
        markObject(object->prototype);

    for(index = 0, count = object->elementCount; index < count; ++index)
        if(object->element[index].value.check == 1)
            markValue(object->element[index].value);

    for(index = 2, count = object->hashmapCount; index < count; ++index)
        if(object->hashmap[index].value.check == 1)
            markValue(object->hashmap[index].value);

    if(object->type->mark)
        object->type->mark(object);
}

static void markChars(ecccharbuffer_t* chars)
{
    if(chars->flags & ECC_CHARBUFFLAG_MARK)
        return;

    chars->flags |= ECC_CHARBUFFLAG_MARK;
}

// MARK: - Methods

void setup(void)
{
    assert(!self);

    self = malloc(sizeof(*self));
    *self = ECCNSMemoryPool.identity;
}

void teardown(void)
{
    assert(self);

    unmarkAll();
    collectUnmarked();

    free(self->functionList), self->functionList = NULL;
    free(self->objectList), self->objectList = NULL;
    free(self->charsList), self->charsList = NULL;

    free(self), self = NULL;
}

void addFunction(eccobjfunction_t* function)
{
    assert(function);

    if(self->functionCount >= self->functionCapacity)
    {
        self->functionCapacity = self->functionCapacity ? self->functionCapacity * 2 : 8;
        self->functionList = realloc(self->functionList, self->functionCapacity * sizeof(*self->functionList));
        memset(self->functionList + self->functionCount, 0, sizeof(*self->functionList) * (self->functionCapacity - self->functionCount));
    }

    self->functionList[self->functionCount++] = function;
}

void addObject(eccobject_t* object)
{
    assert(object);

    //	fprintf(stderr, " > add %p %u\n", object, self->objectCount);
    //	ECCNSObject.dumpTo(object, stderr);

    if(self->objectCount >= self->objectCapacity)
    {
        self->objectCapacity = self->objectCapacity ? self->objectCapacity * 2 : 8;
        self->objectList = realloc(self->objectList, self->objectCapacity * sizeof(*self->objectList));
        memset(self->objectList + self->objectCount, 0, sizeof(*self->objectList) * (self->objectCapacity - self->objectCount));
    }

    self->objectList[self->objectCount++] = object;
}

void addChars(ecccharbuffer_t* chars)
{
    assert(chars);

    if(self->charsCount >= self->charsCapacity)
    {
        self->charsCapacity = self->charsCapacity ? self->charsCapacity * 2 : 8;
        self->charsList = realloc(self->charsList, self->charsCapacity * sizeof(*self->charsList));
        memset(self->charsList + self->charsCount, 0, sizeof(*self->charsList) * (self->charsCapacity - self->charsCount));
    }

    self->charsList[self->charsCount++] = chars;
}

void unmarkAll(void)
{
    uint32_t index, count;

    for(index = 0, count = self->functionCount; index < count; ++index)
    {
        self->functionList[index]->object.flags &= ~ECC_OBJFLAG_MARK;
        self->functionList[index]->environment.flags &= ~ECC_OBJFLAG_MARK;
    }

    for(index = 0, count = self->objectCount; index < count; ++index)
        self->objectList[index]->flags &= ~ECC_OBJFLAG_MARK;

    for(index = 0, count = self->charsCount; index < count; ++index)
        self->charsList[index]->flags &= ~ECC_CHARBUFFLAG_MARK;
}

void markValue(eccvalue_t value)
{
    if(value.type >= ECC_VALTYPE_OBJECT)
        markObject(value.data.object);
    else if(value.type == ECC_VALTYPE_CHARS)
        markChars(value.data.chars);
}

static void releaseObject(eccobject_t* object)
{
    if(object->referenceCount > 0 && !--object->referenceCount)
        cleanupObject(object);
}

static eccvalue_t releaseValue(eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        --value.data.chars->referenceCount;
    if(value.type >= ECC_VALTYPE_OBJECT)
        releaseObject(value.data.object);

    return value;
}

static void captureObject(eccobject_t* object);

static eccvalue_t retainValue(eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        ++value.data.chars->referenceCount;
    if(value.type >= ECC_VALTYPE_OBJECT)
    {
        ++value.data.object->referenceCount;
        if(!(value.data.object->flags & ECC_OBJFLAG_MARK))
        {
            value.data.object->flags |= ECC_OBJFLAG_MARK;
            captureObject(value.data.object);
        }
    }
    return value;
}

static void cleanupObject(eccobject_t* object)
{
    eccvalue_t value;

    if(object->prototype && object->prototype->referenceCount)
        --object->prototype->referenceCount;

    if(object->elementCount)
        while(object->elementCount--)
            if((value = object->element[object->elementCount].value).check == 1)
                releaseValue(value);

    if(object->hashmapCount)
        while(object->hashmapCount--)
            if((value = object->hashmap[object->hashmapCount].value).check == 1)
                releaseValue(value);
}

static void captureObject(eccobject_t* object)
{
    uint32_t index, count;
    eccobjelement_t* element;
    ecchashmap_t* hashmap;

    if(object->prototype)
    {
        ++object->prototype->referenceCount;
        if(!(object->prototype->flags & ECC_OBJFLAG_MARK))
        {
            object->prototype->flags |= ECC_OBJFLAG_MARK;
            captureObject(object->prototype);
        }
    }

    count = object->elementCount < object->elementCapacity ? object->elementCount : object->elementCapacity;
    for(index = 0; index < count; ++index)
    {
        element = object->element + index;
        if(element->value.check == 1)
            retainValue(element->value);
    }

    count = object->hashmapCount;
    for(index = 2; index < count; ++index)
    {
        hashmap = object->hashmap + index;
        if(hashmap->value.check == 1)
            retainValue(hashmap->value);
    }

    if(object->type->capture)
        object->type->capture(object);
}

void collectUnmarked(void)
{
    uint32_t index;

    // finalize & destroy

    index = self->functionCount;
    while(index--)
        if(!(self->functionList[index]->object.flags & ECC_OBJFLAG_MARK) && !(self->functionList[index]->environment.flags & ECC_OBJFLAG_MARK))
        {
            ECCNSFunction.destroy(self->functionList[index]);
            self->functionList[index] = self->functionList[--self->functionCount];
        }

    index = self->objectCount;
    while(index--)
        if(!(self->objectList[index]->flags & ECC_OBJFLAG_MARK))
        {
            ECCNSObject.finalize(self->objectList[index]);
            ECCNSObject.destroy(self->objectList[index]);
            self->objectList[index] = self->objectList[--self->objectCount];
        }

    index = self->charsCount;
    while(index--)
        if(!(self->charsList[index]->flags & ECC_CHARBUFFLAG_MARK))
        {
            ECCNSChars.destroy(self->charsList[index]);
            self->charsList[index] = self->charsList[--self->charsCount];
        }
}

void collectUnreferencedFromIndices(uint32_t indices[3])
{
    uint32_t index;

    // prepare

    index = self->objectCount;
    while(index-- > indices[1])
        if(self->objectList[index]->referenceCount <= 0)
            cleanupObject(self->objectList[index]);

    index = self->objectCount;
    while(index-- > indices[1])
        if(self->objectList[index]->referenceCount > 0 && !(self->objectList[index]->flags & ECC_OBJFLAG_MARK))
        {
            self->objectList[index]->flags |= ECC_OBJFLAG_MARK;
            captureObject(self->objectList[index]);
        }

    // destroy

    index = self->functionCount;
    while(index-- > indices[0])
        if(!self->functionList[index]->object.referenceCount && !self->functionList[index]->environment.referenceCount)
        {
            ECCNSFunction.destroy(self->functionList[index]);
            self->functionList[index] = self->functionList[--self->functionCount];
        }

    index = self->objectCount;
    while(index-- > indices[1])
        if(self->objectList[index]->referenceCount <= 0)
        {
            ECCNSObject.finalize(self->objectList[index]);
            ECCNSObject.destroy(self->objectList[index]);
            self->objectList[index] = self->objectList[--self->objectCount];
        }

    index = self->charsCount;
    while(index-- > indices[2])
        if(self->charsList[index]->referenceCount <= 0)
        {
            ECCNSChars.destroy(self->charsList[index]);
            self->charsList[index] = self->charsList[--self->charsCount];
        }
}

void unreferenceFromIndices(uint32_t indices[3])
{
    uint32_t index;

    index = self->functionCount;
    while(index-- > indices[0])
    {
        --self->functionList[index]->object.referenceCount;
        --self->functionList[index]->environment.referenceCount;
    }

    index = self->objectCount;
    while(index-- > indices[1])
        --self->objectList[index]->referenceCount;

    index = self->charsCount;
    while(index-- > indices[2])
        --self->charsList[index]->referenceCount;
}

void getIndices(uint32_t indices[3])
{
    indices[0] = self->functionCount;
    indices[1] = self->objectCount;
    indices[2] = self->charsCount;
}
