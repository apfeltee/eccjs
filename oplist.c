
/*
//  oplist.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

eccoplist_t* ecc_oplist_create(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text)
{
    eccoplist_t* self = (eccoplist_t*)malloc(sizeof(*self));
    self->ops = (eccoperand_t*)malloc(sizeof(*self->ops) * 1);
    self->ops[0] = ecc_oper_make(native, value, text);
    self->count = 1;
    return self;
}

void ecc_oplist_destroy(eccoplist_t* self)
{
    assert(self);
    free(self->ops), self->ops = NULL;
    free(self), self = NULL;
}

eccoplist_t* ecc_oplist_join(eccoplist_t* self, eccoplist_t* with)
{
    size_t needed;
    eccoperand_t* tmp;
    if(!self)
    {
        return with;
    }
    else if(!with)
    {
        return self;
    }
    needed = sizeof(*self->ops) * (self->count + with->count);
    tmp = (eccoperand_t*)realloc(self->ops, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in join: failed to reallocate for %ld bytes\n", needed);
    }
    self->ops = tmp;
    memcpy(self->ops + self->count, with->ops, sizeof(*self->ops) * with->count);
    self->count += with->count;
    ecc_oplist_destroy(with);
    with = NULL;
    return self;
}

eccoplist_t* ecc_oplist_join3(eccoplist_t* self, eccoplist_t* a, eccoplist_t* b)
{
    size_t needed;
    eccoperand_t* tmp;
    if(!self)
        return ecc_oplist_join(a, b);
    else if(!a)
        return ecc_oplist_join(self, b);
    else if(!b)
        return ecc_oplist_join(self, a);
    needed = (sizeof(*self->ops) * (self->count + a->count + b->count));
    tmp = (eccoperand_t*)realloc(self->ops, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in join3: failed to reallocate for %ld bytes\n", needed);
    }
    self->ops = tmp;
    memcpy(self->ops + self->count, a->ops, sizeof(*self->ops) * a->count);
    memcpy(self->ops + self->count + a->count, b->ops, sizeof(*self->ops) * b->count);
    self->count += a->count + b->count;
    ecc_oplist_destroy(a), a = NULL;
    ecc_oplist_destroy(b), b = NULL;
    return self;
}

eccoplist_t* ecc_oplist_joindiscarded(eccoplist_t* self, uint32_t n, eccoplist_t* with)
{
    while(n > 16)
    {
        self = ecc_oplist_append(self, ecc_oper_make(ecc_oper_discardn, ecc_value_fromint(16), ECC_String_Empty));
        n -= 16;
    }

    if(n == 1)
        self = ecc_oplist_append(self, ecc_oper_make(ecc_oper_discard, ECCValConstUndefined, ECC_String_Empty));
    else
        self = ecc_oplist_append(self, ecc_oper_make(ecc_oper_discardn, ecc_value_fromint(n), ECC_String_Empty));

    return ecc_oplist_join(self, with);
}

eccoplist_t* ecc_oplist_unshift(eccoperand_t op, eccoplist_t* self)
{
    size_t needed;
    eccoperand_t* tmp;
    if(!self)
        return ecc_oplist_create(op.native, op.opvalue, op.text);
    needed = (sizeof(*self->ops) * (self->count + 1));
    tmp = (eccoperand_t*)realloc(self->ops, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in unshift: failed to reallocate for %ld bytes\n", needed);
    }
    self->ops = tmp;
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count++);
    self->ops[0] = op;
    return self;
}

eccoplist_t* ecc_oplist_unshiftjoin(eccoperand_t op, eccoplist_t* self, eccoplist_t* with)
{
    size_t needed;
    eccoperand_t* tmp;
    if(!self)
    {
        return ecc_oplist_unshift(op, with);
    }
    else if(!with)
    {
        return ecc_oplist_unshift(op, self);
    }
    needed = (sizeof(*self->ops) * (self->count + with->count + 1));
    tmp = (eccoperand_t*)realloc(self->ops, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in unshiftjoin: failed to reallocate for %ld bytes\n", needed);
    }
    self->ops = tmp;
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
    memcpy(self->ops + self->count + 1, with->ops, sizeof(*self->ops) * with->count);
    self->ops[0] = op;
    self->count += with->count + 1;
    ecc_oplist_destroy(with);
    with = NULL;
    return self;
}

eccoplist_t* ecc_oplist_unshiftjoin3(eccoperand_t op, eccoplist_t* self, eccoplist_t* a, eccoplist_t* b)
{
    size_t needed;
    eccoperand_t* tmp;
    if(!self)
    {
        return ecc_oplist_unshiftjoin(op, a, b);
    }
    else if(!a)
    {
        return ecc_oplist_unshiftjoin(op, self, b);
    }
    else if(!b)
    {
        return ecc_oplist_unshiftjoin(op, self, a);
    }
    needed = (sizeof(*self->ops) * (self->count + a->count + b->count + 1));
    tmp = (eccoperand_t*)realloc(self->ops, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in unshiftjoin3: failed to reallocate for %ld bytes\n", needed);
    }
    self->ops = tmp;
    memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
    memcpy(self->ops + self->count + 1, a->ops, sizeof(*self->ops) * a->count);
    memcpy(self->ops + self->count + a->count + 1, b->ops, sizeof(*self->ops) * b->count);
    self->ops[0] = op;
    self->count += a->count + b->count + 1;
    ecc_oplist_destroy(a);
    a = NULL;
    ecc_oplist_destroy(b);
    b = NULL;
    return self;
}

eccoplist_t* ecc_oplist_shift(eccoplist_t* self)
{
    memmove(self->ops, self->ops + 1, sizeof(*self->ops) * --self->count);
    return self;
}

eccoplist_t* ecc_oplist_append(eccoplist_t* self, eccoperand_t op)
{
    size_t needed;
    eccoperand_t* tmp;
    if(!self)
    {
        return ecc_oplist_create(op.native, op.opvalue, op.text);
    }
    needed = (sizeof(*self->ops) * (self->count + 1));
    tmp = (eccoperand_t*)realloc(self->ops, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in append: failed to reallocate for %ld bytes\n", needed);
    }
    self->ops = tmp;
    self->ops[self->count++] = op;
    return self;
}

eccoplist_t* ecc_oplist_appendnoop(eccoplist_t* self)
{
    return ecc_oplist_append(self, ecc_oper_make(ecc_oper_noop, ECCValConstUndefined, ECC_String_Empty));
}

eccoplist_t* ecc_oplist_createloop(eccoplist_t* initial, eccoplist_t* condition, eccoplist_t* step, eccoplist_t* body, int reverseCondition)
{
    if(condition && step && condition->count == 3 && !reverseCondition)
    {
        if(condition->ops[1].native == ecc_oper_getlocal && (condition->ops[0].native == ecc_oper_less || condition->ops[0].native == ecc_oper_lessorequal))
            if(step->count >= 2 && step->ops[1].opvalue.data.key.data.integer == condition->ops[1].opvalue.data.key.data.integer)
            {
                eccvalue_t stepValue;
                if(step->count == 2 && (step->ops[0].native == ecc_oper_incrementref || step->ops[0].native == ecc_oper_postincrementref))
                {
                    stepValue = ecc_value_fromint(1);
                }
                else if(step->count == 3 && step->ops[0].native == ecc_oper_addassignref && step->ops[2].native == ecc_oper_value
                        && step->ops[2].opvalue.type == ECC_VALTYPE_INTEGER && step->ops[2].opvalue.data.integer > 0)
                {
                    stepValue = step->ops[2].opvalue;
                }
                else
                {
                    goto normal;
                }
                if(condition->ops[2].native == ecc_oper_getlocal)
                {
                    body = ecc_oplist_unshift(ecc_oper_make(ecc_oper_getlocalref, condition->ops[2].opvalue, condition->ops[2].text), body);
                }
                else if(condition->ops[2].native == ecc_oper_value)
                {
                    body = ecc_oplist_unshift(ecc_oper_make(ecc_oper_valueconstref, condition->ops[2].opvalue, condition->ops[2].text), body);
                }
                else
                {
                    goto normal;
                }
                body = ecc_oplist_appendnoop(
                    ecc_oplist_unshift(
                        ecc_oper_make(ecc_oper_getlocalref, condition->ops[1].opvalue, condition->ops[1].text),
                        body
                    )
                );
                body = ecc_oplist_unshift(ecc_oper_make(ecc_oper_value, stepValue, condition->ops[0].text), body);
                body = ecc_oplist_unshift(
                    ecc_oper_make(
                        (condition->ops[0].native == ecc_oper_less ? ecc_oper_iteratelessref : ecc_oper_iteratelessorequalref),
                        ecc_value_fromint(body->count),
                        condition->ops[0].text
                    ), body
                );
                ecc_oplist_destroy(condition), condition = NULL;
                ecc_oplist_destroy(step), step = NULL;
                return ecc_oplist_join(initial, body);
            }

        if(condition->ops[1].native == ecc_oper_getlocal && (condition->ops[0].native == ecc_oper_more || condition->ops[0].native == ecc_oper_moreorequal))
            if(step->count >= 2 && step->ops[1].opvalue.data.key.data.integer == condition->ops[1].opvalue.data.key.data.integer)
            {
                eccvalue_t stepValue;
                if(step->count == 2 && (step->ops[0].native == ecc_oper_decrementref || step->ops[0].native == ecc_oper_postdecrementref))
                {
                    stepValue = ecc_value_fromint(1);
                }
                else if(step->count == 3 && step->ops[0].native == ecc_oper_minusassignref && step->ops[2].native == ecc_oper_value
                        && step->ops[2].opvalue.type == ECC_VALTYPE_INTEGER && step->ops[2].opvalue.data.integer > 0)
                {
                    stepValue = step->ops[2].opvalue;
                }
                else
                {
                    goto normal;
                }
                if(condition->ops[2].native == ecc_oper_getlocal)
                {
                    body = ecc_oplist_unshift(ecc_oper_make(ecc_oper_getlocalref, condition->ops[2].opvalue, condition->ops[2].text), body);
                }
                else if(condition->ops[2].native == ecc_oper_value)
                {
                    body = ecc_oplist_unshift(ecc_oper_make(ecc_oper_valueconstref, condition->ops[2].opvalue, condition->ops[2].text), body);
                }
                else
                {
                    goto normal;
                }
                body = ecc_oplist_appendnoop(
                ecc_oplist_unshift(ecc_oper_make(ecc_oper_getlocalref, condition->ops[1].opvalue, condition->ops[1].text), body));
                body = ecc_oplist_unshift(ecc_oper_make(ecc_oper_value, stepValue, condition->ops[0].text), body);
                body = ecc_oplist_unshift(ecc_oper_make(condition->ops[0].native == ecc_oper_more ? ecc_oper_iteratemoreref : ecc_oper_iteratemoreorequalref,
                                                                  ecc_value_fromint(body->count), condition->ops[0].text),
                                                body);
                ecc_oplist_destroy(condition), condition = NULL;
                ecc_oplist_destroy(step), step = NULL;
                return ecc_oplist_join(initial, body);
            }
    }
    normal:
    {
        int skipOpCount;

        if(!condition)
        {
            condition = ecc_oplist_create(ecc_oper_value, ecc_value_truth(1), ECC_String_Empty);
        }
        if(step)
        {
            step = ecc_oplist_unshift(ecc_oper_make(ecc_oper_discard, ECCValConstNone, ECC_String_Empty), step);
            skipOpCount = step->count;
        }
        else
        {
            skipOpCount = 0;
        }
        body = ecc_oplist_appendnoop(body);
        if(reverseCondition)
        {
            skipOpCount += condition->count + body->count;
            body = ecc_oplist_append(body, ecc_oper_make(ecc_oper_value, ecc_value_truth(1), ECC_String_Empty));
            body = ecc_oplist_append(body, ecc_oper_make(ecc_oper_jump, ecc_value_fromint(-body->count - 1), ECC_String_Empty));
        }
        body = ecc_oplist_join(ecc_oplist_join(step, condition), body);
        body = ecc_oplist_unshift(ecc_oper_make(ecc_oper_jump, ecc_value_fromint(body->count), ECC_String_Empty), body);
        initial = ecc_oplist_append(initial, ecc_oper_make(ecc_oper_iterate, ecc_value_fromint(skipOpCount), ECC_String_Empty));
        return ecc_oplist_join(initial, body);
    }
}

void ecc_oplist_optimizewithenvironment(eccoplist_t* self, eccobject_t* environment, uint32_t selfIndex)
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
        if(self->ops[index].native == ecc_oper_with)
        {
            index += self->ops[index].opvalue.data.integer;
            haveLocal = 1;
        }
        if(self->ops[index].native == ecc_oper_function)
        {
            uint32_t subselfidx = ((index && (self->ops[index - 1].native == ecc_oper_setlocalslot)) ? self->ops[index - 1].opvalue.data.integer : 0);
            ecc_oplist_optimizewithenvironment(self->ops[index].opvalue.data.function->oplist, &self->ops[index].opvalue.data.function->funcenv, subselfidx);
        }
        if(self->ops[index].native == ecc_oper_pushenvironment)
        {
            environments[envlevel++] = self->ops[index].opvalue.data.key;
        }
        if(self->ops[index].native == ecc_oper_popenvironment)
        {
            --envlevel;
        }
        cansearch = (
            (self->ops[index].native == ecc_oper_createlocalref) ||
            (self->ops[index].native == ecc_oper_getlocalrefornull) ||
            (self->ops[index].native == ecc_oper_getlocalref) ||
            (self->ops[index].native == ecc_oper_getlocal) ||
            (self->ops[index].native == ecc_oper_setlocal) ||
            (self->ops[index].native == ecc_oper_deletelocal)
        );
        if(cansearch)
        {
            eccobject_t* searchenv = environment;
            uint32_t level;
            level = envlevel;
            while(level--)
            {
                if(ecc_keyidx_isequal(environments[level], self->ops[index].opvalue.data.key))
                {
                    goto notfound;
                }
            }
            level = envlevel;
            do
            {
                for(slot = searchenv->hmapmapcount; slot--;)
                {
                    if(searchenv->hmapmapitems[slot].hmapmapvalue.check == 1)
                    {
                        if(ecc_keyidx_isequal(searchenv->hmapmapitems[slot].hmapmapvalue.key, self->ops[index].opvalue.data.key))
                        {
                            if(!level)
                            {
                                self->ops[index] = ecc_oper_make(
                                    self->ops[index].native == ecc_oper_createlocalref    ? ecc_oper_getlocalslotref :
                                    self->ops[index].native == ecc_oper_getlocalrefornull ? ecc_oper_getlocalslotref :
                                    self->ops[index].native == ecc_oper_getlocalref       ? ecc_oper_getlocalslotref :
                                    self->ops[index].native == ecc_oper_getlocal          ? ecc_oper_getlocalslot :
                                    self->ops[index].native == ecc_oper_setlocal          ? ecc_oper_setlocalslot :
                                    self->ops[index].native == ecc_oper_deletelocal       ? ecc_oper_deletelocalslot :
                                    NULL
                                    ,
                                    ecc_value_fromint(slot), self->ops[index].text);
                            }
                            else if(slot <= INT16_MAX && level <= INT16_MAX)
                            {
                                self->ops[index] = ecc_oper_make(
                                    self->ops[index].native == ecc_oper_createlocalref    ? ecc_oper_getparentslotref :
                                    self->ops[index].native == ecc_oper_getlocalrefornull ? ecc_oper_getparentslotref :
                                    self->ops[index].native == ecc_oper_getlocalref       ? ecc_oper_getparentslotref :
                                    self->ops[index].native == ecc_oper_getlocal          ? ecc_oper_getparentslot :
                                    self->ops[index].native == ecc_oper_setlocal          ? ecc_oper_setparentslot :
                                    self->ops[index].native == ecc_oper_deletelocal       ? ecc_oper_deleteparentslot :
                                    NULL
                                    ,
                                    ecc_value_fromint((level << 16) | slot), self->ops[index].text);
                            }
                            else
                            {
                                goto notfound;
                            }
                            if(index > 1 && level == 1 && slot == selfIndex)
                            {
                                eccoperand_t op = self->ops[index - 1];
                                if(op.native == ecc_oper_call && self->ops[index - 2].native == ecc_oper_result)
                                {
                                    self->ops[index - 1] = ecc_oper_make(ecc_oper_repopulate, op.opvalue, op.text);
                                    self->ops[index] = ecc_oper_make(ecc_oper_value, ecc_value_fromint(-index - 1), self->ops[index].text);
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
        ecc_object_stripmap(environment);
}

void ecc_oplist_dumpto(eccoplist_t* self, FILE* file)
{
    uint32_t i;

    assert(self);

    fputc('\n', stderr);
    if(!self)
        return;

    for(i = 0; i < self->count; ++i)
    {
        char c = self->ops[i].text.flags & ECC_TEXTFLAG_BREAKFLAG ? i ? '!' : 'T' : '|';
        fprintf(file, "[%p] %c %s ", (void*)(self->ops + i), c, ecc_oper_tochars(self->ops[i].native));

        if(self->ops[i].native == ecc_oper_function)
        {
            fprintf(file, "{");
            ecc_oplist_dumpto(self->ops[i].opvalue.data.function->oplist, stderr);
            fprintf(file, "} ");
        }
        else if(self->ops[i].native == ecc_oper_getparentslot || self->ops[i].native == ecc_oper_getparentslotref || self->ops[i].native == ecc_oper_setparentslot)
            fprintf(file, "[-%hu] %hu", (uint32_t)(self->ops[i].opvalue.data.integer >> 16), (uint32_t)(self->ops[i].opvalue.data.integer & 0xffff));
        else if(self->ops[i].opvalue.type != ECC_VALTYPE_UNDEFINED || self->ops[i].native == ecc_oper_value || self->ops[i].native == ecc_oper_exchange)
            ecc_value_dumpto(self->ops[i].opvalue, file);

        if(self->ops[i].native == ecc_oper_text)
            fprintf(file, "'%.*s'", (int)self->ops[i].text.length, self->ops[i].text.bytes);

        if(self->ops[i].text.length)
            fprintf(file, "  `%.*s`", (int)self->ops[i].text.length, self->ops[i].text.bytes);

        fputc('\n', stderr);
    }
}

ecctextstring_t ecc_oplist_text(eccoplist_t* oplist)
{
    uint32_t length;
    if(!oplist)
        return ECC_String_Empty;

    length = oplist->ops[oplist->count - 1].text.bytes + oplist->ops[oplist->count - 1].text.length - oplist->ops[0].text.bytes;

    return ecc_textbuf_make(oplist->ops[0].text.bytes, oplist->ops[0].text.length > length ? oplist->ops[0].text.length : length);
}
