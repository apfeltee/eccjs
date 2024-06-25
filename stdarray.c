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
    eccobjscriptfunction_t* function;
    eccobject_t* arguments;
    const eccoperand_t* ops;
};

eccobject_t* io_libecc_array_prototype = NULL;
eccobjscriptfunction_t* io_libecc_array_constructor = NULL;

const eccobjinterntype_t io_libecc_array_type = {
    .text = &ECC_ConstString_ArrayType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static eccobject_t* create(void);
static eccobject_t* createSized(uint32_t size);
const struct type_io_libecc_Array io_libecc_Array = {
    setup,
    teardown,
    create,
    createSized,
    {}
};

static int valueIsArray(eccvalue_t value)
{
    return ECCNSValue.isObject(value) && ECCNSValue.objectIsArray(value.data.object);
}

static uint32_t valueArrayLength(eccvalue_t value)
{
    if(valueIsArray(value))
        return value.data.object->elementCount;

    return 1;
}

static uint32_t objectLength(eccstate_t* context, eccobject_t* object)
{
    if(object->type == &io_libecc_array_type)
        return object->elementCount;
    else
        return ECCNSValue.toInteger(context, ECCNSObject.getMember(context, object, io_libecc_key_length)).data.integer;
}

static void objectResize(eccstate_t* context, eccobject_t* object, uint32_t length)
{
    if(object->type == &io_libecc_array_type)
    {
        if(ECCNSObject.resizeElement(object, length) && context->parent->strictMode)
        {
            ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);
            ECCNSContext.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", length));
        }
    }
    else
        ECCNSObject.putMember(context, object, io_libecc_key_length, ECCNSValue.binary(length));
}

static void valueAppendFromElement(eccstate_t* context, eccvalue_t value, eccobject_t* object, uint32_t* element)
{
    uint32_t index;

    if(valueIsArray(value))
        for(index = 0; index < value.data.object->elementCount; ++index)
            ECCNSObject.putElement(context, object, (*element)++, ECCNSObject.getElement(context, value.data.object, index));
    else
        ECCNSObject.putElement(context, object, (*element)++, value);
}

static eccvalue_t isArray(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);

    return ECCNSValue.truth(value.type == ECC_VALTYPE_OBJECT && value.data.object->type == &io_libecc_array_type);
}

static eccvalue_t toChars(eccstate_t* context, eccvalue_t thisval, ecctextstring_t separator)
{
    eccobject_t* object = thisval.data.object;
    eccvalue_t value, length = ECCNSObject.getMember(context, object, io_libecc_key_length);
    uint32_t index, count = ECCNSValue.toInteger(context, length).data.integer;
    eccappendbuffer_t chars;

    io_libecc_Chars.beginAppend(&chars);
    for(index = 0; index < count; ++index)
    {
        value = ECCNSObject.getElement(context, thisval.data.object, index);

        if(index)
            io_libecc_Chars.append(&chars, "%.*s", separator.length, separator.bytes);

        if(value.type != ECC_VALTYPE_UNDEFINED && value.type != ECC_VALTYPE_NULL)
            io_libecc_Chars.appendValue(&chars, context, value);
    }

    return io_libecc_Chars.endAppend(&chars);
}

static eccvalue_t toString(eccstate_t* context)
{
    eccvalue_t function;

    context->thisvalue = ECCNSValue.toObject(context, ECCNSContext.getThis(context));
    function = ECCNSObject.getMember(context, context->thisvalue.data.object, io_libecc_key_join);

    if(function.type == ECC_VALTYPE_FUNCTION)
        return ECCNSContext.callFunction(context, function.data.function, context->thisvalue, 0);
    else
        return ECCNSObject.toString(context);
}

static eccvalue_t concat(eccstate_t* context)
{
    eccvalue_t value;
    uint32_t element = 0, length = 0, index, count;
    eccobject_t* array = NULL;

    value = ECCNSValue.toObject(context, ECCNSContext.getThis(context));
    count = ECCNSContext.argumentCount(context);

    length += valueArrayLength(value);
    for(index = 0; index < count; ++index)
        length += valueArrayLength(ECCNSContext.argument(context, index));

    array = io_libecc_Array.createSized(length);

    valueAppendFromElement(context, value, array, &element);
    for(index = 0; index < count; ++index)
        valueAppendFromElement(context, ECCNSContext.argument(context, index), array, &element);

    return ECCNSValue.object(array);
}

