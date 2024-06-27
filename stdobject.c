//
//  object.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static void objnsfn_setup(void);
static void objnsfn_teardown(void);
static eccobject_t* objnsfn_create(eccobject_t* prototype);
static eccobject_t* objnsfn_createSized(eccobject_t* prototype, uint16_t size);
static eccobject_t* objnsfn_createTyped(const eccobjinterntype_t* type);
static eccobject_t* objnsfn_initialize(eccobject_t* restrict, eccobject_t* restrict prototype);
static eccobject_t* objnsfn_initializeSized(eccobject_t* restrict, eccobject_t* restrict prototype, uint16_t size);
static eccobject_t* objnsfn_finalize(eccobject_t*);
static eccobject_t* objnsfn_copy(const eccobject_t* original);
static void objnsfn_destroy(eccobject_t*);
static eccvalue_t objnsfn_getMember(eccstate_t*, eccobject_t*, eccindexkey_t key);
static eccvalue_t objnsfn_putMember(eccstate_t*, eccobject_t*, eccindexkey_t key, eccvalue_t);
static eccvalue_t* objnsfn_member(eccobject_t*, eccindexkey_t key, eccvalflag_t);
static eccvalue_t* objnsfn_addMember(eccobject_t*, eccindexkey_t key, eccvalue_t, eccvalflag_t);
static int objnsfn_deleteMember(eccobject_t*, eccindexkey_t key);
static eccvalue_t objnsfn_getElement(eccstate_t*, eccobject_t*, uint32_t index);
static eccvalue_t objnsfn_putElement(eccstate_t*, eccobject_t*, uint32_t index, eccvalue_t);
static eccvalue_t* objnsfn_element(eccobject_t*, uint32_t index, eccvalflag_t);
static eccvalue_t* objnsfn_addElement(eccobject_t*, uint32_t index, eccvalue_t, eccvalflag_t);
static int objnsfn_deleteElement(eccobject_t*, uint32_t index);
static eccvalue_t objnsfn_getProperty(eccstate_t*, eccobject_t*, eccvalue_t primitive);
static eccvalue_t objnsfn_putProperty(eccstate_t*, eccobject_t*, eccvalue_t primitive, eccvalue_t);
static eccvalue_t* objnsfn_property(eccobject_t*, eccvalue_t primitive, eccvalflag_t);
static eccvalue_t* objnsfn_addProperty(eccobject_t*, eccvalue_t primitive, eccvalue_t, eccvalflag_t);
static int objnsfn_deleteProperty(eccobject_t*, eccvalue_t primitive);
static eccvalue_t objnsfn_putValue(eccstate_t*, eccobject_t*, eccvalue_t*, eccvalue_t);
static eccvalue_t objnsfn_getValue(eccstate_t*, eccobject_t*, eccvalue_t*);
static void objnsfn_packValue(eccobject_t*);
static void objnsfn_stripMap(eccobject_t*);
static void objnsfn_reserveSlots(eccobject_t*, uint16_t slots);
static int objnsfn_resizeElement(eccobject_t*, uint32_t size);
static void objnsfn_populateElementWithCList(eccobject_t*, uint32_t count, const char* list[]);
static eccvalue_t objobjectfn_toString(eccstate_t*);
static void objnsfn_dumpTo(eccobject_t*, FILE* file);
const struct eccpseudonsobject_t ECCNSObject = {
    objnsfn_setup,
    objnsfn_teardown,
    objnsfn_create,
    objnsfn_createSized,
    objnsfn_createTyped,
    objnsfn_initialize,
    objnsfn_initializeSized,
    objnsfn_finalize,
    objnsfn_copy,
    objnsfn_destroy,
    objnsfn_getMember,
    objnsfn_putMember,
    objnsfn_member,
    objnsfn_addMember,
    objnsfn_deleteMember,
    objnsfn_getElement,
    objnsfn_putElement,
    objnsfn_element,
    objnsfn_addElement,
    objnsfn_deleteElement,
    objnsfn_getProperty,
    objnsfn_putProperty,
    objnsfn_property,
    objnsfn_addProperty,
    objnsfn_deleteProperty,
    objnsfn_putValue,
    objnsfn_getValue,
    objnsfn_packValue,
    objnsfn_stripMap,
    objnsfn_reserveSlots,
    objnsfn_resizeElement,
    objnsfn_populateElementWithCList,
    objobjectfn_toString,
    objnsfn_dumpTo,
    {},
};

const uint32_t io_libecc_object_ElementMax = 0xffffff;

static const int defaultSize = 8;

eccobject_t* ECC_Prototype_Object = NULL;
eccobjfunction_t* ECC_CtorFunc_Object = NULL;

const eccobjinterntype_t ECC_Type_Object = {
    .text = &ECC_ConstString_ObjectType,
};

// MARK: - Static Members

static uint16_t eccobject_getSlot(const eccobject_t* const self, const eccindexkey_t key)
{
    return self->hashmap[self->hashmap[self->hashmap[self->hashmap[1].slot[key.data.depth[0]]].slot[key.data.depth[1]]].slot[key.data.depth[2]]].slot[key.data.depth[3]];
}

