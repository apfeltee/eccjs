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
static eccoplist_t* oplistfn_create(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text);
static void oplistfn_destroy(eccoplist_t*);
static eccoplist_t* oplistfn_join(eccoplist_t*, eccoplist_t*);
static eccoplist_t* oplistfn_join3(eccoplist_t*, eccoplist_t*, eccoplist_t*);
static eccoplist_t* oplistfn_joinDiscarded(eccoplist_t*, uint16_t n, eccoplist_t*);
static eccoplist_t* oplistfn_unshift(eccoperand_t op, eccoplist_t*);
static eccoplist_t* oplistfn_unshiftJoin(eccoperand_t op, eccoplist_t*, eccoplist_t*);
static eccoplist_t* oplistfn_unshiftJoin3(eccoperand_t op, eccoplist_t*, eccoplist_t*, eccoplist_t*);
static eccoplist_t* oplistfn_shift(eccoplist_t*);
static eccoplist_t* oplistfn_append(eccoplist_t*, eccoperand_t op);
static eccoplist_t* oplistfn_appendNoop(eccoplist_t*);
static eccoplist_t* oplistfn_createLoop(eccoplist_t* initial, eccoplist_t* condition, eccoplist_t* step, eccoplist_t* body, int reverseCondition);
static void oplistfn_optimizeWithEnvironment(eccoplist_t*, eccobject_t* environment, uint32_t index);
static void oplistfn_dumpTo(eccoplist_t*, FILE* file);
static ecctextstring_t oplistfn_text(eccoplist_t* oplist);
const struct eccpseudonsoplist_t ECCNSOpList = {
    oplistfn_create,
    oplistfn_destroy,
    oplistfn_join,
    oplistfn_join3,
    oplistfn_joinDiscarded,
    oplistfn_unshift,
    oplistfn_unshiftJoin,
    oplistfn_unshiftJoin3,
    oplistfn_shift,
    oplistfn_append,
    oplistfn_appendNoop,
    oplistfn_createLoop,
    oplistfn_optimizeWithEnvironment,
    oplistfn_dumpTo,
    oplistfn_text,
    {}
};

eccoplist_t* oplistfn_create(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text)
{
    eccoplist_t* self = malloc(sizeof(*self));
    self->ops = malloc(sizeof(*self->ops) * 1);
    self->ops[0] = ECCNSOperand.make(native, value, text);
    self->count = 1;
    return self;
}

void oplistfn_destroy(eccoplist_t* self)
{
    assert(self);

    free(self->ops), self->ops = NULL;
    free(self), self = NULL;
}

eccoplist_t* oplistfn_join(eccoplist_t* self, eccoplist_t* with)
{
    if(!self)
        return with;
    else if(!with)
        return self;

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + with->count));
    memcpy(self->ops + self->count, with->ops, sizeof(*self->ops) * with->count);
    self->count += with->count;

    oplistfn_destroy(with), with = NULL;

    return self;
}

eccoplist_t* oplistfn_join3(eccoplist_t* self, eccoplist_t* a, eccoplist_t* b)
{
    if(!self)
        return oplistfn_join(a, b);
    else if(!a)
        return oplistfn_join(self, b);
    else if(!b)
        return oplistfn_join(self, a);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + a->count + b->count));
    memcpy(self->ops + self->count, a->ops, sizeof(*self->ops) * a->count);
    memcpy(self->ops + self->count + a->count, b->ops, sizeof(*self->ops) * b->count);
    self->count += a->count + b->count;

    oplistfn_destroy(a), a = NULL;
    oplistfn_destroy(b), b = NULL;

    return self;
}

eccoplist_t* oplistfn_joinDiscarded(eccoplist_t* self, uint16_t n, eccoplist_t* with)
{
    while(n > 16)
    {
        self = ECCNSOpList.append(self, ECCNSOperand.make(ECCNSOperand.discardN, ECCNSValue.integer(16), ECC_ConstString_Empty));
        n -= 16;
    }

    if(n == 1)
        self = ECCNSOpList.append(self, ECCNSOperand.make(ECCNSOperand.discard, ECCValConstUndefined, ECC_ConstString_Empty));
    else
        self = ECCNSOpList.append(self, ECCNSOperand.make(ECCNSOperand.discardN, ECCNSValue.integer(n), ECC_ConstString_Empty));

    return oplistfn_join(self, with);
}

