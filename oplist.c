//
//  oplist.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

// MARK: - Static Members

// MARK: - Methods
static eccoplist_t* create(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text);
static void destroy(eccoplist_t*);
static eccoplist_t* join(eccoplist_t*, eccoplist_t*);
static eccoplist_t* join3(eccoplist_t*, eccoplist_t*, eccoplist_t*);
static eccoplist_t* joinDiscarded(eccoplist_t*, uint16_t n, eccoplist_t*);
static eccoplist_t* unshift(eccoperand_t op, eccoplist_t*);
static eccoplist_t* unshiftJoin(eccoperand_t op, eccoplist_t*, eccoplist_t*);
static eccoplist_t* unshiftJoin3(eccoperand_t op, eccoplist_t*, eccoplist_t*, eccoplist_t*);
static eccoplist_t* shift(eccoplist_t*);
static eccoplist_t* append(eccoplist_t*, eccoperand_t op);
static eccoplist_t* appendNoop(eccoplist_t*);
static eccoplist_t* createLoop(eccoplist_t* initial, eccoplist_t* condition, eccoplist_t* step, eccoplist_t* body, int reverseCondition);
static void optimizeWithEnvironment(eccoplist_t*, eccobject_t* environment, uint32_t index);
static void dumpTo(eccoplist_t*, FILE* file);
static ecctextstring_t text(eccoplist_t* oplist);
const struct type_io_libecc_OpList io_libecc_OpList = {
    create, destroy, join,       join3,      joinDiscarded,           unshift, unshiftJoin, unshiftJoin3,
    shift,  append,  appendNoop, createLoop, optimizeWithEnvironment, dumpTo,  text,
    {}
};

eccoplist_t* create(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text)
{
    eccoplist_t* self = malloc(sizeof(*self));
    self->ops = malloc(sizeof(*self->ops) * 1);
    self->ops[0] = io_libecc_Op.make(native, value, text);
    self->count = 1;
    return self;
}

void destroy(eccoplist_t* self)
{
    assert(self);

    free(self->ops), self->ops = NULL;
    free(self), self = NULL;
}

eccoplist_t* join(eccoplist_t* self, eccoplist_t* with)
{
    if(!self)
        return with;
    else if(!with)
        return self;

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + with->count));
    memcpy(self->ops + self->count, with->ops, sizeof(*self->ops) * with->count);
    self->count += with->count;

    destroy(with), with = NULL;

    return self;
}

eccoplist_t* join3(eccoplist_t* self, eccoplist_t* a, eccoplist_t* b)
{
    if(!self)
        return join(a, b);
    else if(!a)
        return join(self, b);
    else if(!b)
        return join(self, a);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + a->count + b->count));
    memcpy(self->ops + self->count, a->ops, sizeof(*self->ops) * a->count);
    memcpy(self->ops + self->count + a->count, b->ops, sizeof(*self->ops) * b->count);
    self->count += a->count + b->count;

    destroy(a), a = NULL;
    destroy(b), b = NULL;

    return self;
}

eccoplist_t* joinDiscarded(eccoplist_t* self, uint16_t n, eccoplist_t* with)
{
    while(n > 16)
    {
        self = io_libecc_OpList.append(self, io_libecc_Op.make(io_libecc_Op.discardN, ECCNSValue.integer(16), ECC_ConstString_Empty));
        n -= 16;
    }

    if(n == 1)
        self = io_libecc_OpList.append(self, io_libecc_Op.make(io_libecc_Op.discard, ECCValConstUndefined, ECC_ConstString_Empty));
    else
        self = io_libecc_OpList.append(self, io_libecc_Op.make(io_libecc_Op.discardN, ECCNSValue.integer(n), ECC_ConstString_Empty));

    return join(self, with);
}

eccoplist_t* unshift(eccoperand_t op, eccoplist_t* self)
{
    if(!self)
        return create(op.native, op.value, op.text);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + 1));
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count++);
    self->ops[0] = op;
    return self;
}

eccoplist_t* unshiftJoin(eccoperand_t op, eccoplist_t* self, eccoplist_t* with)
{
    if(!self)
        return unshift(op, with);
    else if(!with)
        return unshift(op, self);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + with->count + 1));
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
    memcpy(self->ops + self->count + 1, with->ops, sizeof(*self->ops) * with->count);
    self->ops[0] = op;
    self->count += with->count + 1;

    destroy(with), with = NULL;

    return self;
}