static uint32_t eccobject_getIndexOrKey(eccvalue_t property, eccindexkey_t* key)
{
    bool isbin;
    bool issame;
    bool abovezero;
    bool belomax;
    uint32_t index;
    index = UINT32_MAX;
    assert(ECCNSValue.isPrimitive(property));
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
            else if(ECCNSValue.isString(property))
            {
                ecctextstring_t text = ECCNSValue.textOf(&property);
                if((index = ECCNSLexer.scanElement(text)) == UINT32_MAX)
                    *key = ECCNSKey.makeWithText(text, ECC_INDEXFLAG_COPYONCREATE);
            }
            else
                return eccobject_getIndexOrKey(ECCNSValue.toString(NULL, property), key);
        }
    }

    return index;
}

static eccindexkey_t eccobject_keyOfIndex(uint32_t index, int create)
{
    char buffer[10 + 1];
    uint16_t length;

    length = snprintf(buffer, sizeof(buffer), "%u", (unsigned)index);
    if(create)
        return ECCNSKey.makeWithText(ECCNSText.make(buffer, length), ECC_INDEXFLAG_COPYONCREATE);
    else
        return ECCNSKey.search(ECCNSText.make(buffer, length));
}

static uint32_t eccobject_nextPowerOfTwo(uint32_t v)
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

static uint32_t eccobject_elementCount(eccobject_t* self)
{
    if(self->elementCount < io_libecc_object_ElementMax)
        return self->elementCount;
    else
        return io_libecc_object_ElementMax;
}

static void eccobject_readonlyError(eccstate_t* context, eccvalue_t* ref, eccobject_t* thisobj)
{
    ecctextstring_t text;

    do
    {
        ecchashmap_t* hashmap = (ecchashmap_t*)ref;
        eccobjelement_t* element = (eccobjelement_t*)ref;

        if(hashmap >= thisobj->hashmap && hashmap < thisobj->hashmap + thisobj->hashmapCount)
        {
            const ecctextstring_t* keyText = ECCNSKey.textOf(hashmap->value.key);
            ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is read-only", keyText->length, keyText->bytes));
        }
        else if(element >= thisobj->element && element < thisobj->element + thisobj->elementCount)
            ECCNSContext.typeError(context, ECCNSChars.create("'%u' is read-only", element - thisobj->element));

    } while((thisobj = thisobj->prototype));

    text = ECCNSContext.textSeek(context);
    ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is read-only", text.length, text.bytes));
}

//

static eccobject_t* eccobject_checkObject(eccstate_t* context, int argument)
{
    eccvalue_t value = ECCNSContext.argument(context, argument);
    if(!ECCNSValue.isObject(value))
        ECCNSContext.typeError(context, ECCNSChars.create("not an object"));

    return value.data.object;
}

static eccvalue_t objobjectfn_valueOf(eccstate_t* context)
{
    return ECCNSValue.toObject(context, ECCNSContext.getThis(context));
}

static eccvalue_t objobjectfn_hasOwnProperty(eccstate_t* context)
{
    eccobject_t* self;
    eccvalue_t value;
    eccindexkey_t key;
    uint32_t index;

    self = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    value = ECCNSValue.toPrimitive(context, ECCNSContext.argument(context, 0), ECC_VALHINT_STRING);
    index = eccobject_getIndexOrKey(value, &key);

    if(index < UINT32_MAX)
        return ECCNSValue.truth(objnsfn_element(self, index, ECC_VALFLAG_ASOWN) != NULL);
    else
        return ECCNSValue.truth(objnsfn_member(self, key, ECC_VALFLAG_ASOWN) != NULL);
}

static eccvalue_t objobjectfn_isPrototypeOf(eccstate_t* context)
{
    eccvalue_t arg0;

    arg0 = ECCNSContext.argument(context, 0);

    if(ECCNSValue.isObject(arg0))
    {
        eccobject_t* v = arg0.data.object;
        eccobject_t* o = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;

        do
            if(v == o)
                return ECCValConstTrue;
        while((v = v->prototype));
    }

    return ECCValConstFalse;
}

static eccvalue_t objobjectfn_propertyIsEnumerable(eccstate_t* context)
{
    eccvalue_t value;
    eccobject_t* object;
    eccvalue_t* ref;

    value = ECCNSValue.toPrimitive(context, ECCNSContext.argument(context, 0), ECC_VALHINT_STRING);
    object = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    ref = objnsfn_property(object, value, ECC_VALFLAG_ASOWN);

    if(ref)
        return ECCNSValue.truth(!(ref->flags & ECC_VALFLAG_HIDDEN));
    else
        return ECCValConstFalse;
}

static eccvalue_t objobjectfn_constructor(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);

    if(value.type == ECC_VALTYPE_NULL || value.type == ECC_VALTYPE_UNDEFINED)
        return ECCNSValue.object(objnsfn_create(ECC_Prototype_Object));
    else if(context->construct && ECCNSValue.isObject(value))
        return value;
    else
        return ECCNSValue.toObject(context, value);
}

static eccvalue_t objobjectfn_getPrototypeOf(eccstate_t* context)
{
    eccobject_t* object;

    object = ECCNSValue.toObject(context, ECCNSContext.argument(context, 0)).data.object;

    return object->prototype ? ECCNSValue.objectValue(object->prototype) : ECCValConstUndefined;
}

