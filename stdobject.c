//
//  object.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"


// MARK: - Private

static void setup(void);
static void teardown(void);
static struct eccobject_t* create(struct eccobject_t* prototype);
static struct eccobject_t* createSized(struct eccobject_t* prototype, uint16_t size);
static struct eccobject_t* createTyped(const struct io_libecc_object_Type* type);
static struct eccobject_t* initialize(struct eccobject_t* restrict, struct eccobject_t* restrict prototype);
static struct eccobject_t* initializeSized(struct eccobject_t* restrict, struct eccobject_t* restrict prototype, uint16_t size);
static struct eccobject_t* finalize(struct eccobject_t*);
static struct eccobject_t* copy(const struct eccobject_t* original);
static void destroy(struct eccobject_t*);
static struct eccvalue_t getMember(struct eccstate_t* const, struct eccobject_t*, struct io_libecc_Key key);
static struct eccvalue_t putMember(struct eccstate_t* const, struct eccobject_t*, struct io_libecc_Key key, struct eccvalue_t);
static struct eccvalue_t* member(struct eccobject_t*, struct io_libecc_Key key, enum io_libecc_value_Flags);
static struct eccvalue_t* addMember(struct eccobject_t*, struct io_libecc_Key key, struct eccvalue_t, enum io_libecc_value_Flags);
static int deleteMember(struct eccobject_t*, struct io_libecc_Key key);
static struct eccvalue_t getElement(struct eccstate_t* const, struct eccobject_t*, uint32_t index);
static struct eccvalue_t putElement(struct eccstate_t* const, struct eccobject_t*, uint32_t index, struct eccvalue_t);
static struct eccvalue_t* element(struct eccobject_t*, uint32_t index, enum io_libecc_value_Flags);
static struct eccvalue_t* addElement(struct eccobject_t*, uint32_t index, struct eccvalue_t, enum io_libecc_value_Flags);
static int deleteElement(struct eccobject_t*, uint32_t index);
static struct eccvalue_t getProperty(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t primitive);
static struct eccvalue_t putProperty(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t primitive, struct eccvalue_t);
static struct eccvalue_t* property(struct eccobject_t*, struct eccvalue_t primitive, enum io_libecc_value_Flags);
static struct eccvalue_t* addProperty(struct eccobject_t*, struct eccvalue_t primitive, struct eccvalue_t, enum io_libecc_value_Flags);
static int deleteProperty(struct eccobject_t*, struct eccvalue_t primitive);
static struct eccvalue_t putValue(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t*, struct eccvalue_t);
static struct eccvalue_t getValue(struct eccstate_t* const, struct eccobject_t*, struct eccvalue_t*);
static void packValue(struct eccobject_t*);
static void stripMap(struct eccobject_t*);
static void reserveSlots(struct eccobject_t*, uint16_t slots);
static int resizeElement(struct eccobject_t*, uint32_t size);
static void populateElementWithCList(struct eccobject_t*, uint32_t count, const char* list[]);
static struct eccvalue_t toString(struct eccstate_t* const);
static void dumpTo(struct eccobject_t*, FILE* file);
const struct type_io_libecc_Object io_libecc_Object = {
    setup,          teardown, create,     createSized,   createTyped, initialize,   initializeSized, finalize,
    copy,           destroy,  getMember,  putMember,     member,      addMember,    deleteMember,    getElement,
    putElement,     element,  addElement, deleteElement, getProperty, putProperty,  property,        addProperty,
    deleteProperty, putValue, getValue,   packValue,     stripMap,    reserveSlots, resizeElement,   populateElementWithCList,
    toString,       dumpTo,
};


const uint32_t io_libecc_object_ElementMax = 0xffffff;

static const int defaultSize = 8;

struct eccobject_t * io_libecc_object_prototype = NULL;
struct io_libecc_Function * io_libecc_object_constructor = NULL;

const struct io_libecc_object_Type io_libecc_object_type = {
	.text = &io_libecc_text_objectType,
};

// MARK: - Static Members

static inline
uint16_t getSlot (const struct eccobject_t * const self, const struct io_libecc_Key key)
{
	return
		self->hashmap[
		self->hashmap[
		self->hashmap[
		self->hashmap[1]
		.slot[key.data.depth[0]]]
		.slot[key.data.depth[1]]]
		.slot[key.data.depth[2]]]
		.slot[key.data.depth[3]];
}

static inline
uint32_t getIndexOrKey (struct eccvalue_t property, struct io_libecc_Key *key)
{
	uint32_t index = UINT32_MAX;
	
	assert(io_libecc_Value.isPrimitive(property));
	
	if (property.type == io_libecc_value_keyType)
		*key = property.data.key;
	else
	{
		if (property.type == io_libecc_value_integerType && property.data.integer >= 0)
			index = property.data.integer;
		else if (property.type == io_libecc_value_binaryType && property.data.binary >= 0 && property.data.binary < UINT32_MAX && property.data.binary == (uint32_t)property.data.binary)
			index = property.data.binary;
		else if (io_libecc_Value.isString(property))
		{
			struct io_libecc_Text text = io_libecc_Value.textOf(&property);
			if ((index = io_libecc_Lexer.scanElement(text)) == UINT32_MAX)
				*key = io_libecc_Key.makeWithText(text, io_libecc_key_copyOnCreate);
		}
		else
			return getIndexOrKey(io_libecc_Value.toString(NULL, property), key);
	}
	
	return index;
}

static inline
struct io_libecc_Key keyOfIndex (uint32_t index, int create)
{
	char buffer[10 + 1];
	uint16_t length;
	
	length = snprintf(buffer, sizeof(buffer), "%u", (unsigned)index);
	if (create)
		return io_libecc_Key.makeWithText(io_libecc_Text.make(buffer, length), io_libecc_key_copyOnCreate);
	else
		return io_libecc_Key.search(io_libecc_Text.make(buffer, length));
}

static inline
uint32_t nextPowerOfTwo(uint32_t v)
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

