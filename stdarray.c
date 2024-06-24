//
//  array.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

// MARK: - Private

struct eccobject_t * io_libecc_array_prototype = NULL;
struct io_libecc_Function * io_libecc_array_constructor = NULL;

const struct io_libecc_object_Type io_libecc_array_type = {
	.text = &io_libecc_text_arrayType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static struct eccobject_t* create(void);
static struct eccobject_t* createSized(uint32_t size);
const struct type_io_libecc_Array io_libecc_Array = {
    setup,
    teardown,
    create,
    createSized,
};

static
int valueIsArray (struct eccvalue_t value)
{
	return io_libecc_Value.isObject(value) && io_libecc_Value.objectIsArray(value.data.object);
}

static
uint32_t valueArrayLength (struct eccvalue_t value)
{
	if (valueIsArray(value))
		return value.data.object->elementCount;
	
	return 1;
}

static
uint32_t objectLength (struct eccstate_t * const context, struct eccobject_t *object)
{
	if (object->type == &io_libecc_array_type)
		return object->elementCount;
	else
		return io_libecc_Value.toInteger(context, io_libecc_Object.getMember(context, object, io_libecc_key_length)).data.integer;
}

static
void objectResize (struct eccstate_t * const context, struct eccobject_t *object, uint32_t length)
{
	if (object->type == &io_libecc_array_type)
	{
		if (io_libecc_Object.resizeElement(object, length) && context->parent->strictMode)
		{
			io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", length));
		}
	}
	else
		io_libecc_Object.putMember(context, object, io_libecc_key_length, io_libecc_Value.binary(length));
}

static
void valueAppendFromElement (struct eccstate_t * const context, struct eccvalue_t value, struct eccobject_t *object, uint32_t *element)
{
	uint32_t index;
	
	if (valueIsArray(value))
		for (index = 0; index < value.data.object->elementCount; ++index)
			io_libecc_Object.putElement(context, object, (*element)++, io_libecc_Object.getElement(context, value.data.object, index));
	else
		io_libecc_Object.putElement(context, object, (*element)++, value);
}

static
struct eccvalue_t isArray (struct eccstate_t * const context)
{
	struct eccvalue_t value;
	
	value = io_libecc_Context.argument(context, 0);
	
	return io_libecc_Value.truth(value.type == io_libecc_value_objectType && value.data.object->type == &io_libecc_array_type);
}

static
struct eccvalue_t toChars (struct eccstate_t * const context, struct eccvalue_t this, struct io_libecc_Text separator)
{
	struct eccobject_t *object = this.data.object;
	struct eccvalue_t value, length = io_libecc_Object.getMember(context, object, io_libecc_key_length);
	uint32_t index, count = io_libecc_Value.toInteger(context, length).data.integer;
	struct io_libecc_chars_Append chars;
	
	io_libecc_Chars.beginAppend(&chars);
	for (index = 0; index < count; ++index)
	{
		value = io_libecc_Object.getElement(context, this.data.object, index);
		
		if (index)
			io_libecc_Chars.append(&chars, "%.*s", separator.length, separator.bytes);
		
		if (value.type != io_libecc_value_undefinedType && value.type != io_libecc_value_nullType)
			io_libecc_Chars.appendValue(&chars, context, value);
	}
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct eccvalue_t toString (struct eccstate_t * const context)
{
	struct eccvalue_t function;
	
	context->this = io_libecc_Value.toObject(context, io_libecc_Context.this(context));
	function = io_libecc_Object.getMember(context, context->this.data.object, io_libecc_key_join);
	
	if (function.type == io_libecc_value_functionType)
		return io_libecc_Context.callFunction(context, function.data.function, context->this, 0);
	else
		return io_libecc_Object.toString(context);
}

static
struct eccvalue_t concat (struct eccstate_t * const context)
{
	struct eccvalue_t value;
	uint32_t element = 0, length = 0, index, count;
	struct eccobject_t *array = NULL;
	
	value = io_libecc_Value.toObject(context, io_libecc_Context.this(context));
	count = io_libecc_Context.argumentCount(context);
	
	length += valueArrayLength(value);
	for (index = 0; index < count; ++index)
		length += valueArrayLength(io_libecc_Context.argument(context, index));
	
	array = io_libecc_Array.createSized(length);
	
	valueAppendFromElement(context, value, array, &element);
	for (index = 0; index < count; ++index)
		valueAppendFromElement(context, io_libecc_Context.argument(context, index), array, &element);
	
	return io_libecc_Value.object(array);
}

static
struct eccvalue_t join (struct eccstate_t * const context)
{
	struct eccvalue_t object;
	struct eccvalue_t value;
	struct io_libecc_Text separator;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type == io_libecc_value_undefinedType)
		separator = io_libecc_Text.make(",", 1);
	else
	{
		value = io_libecc_Value.toString(context, value);
		separator = io_libecc_Value.textOf(&value);
	}
	
	object = io_libecc_Value.toObject(context, io_libecc_Context.this(context));
	
	return toChars(context, object, separator);
}

static
struct eccvalue_t pop (struct eccstate_t * const context)
{
	struct eccvalue_t value = io_libecc_value_undefined;
	struct eccobject_t *this;
	uint32_t length;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	length = objectLength(context, this);
	
	if (length)
	{
		--length;
		value = io_libecc_Object.getElement(context, this, length);
		
		if (!io_libecc_Object.deleteElement(this, length) && context->parent->strictMode)
		{
			io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", length));
		}
	}
	objectResize(context, this, length);
	
	return value;
}

static
struct eccvalue_t push (struct eccstate_t * const context)
{
	struct eccobject_t *this;
	uint32_t length = 0, index, count, base;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	count = io_libecc_Context.argumentCount(context);
	
	base = objectLength(context, this);
	length = UINT32_MAX - base < count? UINT32_MAX: base + count;
	objectResize(context, this, length);
	
	for (index = base; index < length; ++index)
		io_libecc_Object.putElement(context, this, index, io_libecc_Context.argument(context, index - base));
	
	if (UINT32_MAX - base < count)
	{
		io_libecc_Object.putElement(context, this, index, io_libecc_Context.argument(context, index - base));
		
		if (this->type == &io_libecc_array_type)
			io_libecc_Context.rangeError(context, io_libecc_Chars.create("max length exeeded"));
		else
		{
			double index, length = (double)base + count;
			for (index = (double)UINT32_MAX + 1; index < length; ++index)
				io_libecc_Object.putProperty(context, this, io_libecc_Value.binary(index), io_libecc_Context.argument(context, index - base));
			
			io_libecc_Object.putMember(context, this, io_libecc_key_length, io_libecc_Value.binary(length));
			return io_libecc_Value.binary(length);
		}
	}
	
	return io_libecc_Value.binary(length);
}

static
struct eccvalue_t reverse (struct eccstate_t * const context)
{
	struct eccvalue_t temp;
	struct eccobject_t *this;
	uint32_t index, half, last, length;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	length = objectLength(context, this);
	
	last = length - 1;
	half = length / 2;
	
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	
	for (index = 0; index < half; ++index)
	{
		temp = io_libecc_Object.getElement(context, this, index);
		io_libecc_Object.putElement(context, this, index, io_libecc_Object.getElement(context, this, last - index));
		io_libecc_Object.putElement(context, this, last - index, temp);
	}
	
	return io_libecc_Value.object(this);
}

static
struct eccvalue_t shift (struct eccstate_t * const context)
{
	struct eccvalue_t result;
	struct eccobject_t *this;
	uint32_t index, count, length;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	length = objectLength(context, this);
	
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	
	if (length)
	{
		length--;
		result = io_libecc_Object.getElement(context, this, 0);
		
		for (index = 0, count = length; index < count; ++index)
			io_libecc_Object.putElement(context, this, index, io_libecc_Object.getElement(context, this, index + 1));
		
		if (!io_libecc_Object.deleteElement(this, length) && context->parent->strictMode)
		{
			io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", length));
		}
	}
	else
		result = io_libecc_value_undefined;
	
	objectResize(context, this, length);
	
	return result;
}

static
struct eccvalue_t unshift (struct eccstate_t * const context)
{
	struct eccobject_t *this;
	uint32_t length = 0, index, count;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	count = io_libecc_Context.argumentCount(context);
	
	length = objectLength(context, this) + count;
	objectResize(context, this, length);
	
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	
	for (index = count; index < length; ++index)
		io_libecc_Object.putElement(context, this, index, io_libecc_Object.getElement(context, this, index - count));
	
	for (index = 0; index < count; ++index)
		io_libecc_Object.putElement(context, this, index, io_libecc_Context.argument(context, index));
	
	return io_libecc_Value.binary(length);
}

static
struct eccvalue_t slice (struct eccstate_t * const context)
{
	struct eccobject_t *this, *result;
	struct eccvalue_t start, end;
	uint32_t from, to, length;
	double binary;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	length = objectLength(context, this);
	
	start = io_libecc_Context.argument(context, 0);
	binary = io_libecc_Value.toBinary(context, start).data.binary;
	if (start.type == io_libecc_value_undefinedType)
		from = 0;
	else if (binary >= 0)
		from = binary < length? binary: length;
	else
		from = binary + length >= 0? length + binary: 0;
	
	end = io_libecc_Context.argument(context, 1);
	binary = io_libecc_Value.toBinary(context, end).data.binary;
	if (end.type == io_libecc_value_undefinedType)
		to = length;
	else if (binary < 0 || isnan(binary))
		to = binary + length >= 0? length + binary: 0;
	else
		to = binary < length? binary: length;
	
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	
	if (to > from)
	{
		length = to - from;
		result = io_libecc_Array.createSized(length);
		
		for (to = 0; to < length; ++from, ++to)
			io_libecc_Object.putElement(context, result, to, io_libecc_Object.getElement(context, this, from));
	}
	else
		result = io_libecc_Array.createSized(0);
	
	return io_libecc_Value.object(result);
}

struct Compare {
	struct eccstate_t context;
	struct io_libecc_Function * function;
	struct eccobject_t * arguments;
	const struct io_libecc_Op * ops;
};

static
struct eccvalue_t defaultComparison (struct eccstate_t * const context)
{
	struct eccvalue_t left, right, result;
	
	left = io_libecc_Context.argument(context, 0);
	right = io_libecc_Context.argument(context, 1);
	result = io_libecc_Value.less(context, io_libecc_Value.toString(context, left), io_libecc_Value.toString(context, right));
	
	return io_libecc_Value.integer(io_libecc_Value.isTrue(result)? -1: 0);
}

static inline
int gcd(int m, int n)
{
	while (n)
	{
		int t = m % n;
		m = n;
		n = t;
	}
	return m;
}

static inline
void rotate(struct eccobject_t *object, struct eccstate_t *context, uint32_t first, uint32_t half, uint32_t last)
{
	struct eccvalue_t value, leftValue;
	uint32_t n, shift, a, b;
	
	if (first == half || half == last)
		return;
	
	n = gcd(last - first, half - first);
	while (n--)
	{
		shift = half - first;
		a = first + n;
		b = a + shift;
		leftValue = io_libecc_Object.getElement(context, object, a);
		while (b != first + n)
		{
			value = io_libecc_Object.getElement(context, object, b);
			io_libecc_Object.putElement(context, object, a, value);
			a = b;
			if (last - b > shift)
				b += shift;
			else
				b = half - (last - b);
		}
		io_libecc_Object.putElement(context, object, a, leftValue);
	}
}

static inline
int compare (struct Compare *cmp, struct eccvalue_t left, struct eccvalue_t right)
{
	uint16_t hashmapCount;
	
	if (left.check != 1)
		return 0;
	else if (right.check != 1)
		return 1;
	
	if (left.type == io_libecc_value_undefinedType)
		return 0;
	else if (right.type == io_libecc_value_undefinedType)
		return 1;
	
	hashmapCount = cmp->context.environment->hashmapCount;
	switch (hashmapCount) {
		default:
			memcpy(cmp->context.environment->hashmap + 5,
				   cmp->function->environment.hashmap,
				   sizeof(*cmp->context.environment->hashmap) * (hashmapCount - 5));
		case 5:
			cmp->context.environment->hashmap[3 + 1].value = right;
		case 4:
			cmp->context.environment->hashmap[3 + 0].value = left;
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
	
	return io_libecc_Value.toInteger(&cmp->context, cmp->context.ops->native(&cmp->context)).data.integer < 0;
}

static inline
uint32_t search(struct eccobject_t *object, struct Compare *cmp, uint32_t first, uint32_t last, struct eccvalue_t right)
{
	struct eccvalue_t left;
	uint32_t half;
	
	while (first < last)
	{
		half = (first + last) >> 1;
		left = io_libecc_Object.getElement(&cmp->context, object, half);
		if (compare(cmp, left, right))
			first = half + 1;
		else
			last = half;
	}
	return first;
}

static inline
void merge(struct eccobject_t *object, struct Compare *cmp, uint32_t first, uint32_t pivot, uint32_t last, uint32_t len1, uint32_t len2)
{
	uint32_t left, right, half1, half2;
	
	if (len1 == 0 || len2 == 0)
		return;
	
	if (len1 + len2 == 2)
	{
		struct eccvalue_t left, right;
		left = io_libecc_Object.getElement(&cmp->context, object, pivot);
		right = io_libecc_Object.getElement(&cmp->context, object, first);
		if (compare(cmp, left, right))
		{
			io_libecc_Object.putElement(&cmp->context, object, pivot, right);
			io_libecc_Object.putElement(&cmp->context, object, first, left);
		}
		return;
	}
	
	if (len1 > len2)
	{
		half1 = len1 >> 1;
		left = first + half1;
		right = search(object, cmp, pivot, last, io_libecc_Object.getElement(&cmp->context, object, first + half1));
		half2 = right - pivot;
	}
	else
	{
		half2 = len2 >> 1;
		left = search(object, cmp, first, pivot, io_libecc_Object.getElement(&cmp->context, object, pivot + half2));
		right = pivot + half2;
		half1 = left - first;
	}
	rotate(object, &cmp->context, left, pivot, right);
	
	pivot = left + half2;
	merge(object, cmp, first, left, pivot, half1, half2);
	merge(object, cmp, pivot, right, last, len1 - half1, len2 - half2);
}

static
void sortAndMerge(struct eccobject_t *object, struct Compare *cmp, uint32_t first, uint32_t last)
{
	uint32_t half;
	
	if (last - first < 8)
	{
		struct eccvalue_t left, right;
		uint32_t i, j;
		
		for (i = first + 1; i < last; ++i)
		{
			right = io_libecc_Object.getElement(&cmp->context, object, i);
			for (j = i; j > first; --j)
			{
				left = io_libecc_Object.getElement(&cmp->context, object, j - 1);
				if (compare(cmp, left, right))
					break;
				else
					io_libecc_Object.putElement(&cmp->context, object, j, left);
			}
			io_libecc_Object.putElement(&cmp->context, object, j, right);
		}
		return;
	}
	
	half = (first + last) >> 1;
	sortAndMerge(object, cmp, first, half);
	sortAndMerge(object, cmp, half, last);
	merge(object, cmp, first, half, last, half - first, last - half);
}

static
void sortInPlace (struct eccstate_t * const context, struct eccobject_t *object, struct io_libecc_Function *function, int first, int last)
{
	struct io_libecc_Op defaultOps = { defaultComparison, io_libecc_value_undefined, io_libecc_text_nativeCode };
	const struct io_libecc_Op * ops = function? function->oplist->ops: &defaultOps;
	
	struct Compare cmp = {
		{
			.this = io_libecc_Value.object(object),
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
	
	if (function && function->flags & io_libecc_function_needHeap)
	{
		struct eccobject_t *environment = io_libecc_Object.copy(&function->environment);
		
		cmp.context.environment = environment;
		cmp.arguments = io_libecc_Arguments.createSized(2);
		++cmp.arguments->referenceCount;
		
		environment->hashmap[2].value = io_libecc_Value.object(cmp.arguments);
		
		sortAndMerge(object, &cmp, first, last);
	}
	else
	{
		struct eccobject_t environment = function? function->environment: io_libecc_Object.identity;
		struct eccobject_t arguments = io_libecc_Object.identity;
		union io_libecc_object_Hashmap hashmap[function? function->environment.hashmapCapacity: 3];
		union io_libecc_object_Element element[2];
		
		if (function)
			memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		else
			environment.hashmapCount = 3;
		
		cmp.context.environment = &environment;
		cmp.arguments = &arguments;
		
		arguments.element = element;
		arguments.elementCount = 2;
		environment.hashmap = hashmap;
		environment.hashmap[2].value = io_libecc_Value.object(&arguments);
		
		sortAndMerge(object, &cmp, first, last);
	}
}

static
struct eccvalue_t sort (struct eccstate_t * const context)
{
	struct eccobject_t *this;
	struct eccvalue_t compare;
	uint32_t count;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	count = io_libecc_Value.toInteger(context, io_libecc_Object.getMember(context, this, io_libecc_key_length)).data.integer;
	compare = io_libecc_Context.argument(context, 0);
	
	if (compare.type == io_libecc_value_functionType)
		sortInPlace(context, this, compare.data.function, 0, count);
	else if (compare.type == io_libecc_value_undefinedType)
		sortInPlace(context, this, NULL, 0, count);
	else
		io_libecc_Context.typeError(context, io_libecc_Chars.create("comparison function must be a function or undefined"));
	
	return io_libecc_Value.object(this);
}

static
struct eccvalue_t splice (struct eccstate_t * const context)
{
	struct eccobject_t *this, *result;
	uint32_t length, from, to, count = 0, add = 0, start = 0, delete = 0;
	
	count = io_libecc_Context.argumentCount(context);
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	length = objectLength(context, this);
	
	if (count >= 1)
	{
		double binary = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 0)).data.binary;
		if (isnan(binary))
			binary = 0;
		else if (binary < 0)
			binary += length;
		
		if (binary < 0)
			binary = 0;
		else if (binary > length)
			binary = length;
		
		start = binary;
	}
	
	if (count >= 2)
	{
		double binary = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 1)).data.binary;
		if (isnan(binary) || binary < 0)
			binary = 0;
		else if (binary > length - start)
			binary = length - start;
		
		delete = binary;
	}
	
	if (count > 2)
		add = count - 2;
	
	if (length - delete + add > length)
		objectResize(context, this, length - delete + add);
	
	result = io_libecc_Array.createSized(delete);
	
	for (from = start, to = 0; to < delete; ++from, ++to)
		io_libecc_Object.putElement(context, result, to, io_libecc_Object.getElement(context, this, from));
	
	if (delete > add)
	{
		for (from = start + delete, to = start + add; from < length; ++from, ++to)
			io_libecc_Object.putElement(context, this, to, io_libecc_Object.getElement(context, this, from));
		
		for (; to < length; ++to)
			io_libecc_Object.putElement(context, this, to, io_libecc_value_none);
	}
	else if (delete < add)
		for (from = length, to = length + add - delete; from > start;)
			io_libecc_Object.putElement(context, this, --to, io_libecc_Object.getElement(context, this, --from));
	
	for (from = 2, to = start; from < count; ++from, ++to)
		io_libecc_Object.putElement(context, this, to, io_libecc_Context.argument(context, from));
	
	if (length - delete + add <= length)
		objectResize(context, this, length - delete + add);
	
	return io_libecc_Value.object(result);
}