static eccvalue_t objobjectfn_getOwnPropertyDescriptor(eccstate_t* context)
{
    eccobject_t* object;
    eccvalue_t value;
    eccvalue_t* ref;

    object = ECCNSValue.toObject(context, ECCNSContext.argument(context, 0)).data.object;
    value = ECCNSValue.toPrimitive(context, ECCNSContext.argument(context, 1), ECC_VALHINT_STRING);
    ref = objnsfn_property(object, value, ECC_VALFLAG_ASOWN);

    if(ref)
    {
        eccobject_t* result = objnsfn_create(ECC_Prototype_Object);

        if(ref->flags & ECC_VALFLAG_ACCESSOR)
        {
            if(ref->flags & ECC_VALFLAG_ASDATA)
            {
                objnsfn_addMember(result, ECC_ConstKey_value, ECCNSObject.getValue(context, object, ref), 0);
                objnsfn_addMember(result, ECC_ConstKey_writable, ECCNSValue.truth(!(ref->flags & ECC_VALFLAG_READONLY)), 0);
            }
            else
            {
                objnsfn_addMember(result, ref->flags & ECC_VALFLAG_GETTER ? ECC_ConstKey_get : ECC_ConstKey_set, ECCNSValue.function(ref->data.function), 0);
                if(ref->data.function->pair)
                    objnsfn_addMember(result, ref->flags & ECC_VALFLAG_GETTER ? ECC_ConstKey_set : ECC_ConstKey_get, ECCNSValue.function(ref->data.function->pair), 0);
            }
        }
        else
        {
            objnsfn_addMember(result, ECC_ConstKey_value, *ref, 0);
            objnsfn_addMember(result, ECC_ConstKey_writable, ECCNSValue.truth(!(ref->flags & ECC_VALFLAG_READONLY)), 0);
        }

        objnsfn_addMember(result, ECC_ConstKey_enumerable, ECCNSValue.truth(!(ref->flags & ECC_VALFLAG_HIDDEN)), 0);
        objnsfn_addMember(result, ECC_ConstKey_configurable, ECCNSValue.truth(!(ref->flags & ECC_VALFLAG_SEALED)), 0);

        return ECCNSValue.object(result);
    }

    return ECCValConstUndefined;
}

static eccvalue_t objobjectfn_getOwnPropertyNames(eccstate_t* context)
{
    eccobject_t *object, *parent;
    eccobject_t* result;
    uint32_t index, count, length;

    object = eccobject_checkObject(context, 0);
    result = ECCNSArray.create();
    length = 0;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].value.check == 1)
            objnsfn_addElement(result, length++, ECCNSValue.chars(ECCNSChars.create("%d", index)), 0);

    parent = object;
    while((parent = parent->prototype))
    {
        for(index = 2; index < parent->hashmapCount; ++index)
        {
            eccvalue_t value = parent->hashmap[index].value;
            if(value.check == 1 && value.flags & ECC_VALFLAG_ASOWN)
                objnsfn_addElement(result, length++, ECCNSValue.text(ECCNSKey.textOf(value.key)), 0);
        }
    }

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].value.check == 1)
            objnsfn_addElement(result, length++, ECCNSValue.text(ECCNSKey.textOf(object->hashmap[index].value.key)), 0);

    return ECCNSValue.object(result);
}

static eccvalue_t objobjectfn_defineProperty(eccstate_t* context)
{
    eccobject_t *object, *descriptor;
    eccvalue_t property, value, *getter, *setter, *current, *flag;
    eccindexkey_t key;
    uint32_t index;

    object = eccobject_checkObject(context, 0);
    property = ECCNSValue.toPrimitive(context, ECCNSContext.argument(context, 1), ECC_VALHINT_STRING);
    descriptor = eccobject_checkObject(context, 2);

    getter = objnsfn_member(descriptor, ECC_ConstKey_get, 0);
    setter = objnsfn_member(descriptor, ECC_ConstKey_set, 0);

    current = ECCNSObject.property(object, property, ECC_VALFLAG_ASOWN);

    if(getter || setter)
    {
        if(getter && getter->type == ECC_VALTYPE_UNDEFINED)
            getter = NULL;

        if(setter && setter->type == ECC_VALTYPE_UNDEFINED)
            setter = NULL;

        if(getter && getter->type != ECC_VALTYPE_FUNCTION)
            ECCNSContext.typeError(context, ECCNSChars.create("getter is not a function"));

        if(setter && setter->type != ECC_VALTYPE_FUNCTION)
            ECCNSContext.typeError(context, ECCNSChars.create("setter is not a function"));

        if(objnsfn_member(descriptor, ECC_ConstKey_value, 0) || objnsfn_member(descriptor, ECC_ConstKey_writable, 0))
            ECCNSContext.typeError(context, ECCNSChars.create("value & writable forbidden when a getter or setter are set"));

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
            value = ECCNSValue.function(ECCNSFunction.createWithNative(ECCNSOperand.noop, 0));
            value.flags |= ECC_VALFLAG_GETTER;
        }
    }
    else
    {
        value = objnsfn_getMember(context, descriptor, ECC_ConstKey_value);

        flag = objnsfn_member(descriptor, ECC_ConstKey_writable, 0);
        if((flag && !ECCNSValue.isTrue(objnsfn_getValue(context, descriptor, flag))) || (!flag && (!current || current->flags & ECC_VALFLAG_READONLY)))
            value.flags |= ECC_VALFLAG_READONLY;
    }

    flag = objnsfn_member(descriptor, ECC_ConstKey_enumerable, 0);
    if((flag && !ECCNSValue.isTrue(objnsfn_getValue(context, descriptor, flag))) || (!flag && (!current || current->flags & ECC_VALFLAG_HIDDEN)))
        value.flags |= ECC_VALFLAG_HIDDEN;

    flag = objnsfn_member(descriptor, ECC_ConstKey_configurable, 0);
    if((flag && !ECCNSValue.isTrue(objnsfn_getValue(context, descriptor, flag))) || (!flag && (!current || current->flags & ECC_VALFLAG_SEALED)))
        value.flags |= ECC_VALFLAG_SEALED;

    if(!current)
    {
        objnsfn_addProperty(object, property, value, 0);
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
                            ECCNSContext.callFunction(context, currentSetter, ECCNSValue.object(object), 1, value);
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
                if(!ECCNSValue.isTrue(ECCNSValue.same(context, *current, value)))
                    goto sealedError;
            }
        }

        objnsfn_addProperty(object, property, value, 0);
    }

    return ECCValConstTrue;

