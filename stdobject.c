//
//  object.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

const uint32_t io_libecc_object_ElementMax = 0xffffff;

static const int defaultSize = 8;

eccobject_t* ECC_Prototype_Object = NULL;
eccobjfunction_t* ECC_CtorFunc_Object = NULL;

const eccobjinterntype_t ECC_Type_Object = {
    .text = &ECC_ConstString_ObjectType,
};

// MARK: - Static Members

uint16_t eccobject_getSlot(const eccobject_t* const self, const eccindexkey_t key)
{
    return self->hashmap[self->hashmap[self->hashmap[self->hashmap[1].slot[key.data.depth[0]]].slot[key.data.depth[1]]].slot[key.data.depth[2]]].slot[key.data.depth[3]];
}

uint32_t eccobject_getIndexOrKey(eccvalue_t property, eccindexkey_t* key)
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
                abovezero = (property.data.binary >= 0);
                belomax = (property.data.binary < UINT32_MAX);
                /*
                * NOTE: this *will* break when using '-march=native' and '-ffast-math' mode.
                * it emits illegal instructions. but i have no clue why.
                */
                issame = (((float)property.data.binary) == ((uint32_t)property.data.binary));
            }
            if(isbin && abovezero && belomax && issame)
            {
                index = property.data.binary;
            }
            else if(ecc_value_isstring(property))
            {
                ecctextstring_t text = ecc_value_textof(&property);
                if((index = ecc_astlex_scanelement(text)) == UINT32_MAX)
                    *key = ecc_keyidx_makewithtext(text, ECC_INDEXFLAG_COPYONCREATE);
            }
            else
                return eccobject_getIndexOrKey(ecc_value_tostring(NULL, property), key);
        }
    }

    return index;
}

eccindexkey_t eccobject_keyOfIndex(uint32_t index, int create)
{
    char buffer[10 + 1];
    uint16_t length;

    length = snprintf(buffer, sizeof(buffer), "%u", (unsigned)index);
    if(create)
        return ecc_keyidx_makewithtext(ecc_textbuf_make(buffer, length), ECC_INDEXFLAG_COPYONCREATE);
    else
        return ecc_keyidx_search(ecc_textbuf_make(buffer, length));
}

uint32_t eccobject_nextPowerOfTwo(uint32_t v)
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

uint32_t eccobject_elementCount(eccobject_t* self)
{
    if(self->elementCount < io_libecc_object_ElementMax)
        return self->elementCount;
    else
        return io_libecc_object_ElementMax;
}

void eccobject_readonlyError(ecccontext_t* context, eccvalue_t* ref, eccobject_t* thisobj)
{
    ecctextstring_t text;

    do
    {
        ecchashmap_t* hashmap = (ecchashmap_t*)ref;
        ecchashitem_t* element = (ecchashitem_t*)ref;

        if(hashmap >= thisobj->hashmap && hashmap < thisobj->hashmap + thisobj->hashmapCount)
        {
            const ecctextstring_t* keyText = ecc_keyidx_textof(hashmap->hmapmapvalue.key);
            ecc_context_typeerror(context, ecc_strbuf_create("'%.*s' is read-only", keyText->length, keyText->bytes));
        }
        else if(element >= thisobj->element && element < thisobj->element + thisobj->elementCount)
            ecc_context_typeerror(context, ecc_strbuf_create("'%u' is read-only", element - thisobj->element));

    } while((thisobj = thisobj->prototype));

    text = ecc_context_textseek(context);
    ecc_context_typeerror(context, ecc_strbuf_create("'%.*s' is read-only", text.length, text.bytes));
}

//

eccobject_t* eccobject_checkObject(ecccontext_t* context, int argument)
{
    eccvalue_t value = ecc_context_argument(context, argument);
    if(!ecc_value_isobject(value))
        ecc_context_typeerror(context, ecc_strbuf_create("not an object"));

    return value.data.object;
}

eccvalue_t objobjectfn_valueOf(ecccontext_t* context)
{
    return ecc_value_toobject(context, ecc_context_this(context));
}