eccoplist_t* unshiftJoin3(eccoperand_t op, eccoplist_t* self, eccoplist_t* a, eccoplist_t* b)
{
    if(!self)
        return unshiftJoin(op, a, b);
    else if(!a)
        return unshiftJoin(op, self, b);
    else if(!b)
        return unshiftJoin(op, self, a);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + a->count + b->count + 1));
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
    memcpy(self->ops + self->count + 1, a->ops, sizeof(*self->ops) * a->count);
    memcpy(self->ops + self->count + a->count + 1, b->ops, sizeof(*self->ops) * b->count);
    self->ops[0] = op;
    self->count += a->count + b->count + 1;

    destroy(a), a = NULL;
    destroy(b), b = NULL;

    return self;
}

eccoplist_t* shift(eccoplist_t* self)
{
    memmove(self->ops, self->ops + 1, sizeof(*self->ops) * --self->count);
    return self;
}

eccoplist_t* append(eccoplist_t* self, eccoperand_t op)
{
    if(!self)
        return create(op.native, op.value, op.text);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + 1));
    self->ops[self->count++] = op;
    return self;
}

eccoplist_t* appendNoop(eccoplist_t* self)
{
    return append(self, io_libecc_Op.make(io_libecc_Op.noop, ECCValConstUndefined, ECC_ConstString_Empty));
}

eccoplist_t* createLoop(eccoplist_t* initial, eccoplist_t* condition, eccoplist_t* step, eccoplist_t* body, int reverseCondition)
{
    if(condition && step && condition->count == 3 && !reverseCondition)
    {
        if(condition->ops[1].native == io_libecc_Op.getLocal && (condition->ops[0].native == io_libecc_Op.less || condition->ops[0].native == io_libecc_Op.lessOrEqual))
            if(step->count >= 2 && step->ops[1].value.data.key.data.integer == condition->ops[1].value.data.key.data.integer)
            {
                eccvalue_t stepValue;
                if(step->count == 2 && (step->ops[0].native == io_libecc_Op.incrementRef || step->ops[0].native == io_libecc_Op.postIncrementRef))
                    stepValue = ECCNSValue.integer(1);
                else if(step->count == 3 && step->ops[0].native == io_libecc_Op.addAssignRef && step->ops[2].native == io_libecc_Op.value
                        && step->ops[2].value.type == ECC_VALTYPE_INTEGER && step->ops[2].value.data.integer > 0)
                    stepValue = step->ops[2].value;
                else
                    goto normal;

                if(condition->ops[2].native == io_libecc_Op.getLocal)
                    body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[2].value, condition->ops[2].text), body);
                else if(condition->ops[2].native == io_libecc_Op.value)
                    body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.valueConstRef, condition->ops[2].value, condition->ops[2].text), body);
                else
                    goto normal;

                body = io_libecc_OpList.appendNoop(
                io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[1].value, condition->ops[1].text), body));
                body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.value, stepValue, condition->ops[0].text), body);
                body = io_libecc_OpList.unshift(io_libecc_Op.make(condition->ops[0].native == io_libecc_Op.less ? io_libecc_Op.iterateLessRef : io_libecc_Op.iterateLessOrEqualRef,
                                                                  ECCNSValue.integer(body->count), condition->ops[0].text),
                                                body);
                io_libecc_OpList.destroy(condition), condition = NULL;
                io_libecc_OpList.destroy(step), step = NULL;
                return io_libecc_OpList.join(initial, body);
            }

        if(condition->ops[1].native == io_libecc_Op.getLocal && (condition->ops[0].native == io_libecc_Op.more || condition->ops[0].native == io_libecc_Op.moreOrEqual))
            if(step->count >= 2 && step->ops[1].value.data.key.data.integer == condition->ops[1].value.data.key.data.integer)
            {
                eccvalue_t stepValue;
                if(step->count == 2 && (step->ops[0].native == io_libecc_Op.decrementRef || step->ops[0].native == io_libecc_Op.postDecrementRef))
                    stepValue = ECCNSValue.integer(1);
                else if(step->count == 3 && step->ops[0].native == io_libecc_Op.minusAssignRef && step->ops[2].native == io_libecc_Op.value
                        && step->ops[2].value.type == ECC_VALTYPE_INTEGER && step->ops[2].value.data.integer > 0)
                    stepValue = step->ops[2].value;
                else
                    goto normal;

                if(condition->ops[2].native == io_libecc_Op.getLocal)
                    body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[2].value, condition->ops[2].text), body);
                else if(condition->ops[2].native == io_libecc_Op.value)
                    body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.valueConstRef, condition->ops[2].value, condition->ops[2].text), body);
                else
                    goto normal;

                body = io_libecc_OpList.appendNoop(
                io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[1].value, condition->ops[1].text), body));
                body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.value, stepValue, condition->ops[0].text), body);
                body = io_libecc_OpList.unshift(io_libecc_Op.make(condition->ops[0].native == io_libecc_Op.more ? io_libecc_Op.iterateMoreRef : io_libecc_Op.iterateMoreOrEqualRef,
                                                                  ECCNSValue.integer(body->count), condition->ops[0].text),
                                                body);
                io_libecc_OpList.destroy(condition), condition = NULL;
                io_libecc_OpList.destroy(step), step = NULL;
                return io_libecc_OpList.join(initial, body);
            }
    }

