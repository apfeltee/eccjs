
/*
//  object.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

eccobject_t* ECC_Prototype_Object = NULL;
eccobjfunction_t* ECC_CtorFunc_Object = NULL;

const eccobjinterntype_t ECC_Type_Object = {
    .text = &ECC_String_ObjectType,
};

uint32_t ecc_object_getslot(const eccobject_t* const self, const eccindexkey_t key)
{
    return self->hmapmapitems[self->hmapmapitems[self->hmapmapitems[self->hmapmapitems[1].slot[key.data.depth[0]]].slot[key.data.depth[1]]].slot[key.data.depth[2]]].slot[key.data.depth[3]];
}

uint32_t ecc_object_getindexorkey(eccvalue_t property, eccindexkey_t* key)
{
    bool isbin;
    bool issame;
    bool abovezero;
    bool belomax;
    uint32_t index;
    index = UINT32_MAX;
    assert(ecc_value_isprimitive(property));
    if(property.type == ECC_VALTYPE_KEY)
    {
        *key = property.data.key;
    }
    else
    {
        if(property.type == ECC_VALTYPE_INTEGER && property.data.integer >= 0)
        {
            index = property.data.integer;
        }
        else
        {
            abovezero = false;
            belomax = false;
            issame = false;
            isbin = (property.type == ECC_VALTYPE_BINARY);
            if(isbin)
            {
                abovezero = (property.data.valnumfloat >= 0);
                belomax = (property.data.valnumfloat < UINT32_MAX);
                /*
                * NOTE: this *will* break when using '-march=native' and '-ffast-math' mode.
                * it emits illegal instructions. but i have no clue why.
                */
                issame = (((float)property.data.valnumfloat) == ((uint32_t)property.data.valnumfloat));
            }
            if(isbin && abovezero && belomax && issame)
            {
                index = property.data.valnumfloat;
            }
            else if(ecc_value_isstring(property))
            {
                eccstrbox_t text = ecc_value_textof(&property);
                if((index = ecc_astlex_scanelement(text)) == UINT32_MAX)
                    *key = ecc_keyidx_makewithtext(text, ECC_INDEXFLAG_COPYONCREATE);
            }
            else
                return ecc_object_getindexorkey(ecc_value_tostring(NULL, property), key);
        }
    }

    return index;
}

eccindexkey_t ecc_object_keyofindex(uint32_t index, int create)
{
    char buffer[10 + 1];
    uint32_t length;

    length = snprintf(buffer, sizeof(buffer), "%u", (unsigned)index);
    if(create)
        return ecc_keyidx_makewithtext(ecc_strbox_make(buffer, length), ECC_INDEXFLAG_COPYONCREATE);
    else
        return ecc_keyidx_search(ecc_strbox_make(buffer, length));
}

uint32_t ecc_object_nextpoweroftwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

uint32_t ecc_object_elementcount(eccobject_t* self)
{
    if(self->hmapitemcount < ECC_CONF_MAXELEMENTS)
        return self->hmapitemcount;
    else
        return ECC_CONF_MAXELEMENTS;
}

void ecc_object_readonlyerror(ecccontext_t* context, eccvalue_t* ref, eccobject_t* thisobj)
{
    eccstrbox_t text;

    do
    {
        ecchashmap_t* hashmap = (ecchashmap_t*)ref;
        ecchashitem_t* element = (ecchashitem_t*)ref;

        if(hashmap >= thisobj->hmapmapitems && hashmap < thisobj->hmapmapitems + thisobj->hmapmapcount)
        {
            const eccstrbox_t* keyText = ecc_keyidx_textof(hashmap->hmapmapvalue.key);
            ecc_context_typeerror(context, ecc_strbuf_create("'%.*s' is read-only", keyText->length, keyText->bytes));
        }
        else if(element >= thisobj->hmapitemitems && element < thisobj->hmapitemitems + thisobj->hmapitemcount)
            ecc_context_typeerror(context, ecc_strbuf_create("'%u' is read-only", element - thisobj->hmapitemitems));

    } while((thisobj = thisobj->prototype));

    text = ecc_context_textseek(context);
    ecc_context_typeerror(context, ecc_strbuf_create("'%.*s' is read-only", text.length, text.bytes));
}

eccobject_t* ecc_object_checkobject(ecccontext_t* context, int argument)
{
    eccvalue_t value = ecc_context_argument(context, argument);
    if(!ecc_value_isobject(value))
        ecc_context_typeerror(context, ecc_strbuf_create("not an object"));

    return value.data.object;
}

eccvalue_t ecc_objfnobject_valueof(ecccontext_t* context)
{
    return ecc_value_toobject(context, ecc_context_this(context));
}

eccvalue_t ecc_objfnobject_hasownproperty(ecccontext_t* context)
{
    eccobject_t* self;
    eccvalue_t value;
    eccindexkey_t key;
    uint32_t index;

    self = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    value = ecc_value_toprimitive(context, ecc_context_argument(context, 0), ECC_VALHINT_STRING);
    index = ecc_object_getindexorkey(value, &key);

    if(index < UINT32_MAX)
        return ecc_value_truth(ecc_object_element(self, index, ECC_VALFLAG_ASOWN) != NULL);
    else
        return ecc_value_truth(ecc_object_member(self, key, ECC_VALFLAG_ASOWN) != NULL);
}

eccvalue_t ecc_objfnobject_isprototypeof(ecccontext_t* context)
{
    eccvalue_t arg0;

    arg0 = ecc_context_argument(context, 0);

    if(ecc_value_isobject(arg0))
    {
        eccobject_t* v = arg0.data.object;
        eccobject_t* o = ecc_value_toobject(context, ecc_context_this(context)).data.object;

        do
            if(v == o)
                return ECCValConstTrue;
        while((v = v->prototype));
    }

    return ECCValConstFalse;
}

eccvalue_t ecc_objfnobject_propertyisenumerable(ecccontext_t* context)
{
    eccvalue_t value;
    eccobject_t* object;
    eccvalue_t* ref;

    value = ecc_value_toprimitive(context, ecc_context_argument(context, 0), ECC_VALHINT_STRING);
    object = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    ref = ecc_object_property(object, value, ECC_VALFLAG_ASOWN);

    if(ref)
        return ecc_value_truth(!(ref->flags & ECC_VALFLAG_HIDDEN));
    else
        return ECCValConstFalse;
}

eccvalue_t ecc_objfnobject_constructor(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);

    if(value.type == ECC_VALTYPE_NULL || value.type == ECC_VALTYPE_UNDEFINED)
        return ecc_value_object(ecc_object_create(ECC_Prototype_Object));
    else if(context->construct && ecc_value_isobject(value))
        return value;
    else
        return ecc_value_toobject(context, value);
}