sealedError:
    ECCNSContext.setTextIndexArgument(context, 1);
    index = eccobject_getIndexOrKey(property, &key);
    if(index == UINT32_MAX)
    {
        const ecctextstring_t* text = ECCNSKey.textOf(key);
        ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is non-configurable", text->length, text->bytes));
    }
    else
        ECCNSContext.typeError(context, ECCNSChars.create("'%u' is non-configurable", index));
}

static eccvalue_t objobjectfn_defineProperties(eccstate_t* context)
{
    ecchashmap_t* originalHashmap = context->environment->hashmap;
    uint16_t originalHashmapCount = context->environment->hashmapCount;

    uint16_t index, count, hashmapCount = 6;
    eccobject_t *object, *properties;
    ecchashmap_t hashmap[hashmapCount];

    memset(hashmap, 0, hashmapCount * sizeof(*hashmap));

    object = eccobject_checkObject(context, 0);
    properties = ECCNSValue.toObject(context, ECCNSContext.argument(context, 1)).data.object;

    context->environment->hashmap = hashmap;
    context->environment->hashmapCount = hashmapCount;

    ECCNSContext.replaceArgument(context, 0, ECCNSValue.object(object));

    for(index = 0, count = eccobject_elementCount(properties); index < count; ++index)
    {
        if(!properties->element[index].value.check)
            continue;

        ECCNSContext.replaceArgument(context, 1, ECCNSValue.binary(index));
        ECCNSContext.replaceArgument(context, 2, properties->element[index].value);
        objobjectfn_defineProperty(context);
    }

    for(index = 2; index < properties->hashmapCount; ++index)
    {
        if(!properties->hashmap[index].value.check)
            continue;

        ECCNSContext.replaceArgument(context, 1, ECCNSValue.key(properties->hashmap[index].value.key));
        ECCNSContext.replaceArgument(context, 2, properties->hashmap[index].value);
        objobjectfn_defineProperty(context);
    }

    context->environment->hashmap = originalHashmap;
    context->environment->hashmapCount = originalHashmapCount;

    return ECCValConstUndefined;
}

static eccvalue_t objobjectfn_objectCreate(eccstate_t* context)
{
    eccobject_t *object, *result;
    eccvalue_t properties;

    object = eccobject_checkObject(context, 0);
    properties = ECCNSContext.argument(context, 1);

    result = objnsfn_create(object);
    if(properties.type != ECC_VALTYPE_UNDEFINED)
    {
        ECCNSContext.replaceArgument(context, 0, ECCNSValue.object(result));
        objobjectfn_defineProperties(context);
    }

    return ECCNSValue.object(result);
}

static eccvalue_t objobjectfn_seal(eccstate_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].value.check == 1)
            object->element[index].value.flags |= ECC_VALFLAG_SEALED;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].value.check == 1)
            object->hashmap[index].value.flags |= ECC_VALFLAG_SEALED;

    return ECCNSValue.object(object);
}

static eccvalue_t objobjectfn_freeze(eccstate_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].value.check == 1)
            object->element[index].value.flags |= ECC_VALFLAG_FROZEN;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].value.check == 1)
            object->hashmap[index].value.flags |= ECC_VALFLAG_FROZEN;

    return ECCNSValue.object(object);
}

static eccvalue_t objobjectfn_preventExtensions(eccstate_t* context)
{
    eccobject_t* object;

    object = eccobject_checkObject(context, 0);
    object->flags |= ECC_OBJFLAG_SEALED;

    return ECCNSValue.object(object);
}

static eccvalue_t objobjectfn_isSealed(eccstate_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    if(!(object->flags & ECC_OBJFLAG_SEALED))
        return ECCValConstFalse;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].value.check == 1 && !(object->element[index].value.flags & ECC_VALFLAG_SEALED))
            return ECCValConstFalse;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].value.check == 1 && !(object->hashmap[index].value.flags & ECC_VALFLAG_SEALED))
            return ECCValConstFalse;

    return ECCValConstTrue;
}

static eccvalue_t objobjectfn_isFrozen(eccstate_t* context)
{
    eccobject_t* object;
    uint32_t index, count;

    object = eccobject_checkObject(context, 0);
    if(!(object->flags & ECC_OBJFLAG_SEALED))
        return ECCValConstFalse;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].value.check == 1 && !(object->element[index].value.flags & ECC_VALFLAG_FROZEN))
            return ECCValConstFalse;

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].value.check == 1 && !(object->hashmap[index].value.flags & ECC_VALFLAG_FROZEN))
            return ECCValConstFalse;

    return ECCValConstTrue;
}

static eccvalue_t objobjectfn_isExtensible(eccstate_t* context)
{
    eccobject_t* object;

    object = eccobject_checkObject(context, 0);
    return ECCNSValue.truth(!(object->flags & ECC_OBJFLAG_SEALED));
}