normal:
{
    int skipOpCount;

    if(!condition)
        condition = io_libecc_OpList.create(io_libecc_Op.value, ECCNSValue.truth(1), ECC_ConstString_Empty);

    if(step)
    {
        step = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.discard, ECCValConstNone, ECC_ConstString_Empty), step);
        skipOpCount = step->count;
    }
    else
        skipOpCount = 0;

    body = io_libecc_OpList.appendNoop(body);

    if(reverseCondition)
    {
        skipOpCount += condition->count + body->count;
        body = io_libecc_OpList.append(body, io_libecc_Op.make(io_libecc_Op.value, ECCNSValue.truth(1), ECC_ConstString_Empty));
        body = io_libecc_OpList.append(body, io_libecc_Op.make(io_libecc_Op.jump, ECCNSValue.integer(-body->count - 1), ECC_ConstString_Empty));
    }

    body = io_libecc_OpList.join(io_libecc_OpList.join(step, condition), body);
    body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.jump, ECCNSValue.integer(body->count), ECC_ConstString_Empty), body);
    initial = io_libecc_OpList.append(initial, io_libecc_Op.make(io_libecc_Op.iterate, ECCNSValue.integer(skipOpCount), ECC_ConstString_Empty));
    return io_libecc_OpList.join(initial, body);
}
}

void optimizeWithEnvironment(eccoplist_t* self, eccobject_t* environment, uint32_t selfIndex)
{
    uint32_t index, count, slot, haveLocal = 0, environmentLevel = 0;
    eccindexkey_t environments[0xff];

    if(!self)
        return;

    for(index = 0, count = self->count; index < count; ++index)
    {
        if(self->ops[index].native == io_libecc_Op.with)
        {
            index += self->ops[index].value.data.integer;
            haveLocal = 1;
        }

        if(self->ops[index].native == io_libecc_Op.function)
        {
            uint32_t subselfidx = ((index && (self->ops[index - 1].native == io_libecc_Op.setLocalSlot)) ? self->ops[index - 1].value.data.integer : 0);
            optimizeWithEnvironment(self->ops[index].value.data.function->oplist, &self->ops[index].value.data.function->environment, subselfidx);
        }

        if(self->ops[index].native == io_libecc_Op.pushEnvironment)
            environments[environmentLevel++] = self->ops[index].value.data.key;

        if(self->ops[index].native == io_libecc_Op.popEnvironment)
            --environmentLevel;

        if(self->ops[index].native == io_libecc_Op.createLocalRef || self->ops[index].native == io_libecc_Op.getLocalRefOrNull
           || self->ops[index].native == io_libecc_Op.getLocalRef || self->ops[index].native == io_libecc_Op.getLocal
           || self->ops[index].native == io_libecc_Op.setLocal || self->ops[index].native == io_libecc_Op.deleteLocal)
        {
            eccobject_t* searchEnvironment = environment;
            uint32_t level;

            level = environmentLevel;
            while(level--)
                if(io_libecc_Key.isEqual(environments[level], self->ops[index].value.data.key))
                    goto notfound;

            level = environmentLevel;
            do
            {
                for(slot = searchEnvironment->hashmapCount; slot--;)
                {
                    if(searchEnvironment->hashmap[slot].value.check == 1)
                    {
                        if(io_libecc_Key.isEqual(searchEnvironment->hashmap[slot].value.key, self->ops[index].value.data.key))
                        {
                            if(!level)
                            {
                                self->ops[index] = io_libecc_Op.make(self->ops[index].native == io_libecc_Op.createLocalRef    ? io_libecc_Op.getLocalSlotRef :
                                                                     self->ops[index].native == io_libecc_Op.getLocalRefOrNull ? io_libecc_Op.getLocalSlotRef :
                                                                     self->ops[index].native == io_libecc_Op.getLocalRef       ? io_libecc_Op.getLocalSlotRef :
                                                                     self->ops[index].native == io_libecc_Op.getLocal          ? io_libecc_Op.getLocalSlot :
                                                                     self->ops[index].native == io_libecc_Op.setLocal          ? io_libecc_Op.setLocalSlot :
                                                                     self->ops[index].native == io_libecc_Op.deleteLocal       ? io_libecc_Op.deleteLocalSlot :
                                                                                                                                 NULL,
                                                                     ECCNSValue.integer(slot), self->ops[index].text);
                            }
                            else if(slot <= INT16_MAX && level <= INT16_MAX)
                            {
                                self->ops[index] = io_libecc_Op.make(self->ops[index].native == io_libecc_Op.createLocalRef    ? io_libecc_Op.getParentSlotRef :
                                                                     self->ops[index].native == io_libecc_Op.getLocalRefOrNull ? io_libecc_Op.getParentSlotRef :
                                                                     self->ops[index].native == io_libecc_Op.getLocalRef       ? io_libecc_Op.getParentSlotRef :
                                                                     self->ops[index].native == io_libecc_Op.getLocal          ? io_libecc_Op.getParentSlot :
                                                                     self->ops[index].native == io_libecc_Op.setLocal          ? io_libecc_Op.setParentSlot :
                                                                     self->ops[index].native == io_libecc_Op.deleteLocal       ? io_libecc_Op.deleteParentSlot :
                                                                                                                                 NULL,
                                                                     ECCNSValue.integer((level << 16) | slot), self->ops[index].text);
                            }
                            else
                                goto notfound;

                            if(index > 1 && level == 1 && slot == selfIndex)
                            {
                                eccoperand_t op = self->ops[index - 1];
                                if(op.native == io_libecc_Op.call && self->ops[index - 2].native == io_libecc_Op.result)
                                {
                                    self->ops[index - 1] = io_libecc_Op.make(io_libecc_Op.repopulate, op.value, op.text);
                                    self->ops[index] = io_libecc_Op.make(io_libecc_Op.value, ECCNSValue.integer(-index - 1), self->ops[index].text);
                                }
                            }

                            goto found;
                        }
                    }
                }

                ++level;
            } while((searchEnvironment = searchEnvironment->prototype));

        notfound:
            haveLocal = 1;
        found:;
        }
    }

    if(!haveLocal)
        ECCNSObject.stripMap(environment);
}

