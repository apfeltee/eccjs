//
//  arguments.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

struct eccobject_t * io_libecc_arguments_prototype;

const struct io_libecc_object_Type io_libecc_arguments_type = {
	.text = &io_libecc_text_argumentsType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static struct eccobject_t* createSized(uint32_t size);
static struct eccobject_t* createWithCList(int count, const char* list[]);
const struct type_io_libecc_Arguments io_libecc_Arguments = {
    setup,
    teardown,
    createSized,
    createWithCList,
};

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
	
	if (io_libecc_Object.resizeElement(context->this.data.object, length) && context->strictMode)
	{
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", context->this.data.object->elementCount));
	}
	
	return io_libecc_value_undefined;
}

static
struct eccvalue_t getCallee (struct eccstate_t * const context)
{
	io_libecc_Context.rewindStatement(context->parent);
	io_libecc_Context.typeError(context, io_libecc_Chars.create("'callee' cannot be accessed in this context"));
	
	return io_libecc_value_undefined;
}

static
struct eccvalue_t setCallee (struct eccstate_t * const context)
{
	io_libecc_Context.rewindStatement(context->parent);
	io_libecc_Context.typeError(context, io_libecc_Chars.create("'callee' cannot be accessed in this context"));
	
	return io_libecc_value_undefined;
}

// MARK: - Methods

void setup (void)
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	const enum io_libecc_value_Flags s = io_libecc_value_sealed;
	
	io_libecc_arguments_prototype = io_libecc_Object.createTyped(&io_libecc_arguments_type);
	
	io_libecc_Object.addMember(io_libecc_arguments_prototype, io_libecc_key_length, io_libecc_Function.accessor(getLength, setLength), h|s | io_libecc_value_asOwn | io_libecc_value_asData);
	io_libecc_Object.addMember(io_libecc_arguments_prototype, io_libecc_key_callee, io_libecc_Function.accessor(getCallee, setCallee), h|s | io_libecc_value_asOwn);
}

void teardown (void)
{
	io_libecc_arguments_prototype = NULL;
}

struct eccobject_t *createSized (uint32_t size)
{
	struct eccobject_t *self = io_libecc_Object.create(io_libecc_arguments_prototype);
	
	io_libecc_Object.resizeElement(self, size);
	
	return self;
}

struct eccobject_t *createWithCList (int count, const char * list[])
{
	struct eccobject_t *self = createSized(count);
	
	io_libecc_Object.populateElementWithCList(self, count, list);
	
	return self;
}