eccvalue_t objobjectfn_hasOwnProperty(ecccontext_t* context)
{
    eccobject_t* self;
    eccvalue_t value;
    eccindexkey_t key;
    uint32_t index;

    self = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    value = ecc_value_toprimitive(context, ecc_context_argument(context, 0), ECC_VALHINT_STRING);
    index = eccobject_getIndexOrKey(value, &key);

    if(index < UINT32_MAX)
        return ecc_value_truth(ecc_object_element(self, index, ECC_VALFLAG_ASOWN) != NULL);
    else
        return ecc_value_truth(ecc_object_member(self, key, ECC_VALFLAG_ASOWN) != NULL);
}

eccvalue_t objobjectfn_isPrototypeOf(ecccontext_t* context)
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

eccvalue_t objobjectfn_propertyIsEnumerable(ecccontext_t* context)
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

eccvalue_t objobjectfn_constructor(ecccontext_t* context)
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

eccvalue_t objobjectfn_getPrototypeOf(ecccontext_t* context)
{
    eccobject_t* object;

    object = ecc_value_toobject(context, ecc_context_argument(context, 0)).data.object;

    return object->prototype ? ecc_value_objectvalue(object->prototype) : ECCValConstUndefined;
}

eccvalue_t objobjectfn_getOwnPropertyDescriptor(ecccontext_t* context)
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

eccvalue_t objobjectfn_getOwnPropertyNames(ecccontext_t* context)
{
    eccobject_t *object, *parent;
    eccobject_t* result;
    uint32_t index, count, length;

    object = eccobject_checkObject(context, 0);
    result = ecc_array_create();
    length = 0;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].hmapitemvalue.check == 1)
            ecc_object_addelement(result, length++, ecc_value_fromchars(ecc_strbuf_create("%d", index)), 0);

    parent = object;
    while((parent = parent->prototype))
    {
        for(index = 2; index < parent->hashmapCount; ++index)
        {
            eccvalue_t value = parent->hashmap[index].hmapmapvalue;
            if(value.check == 1 && value.flags & ECC_VALFLAG_ASOWN)
                ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(value.key)), 0);
        }
    }

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].hmapmapvalue.check == 1)
            ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(object->hashmap[index].hmapmapvalue.key)), 0);

    return ecc_value_object(result);
}

eccvalue_t objobjectfn_defineProperty(ecccontext_t* context)
{
    eccobject_t *object, *descriptor;
    eccvalue_t property, value, *getter, *setter, *current, *flag;
    eccindexkey_t key;
    uint32_t index;

    object = eccobject_checkObject(context, 0);
    property = ecc_value_toprimitive(context, ecc_context_argument(context, 1), ECC_VALHINT_STRING);
    descriptor = eccobject_checkObject(context, 2);

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
    index = eccobject_getIndexOrKey(property, &key);
    if(index == UINT32_MAX)
    {
        const ecctextstring_t* text = ecc_keyidx_textof(key);
        ecc_context_typeerror(context, ecc_strbuf_create("'%.*s' is non-configurable", text->length, text->bytes));
    }
    else
        ecc_context_typeerror(context, ecc_strbuf_create("'%u' is non-configurable", index));
    return ECCValConstUndefined;
}

eccvalue_t objobjectfn_defineProperties(ecccontext_t* context)
{
    ecchashmap_t* originalHashmap = context->environment->hashmap;
    uint16_t originalHashmapCount = context->environment->hashmapCount;

    uint16_t index, count, hashmapCount = 6;
    eccobject_t *object, *properties;
    ecchashmap_t hashmap[hashmapCount];

    memset(hashmap, 0, hashmapCount * sizeof(*hashmap));

    object = eccobject_checkObject(context, 0);
    properties = ecc_value_toobject(context, ecc_context_argument(context, 1)).data.object;

    context->environment->hashmap = hashmap;
    context->environment->hashmapCount = hashmapCount;

    ecc_context_replaceargument(context, 0, ecc_value_object(object));

    for(index = 0, count = eccobject_elementCount(properties); index < count; ++index)
    {
        if(!properties->element[index].hmapitemvalue.check)
            continue;

        ecc_context_replaceargument(context, 1, ecc_value_fromfloat(index));
        ecc_context_replaceargument(context, 2, properties->element[index].hmapitemvalue);
        objobjectfn_defineProperty(context);
    }

    for(index = 2; index < properties->hashmapCount; ++index)
    {
        if(!properties->hashmap[index].hmapmapvalue.check)
            continue;

        ecc_context_replaceargument(context, 1, ecc_value_fromkey(properties->hashmap[index].hmapmapvalue.key));
        ecc_context_replaceargument(context, 2, properties->hashmap[index].hmapmapvalue);
        objobjectfn_defineProperty(context);
    }

    context->environment->hashmap = originalHashmap;
    context->environment->hashmapCount = originalHashmapCount;

    return ECCValConstUndefined;
}