static inline
uint32_t elementCount (struct eccobject_t *self)
{
	if (self->elementCount < io_libecc_object_ElementMax)
		return self->elementCount;
	else
		return io_libecc_object_ElementMax;
}

static
void readonlyError(struct eccstate_t * const context, struct eccvalue_t *ref, struct eccobject_t *this)
{
	struct io_libecc_Text text;
	
	do
	{
		union io_libecc_object_Hashmap *hashmap = (union io_libecc_object_Hashmap *)ref;
		union io_libecc_object_Element *element = (union io_libecc_object_Element *)ref;
		
		if (hashmap >= this->hashmap && hashmap < this->hashmap + this->hashmapCount)
		{
			const struct io_libecc_Text *keyText = io_libecc_Key.textOf(hashmap->value.key);
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is read-only", keyText->length, keyText->bytes));
		}
		else if (element >= this->element && element < this->element + this->elementCount)
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is read-only", element - this->element));
		
	} while (( this = this->prototype ));
	
	text = io_libecc_Context.textSeek(context);
	io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is read-only", text.length, text.bytes));
}

//

static
struct eccobject_t *checkObject (struct eccstate_t * const context, int argument)
{
	struct eccvalue_t value = io_libecc_Context.argument(context, argument);
	if (!io_libecc_Value.isObject(value))
		io_libecc_Context.typeError(context, io_libecc_Chars.create("not an object"));
	
	return value.data.object;
}

static
struct eccvalue_t valueOf (struct eccstate_t * const context)
{
	return io_libecc_Value.toObject(context, io_libecc_Context.this(context));
}

static
struct eccvalue_t hasOwnProperty (struct eccstate_t * const context)
{
	struct eccobject_t *self;
	struct eccvalue_t value;
	struct io_libecc_Key key;
	uint32_t index;
	
	self = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	value = io_libecc_Value.toPrimitive(context, io_libecc_Context.argument(context, 0), io_libecc_value_hintString);
	index = getIndexOrKey(value, &key);
	
	if (index < UINT32_MAX)
		return io_libecc_Value.truth(element(self, index, io_libecc_value_asOwn) != NULL);
	else
		return io_libecc_Value.truth(member(self, key, io_libecc_value_asOwn) != NULL);
}

static
struct eccvalue_t isPrototypeOf (struct eccstate_t * const context)
{
	struct eccvalue_t arg0;
	
	arg0 = io_libecc_Context.argument(context, 0);
	
	if (io_libecc_Value.isObject(arg0))
	{
		struct eccobject_t *v = arg0.data.object;
		struct eccobject_t *o = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
		
		do
			if (v == o)
				return io_libecc_value_true;
		while (( v = v->prototype ));
	}
	
	return io_libecc_value_false;
}

static
struct eccvalue_t propertyIsEnumerable (struct eccstate_t * const context)
{
	struct eccvalue_t value;
	struct eccobject_t *object;
	struct eccvalue_t *ref;
	
	value = io_libecc_Value.toPrimitive(context, io_libecc_Context.argument(context, 0), io_libecc_value_hintString);
	object = io_libecc_Value.toObject(context, io_libecc_Context.this(context)).data.object;
	ref = property(object, value, io_libecc_value_asOwn);
	
	if (ref)
		return io_libecc_Value.truth(!(ref->flags & io_libecc_value_hidden));
	else
		return io_libecc_value_false;
}

static
struct eccvalue_t constructor (struct eccstate_t * const context)
{
	struct eccvalue_t value;
	
	value = io_libecc_Context.argument(context, 0);
	
	if (value.type == io_libecc_value_nullType || value.type == io_libecc_value_undefinedType)
		return io_libecc_Value.object(create(io_libecc_object_prototype));
	else if (context->construct && io_libecc_Value.isObject(value))
		return value;
	else
		return io_libecc_Value.toObject(context, value);
}

static
struct eccvalue_t getPrototypeOf (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	
	object = io_libecc_Value.toObject(context, io_libecc_Context.argument(context, 0)).data.object;
	
	return object->prototype? io_libecc_Value.objectValue(object->prototype): io_libecc_value_undefined;
}

static
struct eccvalue_t getOwnPropertyDescriptor (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	struct eccvalue_t value;
	struct eccvalue_t *ref;
	
	object = io_libecc_Value.toObject(context, io_libecc_Context.argument(context, 0)).data.object;
	value = io_libecc_Value.toPrimitive(context, io_libecc_Context.argument(context, 1), io_libecc_value_hintString);
	ref = property(object, value, io_libecc_value_asOwn);
	
	if (ref)
	{
		struct eccobject_t *result = create(io_libecc_object_prototype);
		
		if (ref->flags & io_libecc_value_accessor)
		{
			if (ref->flags & io_libecc_value_asData)
			{
				addMember(result, io_libecc_key_value, io_libecc_Object.getValue(context, object, ref), 0);
				addMember(result, io_libecc_key_writable, io_libecc_Value.truth(!(ref->flags & io_libecc_value_readonly)), 0);
			}
			else
			{
				addMember(result, ref->flags & io_libecc_value_getter? io_libecc_key_get: io_libecc_key_set, io_libecc_Value.function(ref->data.function), 0);
				if (ref->data.function->pair)
					addMember(result, ref->flags & io_libecc_value_getter? io_libecc_key_set: io_libecc_key_get, io_libecc_Value.function(ref->data.function->pair), 0);
			}
		}
		else
		{
			addMember(result, io_libecc_key_value, *ref, 0);
			addMember(result, io_libecc_key_writable, io_libecc_Value.truth(!(ref->flags & io_libecc_value_readonly)), 0);
		}
		
		addMember(result, io_libecc_key_enumerable, io_libecc_Value.truth(!(ref->flags & io_libecc_value_hidden)), 0);
		addMember(result, io_libecc_key_configurable, io_libecc_Value.truth(!(ref->flags & io_libecc_value_sealed)), 0);
		
		return io_libecc_Value.object(result);
	}
	
	return io_libecc_value_undefined;
}