static eccvalue_t objobjectfn_keys(eccstate_t* context)
{
    eccobject_t *object, *parent;
    eccobject_t* result;
    uint32_t index, count, length;

    object = eccobject_checkObject(context, 0);
    result = ECCNSArray.create();
    length = 0;

    for(index = 0, count = eccobject_elementCount(object); index < count; ++index)
        if(object->element[index].value.check == 1 && !(object->element[index].value.flags & ECC_VALFLAG_HIDDEN))
            objnsfn_addElement(result, length++, ECCNSValue.chars(ECCNSChars.create("%d", index)), 0);

    parent = object;
    while((parent = parent->prototype))
    {
        for(index = 2; index < parent->hashmapCount; ++index)
        {
            eccvalue_t value = parent->hashmap[index].value;
            if(value.check == 1 && value.flags & ECC_VALFLAG_ASOWN & !(value.flags & ECC_VALFLAG_HIDDEN))
                objnsfn_addElement(result, length++, ECCNSValue.text(ECCNSKey.textOf(value.key)), 0);
        }
    }

    for(index = 2; index < object->hashmapCount; ++index)
        if(object->hashmap[index].value.check == 1 && !(object->hashmap[index].value.flags & ECC_VALFLAG_HIDDEN))
            objnsfn_addElement(result, length++, ECCNSValue.text(ECCNSKey.textOf(object->hashmap[index].value.key)), 0);

    return ECCNSValue.object(result);
}

// MARK: - Methods

static void objnsfn_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    assert(sizeof(*ECC_Prototype_Object->hashmap) == 32);

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Object, objobjectfn_constructor, 1, NULL, ECCNSValue.object(ECC_Prototype_Object), NULL);

    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "getPrototypeOf", objobjectfn_getPrototypeOf, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "getOwnPropertyDescriptor", objobjectfn_getOwnPropertyDescriptor, 2, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "getOwnPropertyNames", objobjectfn_getOwnPropertyNames, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "create", objobjectfn_objectCreate, 2, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "defineProperty", objobjectfn_defineProperty, 3, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "defineProperties", objobjectfn_defineProperties, 2, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "seal", objobjectfn_seal, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "freeze", objobjectfn_freeze, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "preventExtensions", objobjectfn_preventExtensions, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "isSealed", objobjectfn_isSealed, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "isFrozen", objobjectfn_isFrozen, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "isExtensible", objobjectfn_isExtensible, 1, h);
    ECCNSFunction.addMethod(ECC_CtorFunc_Object, "keys", objobjectfn_keys, 1, h);

    ECCNSFunction.addToObject(ECC_Prototype_Object, "toString", objobjectfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Object, "toLocaleString", objobjectfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Object, "valueOf", objobjectfn_valueOf, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Object, "hasOwnProperty", objobjectfn_hasOwnProperty, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Object, "isPrototypeOf", objobjectfn_isPrototypeOf, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Object, "propertyIsEnumerable", objobjectfn_propertyIsEnumerable, 1, h);
}

static void objnsfn_teardown(void)
{
    ECC_Prototype_Object = NULL;
    ECC_CtorFunc_Object = NULL;
}

static eccobject_t* objnsfn_create(eccobject_t* prototype)
{
    return objnsfn_createSized(prototype, defaultSize);
}

static eccobject_t* objnsfn_createSized(eccobject_t* prototype, uint16_t size)
{
    eccobject_t* self = calloc(sizeof(*self), 1);
    ECCNSMemoryPool.addObject(self);
    return objnsfn_initializeSized(self, prototype, size);
}

static eccobject_t* objnsfn_createTyped(const eccobjinterntype_t* type)
{
    eccobject_t* self = objnsfn_createSized(ECC_Prototype_Object, defaultSize);
    self->type = type;
    return self;
}

static eccobject_t* objnsfn_initialize(eccobject_t* restrict self, eccobject_t* restrict prototype)
{
    return objnsfn_initializeSized(self, prototype, defaultSize);
}

static eccobject_t* objnsfn_initializeSized(eccobject_t* restrict self, eccobject_t* restrict prototype, uint16_t size)
{
    size_t byteSize;

    assert(self);

    *self = ECCNSObject.identity;

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
        self->hashmap = malloc(byteSize);
        memset(self->hashmap, 0, byteSize);
    }
    else
        // size is == zero = to be initialized later by caller
        self->hashmapCapacity = 2;

    return self;
}

static eccobject_t* objnsfn_finalize(eccobject_t* self)
{
    assert(self);

    if(self->type->finalize)
        self->type->finalize(self);

    free(self->hashmap), self->hashmap = NULL;
    free(self->element), self->element = NULL;

    return self;
}

static eccobject_t* objnsfn_copy(const eccobject_t* original)
{
    size_t byteSize;

    eccobject_t* self = malloc(sizeof(*self));
    ECCNSMemoryPool.addObject(self);

    *self = *original;

    byteSize = sizeof(*self->element) * self->elementCount;
    self->element = malloc(byteSize);
    memcpy(self->element, original->element, byteSize);

    byteSize = sizeof(*self->hashmap) * self->hashmapCount;
    self->hashmap = malloc(byteSize);
    memcpy(self->hashmap, original->hashmap, byteSize);

    return self;
}

static void objnsfn_destroy(eccobject_t* self)
{
    assert(self);

    free(self), self = NULL;
}