void dumpTo(eccoplist_t* self, FILE* file)
{
    uint32_t i;

    assert(self);

    fputc('\n', stderr);
    if(!self)
        return;

    for(i = 0; i < self->count; ++i)
    {
        char c = self->ops[i].text.flags & ECC_TEXTFLAG_BREAKFLAG ? i ? '!' : 'T' : '|';
        fprintf(file, "[%p] %c %s ", (void*)(self->ops + i), c, io_libecc_Op.toChars(self->ops[i].native));

        if(self->ops[i].native == io_libecc_Op.function)
        {
            fprintf(file, "{");
            io_libecc_OpList.dumpTo(self->ops[i].value.data.function->oplist, stderr);
            fprintf(file, "} ");
        }
        else if(self->ops[i].native == io_libecc_Op.getParentSlot || self->ops[i].native == io_libecc_Op.getParentSlotRef || self->ops[i].native == io_libecc_Op.setParentSlot)
            fprintf(file, "[-%hu] %hu", (uint16_t)(self->ops[i].value.data.integer >> 16), (uint16_t)(self->ops[i].value.data.integer & 0xffff));
        else if(self->ops[i].value.type != ECC_VALTYPE_UNDEFINED || self->ops[i].native == io_libecc_Op.value || self->ops[i].native == io_libecc_Op.exchange)
            ECCNSValue.dumpTo(self->ops[i].value, file);

        if(self->ops[i].native == io_libecc_Op.text)
            fprintf(file, "'%.*s'", (int)self->ops[i].text.length, self->ops[i].text.bytes);

        if(self->ops[i].text.length)
            fprintf(file, "  `%.*s`", (int)self->ops[i].text.length, self->ops[i].text.bytes);

        fputc('\n', stderr);
    }
}

ecctextstring_t text(eccoplist_t* oplist)
{
    uint16_t length;
    if(!oplist)
        return ECC_ConstString_Empty;

    length = oplist->ops[oplist->count - 1].text.bytes + oplist->ops[oplist->count - 1].text.length - oplist->ops[0].text.bytes;

    return ECCNSText.make(oplist->ops[0].text.bytes, oplist->ops[0].text.length > length ? oplist->ops[0].text.length : length);
}