static
struct eccvalue_t getOwnPropertyNames (struct eccstate_t * const context)
{
	struct eccobject_t *object, *parent;
	struct eccobject_t *result;
	uint32_t index, count, length;
	
	object = checkObject(context, 0);
	result = io_libecc_Array.create();
	length = 0;
	
	for (index = 0, count = elementCount(object); index < count; ++index)
		if (object->element[index].value.check == 1)
			addElement(result, length++, io_libecc_Value.chars(io_libecc_Chars.create("%d", index)), 0);
	
	parent = object;
	while (( parent = parent->prototype ))
	{
		for (index = 2; index < parent->hashmapCount; ++index)
		{
			struct eccvalue_t value = parent->hashmap[index].value;
			if (value.check == 1 && value.flags & io_libecc_value_asOwn)
				addElement(result, length++, io_libecc_Value.text(io_libecc_Key.textOf(value.key)), 0);
		}
	}
	
	for (index = 2; index < object->hashmapCount; ++index)
		if (object->hashmap[index].value.check == 1)
			addElement(result, length++, io_libecc_Value.text(io_libecc_Key.textOf(object->hashmap[index].value.key)), 0);
	
	return io_libecc_Value.object(result);
}

static
struct eccvalue_t defineProperty (struct eccstate_t * const context)
{
	struct eccobject_t *object, *descriptor;
	struct eccvalue_t property, value, *getter, *setter, *current, *flag;
	struct io_libecc_Key key;
	uint32_t index;
	
	object = checkObject(context, 0);
	property = io_libecc_Value.toPrimitive(context, io_libecc_Context.argument(context, 1), io_libecc_value_hintString);
	descriptor = checkObject(context, 2);
	
	getter = member(descriptor, io_libecc_key_get, 0);
	setter = member(descriptor, io_libecc_key_set, 0);
	
	current = io_libecc_Object.property(object, property, io_libecc_value_asOwn);
	
	if (getter || setter)
	{
		if (getter && getter->type == io_libecc_value_undefinedType)
			getter = NULL;
		
		if (setter && setter->type == io_libecc_value_undefinedType)
			setter = NULL;
		
		if (getter && getter->type != io_libecc_value_functionType)
			io_libecc_Context.typeError(context, io_libecc_Chars.create("getter is not a function"));
		
		if (setter && setter->type != io_libecc_value_functionType)
			io_libecc_Context.typeError(context, io_libecc_Chars.create("setter is not a function"));
		
		if (member(descriptor, io_libecc_key_value, 0) || member(descriptor, io_libecc_key_writable, 0))
			io_libecc_Context.typeError(context, io_libecc_Chars.create("value & writable forbidden when a getter or setter are set"));
		
		if (getter)
		{
			value = *getter;
			if (setter)
				value.data.function->pair = setter->data.function;
			
			value.flags |= io_libecc_value_getter;
		}
		else if (setter)
		{
			value = *setter;
			value.flags |= io_libecc_value_setter;
		}
		else
		{
			value = io_libecc_Value.function(io_libecc_Function.createWithNative(io_libecc_Op.noop, 0));
			value.flags |= io_libecc_value_getter;
		}
	}
	else
	{
		value = getMember(context, descriptor, io_libecc_key_value);
		
		flag = member(descriptor, io_libecc_key_writable, 0);
		if ((flag && !io_libecc_Value.isTrue(getValue(context, descriptor, flag))) || (!flag && (!current || current->flags & io_libecc_value_readonly)))
			value.flags |= io_libecc_value_readonly;
	}
	
	flag = member(descriptor, io_libecc_key_enumerable, 0);
	if ((flag && !io_libecc_Value.isTrue(getValue(context, descriptor, flag))) || (!flag && (!current || current->flags & io_libecc_value_hidden)))
		value.flags |= io_libecc_value_hidden;
	
	flag = member(descriptor, io_libecc_key_configurable, 0);
	if ((flag && !io_libecc_Value.isTrue(getValue(context, descriptor, flag))) || (!flag && (!current || current->flags & io_libecc_value_sealed)))
		value.flags |= io_libecc_value_sealed;
	
	if (!current)
	{
		addProperty(object, property, value, 0);
		return io_libecc_value_true;
	}
	else
	{
		if (current->flags & io_libecc_value_sealed)
		{
			if (!(value.flags & io_libecc_value_sealed) || (value.flags & io_libecc_value_hidden) != (current->flags & io_libecc_value_hidden))
				goto sealedError;
			
			if (current->flags & io_libecc_value_accessor)
			{
				if (!(getter || setter))
				{
					if (current->flags & io_libecc_value_asData)
					{
						struct io_libecc_Function *currentSetter = current->flags & io_libecc_value_getter? current->data.function->pair: current->data.function;
						if (currentSetter)
						{
							io_libecc_Context.callFunction(context, currentSetter, io_libecc_Value.object(object), 1, value);
							return io_libecc_value_true;
						}
					}
					goto sealedError;
				}
				else
				{
					struct io_libecc_Function *currentGetter = current->flags & io_libecc_value_getter? current->data.function: current->data.function->pair;
					struct io_libecc_Function *currentSetter = current->flags & io_libecc_value_getter? current->data.function->pair: current->data.function;
					
					if (!getter != !currentGetter || !setter != !currentSetter)
						goto sealedError;
					else if (getter && getter->data.function->pair != currentGetter)
						goto sealedError;
					else if (setter && setter->data.function != currentSetter)
						goto sealedError;
				}
			}
			else
			{
				if (!io_libecc_Value.isTrue(io_libecc_Value.same(context, *current, value)))
					goto sealedError;
			}
		}
		
		addProperty(object, property, value, 0);
	}
	
	return io_libecc_value_true;
	
sealedError:
	io_libecc_Context.setTextIndexArgument(context, 1);
	index = getIndexOrKey(property, &key);
	if (index == UINT32_MAX)
	{
		const struct io_libecc_Text *text = io_libecc_Key.textOf(key);
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is non-configurable", text->length, text->bytes));
	}
	else
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", index));
}