eccvalue_t ecc_objfnobject_getprototypeof(ecccontext_t* context)
{
    eccobject_t* object;

    object = ecc_value_toobject(context, ecc_context_argument(context, 0)).data.object;

    return object->prototype ? ecc_value_objectvalue(object->prototype) : ECCValConstUndefined;
}

eccvalue_t ecc_objfnobject_getownpropertydescriptor(ecccontext_t* context)
{
    eccobject_t* object;
    eccvalue_t value;
    eccvalue_t* ref;

    object = ecc_value_toobject(context, ecc_context_argument(context, 0)).data.object;
    value = ecc_value_toprimitive(context, ecc_context_argument(context, 1), ECC_VALHINT_STRING);
    ref = ecc_object_property(object, value, ECC_VALFLAG_ASOWN);

    if(ref)
    {
        eccobject_t* result = ecc_object_create(ECC_Prototype_Object);

        if(ref->flags & ECC_VALFLAG_ACCESSOR)
        {
            if(ref->flags & ECC_VALFLAG_ASDATA)
            {
                ecc_object_addmember(result, ECC_ConstKey_value, ecc_object_getvalue(context, object, ref), 0);
                ecc_object_addmember(result, ECC_ConstKey_writable, ecc_value_truth(!(ref->flags & ECC_VALFLAG_READONLY)), 0);
            }
            else
            {
                ecc_object_addmember(result, ref->flags & ECC_VALFLAG_GETTER ? ECC_ConstKey_get : ECC_ConstKey_set, ecc_value_function(ref->data.function), 0);
                if(ref->data.function->pair)
                    ecc_object_addmember(result, ref->flags & ECC_VALFLAG_GETTER ? ECC_ConstKey_set : ECC_ConstKey_get, ecc_value_function(ref->data.function->pair), 0);
            }
        }
        else
        {
            ecc_object_addmember(result, ECC_ConstKey_value, *ref, 0);
            ecc_object_addmember(result, ECC_ConstKey_writable, ecc_value_truth(!(ref->flags & ECC_VALFLAG_READONLY)), 0);
        }

        ecc_object_addmember(result, ECC_ConstKey_enumerable, ecc_value_truth(!(ref->flags & ECC_VALFLAG_HIDDEN)), 0);
        ecc_object_addmember(result, ECC_ConstKey_configurable, ecc_value_truth(!(ref->flags & ECC_VALFLAG_SEALED)), 0);

        return ecc_value_object(result);
    }

    return ECCValConstUndefined;
}

eccvalue_t ecc_objfnobject_getownpropertynames(ecccontext_t* context)
{
    eccobject_t *object, *parent;
    eccobject_t* result;
    uint32_t index, count, length;

    object = ecc_object_checkobject(context, 0);
    result = ecc_array_create();
    length = 0;

    for(index = 0, count = ecc_object_elementcount(object); index < count; ++index)
        if(object->hmapitemitems[index].hmapitemvalue.check == 1)
            ecc_object_addelement(result, length++, ecc_value_fromchars(ecc_strbuf_create("%d", index)), 0);

    parent = object;
    while((parent = parent->prototype))
    {
        for(index = 2; index < parent->hmapmapcount; ++index)
        {
            eccvalue_t value = parent->hmapmapitems[index].hmapmapvalue;
            if(value.check == 1 && value.flags & ECC_VALFLAG_ASOWN)
                ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(value.key)), 0);
        }
    }

    for(index = 2; index < object->hmapmapcount; ++index)
        if(object->hmapmapitems[index].hmapmapvalue.check == 1)
            ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(object->hmapmapitems[index].hmapmapvalue.key)), 0);

    return ecc_value_object(result);
}

eccvalue_t ecc_objfnobject_defineproperty(ecccontext_t* context)
{
    eccobject_t *object, *descriptor;
    eccvalue_t property, value, *getter, *setter, *current, *flag;
    eccindexkey_t key;
    uint32_t index;

    object = ecc_object_checkobject(context, 0);
    property = ecc_value_toprimitive(context, ecc_context_argument(context, 1), ECC_VALHINT_STRING);
    descriptor = ecc_object_checkobject(context, 2);

    getter = ecc_object_member(descriptor, ECC_ConstKey_get, 0);
    setter = ecc_object_member(descriptor, ECC_ConstKey_set, 0);

    current = ecc_object_property(object, property, ECC_VALFLAG_ASOWN);

    if(getter || setter)
    {
        if(getter && getter->type == ECC_VALTYPE_UNDEFINED)
            getter = NULL;

        if(setter && setter->type == ECC_VALTYPE_UNDEFINED)
            setter = NULL;

        if(getter && getter->type != ECC_VALTYPE_FUNCTION)
            ecc_context_typeerror(context, ecc_strbuf_create("getter is not a function"));

        if(setter && setter->type != ECC_VALTYPE_FUNCTION)
            ecc_context_typeerror(context, ecc_strbuf_create("setter is not a function"));

        if(ecc_object_member(descriptor, ECC_ConstKey_value, 0) || ecc_object_member(descriptor, ECC_ConstKey_writable, 0))
            ecc_context_typeerror(context, ecc_strbuf_create("value & writable forbidden when a getter or setter are set"));

        if(getter)
        {
            value = *getter;
            if(setter)
                value.data.function->pair = setter->data.function;

            value.flags |= ECC_VALFLAG_GETTER;
        }
        else if(setter)
        {
            value = *setter;
            value.flags |= ECC_VALFLAG_SETTER;
        }
        else
        {
            value = ecc_value_function(ecc_function_createwithnative(ecc_oper_noop, 0));
            value.flags |= ECC_VALFLAG_GETTER;
        }
    }
    else
    {
        value = ecc_object_getmember(context, descriptor, ECC_ConstKey_value);

        flag = ecc_object_member(descriptor, ECC_ConstKey_writable, 0);
        if((flag && !ecc_value_istrue(ecc_object_getvalue(context, descriptor, flag))) || (!flag && (!current || current->flags & ECC_VALFLAG_READONLY)))
            value.flags |= ECC_VALFLAG_READONLY;
    }

    flag = ecc_object_member(descriptor, ECC_ConstKey_enumerable, 0);
    if((flag && !ecc_value_istrue(ecc_object_getvalue(context, descriptor, flag))) || (!flag && (!current || current->flags & ECC_VALFLAG_HIDDEN)))
        value.flags |= ECC_VALFLAG_HIDDEN;

    flag = ecc_object_member(descriptor, ECC_ConstKey_configurable, 0);
    if((flag && !ecc_value_istrue(ecc_object_getvalue(context, descriptor, flag))) || (!flag && (!current || current->flags & ECC_VALFLAG_SEALED)))
        value.flags |= ECC_VALFLAG_SEALED;

    if(!current)
    {
        ecc_object_addproperty(object, property, value, 0);
        return ECCValConstTrue;
    }
    else
    {
        if(current->flags & ECC_VALFLAG_SEALED)
        {
            if(!(value.flags & ECC_VALFLAG_SEALED) || (value.flags & ECC_VALFLAG_HIDDEN) != (current->flags & ECC_VALFLAG_HIDDEN))
                goto sealedError;

            if(current->flags & ECC_VALFLAG_ACCESSOR)
            {
                if(!(getter || setter))
                {
                    if(current->flags & ECC_VALFLAG_ASDATA)
                    {
                        eccobjfunction_t* currentSetter = current->flags & ECC_VALFLAG_GETTER ? current->data.function->pair : current->data.function;
                        if(currentSetter)
                        {
                            ecc_context_callfunction(context, currentSetter, ecc_value_object(object), 1, value);
                            return ECCValConstTrue;
                        }
                    }
                    goto sealedError;
                }
                else
                {
                    eccobjfunction_t* currentGetter = current->flags & ECC_VALFLAG_GETTER ? current->data.function : current->data.function->pair;
                    eccobjfunction_t* currentSetter = current->flags & ECC_VALFLAG_GETTER ? current->data.function->pair : current->data.function;

                    if(!getter != !currentGetter || !setter != !currentSetter)
                        goto sealedError;
                    else if(getter && getter->data.function->pair != currentGetter)
                        goto sealedError;
                    else if(setter && setter->data.function != currentSetter)
                        goto sealedError;
                }
            }
            else
            {
                if(!ecc_value_istrue(ecc_value_same(context, *current, value)))
                    goto sealedError;
            }
        }

        ecc_object_addproperty(object, property, value, 0);
    }

    return ECCValConstTrue;

sealedError:
    ecc_context_settextindexargument(context, 1);
    index = ecc_object_getindexorkey(property, &key);
    if(index == UINT32_MAX)
    {
        const eccstrbox_t* text = ecc_keyidx_textof(key);
        ecc_context_typeerror(context, ecc_strbuf_create("'%.*s' is non-configurable", text->length, text->bytes));
    }
    else
        ecc_context_typeerror(context, ecc_strbuf_create("'%u' is non-configurable", index));
    return ECCValConstUndefined;
}

