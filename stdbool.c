//
//  boolean.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"


// MARK: - Private

eccobject_t * io_libecc_boolean_prototype = NULL;
struct io_libecc_Function * io_libecc_boolean_constructor = NULL;

const struct io_libecc_object_Type io_libecc_boolean_type = {
	.text = &ECC_ConstString_BooleanType,
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
eccvalue_t toString (eccstate_t * const context)
{
	int truth;
	
	io_libecc_Context.assertThisMask(context, ECC_VALMASK_BOOLEAN);
	
	truth = ECCNSValue.isObject(context->this)? context->this.data.boolean->truth: ECCNSValue.isTrue(context->this);
	
	return ECCNSValue.text(truth? &ECC_ConstString_True: &ECC_ConstString_False);
}

static
eccvalue_t valueOf (eccstate_t * const context)
{
	int truth;
	
	io_libecc_Context.assertThisMask(context, ECC_VALMASK_BOOLEAN);
	
	truth = ECCNSValue.isObject(context->this)? context->this.data.boolean->truth: ECCNSValue.isTrue(context->this);
	
	return ECCNSValue.truth(truth);
}

static
eccvalue_t constructor (eccstate_t * const context)
{
	char truth;
	
	truth = ECCNSValue.isTrue(io_libecc_Context.argument(context, 0));
	if (context->construct)
		return ECCNSValue.boolean(io_libecc_Boolean.create(truth));
	else
		return ECCNSValue.truth(truth);
}

// MARK: - Methods

void setup ()
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_boolean_constructor, constructor, 1,
		&io_libecc_boolean_prototype, ECCNSValue.boolean(create(0)),
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
