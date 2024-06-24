//
//  oplist.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "oplist.h"

// MARK: - Private

// MARK: - Static Members

// MARK: - Methods
static struct io_libecc_OpList* create(const io_libecc_native_io_libecc_Function native, struct io_libecc_Value value, struct io_libecc_Text text);
static void destroy(struct io_libecc_OpList*);
static struct io_libecc_OpList* join(struct io_libecc_OpList*, struct io_libecc_OpList*);
static struct io_libecc_OpList* join3(struct io_libecc_OpList*, struct io_libecc_OpList*, struct io_libecc_OpList*);
static struct io_libecc_OpList* joinDiscarded(struct io_libecc_OpList*, uint16_t n, struct io_libecc_OpList*);
static struct io_libecc_OpList* unshift(struct io_libecc_Op op, struct io_libecc_OpList*);
static struct io_libecc_OpList* unshiftJoin(struct io_libecc_Op op, struct io_libecc_OpList*, struct io_libecc_OpList*);
static struct io_libecc_OpList* unshiftJoin3(struct io_libecc_Op op, struct io_libecc_OpList*, struct io_libecc_OpList*, struct io_libecc_OpList*);
static struct io_libecc_OpList* shift(struct io_libecc_OpList*);
static struct io_libecc_OpList* append(struct io_libecc_OpList*, struct io_libecc_Op op);
static struct io_libecc_OpList* appendNoop(struct io_libecc_OpList*);
static struct io_libecc_OpList*
createLoop(struct io_libecc_OpList* initial, struct io_libecc_OpList* condition, struct io_libecc_OpList* step, struct io_libecc_OpList* body, int reverseCondition);
static void optimizeWithEnvironment(struct io_libecc_OpList*, struct io_libecc_Object* environment, uint32_t index);
static void dumpTo(struct io_libecc_OpList*, FILE* file);
static struct io_libecc_Text text(struct io_libecc_OpList* oplist);
const struct type_io_libecc_OpList io_libecc_OpList = {
    create, destroy, join,       join3,      joinDiscarded,           unshift, unshiftJoin, unshiftJoin3,
    shift,  append,  appendNoop, createLoop, optimizeWithEnvironment, dumpTo,  text,
};

struct io_libecc_OpList * create (const io_libecc_native_io_libecc_Function native, struct io_libecc_Value value, struct io_libecc_Text text)
{
	struct io_libecc_OpList *self = malloc(sizeof(*self));
	self->ops = malloc(sizeof(*self->ops) * 1);
	self->ops[0] = io_libecc_Op.make(native, value, text);
	self->count = 1;
	return self;
}

void destroy (struct io_libecc_OpList * self)
{
	assert(self);
	
	free(self->ops), self->ops = NULL;
	free(self), self = NULL;
}

struct io_libecc_OpList * join (struct io_libecc_OpList *self, struct io_libecc_OpList *with)
{
	if (!self)
		return with;
	else if (!with)
		return self;
	
	self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + with->count));
	memcpy(self->ops + self->count, with->ops, sizeof(*self->ops) * with->count);
	self->count += with->count;
	
	destroy(with), with = NULL;
	
	return self;
}

struct io_libecc_OpList * join3 (struct io_libecc_OpList *self, struct io_libecc_OpList *a, struct io_libecc_OpList *b)
{
	if (!self)
		return join(a, b);
	else if (!a)
		return join(self, b);
	else if (!b)
		return join(self, a);
	
	self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + a->count + b->count));
	memcpy(self->ops + self->count, a->ops, sizeof(*self->ops) * a->count);
	memcpy(self->ops + self->count + a->count, b->ops, sizeof(*self->ops) * b->count);
	self->count += a->count + b->count;
	
	destroy(a), a = NULL;
	destroy(b), b = NULL;
	
	return self;
}