eccvalue_t objobjectfn_objectCreate(ecccontext_t* context)
{
    eccobject_t *object, *result;
    eccvalue_t properties;

    object = eccobject_checkObject(context, 0);
    properties = ecc_context_argument(context, 1);

    result = ecc_object_create(object);
    if(properties.type != ECC_VALTYPE_UNDEFINED)
    {
        ecc_context_replaceargument(context, 0, ecc_value_object(result));
        objobjectfn_defineProperties(context);
    }

    return ecc_value_object(result);
}

eccvalue_t objobjectfn_seal(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].hmapitemvalue.check == 1)
            object->element[index].hmapitemvalue.flags |= ECC_VALFLAG_SEALED;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].hmapmapvalue.check == 1)
            object->hashmap[index].hmapmapvalue.flags |= ECC_VALFLAG_SEALED;

    return ecc_value_object(object);
}

eccvalue_t objobjectfn_freeze(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].hmapitemvalue.check == 1)
            object->element[index].hmapitemvalue.flags |= ECC_VALFLAG_FROZEN;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].hmapmapvalue.check == 1)
            object->hashmap[index].hmapmapvalue.flags |= ECC_VALFLAG_FROZEN;

    return ecc_value_object(object);
}

eccvalue_t objobjectfn_preventExtensions(ecccontext_t* context)
{
    eccobject_t* object;

    object = eccobject_checkObject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    return ecc_value_object(object);
}

eccvalue_t objobjectfn_isSealed(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    if(!(object->flags & ECC_OBJFLAG_SEALED))
        return ECCValConstFalse;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].hmapitemvalue.check == 1 && !(object->element[index].hmapitemvalue.flags & ECC_VALFLAG_SEALED))
            return ECCValConstFalse;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].hmapmapvalue.check == 1 && !(object->hashmap[index].hmapmapvalue.flags & ECC_VALFLAG_SEALED))
            return ECCValConstFalse;

    return ECCValConstTrue;
}

eccvalue_t objobjectfn_isFrozen(ecccontext_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    if(!(object->flags & ECC_OBJFLAG_SEALED))
        return ECCValConstFalse;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].hmapitemvalue.check == 1 && !(object->element[index].hmapitemvalue.flags & ECC_VALFLAG_FROZEN))
            return ECCValConstFalse;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].hmapmapvalue.check == 1 && !(object->hashmap[index].hmapmapvalue.flags & ECC_VALFLAG_FROZEN))
            return ECCValConstFalse;

    return ECCValConstTrue;
}

eccvalue_t objobjectfn_isExtensible(ecccontext_t* context)
{
    eccobject_t* object;

    object = eccobject_checkObject(context, 0);
    return ecc_value_truth(!(object->flags & ECC_OBJFLAG_SEALED));
}

eccvalue_t objobjectfn_keys(ecccontext_t* context)
{
    eccobject_t *object, *parent;
    eccobject_t* result;
    uint32_t index, count, length;

    object = eccobject_checkObject(context, 0);
    result = ecc_array_create();
    length = 0;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].hmapitemvalue.check == 1 && !(object->element[index].hmapitemvalue.flags & ECC_VALFLAG_HIDDEN))
            ecc_object_addelement(result, length++, ecc_value_fromchars(ecc_strbuf_create("%d", index)), 0);

    parent = object;
    while((parent = parent->prototype))
    {
        for(index = 2; index < parent->hashmapCount; ++index)
        {
            eccvalue_t value = parent->hashmap[index].hmapmapvalue;
            if(value.check == 1 && value.flags & ECC_VALFLAG_ASOWN & !(value.flags & ECC_VALFLAG_HIDDEN))
                ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(value.key)), 0);
        }
    }

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].hmapmapvalue.check == 1 && !(object->hashmap[index].hmapmapvalue.flags & ECC_VALFLAG_HIDDEN))
            ecc_object_addelement(result, length++, ecc_value_fromtext(ecc_keyidx_textof(object->hashmap[index].hmapmapvalue.key)), 0);

    return ecc_value_object(result);
}