static eccvalue_t join(eccstate_t* context)
{
    eccvalue_t object;
    eccvalue_t value;
    ecctextstring_t separator;

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        separator = ECCNSText.make(",", 1);
    else
    {
        value = ECCNSValue.toString(context, value);
        separator = ECCNSValue.textOf(&value);
    }

    object = ECCNSValue.toObject(context, ECCNSContext.getThis(context));

    return toChars(context, object, separator);
}

static eccvalue_t pop(eccstate_t* context)
{
    eccvalue_t value = ECCValConstUndefined;
    eccobject_t* thisobj;
    uint32_t length;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    length = objectLength(context, thisobj);

    if(length)
    {
        --length;
        value = ECCNSObject.getElement(context, thisobj, length);

        if(!ECCNSObject.deleteElement(thisobj, length) && context->parent->strictMode)
        {
            ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);
            ECCNSContext.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", length));
        }
    }
    objectResize(context, thisobj, length);

    return value;
}

static eccvalue_t push(eccstate_t* context)
{
    eccobject_t* thisobj;
    uint32_t length = 0, index, count, base;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    count = ECCNSContext.argumentCount(context);

    base = objectLength(context, thisobj);
    length = UINT32_MAX - base < count ? UINT32_MAX : base + count;
    objectResize(context, thisobj, length);

    for(index = base; index < length; ++index)
        ECCNSObject.putElement(context, thisobj, index, ECCNSContext.argument(context, index - base));

    if(UINT32_MAX - base < count)
    {
        ECCNSObject.putElement(context, thisobj, index, ECCNSContext.argument(context, index - base));

        if(thisobj->type == &io_libecc_array_type)
            ECCNSContext.rangeError(context, io_libecc_Chars.create("max length exeeded"));
        else
        {
            double subidx, sublen = (double)base + count;
            for(subidx = (double)UINT32_MAX + 1; subidx < sublen; ++subidx)
                ECCNSObject.putProperty(context, thisobj, ECCNSValue.binary(subidx), ECCNSContext.argument(context, subidx - base));

            ECCNSObject.putMember(context, thisobj, io_libecc_key_length, ECCNSValue.binary(sublen));
            return ECCNSValue.binary(sublen);
        }
    }

    return ECCNSValue.binary(length);
}

static eccvalue_t reverse(eccstate_t* context)
{
    eccvalue_t temp;
    eccobject_t* thisobj;
    uint32_t index, half, last, length;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    length = objectLength(context, thisobj);

    last = length - 1;
    half = length / 2;

    ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);

    for(index = 0; index < half; ++index)
    {
        temp = ECCNSObject.getElement(context, thisobj, index);
        ECCNSObject.putElement(context, thisobj, index, ECCNSObject.getElement(context, thisobj, last - index));
        ECCNSObject.putElement(context, thisobj, last - index, temp);
    }

    return ECCNSValue.object(thisobj);
}

static eccvalue_t shift(eccstate_t* context)
{
    eccvalue_t result;
    eccobject_t* thisobj;
    uint32_t index, count, length;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    length = objectLength(context, thisobj);

    ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);

    if(length)
    {
        length--;
        result = ECCNSObject.getElement(context, thisobj, 0);

        for(index = 0, count = length; index < count; ++index)
            ECCNSObject.putElement(context, thisobj, index, ECCNSObject.getElement(context, thisobj, index + 1));

        if(!ECCNSObject.deleteElement(thisobj, length) && context->parent->strictMode)
        {
            ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);
            ECCNSContext.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", length));
        }
    }
    else
        result = ECCValConstUndefined;

    objectResize(context, thisobj, length);

    return result;
}

static eccvalue_t unshift(eccstate_t* context)
{
    eccobject_t* thisobj;
    uint32_t length = 0, index, count;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    count = ECCNSContext.argumentCount(context);

    length = objectLength(context, thisobj) + count;
    objectResize(context, thisobj, length);

    ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);

    for(index = count; index < length; ++index)
        ECCNSObject.putElement(context, thisobj, index, ECCNSObject.getElement(context, thisobj, index - count));

    for(index = 0; index < count; ++index)
        ECCNSObject.putElement(context, thisobj, index, ECCNSContext.argument(context, index));

    return ECCNSValue.binary(length);
}

