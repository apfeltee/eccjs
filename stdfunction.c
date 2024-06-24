//
//  function.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "builtins.h"

#include "ecc.h"
#include "oplist.h"
#include "pool.h"

static void setup(void);
static void teardown(void);
static struct io_libecc_Function* create(struct io_libecc_Object* environment);
static struct io_libecc_Function* createSized(struct io_libecc_Object* environment, uint32_t size);
static struct io_libecc_Function* createWithNative(const io_libecc_native_io_libecc_Function native, int parameterCount);
static struct io_libecc_Function* copy(struct io_libecc_Function* original);
static void destroy(struct io_libecc_Function*);
static void addMember(struct io_libecc_Function*, const char* name, struct io_libecc_Value value, enum io_libecc_value_Flags);
static void addValue(struct io_libecc_Function*, const char* name, struct io_libecc_Value value, enum io_libecc_value_Flags);
static struct io_libecc_Function*
addMethod(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
static struct io_libecc_Function*
addFunction(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
static struct io_libecc_Function*
addToObject(struct io_libecc_Object* object, const char* name, const io_libecc_native_io_libecc_Function native, int parameterCount, enum io_libecc_value_Flags);
static void linkPrototype(struct io_libecc_Function*, struct io_libecc_Value prototype, enum io_libecc_value_Flags);
static void setupBuiltinObject(struct io_libecc_Function**,
                               const io_libecc_native_io_libecc_Function,
                               int parameterCount,
                               struct io_libecc_Object**,
                               struct io_libecc_Value prototype,
                               const struct io_libecc_object_Type* type);
static struct io_libecc_Value accessor(const io_libecc_native_io_libecc_Function getter, const io_libecc_native_io_libecc_Function setter);
const struct type_io_libecc_Function io_libecc_Function = {
    setup,     teardown,    create,      createSized,   createWithNative,   copy,     destroy, addMember, addValue,
    addMethod, addFunction, addToObject, linkPrototype, setupBuiltinObject, accessor,
};


// MARK: - Private

static va_list empty_ap;

static void mark (struct io_libecc_Object *object);
static void capture (struct io_libecc_Object *object);

struct io_libecc_Object * io_libecc_function_prototype = NULL;
struct io_libecc_Function * io_libecc_function_constructor = NULL;

const struct io_libecc_object_Type io_libecc_function_type = {
	.text = &io_libecc_text_functionType,
	.mark = mark,
	.capture = capture,
	/* XXX: don't finalize */
};

static
void capture (struct io_libecc_Object *object)
{
	struct io_libecc_Function *self = (struct io_libecc_Function *)object;
	
	if (self->refObject)
		++self->refObject->referenceCount;
	
	if (self->pair)
		++self->pair->object.referenceCount;
}

static
void mark (struct io_libecc_Object *object)
{
	struct io_libecc_Function *self = (struct io_libecc_Function *)object;
	
	io_libecc_Pool.markObject(&self->environment);
	
	if (self->refObject)
		io_libecc_Pool.markObject(self->refObject);
	
	if (self->pair)
		io_libecc_Pool.markObject(&self->pair->object);
}

// MARK: - Static Members

static
struct io_libecc_Value toChars (struct io_libecc_Context * const context, struct io_libecc_Value value)
{
	struct io_libecc_Function *self;
	struct io_libecc_chars_Append chars;
	
	assert(value.type == io_libecc_value_functionType);
	assert(value.data.function);
	
	self = value.data.function;
	io_libecc_Chars.beginAppend(&chars);
	
	io_libecc_Chars.append(&chars, "function %s", self->name? self->name: "anonymous");
	
	if (self->text.bytes == io_libecc_text_nativeCode.bytes)
		io_libecc_Chars.append(&chars, "() [native code]");
	else
		io_libecc_Chars.append(&chars, "%.*s", self->text.length, self->text.bytes);
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct io_libecc_Value toString (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_functionType);
	
	return toChars(context, context->this);
}

static
struct io_libecc_Value apply (struct io_libecc_Context * const context)
{
	struct io_libecc_Value this, arguments;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_functionType);
	
	context->strictMode = context->parent->strictMode;
	
	this = io_libecc_Context.argument(context, 0);
	if (this.type != io_libecc_value_undefinedType && this.type != io_libecc_value_nullType)
		this = io_libecc_Value.toObject(context, this);
	
	arguments = io_libecc_Context.argument(context, 1);
	
	if (arguments.type == io_libecc_value_undefinedType || arguments.type == io_libecc_value_nullType)
		return io_libecc_Op.callFunctionVA(context, io_libecc_context_applyOffset, context->this.data.function, this, 0, empty_ap);
	else
	{
		if (!io_libecc_Value.isObject(arguments))
			io_libecc_Context.typeError(context, io_libecc_Chars.create("arguments is not an object"));
		
		return io_libecc_Op.callFunctionArguments(context, io_libecc_context_applyOffset, context->this.data.function, this, arguments.data.object);
	}
}

static
struct io_libecc_Value call (struct io_libecc_Context * const context)
{
	struct io_libecc_Object arguments;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_functionType);
	
	context->strictMode = context->parent->strictMode;
	
	arguments = *context->environment->hashmap[2].value.data.object;
	
	if (arguments.elementCount)
	{
		struct io_libecc_Value this = io_libecc_Context.argument(context, 0);
		if (this.type != io_libecc_value_undefinedType && this.type != io_libecc_value_nullType)
			this = io_libecc_Value.toObject(context, this);
		
		--arguments.elementCapacity;
		--arguments.elementCount;
		++arguments.element;
		if (!arguments.elementCount)
		{
			arguments.element = NULL;
			arguments.elementCapacity = 0;
		}
		
		return io_libecc_Op.callFunctionArguments(context, io_libecc_context_callOffset, context->this.data.function, this, &arguments);
	}
	else
		return io_libecc_Op.callFunctionVA(context, io_libecc_context_callOffset, context->this.data.function, io_libecc_value_undefined, 0, empty_ap);
}