static
struct eccvalue_t defineProperties (struct eccstate_t * const context)
{
	union io_libecc_object_Hashmap *originalHashmap = context->environment->hashmap;
	uint16_t originalHashmapCount = context->environment->hashmapCount;
	
	uint16_t index, count, hashmapCount = 6;
	struct eccobject_t *object, *properties;
	union io_libecc_object_Hashmap hashmap[hashmapCount];
	
	memset(hashmap, 0, hashmapCount * sizeof(*hashmap));
	
	object = checkObject(context, 0);
	properties = io_libecc_Value.toObject(context, io_libecc_Context.argument(context, 1)).data.object;
	
	context->environment->hashmap = hashmap;
	context->environment->hashmapCount = hashmapCount;
	
	io_libecc_Context.replaceArgument(context, 0, io_libecc_Value.object(object));
	
	for (index = 0, count = elementCount(properties); index < count; ++index)
	{
		if (!properties->element[index].value.check)
			continue;
		
		io_libecc_Context.replaceArgument(context, 1, io_libecc_Value.binary(index));
		io_libecc_Context.replaceArgument(context, 2, properties->element[index].value);
		defineProperty(context);
	}
	
	for (index = 2; index < properties->hashmapCount; ++index)
	{
		if (!properties->hashmap[index].value.check)
			continue;
		
		io_libecc_Context.replaceArgument(context, 1, io_libecc_Value.key(properties->hashmap[index].value.key));
		io_libecc_Context.replaceArgument(context, 2, properties->hashmap[index].value);
		defineProperty(context);
	}
	
	context->environment->hashmap = originalHashmap;
	context->environment->hashmapCount = originalHashmapCount;
	
	return io_libecc_value_undefined;
}

static
struct eccvalue_t objectCreate (struct eccstate_t * const context)
{
	struct eccobject_t *object, *result;
	struct eccvalue_t properties;
	
	object = checkObject(context, 0);
	properties = io_libecc_Context.argument(context, 1);
	
	result = create(object);
	if (properties.type != io_libecc_value_undefinedType)
	{
		io_libecc_Context.replaceArgument(context, 0, io_libecc_Value.object(result));
		defineProperties(context);
	}
	
	return io_libecc_Value.object(result);
}

static
struct eccvalue_t seal (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	uint32_t index, count;
	
	object = checkObject(context, 0);
	object->flags |= io_libecc_object_sealed;
	
	for (index = 0, count = elementCount(object); index < count; ++index)
		if (object->element[index].value.check == 1)
			object->element[index].value.flags |= io_libecc_value_sealed;
	
	for (index = 2; index < object->hashmapCount; ++index)
		if (object->hashmap[index].value.check == 1)
			object->hashmap[index].value.flags |= io_libecc_value_sealed;
	
	return io_libecc_Value.object(object);
}

static
struct eccvalue_t freeze (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	uint32_t index, count;
	
	object = checkObject(context, 0);
	object->flags |= io_libecc_object_sealed;
	
	for (index = 0, count = elementCount(object); index < count; ++index)
		if (object->element[index].value.check == 1)
			object->element[index].value.flags |= io_libecc_value_frozen;
	
	for (index = 2; index < object->hashmapCount; ++index)
		if (object->hashmap[index].value.check == 1)
			object->hashmap[index].value.flags |= io_libecc_value_frozen;
	
	return io_libecc_Value.object(object);
}

static
struct eccvalue_t preventExtensions (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	
	object = checkObject(context, 0);
	object->flags |= io_libecc_object_sealed;
	
	return io_libecc_Value.object(object);
}

static
struct eccvalue_t isSealed (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	uint32_t index, count;
	
	object = checkObject(context, 0);
	if (!(object->flags & io_libecc_object_sealed))
		return io_libecc_value_false;
	
	for (index = 0, count = elementCount(object); index < count; ++index)
		if (object->element[index].value.check == 1 && !(object->element[index].value.flags & io_libecc_value_sealed))
			return io_libecc_value_false;
	
	for (index = 2; index < object->hashmapCount; ++index)
		if (object->hashmap[index].value.check == 1 && !(object->hashmap[index].value.flags & io_libecc_value_sealed))
			return io_libecc_value_false;
	
	return io_libecc_value_true;
}

static
struct eccvalue_t isFrozen (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	uint32_t index, count;
	
	object = checkObject(context, 0);
	if (!(object->flags & io_libecc_object_sealed))
		return io_libecc_value_false;
	
	for (index = 0, count = elementCount(object); index < count; ++index)
		if (object->element[index].value.check == 1 && !(object->element[index].value.flags & io_libecc_value_frozen))
			return io_libecc_value_false;
	
	for (index = 2; index < object->hashmapCount; ++index)
		if (object->hashmap[index].value.check == 1 && !(object->hashmap[index].value.flags & io_libecc_value_frozen))
			return io_libecc_value_false;
	
	return io_libecc_value_true;
}

static
struct eccvalue_t isExtensible (struct eccstate_t * const context)
{
	struct eccobject_t *object;
	
	object = checkObject(context, 0);
	return io_libecc_Value.truth(!(object->flags & io_libecc_object_sealed));
}

static
struct eccvalue_t keys (struct eccstate_t * const context)
{
	struct eccobject_t *object, *parent;
	struct eccobject_t *result;
	uint32_t index, count, length;
	
	object = checkObject(context, 0);
	result = io_libecc_Array.create();
	length = 0;
	
	for (index = 0, count = elementCount(object); index < count; ++index)
		if (object->element[index].value.check == 1 && !(object->element[index].value.flags & io_libecc_value_hidden))
			addElement(result, length++, io_libecc_Value.chars(io_libecc_Chars.create("%d", index)), 0);
	
	parent = object;
	while (( parent = parent->prototype ))
	{
		for (index = 2; index < parent->hashmapCount; ++index)
		{
			struct eccvalue_t value = parent->hashmap[index].value;
			if (value.check == 1 && value.flags & io_libecc_value_asOwn & !(value.flags & io_libecc_value_hidden))
				addElement(result, length++, io_libecc_Value.text(io_libecc_Key.textOf(value.key)), 0);
		}
	}
	
