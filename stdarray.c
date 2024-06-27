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
static eccvalue_t objarrayfn_sort(eccstate_t *context);
static eccvalue_t objarrayfn_splice(eccstate_t *context);
static eccvalue_t objarrayfn_indexOf(eccstate_t *context);
static eccvalue_t objarrayfn_lastIndexOf(eccstate_t *context);
static eccvalue_t objarrayfn_getLength(eccstate_t *context);
static eccvalue_t objarrayfn_setLength(eccstate_t *context);
static eccvalue_t objarrayfn_constructor(eccstate_t *context);

int ecc_array_valueisarray(eccvalue_t value)
{
    return ecc_value_isobject(value) && ecc_value_objectisarray(value.data.object);
}

uint32_t ecc_array_getlengtharrayval(eccvalue_t value)
{
    if(ecc_array_valueisarray(value))
        return value.data.object->elementCount;

    return 1;
}

uint32_t ecc_array_getlength(eccstate_t* context, eccobject_t* object)
{
    if(object->type == &ECC_Type_Array)
        return object->elementCount;
    else
        return ecc_value_tointeger(context, ecc_object_getmember(context, object, ECC_ConstKey_length)).data.integer;
}

void ecc_array_objectresize(eccstate_t* context, eccobject_t* object, uint32_t length)
{
    if(object->type == &ECC_Type_Array)
    {
        if(ecc_object_resizeelement(object, length) && context->parent->strictMode)
        {
            ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
            ecc_context_typeerror(context, ecc_charbuf_create("'%u' is non-configurable", length));
        }
    }
    else
        ecc_object_putmember(context, object, ECC_ConstKey_length, ecc_value_binary(length));
}

void ecc_array_valueappendfromelement(eccstate_t* context, eccvalue_t value, eccobject_t* object, uint32_t* element)
{
    uint32_t index;

    if(ecc_array_valueisarray(value))
        for(index = 0; index < value.data.object->elementCount; ++index)
            ecc_object_putelement(context, object, (*element)++, ecc_object_getelement(context, value.data.object, index));
    else
        ecc_object_putelement(context, object, (*element)++, value);
}

static eccvalue_t objarrayfn_isArray(eccstate_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);

    return ecc_value_truth(value.type == ECC_VALTYPE_OBJECT && value.data.object->type == &ECC_Type_Array);
}

eccvalue_t ecc_array_tochars(eccstate_t* context, eccvalue_t thisval, ecctextstring_t separator)
{
    eccobject_t* object = thisval.data.object;
    eccvalue_t value, length = ecc_object_getmember(context, object, ECC_ConstKey_length);
    uint32_t index, count = ecc_value_tointeger(context, length).data.integer;
    eccappendbuffer_t chars;

    ecc_charbuf_beginappend(&chars);
    for(index = 0; index < count; ++index)
    {
        value = ecc_object_getelement(context, thisval.data.object, index);

        if(index)
            ecc_charbuf_append(&chars, "%.*s", separator.length, separator.bytes);

        if(value.type != ECC_VALTYPE_UNDEFINED && value.type != ECC_VALTYPE_NULL)
            ecc_charbuf_appendvalue(&chars, context, value);
    }

    return ecc_charbuf_endappend(&chars);
}

static eccvalue_t objarrayfn_toString(eccstate_t* context)
{
    eccvalue_t function;

    context->thisvalue = ecc_value_toobject(context, ecc_context_this(context));
    function = ecc_object_getmember(context, context->thisvalue.data.object, ECC_ConstKey_join);

    if(function.type == ECC_VALTYPE_FUNCTION)
        return ecc_context_callfunction(context, function.data.function, context->thisvalue, 0);
    else
        return ecc_object_tostringfn(context);
}

