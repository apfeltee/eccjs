//
//  array.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

typedef struct eccarraycomparestate_t eccarraycomparestate_t;

struct eccarraycomparestate_t
{
    eccstate_t context;
    eccobjfunction_t* function;
    eccobject_t* arguments;
    const eccoperand_t* ops;

};

eccobject_t* ECC_Prototype_Array = NULL;
eccobjfunction_t* ECC_CtorFunc_Array = NULL;

const eccobjinterntype_t ECC_Type_Array = {
    .text = &ECC_ConstString_ArrayType,
};



static int eccarray_valueIsArray(eccvalue_t value);
static uint32_t eccarray_getLengthArrayVal(eccvalue_t value);
static uint32_t eccarray_getLength(eccstate_t *context, eccobject_t *object);
static void eccarray_objectResize(eccstate_t *context, eccobject_t *object, uint32_t length);
static void eccarray_valueAppendFromElement(eccstate_t *context, eccvalue_t value, eccobject_t *object, uint32_t *element);
static eccvalue_t objarrayfn_isArray(eccstate_t *context);
static eccvalue_t eccarray_toChars(eccstate_t *context, eccvalue_t thisval, ecctextstring_t separator);
static eccvalue_t objarrayfn_toString(eccstate_t *context);
static eccvalue_t objarrayfn_concat(eccstate_t *context);
static eccvalue_t objarrayfn_join(eccstate_t *context);
static eccvalue_t objarrayfn_pop(eccstate_t *context);
static eccvalue_t objarrayfn_push(eccstate_t *context);
static eccvalue_t objarrayfn_reverse(eccstate_t *context);
static eccvalue_t objarrayfn_shift(eccstate_t *context);
static eccvalue_t objarrayfn_unshift(eccstate_t *context);
static eccvalue_t objarrayfn_slice(eccstate_t *context);
static eccvalue_t objarrayfn_defaultComparison(eccstate_t *context);
int eccarray_gcd(int m, int n);
void eccarray_rotate(eccobject_t *object, eccstate_t *context, uint32_t first, uint32_t half, uint32_t last);
int eccarray_compare(eccarraycomparestate_t *cmp, eccvalue_t left, eccvalue_t right);
uint32_t eccarray_search(eccobject_t *object, eccarraycomparestate_t *cmp, uint32_t first, uint32_t last, eccvalue_t right);
void eccarray_merge(eccobject_t *object, eccarraycomparestate_t *cmp, uint32_t first, uint32_t pivot, uint32_t last, uint32_t len1, uint32_t len2);
static void eccarray_sortAndMerge(eccobject_t *object, eccarraycomparestate_t *cmp, uint32_t first, uint32_t last);
static void eccarray_sortInPlace(eccstate_t *context, eccobject_t *object, eccobjfunction_t *function, int first, int last);
static eccvalue_t objarrayfn_sort(eccstate_t *context);
static eccvalue_t objarrayfn_splice(eccstate_t *context);
static eccvalue_t objarrayfn_indexOf(eccstate_t *context);
static eccvalue_t objarrayfn_lastIndexOf(eccstate_t *context);
static eccvalue_t objarrayfn_getLength(eccstate_t *context);
static eccvalue_t objarrayfn_setLength(eccstate_t *context);
static eccvalue_t objarrayfn_constructor(eccstate_t *context);
static void nsarrayfn_setup(void);
static void nsarrayfn_teardown(void);
static eccobject_t *nsarrayfn_create(void);
static eccobject_t *nsarrayfn_createSized(uint32_t size);


const struct eccpseudonsarray_t ECCNSArray = {
    nsarrayfn_setup,
    nsarrayfn_teardown,
    nsarrayfn_create,
    nsarrayfn_createSized,
    {}
};

static int eccarray_valueIsArray(eccvalue_t value)
{
    return ecc_value_isobject(value) && ecc_value_objectisarray(value.data.object);
}

static uint32_t eccarray_getLengthArrayVal(eccvalue_t value)
{
    if(eccarray_valueIsArray(value))
        return value.data.object->elementCount;

    return 1;
}

static uint32_t eccarray_getLength(eccstate_t* context, eccobject_t* object)
{
    if(object->type == &ECC_Type_Array)
        return object->elementCount;
    else
        return ecc_value_tointeger(context, ECCNSObject.getMember(context, object, ECC_ConstKey_length)).data.integer;
}