	for (index = 2; index < object->hashmapCount; ++index)
		if (object->hashmap[index].value.check == 1 && !(object->hashmap[index].value.flags & io_libecc_value_hidden))
			addElement(result, length++, io_libecc_Value.text(io_libecc_Key.textOf(object->hashmap[index].value.key)), 0);
	
	return io_libecc_Value.object(result);
}

// MARK: - Methods

void setup ()
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	assert(sizeof(*io_libecc_object_prototype->hashmap) == 32);
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_object_constructor, constructor, 1,
		NULL, io_libecc_Value.object(io_libecc_object_prototype),
		NULL);
	
	io_libecc_Function.addMethod(io_libecc_object_constructor, "getPrototypeOf", getPrototypeOf, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "getOwnPropertyDescriptor", getOwnPropertyDescriptor, 2, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "getOwnPropertyNames", getOwnPropertyNames, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "create", objectCreate, 2, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "defineProperty", defineProperty, 3, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "defineProperties", defineProperties, 2, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "seal", seal, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "freeze", freeze, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "preventExtensions", preventExtensions, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "isSealed", isSealed, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "isFrozen", isFrozen, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "isExtensible", isExtensible, 1, h);
	io_libecc_Function.addMethod(io_libecc_object_constructor, "keys", keys, 1, h);
	
	io_libecc_Function.addToObject(io_libecc_object_prototype, "toString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_object_prototype, "toLocaleString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_object_prototype, "valueOf", valueOf, 0, h);
	io_libecc_Function.addToObject(io_libecc_object_prototype, "hasOwnProperty", hasOwnProperty, 1, h);
	io_libecc_Function.addToObject(io_libecc_object_prototype, "isPrototypeOf", isPrototypeOf, 1, h);
	io_libecc_Function.addToObject(io_libecc_object_prototype, "propertyIsEnumerable", propertyIsEnumerable, 1, h);
}

void teardown (void)
{
	io_libecc_object_prototype = NULL;
	io_libecc_object_constructor = NULL;
}

struct eccobject_t * create (struct eccobject_t *prototype)
{
	return createSized(prototype, defaultSize);
}

struct eccobject_t * createSized (struct eccobject_t *prototype, uint16_t size)
{
	struct eccobject_t *self = calloc(sizeof(*self), 1);
	io_libecc_Pool.addObject(self);
	return initializeSized(self, prototype, size);
}

struct eccobject_t * createTyped (const struct io_libecc_object_Type *type)
{
	struct eccobject_t *self = createSized(io_libecc_object_prototype, defaultSize);
	self->type = type;
	return self;
}

struct eccobject_t * initialize (struct eccobject_t * restrict self, struct eccobject_t * restrict prototype)
{
	return initializeSized(self, prototype, defaultSize);
}

struct eccobject_t * initializeSized (struct eccobject_t * restrict self, struct eccobject_t * restrict prototype, uint16_t size)
{
	size_t byteSize;
	
	assert(self);
	
	*self = io_libecc_Object.identity;
	
	self->type = prototype? prototype->type: &io_libecc_object_type;
	
	self->prototype = prototype;
	self->hashmapCount = 2;
	
	// hashmap is always 2 slots minimum
	// slot 0 is self referencing undefined value (all zeroes)
	// slot 1 is entry point, referencing undefined (slot 0) by default (all zeroes)
	
	if (size > 0)
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

struct eccobject_t * finalize (struct eccobject_t *self)
{
	assert(self);
	
	if (self->type->finalize)
		self->type->finalize(self);
	
	free(self->hashmap), self->hashmap = NULL;
	free(self->element), self->element = NULL;
	
	return self;
}

struct eccobject_t * copy (const struct eccobject_t *original)
{
	size_t byteSize;
	
	struct eccobject_t *self = malloc(sizeof(*self));
	io_libecc_Pool.addObject(self);
	
	*self = *original;
	
	byteSize = sizeof(*self->element) * self->elementCount;
	self->element = malloc(byteSize);
	memcpy(self->element, original->element, byteSize);
	
	byteSize = sizeof(*self->hashmap) * self->hashmapCount;
	self->hashmap = malloc(byteSize);
	memcpy(self->hashmap, original->hashmap, byteSize);
	
	return self;
}

void destroy (struct eccobject_t *self)
{
	assert(self);
	
	free(self), self = NULL;
}

struct eccvalue_t * member (struct eccobject_t *self, struct io_libecc_Key member, enum io_libecc_value_Flags flags)
{
	int lookupChain = !(flags & io_libecc_value_asOwn);
	struct eccobject_t *object = self;
	struct eccvalue_t *ref = NULL;
	uint32_t slot;
	
	assert(self);
	
	do
	{
		if (( slot = getSlot(object, member) ))
		{
			ref = &object->hashmap[slot].value;
			if (ref->check == 1)
				return lookupChain || object == self || (ref->flags & flags) ? ref: NULL;
		}
	}
	while ((object = object->prototype));
	
	return NULL;
}

struct eccvalue_t * element (struct eccobject_t *self, uint32_t index, enum io_libecc_value_Flags flags)
{
	int lookupChain = !(flags & io_libecc_value_asOwn);
	struct eccobject_t *object = self;
	struct eccvalue_t *ref = NULL;
	
	assert(self);
	
	if (self->type == &io_libecc_string_type)
	{
		struct eccvalue_t *ref = io_libecc_Object.addMember(self, io_libecc_key_none, io_libecc_String.valueAtIndex((struct io_libecc_String *)self, index), 0);
		ref->check = 0;
		return ref;
	}
	else if (index > io_libecc_object_ElementMax)
	{
		struct io_libecc_Key key = keyOfIndex(index, 0);
		if (key.data.integer)
			return member(self, key, flags);
	}
	else
		do
		{
			if (index < object->elementCount)
			{
				ref = &object->element[index].value;
				if (ref->check == 1)
					return lookupChain || object == self || (ref->flags & flags) ? ref: NULL;
			}
		}
		while ((object = object->prototype));
	