static eccvalue_t objarrayfn_concat(eccstate_t* context)
{
    eccvalue_t value;
    uint32_t element = 0, length = 0, index, count;
    eccobject_t* array = NULL;

    value = ecc_value_toobject(context, ecc_context_this(context));
    count = ecc_context_argumentcount(context);

    length += ecc_array_getlengtharrayval(value);
    for(index = 0; index < count; ++index)
        length += ecc_array_getlengtharrayval(ecc_context_argument(context, index));

    array = ecc_array_createsized(length);

    ecc_array_valueappendfromelement(context, value, array, &element);
    for(index = 0; index < count; ++index)
        ecc_array_valueappendfromelement(context, ecc_context_argument(context, index), array, &element);

    return ecc_value_object(array);
}

static eccvalue_t objarrayfn_join(eccstate_t* context)
{
    eccvalue_t object;
    eccvalue_t value;
    ecctextstring_t separator;
    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
    {
        separator = ecc_textbuf_make(",", 1);
    }
    else
    {
        value = ecc_value_tostring(context, value);
        separator = ecc_value_textof(&value);
    }
    object = ecc_value_toobject(context, ecc_context_this(context));
    return ecc_array_tochars(context, object, separator);
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
    thisval = ecc_context_this(context);
    thisobj = ecc_value_toobject(context, thisval).data.object;
    length = ecc_array_getlength(context, thisobj);
    /* the function that will be called */
    callme = ecc_context_argument(context, 0);
    for(index = 0; index < length; index++)
    {
        /* get value at index ... */
        thing = ecc_object_getelement(context, thisobj, index);
        /* second argument is the index */
        numval = ecc_value_integer(index);
        /* call the function */
        retval = ecc_context_callfunction(context, callme.data.function, thisval, 2, thing, numval);
        /* put new value back */
        ecc_object_putelement(context, thisobj, index, retval);
    }
    return ecc_value_object(thisobj);
}

static eccvalue_t objarrayfn_pop(eccstate_t* context)
{
    eccvalue_t value = ECCValConstUndefined;
    eccobject_t* thisobj;
    uint32_t length;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    length = ecc_array_getlength(context, thisobj);

    if(length)
    {
        --length;
        value = ecc_object_getelement(context, thisobj, length);
        if(!ecc_object_deleteelement(thisobj, length) && context->parent->strictMode)
        {
            ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
            ecc_context_typeerror(context, ecc_charbuf_create("'%u' is non-configurable", length));
        }
    }
    ecc_array_objectresize(context, thisobj, length);

    return value;
}

static eccvalue_t objarrayfn_push(eccstate_t* context)
{
    eccobject_t* thisobj;
    uint32_t length = 0, index, count, base;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    count = ecc_context_argumentcount(context);

    base = ecc_array_getlength(context, thisobj);
    length = UINT32_MAX - base < count ? UINT32_MAX : base + count;
    ecc_array_objectresize(context, thisobj, length);

    for(index = base; index < length; ++index)
        ecc_object_putelement(context, thisobj, index, ecc_context_argument(context, index - base));

    if(UINT32_MAX - base < count)
    {
        ecc_object_putelement(context, thisobj, index, ecc_context_argument(context, index - base));

        if(thisobj->type == &ECC_Type_Array)
            ecc_context_rangeerror(context, ecc_charbuf_create("max length exeeded"));
        else
        {
            double subidx, sublen = (double)base + count;
            for(subidx = (double)UINT32_MAX + 1; subidx < sublen; ++subidx)
                ecc_object_putproperty(context, thisobj, ecc_value_binary(subidx), ecc_context_argument(context, subidx - base));

            ecc_object_putmember(context, thisobj, ECC_ConstKey_length, ecc_value_binary(sublen));
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

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    length = ecc_array_getlength(context, thisobj);

    last = length - 1;
    half = length / 2;

    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);

    for(index = 0; index < half; ++index)
    {
        temp = ecc_object_getelement(context, thisobj, index);
        ecc_object_putelement(context, thisobj, index, ecc_object_getelement(context, thisobj, last - index));
        ecc_object_putelement(context, thisobj, last - index, temp);
    }

    return ecc_value_object(thisobj);
}

static eccvalue_t objarrayfn_shift(eccstate_t* context)
{
    eccvalue_t result;
    eccobject_t* thisobj;
    uint32_t index, count, length;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    length = ecc_array_getlength(context, thisobj);

    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);

    if(length)
    {
        length--;
        result = ecc_object_getelement(context, thisobj, 0);

        for(index = 0, count = length; index < count; ++index)
            ecc_object_putelement(context, thisobj, index, ecc_object_getelement(context, thisobj, index + 1));

        if(!ecc_object_deleteelement(thisobj, length) && context->parent->strictMode)
        {
            ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
            ecc_context_typeerror(context, ecc_charbuf_create("'%u' is non-configurable", length));
        }
    }
    else
        result = ECCValConstUndefined;

    ecc_array_objectresize(context, thisobj, length);

    return result;
}