static void eccarray_objectResize(eccstate_t* context, eccobject_t* object, uint32_t length)
{
    if(object->type == &ECC_Type_Array)
    {
        if(ECCNSObject.resizeElement(object, length) && context->parent->strictMode)
        {
            ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
            ECCNSContext.typeError(context, ECCNSChars.create("'%u' is non-configurable", length));
        }
    }
    else
        ECCNSObject.putMember(context, object, ECC_ConstKey_length, ecc_value_binary(length));
}

static void eccarray_valueAppendFromElement(eccstate_t* context, eccvalue_t value, eccobject_t* object, uint32_t* element)
{
    uint32_t index;

    if(eccarray_valueIsArray(value))
        for(index = 0; index < value.data.object->elementCount; ++index)
            ECCNSObject.putElement(context, object, (*element)++, ECCNSObject.getElement(context, value.data.object, index));
    else
        ECCNSObject.putElement(context, object, (*element)++, value);
}

static eccvalue_t objarrayfn_isArray(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);

    return ecc_value_truth(value.type == ECC_VALTYPE_OBJECT && value.data.object->type == &ECC_Type_Array);
}

static eccvalue_t eccarray_toChars(eccstate_t* context, eccvalue_t thisval, ecctextstring_t separator)
{
    eccobject_t* object = thisval.data.object;
    eccvalue_t value, length = ECCNSObject.getMember(context, object, ECC_ConstKey_length);
    uint32_t index, count = ecc_value_tointeger(context, length).data.integer;
    eccappendbuffer_t chars;

    ECCNSChars.beginAppend(&chars);
    for(index = 0; index < count; ++index)
    {
        value = ECCNSObject.getElement(context, thisval.data.object, index);

        if(index)
            ECCNSChars.append(&chars, "%.*s", separator.length, separator.bytes);

        if(value.type != ECC_VALTYPE_UNDEFINED && value.type != ECC_VALTYPE_NULL)
            ECCNSChars.appendValue(&chars, context, value);
    }

    return ECCNSChars.endAppend(&chars);
}

static eccvalue_t objarrayfn_toString(eccstate_t* context)
{
    eccvalue_t function;

    context->thisvalue = ecc_value_toobject(context, ECCNSContext.getThis(context));
    function = ECCNSObject.getMember(context, context->thisvalue.data.object, ECC_ConstKey_join);

    if(function.type == ECC_VALTYPE_FUNCTION)
        return ECCNSContext.callFunction(context, function.data.function, context->thisvalue, 0);
    else
        return ECCNSObject.toString(context);
}

static eccvalue_t objarrayfn_concat(eccstate_t* context)
{
    eccvalue_t value;
    uint32_t element = 0, length = 0, index, count;
    eccobject_t* array = NULL;

    value = ecc_value_toobject(context, ECCNSContext.getThis(context));
    count = ECCNSContext.argumentCount(context);

    length += eccarray_getLengthArrayVal(value);
    for(index = 0; index < count; ++index)
        length += eccarray_getLengthArrayVal(ECCNSContext.argument(context, index));

    array = ECCNSArray.createSized(length);

    eccarray_valueAppendFromElement(context, value, array, &element);
    for(index = 0; index < count; ++index)
        eccarray_valueAppendFromElement(context, ECCNSContext.argument(context, index), array, &element);

    return ecc_value_object(array);
}

static eccvalue_t objarrayfn_join(eccstate_t* context)
{
    eccvalue_t object;
    eccvalue_t value;
    ecctextstring_t separator;
    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
    {
        separator = ECCNSText.make(",", 1);
    }
    else
    {
        value = ecc_value_tostring(context, value);
        separator = ecc_value_textof(&value);
    }
    object = ecc_value_toobject(context, ECCNSContext.getThis(context));
    return eccarray_toChars(context, object, separator);
}