	return NULL;
}

struct eccvalue_t * property (struct eccobject_t *self, struct eccvalue_t property, enum io_libecc_value_Flags flags)
{
	struct io_libecc_Key key;
	uint32_t index = getIndexOrKey(property, &key);
	
	if (index < UINT32_MAX)
		return element(self, index, flags);
	else
		return member(self, key, flags);
}

struct eccvalue_t getValue (struct eccstate_t *context, struct eccobject_t *self, struct eccvalue_t *ref)
{
	if (!ref)
		return io_libecc_value_undefined;
	
	if (ref->flags & io_libecc_value_accessor)
	{
		if (!context)
			io_libecc_Ecc.fatal("cannot use getter outside context");
		
		if (ref->flags & io_libecc_value_getter)
			return io_libecc_Context.callFunction(context, ref->data.function, io_libecc_Value.object(self), 0 | io_libecc_context_asAccessor);
		else if (ref->data.function->pair)
			return io_libecc_Context.callFunction(context, ref->data.function->pair, io_libecc_Value.object(self), 0 | io_libecc_context_asAccessor);
		else
			return io_libecc_value_undefined;
	}
	
	return *ref;
}

struct eccvalue_t getMember (struct eccstate_t *context, struct eccobject_t *self, struct io_libecc_Key key)
{
	return getValue(context, self, member(self, key, 0));
}

struct eccvalue_t getElement (struct eccstate_t *context, struct eccobject_t *self, uint32_t index)
{
	if (self->type == &io_libecc_string_type)
		return io_libecc_String.valueAtIndex((struct io_libecc_String *)self, index);
	else
		return getValue(context, self, element(self, index, 0));
}

struct eccvalue_t getProperty (struct eccstate_t *context, struct eccobject_t *self, struct eccvalue_t property)
{
	struct io_libecc_Key key;
	uint32_t index = getIndexOrKey(property, &key);
	
	if (index < UINT32_MAX)
		return getElement(context, self, index);
	else
		return getMember(context, self, key);
}

struct eccvalue_t putValue (struct eccstate_t *context, struct eccobject_t *self, struct eccvalue_t *ref, struct eccvalue_t value)
{
	if (ref->flags & io_libecc_value_accessor)
	{
		assert(context);
		
		if (ref->flags & io_libecc_value_setter)
			io_libecc_Context.callFunction(context, ref->data.function, io_libecc_Value.object(self), 1 | io_libecc_context_asAccessor, value);
		else if (ref->data.function->pair)
			io_libecc_Context.callFunction(context, ref->data.function->pair, io_libecc_Value.object(self), 1 | io_libecc_context_asAccessor, value);
		else if (context->strictMode || (context->parent && context->parent->strictMode))
			readonlyError(context, ref, self);
		
		return value;
	}
	
	if (ref->check == 1)
	{
		if (ref->flags & io_libecc_value_readonly)
		{
			if (context->strictMode)
				readonlyError(context, ref, self);
			else
				return value;
		}
		else
			value.flags = ref->flags;
	}
	
	return *ref = value;
}

struct eccvalue_t putMember (struct eccstate_t *context, struct eccobject_t *self, struct io_libecc_Key key, struct eccvalue_t value)
{
	struct eccvalue_t *ref;
	
	value.flags = 0;
	
	if (( ref = member(self, key, io_libecc_value_asOwn | io_libecc_value_accessor) ))
		return putValue(context, self, ref, value);
	else if (self->prototype && ( ref = member(self->prototype, key, 0) ))
	{
		if (ref->flags & io_libecc_value_readonly)
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is readonly", io_libecc_Key.textOf(key)->length, io_libecc_Key.textOf(key)->bytes));
	}
	
	if (self->flags & io_libecc_object_sealed)
		io_libecc_Context.typeError(context, io_libecc_Chars.create("object is not extensible"));
	
	return *addMember(self, key, value, 0);
}

struct eccvalue_t putElement (struct eccstate_t *context, struct eccobject_t *self, uint32_t index, struct eccvalue_t value)
{
	struct eccvalue_t *ref;
	
	if (index > io_libecc_object_ElementMax)
	{
		if (self->elementCapacity <= index)
			resizeElement(self, index < UINT32_MAX? index + 1: index);
		else if (self->elementCount <= index)
			self->elementCount = index + 1;
		
		return putMember(context, self, keyOfIndex(index, 1), value);
	}
	
	value.flags = 0;
	
	if (( ref = element(self, index, io_libecc_value_asOwn | io_libecc_value_accessor) ))
		return putValue(context, self, ref, value);
	else if (self->prototype && ( ref = element(self, index, 0) ))
	{
		if (ref->flags & io_libecc_value_readonly)
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%u' is readonly", index, index));
	}
	
	if (self->flags & io_libecc_object_sealed)
		io_libecc_Context.typeError(context, io_libecc_Chars.create("object is not extensible"));
	
	return *addElement(self, index, value, 0);
}

struct eccvalue_t putProperty (struct eccstate_t *context, struct eccobject_t *self, struct eccvalue_t primitive, struct eccvalue_t value)
{
	struct io_libecc_Key key;
	uint32_t index = getIndexOrKey(primitive, &key);
	
	if (index < UINT32_MAX)
		return putElement(context, self, index, value);
	else
		return putMember(context, self, key, value);
}

struct eccvalue_t * addMember (struct eccobject_t *self, struct io_libecc_Key key, struct eccvalue_t value, enum io_libecc_value_Flags flags)
{
	uint32_t slot = 1;
	int depth = 0;
	
	assert(self);
	
	do
	{
		if (!self->hashmap[slot].slot[key.data.depth[depth]])
		{
			int need = 4 - depth - (self->hashmapCapacity - self->hashmapCount);
			if (need > 0)
			{
				uint16_t capacity = self->hashmapCapacity;
				self->hashmapCapacity = self->hashmapCapacity? self->hashmapCapacity * 2: 2;
				self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);
				memset(self->hashmap + capacity, 0, sizeof(*self->hashmap) * (self->hashmapCapacity - capacity));
			}
			
			do
			{
				assert(self->hashmapCount < UINT16_MAX);
				slot = self->hashmap[slot].slot[key.data.depth[depth]] = self->hashmapCount++;
			} while (++depth < 4);
			break;
		}
		else
			assert(self->hashmap[slot].value.check != 1);
		