static eccvalue_t objarrayfn_unshift(eccstate_t* context)
{
    eccobject_t* thisobj;
    uint32_t length = 0, index, count;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    count = ecc_context_argumentcount(context);

    length = ecc_array_getlength(context, thisobj) + count;
    ecc_array_objectresize(context, thisobj, length);

    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);

    for(index = count; index < length; ++index)
        ecc_object_putelement(context, thisobj, index, ecc_object_getelement(context, thisobj, index - count));

    for(index = 0; index < count; ++index)
        ecc_object_putelement(context, thisobj, index, ecc_context_argument(context, index));

    return ecc_value_binary(length);
}

static eccvalue_t objarrayfn_slice(eccstate_t* context)
{
    eccobject_t* thisobj, *result;
    eccvalue_t start, end;
    uint32_t from, to, length;
    double binary;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    length = ecc_array_getlength(context, thisobj);

    start = ecc_context_argument(context, 0);
    binary = ecc_value_tobinary(context, start).data.binary;
    if(start.type == ECC_VALTYPE_UNDEFINED)
        from = 0;
    else if(binary >= 0)
        from = binary < length ? binary : length;
    else
        from = binary + length >= 0 ? length + binary : 0;

    end = ecc_context_argument(context, 1);
    binary = ecc_value_tobinary(context, end).data.binary;
    if(end.type == ECC_VALTYPE_UNDEFINED)
        to = length;
    else if(binary < 0 || isnan(binary))
        to = binary + length >= 0 ? length + binary : 0;
    else
        to = binary < length ? binary : length;

    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);

    if(to > from)
    {
        length = to - from;
        result = ecc_array_createsized(length);

        for(to = 0; to < length; ++from, ++to)
            ecc_object_putelement(context, result, to, ecc_object_getelement(context, thisobj, from));
    }
    else
        result = ecc_array_createsized(0);

    return ecc_value_object(result);
}

static eccvalue_t objarrayfn_defaultComparison(eccstate_t* context)
{
    eccvalue_t left, right, result;

    left = ecc_context_argument(context, 0);
    right = ecc_context_argument(context, 1);
    result = ecc_value_less(context, ecc_value_tostring(context, left), ecc_value_tostring(context, right));

    return ecc_value_integer(ecc_value_istrue(result) ? -1 : 0);
}

int ecc_array_gcd(int m, int n)
{
    while(n)
    {
        int t = m % n;
        m = n;
        n = t;
    }
    return m;
}

void ecc_array_rotate(eccobject_t* object, eccstate_t* context, uint32_t first, uint32_t half, uint32_t last)
{
    eccvalue_t value, leftValue;
    uint32_t n, shift, a, b;

    if(first == half || half == last)
        return;

    n = ecc_array_gcd(last - first, half - first);
    while(n--)
    {
        shift = half - first;
        a = first + n;
        b = a + shift;
        leftValue = ecc_object_getelement(context, object, a);
        while(b != first + n)
        {
            value = ecc_object_getelement(context, object, b);
            ecc_object_putelement(context, object, a, value);
            a = b;
            if(last - b > shift)
                b += shift;
            else
                b = half - (last - b);
        }
        ecc_object_putelement(context, object, a, leftValue);
    }
}