static eccvalue_t objarrayfn_map(eccstate_t* context)
{
    eccvalue_t thing;
    eccvalue_t callme;
    eccvalue_t numval;
    eccvalue_t retval;
    eccvalue_t thisval;
    uint32_t index;
    uint32_t length;
    eccobject_t* thisobj;
    thisval = ECCNSContext.getThis(context);
    thisobj = ecc_value_toobject(context, thisval).data.object;
    length = eccarray_getLength(context, thisobj);
    /* the function that will be called */
    callme = ECCNSContext.argument(context, 0);
    for(index = 0; index < length; index++)
    {
        /* get value at index ... */
        thing = ECCNSObject.getElement(context, thisobj, index);
        /* second argument is the index */
        numval = ecc_value_integer(index);
        /* call the function */
        retval = ECCNSContext.callFunction(context, callme.data.function, thisval, 2, thing, numval);
        /* put new value back */
        ECCNSObject.putElement(context, thisobj, index, retval);
    }
    return ecc_value_object(thisobj);
}

static eccvalue_t objarrayfn_pop(eccstate_t* context)
{
    eccvalue_t value = ECCValConstUndefined;
    eccobject_t* thisobj;
    uint32_t length;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    length = eccarray_getLength(context, thisobj);

    if(length)
    {
        --length;
        value = ECCNSObject.getElement(context, thisobj, length);
        if(!ECCNSObject.deleteElement(thisobj, length) && context->parent->strictMode)
        {
            ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
            ECCNSContext.typeError(context, ECCNSChars.create("'%u' is non-configurable", length));
        }
    }
    eccarray_objectResize(context, thisobj, length);

    return value;
}

static eccvalue_t objarrayfn_push(eccstate_t* context)
{
    eccobject_t* thisobj;
    uint32_t length = 0, index, count, base;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    count = ECCNSContext.argumentCount(context);

    base = eccarray_getLength(context, thisobj);
    length = UINT32_MAX - base < count ? UINT32_MAX : base + count;
    eccarray_objectResize(context, thisobj, length);

    for(index = base; index < length; ++index)
        ECCNSObject.putElement(context, thisobj, index, ECCNSContext.argument(context, index - base));

    if(UINT32_MAX - base < count)
    {
        ECCNSObject.putElement(context, thisobj, index, ECCNSContext.argument(context, index - base));

        if(thisobj->type == &ECC_Type_Array)
            ECCNSContext.rangeError(context, ECCNSChars.create("max length exeeded"));
        else
        {
            double subidx, sublen = (double)base + count;
            for(subidx = (double)UINT32_MAX + 1; subidx < sublen; ++subidx)
                ECCNSObject.putProperty(context, thisobj, ecc_value_binary(subidx), ECCNSContext.argument(context, subidx - base));

            ECCNSObject.putMember(context, thisobj, ECC_ConstKey_length, ecc_value_binary(sublen));
            return ecc_value_binary(sublen);
        }
    }

    return ecc_value_binary(length);
}

static eccvalue_t objarrayfn_reverse(eccstate_t* context)
{
    eccvalue_t temp;
    eccobject_t* thisobj;
    uint32_t index, half, last, length;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    length = eccarray_getLength(context, thisobj);

    last = length - 1;
    half = length / 2;

    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);

    for(index = 0; index < half; ++index)
    {
        temp = ECCNSObject.getElement(context, thisobj, index);
        ECCNSObject.putElement(context, thisobj, index, ECCNSObject.getElement(context, thisobj, last - index));
        ECCNSObject.putElement(context, thisobj, last - index, temp);
    }

    return ecc_value_object(thisobj);
}

static eccvalue_t objarrayfn_shift(eccstate_t* context)
{
    eccvalue_t result;
    eccobject_t* thisobj;
    uint32_t index, count, length;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    length = eccarray_getLength(context, thisobj);

    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);

    if(length)
    {
        length--;
        result = ECCNSObject.getElement(context, thisobj, 0);

        for(index = 0, count = length; index < count; ++index)
            ECCNSObject.putElement(context, thisobj, index, ECCNSObject.getElement(context, thisobj, index + 1));

        if(!ECCNSObject.deleteElement(thisobj, length) && context->parent->strictMode)
        {
            ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
            ECCNSContext.typeError(context, ECCNSChars.create("'%u' is non-configurable", length));
        }
    }
    else
        result = ECCValConstUndefined;

    eccarray_objectResize(context, thisobj, length);

    return result;
}