static
struct io_libecc_Value bindCall (struct io_libecc_Context * const context)
{
	struct io_libecc_Function *function;
	struct io_libecc_Object *arguments;
	uint16_t count, length;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_functionType);
	
	context->strictMode = context->parent->strictMode;
	
	function = context->this.data.function;
	
	count = io_libecc_Context.argumentCount(context);
	length = (function->environment.elementCount - 1) + count;
	arguments = io_libecc_Array.createSized(length);
	
	memcpy(arguments->element, function->environment.element + 1, sizeof(*arguments->element) * (function->environment.elementCount - 1));
	memcpy(arguments->element + (function->environment.elementCount - 1), context->environment->hashmap[2].value.data.object->element, sizeof(*arguments->element) * (context->environment->hashmap[2].value.data.object->elementCount));
	
	return io_libecc_Op.callFunctionArguments(context, 0, context->this.data.function->pair, function->environment.element[0].value, arguments);
}

static
struct io_libecc_Value bind (struct io_libecc_Context * const context)
{
	struct io_libecc_Function *function;
	uint16_t index, count;
	int parameterCount = 0;
	
	io_libecc_Context.assertThisType(context, io_libecc_value_functionType);
	
	count = io_libecc_Context.argumentCount(context);
	parameterCount = context->this.data.function->parameterCount - (count > 1? count - 1: 0);
	function = createWithNative(bindCall, parameterCount > 0? parameterCount: 0);
	
	io_libecc_Object.resizeElement(&function->environment, count? count: 1);
	if (count)
		for (index = 0; index < count; ++index)
			function->environment.element[index].value = io_libecc_Context.argument(context, index);
	else
		function->environment.element[0].value = io_libecc_value_undefined;
	
	function->pair = context->this.data.function;
	function->boundThis = io_libecc_Value.function(function);
	function->flags |= io_libecc_function_needArguments | io_libecc_function_useBoundThis;
	
	return io_libecc_Value.function(function);
}

static
struct io_libecc_Value prototypeConstructor (struct io_libecc_Context * const context)
{
	return io_libecc_value_undefined;
}

static
struct io_libecc_Value constructor (struct io_libecc_Context * const context)
{
	int argumentCount;
	