eccvalue_t ecc_objfnobject_defineproperties(ecccontext_t* context)
{
    ecchashmap_t* originalHashmap = context->execenv->hmapmapitems;
    uint32_t originalHashmapCount = context->execenv->hmapmapcount;

    uint32_t index, count, hashmapCount = 6;
    eccobject_t *object, *properties;
    ecchashmap_t hashmap[hashmapCount];

    memset(hashmap, 0, hashmapCount * sizeof(*hashmap));

    object = ecc_object_checkobject(context, 0);
    properties = ecc_value_toobject(context, ecc_context_argument(context, 1)).data.object;

    context->execenv->hmapmapitems = hashmap;
    context->execenv->hmapmapcount = hashmapCount;

    ecc_context_replaceargument(context, 0, ecc_value_object(object));

    for(index = 0, count = ecc_object_elementcount(properties); index < count; ++index)
    {
        if(!properties->hmapitemitems[index].hmapitemvalue.check)
            continue;

        ecc_context_replaceargument(context, 1, ecc_value_fromfloat(index));
        ecc_context_replaceargument(context, 2, properties->hmapitemitems[index].hmapitemvalue);
        ecc_objfnobject_defineproperty(context);
    }

    for(index = 2; index < properties->hmapmapcount; ++index)
    {
        if(!properties->hmapmapitems[index].hmapmapvalue.check)
            continue;

        ecc_context_replaceargument(context, 1, ecc_value_fromkey(properties->hmapmapitems[index].hmapmapvalue.key));
        ecc_context_replaceargument(context, 2, properties->hmapmapitems[index].hmapmapvalue);
        ecc_objfnobject_defineproperty(context);
    }

    context->execenv->hmapmapitems = originalHashmap;
    context->execenv->hmapmapcount = originalHashmapCount;

    return ECCValConstUndefined;
}

eccvalue_t ecc_objfnobject_objectcreate(ecccontext_t* context)
{
    eccobject_t *object, *result;
    eccvalue_t properties;

    object = ecc_object_checkobject(context, 0);
    properties = ecc_context_argument(context, 1);

    result = ecc_object_create(object);
    if(properties.type != ECC_VALTYPE_UNDEFINED)
    {
        ecc_context_replaceargument(context, 0, ecc_value_object(result));
        ecc_objfnobject_defineproperties(context);
    }

    return ecc_value_object(result);
}

eccvalue_t ecc_objfnobject_seal(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = ecc_object_checkobject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    for(index = 0, count = ecc_object_elementcount(object); index < count; ++index)
        if(object->hmapitemitems[index].hmapitemvalue.check == 1)
            object->hmapitemitems[index].hmapitemvalue.flags |= ECC_VALFLAG_SEALED;

    for(index = 2; index < object->hmapmapcount; ++index)
        if(object->hmapmapitems[index].hmapmapvalue.check == 1)
            object->hmapmapitems[index].hmapmapvalue.flags |= ECC_VALFLAG_SEALED;

    return ecc_value_object(object);
}

eccvalue_t ecc_objfnobject_freeze(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = ecc_object_checkobject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    for(index = 0, count = ecc_object_elementcount(object); index < count; ++index)
        if(object->hmapitemitems[index].hmapitemvalue.check == 1)
            object->hmapitemitems[index].hmapitemvalue.flags |= ECC_VALFLAG_FROZEN;

    for(index = 2; index < object->hmapmapcount; ++index)
        if(object->hmapmapitems[index].hmapmapvalue.check == 1)
            object->hmapmapitems[index].hmapmapvalue.flags |= ECC_VALFLAG_FROZEN;

    return ecc_value_object(object);
}

eccvalue_t ecc_objfnobject_preventextensions(ecccontext_t* context)
{
    eccobject_t* object;

    object = ecc_object_checkobject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    return ecc_value_object(object);
}