eccoplist_t* oplistfn_unshift(eccoperand_t op, eccoplist_t* self)
{
    if(!self)
        return oplistfn_create(op.native, op.value, op.text);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + 1));
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count++);
    self->ops[0] = op;
    return self;
}

eccoplist_t* oplistfn_unshiftJoin(eccoperand_t op, eccoplist_t* self, eccoplist_t* with)
{
    if(!self)
        return oplistfn_unshift(op, with);
    else if(!with)
        return oplistfn_unshift(op, self);

    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + with->count + 1));
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
    memcpy(self->ops + self->count + 1, with->ops, sizeof(*self->ops) * with->count);
    self->ops[0] = op;
    self->count += with->count + 1;

    oplistfn_destroy(with), with = NULL;

    return self;
}

eccoplist_t* oplistfn_unshiftJoin3(eccoperand_t op, eccoplist_t* self, eccoplist_t* a, eccoplist_t* b)
{
    if(!self)
    {
        return oplistfn_unshiftJoin(op, a, b);
    }
    else if(!a)
    {
        return oplistfn_unshiftJoin(op, self, b);
    }
    else if(!b)
    {
        return oplistfn_unshiftJoin(op, self, a);
    }
    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + a->count + b->count + 1));
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
    memcpy(self->ops + self->count + 1, a->ops, sizeof(*self->ops) * a->count);
    memcpy(self->ops + self->count + a->count + 1, b->ops, sizeof(*self->ops) * b->count);
    self->ops[0] = op;
    self->count += a->count + b->count + 1;
    oplistfn_destroy(a);
    a = NULL;
    oplistfn_destroy(b);
    b = NULL;
    return self;
}

eccoplist_t* oplistfn_shift(eccoplist_t* self)
{
    memmove(self->ops, self->ops + 1, sizeof(*self->ops) * --self->count);
    return self;
}

eccoplist_t* oplistfn_append(eccoplist_t* self, eccoperand_t op)
{
    if(!self)
    {
        return oplistfn_create(op.native, op.value, op.text);
    }
    self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + 1));
    self->ops[self->count++] = op;
    return self;
}

eccoplist_t* oplistfn_appendNoop(eccoplist_t* self)
{
    return oplistfn_append(self, ECCNSOperand.make(ECCNSOperand.noop, ECCValConstUndefined, ECC_ConstString_Empty));
}