static eccvalue_t* objnsfn_member(eccobject_t* self, eccindexkey_t member, eccvalflag_t flags)
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
            ref = &object->hashmap[slot].value;
            if(ref->check == 1)
                return lookupChain || object == self || (ref->flags & flags) ? ref : NULL;
        }
    } while((object = object->prototype));

    return NULL;
}

static eccvalue_t* objnsfn_element(eccobject_t* self, uint32_t index, eccvalflag_t flags)
{
    int lookupChain = !(flags & ECC_VALFLAG_ASOWN);
    eccobject_t* object = self;
    eccvalue_t* ref = NULL;

    assert(self);

    if(self->type == &ECC_Type_String)
    {
        eccvalue_t* subref = ECCNSObject.addMember(self, ECC_ConstKey_none, ECCNSString.valueAtIndex((eccobjstring_t*)self, index), 0);
        subref->check = 0;
        return subref;
    }
    else if(index > io_libecc_object_ElementMax)
    {
        eccindexkey_t key = eccobject_keyOfIndex(index, 0);
        if(key.data.integer)
            return objnsfn_member(self, key, flags);
    }
    else
        do
        {
            if(index < object->elementCount)
            {
                ref = &object->element[index].value;
                if(ref->check == 1)
                    return lookupChain || object == self || (ref->flags & flags) ? ref : NULL;
            }
        } while((object = object->prototype));

    return NULL;
}

static eccvalue_t* objnsfn_property(eccobject_t* self, eccvalue_t property, eccvalflag_t flags)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(property, &key);

    if(index < UINT32_MAX)
        return objnsfn_element(self, index, flags);
    else
        return objnsfn_member(self, key, flags);
}

static eccvalue_t objnsfn_getValue(eccstate_t* context, eccobject_t* self, eccvalue_t* ref)
{
    if(!ref)
        return ECCValConstUndefined;

    if(ref->flags & ECC_VALFLAG_ACCESSOR)
    {
        if(!context)
            ECCNSScript.fatal("cannot use getter outside context");

        if(ref->flags & ECC_VALFLAG_GETTER)
            return ECCNSContext.callFunction(context, ref->data.function, ECCNSValue.object(self), 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        else if(ref->data.function->pair)
            return ECCNSContext.callFunction(context, ref->data.function->pair, ECCNSValue.object(self), 0 | ECC_CTXSPECIALTYPE_ASACCESSOR);
        else
            return ECCValConstUndefined;
    }

    return *ref;
}

static eccvalue_t objnsfn_getMember(eccstate_t* context, eccobject_t* self, eccindexkey_t key)
{
    return objnsfn_getValue(context, self, objnsfn_member(self, key, 0));
}

static eccvalue_t objnsfn_getElement(eccstate_t* context, eccobject_t* self, uint32_t index)
{
    if(self->type == &ECC_Type_String)
        return ECCNSString.valueAtIndex((eccobjstring_t*)self, index);
    else
        return objnsfn_getValue(context, self, objnsfn_element(self, index, 0));
}

static eccvalue_t objnsfn_getProperty(eccstate_t* context, eccobject_t* self, eccvalue_t property)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(property, &key);

    if(index < UINT32_MAX)
        return objnsfn_getElement(context, self, index);
    else
        return objnsfn_getMember(context, self, key);
}

static eccvalue_t objnsfn_putValue(eccstate_t* context, eccobject_t* self, eccvalue_t* ref, eccvalue_t value)
{
    if(ref->flags & ECC_VALFLAG_ACCESSOR)
    {
        assert(context);

        if(ref->flags & ECC_VALFLAG_SETTER)
            ECCNSContext.callFunction(context, ref->data.function, ECCNSValue.object(self), 1 | ECC_CTXSPECIALTYPE_ASACCESSOR, value);
        else if(ref->data.function->pair)
            ECCNSContext.callFunction(context, ref->data.function->pair, ECCNSValue.object(self), 1 | ECC_CTXSPECIALTYPE_ASACCESSOR, value);
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

static eccvalue_t objnsfn_putMember(eccstate_t* context, eccobject_t* self, eccindexkey_t key, eccvalue_t value)
{
    eccvalue_t* ref;

    value.flags = 0;

    if((ref = objnsfn_member(self, key, ECC_VALFLAG_ASOWN | ECC_VALFLAG_ACCESSOR)))
        return objnsfn_putValue(context, self, ref, value);
    else if(self->prototype && (ref = objnsfn_member(self->prototype, key, 0)))
    {
        if(ref->flags & ECC_VALFLAG_READONLY)
            ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is readonly", ECCNSKey.textOf(key)->length, ECCNSKey.textOf(key)->bytes));
    }

    if(self->flags & ECC_OBJFLAG_SEALED)
        ECCNSContext.typeError(context, ECCNSChars.create("object is not extensible"));

    return *objnsfn_addMember(self, key, value, 0);
}

static eccvalue_t objnsfn_putElement(eccstate_t* context, eccobject_t* self, uint32_t index, eccvalue_t value)
{
    eccvalue_t* ref;

    if(index > io_libecc_object_ElementMax)
    {
        if(self->elementCapacity <= index)
            objnsfn_resizeElement(self, index < UINT32_MAX ? index + 1 : index);
        else if(self->elementCount <= index)
            self->elementCount = index + 1;

        return objnsfn_putMember(context, self, eccobject_keyOfIndex(index, 1), value);
    }

    value.flags = 0;

    if((ref = objnsfn_element(self, index, ECC_VALFLAG_ASOWN | ECC_VALFLAG_ACCESSOR)))
        return objnsfn_putValue(context, self, ref, value);
    else if(self->prototype && (ref = objnsfn_element(self, index, 0)))
    {
        if(ref->flags & ECC_VALFLAG_READONLY)
            ECCNSContext.typeError(context, ECCNSChars.create("'%u' is readonly", index, index));
    }

    if(self->flags & ECC_OBJFLAG_SEALED)
        ECCNSContext.typeError(context, ECCNSChars.create("object is not extensible"));

    return *objnsfn_addElement(self, index, value, 0);
}

static eccvalue_t objnsfn_putProperty(eccstate_t* context, eccobject_t* self, eccvalue_t primitive, eccvalue_t value)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(primitive, &key);

    if(index < UINT32_MAX)
        return objnsfn_putElement(context, self, index, value);
    else
        return objnsfn_putMember(context, self, key, value);
}

static eccvalue_t* objnsfn_addMember(eccobject_t* self, eccindexkey_t key, eccvalue_t value, eccvalflag_t flags)
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
                self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);
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
            assert(self->hashmap[slot].value.check != 1);

        slot = self->hashmap[slot].slot[key.data.depth[depth]];
        assert(slot != 1);
        assert(slot < self->hashmapCount);
    } while(++depth < 4);

    if(value.flags & ECC_VALFLAG_ACCESSOR)
        if(self->hashmap[slot].value.check == 1 && self->hashmap[slot].value.flags & ECC_VALFLAG_ACCESSOR)
            if((self->hashmap[slot].value.flags & ECC_VALFLAG_ACCESSOR) != (value.flags & ECC_VALFLAG_ACCESSOR))
                value.data.function->pair = self->hashmap[slot].value.data.function;

    value.key = key;
    value.flags |= flags;

    self->hashmap[slot].value = value;

    return &self->hashmap[slot].value;
}