eccvalue_t ecc_objfnobject_issealed(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = ecc_object_checkobject(context, 0);
    if(!(object->flags & ECC_OBJFLAG_SEALED))
        return ECCValConstFalse;

    for(index = 0, count = ecc_object_elementcount(object); index < count; ++index)
        if(object->hmapitemitems[index].hmapitemvalue.check == 1 && !(object->hmapitemitems[index].hmapitemvalue.flags & ECC_VALFLAG_SEALED))
            return ECCValConstFalse;

    for(index = 2; index < object->hmapmapcount; ++index)
        if(object->hmapmapitems[index].hmapmapvalue.check == 1 && !(object->hmapmapitems[index].hmapmapvalue.flags & ECC_VALFLAG_SEALED))
            return ECCValConstFalse;

    return ECCValConstTrue;
}

eccvalue_t ecc_objfnobject_isfrozen(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = ecc_object_checkobject(context, 0);
    if(!(object->flags & ECC_OBJFLAG_SEALED))
        return ECCValConstFalse;

    for(index = 0, count = ecc_object_elementcount(object); index < count; ++index)
        if(object->hmapitemitems[index].hmapitemvalue.check == 1 && !(object->hmapitemitems[index].hmapitemvalue.flags & ECC_VALFLAG_FROZEN))
            return ECCValConstFalse;

    for(index = 2; index < object->hmapmapcount; ++index)
        if(object->hmapmapitems[index].hmapmapvalue.check == 1 && !(object->hmapmapitems[index].hmapmapvalue.flags & ECC_VALFLAG_FROZEN))
            return ECCValConstFalse;

    return ECCValConstTrue;
}

eccvalue_t ecc_objfnobject_isextensible(ecccontext_t* context)
{
    eccobject_t* object;

    object = ecc_object_checkobject(context, 0);
    return ecc_value_truth(!(object->flags & ECC_OBJFLAG_SEALED));
}

eccvalue_t ecc_objfnobject_keys(ecccontext_t* context)
{
    eccobject_t *object, *parent;
    eccobject_t* result;
    uint32_t index, count, length;

    object = ecc_object_checkobject(context, 0);
    result = ecc_array_create();
    length = 0;

    for(index = 0, count = ecc_object_elementcount(object); index < count; ++index)
        if(object->hmapitemitems[index].hmapitemvalue.check == 1 && !(object->hmapitemitems[index].hmapitemvalue.flags & ECC_VALFLAG_HIDDEN))
            ecc_object_addelement(result, length++, ecc_value_fromchars(ecc_strbuf_create("%d", index)), 0);

    parent = object;
    while((parent = parent->prototype))
    {
        for(index = 2; index < parent->hmapmapcount; ++index)
        {
            eccvalue_t value = parent->hmapmapitems[index].hmapmapvalue;
            if(value.check == 1 && value.flags & ECC_VALFLAG_ASOWN & !(value.flags & ECC_VALFLAG_HIDDEN))
                ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(value.key)), 0);
        }
    }

    for(index = 2; index < object->hmapmapcount; ++index)
        if(object->hmapmapitems[index].hmapmapvalue.check == 1 && !(object->hmapmapitems[index].hmapmapvalue.flags & ECC_VALFLAG_HIDDEN))
            ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(object->hmapmapitems[index].hmapmapvalue.key)), 0);

    return ecc_value_object(result);
}

void ecc_object_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    //assert(sizeof(*ECC_Prototype_Object->hmapmapitems) == 32);

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Object, ecc_objfnobject_constructor, 1, NULL, ecc_value_object(ECC_Prototype_Object), NULL);

    ecc_function_addmethod(ECC_CtorFunc_Object, "getPrototypeOf", ecc_objfnobject_getprototypeof, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "getOwnPropertyDescriptor", ecc_objfnobject_getownpropertydescriptor, 2, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "getOwnPropertyNames", ecc_objfnobject_getownpropertynames, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "create", ecc_objfnobject_objectcreate, 2, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "defineProperty", ecc_objfnobject_defineproperty, 3, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "defineProperties", ecc_objfnobject_defineproperties, 2, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "seal", ecc_objfnobject_seal, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "freeze", ecc_objfnobject_freeze, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "preventExtensions", ecc_objfnobject_preventextensions, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "isSealed", ecc_objfnobject_issealed, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "isFrozen", ecc_objfnobject_isfrozen, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "isExtensible", ecc_objfnobject_isextensible, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "keys", ecc_objfnobject_keys, 1, h);

    ecc_function_addto(ECC_Prototype_Object, "toString", ecc_object_tostringfn, 0, h);
    ecc_function_addto(ECC_Prototype_Object, "toLocaleString", ecc_object_tostringfn, 0, h);
    ecc_function_addto(ECC_Prototype_Object, "valueOf", ecc_objfnobject_valueof, 0, h);
    ecc_function_addto(ECC_Prototype_Object, "hasOwnProperty", ecc_objfnobject_hasownproperty, 1, h);
    ecc_function_addto(ECC_Prototype_Object, "isPrototypeOf", ecc_objfnobject_isprototypeof, 1, h);
    ecc_function_addto(ECC_Prototype_Object, "propertyIsEnumerable", ecc_objfnobject_propertyisenumerable, 1, h);
}

void ecc_object_teardown(void)
{
    ECC_Prototype_Object = NULL;
    ECC_CtorFunc_Object = NULL;
}

eccobject_t* ecc_object_create(eccobject_t* prototype)
{
    return ecc_object_createsized(prototype, ECC_CONF_DEFAULTSIZE);
}

eccobject_t* ecc_object_createsized(eccobject_t* prototype, uint32_t size)
{
    eccobject_t* self;
    self = (eccobject_t*)calloc(sizeof(*self), 1);
    ecc_mempool_addobject(self);
    return ecc_object_initializesized(self, prototype, size);
}

eccobject_t* ecc_object_createtyped(const eccobjinterntype_t* type)
{
    eccobject_t* self = ecc_object_createsized(ECC_Prototype_Object, ECC_CONF_DEFAULTSIZE);
    self->type = type;
    return self;
}

eccobject_t* ecc_object_initialize(eccobject_t* self, eccobject_t* prototype)
{
    return ecc_object_initializesized(self, prototype, ECC_CONF_DEFAULTSIZE);
}