struct io_libecc_OpList * joinDiscarded (struct io_libecc_OpList *self, uint16_t n, struct io_libecc_OpList *with)
{
	while (n > 16)
	{
		self = io_libecc_OpList.append(self, io_libecc_Op.make(io_libecc_Op.discardN, io_libecc_Value.integer(16), io_libecc_text_empty));
		n -= 16;
	}
	
	if (n == 1)
		self = io_libecc_OpList.append(self, io_libecc_Op.make(io_libecc_Op.discard, io_libecc_value_undefined, io_libecc_text_empty));
	else
		self = io_libecc_OpList.append(self, io_libecc_Op.make(io_libecc_Op.discardN, io_libecc_Value.integer(n), io_libecc_text_empty));
	
	return join(self, with);
}

struct io_libecc_OpList * unshift (struct io_libecc_Op op, struct io_libecc_OpList *self)
{
	if (!self)
		return create(op.native, op.value, op.text);
	
	self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + 1));
	memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count++);
	self->ops[0] = op;
	return self;
}

struct io_libecc_OpList * unshiftJoin (struct io_libecc_Op op, struct io_libecc_OpList *self, struct io_libecc_OpList *with)
{
	if (!self)
		return unshift(op, with);
	else if (!with)
		return unshift(op, self);
	
	self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + with->count + 1));
	memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
	memcpy(self->ops + self->count + 1, with->ops, sizeof(*self->ops) * with->count);
	self->ops[0] = op;
	self->count += with->count + 1;
	
	destroy(with), with = NULL;
	
	return self;
}

struct io_libecc_OpList * unshiftJoin3 (struct io_libecc_Op op, struct io_libecc_OpList *self, struct io_libecc_OpList *a, struct io_libecc_OpList *b)
{
	if (!self)
		return unshiftJoin(op, a, b);
	else if (!a)
		return unshiftJoin(op, self, b);
	else if (!b)
		return unshiftJoin(op, self, a);
	
	self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + a->count + b->count + 1));
	memmove(self->ops + 1, self->ops, sizeof(*self->ops) * self->count);
	memcpy(self->ops + self->count + 1, a->ops, sizeof(*self->ops) * a->count);
	memcpy(self->ops + self->count + a->count + 1, b->ops, sizeof(*self->ops) * b->count);
	self->ops[0] = op;
	self->count += a->count + b->count + 1;
	
	destroy(a), a = NULL;
	destroy(b), b = NULL;
	
	return self;
}

struct io_libecc_OpList * shift (struct io_libecc_OpList *self)
{
	memmove(self->ops, self->ops + 1, sizeof(*self->ops) * --self->count);
	return self;
}

struct io_libecc_OpList * append (struct io_libecc_OpList *self, struct io_libecc_Op op)
{
	if (!self)
		return create(op.native, op.value, op.text);
	
	self->ops = realloc(self->ops, sizeof(*self->ops) * (self->count + 1));
	self->ops[self->count++] = op;
	return self;
}

struct io_libecc_OpList * appendNoop (struct io_libecc_OpList *self)
{
	return append(self, io_libecc_Op.make(io_libecc_Op.noop, io_libecc_value_undefined, io_libecc_text_empty));
}