static
struct eccvalue_t indexOf (struct eccstate_t * const context)
{
	struct eccobject_t *this;
	struct eccvalue_t search, start;
	uint32_t length = 0;
	int32_t index;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	length = objectLength(context, this);
	
	search = io_libecc_Context.argument(context, 0);
	start = io_libecc_Value.toInteger(context, io_libecc_Context.argument(context, 1));
	index = start.data.integer < 0? length + start.data.integer: start.data.integer;
	
	if (index < 0)
		index = 0;
	
	for (; index < length; ++index)
		if (io_libecc_Value.isTrue(io_libecc_Value.same(context, search, io_libecc_Object.getElement(context, this, index))))
			return io_libecc_Value.binary(index);
	
	return io_libecc_Value.binary(-1);
}

static
struct eccvalue_t lastIndexOf (struct eccstate_t * const context)
{
	struct eccobject_t *this;
	struct eccvalue_t search, start;
	uint32_t length = 0;
	int32_t index;
	
	this = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	length = objectLength(context, this);
	
	search = io_libecc_Context.argument(context, 0);
	start = io_libecc_Value.toInteger(context, io_libecc_Context.argument(context, 1));
	index = start.data.integer <= 0? length + start.data.integer: start.data.integer + 1;
	
	if (index < 0)
		index = 0;
	else if (index > length)
		index = length;
	