		slot = self->hashmap[slot].slot[key.data.depth[depth]];
		assert(slot != 1);
		assert(slot < self->hashmapCount);
	} while (++depth < 4);
	
	if (value.flags & io_libecc_value_accessor)
		if (self->hashmap[slot].value.check == 1 && self->hashmap[slot].value.flags & io_libecc_value_accessor)
			if ((self->hashmap[slot].value.flags & io_libecc_value_accessor) != (value.flags & io_libecc_value_accessor))
				value.data.function->pair = self->hashmap[slot].value.data.function;
	
	value.key = key;
	value.flags |= flags;
	
	self->hashmap[slot].value = value;
	
	return &self->hashmap[slot].value;
}

struct eccvalue_t * addElement (struct eccobject_t *self, uint32_t index, struct eccvalue_t value, enum io_libecc_value_Flags flags)
{
	struct eccvalue_t *ref;
	
	assert(self);
	
	if (self->elementCapacity <= index)
		resizeElement(self, index < UINT32_MAX? index + 1: index);
	else if (self->elementCount <= index)
		self->elementCount = index + 1;
	
	if (index > io_libecc_object_ElementMax)
		return addMember(self, keyOfIndex(index, 1), value, flags);
	
	ref = &self->element[index].value;
	
	value.flags |= flags;
	*ref = value;
	
	return ref;
}

struct eccvalue_t * addProperty (struct eccobject_t *self, struct eccvalue_t primitive, struct eccvalue_t value, enum io_libecc_value_Flags flags)
{
	struct io_libecc_Key key;
	uint32_t index = getIndexOrKey(primitive, &key);
	
	if (index < UINT32_MAX)
		return addElement(self, index, value, flags);
	else
		return addMember(self, key, value, flags);
}

int deleteMember (struct eccobject_t *self, struct io_libecc_Key member)
{
	struct eccobject_t *object = self;
	uint32_t slot, refSlot;
	
	assert(object);
	assert(member.data.integer);
	
	refSlot =
		self->hashmap[
		self->hashmap[
		self->hashmap[1]
		.slot[member.data.depth[0]]]
		.slot[member.data.depth[1]]]
		.slot[member.data.depth[2]];
	
	slot = self->hashmap[refSlot].slot[member.data.depth[3]];
	
	if (!slot || !(object->hashmap[slot].value.check == 1))
		return 1;
	
	if (object->hashmap[slot].value.flags & io_libecc_value_sealed)
		return 0;
	
	object->hashmap[slot].value = io_libecc_value_undefined;
	self->hashmap[refSlot].slot[member.data.depth[3]] = 0;
	return 1;
}

int deleteElement (struct eccobject_t *self, uint32_t index)
{
	assert(self);
	
	if (index > io_libecc_object_ElementMax)
	{
		struct io_libecc_Key key = keyOfIndex(index, 0);
		if (key.data.integer)
			return deleteMember(self, key);
		else
			return 1;
	}
	
	if (index < self->elementCount)
	{
		if (self->element[index].value.flags & io_libecc_value_sealed)
			return 0;
		
		memset(&self->element[index], 0, sizeof(*self->element));
	}
	
	return 1;
}

int deleteProperty (struct eccobject_t *self, struct eccvalue_t primitive)
{
	struct io_libecc_Key key;
	uint32_t index = getIndexOrKey(primitive, &key);
	
	if (index < UINT32_MAX)
		return deleteElement(self, index);
	else
		return deleteMember(self, key);
}

void packValue (struct eccobject_t *self)
{
	union io_libecc_object_Hashmap data;
	uint32_t index = 2, valueIndex = 2, copyIndex, slot;
	
	assert(self);
	
	for (; index < self->hashmapCount; ++index)
		if (self->hashmap[index].value.check == 1)
		{
			data = self->hashmap[index];
			for (copyIndex = index; copyIndex > valueIndex; --copyIndex)
			{
				self->hashmap[copyIndex] = self->hashmap[copyIndex - 1];
				if (!(self->hashmap[copyIndex].value.check == 1))
					for (slot = 0; slot < 16; ++slot)
					{
						if (self->hashmap[copyIndex].slot[slot] == index)
							self->hashmap[copyIndex].slot[slot] = valueIndex;
						else if (self->hashmap[copyIndex].slot[slot] >= valueIndex && self->hashmap[copyIndex].slot[slot] < index)
							self->hashmap[copyIndex].slot[slot]++;
					}
			}
			for (slot = 0; slot < 16; ++slot)
				if (self->hashmap[1].slot[slot] >= valueIndex && self->hashmap[1].slot[slot] < index)
					self->hashmap[1].slot[slot]++;
			
			self->hashmap[valueIndex++] = data;
		}
	
	self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * (self->hashmapCount));
	self->hashmapCapacity = self->hashmapCount;
	
	if (self->elementCount)
		self->elementCapacity = self->elementCount;
}

void stripMap (struct eccobject_t *self)
{
	uint32_t index = 2;
	
	assert(self);
	
	while (index < self->hashmapCount && self->hashmap[index].value.check == 1)
		++index;
	
	self->hashmapCapacity = self->hashmapCount = index;
	self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);
	
	memset(self->hashmap + 1, 0, sizeof(*self->hashmap));
}

void reserveSlots (struct eccobject_t *self, uint16_t slots)
{
	int need = (slots * 4) - (self->hashmapCapacity - self->hashmapCount);
	
	assert(slots < self->hashmapCapacity);
	
	if (need > 0)
	{
		uint16_t capacity = self->hashmapCapacity;
		self->hashmapCapacity = self->hashmapCapacity? self->hashmapCapacity * 2: 2;
		self->hashmap = realloc(self->hashmap, sizeof(*self->hashmap) * self->hashmapCapacity);
		memset(self->hashmap + capacity, 0, sizeof(*self->hashmap) * (self->hashmapCapacity - capacity));
	}
}