static eccvalue_t* objnsfn_addElement(eccobject_t* self, uint32_t index, eccvalue_t value, eccvalflag_t flags)
{
    eccvalue_t* ref;

    assert(self);

    if(self->elementCapacity <= index)
        objnsfn_resizeElement(self, index < UINT32_MAX ? index + 1 : index);
    else if(self->elementCount <= index)
        self->elementCount = index + 1;

    if(index > io_libecc_object_ElementMax)
        return objnsfn_addMember(self, eccobject_keyOfIndex(index, 1), value, flags);

    ref = &self->element[index].value;

    value.flags |= flags;
    *ref = value;

    return ref;
}

static eccvalue_t* objnsfn_addProperty(eccobject_t* self, eccvalue_t primitive, eccvalue_t value, eccvalflag_t flags)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(primitive, &key);

    if(index < UINT32_MAX)
        return objnsfn_addElement(self, index, value, flags);
    else
        return objnsfn_addMember(self, key, value, flags);
}

static int objnsfn_deleteMember(eccobject_t* self, eccindexkey_t member)
{
    eccobject_t* object = self;
    uint32_t slot, refSlot;

    assert(object);
    assert(member.data.integer);

    refSlot = self->hashmap[self->hashmap[self->hashmap[1].slot[member.data.depth[0]]].slot[member.data.depth[1]]].slot[member.data.depth[2]];

    slot = self->hashmap[refSlot].slot[member.data.depth[3]];

    if(!slot || !(object->hashmap[slot].value.check == 1))
        return 1;

    if(object->hashmap[slot].value.flags & ECC_VALFLAG_SEALED)
        return 0;

    object->hashmap[slot].value = ECCValConstUndefined;
    self->hashmap[refSlot].slot[member.data.depth[3]] = 0;
    return 1;
}

static int objnsfn_deleteElement(eccobject_t* self, uint32_t index)
{
    assert(self);

    if(index > io_libecc_object_ElementMax)
    {
        eccindexkey_t key = eccobject_keyOfIndex(index, 0);
        if(key.data.integer)
            return objnsfn_deleteMember(self, key);
        else
            return 1;
    }

    if(index < self->elementCount)
    {
        if(self->element[index].value.flags & ECC_VALFLAG_SEALED)
            return 0;

        memset(&self->element[index], 0, sizeof(*self->element));
    }

    return 1;
}

static int objnsfn_deleteProperty(eccobject_t* self, eccvalue_t primitive)
{
    eccindexkey_t key;
    uint32_t index = eccobject_getIndexOrKey(primitive, &key);

    if(index < UINT32_MAX)
        return objnsfn_deleteElement(self, index);
    else
        return objnsfn_deleteMember(self, key);
}

static void objnsfn_packValue(eccobject_t* self)
{
    ecchashmap_t data;
    uint32_t index = 2, valueIndex = 2, copyIndex, slot;

    assert(self);

    for(; index < self->hashmapCount; ++index)
    {
        if(self->hashmap[index].value.check == 1)
        {
            data = self->hashmap[index];
            for(copyIndex = index; copyIndex > valueIndex; --copyIndex)
            {
                self->hashmap[copyIndex] = self->hashmap[copyIndex - 1];
                if(!(self->hashmap[copyIndex].value.check == 1))
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
    self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * (self->hashmapCount));
    self->hashmapCapacity = self->hashmapCount;

    if(self->elementCount)
    {
        self->elementCapacity = self->elementCount;
    }
}

static void objnsfn_stripMap(eccobject_t* self)
{
    uint32_t index = 2;

    assert(self);

    while(index < self->hashmapCount && self->hashmap[index].value.check == 1)
        ++index;

    self->hashmapCapacity = self->hashmapCount = index;
    self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);

    memset(self->hashmap + 1, 0, sizeof(*self->hashmap));
}

static void objnsfn_reserveSlots(eccobject_t* self, uint16_t slots)
{
    int need = (slots * 4) - (self->hashmapCapacity - self->hashmapCount);

    assert(slots < self->hashmapCapacity);

    if(need > 0)
    {
        uint16_t capacity = self->hashmapCapacity;
        self->hashmapCapacity = self->hashmapCapacity ? self->hashmapCapacity * 2 : 2;
        self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);
        memset(self->hashmap + capacity, 0, sizeof(*self->hashmap) * (self->hashmapCapacity - capacity));
    }
}