	for (; index--;)
		if (io_libecc_Value.isTrue(io_libecc_Value.same(context, search, io_libecc_Object.getElement(context, this, index))))
			return io_libecc_Value.binary(index);
	
	return io_libecc_Value.binary(-1);
}

static
struct eccvalue_t getLength (struct eccstate_t * const context)
{
	return io_libecc_Value.binary(context->this.data.object->elementCount);
}

static
struct eccvalue_t setLength (struct eccstate_t * const context)
{
	double length;
	
	length = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 0)).data.binary;
	if (!isfinite(length) || length < 0 || length > UINT32_MAX || length != (uint32_t)length)
		io_libecc_Context.rangeError(context, io_libecc_Chars.create("invalid array length"));
	
	if (io_libecc_Object.resizeElement(context->this.data.object, length) && context->parent->strictMode)
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", context->this.data.object->elementCount));
	
	return io_libecc_value_undefined;
}

static
struct eccvalue_t constructor (struct eccstate_t * const context)
{
	struct eccvalue_t value;
	uint32_t index, count, length;
	struct eccobject_t *array;
	
	length = count = io_libecc_Context.argumentCount(context);
	value = io_libecc_Context.argument(context, 0);
	if (count == 1 && io_libecc_Value.isNumber(value) && io_libecc_Value.isPrimitive(value))
	{
		double binary = io_libecc_Value.toBinary(context, value).data.binary;
		if (isfinite(binary) && binary >= 0 && binary <= UINT32_MAX && binary == (uint32_t)binary)
		{
			length = binary;
			count = 0;
		}
		else
			io_libecc_Context.rangeError(context, io_libecc_Chars.create("invalid array length"));
	}
	
	array = io_libecc_Array.createSized(length);
	
	for (index = 0; index < count; ++index)
	{
		array->element[index].value = io_libecc_Context.argument(context, index);
		array->element[index].value.flags &= ~(io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	}
	
	return io_libecc_Value.object(array);
}

// MARK: - Methods

void setup (void)
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	const enum io_libecc_value_Flags s = io_libecc_value_sealed;
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_array_constructor, constructor, -1,
		&io_libecc_array_prototype, io_libecc_Value.object(createSized(0)),
		&io_libecc_array_type);
	
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
	
	io_libecc_Object.addMember(io_libecc_array_prototype, io_libecc_key_length, io_libecc_Function.accessor(getLength, setLength), h|s | io_libecc_value_asOwn | io_libecc_value_asData);
}

void teardown (void)
{
	io_libecc_array_prototype = NULL;
	io_libecc_array_constructor = NULL;
}

struct eccobject_t *create (void)
{
	return createSized(0);
}

struct eccobject_t *createSized (uint32_t size)
{
	struct eccobject_t *self = io_libecc_Object.create(io_libecc_array_prototype);
	
	io_libecc_Object.resizeElement(self, size);
	
	return self;
}