int resizeElement (struct eccobject_t *self, uint32_t size)
{
	uint32_t capacity;
	
	if (size <= self->elementCapacity)
		capacity = self->elementCapacity;
	else if (size < 4)
	{
		/* 64-bytes mini */
		capacity = 4;
	}
	else if (size < 64)
	{
		/* power of two steps between */
		capacity = nextPowerOfTwo(size);
	}
	else if (size > io_libecc_object_ElementMax)
		capacity = io_libecc_object_ElementMax + 1;
	else
	{
		/* 1024-bytes chunk */
		capacity = size - 1;
		capacity |= 63;
		++capacity;
	}
	
	assert(self);
	
	if (capacity != self->elementCapacity)
	{
		if (size > io_libecc_object_ElementMax)
		{
			io_libecc_Env.printWarning("Faking array length of %u while actual physical length is %u. Using array length > 0x%x is discouraged", size, capacity, io_libecc_object_ElementMax);
		}
		
		self->element = realloc(self->element, sizeof(*self->element) * capacity);
		if (capacity > self->elementCapacity)
			memset(self->element + self->elementCapacity, 0, sizeof(*self->element) * (capacity - self->elementCapacity));
		
		self->elementCapacity = capacity;
	}
	else if (size < self->elementCount)
	{
		union io_libecc_object_Element *element;
		uint32_t until = size, e;
		
		if (self->elementCount > io_libecc_object_ElementMax)
		{
			union io_libecc_object_Hashmap *hashmap;
			uint32_t index, h;
			
			for (h = 2; h < self->hashmapCount; ++h)
			{
				hashmap = &self->hashmap[h];
				if (hashmap->value.check == 1)
				{
					index = io_libecc_Lexer.scanElement(*io_libecc_Key.textOf(hashmap->value.key));
					if (hashmap->value.check == 1 && (hashmap->value.flags & io_libecc_value_sealed) && index >= until)
						until = index + 1;
				}
			}
			
			for (h = 2; h < self->hashmapCount; ++h)
			{
				hashmap = &self->hashmap[h];
				if (hashmap->value.check == 1)
					if (io_libecc_Lexer.scanElement(*io_libecc_Key.textOf(hashmap->value.key)) >= until)
						self->hashmap[h].value.check = 0;
			}
			
			if (until > size)
			{
				self->elementCount = until;
				return 1;
			}
			self->elementCount = self->elementCapacity;
		}
		
		for (e = size; e < self->elementCount; ++e)
		{
			element = &self->element[e];
			if (element->value.check == 1 && (element->value.flags & io_libecc_value_sealed) && e >= until)
				until = e + 1;
		}
		
		memset(self->element + until, 0, sizeof(*self->element) * (self->elementCount - until));
		
		if (until > size)
		{
			self->elementCount = until;
			return 1;
		}
	}
	self->elementCount = size;
	
	return 0;
}

void populateElementWithCList (struct eccobject_t *self, uint32_t count, const char * list[])
{
	double binary;
	char *end;
	int index;
	
	assert(self);
	assert(count <= io_libecc_object_ElementMax);
	
	if (count > self->elementCount)
		resizeElement(self, count);
	
	for (index = 0; index < count; ++index)
	{
		uint16_t length = (uint16_t)strlen(list[index]);
		binary = strtod(list[index], &end);
		
		if (end == list[index] + length)
			self->element[index].value = io_libecc_Value.binary(binary);
		else
		{
			struct io_libecc_Chars *chars = io_libecc_Chars.createSized(length);
			memcpy(chars->bytes, list[index], length);
			
			self->element[index].value = io_libecc_Value.chars(chars);
		}
	}
}

struct eccvalue_t toString (struct eccstate_t * const context)
{
	if (context->this.type == io_libecc_value_nullType)
		return io_libecc_Value.text(&io_libecc_text_nullType);
	else if (context->this.type == io_libecc_value_undefinedType)
		return io_libecc_Value.text(&io_libecc_text_undefinedType);
	else if (io_libecc_Value.isString(context->this))
		return io_libecc_Value.text(&io_libecc_text_stringType);
	else if (io_libecc_Value.isNumber(context->this))
		return io_libecc_Value.text(&io_libecc_text_numberType);
	else if (io_libecc_Value.isBoolean(context->this))
		return io_libecc_Value.text(&io_libecc_text_booleanType);
	else if (io_libecc_Value.isObject(context->this))
		return io_libecc_Value.text(context->this.data.object->type->text);
	else
		assert(0);
	
	return io_libecc_value_undefined;
}

void dumpTo(struct eccobject_t *self, FILE *file)
{
	uint32_t index, count;
	int isArray;
	
	assert(self);
	
	isArray = io_libecc_Value.objectIsArray(self);
	
	fprintf(file, isArray? "[ ": "{ ");
	
	for (index = 0, count = elementCount(self); index < count; ++index)
	{
		if (self->element[index].value.check == 1)
		{
			if (!isArray)
				fprintf(file, "%d: ", (int)index);
			
			if (self->element[index].value.type == io_libecc_value_objectType && self->element[index].value.data.object == self)
				fprintf(file, "this");
			else
				io_libecc_Value.dumpTo(self->element[index].value, file);
			
			fprintf(file, ", ");
		}
	}
	
	if (!isArray)
	{
		for (index = 0; index < self->hashmapCount; ++index)
		{
			if (self->hashmap[index].value.check == 1)
			{
				fprintf(stderr, "'");
				io_libecc_Key.dumpTo(self->hashmap[index].value.key, file);
				fprintf(file, "': ");
				
				if (self->hashmap[index].value.type == io_libecc_value_objectType && self->hashmap[index].value.data.object == self)
					fprintf(file, "this");
				else
					io_libecc_Value.dumpTo(self->hashmap[index].value, file);
				
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
	
	fprintf(file, isArray? "]": "}");
}