struct io_libecc_OpList * createLoop (struct io_libecc_OpList * initial, struct io_libecc_OpList * condition, struct io_libecc_OpList * step, struct io_libecc_OpList * body, int reverseCondition)
{
	if (condition && step && condition->count == 3 && !reverseCondition)
	{
		if (condition->ops[1].native == io_libecc_Op.getLocal && (
			condition->ops[0].native == io_libecc_Op.less ||
			condition->ops[0].native == io_libecc_Op.lessOrEqual ))
			if (step->count >= 2 && step->ops[1].value.data.key.data.integer == condition->ops[1].value.data.key.data.integer)
			{
				struct io_libecc_Value stepValue;
				if (step->count == 2 && (step->ops[0].native == io_libecc_Op.incrementRef || step->ops[0].native == io_libecc_Op.postIncrementRef))
					stepValue = io_libecc_Value.integer(1);
				else if (step->count == 3 && step->ops[0].native == io_libecc_Op.addAssignRef && step->ops[2].native == io_libecc_Op.value && step->ops[2].value.type == io_libecc_value_integerType && step->ops[2].value.data.integer > 0)
					stepValue = step->ops[2].value;
				else
					goto normal;
				
				if (condition->ops[2].native == io_libecc_Op.getLocal)
					body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[2].value, condition->ops[2].text), body);
				else if (condition->ops[2].native == io_libecc_Op.value)
					body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.valueConstRef, condition->ops[2].value, condition->ops[2].text), body);
				else
					goto normal;
				
				body = io_libecc_OpList.appendNoop(io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[1].value, condition->ops[1].text), body));
				body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.value, stepValue, condition->ops[0].text), body);
				body = io_libecc_OpList.unshift(io_libecc_Op.make(condition->ops[0].native == io_libecc_Op.less? io_libecc_Op.iterateLessRef: io_libecc_Op.iterateLessOrEqualRef, io_libecc_Value.integer(body->count), condition->ops[0].text), body);
				io_libecc_OpList.destroy(condition), condition = NULL;
				io_libecc_OpList.destroy(step), step = NULL;
				return io_libecc_OpList.join(initial, body);
			}
		
		if (condition->ops[1].native == io_libecc_Op.getLocal && (
			condition->ops[0].native == io_libecc_Op.more ||
			condition->ops[0].native == io_libecc_Op.moreOrEqual ))
			if (step->count >= 2 && step->ops[1].value.data.key.data.integer == condition->ops[1].value.data.key.data.integer)
			{
				struct io_libecc_Value stepValue;
				if (step->count == 2 && (step->ops[0].native == io_libecc_Op.decrementRef || step->ops[0].native == io_libecc_Op.postDecrementRef))
					stepValue = io_libecc_Value.integer(1);
				else if (step->count == 3 && step->ops[0].native == io_libecc_Op.minusAssignRef && step->ops[2].native == io_libecc_Op.value && step->ops[2].value.type == io_libecc_value_integerType && step->ops[2].value.data.integer > 0)
					stepValue = step->ops[2].value;
				else
					goto normal;
				
				if (condition->ops[2].native == io_libecc_Op.getLocal)
					body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[2].value, condition->ops[2].text), body);
				else if (condition->ops[2].native == io_libecc_Op.value)
					body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.valueConstRef, condition->ops[2].value, condition->ops[2].text), body);
				else
					goto normal;
				
				body = io_libecc_OpList.appendNoop(io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.getLocalRef, condition->ops[1].value, condition->ops[1].text), body));
				body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.value, stepValue, condition->ops[0].text), body);
				body = io_libecc_OpList.unshift(io_libecc_Op.make(condition->ops[0].native == io_libecc_Op.more? io_libecc_Op.iterateMoreRef: io_libecc_Op.iterateMoreOrEqualRef, io_libecc_Value.integer(body->count), condition->ops[0].text), body);
				io_libecc_OpList.destroy(condition), condition = NULL;
				io_libecc_OpList.destroy(step), step = NULL;
				return io_libecc_OpList.join(initial, body);
			}
	}
	