static eccvalue_t objarrayfn_unshift(eccstate_t* context)
{
    eccobject_t* thisobj;
    uint32_t length = 0, index, count;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    count = ECCNSContext.argumentCount(context);

    length = eccarray_getLength(context, thisobj) + count;
    eccarray_objectResize(context, thisobj, length);

    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);

    for(index = count; index < length; ++index)
        ECCNSObject.putElement(context, thisobj, index, ECCNSObject.getElement(context, thisobj, index - count));

    for(index = 0; index < count; ++index)
        ECCNSObject.putElement(context, thisobj, index, ECCNSContext.argument(context, index));

    return ecc_value_binary(length);
}

static eccvalue_t objarrayfn_slice(eccstate_t* context)
{
    eccobject_t* thisobj, *result;
    eccvalue_t start, end;
    uint32_t from, to, length;
    double binary;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    length = eccarray_getLength(context, thisobj);

    start = ECCNSContext.argument(context, 0);
    binary = ecc_value_tobinary(context, start).data.binary;
    if(start.type == ECC_VALTYPE_UNDEFINED)
        from = 0;
    else if(binary >= 0)
        from = binary < length ? binary : length;
    else
        from = binary + length >= 0 ? length + binary : 0;

    end = ECCNSContext.argument(context, 1);
    binary = ecc_value_tobinary(context, end).data.binary;
    if(end.type == ECC_VALTYPE_UNDEFINED)
        to = length;
    else if(binary < 0 || isnan(binary))
        to = binary + length >= 0 ? length + binary : 0;
    else
        to = binary < length ? binary : length;

    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);

    if(to > from)
    {
        length = to - from;
        result = ECCNSArray.createSized(length);

        for(to = 0; to < length; ++from, ++to)
            ECCNSObject.putElement(context, result, to, ECCNSObject.getElement(context, thisobj, from));
    }
    else
        result = ECCNSArray.createSized(0);

    return ecc_value_object(result);
}

static eccvalue_t objarrayfn_defaultComparison(eccstate_t* context)
{
    eccvalue_t left, right, result;

    left = ECCNSContext.argument(context, 0);
    right = ECCNSContext.argument(context, 1);
    result = ecc_value_less(context, ecc_value_tostring(context, left), ecc_value_tostring(context, right));

    return ecc_value_integer(ecc_value_istrue(result) ? -1 : 0);
}

int eccarray_gcd(int m, int n)
{
    while(n)
    {
        int t = m % n;
        m = n;
        n = t;
    }
    return m;
}

void eccarray_rotate(eccobject_t* object, eccstate_t* context, uint32_t first, uint32_t half, uint32_t last)
{
    eccvalue_t value, leftValue;
    uint32_t n, shift, a, b;

    if(first == half || half == last)
        return;

    n = eccarray_gcd(last - first, half - first);
    while(n--)
    {
        shift = half - first;
        a = first + n;
        b = a + shift;
        leftValue = ECCNSObject.getElement(context, object, a);
        while(b != first + n)
        {
            value = ECCNSObject.getElement(context, object, b);
            ECCNSObject.putElement(context, object, a, value);
            a = b;
            if(last - b > shift)
                b += shift;
            else
                b = half - (last - b);
        }
        ECCNSObject.putElement(context, object, a, leftValue);
    }
}

int eccarray_compare(eccarraycomparestate_t* cmp, eccvalue_t left, eccvalue_t right)
{
    uint16_t hashmapCount;

    if(left.check != 1)
        return 0;
    else if(right.check != 1)
        return 1;

    if(left.type == ECC_VALTYPE_UNDEFINED)
        return 0;
    else if(right.type == ECC_VALTYPE_UNDEFINED)
        return 1;

    hashmapCount = cmp->context.environment->hashmapCount;
    switch(hashmapCount)
    {
        default:
            {
                memcpy(cmp->context.environment->hashmap + 5, cmp->function->environment.hashmap, sizeof(*cmp->context.environment->hashmap) * (hashmapCount - 5));
            }
            /* fallthrough */
        case 5:
            {
                cmp->context.environment->hashmap[3 + 1].value = right;
            }
            /* fallthrough */
        case 4:
            {
                cmp->context.environment->hashmap[3 + 0].value = left;
            }
            /* fallthrough */
        case 3:
            break;
        case 2:
        case 1:
        case 0:
            assert(0);
            break;
    }

    cmp->context.ops = cmp->ops;
    cmp->arguments->element[0].value = left;
    cmp->arguments->element[1].value = right;

    return ecc_value_tointeger(&cmp->context, cmp->context.ops->native(&cmp->context)).data.integer < 0;
}