eccoplist_t* oplistfn_createLoop(eccoplist_t* initial, eccoplist_t* condition, eccoplist_t* step, eccoplist_t* body, int reverseCondition)
{
    if(condition && step && condition->count == 3 && !reverseCondition)
    {
        if(condition->ops[1].native == ECCNSOperand.getLocal && (condition->ops[0].native == ECCNSOperand.less || condition->ops[0].native == ECCNSOperand.lessOrEqual))
            if(step->count >= 2 && step->ops[1].value.data.key.data.integer == condition->ops[1].value.data.key.data.integer)
            {
                eccvalue_t stepValue;
                if(step->count == 2 && (step->ops[0].native == ECCNSOperand.incrementRef || step->ops[0].native == ECCNSOperand.postIncrementRef))
                {
                    stepValue = ECCNSValue.integer(1);
                }
                else if(step->count == 3 && step->ops[0].native == ECCNSOperand.addAssignRef && step->ops[2].native == ECCNSOperand.value
                        && step->ops[2].value.type == ECC_VALTYPE_INTEGER && step->ops[2].value.data.integer > 0)
                {
                    stepValue = step->ops[2].value;
                }
                else
                {
                    goto normal;
                }
                if(condition->ops[2].native == ECCNSOperand.getLocal)
                {
                    body = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.getLocalRef, condition->ops[2].value, condition->ops[2].text), body);
                }
                else if(condition->ops[2].native == ECCNSOperand.value)
                {
                    body = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.valueConstRef, condition->ops[2].value, condition->ops[2].text), body);
                }
                else
                {
                    goto normal;
                }
                body = ECCNSOpList.appendNoop(
                    ECCNSOpList.unshift(
                        ECCNSOperand.make(ECCNSOperand.getLocalRef, condition->ops[1].value, condition->ops[1].text),
                        body
                    )
                );
                body = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.value, stepValue, condition->ops[0].text), body);
                body = ECCNSOpList.unshift(
                    ECCNSOperand.make(
                        (condition->ops[0].native == ECCNSOperand.less ? ECCNSOperand.iterateLessRef : ECCNSOperand.iterateLessOrEqualRef),
                        ECCNSValue.integer(body->count),
                        condition->ops[0].text
                    ), body
                );
                ECCNSOpList.destroy(condition), condition = NULL;
                ECCNSOpList.destroy(step), step = NULL;
                return ECCNSOpList.join(initial, body);
            }

        if(condition->ops[1].native == ECCNSOperand.getLocal && (condition->ops[0].native == ECCNSOperand.more || condition->ops[0].native == ECCNSOperand.moreOrEqual))
            if(step->count >= 2 && step->ops[1].value.data.key.data.integer == condition->ops[1].value.data.key.data.integer)
            {
                eccvalue_t stepValue;
                if(step->count == 2 && (step->ops[0].native == ECCNSOperand.decrementRef || step->ops[0].native == ECCNSOperand.postDecrementRef))
                {
                    stepValue = ECCNSValue.integer(1);
                }
                else if(step->count == 3 && step->ops[0].native == ECCNSOperand.minusAssignRef && step->ops[2].native == ECCNSOperand.value
                        && step->ops[2].value.type == ECC_VALTYPE_INTEGER && step->ops[2].value.data.integer > 0)
                {
                    stepValue = step->ops[2].value;
                }
                else
                {
                    goto normal;
                }
                if(condition->ops[2].native == ECCNSOperand.getLocal)
                {
                    body = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.getLocalRef, condition->ops[2].value, condition->ops[2].text), body);
                }
                else if(condition->ops[2].native == ECCNSOperand.value)
                {
                    body = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.valueConstRef, condition->ops[2].value, condition->ops[2].text), body);
                }
                else
                {
                    goto normal;
                }
                body = ECCNSOpList.appendNoop(
                ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.getLocalRef, condition->ops[1].value, condition->ops[1].text), body));
                body = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.value, stepValue, condition->ops[0].text), body);
                body = ECCNSOpList.unshift(ECCNSOperand.make(condition->ops[0].native == ECCNSOperand.more ? ECCNSOperand.iterateMoreRef : ECCNSOperand.iterateMoreOrEqualRef,
                                                                  ECCNSValue.integer(body->count), condition->ops[0].text),
                                                body);
                ECCNSOpList.destroy(condition), condition = NULL;
                ECCNSOpList.destroy(step), step = NULL;
                return ECCNSOpList.join(initial, body);
            }
    }
    normal:
    {
        int skipOpCount;

        if(!condition)
        {
            condition = ECCNSOpList.create(ECCNSOperand.value, ECCNSValue.truth(1), ECC_ConstString_Empty);
        }
        if(step)
        {
            step = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.discard, ECCValConstNone, ECC_ConstString_Empty), step);
            skipOpCount = step->count;
        }
        else
        {
            skipOpCount = 0;
        }
        body = ECCNSOpList.appendNoop(body);
        if(reverseCondition)
        {
            skipOpCount += condition->count + body->count;
            body = ECCNSOpList.append(body, ECCNSOperand.make(ECCNSOperand.value, ECCNSValue.truth(1), ECC_ConstString_Empty));
            body = ECCNSOpList.append(body, ECCNSOperand.make(ECCNSOperand.jump, ECCNSValue.integer(-body->count - 1), ECC_ConstString_Empty));
        }
        body = ECCNSOpList.join(ECCNSOpList.join(step, condition), body);
        body = ECCNSOpList.unshift(ECCNSOperand.make(ECCNSOperand.jump, ECCNSValue.integer(body->count), ECC_ConstString_Empty), body);
        initial = ECCNSOpList.append(initial, ECCNSOperand.make(ECCNSOperand.iterate, ECCNSValue.integer(skipOpCount), ECC_ConstString_Empty));
        return ECCNSOpList.join(initial, body);
    }
}