// MARK: - Methods

void ecc_object_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    assert(sizeof(*ECC_Prototype_Object->hashmap) == 32);

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Object, objobjectfn_constructor, 1, NULL, ecc_value_object(ECC_Prototype_Object), NULL);

    ecc_function_addmethod(ECC_CtorFunc_Object, "getPrototypeOf", objobjectfn_getPrototypeOf, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "getOwnPropertyDescriptor", objobjectfn_getOwnPropertyDescriptor, 2, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "getOwnPropertyNames", objobjectfn_getOwnPropertyNames, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "create", objobjectfn_objectCreate, 2, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "defineProperty", objobjectfn_defineProperty, 3, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "defineProperties", objobjectfn_defineProperties, 2, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "seal", objobjectfn_seal, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "freeze", objobjectfn_freeze, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "preventExtensions", objobjectfn_preventExtensions, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "isSealed", objobjectfn_isSealed, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "isFrozen", objobjectfn_isFrozen, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "isExtensible", objobjectfn_isExtensible, 1, h);
    ecc_function_addmethod(ECC_CtorFunc_Object, "keys", objobjectfn_keys, 1, h);

    ecc_function_addto(ECC_Prototype_Object, "toString", ecc_object_tostringfn, 0, h);
    ecc_function_addto(ECC_Prototype_Object, "toLocaleString", ecc_object_tostringfn, 0, h);
    ecc_function_addto(ECC_Prototype_Object, "valueOf", objobjectfn_valueOf, 0, h);
    ecc_function_addto(ECC_Prototype_Object, "hasOwnProperty", objobjectfn_hasOwnProperty, 1, h);
    ecc_function_addto(ECC_Prototype_Object, "isPrototypeOf", objobjectfn_isPrototypeOf, 1, h);
    ecc_function_addto(ECC_Prototype_Object, "propertyIsEnumerable", objobjectfn_propertyIsEnumerable, 1, h);
}

void ecc_object_teardown(void)
{
    ECC_Prototype_Object = NULL;
    ECC_CtorFunc_Object = NULL;
}

eccobject_t* ecc_object_create(eccobject_t* prototype)
{
    return ecc_object_createsized(prototype, defaultSize);
}

eccobject_t* ecc_object_createsized(eccobject_t* prototype, uint16_t size)
{
    eccobject_t* self;
    self = (eccobject_t*)calloc(sizeof(*self), 1);
    ecc_mempool_addobject(self);
    return ecc_object_initializesized(self, prototype, size);
}

eccobject_t* ecc_object_createtyped(const eccobjinterntype_t* type)
{
    eccobject_t* self = ecc_object_createsized(ECC_Prototype_Object, defaultSize);
    self->type = type;
    return self;
}

eccobject_t* ecc_object_initialize(eccobject_t* self, eccobject_t* prototype)
{
    return ecc_object_initializesized(self, prototype, defaultSize);
}

eccobject_t* ecc_object_initializesized(eccobject_t* self, eccobject_t* prototype, uint16_t size)
{
    size_t byteSize;
    assert(self);
    memset(self, 0, sizeof(eccobject_t));
    self->type = prototype ? prototype->type : &ECC_Type_Object;

    self->prototype = prototype;
    self->hashmapCount = 2;

    // hashmap is always 2 slots minimum
    // slot 0 is self referencing undefined value (all zeroes)
    // slot 1 is entry point, referencing undefined (slot 0) by default (all zeroes)

    if(size > 0)
    {
        self->hashmapCapacity = size;

        byteSize = sizeof(*self->hashmap) * self->hashmapCapacity;
        self->hashmap = (ecchashmap_t*)malloc(byteSize);
        memset(self->hashmap, 0, byteSize);
    }
    else
        // size is == zero = to be initialized later by caller
        self->hashmapCapacity = 2;

    return self;
}