uint32_t eccarray_search(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t last, eccvalue_t right)
{
    eccvalue_t left;
    uint32_t half;

    while(first < last)
    {
        half = (first + last) >> 1;
        left = ECCNSObject.getElement(&cmp->context, object, half);
        if(eccarray_compare(cmp, left, right))
            first = half + 1;
        else
            last = half;
    }
    return first;
}

void eccarray_merge(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t pivot, uint32_t last, uint32_t len1, uint32_t len2)
{
    uint32_t left, right, half1, half2;

    if(len1 == 0 || len2 == 0)
        return;

    if(len1 + len2 == 2)
    {
        eccvalue_t leftval, rightval;
        leftval = ECCNSObject.getElement(&cmp->context, object, pivot);
        rightval = ECCNSObject.getElement(&cmp->context, object, first);
        if(eccarray_compare(cmp, leftval, rightval))
        {
            ECCNSObject.putElement(&cmp->context, object, pivot, rightval);
            ECCNSObject.putElement(&cmp->context, object, first, leftval);
        }
        return;
    }

    if(len1 > len2)
    {
        half1 = len1 >> 1;
        left = first + half1;
        right = eccarray_search(object, cmp, pivot, last, ECCNSObject.getElement(&cmp->context, object, first + half1));
        half2 = right - pivot;
    }
    else
    {
        half2 = len2 >> 1;
        left = eccarray_search(object, cmp, first, pivot, ECCNSObject.getElement(&cmp->context, object, pivot + half2));
        right = pivot + half2;
        half1 = left - first;
    }
    eccarray_rotate(object, &cmp->context, left, pivot, right);

    pivot = left + half2;
    eccarray_merge(object, cmp, first, left, pivot, half1, half2);
    eccarray_merge(object, cmp, pivot, right, last, len1 - half1, len2 - half2);
}

static void eccarray_sortAndMerge(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t last)
{
    uint32_t half;

    if(last - first < 8)
    {
        eccvalue_t left, right;
        uint32_t i, j;

        for(i = first + 1; i < last; ++i)
        {
            right = ECCNSObject.getElement(&cmp->context, object, i);
            for(j = i; j > first; --j)
            {
                left = ECCNSObject.getElement(&cmp->context, object, j - 1);
                if(eccarray_compare(cmp, left, right))
                    break;
                else
                    ECCNSObject.putElement(&cmp->context, object, j, left);
            }
            ECCNSObject.putElement(&cmp->context, object, j, right);
        }
        return;
    }

    half = (first + last) >> 1;
    eccarray_sortAndMerge(object, cmp, first, half);
    eccarray_sortAndMerge(object, cmp, half, last);
    eccarray_merge(object, cmp, first, half, last, half - first, last - half);
}

static void eccarray_sortInPlace(eccstate_t* context, eccobject_t* object, eccobjfunction_t* function, int first, int last)
{
    eccoperand_t defaultOps = { objarrayfn_defaultComparison, ECCValConstUndefined, ECC_ConstString_NativeCode };
    const eccoperand_t* ops = function ? function->oplist->ops : &defaultOps;

    /*
    eccstate_t context;
    eccobjfunction_t* function;
    eccobject_t* arguments;
    const eccoperand_t* ops;

    */
    eccarraycomparestate_t cmp = {};
    cmp.context.thisvalue = ecc_value_object(object);
    cmp.context.parent = context;
    cmp.context.ecc = context->ecc;
    cmp.context.depth = context->depth + 1;
    cmp.context.ops = ops;
    cmp.context.textIndex = ECC_CTXINDEXTYPE_CALL;
    cmp.function = function;
    cmp.arguments = NULL;
    cmp.ops = ops;

    if(function && function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ECCNSObject.copy(&function->environment);

        cmp.context.environment = environment;
        cmp.arguments = ECCNSArguments.createSized(2);
        ++cmp.arguments->referenceCount;

        environment->hashmap[2].value = ecc_value_object(cmp.arguments);

        eccarray_sortAndMerge(object, &cmp, first, last);
    }
    else
    {
        eccobject_t environment = function ? function->environment : ECCNSObject.identity;
        eccobject_t arguments = ECCNSObject.identity;
        ecchashmap_t hashmap[function ? function->environment.hashmapCapacity : 3];
        ecchashitem_t element[2];

        if(function)
            memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        else
            environment.hashmapCount = 3;

        cmp.context.environment = &environment;
        cmp.arguments = &arguments;

        arguments.element = element;
        arguments.elementCount = 2;
        environment.hashmap = hashmap;
        environment.hashmap[2].value = ecc_value_object(&arguments);

        eccarray_sortAndMerge(object, &cmp, first, last);
    }
}