static int ecc_array_compare(eccarraycomparestate_t* cmp, eccvalue_t left, eccvalue_t right)
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

static uint32_t ecc_array_search(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t last, eccvalue_t right)
{
    eccvalue_t left;
    uint32_t half;

    while(first < last)
    {
        half = (first + last) >> 1;
        left = ecc_object_getelement(&cmp->context, object, half);
        if(ecc_array_compare(cmp, left, right))
            first = half + 1;
        else
            last = half;
    }
    return first;
}

static void ecc_array_merge(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t pivot, uint32_t last, uint32_t len1, uint32_t len2)
{
    uint32_t left, right, half1, half2;

    if(len1 == 0 || len2 == 0)
        return;

    if(len1 + len2 == 2)
    {
        eccvalue_t leftval, rightval;
        leftval = ecc_object_getelement(&cmp->context, object, pivot);
        rightval = ecc_object_getelement(&cmp->context, object, first);
        if(ecc_array_compare(cmp, leftval, rightval))
        {
            ecc_object_putelement(&cmp->context, object, pivot, rightval);
            ecc_object_putelement(&cmp->context, object, first, leftval);
        }
        return;
    }

    if(len1 > len2)
    {
        half1 = len1 >> 1;
        left = first + half1;
        right = ecc_array_search(object, cmp, pivot, last, ecc_object_getelement(&cmp->context, object, first + half1));
        half2 = right - pivot;
    }
    else
    {
        half2 = len2 >> 1;
        left = ecc_array_search(object, cmp, first, pivot, ecc_object_getelement(&cmp->context, object, pivot + half2));
        right = pivot + half2;
        half1 = left - first;
    }
    ecc_array_rotate(object, &cmp->context, left, pivot, right);

    pivot = left + half2;
    ecc_array_merge(object, cmp, first, left, pivot, half1, half2);
    ecc_array_merge(object, cmp, pivot, right, last, len1 - half1, len2 - half2);
}

static void ecc_array_sortandmerge(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t last)
{
    uint32_t half;

    if(last - first < 8)
    {
        eccvalue_t left, right;
        uint32_t i, j;

        for(i = first + 1; i < last; ++i)
        {
            right = ecc_object_getelement(&cmp->context, object, i);
            for(j = i; j > first; --j)
            {
                left = ecc_object_getelement(&cmp->context, object, j - 1);
                if(ecc_array_compare(cmp, left, right))
                    break;
                else
                    ecc_object_putelement(&cmp->context, object, j, left);
            }
            ecc_object_putelement(&cmp->context, object, j, right);
        }
        return;
    }

    half = (first + last) >> 1;
    ecc_array_sortandmerge(object, cmp, first, half);
    ecc_array_sortandmerge(object, cmp, half, last);
    ecc_array_merge(object, cmp, first, half, last, half - first, last - half);
}

void ecc_array_sortinplace(eccstate_t* context, eccobject_t* object, eccobjfunction_t* function, int first, int last)
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
        eccobject_t* environment = ecc_object_copy(&function->environment);

        cmp.context.environment = environment;
        cmp.arguments = ecc_args_createsized(2);
        ++cmp.arguments->referenceCount;

        environment->hashmap[2].value = ecc_value_object(cmp.arguments);

        ecc_array_sortandmerge(object, &cmp, first, last);
    }
    else
    {
        eccobject_t environment;
        eccobject_t arguments;
        memset(&arguments, 0, sizeof(eccobject_t));
        memset(&environment, 0, sizeof(eccobject_t));

        if(function)
        {
            environment = function->environment;
        }
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

        ecc_array_sortandmerge(object, &cmp, first, last);
    }
}