static int objnsfn_resizeElement(eccobject_t* self, uint32_t size)
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
            ECCNSEnv.printWarning("Faking array length of %u while actual physical length is %u. Using array length > 0x%x is discouraged", size, capacity,
                                  io_libecc_object_ElementMax);
        }

        self->element = realloc(self->element, sizeof(*self->element) * capacity);
        if(capacity > self->elementCapacity)
            memset(self->element + self->elementCapacity, 0, sizeof(*self->element) * (capacity - self->elementCapacity));

        self->elementCapacity = capacity;
    }
    else if(size < self->elementCount)
    {
        eccobjelement_t* element;
        uint32_t until = size, e;

        if(self->elementCount > io_libecc_object_ElementMax)
        {
            ecchashmap_t* hashmap;
            uint32_t index, h;

            for(h = 2; h < self->hashmapCount; ++h)
            {
                hashmap = &self->hashmap[h];
                if(hashmap->value.check == 1)
                {
                    index = ECCNSLexer.scanElement(*ECCNSKey.textOf(hashmap->value.key));
                    if(hashmap->value.check == 1 && (hashmap->value.flags & ECC_VALFLAG_SEALED) && index >= until)
                        until = index + 1;
                }
            }

            for(h = 2; h < self->hashmapCount; ++h)
            {
                hashmap = &self->hashmap[h];
                if(hashmap->value.check == 1)
                    if(ECCNSLexer.scanElement(*ECCNSKey.textOf(hashmap->value.key)) >= until)
                        self->hashmap[h].value.check = 0;
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
            if(element->value.check == 1 && (element->value.flags & ECC_VALFLAG_SEALED) && e >= until)
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

static void objnsfn_populateElementWithCList(eccobject_t* self, uint32_t count, const char* list[])
{
    double binary;
    char* end;
    int index;

    assert(self);
    assert(count <= io_libecc_object_ElementMax);

    if(count > self->elementCount)
        objnsfn_resizeElement(self, count);

    for(index = 0; index < (int)count; ++index)
    {
        uint16_t length = (uint16_t)strlen(list[index]);
        binary = strtod(list[index], &end);

        if(end == list[index] + length)
            self->element[index].value = ECCNSValue.binary(binary);
        else
        {
            ecccharbuffer_t* chars = ECCNSChars.createSized(length);
            memcpy(chars->bytes, list[index], length);

            self->element[index].value = ECCNSValue.chars(chars);
        }
    }
}

static eccvalue_t objobjectfn_toString(eccstate_t* context)
{
    if(context->thisvalue.type == ECC_VALTYPE_NULL)
        return ECCNSValue.text(&ECC_ConstString_NullType);
    else if(context->thisvalue.type == ECC_VALTYPE_UNDEFINED)
        return ECCNSValue.text(&ECC_ConstString_UndefinedType);
    else if(ECCNSValue.isString(context->thisvalue))
        return ECCNSValue.text(&ECC_ConstString_StringType);
    else if(ECCNSValue.isNumber(context->thisvalue))
        return ECCNSValue.text(&ECC_ConstString_NumberType);
    else if(ECCNSValue.isBoolean(context->thisvalue))
        return ECCNSValue.text(&ECC_ConstString_BooleanType);
    else if(ECCNSValue.isObject(context->thisvalue))
        return ECCNSValue.text(context->thisvalue.data.object->type->text);
    else
        assert(0);

    return ECCValConstUndefined;
}

static void objnsfn_dumpTo(eccobject_t* self, FILE* file)
{
    uint32_t index, count;
    int isArray;

    assert(self);

    isArray = ECCNSValue.objectIsArray(self);

    fprintf(file, isArray ? "[ " : "{ ");

    for(index = 0, count = eccobject_elementCount(self); index < count; ++index)
    {
        if(self->element[index].value.check == 1)
        {
            if(!isArray)
                fprintf(file, "%d: ", (int)index);

            if(self->element[index].value.type == ECC_VALTYPE_OBJECT && self->element[index].value.data.object == self)
                fprintf(file, "this");
            else
                ECCNSValue.dumpTo(self->element[index].value, file);

            fprintf(file, ", ");
        }
    }

    if(!isArray)
    {
        for(index = 0; index < self->hashmapCount; ++index)
        {
            if(self->hashmap[index].value.check == 1)
            {
                fprintf(stderr, "'");
                ECCNSKey.dumpTo(self->hashmap[index].value.key, file);
                fprintf(file, "': ");

                if(self->hashmap[index].value.type == ECC_VALTYPE_OBJECT && self->hashmap[index].value.data.object == self)
                    fprintf(file, "this");
                else
                    ECCNSValue.dumpTo(self->hashmap[index].value, file);

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