eccobject_t* ecc_object_finalize(eccobject_t* self)
{
    assert(self);

    if(self->type->finalize)
        self->type->finalize(self);

    free(self->hashmap), self->hashmap = NULL;
    free(self->element), self->element = NULL;

    return self;
}

eccobject_t* ecc_object_copy(const eccobject_t* original)
{
    size_t byteSize;

    eccobject_t* self;
    self = (eccobject_t*)malloc(sizeof(*self));
    ecc_mempool_addobject(self);

    *self = *original;

    byteSize = sizeof(*self->element) * self->elementCount;
    self->element = (ecchashitem_t*)malloc(byteSize);
    memcpy(self->element, original->element, byteSize);

    byteSize = sizeof(*self->hashmap) * self->hashmapCount;
    self->hashmap = (ecchashmap_t*)malloc(byteSize);
    memcpy(self->hashmap, original->hashmap, byteSize);

    return self;
}

void ecc_object_destroy(eccobject_t* self)
{
    assert(self);

    free(self), self = NULL;
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
        if((slot = eccobject_getSlot(object, member)))
        {
            ref = &object->hashmap[slot].hmapmapvalue;
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
    else if(index > io_libecc_object_ElementMax)
    {
        eccindexkey_t key = eccobject_keyOfIndex(index, 0);
        if(key.data.integer)
            return ecc_object_member(self, key, flags);
    }
    else
        do
        {
            if(index < object->elementCount)
            {
                ref = &object->element[index].hmapitemvalue;
                if(ref->check == 1)
                    return lookupChain || object == self || (ref->flags & flags) ? ref : NULL;
            }
        } while((object = object->prototype));

    return NULL;
}

eccvalue_t* ecc_object_property(eccobject_t* self, eccvalue_t property, int flags)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(property, &key);

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
    uint32_t index = eccobject_getIndexOrKey(property, &key);

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
        else if(context->strictMode || (context->parent && context->parent->strictMode))
            eccobject_readonlyError(context, ref, self);

        return value;
    }

    if(ref->check == 1)
    {
        if(ref->flags & ECC_VALFLAG_READONLY)
        {
            if(context->strictMode)
                eccobject_readonlyError(context, ref, self);
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

    if(index > io_libecc_object_ElementMax)
    {
        if(self->elementCapacity <= index)
            ecc_object_resizeelement(self, index < UINT32_MAX ? index + 1 : index);
        else if(self->elementCount <= index)
            self->elementCount = index + 1;

        return ecc_object_putmember(context, self, eccobject_keyOfIndex(index, 1), value);
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
    uint32_t index = eccobject_getIndexOrKey(primitive, &key);

    if(index < UINT32_MAX)
        return ecc_object_putelement(context, self, index, value);
    else
        return ecc_object_putmember(context, self, key, value);
}

eccvalue_t* ecc_object_addmember(eccobject_t* self, eccindexkey_t key, eccvalue_t value, int flags)
{
    uint32_t slot = 1;
    int depth = 0;

    assert(self);

    do
    {
        if(!self->hashmap[slot].slot[key.data.depth[depth]])
        {
            int need = 4 - depth - (self->hashmapCapacity - self->hashmapCount);
            if(need > 0)
            {
                uint16_t capacity = self->hashmapCapacity;
                self->hashmapCapacity = self->hashmapCapacity ? self->hashmapCapacity * 2 : 2;
                self->hashmap = (ecchashmap_t*)realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);
                memset(self->hashmap + capacity, 0, sizeof(*self->hashmap) * (self->hashmapCapacity - capacity));
            }

            do
            {
                assert(self->hashmapCount < UINT16_MAX);
                slot = self->hashmap[slot].slot[key.data.depth[depth]] = self->hashmapCount++;
            } while(++depth < 4);
            break;
        }
        else
            assert(self->hashmap[slot].hmapmapvalue.check != 1);

        slot = self->hashmap[slot].slot[key.data.depth[depth]];
        assert(slot != 1);
        assert(slot < self->hashmapCount);
    } while(++depth < 4);

    if(value.flags & ECC_VALFLAG_ACCESSOR)
        if(self->hashmap[slot].hmapmapvalue.check == 1 && self->hashmap[slot].hmapmapvalue.flags & ECC_VALFLAG_ACCESSOR)
            if((self->hashmap[slot].hmapmapvalue.flags & ECC_VALFLAG_ACCESSOR) != (value.flags & ECC_VALFLAG_ACCESSOR))
                value.data.function->pair = self->hashmap[slot].hmapmapvalue.data.function;

    value.key = key;
    value.flags |= flags;

    self->hashmap[slot].hmapmapvalue = value;

    return &self->hashmap[slot].hmapmapvalue;
}

eccvalue_t* ecc_object_addelement(eccobject_t* self, uint32_t index, eccvalue_t value, int flags)
{
    eccvalue_t* ref;

    assert(self);

    if(self->elementCapacity <= index)
        ecc_object_resizeelement(self, index < UINT32_MAX ? index + 1 : index);
    else if(self->elementCount <= index)
        self->elementCount = index + 1;

    if(index > io_libecc_object_ElementMax)
        return ecc_object_addmember(self, eccobject_keyOfIndex(index, 1), value, flags);

    ref = &self->element[index].hmapitemvalue;

    value.flags |= flags;
    *ref = value;

    return ref;
}

eccvalue_t* ecc_object_addproperty(eccobject_t* self, eccvalue_t primitive, eccvalue_t value, int flags)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(primitive, &key);

    if(index < UINT32_MAX)
        return ecc_object_addelement(self, index, value, flags);
    else
        return ecc_object_addmember(self, key, value, flags);
}