static eccvalue_t objarrayfn_sort(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t compare;
    uint32_t count;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    count = ecc_value_tointeger(context, ecc_object_getmember(context, thisobj, ECC_ConstKey_length)).data.integer;
    compare = ecc_context_argument(context, 0);

    if(compare.type == ECC_VALTYPE_FUNCTION)
        ecc_array_sortinplace(context, thisobj, compare.data.function, 0, count);
    else if(compare.type == ECC_VALTYPE_UNDEFINED)
        ecc_array_sortinplace(context, thisobj, NULL, 0, count);
    else
        ecc_context_typeerror(context, ecc_charbuf_create("comparison function must be a function or undefined"));

    return ecc_value_object(thisobj);
}

static eccvalue_t objarrayfn_splice(eccstate_t* context)
{
    eccobject_t* thisobj, *result;
    uint32_t length, from, to, count = 0, add = 0, start = 0, delindex = 0;

    count = ecc_context_argumentcount(context);
    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    length = ecc_array_getlength(context, thisobj);

    if(count >= 1)
    {
        double binary = ecc_value_tobinary(context, ecc_context_argument(context, 0)).data.binary;
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
        double binary = ecc_value_tobinary(context, ecc_context_argument(context, 1)).data.binary;
        if(isnan(binary) || binary < 0)
            binary = 0;
        else if(binary > length - start)
            binary = length - start;

        delindex = binary;
    }

    if(count > 2)
        add = count - 2;

    if(length - delindex +add > length)
        ecc_array_objectresize(context, thisobj, length - delindex +add);

    result = ecc_array_createsized(delindex);

    for(from = start, to = 0; to < delindex; ++from, ++to)
        ecc_object_putelement(context, result, to, ecc_object_getelement(context, thisobj, from));

    if(delindex > add)
    {
        for(from = start + delindex, to = start + add; from < length; ++from, ++to)
            ecc_object_putelement(context, thisobj, to, ecc_object_getelement(context, thisobj, from));

        for(; to < length; ++to)
            ecc_object_putelement(context, thisobj, to, ECCValConstNone);
    }
    else if(delindex < add)
        for(from = length, to = length + add - delindex; from > start;)
            ecc_object_putelement(context, thisobj, --to, ecc_object_getelement(context, thisobj, --from));

    for(from = 2, to = start; from < count; ++from, ++to)
        ecc_object_putelement(context, thisobj, to, ecc_context_argument(context, from));

    if(length - delindex +add <= length)
        ecc_array_objectresize(context, thisobj, length - delindex +add);

    return ecc_value_object(result);
}

static eccvalue_t objarrayfn_indexOf(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t search, start;
    uint32_t length = 0;
    int32_t index;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    length = ecc_array_getlength(context, thisobj);

    search = ecc_context_argument(context, 0);
    start = ecc_value_tointeger(context, ecc_context_argument(context, 1));
    index = ((start.data.integer < 0) ? ((int32_t)(length + start.data.integer)) : (int32_t)start.data.integer);
    

    if(index < 0)
        index = 0;

    for(; index < (int32_t)length; ++index)
        if(ecc_value_istrue(ecc_value_same(context, search, ecc_object_getelement(context, thisobj, index))))
            return ecc_value_binary(index);

    return ecc_value_binary(-1);
}

static eccvalue_t objarrayfn_lastIndexOf(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t search, start;
    uint32_t length = 0;
    int32_t index;

    thisobj = ecc_value_toobject(context, ecc_context_this(context)).data.object;
    length = ecc_array_getlength(context, thisobj);

    search = ecc_context_argument(context, 0);
    start = ecc_value_tointeger(context, ecc_context_argument(context, 1));
    index = ((start.data.integer <= 0) ? ((uint32_t)(length + start.data.integer)) : ((uint32_t)(start.data.integer + 1)));

    if(index < 0)
        index = 0;
    else if(index > (int32_t)length)
        index = length;

    for(; index--;)
        if(ecc_value_istrue(ecc_value_same(context, search, ecc_object_getelement(context, thisobj, index))))
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

    length = ecc_value_tobinary(context, ecc_context_argument(context, 0)).data.binary;
    if(!isfinite(length) || length < 0 || length > UINT32_MAX || length != (uint32_t)length)
        ecc_context_rangeerror(context, ecc_charbuf_create("invalid array length"));

    if(ecc_object_resizeelement(context->thisvalue.data.object, length) && context->parent->strictMode)
        ecc_context_typeerror(context, ecc_charbuf_create("'%u' is non-configurable", context->thisvalue.data.object->elementCount));

    return ECCValConstUndefined;
}