static eccvalue_t slice(eccstate_t* context)
{
    eccobject_t* thisobj, *result;
    eccvalue_t start, end;
    uint32_t from, to, length;
    double binary;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    length = objectLength(context, thisobj);

    start = ECCNSContext.argument(context, 0);
    binary = ECCNSValue.toBinary(context, start).data.binary;
    if(start.type == ECC_VALTYPE_UNDEFINED)
        from = 0;
    else if(binary >= 0)
        from = binary < length ? binary : length;
    else
        from = binary + length >= 0 ? length + binary : 0;

    end = ECCNSContext.argument(context, 1);
    binary = ECCNSValue.toBinary(context, end).data.binary;
    if(end.type == ECC_VALTYPE_UNDEFINED)
        to = length;
    else if(binary < 0 || isnan(binary))
        to = binary + length >= 0 ? length + binary : 0;
    else
        to = binary < length ? binary : length;

    ECCNSContext.setTextIndex(context, io_libecc_context_callIndex);

    if(to > from)
    {
        length = to - from;
        result = io_libecc_Array.createSized(length);

        for(to = 0; to < length; ++from, ++to)
            ECCNSObject.putElement(context, result, to, ECCNSObject.getElement(context, thisobj, from));
    }
    else
        result = io_libecc_Array.createSized(0);

    return ECCNSValue.object(result);
}

static eccvalue_t defaultComparison(eccstate_t* context)
{
    eccvalue_t left, right, result;

    left = ECCNSContext.argument(context, 0);
    right = ECCNSContext.argument(context, 1);
    result = ECCNSValue.less(context, ECCNSValue.toString(context, left), ECCNSValue.toString(context, right));

    return ECCNSValue.integer(ECCNSValue.isTrue(result) ? -1 : 0);
}

static inline int gcd(int m, int n)
{
    while(n)
    {
        int t = m % n;
        m = n;
        n = t;
    }
    return m;
}

static inline void rotate(eccobject_t* object, eccstate_t* context, uint32_t first, uint32_t half, uint32_t last)
{
    eccvalue_t value, leftValue;
    uint32_t n, shift, a, b;

    if(first == half || half == last)
        return;

    n = gcd(last - first, half - first);
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

static inline int compare(eccarraycomparestate_t* cmp, eccvalue_t left, eccvalue_t right)
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

    return ECCNSValue.toInteger(&cmp->context, cmp->context.ops->native(&cmp->context)).data.integer < 0;
}

static inline uint32_t search(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t last, eccvalue_t right)
{
    eccvalue_t left;
    uint32_t half;

    while(first < last)
    {
        half = (first + last) >> 1;
        left = ECCNSObject.getElement(&cmp->context, object, half);
        if(compare(cmp, left, right))
            first = half + 1;
        else
            last = half;
    }
    return first;
}

static inline void merge(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t pivot, uint32_t last, uint32_t len1, uint32_t len2)
{
    uint32_t left, right, half1, half2;

    if(len1 == 0 || len2 == 0)
        return;

    if(len1 + len2 == 2)
    {
        eccvalue_t leftval, rightval;
        leftval = ECCNSObject.getElement(&cmp->context, object, pivot);
        rightval = ECCNSObject.getElement(&cmp->context, object, first);
        if(compare(cmp, leftval, rightval))
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
        right = search(object, cmp, pivot, last, ECCNSObject.getElement(&cmp->context, object, first + half1));
        half2 = right - pivot;
    }
    else
    {
        half2 = len2 >> 1;
        left = search(object, cmp, first, pivot, ECCNSObject.getElement(&cmp->context, object, pivot + half2));
        right = pivot + half2;
        half1 = left - first;
    }
    rotate(object, &cmp->context, left, pivot, right);

    pivot = left + half2;
    merge(object, cmp, first, left, pivot, half1, half2);
    merge(object, cmp, pivot, right, last, len1 - half1, len2 - half2);
}

static void sortAndMerge(eccobject_t* object, eccarraycomparestate_t* cmp, uint32_t first, uint32_t last)
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
                if(compare(cmp, left, right))
                    break;
                else
                    ECCNSObject.putElement(&cmp->context, object, j, left);
            }
            ECCNSObject.putElement(&cmp->context, object, j, right);
        }
        return;
    }

    half = (first + last) >> 1;
    sortAndMerge(object, cmp, first, half);
    sortAndMerge(object, cmp, half, last);
    merge(object, cmp, first, half, last, half - first, last - half);
}