normal:
	{
		int skipOpCount;
		
		if (!condition)
			condition = io_libecc_OpList.create(io_libecc_Op.value, io_libecc_Value.truth(1), io_libecc_text_empty);
		
		if (step)
		{
			step = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.discard, io_libecc_value_none, io_libecc_text_empty), step);
			skipOpCount = step->count;
		}
		else
			skipOpCount = 0;
		
		body = io_libecc_OpList.appendNoop(body);
		
		if (reverseCondition)
		{
			skipOpCount += condition->count + body->count;
			body = io_libecc_OpList.append(body, io_libecc_Op.make(io_libecc_Op.value, io_libecc_Value.truth(1), io_libecc_text_empty));
			body = io_libecc_OpList.append(body, io_libecc_Op.make(io_libecc_Op.jump, io_libecc_Value.integer(-body->count - 1), io_libecc_text_empty));
		}
		
		body = io_libecc_OpList.join(io_libecc_OpList.join(step, condition), body);
		body = io_libecc_OpList.unshift(io_libecc_Op.make(io_libecc_Op.jump, io_libecc_Value.integer(body->count), io_libecc_text_empty), body);
		initial = io_libecc_OpList.append(initial, io_libecc_Op.make(io_libecc_Op.iterate, io_libecc_Value.integer(skipOpCount), io_libecc_text_empty));
		return io_libecc_OpList.join(initial, body);
	}
}

void optimizeWithEnvironment (struct io_libecc_OpList *self, struct io_libecc_Object *environment, uint32_t selfIndex)
{
	uint32_t index, count, slot, haveLocal = 0, environmentLevel = 0;
	struct io_libecc_Key environments[0xff];
	
	if (!self)
		return;
	
	for (index = 0, count = self->count; index < count; ++index)
	{
		if (self->ops[index].native == io_libecc_Op.with)
		{
			index += self->ops[index].value.data.integer;
			haveLocal = 1;
		}
		
		if (self->ops[index].native == io_libecc_Op.function)
		{
			uint32_t selfIndex = index && self->ops[index - 1].native == io_libecc_Op.setLocalSlot? self->ops[index - 1].value.data.integer: 0;
			optimizeWithEnvironment(self->ops[index].value.data.function->oplist, &self->ops[index].value.data.function->environment, selfIndex);
		}
		
		if (self->ops[index].native == io_libecc_Op.pushEnvironment)
			environments[environmentLevel++] = self->ops[index].value.data.key;
		
		if (self->ops[index].native == io_libecc_Op.popEnvironment)
			--environmentLevel;
		
		if (self->ops[index].native == io_libecc_Op.createLocalRef
			|| self->ops[index].native == io_libecc_Op.getLocalRefOrNull
			|| self->ops[index].native == io_libecc_Op.getLocalRef
			|| self->ops[index].native == io_libecc_Op.getLocal
			|| self->ops[index].native == io_libecc_Op.setLocal
			|| self->ops[index].native == io_libecc_Op.deleteLocal
			)
		{
			struct io_libecc_Object *searchEnvironment = environment;
			uint32_t level;
			
			level = environmentLevel;
			while (level--)
				if (io_libecc_Key.isEqual(environments[level], self->ops[index].value.data.key))
					goto notfound;
			
			level = environmentLevel;
			do
			{
				for (slot = searchEnvironment->hashmapCount; slot--;)
				{
					if (searchEnvironment->hashmap[slot].value.check == 1)
					{
						if (io_libecc_Key.isEqual(searchEnvironment->hashmap[slot].value.key, self->ops[index].value.data.key))
						{
							if (!level)
							{
								self->ops[index] = io_libecc_Op.make(
									self->ops[index].native == io_libecc_Op.createLocalRef? io_libecc_Op.getLocalSlotRef:
									self->ops[index].native == io_libecc_Op.getLocalRefOrNull? io_libecc_Op.getLocalSlotRef:
									self->ops[index].native == io_libecc_Op.getLocalRef? io_libecc_Op.getLocalSlotRef:
									self->ops[index].native == io_libecc_Op.getLocal? io_libecc_Op.getLocalSlot:
									self->ops[index].native == io_libecc_Op.setLocal? io_libecc_Op.setLocalSlot:
									self->ops[index].native == io_libecc_Op.deleteLocal? io_libecc_Op.deleteLocalSlot: NULL
									, io_libecc_Value.integer(slot), self->ops[index].text);
							}
							else if (slot <= INT16_MAX && level <= INT16_MAX)
							{
								self->ops[index] = io_libecc_Op.make(
									self->ops[index].native == io_libecc_Op.createLocalRef? io_libecc_Op.getParentSlotRef:
									self->ops[index].native == io_libecc_Op.getLocalRefOrNull? io_libecc_Op.getParentSlotRef:
									self->ops[index].native == io_libecc_Op.getLocalRef? io_libecc_Op.getParentSlotRef:
									self->ops[index].native == io_libecc_Op.getLocal? io_libecc_Op.getParentSlot:
									self->ops[index].native == io_libecc_Op.setLocal? io_libecc_Op.setParentSlot:
									self->ops[index].native == io_libecc_Op.deleteLocal? io_libecc_Op.deleteParentSlot: NULL
									, io_libecc_Value.integer((level << 16) | slot), self->ops[index].text);
							}
							else
								goto notfound;
							
							if (index > 1 && level == 1 && slot == selfIndex)
							{
								struct io_libecc_Op op = self->ops[index - 1];
								if (op.native == io_libecc_Op.call && self->ops[index - 2].native == io_libecc_Op.result)
								{
									self->ops[index - 1] = io_libecc_Op.make(io_libecc_Op.repopulate, op.value, op.text);
									self->ops[index] = io_libecc_Op.make(io_libecc_Op.value, io_libecc_Value.integer(-index - 1), self->ops[index].text);
								}
							}
							
							goto found;
						}
					}
				}
				
				++level;
			}
			while (( searchEnvironment = searchEnvironment->prototype ));
			
		notfound:
			haveLocal = 1;
		found:
			;
		}
	}
	
	if (!haveLocal)
		io_libecc_Object.stripMap(environment);
}