static eccvalue_t objarrayfn_sort(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t compare;
    uint32_t count;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    count = ecc_value_tointeger(context, ECCNSObject.getMember(context, thisobj, ECC_ConstKey_length)).data.integer;
    compare = ECCNSContext.argument(context, 0);

    if(compare.type == ECC_VALTYPE_FUNCTION)
        eccarray_sortInPlace(context, thisobj, compare.data.function, 0, count);
    else if(compare.type == ECC_VALTYPE_UNDEFINED)
        eccarray_sortInPlace(context, thisobj, NULL, 0, count);
    else
        ECCNSContext.typeError(context, ECCNSChars.create("comparison function must be a function or undefined"));

    return ecc_value_object(thisobj);
}

static eccvalue_t objarrayfn_splice(eccstate_t* context)
{
    eccobject_t* thisobj, *result;
    uint32_t length, from, to, count = 0, add = 0, start = 0, delindex = 0;

    count = ECCNSContext.argumentCount(context);
    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    length = eccarray_getLength(context, thisobj);

    if(count >= 1)
    {
        double binary = ecc_value_tobinary(context, ECCNSContext.argument(context, 0)).data.binary;
        if(isnan(binary))
            binary = 0;
        else if(binary < 0)
            binary += length;

        if(binary < 0)
            binary = 0;
        else if(binary > length)
            binary = length;

        start = binary;
    }

    if(count >= 2)
    {
        double binary = ecc_value_tobinary(context, ECCNSContext.argument(context, 1)).data.binary;
        if(isnan(binary) || binary < 0)
            binary = 0;
        else if(binary > length - start)
            binary = length - start;

        delindex = binary;
    }

    if(count > 2)
        add = count - 2;

    if(length - delindex +add > length)
        eccarray_objectResize(context, thisobj, length - delindex +add);

    result = ECCNSArray.createSized(delindex);

    for(from = start, to = 0; to < delindex; ++from, ++to)
        ECCNSObject.putElement(context, result, to, ECCNSObject.getElement(context, thisobj, from));

    if(delindex > add)
    {
        for(from = start + delindex, to = start + add; from < length; ++from, ++to)
            ECCNSObject.putElement(context, thisobj, to, ECCNSObject.getElement(context, thisobj, from));

        for(; to < length; ++to)
            ECCNSObject.putElement(context, thisobj, to, ECCValConstNone);
    }
    else if(delindex < add)
        for(from = length, to = length + add - delindex; from > start;)
            ECCNSObject.putElement(context, thisobj, --to, ECCNSObject.getElement(context, thisobj, --from));

    for(from = 2, to = start; from < count; ++from, ++to)
        ECCNSObject.putElement(context, thisobj, to, ECCNSContext.argument(context, from));

    if(length - delindex +add <= length)
        eccarray_objectResize(context, thisobj, length - delindex +add);

    return ecc_value_object(result);
}

static eccvalue_t objarrayfn_indexOf(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t search, start;
    uint32_t length = 0;
    int32_t index;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    length = eccarray_getLength(context, thisobj);

    search = ECCNSContext.argument(context, 0);
    start = ecc_value_tointeger(context, ECCNSContext.argument(context, 1));
    index = ((start.data.integer < 0) ? ((int32_t)(length + start.data.integer)) : (int32_t)start.data.integer);
    

    if(index < 0)
        index = 0;

    for(; index < (int32_t)length; ++index)
        if(ecc_value_istrue(ecc_value_same(context, search, ECCNSObject.getElement(context, thisobj, index))))
            return ecc_value_binary(index);

    return ecc_value_binary(-1);
}