static void sortInPlace(eccstate_t* context, eccobject_t* object, eccobjscriptfunction_t* function, int first, int last)
{
    eccoperand_t defaultOps = { defaultComparison, ECCValConstUndefined, ECC_ConstString_NativeCode };
    const eccoperand_t* ops = function ? function->oplist->ops : &defaultOps;

    eccarraycomparestate_t cmp = {
        {
        .thisvalue = ECCNSValue.object(object),
        .parent = context,
        .ecc = context->ecc,
        .depth = context->depth + 1,
        .ops = ops,
        .textIndex = io_libecc_context_callIndex,
        },
        function,
        NULL,
        cmp.ops = ops,
    };

    if(function && function->flags & io_libecc_function_needHeap)
    {
        eccobject_t* environment = ECCNSObject.copy(&function->environment);

        cmp.context.environment = environment;
        cmp.arguments = ECCNSArguments.createSized(2);
        ++cmp.arguments->referenceCount;

        environment->hashmap[2].value = ECCNSValue.object(cmp.arguments);

        sortAndMerge(object, &cmp, first, last);
    }
    else
    {
        eccobject_t environment = function ? function->environment : ECCNSObject.identity;
        eccobject_t arguments = ECCNSObject.identity;
        ecchashmap_t hashmap[function ? function->environment.hashmapCapacity : 3];
        eccobjelement_t element[2];

        if(function)
            memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        else
            environment.hashmapCount = 3;

        cmp.context.environment = &environment;
        cmp.arguments = &arguments;

        arguments.element = element;
        arguments.elementCount = 2;
        environment.hashmap = hashmap;
        environment.hashmap[2].value = ECCNSValue.object(&arguments);

        sortAndMerge(object, &cmp, first, last);
    }
}

static eccvalue_t sort(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t compare;
    uint32_t count;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    count = ECCNSValue.toInteger(context, ECCNSObject.getMember(context, thisobj, io_libecc_key_length)).data.integer;
    compare = ECCNSContext.argument(context, 0);

    if(compare.type == ECC_VALTYPE_FUNCTION)
        sortInPlace(context, thisobj, compare.data.function, 0, count);
    else if(compare.type == ECC_VALTYPE_UNDEFINED)
        sortInPlace(context, thisobj, NULL, 0, count);
    else
        ECCNSContext.typeError(context, io_libecc_Chars.create("comparison function must be a function or undefined"));

    return ECCNSValue.object(thisobj);
}

static eccvalue_t splice(eccstate_t* context)
{
    eccobject_t* thisobj, *result;
    uint32_t length, from, to, count = 0, add = 0, start = 0, delete = 0;

    count = ECCNSContext.argumentCount(context);
    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    length = objectLength(context, thisobj);

    if(count >= 1)
    {
        double binary = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0)).data.binary;
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
        double binary = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 1)).data.binary;
        if(isnan(binary) || binary < 0)
            binary = 0;
        else if(binary > length - start)
            binary = length - start;

        delete = binary;
    }

    if(count > 2)
        add = count - 2;

    if(length - delete +add > length)
        objectResize(context, thisobj, length - delete +add);

    result = io_libecc_Array.createSized(delete);

    for(from = start, to = 0; to < delete; ++from, ++to)
        ECCNSObject.putElement(context, result, to, ECCNSObject.getElement(context, thisobj, from));

    if(delete > add)
    {
        for(from = start + delete, to = start + add; from < length; ++from, ++to)
            ECCNSObject.putElement(context, thisobj, to, ECCNSObject.getElement(context, thisobj, from));

        for(; to < length; ++to)
            ECCNSObject.putElement(context, thisobj, to, ECCValConstNone);
    }
    else if(delete < add)
        for(from = length, to = length + add - delete; from > start;)
            ECCNSObject.putElement(context, thisobj, --to, ECCNSObject.getElement(context, thisobj, --from));

    for(from = 2, to = start; from < count; ++from, ++to)
        ECCNSObject.putElement(context, thisobj, to, ECCNSContext.argument(context, from));

    if(length - delete +add <= length)
        objectResize(context, thisobj, length - delete +add);

    return ECCNSValue.object(result);
}

static eccvalue_t indexOf(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t search, start;
    uint32_t length = 0;
    int32_t index;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    length = objectLength(context, thisobj);

    search = ECCNSContext.argument(context, 0);
    start = ECCNSValue.toInteger(context, ECCNSContext.argument(context, 1));
    index = ((start.data.integer < 0) ? ((int32_t)(length + start.data.integer)) : (int32_t)start.data.integer);
    

    if(index < 0)
        index = 0;

    for(; index < (int32_t)length; ++index)
        if(ECCNSValue.isTrue(ECCNSValue.same(context, search, ECCNSObject.getElement(context, thisobj, index))))
            return ECCNSValue.binary(index);

    return ECCNSValue.binary(-1);
}