int ecc_object_deletemember(eccobject_t* self, eccindexkey_t member)
{
    eccobject_t* object = self;
    uint32_t slot, refSlot;

    assert(object);
    assert(member.data.integer);

    refSlot = self->hashmap[self->hashmap[self->hashmap[1].slot[member.data.depth[0]]].slot[member.data.depth[1]]].slot[member.data.depth[2]];

    slot = self->hashmap[refSlot].slot[member.data.depth[3]];

    if(!slot || !(object->hashmap[slot].hmapmapvalue.check == 1))
        return 1;

    if(object->hashmap[slot].hmapmapvalue.flags & ECC_VALFLAG_SEALED)
        return 0;

    object->hashmap[slot].hmapmapvalue = ECCValConstUndefined;
    self->hashmap[refSlot].slot[member.data.depth[3]] = 0;
    return 1;
}

int ecc_object_deleteelement(eccobject_t* self, uint32_t index)
{
    assert(self);

    if(index > io_libecc_object_ElementMax)
    {
        eccindexkey_t key = eccobject_keyOfIndex(index, 0);
        if(key.data.integer)
            return ecc_object_deletemember(self, key);
        else
            return 1;
    }

    if(index < self->elementCount)
    {
        if(self->element[index].hmapitemvalue.flags & ECC_VALFLAG_SEALED)
            return 0;

        memset(&self->element[index], 0, sizeof(*self->element));
    }

    return 1;
}

int ecc_object_deleteproperty(eccobject_t* self, eccvalue_t primitive)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(primitive, &key);

    if(index < UINT32_MAX)
        return ecc_object_deleteelement(self, index);
    else
        return ecc_object_deletemember(self, key);
}