void dumpTo (struct io_libecc_OpList *self, FILE *file)
{
	uint32_t i;
	
	assert(self);
	
	fputc('\n', stderr);
	if (!self)
		return;
	
	for (i = 0; i < self->count; ++i)
	{
		char c = self->ops[i].text.flags & io_libecc_text_breakFlag? i? '!': 'T': '|';
		fprintf(file, "[%p] %c %s ", (void *)(self->ops + i), c, io_libecc_Op.toChars(self->ops[i].native));
		
		if (self->ops[i].native == io_libecc_Op.function)
		{
			fprintf(file, "{");
			io_libecc_OpList.dumpTo(self->ops[i].value.data.function->oplist, stderr);
			fprintf(file, "} ");
		}
		else if (self->ops[i].native == io_libecc_Op.getParentSlot || self->ops[i].native == io_libecc_Op.getParentSlotRef || self->ops[i].native == io_libecc_Op.setParentSlot)
			fprintf(file, "[-%hu] %hu", (uint16_t)(self->ops[i].value.data.integer >> 16), (uint16_t)(self->ops[i].value.data.integer & 0xffff));
		else if (self->ops[i].value.type != io_libecc_value_undefinedType || self->ops[i].native == io_libecc_Op.value || self->ops[i].native == io_libecc_Op.exchange)
			io_libecc_Value.dumpTo(self->ops[i].value, file);
		
		if (self->ops[i].native == io_libecc_Op.text)
			fprintf(file, "'%.*s'", (int)self->ops[i].text.length, self->ops[i].text.bytes);
		
		if (self->ops[i].text.length)
			fprintf(file, "  `%.*s`", (int)self->ops[i].text.length, self->ops[i].text.bytes);
		
		fputc('\n', stderr);
	}
}

struct io_libecc_Text text (struct io_libecc_OpList *oplist)
{
	uint16_t length;
	if (!oplist)
		return io_libecc_text_empty;
	
	length = oplist->ops[oplist->count - 1].text.bytes + oplist->ops[oplist->count - 1].text.length - oplist->ops[0].text.bytes;
	
	return io_libecc_Text.make(
		oplist->ops[0].text.bytes,
		oplist->ops[0].text.length > length? oplist->ops[0].text.length: length);
}