static eccvalue_t lastIndexOf(eccstate_t* context)
{
    eccobject_t* thisobj;
    eccvalue_t search, start;
    uint32_t length = 0;
    int32_t index;

    thisobj = ECCNSValue.toObject(context, ECCNSContext.getThis(context)).data.object;
    length = objectLength(context, thisobj);

    search = ECCNSContext.argument(context, 0);
    start = ECCNSValue.toInteger(context, ECCNSContext.argument(context, 1));
    index = ((start.data.integer <= 0) ? ((uint32_t)(length + start.data.integer)) : ((uint32_t)(start.data.integer + 1)));

    if(index < 0)
        index = 0;
    else if(index > (int32_t)length)
        index = length;

    for(; index--;)
        if(ECCNSValue.isTrue(ECCNSValue.same(context, search, ECCNSObject.getElement(context, thisobj, index))))
            return ECCNSValue.binary(index);

    return ECCNSValue.binary(-1);
}

static eccvalue_t getLength(eccstate_t* context)
{
    return ECCNSValue.binary(context->thisvalue.data.object->elementCount);
}

static eccvalue_t setLength(eccstate_t* context)
{
    double length;

    length = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0)).data.binary;
    if(!isfinite(length) || length < 0 || length > UINT32_MAX || length != (uint32_t)length)
        ECCNSContext.rangeError(context, io_libecc_Chars.create("invalid array length"));

    if(ECCNSObject.resizeElement(context->thisvalue.data.object, length) && context->parent->strictMode)
        ECCNSContext.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", context->thisvalue.data.object->elementCount));

    return ECCValConstUndefined;
}

static eccvalue_t constructor(eccstate_t* context)
{
    eccvalue_t value;
    uint32_t index, count, length;
    eccobject_t* array;

    length = count = ECCNSContext.argumentCount(context);
    value = ECCNSContext.argument(context, 0);
    if(count == 1 && ECCNSValue.isNumber(value) && ECCNSValue.isPrimitive(value))
    {
        double binary = ECCNSValue.toBinary(context, value).data.binary;
        if(isfinite(binary) && binary >= 0 && binary <= UINT32_MAX && binary == (uint32_t)binary)
        {
            length = binary;
            count = 0;
        }
        else
            ECCNSContext.rangeError(context, io_libecc_Chars.create("invalid array length"));
    }

    array = io_libecc_Array.createSized(length);

    for(index = 0; index < count; ++index)
    {
        array->element[index].value = ECCNSContext.argument(context, index);
        array->element[index].value.flags &= ~(ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    }

    return ECCNSValue.object(array);
}

// MARK: - Methods

void setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    io_libecc_Function.setupBuiltinObject(&io_libecc_array_constructor, constructor, -1, &io_libecc_array_prototype, ECCNSValue.object(createSized(0)), &io_libecc_array_type);

    io_libecc_Function.addMethod(io_libecc_array_constructor, "isArray", isArray, 1, h);

    io_libecc_Function.addToObject(io_libecc_array_prototype, "toString", toString, 0, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "toLocaleString", toString, 0, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "concat", concat, -1, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "join", join, 1, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "pop", pop, 0, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "push", push, -1, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "reverse", reverse, 0, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "shift", shift, 0, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "slice", slice, 2, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "sort", sort, 1, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "splice", splice, -2, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "unshift", unshift, -1, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "indexOf", indexOf, -1, h);
    io_libecc_Function.addToObject(io_libecc_array_prototype, "lastIndexOf", lastIndexOf, -1, h);

    ECCNSObject.addMember(io_libecc_array_prototype, io_libecc_key_length, io_libecc_Function.accessor(getLength, setLength), h | s | ECC_VALFLAG_ASOWN | ECC_VALFLAG_ASDATA);
}

void teardown(void)
{
    io_libecc_array_prototype = NULL;
    io_libecc_array_constructor = NULL;
}

eccobject_t* create(void)
{
    return createSized(0);
}

eccobject_t* createSized(uint32_t size)
{
    eccobject_t* self = ECCNSObject.create(io_libecc_array_prototype);

    ECCNSObject.resizeElement(self, size);

    return self;
}