void ecc_object_packvalue(eccobject_t* self)
{
    ecchashmap_t data;
    uint32_t index = 2, valueIndex = 2, copyIndex, slot;

    assert(self);

    for(; index < self->hashmapCount; ++index)
    {
        if(self->hashmap[index].hmapmapvalue.check == 1)
        {
            data = self->hashmap[index];
            for(copyIndex = index; copyIndex > valueIndex; --copyIndex)
            {
                self->hashmap[copyIndex] = self->hashmap[copyIndex - 1];
                if(!(self->hashmap[copyIndex].hmapmapvalue.check == 1))
                {
                    for(slot = 0; slot < 16; ++slot)
                    {
                        if(self->hashmap[copyIndex].slot[slot] == index)
                        {
                            self->hashmap[copyIndex].slot[slot] = valueIndex;
                        }
                        else if(self->hashmap[copyIndex].slot[slot] >= valueIndex && self->hashmap[copyIndex].slot[slot] < index)
                        {
                            self->hashmap[copyIndex].slot[slot]++;
                        }
                    }
                }
            }
            for(slot = 0; slot < 16; ++slot)
            {
                if(self->hashmap[1].slot[slot] >= valueIndex && self->hashmap[1].slot[slot] < index)
                {
                    self->hashmap[1].slot[slot]++;
                }
            }
            self->hashmap[valueIndex++] = data;
        }
    }
    self->hashmap = (ecchashmap_t*)realloc(self->hashmap, sizeof(*self->hashmap) * (self->hashmapCount));
    self->hashmapCapacity = self->hashmapCount;

    if(self->elementCount)
    {
        self->elementCapacity = self->elementCount;
    }
}

void ecc_object_stripmap(eccobject_t* self)
{
    uint32_t index = 2;

    assert(self);

    while(index < self->hashmapCount && self->hashmap[index].hmapmapvalue.check == 1)
        ++index;

    self->hashmapCapacity = self->hashmapCount = index;
    self->hashmap = (ecchashmap_t*)realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);

    memset(self->hashmap + 1, 0, sizeof(*self->hashmap));
}

void ecc_object_reserveslots(eccobject_t* self, uint16_t slots)
{
    int need = (slots * 4) - (self->hashmapCapacity - self->hashmapCount);

    assert(slots < self->hashmapCapacity);

    if(need > 0)
    {
        uint16_t capacity = self->hashmapCapacity;
        self->hashmapCapacity = self->hashmapCapacity ? self->hashmapCapacity * 2 : 2;
        self->hashmap = (ecchashmap_t*)realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);
        memset(self->hashmap + capacity, 0, sizeof(*self->hashmap) * (self->hashmapCapacity - capacity));
    }
}

int ecc_object_resizeelement(eccobject_t* self, uint32_t size)
{
    uint32_t capacity;

    if(size <= self->elementCapacity)
        capacity = self->elementCapacity;
    else if(size < 4)
    {
        /* 64-bytes mini */
        capacity = 4;
    }
    else if(size < 64)
    {
        /* power of two steps between */
        capacity = eccobject_nextPowerOfTwo(size);
    }
    else if(size > io_libecc_object_ElementMax)
        capacity = io_libecc_object_ElementMax + 1;
    else
    {
        /* 1024-bytes chunk */
        capacity = size - 1;
        capacity |= 63;
        ++capacity;
    }

    assert(self);

    if(capacity != self->elementCapacity)
    {
        if(size > io_libecc_object_ElementMax)
        {
            ecc_env_printwarning("Faking array length of %u while actual physical length is %u. Using array length > 0x%x is discouraged", size, capacity,
                                  io_libecc_object_ElementMax);
        }

        self->element = (ecchashitem_t*)realloc(self->element, sizeof(*self->element) * capacity);
        if(capacity > self->elementCapacity)
            memset(self->element + self->elementCapacity, 0, sizeof(*self->element) * (capacity - self->elementCapacity));

        self->elementCapacity = capacity;
    }
    else if(size < self->elementCount)
    {
        ecchashitem_t* element;
        uint32_t until = size, e;

        if(self->elementCount > io_libecc_object_ElementMax)
        {
            ecchashmap_t* hashmap;
            uint32_t index, h;

            for(h = 2; h < self->hashmapCount; ++h)
            {
                hashmap = &self->hashmap[h];
                if(hashmap->hmapmapvalue.check == 1)
                {
                    index = ecc_astlex_scanelement(*ecc_keyidx_textof(hashmap->hmapmapvalue.key));
                    if(hashmap->hmapmapvalue.check == 1 && (hashmap->hmapmapvalue.flags & ECC_VALFLAG_SEALED) && index >= until)
                        until = index + 1;
                }
            }

            for(h = 2; h < self->hashmapCount; ++h)
            {
                hashmap = &self->hashmap[h];
                if(hashmap->hmapmapvalue.check == 1)
                    if(ecc_astlex_scanelement(*ecc_keyidx_textof(hashmap->hmapmapvalue.key)) >= until)
                        self->hashmap[h].hmapmapvalue.check = 0;
            }

            if(until > size)
            {
                self->elementCount = until;
                return 1;
            }
            self->elementCount = self->elementCapacity;
        }

        for(e = size; e < self->elementCount; ++e)
        {
            element = &self->element[e];
            if(element->hmapitemvalue.check == 1 && (element->hmapitemvalue.flags & ECC_VALFLAG_SEALED) && e >= until)
                until = e + 1;
        }

        memset(self->element + until, 0, sizeof(*self->element) * (self->elementCount - until));

        if(until > size)
        {
            self->elementCount = until;
            return 1;
        }
    }
    self->elementCount = size;

    return 0;
}