	argumentCount = io_libecc_Context.argumentCount(context);
	
	{
		int_fast32_t index;
		struct io_libecc_Value value;
		struct io_libecc_chars_Append chars;
		struct io_libecc_Input *input;
		struct io_libecc_Context subContext = {
			.parent = context,
			.this = io_libecc_Value.object(&context->ecc->global->environment),
			.ecc = context->ecc,
			.depth = context->depth + 1,
			.environment = io_libecc_Context.environmentRoot(context->parent),
		};
		
		io_libecc_Chars.beginAppend(&chars);
		io_libecc_Chars.append(&chars, "(function (");
		if (argumentCount)
			for (index = 0; index < argumentCount; ++index)
			{
				if (index == argumentCount - 1)
					io_libecc_Chars.append(&chars, "){");
				
				value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, index));
				io_libecc_Chars.append(&chars, "%.*s", io_libecc_Value.stringLength(&value), io_libecc_Value.stringBytes(&value));
				
				if (index < argumentCount - 2)
					io_libecc_Chars.append(&chars, ",");
			}
		else
			io_libecc_Chars.append(&chars, "){");
		
		io_libecc_Chars.append(&chars, "})");
		
		value = io_libecc_Chars.endAppend(&chars);
		input = io_libecc_Input.createFromBytes(io_libecc_Value.stringBytes(&value), io_libecc_Value.stringLength(&value), "(io_libecc_Function)");
		io_libecc_Context.setTextIndex(context, io_libecc_context_noIndex);
		io_libecc_Ecc.evalInputWithContext(context->ecc, input, &subContext);
	}
	
	return context->ecc->result;
}

// MARK: - Methods

void setup ()
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_function_constructor, constructor, -1,
		&io_libecc_function_prototype, io_libecc_Value.function(createWithNative(prototypeConstructor, 0)),
		&io_libecc_function_type);
	
	io_libecc_function_constructor->object.prototype = io_libecc_function_prototype;
	
	io_libecc_Function.addToObject(io_libecc_function_prototype, "toString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_function_prototype, "apply", apply, 2, h);
	io_libecc_Function.addToObject(io_libecc_function_prototype, "call", call, -1, h);
	io_libecc_Function.addToObject(io_libecc_function_prototype, "bind", bind, -1, h);
}

void teardown (void)
{
	io_libecc_function_prototype = NULL;
	io_libecc_function_constructor = NULL;
}

struct io_libecc_Function * create (struct io_libecc_Object *environment)
{
	return createSized(environment, 8);
}

struct io_libecc_Function * createSized (struct io_libecc_Object *environment, uint32_t size)
{
	struct io_libecc_Function *self = malloc(sizeof(*self));
	io_libecc_Pool.addFunction(self);
	
	*self = io_libecc_Function.identity;
	
	io_libecc_Object.initialize(&self->object, io_libecc_function_prototype);
	io_libecc_Object.initializeSized(&self->environment, environment, size);
	
	return self;
}

struct io_libecc_Function * createWithNative (const io_libecc_native_io_libecc_Function native, int parameterCount)
{
	struct io_libecc_Function *self = NULL;
	
	if (parameterCount < 0)
	{
		self = createSized(NULL, 3);
		self->flags |= io_libecc_function_needArguments;
	}
	else
	{
		self = createSized(NULL, 3 + parameterCount);
		self->parameterCount = parameterCount;
	}
	
	self->environment.hashmapCount = self->environment.hashmapCapacity;
	self->oplist = io_libecc_OpList.create(native, io_libecc_value_undefined, io_libecc_text_nativeCode);
	self->text = io_libecc_text_nativeCode;
	
	io_libecc_Object.addMember(&self->object, io_libecc_key_length, io_libecc_Value.integer(abs(parameterCount)), io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	
	return self;
}

struct io_libecc_Function * copy (struct io_libecc_Function *original)
{
	struct io_libecc_Function *self = malloc(sizeof(*self));
	size_t byteSize;
	
	assert(original);
	io_libecc_Pool.addObject(&self->object);
	
	*self = *original;
	