static eccvalue_t objarrayfn_constructor(eccstate_t* context)
{
    eccvalue_t value;
    uint32_t index, count, length;
    eccobject_t* array;

    length = count = ecc_context_argumentcount(context);
    value = ecc_context_argument(context, 0);
    if(count == 1 && ecc_value_isnumber(value) && ecc_value_isprimitive(value))
    {
        double binary = ecc_value_tobinary(context, value).data.binary;
        if(isfinite(binary) && binary >= 0 && binary <= UINT32_MAX && binary == (uint32_t)binary)
        {
            length = binary;
            count = 0;
        }
        else
            ecc_context_rangeerror(context, ecc_charbuf_create("invalid array length"));
    }

    array = ecc_array_createsized(length);

    for(index = 0; index < count; ++index)
    {
        array->element[index].value = ecc_context_argument(context, index);
        array->element[index].value.flags &= ~(ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    }

    return ecc_value_object(array);
}

void ecc_array_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Array, objarrayfn_constructor, -1, &ECC_Prototype_Array, ecc_value_object(ecc_array_createsized(0)), &ECC_Type_Array);

    ecc_function_addmethod(ECC_CtorFunc_Array, "isArray", objarrayfn_isArray, 1, h);

    ecc_function_addto(ECC_Prototype_Array, "toString", objarrayfn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_Array, "toLocaleString", objarrayfn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_Array, "concat", objarrayfn_concat, -1, h);
    ecc_function_addto(ECC_Prototype_Array, "map", objarrayfn_map, 1, h);
    ecc_function_addto(ECC_Prototype_Array, "join", objarrayfn_join, 1, h);
    ecc_function_addto(ECC_Prototype_Array, "pop", objarrayfn_pop, 0, h);
    ecc_function_addto(ECC_Prototype_Array, "push", objarrayfn_push, -1, h);
    ecc_function_addto(ECC_Prototype_Array, "reverse", objarrayfn_reverse, 0, h);
    ecc_function_addto(ECC_Prototype_Array, "shift", objarrayfn_shift, 0, h);
    ecc_function_addto(ECC_Prototype_Array, "slice", objarrayfn_slice, 2, h);
    ecc_function_addto(ECC_Prototype_Array, "sort", objarrayfn_sort, 1, h);
    ecc_function_addto(ECC_Prototype_Array, "splice", objarrayfn_splice, -2, h);
    ecc_function_addto(ECC_Prototype_Array, "unshift", objarrayfn_unshift, -1, h);
    ecc_function_addto(ECC_Prototype_Array, "indexOf", objarrayfn_indexOf, -1, h);
    ecc_function_addto(ECC_Prototype_Array, "lastIndexOf", objarrayfn_lastIndexOf, -1, h);

    ecc_object_addmember(ECC_Prototype_Array, ECC_ConstKey_length, ecc_function_accessor(objarrayfn_getLength, objarrayfn_setLength), h | s | ECC_VALFLAG_ASOWN | ECC_VALFLAG_ASDATA);
}

void ecc_array_teardown(void)
{
    ECC_Prototype_Array = NULL;
    ECC_CtorFunc_Array = NULL;
}

eccobject_t* ecc_array_create(void)
{
    return ecc_array_createsized(0);
}

eccobject_t* ecc_array_createsized(uint32_t size)
{
    eccobject_t* self = ecc_object_create(ECC_Prototype_Array);

    ecc_object_resizeelement(self, size);

    return self;
}