eccobject_t* ecc_object_initializesized(eccobject_t* self, eccobject_t* prototype, uint32_t size)
{
    size_t byteSize;
    assert(self);
    memset(self, 0, sizeof(eccobject_t));
    self->type = prototype ? prototype->type : &ECC_Type_Object;

    self->prototype = prototype;
    self->hmapmapcount = 2;
    /*
    // hashmap is always 2 slots minimum
    // slot 0 is self referencing undefined value (all zeroes)
    // slot 1 is entry point, referencing undefined (slot 0) by default (all zeroes)
    */
    if(size > 0)
    {
        self->hmapmapcapacity = size;

        byteSize = sizeof(*self->hmapmapitems) * self->hmapmapcapacity;
        self->hmapmapitems = (ecchashmap_t*)malloc(byteSize);
        memset(self->hmapmapitems, 0, byteSize);
    }
    else
    {
        /* size is == zero = to be initialized later by caller */
        self->hmapmapcapacity = 2;
    }
    return self;
}

void ecc_object_destroymapmap(eccobject_t* self)
{
    free(self->hmapmapitems);
    self->hmapmapitems = NULL;
}

void ecc_object_destroymapitems(eccobject_t* self)
{
    free(self->hmapitemitems);
    self->hmapitemitems = NULL;
}

eccobject_t* ecc_object_finalize(eccobject_t* self)
{
    assert(self);
    if(self->type->fnfinalize)
    {
        self->type->fnfinalize(self);
    }
    ecc_object_destroymapmap(self);
    ecc_object_destroymapitems(self);
    return self;
}

eccobject_t* ecc_object_copy(const eccobject_t* original)
{
    size_t bsz;
    eccobject_t* self;
    self = (eccobject_t*)malloc(sizeof(*self));
    ecc_mempool_addobject(self);
    *self = *original;
    bsz = sizeof(*self->hmapitemitems) * self->hmapitemcount;
    self->hmapitemitems = (ecchashitem_t*)malloc(bsz);
    memcpy(self->hmapitemitems, original->hmapitemitems, bsz);
    bsz = sizeof(*self->hmapmapitems) * self->hmapmapcount;
    self->hmapmapitems = (ecchashmap_t*)malloc(bsz);
    memcpy(self->hmapmapitems, original->hmapmapitems, bsz);
    return self;
}

void ecc_object_destroy(eccobject_t* self)
{
    assert(self);
    if(self->mustdestroymapmaps)
    {
        ecc_object_destroymapmap(self);
    }
    if(self->mustdestroymapitems)
    {
        ecc_object_destroymapitems(self);
    }
    free(self);
    self = NULL;
}

eccvalue_t* ecc_object_member(eccobject_t* self, eccindexkey_t member, int flags)
{
    int lookupChain = !(flags & ECC_VALFLAG_ASOWN);
    eccobject_t* object = self;
    eccvalue_t* ref = NULL;
    uint32_t slot;

    assert(self);

    do
    {
        if((slot = ecc_object_getslot(object, member)))
        {
            ref = &object->hmapmapitems[slot].hmapmapvalue;
            if(ref->check == 1)
                return lookupChain || object == self || (ref->flags & flags) ? ref : NULL;
        }
    } while((object = object->prototype));

    return NULL;
}

eccvalue_t* ecc_object_element(eccobject_t* self, uint32_t index, int flags)
{
    int lookupChain = !(flags & ECC_VALFLAG_ASOWN);
    eccobject_t* object = self;
    eccvalue_t* ref = NULL;

    assert(self);

    if(self->type == &ECC_Type_String)
    {
        eccvalue_t* subref = ecc_object_addmember(self, ECC_ConstKey_none, ecc_string_valueatindex((eccobjstring_t*)self, index), 0);
        subref->check = 0;
        return subref;
    }
    else if(index > ECC_CONF_MAXELEMENTS)
    {
        eccindexkey_t key = ecc_object_keyofindex(index, 0);
        if(key.data.integer)
            return ecc_object_member(self, key, flags);
    }
    else
        do
        {
            if(index < object->hmapitemcount)
            {
                ref = &object->hmapitemitems[index].hmapitemvalue;
                if(ref->check == 1)
                    return lookupChain || object == self || (ref->flags & flags) ? ref : NULL;
            }
        } while((object = object->prototype));

    return NULL;
}

eccvalue_t* ecc_object_property(eccobject_t* self, eccvalue_t property, int flags)
{
    eccindexkey_t key;
    uint32_t index = ecc_object_getindexorkey(property, &key);

    if(index < UINT32_MAX)
        return ecc_object_element(self, index, flags);
    else
        return ecc_object_member(self, key, flags);
}