void ecc_object_populateelementwithclist(eccobject_t* self, uint32_t count, const char* list[])
{
    double binary;
    char* end;
    int index;

    assert(self);
    assert(count <= io_libecc_object_ElementMax);

    if(count > self->elementCount)
        ecc_object_resizeelement(self, count);

    for(index = 0; index < (int)count; ++index)
    {
        uint16_t length = (uint16_t)strlen(list[index]);
        binary = strtod(list[index], &end);

        if(end == list[index] + length)
            self->element[index].hmapitemvalue = ecc_value_fromfloat(binary);
        else
        {
            eccstrbuffer_t* chars = ecc_strbuf_createsized(length);
            memcpy(chars->bytes, list[index], length);

            self->element[index].hmapitemvalue = ecc_value_fromchars(chars);
        }
    }
}

eccvalue_t ecc_object_tostringfn(ecccontext_t* context)
{
    if(context->thisvalue.type == ECC_VALTYPE_NULL)
        return ecc_value_fromtext(&ECC_ConstString_NullType);
    else if(context->thisvalue.type == ECC_VALTYPE_UNDEFINED)
        return ecc_value_fromtext(&ECC_ConstString_UndefinedType);
    else if(ecc_value_isstring(context->thisvalue))
        return ecc_value_fromtext(&ECC_ConstString_StringType);
    else if(ecc_value_isnumber(context->thisvalue))
        return ecc_value_fromtext(&ECC_ConstString_NumberType);
    else if(ecc_value_isboolean(context->thisvalue))
        return ecc_value_fromtext(&ECC_ConstString_BooleanType);
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

    for(index = 0, count = eccobject_elementCount(self); index < count; ++index)
    {
        if(self->element[index].hmapitemvalue.check == 1)
        {
            if(!isArray)
                fprintf(file, "%d: ", (int)index);

            if(self->element[index].hmapitemvalue.type == ECC_VALTYPE_OBJECT && self->element[index].hmapitemvalue.data.object == self)
                fprintf(file, "this");
            else
                ecc_value_dumpto(self->element[index].hmapitemvalue, file);

            fprintf(file, ", ");
        }
    }

    if(!isArray)
    {
        for(index = 0; index < self->hashmapCount; ++index)
        {
            if(self->hashmap[index].hmapmapvalue.check == 1)
            {
                fprintf(stderr, "'");
                ecc_keyidx_dumpto(self->hashmap[index].hmapmapvalue.key, file);
                fprintf(file, "': ");

                if(self->hashmap[index].hmapmapvalue.type == ECC_VALTYPE_OBJECT && self->hashmap[index].hmapmapvalue.data.object == self)
                    fprintf(file, "this");
                else
                    ecc_value_dumpto(self->hashmap[index].hmapmapvalue, file);

                fprintf(file, ", ");
            }
            //			else
            //			{
            //				fprintf(file, "\n");
            //				for (int j = 0; j < 16; ++j)
            //					fprintf(file, "%02x ", self->hashmap[index].slot[j]);
            //			}
        }
    }

    fprintf(file, isArray ? "]" : "}");
}