	byteSize = sizeof(*self->object.hashmap) * self->object.hashmapCapacity;
	self->object.hashmap = malloc(byteSize);
	memcpy(self->object.hashmap, original->object.hashmap, byteSize);
	
	return self;
}

void destroy (struct io_libecc_Function *self)
{
	assert(self);
	
	io_libecc_Object.finalize(&self->object);
	io_libecc_Object.finalize(&self->environment);
	
	if (self->oplist)
		io_libecc_OpList.destroy(self->oplist), self->oplist = NULL;
	
	free(self), self = NULL;
}

void addMember(struct io_libecc_Function *self, const char *name, struct io_libecc_Value value, enum io_libecc_value_Flags flags)
{
	assert(self);
	
	if (value.type == io_libecc_value_functionType)
		value.data.function->name = name;
	
	io_libecc_Object.addMember(&self->object, io_libecc_Key.makeWithCString(name), value, flags);
}

struct io_libecc_Function * addMethod(struct io_libecc_Function *self, const char *name, const io_libecc_native_io_libecc_Function native, int parameterCount, enum io_libecc_value_Flags flags)
{
	assert(self);
	
	return addToObject(&self->object, name, native, parameterCount, flags);
}

void addValue(struct io_libecc_Function *self, const char *name, struct io_libecc_Value value, enum io_libecc_value_Flags flags)
{
	assert(self);
	
	if (value.type == io_libecc_value_functionType)
		value.data.function->name = name;
	
	io_libecc_Object.addMember(&self->environment, io_libecc_Key.makeWithCString(name), value, flags);
}

struct io_libecc_Function * addFunction(struct io_libecc_Function *self, const char *name, const io_libecc_native_io_libecc_Function native, int parameterCount, enum io_libecc_value_Flags flags)
{
	assert(self);
	
	return addToObject(&self->environment, name, native, parameterCount, flags);
}

struct io_libecc_Function * addToObject(struct io_libecc_Object *object, const char *name, const io_libecc_native_io_libecc_Function native, int parameterCount, enum io_libecc_value_Flags flags)
{
	struct io_libecc_Function *function;
	
	assert(object);
	
	function = createWithNative(native, parameterCount);
	function->name = name;
	
	io_libecc_Object.addMember(object, io_libecc_Key.makeWithCString(name), io_libecc_Value.function(function), flags);
	
	return function;
}

void linkPrototype (struct io_libecc_Function *self, struct io_libecc_Value prototype, enum io_libecc_value_Flags flags)
{
	assert(self);
	
	io_libecc_Object.addMember(prototype.data.object, io_libecc_key_constructor, io_libecc_Value.function(self), io_libecc_value_hidden);
	io_libecc_Object.addMember(&self->object, io_libecc_key_prototype, prototype, flags);
}

void setupBuiltinObject (struct io_libecc_Function **constructor, const io_libecc_native_io_libecc_Function native, int parameterCount, struct io_libecc_Object **prototype, struct io_libecc_Value prototypeValue, const struct io_libecc_object_Type *type)
{
	struct io_libecc_Function *function = createWithNative(native, parameterCount);
	
	if (prototype)
	{
		struct io_libecc_Object *object = prototypeValue.data.object;
		object->type = type;
		
		if (!object->prototype)
			object->prototype = io_libecc_object_prototype;
		
		*prototype = object;
	}
	
	linkPrototype(function, prototypeValue, io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	*constructor = function;
}

struct io_libecc_Value accessor (const io_libecc_native_io_libecc_Function getter, const io_libecc_native_io_libecc_Function setter)
{
	struct io_libecc_Value value;
	struct io_libecc_Function *getterFunction = NULL, *setterFunction = NULL;
	if (setter)
		setterFunction = io_libecc_Function.createWithNative(setter, 1);
	
	if (getter)
	{
		getterFunction = io_libecc_Function.createWithNative(getter, 0);
		getterFunction->pair = setterFunction;
		value = io_libecc_Value.function(getterFunction);
		value.flags |= io_libecc_value_getter;
	}
	else if (setter)
	{
		value = io_libecc_Value.function(setterFunction);
		value.flags |= io_libecc_value_setter;
	}
	else
		value = io_libecc_value_undefined;
	
	return value;
}