void oplistfn_optimizeWithEnvironment(eccoplist_t* self, eccobject_t* environment, uint32_t selfIndex)
{
    bool cansearch;
    uint32_t index;
    uint32_t count;
    uint32_t slot;
    uint32_t haveLocal = 0;
    uint32_t envlevel = 0;
    eccindexkey_t environments[0xff];
    if(!self)
    {
        return;
    }
    for(index = 0, count = self->count; index < count; ++index)
    {
        if(self->ops[index].native == ECCNSOperand.with)
        {
            index += self->ops[index].value.data.integer;
            haveLocal = 1;
        }
        if(self->ops[index].native == ECCNSOperand.function)
        {
            uint32_t subselfidx = ((index && (self->ops[index - 1].native == ECCNSOperand.setLocalSlot)) ? self->ops[index - 1].value.data.integer : 0);
            oplistfn_optimizeWithEnvironment(self->ops[index].value.data.function->oplist, &self->ops[index].value.data.function->environment, subselfidx);
        }
        if(self->ops[index].native == ECCNSOperand.pushEnvironment)
        {
            environments[envlevel++] = self->ops[index].value.data.key;
        }
        if(self->ops[index].native == ECCNSOperand.popEnvironment)
        {
            --envlevel;
        }
        cansearch = (
            (self->ops[index].native == ECCNSOperand.createLocalRef) ||
            (self->ops[index].native == ECCNSOperand.getLocalRefOrNull) ||
            (self->ops[index].native == ECCNSOperand.getLocalRef) ||
            (self->ops[index].native == ECCNSOperand.getLocal) ||
            (self->ops[index].native == ECCNSOperand.setLocal) ||
            (self->ops[index].native == ECCNSOperand.deleteLocal)
        );
        if(cansearch)
        {
            eccobject_t* searchenv = environment;
            uint32_t level;
            level = envlevel;
            while(level--)
            {
                if(ECCNSKey.isEqual(environments[level], self->ops[index].value.data.key))
                {
                    goto notfound;
                }
            }
            level = envlevel;
            do
            {
                for(slot = searchenv->hashmapCount; slot--;)
                {
                    if(searchenv->hashmap[slot].value.check == 1)
                    {
                        if(ECCNSKey.isEqual(searchenv->hashmap[slot].value.key, self->ops[index].value.data.key))
                        {
                            if(!level)
                            {
                                self->ops[index] = ECCNSOperand.make(
                                    self->ops[index].native == ECCNSOperand.createLocalRef    ? ECCNSOperand.getLocalSlotRef :
                                    self->ops[index].native == ECCNSOperand.getLocalRefOrNull ? ECCNSOperand.getLocalSlotRef :
                                    self->ops[index].native == ECCNSOperand.getLocalRef       ? ECCNSOperand.getLocalSlotRef :
                                    self->ops[index].native == ECCNSOperand.getLocal          ? ECCNSOperand.getLocalSlot :
                                    self->ops[index].native == ECCNSOperand.setLocal          ? ECCNSOperand.setLocalSlot :
                                    self->ops[index].native == ECCNSOperand.deleteLocal       ? ECCNSOperand.deleteLocalSlot :
                                    NULL
                                    ,
                                    ECCNSValue.integer(slot), self->ops[index].text);
                            }
                            else if(slot <= INT16_MAX && level <= INT16_MAX)
                            {
                                self->ops[index] = ECCNSOperand.make(
                                    self->ops[index].native == ECCNSOperand.createLocalRef    ? ECCNSOperand.getParentSlotRef :
                                    self->ops[index].native == ECCNSOperand.getLocalRefOrNull ? ECCNSOperand.getParentSlotRef :
                                    self->ops[index].native == ECCNSOperand.getLocalRef       ? ECCNSOperand.getParentSlotRef :
                                    self->ops[index].native == ECCNSOperand.getLocal          ? ECCNSOperand.getParentSlot :
                                    self->ops[index].native == ECCNSOperand.setLocal          ? ECCNSOperand.setParentSlot :
                                    self->ops[index].native == ECCNSOperand.deleteLocal       ? ECCNSOperand.deleteParentSlot :
                                    NULL
                                    ,
                                    ECCNSValue.integer((level << 16) | slot), self->ops[index].text);
                            }
                            else
                            {
                                goto notfound;
                            }
                            if(index > 1 && level == 1 && slot == selfIndex)
                            {
                                eccoperand_t op = self->ops[index - 1];
                                if(op.native == ECCNSOperand.call && self->ops[index - 2].native == ECCNSOperand.result)
                                {
                                    self->ops[index - 1] = ECCNSOperand.make(ECCNSOperand.repopulate, op.value, op.text);
                                    self->ops[index] = ECCNSOperand.make(ECCNSOperand.value, ECCNSValue.integer(-index - 1), self->ops[index].text);
                                }
                            }
                            goto found;
                        }
                    }
                }
                ++level;
            } while((searchenv = searchenv->prototype));
        notfound:
            {
                haveLocal = 1;
            }
        found:
            {
            }
        }
    }

    if(!haveLocal)
        ECCNSObject.stripMap(environment);
}