eccvalue_t ecc_object_getvalue(ecccontext_t* context, eccobject_t* self, eccvalue_t* ref)
{
    if(!ref)
        return ECCValConstUndefined;

    if(ref->flags & ECC_VALFLAG_ACCESSOR)
    {
        if(!context)
            ecc_script_fatal("cannot use getter outside context");

        if(ref->flags & ECC_VALFLAG_GETTER)
            return ecc_context_callfunction(context, ref->data.function, ecc_value_object(self), 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        else if(ref->data.function->pair)
            return ecc_context_callfunction(context, ref->data.function->pair, ecc_value_object(self), 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        else
            return ECCValConstUndefined;
    }

    return *ref;
}

eccvalue_t ecc_object_getmember(ecccontext_t* context, eccobject_t* self, eccindexkey_t key)
{
    return ecc_object_getvalue(context, self, ecc_object_member(self, key, 0));
}

eccvalue_t ecc_object_getelement(ecccontext_t* context, eccobject_t* self, uint32_t index)
{
    if(self->type == &ECC_Type_String)
        return ecc_string_valueatindex((eccobjstring_t*)self, index);
    else
        return ecc_object_getvalue(context, self, ecc_object_element(self, index, 0));
}

eccvalue_t ecc_object_getproperty(ecccontext_t* context, eccobject_t* self, eccvalue_t property)
{
    eccindexkey_t key;
    uint32_t index = ecc_object_getindexorkey(property, &key);

    if(index < UINT32_MAX)
        return ecc_object_getelement(context, self, index);
    else
        return ecc_object_getmember(context, self, key);
}

eccvalue_t ecc_object_putvalue(ecccontext_t* context, eccobject_t* self, eccvalue_t* ref, eccvalue_t value)
{
    if(ref->flags & ECC_VALFLAG_ACCESSOR)
    {
        assert(context);

        if(ref->flags & ECC_VALFLAG_SETTER)
            ecc_context_callfunction(context, ref->data.function, ecc_value_object(self), 1 | ECC_CTXSPECIALTYPE_ASACCESSOR, value);
        else if(ref->data.function->pair)
            ecc_context_callfunction(context, ref->data.function->pair, ecc_value_object(self), 1 | ECC_CTXSPECIALTYPE_ASACCESSOR, value);
        else if(context->isstrictmode || (context->parent && context->parent->isstrictmode))
            ecc_object_readonlyerror(context, ref, self);

        return value;
    }

    if(ref->check == 1)
    {
        if(ref->flags & ECC_VALFLAG_READONLY)
        {
            if(context->isstrictmode)
                ecc_object_readonlyerror(context, ref, self);
            else
                return value;
        }
        else
            value.flags = ref->flags;
    }

    return *ref = value;
}

eccvalue_t ecc_object_putmember(ecccontext_t* context, eccobject_t* self, eccindexkey_t key, eccvalue_t value)
{
    eccvalue_t* ref;

    value.flags = 0;

    if((ref = ecc_object_member(self, key, ECC_VALFLAG_ASOWN | ECC_VALFLAG_ACCESSOR)))
        return ecc_object_putvalue(context, self, ref, value);
    else if(self->prototype && (ref = ecc_object_member(self->prototype, key, 0)))
    {
        if(ref->flags & ECC_VALFLAG_READONLY)
            ecc_context_typeerror(context, ecc_strbuf_create("'%.*s' is readonly", ecc_keyidx_textof(key)->length, ecc_keyidx_textof(key)->bytes));
    }

    if(self->flags & ECC_OBJFLAG_SEALED)
        ecc_context_typeerror(context, ecc_strbuf_create("object is not extensible"));

    return *ecc_object_addmember(self, key, value, 0);
}

eccvalue_t ecc_object_putelement(ecccontext_t* context, eccobject_t* self, uint32_t index, eccvalue_t value)
{
    eccvalue_t* ref;

    if(index > ECC_CONF_MAXELEMENTS)
    {
        if(self->hmapitemcapacity <= index)
            ecc_object_resizeelement(self, index < UINT32_MAX ? index + 1 : index);
        else if(self->hmapitemcount <= index)
            self->hmapitemcount = index + 1;

        return ecc_object_putmember(context, self, ecc_object_keyofindex(index, 1), value);
    }

    value.flags = 0;

    if((ref = ecc_object_element(self, index, ECC_VALFLAG_ASOWN | ECC_VALFLAG_ACCESSOR)))
        return ecc_object_putvalue(context, self, ref, value);
    else if(self->prototype && (ref = ecc_object_element(self, index, 0)))
    {
        if(ref->flags & ECC_VALFLAG_READONLY)
            ecc_context_typeerror(context, ecc_strbuf_create("'%u' is readonly", index, index));
    }

    if(self->flags & ECC_OBJFLAG_SEALED)
        ecc_context_typeerror(context, ecc_strbuf_create("object is not extensible"));

    return *ecc_object_addelement(self, index, value, 0);
}

eccvalue_t ecc_object_putproperty(ecccontext_t* context, eccobject_t* self, eccvalue_t primitive, eccvalue_t value)
{
    eccindexkey_t key;
    uint32_t index = ecc_object_getindexorkey(primitive, &key);

    if(index < UINT32_MAX)
        return ecc_object_putelement(context, self, index, value);
    else
        return ecc_object_putmember(context, self, key, value);
}

eccvalue_t* ecc_object_addmember(eccobject_t* self, eccindexkey_t key, eccvalue_t value, int flags)
{
    int depth;
    int needtoadd;
    uint32_t capacity;
    uint32_t slot;
    size_t needed;
    ecchashmap_t* tmp;
    slot = 1;
    depth = 0;
    assert(self);
    do
    {
        if(!self->hmapmapitems[slot].slot[key.data.depth[depth]])
        {
            needtoadd = 4 - depth - (self->hmapmapcapacity - self->hmapmapcount);
            if(needtoadd > 0)
            {
                capacity = self->hmapmapcapacity;
                self->hmapmapcapacity = self->hmapmapcapacity ? self->hmapmapcapacity * 2 : 2;
                needed = (sizeof(*self->hmapmapitems) * self->hmapmapcapacity);
                tmp = (ecchashmap_t*)realloc(self->hmapmapitems, needed);
                if(tmp == NULL)
                {
                    fprintf(stderr, "in addmember: failed to reallocate for %ld bytes\n", needed);
                }
                self->hmapmapitems = tmp;
                memset(self->hmapmapitems + capacity, 0, sizeof(*self->hmapmapitems) * (self->hmapmapcapacity - capacity));
            }
            do
            {
                assert(self->hmapmapcount < UINT16_MAX);
                slot = self->hmapmapitems[slot].slot[key.data.depth[depth]] = self->hmapmapcount++;
            } while(++depth < 4);
            break;
        }
        else
            assert(self->hmapmapitems[slot].hmapmapvalue.check != 1);

        slot = self->hmapmapitems[slot].slot[key.data.depth[depth]];
        assert(slot != 1);
        assert(slot < self->hmapmapcount);
    } while(++depth < 4);

    if(value.flags & ECC_VALFLAG_ACCESSOR)
        if(self->hmapmapitems[slot].hmapmapvalue.check == 1 && self->hmapmapitems[slot].hmapmapvalue.flags & ECC_VALFLAG_ACCESSOR)
            if((self->hmapmapitems[slot].hmapmapvalue.flags & ECC_VALFLAG_ACCESSOR) != (value.flags & ECC_VALFLAG_ACCESSOR))
                value.data.function->pair = self->hmapmapitems[slot].hmapmapvalue.data.function;

    value.key = key;
    value.flags |= flags;

    self->hmapmapitems[slot].hmapmapvalue = value;

    return &self->hmapmapitems[slot].hmapmapvalue;
}

eccvalue_t* ecc_object_addelement(eccobject_t* self, uint32_t index, eccvalue_t value, int flags)
{
    eccvalue_t* ref;
    assert(self);
    if(self->hmapitemcapacity <= index)
    {
        ecc_object_resizeelement(self, index < UINT32_MAX ? index + 1 : index);
    }
    else if(self->hmapitemcount <= index)
    {
        self->hmapitemcount = index + 1;
    }
    if(index > ECC_CONF_MAXELEMENTS)
    {
        return ecc_object_addmember(self, ecc_object_keyofindex(index, 1), value, flags);
    }
    ref = &self->hmapitemitems[index].hmapitemvalue;
    value.flags |= flags;
    *ref = value;
    return ref;
}

eccvalue_t* ecc_object_addproperty(eccobject_t* self, eccvalue_t primitive, eccvalue_t value, int flags)
{
    uint32_t index;
    eccindexkey_t key;
    index = ecc_object_getindexorkey(primitive, &key);
    if(index < UINT32_MAX)
    {
        return ecc_object_addelement(self, index, value, flags);
    }
    return ecc_object_addmember(self, key, value, flags);
}

int ecc_object_deletemember(eccobject_t* self, eccindexkey_t member)
{
    eccobject_t* object;
    uint32_t slot;
    uint32_t refSlot;
    object = self;
    assert(object);
    assert(member.data.integer);
    refSlot = self->hmapmapitems[self->hmapmapitems[self->hmapmapitems[1].slot[member.data.depth[0]]].slot[member.data.depth[1]]].slot[member.data.depth[2]];
    slot = self->hmapmapitems[refSlot].slot[member.data.depth[3]];
    if(!slot || !(object->hmapmapitems[slot].hmapmapvalue.check == 1))
    {
        return 1;
    }
    if(object->hmapmapitems[slot].hmapmapvalue.flags & ECC_VALFLAG_SEALED)
    {
        return 0;
    }
    object->hmapmapitems[slot].hmapmapvalue = ECCValConstUndefined;
    self->hmapmapitems[refSlot].slot[member.data.depth[3]] = 0;
    return 1;
}

int ecc_object_deleteelement(eccobject_t* self, uint32_t index)
{
    eccindexkey_t key;
    assert(self);
    if(index > ECC_CONF_MAXELEMENTS)
    {
        key = ecc_object_keyofindex(index, 0);
        if(key.data.integer)
        {
            return ecc_object_deletemember(self, key);
        }
        else
        {
            return 1;
        }
    }
    if(index < self->hmapitemcount)
    {
        if(self->hmapitemitems[index].hmapitemvalue.flags & ECC_VALFLAG_SEALED)
        {
            return 0;
        }
        memset(&self->hmapitemitems[index], 0, sizeof(*self->hmapitemitems));
    }
    return 1;
}

int ecc_object_deleteproperty(eccobject_t* self, eccvalue_t primitive)
{
    uint32_t index;
    eccindexkey_t key;
    index = ecc_object_getindexorkey(primitive, &key);
    if(index < UINT32_MAX)
    {
        return ecc_object_deleteelement(self, index);
    }    
    return ecc_object_deletemember(self, key);
}

void ecc_object_packvalue(eccobject_t* self)
{
    size_t needed;
    uint32_t index;
    uint32_t valueIndex;
    uint32_t copyIndex;
    uint32_t slot;
    ecchashmap_t data;
    ecchashmap_t* tmp;
    index = 2;
    valueIndex = 2;
    assert(self);
    for(; index < self->hmapmapcount; ++index)
    {
        if(self->hmapmapitems[index].hmapmapvalue.check == 1)
        {
            data = self->hmapmapitems[index];
            for(copyIndex = index; copyIndex > valueIndex; --copyIndex)
            {
                self->hmapmapitems[copyIndex] = self->hmapmapitems[copyIndex - 1];
                if(!(self->hmapmapitems[copyIndex].hmapmapvalue.check == 1))
                {
                    for(slot = 0; slot < 16; ++slot)
                    {
                        if(self->hmapmapitems[copyIndex].slot[slot] == index)
                        {
                            self->hmapmapitems[copyIndex].slot[slot] = valueIndex;
                        }
                        else if(self->hmapmapitems[copyIndex].slot[slot] >= valueIndex && self->hmapmapitems[copyIndex].slot[slot] < index)
                        {
                            self->hmapmapitems[copyIndex].slot[slot]++;
                        }
                    }
                }
            }
            for(slot = 0; slot < 16; ++slot)
            {
                if(self->hmapmapitems[1].slot[slot] >= valueIndex && self->hmapmapitems[1].slot[slot] < index)
                {
                    self->hmapmapitems[1].slot[slot]++;
                }
            }
            self->hmapmapitems[valueIndex++] = data;
        }
    }
    needed = (sizeof(*self->hmapmapitems) * (self->hmapmapcount));
    tmp = (ecchashmap_t*)realloc(self->hmapmapitems, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in packvalue: failed to reallocate for %ld bytes\n", needed);
    }
    self->hmapmapitems = tmp;
    self->hmapmapcapacity = self->hmapmapcount;
    if(self->hmapitemcount)
    {
        self->hmapitemcapacity = self->hmapitemcount;
    }
}

void ecc_object_stripmap(eccobject_t* self)
{
    size_t needed;
    uint32_t index;
    ecchashmap_t* tmp;
    index = 2;
    assert(self);
    while(index < self->hmapmapcount && self->hmapmapitems[index].hmapmapvalue.check == 1)
    {
        ++index;
    }
    self->hmapmapcapacity = self->hmapmapcount = index;
    needed = (sizeof(*self->hmapmapitems) * self->hmapmapcapacity);
    tmp = (ecchashmap_t*)realloc(self->hmapmapitems, needed);
    if(tmp == NULL)
    {
        fprintf(stderr, "in stripmap: failed to reallocate for %ld bytes\n", needed);
    }
    self->hmapmapitems = tmp;
    memset(self->hmapmapitems + 1, 0, sizeof(*self->hmapmapitems));
}

void ecc_object_reserveslots(eccobject_t* self, uint32_t slots)
{
    int needtoadd;
    size_t needed;
    uint32_t capacity;
    ecchashmap_t* tmp;
    needtoadd = (slots * 4) - (self->hmapmapcapacity - self->hmapmapcount);
    assert(slots < self->hmapmapcapacity);
    if(needtoadd > 0)
    {
        capacity = self->hmapmapcapacity;
        self->hmapmapcapacity = self->hmapmapcapacity ? self->hmapmapcapacity * 2 : 2;
        needed = (sizeof(*self->hmapmapitems) * self->hmapmapcapacity);
        tmp = (ecchashmap_t*)realloc(self->hmapmapitems, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in reserveslots: failed to reallocate for %ld bytes\n", needed);
        }
        self->hmapmapitems = tmp;
        memset(self->hmapmapitems + capacity, 0, sizeof(*self->hmapmapitems) * (self->hmapmapcapacity - capacity));
    }
}

int ecc_object_resizeelement(eccobject_t* self, uint32_t size)
{
    uint32_t capacity;
    uint32_t until;
    uint32_t e;
    uint32_t index;
    uint32_t h;
    size_t needed;
    ecchashmap_t* hashmap;
    ecchashitem_t* element;
    ecchashitem_t* tmp;
    if(size <= self->hmapitemcapacity)
    {
        capacity = self->hmapitemcapacity;
    }
    else if(size < 4)
    {
        /* 64-bytes mini */
        capacity = 4;
    }
    else if(size < 64)
    {
        /* power of two steps between */
        capacity = ecc_object_nextpoweroftwo(size);
    }
    else if(size > ECC_CONF_MAXELEMENTS)
    {
        capacity = ECC_CONF_MAXELEMENTS + 1;
    }
    else
    {
        /* 1024-bytes chunk */
        capacity = size - 1;
        capacity |= 63;
        ++capacity;
    }
    assert(self);
    if(capacity != self->hmapitemcapacity)
    {
        if(size > ECC_CONF_MAXELEMENTS)
        {
            ecc_env_printwarning("creating array of faked length %u with actual length %u, because array length is >0x%x", size, capacity,
                                  ECC_CONF_MAXELEMENTS);
        }
        needed = (sizeof(*self->hmapitemitems) * capacity);
        tmp = (ecchashitem_t*)realloc(self->hmapitemitems, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in resizeelement: failed to reallocate for %ld bytes\n", needed);
        }
        self->hmapitemitems = tmp;
        if(capacity > self->hmapitemcapacity)
        {
            memset(self->hmapitemitems + self->hmapitemcapacity, 0, sizeof(*self->hmapitemitems) * (capacity - self->hmapitemcapacity));
        }
        self->hmapitemcapacity = capacity;
    }
    else if(size < self->hmapitemcount)
    {
        until = size;
        if(self->hmapitemcount > ECC_CONF_MAXELEMENTS)
        {
            for(h = 2; h < self->hmapmapcount; ++h)
            {
                hashmap = &self->hmapmapitems[h];
                if(hashmap->hmapmapvalue.check == 1)
                {
                    index = ecc_astlex_scanelement(*ecc_keyidx_textof(hashmap->hmapmapvalue.key));
                    if(hashmap->hmapmapvalue.check == 1 && (hashmap->hmapmapvalue.flags & ECC_VALFLAG_SEALED) && index >= until)
                    {
                        until = index + 1;
                    }
                }
            }
            for(h = 2; h < self->hmapmapcount; ++h)
            {
                hashmap = &self->hmapmapitems[h];
                if(hashmap->hmapmapvalue.check == 1)
                {
                    if(ecc_astlex_scanelement(*ecc_keyidx_textof(hashmap->hmapmapvalue.key)) >= until)
                    {
                        self->hmapmapitems[h].hmapmapvalue.check = 0;
                    }
                }
            }
            if(until > size)
            {
                self->hmapitemcount = until;
                return 1;
            }
            self->hmapitemcount = self->hmapitemcapacity;
        }
        for(e = size; e < self->hmapitemcount; ++e)
        {
            element = &self->hmapitemitems[e];
            if(element->hmapitemvalue.check == 1 && (element->hmapitemvalue.flags & ECC_VALFLAG_SEALED) && e >= until)
            {
                until = e + 1;
            }
        }
        memset(self->hmapitemitems + until, 0, sizeof(*self->hmapitemitems) * (self->hmapitemcount - until));
        if(until > size)
        {
            self->hmapitemcount = until;
            return 1;
        }
    }
    self->hmapitemcount = size;
    return 0;
}

void ecc_object_populateelementwithclist(eccobject_t* self, uint32_t count, const char* list[])
{
    double binary;
    char* end;
    int index;

    assert(self);
    assert(count <= ECC_CONF_MAXELEMENTS);

    if(count > self->hmapitemcount)
        ecc_object_resizeelement(self, count);

    for(index = 0; index < (int)count; ++index)
    {
        uint32_t length = (uint32_t)strlen(list[index]);
        binary = strtod(list[index], &end);

        if(end == list[index] + length)
            self->hmapitemitems[index].hmapitemvalue = ecc_value_fromfloat(binary);
        else
        {
            eccstrbuffer_t* chars = ecc_strbuf_createsized(length);
            memcpy(chars->bytes, list[index], length);

            self->hmapitemitems[index].hmapitemvalue = ecc_value_fromchars(chars);
        }
    }
}

eccvalue_t ecc_object_tostringfn(ecccontext_t* context)
{
    if(context->thisvalue.type == ECC_VALTYPE_NULL)
        return ecc_value_fromtext(&ECC_String_NullType);
    else if(context->thisvalue.type == ECC_VALTYPE_UNDEFINED)
        return ecc_value_fromtext(&ECC_String_UndefinedType);
    else if(ecc_value_isstring(context->thisvalue))
        return ecc_value_fromtext(&ECC_String_StringType);
    else if(ecc_value_isnumber(context->thisvalue))
        return ecc_value_fromtext(&ECC_String_NumberType);
    else if(ecc_value_isboolean(context->thisvalue))
        return ecc_value_fromtext(&ECC_String_BooleanType);
    else if(ecc_value_isobject(context->thisvalue))
        return ecc_value_fromtext(context->thisvalue.data.object->type->text);
    else
        assert(0);

    return ECCValConstUndefined;
}

void ecc_object_dumpto(eccobject_t* self, FILE* file)
{
    uint32_t index, count;
    int isArray;

    assert(self);

    isArray = ecc_value_objectisarray(self);

    fprintf(file, isArray ? "[ " : "{ ");

    for(index = 0, count = ecc_object_elementcount(self); index < count; ++index)
    {
        if(self->hmapitemitems[index].hmapitemvalue.check == 1)
        {
            if(!isArray)
                fprintf(file, "%d: ", (int)index);

            if(self->hmapitemitems[index].hmapitemvalue.type == ECC_VALTYPE_OBJECT && self->hmapitemitems[index].hmapitemvalue.data.object == self)
                fprintf(file, "this");
            else
                ecc_value_dumpto(self->hmapitemitems[index].hmapitemvalue, file);

            fprintf(file, ", ");
        }
    }

    if(!isArray)
    {
        for(index = 0; index < self->hmapmapcount; ++index)
        {
            if(self->hmapmapitems[index].hmapmapvalue.check == 1)
            {
                fprintf(stderr, "'");
                ecc_keyidx_dumpto(self->hmapmapitems[index].hmapmapvalue.key, file);
                fprintf(file, "': ");

                if(self->hmapmapitems[index].hmapmapvalue.type == ECC_VALTYPE_OBJECT && self->hmapmapitems[index].hmapmapvalue.data.object == self)
                    fprintf(file, "this");
                else
                    ecc_value_dumpto(self->hmapmapitems[index].hmapmapvalue, file);

                fprintf(file, ", ");
            }
            /*
            else
            {
                fprintf(file, "\n");
                for (int j = 0; j < 16; ++j)
                {
                    fprintf(file, "%02x ", self->hmapmapitems[index].slot[j]);
                }
            }
            */
        }
    }

    fprintf(file, isArray ? "]" : "}");
}