static eccvalue_t objarrayfn_lastIndexOf(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t search, start;
    uint32_t length = 0;
    int32_t index;

    thisobj = ecc_value_toobject(context, ECCNSContext.getThis(context)).data.object;
    length = eccarray_getLength(context, thisobj);

    search = ECCNSContext.argument(context, 0);
    start = ecc_value_tointeger(context, ECCNSContext.argument(context, 1));
    index = ((start.data.integer <= 0) ? ((uint32_t)(length + start.data.integer)) : ((uint32_t)(start.data.integer + 1)));

    if(index < 0)
        index = 0;
    else if(index > (int32_t)length)
        index = length;

    for(; index--;)
        if(ecc_value_istrue(ecc_value_same(context, search, ECCNSObject.getElement(context, thisobj, index))))
            return ecc_value_binary(index);

    return ecc_value_binary(-1);
}

static eccvalue_t objarrayfn_getLength(eccstate_t* context)
{
    return ecc_value_binary(context->thisvalue.data.object->elementCount);
}

static eccvalue_t objarrayfn_setLength(eccstate_t* context)
{
    double length;

    length = ecc_value_tobinary(context, ECCNSContext.argument(context, 0)).data.binary;
    if(!isfinite(length) || length < 0 || length > UINT32_MAX || length != (uint32_t)length)
        ECCNSContext.rangeError(context, ECCNSChars.create("invalid array length"));

    if(ECCNSObject.resizeElement(context->thisvalue.data.object, length) && context->parent->strictMode)
        ECCNSContext.typeError(context, ECCNSChars.create("'%u' is non-configurable", context->thisvalue.data.object->elementCount));

    return ECCValConstUndefined;
}

static eccvalue_t objarrayfn_constructor(eccstate_t* context)
{
    eccvalue_t value;
    uint32_t index, count, length;
    eccobject_t* array;

    length = count = ECCNSContext.argumentCount(context);
    value = ECCNSContext.argument(context, 0);
    if(count == 1 && ecc_value_isnumber(value) && ecc_value_isprimitive(value))
    {
        double binary = ecc_value_tobinary(context, value).data.binary;
        if(isfinite(binary) && binary >= 0 && binary <= UINT32_MAX && binary == (uint32_t)binary)
        {
            length = binary;
            count = 0;
        }
        else
            ECCNSContext.rangeError(context, ECCNSChars.create("invalid array length"));
    }

    array = ECCNSArray.createSized(length);

    for(index = 0; index < count; ++index)
    {
        array->element[index].value = ECCNSContext.argument(context, index);
        array->element[index].value.flags &= ~(ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    }

    return ecc_value_object(array);
}

static void nsarrayfn_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Array, objarrayfn_constructor, -1, &ECC_Prototype_Array, ecc_value_object(nsarrayfn_createSized(0)), &ECC_Type_Array);

    ECCNSFunction.addMethod(ECC_CtorFunc_Array, "isArray", objarrayfn_isArray, 1, h);

    ECCNSFunction.addToObject(ECC_Prototype_Array, "toString", objarrayfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "toLocaleString", objarrayfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "concat", objarrayfn_concat, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "map", objarrayfn_map, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "join", objarrayfn_join, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "pop", objarrayfn_pop, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "push", objarrayfn_push, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "reverse", objarrayfn_reverse, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "shift", objarrayfn_shift, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "slice", objarrayfn_slice, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "sort", objarrayfn_sort, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "splice", objarrayfn_splice, -2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "unshift", objarrayfn_unshift, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "indexOf", objarrayfn_indexOf, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Array, "lastIndexOf", objarrayfn_lastIndexOf, -1, h);

    ECCNSObject.addMember(ECC_Prototype_Array, ECC_ConstKey_length, ECCNSFunction.accessor(objarrayfn_getLength, objarrayfn_setLength), h | s | ECC_VALFLAG_ASOWN | ECC_VALFLAG_ASDATA);
}

static void nsarrayfn_teardown(void)
{
    ECC_Prototype_Array = NULL;
    ECC_CtorFunc_Array = NULL;
}

static eccobject_t* nsarrayfn_create(void)
{
    return nsarrayfn_createSized(0);
}

static eccobject_t* nsarrayfn_createSized(uint32_t size)
{
    eccobject_t* self = ECCNSObject.create(ECC_Prototype_Array);

    ECCNSObject.resizeElement(self, size);

    return self;
}