void oplistfn_dumpTo(eccoplist_t* self, FILE* file)
{
    uint32_t i;

    assert(self);

    fputc('\n', stderr);
    if(!self)
        return;

    for(i = 0; i < self->count; ++i)
    {
        char c = self->ops[i].text.flags & ECC_TEXTFLAG_BREAKFLAG ? i ? '!' : 'T' : '|';
        fprintf(file, "[%p] %c %s ", (void*)(self->ops + i), c, ECCNSOperand.toChars(self->ops[i].native));

        if(self->ops[i].native == ECCNSOperand.function)
        {
            fprintf(file, "{");
            ECCNSOpList.dumpTo(self->ops[i].value.data.function->oplist, stderr);
            fprintf(file, "} ");
        }
        else if(self->ops[i].native == ECCNSOperand.getParentSlot || self->ops[i].native == ECCNSOperand.getParentSlotRef || self->ops[i].native == ECCNSOperand.setParentSlot)
            fprintf(file, "[-%hu] %hu", (uint16_t)(self->ops[i].value.data.integer >> 16), (uint16_t)(self->ops[i].value.data.integer & 0xffff));
        else if(self->ops[i].value.type != ECC_VALTYPE_UNDEFINED || self->ops[i].native == ECCNSOperand.value || self->ops[i].native == ECCNSOperand.exchange)
            ECCNSValue.dumpTo(self->ops[i].value, file);

        if(self->ops[i].native == ECCNSOperand.text)
            fprintf(file, "'%.*s'", (int)self->ops[i].text.length, self->ops[i].text.bytes);

        if(self->ops[i].text.length)
            fprintf(file, "  `%.*s`", (int)self->ops[i].text.length, self->ops[i].text.bytes);

        fputc('\n', stderr);
    }
}

ecctextstring_t oplistfn_text(eccoplist_t* oplist)
{
    uint16_t length;
    if(!oplist)
        return ECC_ConstString_Empty;

    length = oplist->ops[oplist->count - 1].text.bytes + oplist->ops[oplist->count - 1].text.length - oplist->ops[0].text.bytes;

    return ECCNSText.make(oplist->ops[0].text.bytes, oplist->ops[0].text.length > length ? oplist->ops[0].text.length : length);
}
