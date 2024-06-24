//
//  boolean.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"


// MARK: - Private

struct eccobject_t * io_libecc_boolean_prototype = NULL;
struct io_libecc_Function * io_libecc_boolean_constructor = NULL;

const struct io_libecc_object_Type io_libecc_boolean_type = {
	.text = &io_libecc_text_booleanType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static struct io_libecc_Boolean* create(int);
const struct type_io_libecc_Boolean io_libecc_Boolean = {
    setup,
    teardown,
    create,
};

static
struct eccvalue_t toString (struct eccstate_t * const context)
{
	int truth;
	
	io_libecc_Context.assertThisMask(context, io_libecc_value_booleanMask);
	
	truth = io_libecc_Value.isObject(context->this)? context->this.data.boolean->truth: io_libecc_Value.isTrue(context->this);
	
	return io_libecc_Value.text(truth? &io_libecc_text_true: &io_libecc_text_false);
}

static
struct eccvalue_t valueOf (struct eccstate_t * const context)
{
	int truth;
	
	io_libecc_Context.assertThisMask(context, io_libecc_value_booleanMask);
	
	truth = io_libecc_Value.isObject(context->this)? context->this.data.boolean->truth: io_libecc_Value.isTrue(context->this);
	
	return io_libecc_Value.truth(truth);
}

static
struct eccvalue_t constructor (struct eccstate_t * const context)
{
	char truth;
	
	truth = io_libecc_Value.isTrue(io_libecc_Context.argument(context, 0));
	if (context->construct)
		return io_libecc_Value.boolean(io_libecc_Boolean.create(truth));
	else
		return io_libecc_Value.truth(truth);
}

// MARK: - Methods

void setup ()
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_boolean_constructor, constructor, 1,
		&io_libecc_boolean_prototype, io_libecc_Value.boolean(create(0)),
		&io_libecc_boolean_type);
	
	io_libecc_Function.addToObject(io_libecc_boolean_prototype, "toString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_boolean_prototype, "valueOf", valueOf, 0, h);
}

void teardown (void)
{
	io_libecc_boolean_prototype = NULL;
	io_libecc_boolean_constructor = NULL;
}

struct io_libecc_Boolean * create (int truth)
{
	struct io_libecc_Boolean *self = malloc(sizeof(*self));
	*self = io_libecc_Boolean.identity;
	io_libecc_Pool.addObject(&self->object);
	io_libecc_Object.initialize(&self->object, io_libecc_boolean_prototype);
	
	self->truth = truth;
	
	return self;
}
