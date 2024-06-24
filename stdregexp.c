//
//  regexp.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

// MARK: - Private

static void mark (eccobject_t *object);
static void capture (eccobject_t *object);
static void finalize (eccobject_t *object);
static void setup(void);
static void teardown(void);
static struct io_libecc_RegExp* create(struct io_libecc_Chars* pattern, struct io_libecc_Error**, enum io_libecc_regexp_Options);
static struct io_libecc_RegExp* createWith(eccstate_t* context, eccvalue_t pattern, eccvalue_t flags);
static int matchWithState(struct io_libecc_RegExp*, struct io_libecc_regexp_State*);
const struct type_io_libecc_RegExp io_libecc_RegExp = {
    setup, teardown, create, createWith, matchWithState,
};

eccobject_t * io_libecc_regexp_prototype = NULL;
struct io_libecc_Function * io_libecc_regexp_constructor = NULL;

const struct io_libecc_object_Type io_libecc_regexp_type = {
	.text = &ECC_ConstString_RegexpType,
	.mark = mark,
	.capture = capture,
	.finalize = finalize,
};

#define DUMP_REGEXP 0

enum Opcode {
	opOver = 0,
	opNLookahead = 1,
	opLookahead = 2,
	opStart,
	opEnd,
	opLineStart,
	opLineEnd,
	opBoundary,
	
	opSplit,
	opReference,
	opRedo,
	opSave,
	opNSave,
	opAny,
	opOneOf,
	opNeitherOf,
	opInRange,
	opInRangeCase,
	opDigit,
	opSpace,
	opWord,
	opBytes,
	opJump,
	opMatch,
};

struct io_libecc_regexp_Node {
	char *bytes;
	int16_t offset;
	uint8_t opcode;
	uint8_t depth;
};

struct Parse {
	const char *c;
	const char *end;
	uint16_t count;
	int ignoreCase;
	int multiline;
	int disallowQuantifier;
};

static struct io_libecc_regexp_Node * disjunction (struct Parse *p, struct io_libecc_Error **error);

static
void mark (eccobject_t *object)
{
	struct io_libecc_RegExp *self = (struct io_libecc_RegExp *)object;
	
	io_libecc_Pool.markValue(ECCNSValue.chars(self->pattern));
	io_libecc_Pool.markValue(ECCNSValue.chars(self->source));
}

static
void capture (eccobject_t *object)
{
	struct io_libecc_RegExp *self = (struct io_libecc_RegExp *)object;
	
	++self->pattern->referenceCount;
	++self->source->referenceCount;
}

static
void finalize (eccobject_t *object)
{
	struct io_libecc_RegExp *self = (struct io_libecc_RegExp *)object;
	struct io_libecc_regexp_Node *n = self->program;
	while (n->opcode != opOver)
	{
		if (n->bytes)
			free(n->bytes), n->bytes = NULL;
		
		++n;
	}
	
	free(self->program), self->program = NULL;
	
	--self->pattern->referenceCount;
	--self->source->referenceCount;
}

#if DUMP_REGEXP
static
void printNode (struct io_libecc_regexp_Node *n)
{
	switch (n->opcode)
	{
		case opOver: fprintf(stderr, "over "); break;
		case opNLookahead: fprintf(stderr, "!lookahead "); break;
		case opLookahead: fprintf(stderr, "lookahead "); break;
		case opStart: fprintf(stderr, "start "); break;
		case opEnd: fprintf(stderr, "end "); break;
		case opLineStart: fprintf(stderr, "line start "); break;
		case opLineEnd: fprintf(stderr, "line end "); break;
		case opBoundary: fprintf(stderr, "boundary "); break;
		
		case opSplit: fprintf(stderr, "split "); break;
		case opReference: fprintf(stderr, "reference "); break;
		case opRedo: fprintf(stderr, "redo "); break;
		case opSave: fprintf(stderr, "save "); break;
		case opNSave: fprintf(stderr, "!save "); break;
		case opAny: fprintf(stderr, "any "); break;
		case opOneOf: fprintf(stderr, "one of "); break;
		case opNeitherOf: fprintf(stderr, "neither of "); break;
		case opInRange: fprintf(stderr, "in range "); break;
		case opInRangeCase: fprintf(stderr, "in range (ignore case) "); break;
		case opDigit: fprintf(stderr, "digit "); break;
		case opSpace: fprintf(stderr, "space "); break;
		case opWord: fprintf(stderr, "word "); break;
		case opBytes: fprintf(stderr, "bytes "); break;
		case opJump: fprintf(stderr, "jump "); break;
		case opMatch: fprintf(stderr, "match "); break;
	}
	fprintf(stderr, "%d", n->offset);
	if (n->bytes)
	{
		if (n->opcode == opRedo)
		{
			char *c = n->bytes + 2;
			fprintf(stderr, " {%u-%u}", n->bytes[0], n->bytes[1]);
			if (n->bytes[2])
				fprintf(stderr, " lazy");
			
			if (*(c + 1))
				fprintf(stderr, " clear:");
			
			while (*(++c))
				fprintf(stderr, "%u,", *c);
		}
		else if (n->opcode != opRedo)
			fprintf(stderr, " `%s`", n->bytes);
	}
	putc('\n', stderr);
}
#endif

//MARK: parsing

static
struct io_libecc_regexp_Node * node (enum Opcode opcode, long offset, const char *bytes)
{
	struct io_libecc_regexp_Node *n = calloc(2, sizeof(*n));
	
	if (offset && bytes)
	{
		n[0].bytes = calloc(offset + 1, 1);
		memcpy(n[0].bytes, bytes, offset);
	}
	n[0].offset = offset;
	n[0].opcode = opcode;
	
	return n;
}

static
void toss (struct io_libecc_regexp_Node *node)
{
	struct io_libecc_regexp_Node *n = node;
	
	if (!node)
		return;
	
	while (n->opcode != opOver)
	{
		free(n->bytes), n->bytes = NULL;
		++n;
	}
	
	free(node), node = NULL;
}

static
uint16_t nlen (struct io_libecc_regexp_Node *n)
{
	uint16_t len = 0;
	if (n)
		while (n[++len].opcode != opOver);
	
	return len;
}

static
struct io_libecc_regexp_Node *join (struct io_libecc_regexp_Node *a, struct io_libecc_regexp_Node *b)
{
	uint16_t lena = 0, lenb = 0;
	
	if (!a)
		return b;
	else if (!b)
		return a;
	
	while (a[++lena].opcode != opOver);
	while (b[++lenb].opcode != opOver);
	
	if (lena == 1 && lenb == 1 && a[lena - 1].opcode == opBytes && b->opcode == opBytes)
	{
		struct io_libecc_regexp_Node *c = a + lena - 1;
		
		c->bytes = realloc(c->bytes, c->offset + b->offset + 1);
		memcpy(c->bytes + c->offset, b->bytes, b->offset + 1);
		c->offset += b->offset;
		free(b->bytes), b->bytes = NULL;
	}
	else
	{
		a = realloc(a, sizeof(*a) * (lena + lenb + 1));
		memcpy(a + lena, b, sizeof(*a) * (lenb + 1));
	}
	free(b), b = NULL;
	return a;
}

static
int accept(struct Parse *p, char c)
{
	if (*p->c == c)
	{
		++p->c;
		return 1;
	}
	return 0;
}

static
enum Opcode escape (struct Parse *p, int16_t *offset, char buffer[5])
{
	*offset = 1;
	buffer[0] = *(p->c++);
	
	switch (buffer[0])
	{
		case 'D':
			*offset = 0;
		case 'd':
			return opDigit;
			
		case 'S':
			*offset = 0;
		case 's':
			return opSpace;
			
		case 'W':
			*offset = 0;
		case 'w':
			return opWord;
			
		case 'b': buffer[0] = '\b'; break;
		case 'f': buffer[0] = '\f'; break;
		case 'n': buffer[0] = '\n'; break;
		case 'r': buffer[0] = '\r'; break;
		case 't': buffer[0] = '\t'; break;
		case 'v': buffer[0] = '\v'; break;
		case 'c':
			if (tolower(*p->c) >= 'a' && tolower(*p->c) <= 'z')
				buffer[0] = *(p->c++) % 32;
			
			break;
			
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
			buffer[0] -= '0';
			if (*p->c >= '0' && *p->c <= '7')
			{
				buffer[0] = buffer[0] * 8 + *(p->c++) - '0';
				if (*p->c >= '0' && *p->c <= '7')
				{
					buffer[0] = buffer[0] * 8 + *(p->c++) - '0';
					if (*p->c >= '0' && *p->c <= '7')
					{
						if ((int)buffer[0] * 8 + *p->c - '0' <= 0xFF)
							buffer[0] = buffer[0] * 8 + *(p->c++) - '0';
					}
				}
				
				if (buffer[0])
					*offset = io_libecc_Chars.writeCodepoint(buffer, ((uint8_t *)buffer)[0]);
			}
			break;
			
		case 'x':
			if (isxdigit(p->c[0]) && isxdigit(p->c[1]))
			{
				*offset = io_libecc_Chars.writeCodepoint(buffer, io_libecc_Lexer.uint8Hex(p->c[0], p->c[1]));
				p->c += 2;
			}
			break;
			
		case 'u':
			if (isxdigit(p->c[0]) && isxdigit(p->c[1]) && isxdigit(p->c[2]) && isxdigit(p->c[3]))
			{
				*offset = io_libecc_Chars.writeCodepoint(buffer, io_libecc_Lexer.uint16Hex(p->c[0], p->c[1], p->c[2], p->c[3]));
				p->c += 4;
			}
			break;
			
		default:
			break;
	}
	
	return opBytes;
}

static
enum Opcode character (ecctextstring_t text, int16_t *offset, char buffer[12], int ignoreCase)
{
	if (ignoreCase)
	{
		char *split = ECCNSText.toLower(text, buffer);
		char *check = ECCNSText.toUpper(text, split);
		ptrdiff_t length = check - buffer;
		int codepoints = 0;
		
		*check = '\0';
		while (check-- > buffer)
			if ((*check & 0xc0) != 0x80)
				codepoints++;
		
		if (codepoints == 2 && memcmp(buffer, split, split - buffer))
		{
			*offset = length;
			return opOneOf;
		}
	}
	*offset = text.length;
	memcpy(buffer, text.bytes, text.length);
	return opBytes;
}

static
struct io_libecc_regexp_Node * characterNode (ecctextstring_t text, int ignoreCase)
{
	char buffer[12];
	int16_t offset;
	enum Opcode opcode = character(text, &offset, buffer, ignoreCase);
	
	return node(opcode, offset, buffer);
}

static
struct io_libecc_regexp_Node * term (struct Parse *p, struct io_libecc_Error **error)
{
	struct io_libecc_regexp_Node *n;
	ecctextstring_t text;
	
	p->disallowQuantifier = 0;
	
	if (p->c >= p->end - 1)
		return NULL;
	else if (accept(p, '^'))
	{
		p->disallowQuantifier = 1;
		return node(p->multiline? opLineStart: opStart, 0, NULL);
	}
	else if (accept(p, '$'))
	{
		p->disallowQuantifier = 1;
		return node(p->multiline? opLineEnd: opEnd, 0, NULL);
	}
	else if (accept(p, '\\'))
	{
		switch (*p->c)
		{
			case 'b':
				++p->c;
				p->disallowQuantifier = 1;
				return node(opBoundary, 1, NULL);
				
			case 'B':
				++p->c;
				p->disallowQuantifier = 1;
				return node(opBoundary, 0, NULL);
				
			case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			{
				int c = *(p->c++) - '0';
				while (isdigit(*(p->c)))
					c = c * 10 + *(p->c++) - '0';
				
				return node(opReference, c, NULL);
			}
				
			default:
			{
				enum Opcode opcode;
				int16_t offset;
				char buffer[5];
				
				opcode = escape(p, &offset, buffer);
				if (opcode == opBytes)
				{
					text = ECCNSText.make(buffer, offset);
					return characterNode(text, p->ignoreCase);
				}
				else
					return node(opcode, offset, NULL);
			}
		}
	}
	else if (accept(p, '('))
	{
		unsigned char count = 0;
		char modifier = '\0';
		
		if (accept(p, '?'))
		{
			if (*p->c == '=' || *p->c == '!' || *p->c == ':')
				modifier = *(p->c++);
		}
		else
		{
			count = ++p->count;
			if ((int)count * 2 + 1 > 0xff)
			{
				*error = io_libecc_Error.syntaxError(ECCNSText.make(p->c, 1), io_libecc_Chars.create("too many captures"));
				return NULL;
			}
		}
		
		n = disjunction(p, error);
		if (!accept(p, ')'))
		{
			if (!*error)
				*error = io_libecc_Error.syntaxError(ECCNSText.make(p->c, 1), io_libecc_Chars.create("expect ')'"));
			
			return NULL;
		}
		
		switch (modifier) {
			case '\0':
				return join(node(opSave, count * 2, NULL), join(n, node(opSave, count * 2 + 1, NULL)));
				
			case '=':
			{
				int c = nlen(n);
				p->disallowQuantifier = 1;
				return join(node(opLookahead, c + 2, NULL), join(n, node(opMatch, 0, NULL)));
			}
				
			case '!':
			{
				int i = 0, c = nlen(n);
				p->disallowQuantifier = 1;
				for (; i < c; ++i)
					if (n[i].opcode == opSave)
						n[i].opcode = opNSave;
				
				return join(node(opNLookahead, c + 2, NULL), join(n, node(opMatch, 0, NULL)));
			}
			case ':':
				return n;
		}
	}
	else if (accept(p, '.'))
		return node(opAny, 0, NULL);
	else if (accept(p, '['))
	{
		enum Opcode opcode;
		int16_t offset;
		
		int not = accept(p, '^'), length = 0, lastLength, range = -1;
		char buffer[255];
		n = NULL;
		
		while (*p->c != ']' || range >= 0)
		{
			lastLength = length;
			
			if (accept(p, '\\'))
			{
				opcode = escape(p, &offset, buffer + length);
				if (opcode == opBytes)
					length += offset;
				else
				{
					if (not)
						n = join(node(opNLookahead, 3, NULL), join(node(opcode, offset, NULL), join(node(opMatch, 0, NULL), n)));
					else
						n = join(node(opSplit, 3, NULL), join(node(opcode, offset, NULL), join(node(opJump, nlen(n)+2, NULL), n)));
				}
			}
			else
			{
				opcode = opBytes;
				buffer[length++] = *(p->c++);
			}
			
			if (range >= 0)
			{
				if (opcode == opBytes)
				{
					ecctextstring_t text = ECCNSText.make(buffer + range, length - range);
					ecctextchar_t from, to;
					
					from = ECCNSText.nextCharacter(&text);
					ECCNSText.advance(&text, 1);
					to = ECCNSText.nextCharacter(&text);
					
					if (from.codepoint > to.codepoint)
					{
						toss(n);
						*error = io_libecc_Error.syntaxError(ECCNSText.make(p->c - length - range, length - range), io_libecc_Chars.create("range out of order in character class"));
						return NULL;
					}
					
					if (not)
						n = join(node(opNLookahead, 3, NULL),
								 join(node(p->ignoreCase? opInRangeCase: opInRange, length - range, buffer + range),
									  join(node(opMatch, 0, NULL),
										   n)));
					else
						n = join(node(opSplit, 3, NULL),
								 join(node(p->ignoreCase? opInRangeCase: opInRange, length - range, buffer + range),
									  join(node(opJump, nlen(n)+2, NULL),
										   n)));
					
					length = range;
				}
				range = -1;
			}
			
			if (opcode == opBytes && *p->c == '-')
			{
				buffer[length++] = *(p->c++);
				range = lastLength;
			}
			
			if (p->c >= p->end || length >= sizeof(buffer))
			{
				*error = io_libecc_Error.syntaxError(ECCNSText.make(p->c - 1, 1), io_libecc_Chars.create("expect ']'"));
				return NULL;
			}
		}
		
		if (p->ignoreCase)
		{
			char casebuffer[6];
			ecctextstring_t single;
			ecctextchar_t c;
			text = ECCNSText.make(buffer + length, length);
			
			while (text.length)
			{
				c = ECCNSText.prevCharacter(&text);
				single = text;
				single.length = c.units;
				
				offset = ECCNSText.toLower(single, casebuffer) - casebuffer;
				if (memcmp(text.bytes, casebuffer, offset))
				{
					memcpy(buffer + length, casebuffer, offset);
					length += offset;
				}
				
				offset = ECCNSText.toUpper(single, casebuffer) - casebuffer;
				if (memcmp(text.bytes, casebuffer, offset))
				{
					memcpy(buffer + length, casebuffer, offset);
					length += offset;
				}
			}
		}
		
		buffer[length] = '\0';
		accept(p, ']');
		return join(n, node(not? opNeitherOf: opOneOf, length, buffer));
	}
	else if (*p->c && strchr("*+?)}|", *p->c))
		return NULL;
	
	text = ECCNSText.make(p->c, (int32_t)(p->end - p->c));
	text.length = ECCNSText.character(text).units;
	p->c += text.length;
	return characterNode(text, p->ignoreCase);
}

static
struct io_libecc_regexp_Node * alternative (struct Parse *p, struct io_libecc_Error **error)
{
	struct io_libecc_regexp_Node *n = NULL, *t = NULL;
	
	for (;;)
	{
		if (!(t = term(p, error)))
			break;
		
		if (!p->disallowQuantifier)
		{
			int noop = 0, lazy = 0;
			
			uint8_t quantifier =
				accept(p, '?')? '?':
				accept(p, '*')? '*':
				accept(p, '+')? '+':
				accept(p, '{')? '{':
				'\0',
				min = 1,
				max = 1;
			
			switch (quantifier)
			{
				case '?':
					min = 0, max = 1;
					break;
					
				case '*':
					min = 0, max = 0;
					break;
					
				case '+':
					min = 1, max = 0;
					break;
					
				case '{':
				{
					
					if (isdigit(*p->c))
					{
						min = *(p->c++) - '0';
						while (isdigit(*p->c))
							min = min * 10 + *(p->c++) - '0';
					}
					else
					{
						*error = io_libecc_Error.syntaxError(ECCNSText.make(p->c, 1), io_libecc_Chars.create("expect number"));
						goto error;
					}
					
					if (accept(p, ','))
					{
						if (isdigit(*p->c))
						{
							max = *(p->c++) - '0';
							while (isdigit(*p->c))
								max = max * 10 + *(p->c++) - '0';
							
							if (!max)
								noop = 1;
						}
						else
							max = 0;
					}
					else if (!min)
						noop = 1;
					else
						max = min;
					
					if (!accept(p, '}'))
					{
						*error = io_libecc_Error.syntaxError(ECCNSText.make(p->c, 1), io_libecc_Chars.create("expect '}'"));
						goto error;
					}
					break;
				}
			}
			
			lazy = accept(p, '?');
			if (noop)
			{
				toss(t);
				continue;
			}
			
			if (max != 1)
			{
				struct io_libecc_regexp_Node *redo;
				uint16_t index, count = nlen(t) - (lazy? 1: 0), length = 3;
				char buffer[count + length];
				
				for (index = (lazy? 1: 0); index < count; ++index)
					if (t[index].opcode == opSave)
						buffer[length++] = (uint8_t)t[index].offset;
				
				buffer[0] = min;
				buffer[1] = max;
				buffer[2] = lazy;
				buffer[length] = '\0';
				
				redo = node(opRedo, -nlen(t), NULL);
				redo->bytes = malloc(length + 1);
				memcpy(redo->bytes, buffer, length + 1);
				
				t = join(t, redo);
			}
			
			if (min == 0)
			{
				if (lazy)
					t = join(node(opSplit, 2, NULL), join(node(opJump, nlen(t) + 1, NULL), t));
				else
					t = join(node(opSplit, nlen(t) + 1, NULL), t);
			}
		}
		n = join(n, t);
	}
	return n;
	
error:
	toss(t);
	return n;
}

static
struct io_libecc_regexp_Node * disjunction (struct Parse *p, struct io_libecc_Error **error)
{
	struct io_libecc_regexp_Node *n = alternative(p, error), *d;
	if (accept(p, '|'))
	{
		uint16_t len;
		d = disjunction(p, error);
		n = join(n, node(opJump, nlen(d) + 1, NULL));
		len = nlen(n);
		n = join(n, d);
		n = join(node(opSplit, len + 1, NULL), n);
	}
	return n;
}

static
struct io_libecc_regexp_Node * pattern (struct Parse *p, struct io_libecc_Error **error)
{
	assert(*p->c == '/');
	assert(*(p->end - 1) == '/');
	
	++p->c;
	return join(disjunction(p, error), node(opMatch, 0, NULL));
}


//MARK: matching

static
int match (struct io_libecc_regexp_State * const s, struct io_libecc_regexp_Node *n, ecctextstring_t text);

static
void clear (struct io_libecc_regexp_State * const s, const char *c, uint8_t *bytes)
{
	uint8_t index;
	
	if (!bytes)
		return;
	
	while (*bytes)
	{
		index = *bytes++;
		s->index[index] = index % 2? 0: c;
	}
}

static
int forkMatch (struct io_libecc_regexp_State * const s, struct io_libecc_regexp_Node *n, ecctextstring_t text, int16_t offset)
{
	int result;
	
	if (n->depth == 0xff)
		return 0; //!\\ too many recursion
	else
		++n->depth;
	
	result = match(s, n + offset, text);
	--n->depth;
	return result;
}


int match (struct io_libecc_regexp_State * const s, struct io_libecc_regexp_Node *n, ecctextstring_t text)
{
	goto start;
next:
	++n;
start:
	;
	
//	#if DUMP_REGEXP
//		fprintf(stderr, "\n%.*s\n^ ", text.length, text.bytes);
//		printNode(n);
//	#endif
	
	switch((enum Opcode)n->opcode)
	{
		case opNLookahead:
		case opLookahead:
			if (forkMatch(s, n, text, 1) == (n->opcode - 1))
				goto jump;
			
			return 0;
			
		case opStart:
			if (text.bytes != s->start)
				return 0;
			
			goto next;
			
		case opEnd:
			if (text.bytes != s->end)
				return 0;
			
			goto next;
			
		case opLineStart:
		{
			ecctextstring_t prev = ECCNSText.make(text.bytes, (int32_t)(text.bytes - s->start));
			if (text.bytes != s->start && !ECCNSText.isLineFeed(ECCNSText.prevCharacter(&prev)))
				return 0;
			
			goto next;
		}
			
		case opLineEnd:
			if (text.bytes != s->end && !ECCNSText.isLineFeed(ECCNSText.character(text)))
				return 0;
			
			goto next;
			
		case opBoundary:
		{
			ecctextstring_t prev = ECCNSText.make(text.bytes, (int32_t)(text.bytes - s->start));
			if (text.bytes == s->start)
			{
				if (ECCNSText.isWord(ECCNSText.character(text)) != n->offset)
					return 0;
			}
			else if (text.bytes == s->end)
			{
				if (ECCNSText.isWord(ECCNSText.prevCharacter(&prev)) != n->offset)
					return 0;
			}
			else
			{
				if ((ECCNSText.isWord(ECCNSText.prevCharacter(&prev))
					!=
					ECCNSText.isWord(ECCNSText.character(text))) != n->offset
					)
					return 0;
			}
			goto next;
		}
			
		case opSplit:
			if (forkMatch(s, n, text, 1))
				return 1;
			
			goto jump;
			
		case opReference:
		{
			uint16_t len = s->capture[n->offset * 2 + 1]? s->capture[n->offset * 2 + 1] - s->capture[n->offset * 2]: 0;
			
			if (len)
			{
				//#if DUMP_REGEXP
				//				fprintf(stderr, "ref: %.*s", len, s->capture[n->offset * 2]);
				//#endif
				if (text.length < len || memcmp(text.bytes, s->capture[n->offset * 2], len))
					return 0;
				
				ECCNSText.advance(&text, len);
			}
			goto next;
		}
			
		case opRedo:
		{
			int hasmin = n->depth + 1 >= n->bytes[0];
			int lazy = n->bytes[2] && hasmin;
			
			if (n->bytes[1] && n->depth >= n->bytes[1])
				return 0;
			
			if (forkMatch(s, n, text, lazy? 1: n->offset))
			{
				clear(s, text.bytes, (uint8_t *)n->bytes + 3);
				return 1;
			}
			
			if (!hasmin)
				return 0;
			
			if (lazy)
				goto jump;
			else
				goto next;
		}
			
		case opSave:
		{
			const char *capture = s->capture[n->offset];
			s->capture[n->offset] = text.bytes;
			if (forkMatch(s, n, text, 1)) {
				if (s->capture[n->offset] <= s->index[n->offset] && text.bytes == s->capture[n->offset]) {
					s->capture[n->offset] = NULL;
				}
				return 1;
			}
			s->capture[n->offset] = capture;
			return 0;
		}
			
		case opNSave:
		{
			const char *capture = s->capture[n->offset];
			s->capture[n->offset] = text.bytes;
			if (!forkMatch(s, n, text, 1)) {
				s->capture[n->offset] = NULL;
				return 0;
			}
			s->capture[n->offset] = capture;
			return 1;
		}
			
		case opDigit:
			if (text.length < 1 || ECCNSText.isDigit(ECCNSText.nextCharacter(&text)) != n->offset)
				return 0;
			
			goto next;
			
		case opSpace:
			if (text.length < 1 || ECCNSText.isSpace(ECCNSText.nextCharacter(&text)) != n->offset)
				return 0;
			
			goto next;
			
		case opWord:
			if (text.length < 1 || ECCNSText.isWord(ECCNSText.nextCharacter(&text)) != n->offset)
				return 0;
			
			goto next;
			
		case opBytes:
			if (text.length < n->offset || memcmp(n->bytes, text.bytes, n->offset))
				return 0;
			
			ECCNSText.advance(&text, n->offset);
			goto next;
			
		case opOneOf:
		{
			char buffer[5];
			ecctextchar_t c;
			
			if (!text.length)
				return 0;
			
			c = ECCNSText.character(text);
			memcpy(buffer, text.bytes, c.units);
			buffer[c.units] = '\0';
			
			if (n->bytes && strstr(n->bytes, buffer))
			{
				ECCNSText.nextCharacter(&text);
				goto next;
			}
			return 0;
		}
			
		case opNeitherOf:
		{
			char buffer[5];
			ecctextchar_t c;
			
			if (!text.length)
				return 0;
			
			c = ECCNSText.character(text);
			memcpy(buffer, text.bytes, c.units);
			buffer[c.units] = '\0';
			
			if (n->bytes && strstr(n->bytes, buffer))
				return 0;
			
			ECCNSText.nextCharacter(&text);
			goto next;
		}
			
		case opInRange:
		case opInRangeCase:
		{
			ecctextstring_t range = ECCNSText.make(n->bytes, n->offset);
			ecctextchar_t from, to, c;
			
			if (!text.length)
				return 0;
			
			from = ECCNSText.nextCharacter(&range);
			ECCNSText.advance(&range, 1);
			to = ECCNSText.nextCharacter(&range);
			c = ECCNSText.character(text);
			
			if (n->opcode == opInRangeCase)
			{
				char buffer[c.units];
				ecctextstring_t casetext = ECCNSText.make(buffer, 0);
				
				casetext.length = (int32_t)(ECCNSText.toLower(ECCNSText.make(text.bytes, (int32_t)sizeof(buffer)), buffer) - buffer);
				c = ECCNSText.character(casetext);
				if (c.units == casetext.length && (c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
				{
					ECCNSText.nextCharacter(&text);
					goto next;
				}
				
				casetext.length = (int32_t)(ECCNSText.toUpper(ECCNSText.make(text.bytes, (int32_t)sizeof(buffer)), buffer) - buffer);
				c = ECCNSText.character(casetext);
				if (c.units == casetext.length && (c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
				{
					ECCNSText.nextCharacter(&text);
					goto next;
				}
			}
			else
			{
				if ((c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
				{
					ECCNSText.nextCharacter(&text);
					goto next;
				}
			}
			
			return 0;
		}
			
		case opAny:
			if (text.length >= 1 && !ECCNSText.isLineFeed(ECCNSText.nextCharacter(&text)))
				goto next;
			
			return 0;
			
		case opJump:
			goto jump;
			
		case opMatch:
			s->capture[1] = text.bytes;
			return 1;
			
		case opOver:
			break;
	}
	abort();
	
jump:
	n += n->offset;
	goto start;
}


// MARK: - Static Members

static
eccvalue_t constructor (eccstate_t * const context)
{
	eccvalue_t pattern, flags;
	
	pattern = ECCNSContext.argument(context, 0);
	flags = ECCNSContext.argument(context, 1);
	
	return ECCNSValue.regexp(createWith(context, pattern, flags));
}

static
eccvalue_t toString (eccstate_t * const context)
{
	struct io_libecc_RegExp *self = context->this.data.regexp;
	
	ECCNSContext.assertThisType(context, ECC_VALTYPE_REGEXP);
	
	return ECCNSValue.chars(self->pattern);
}

static
eccvalue_t exec (eccstate_t * const context)
{
	struct io_libecc_RegExp *self = context->this.data.regexp;
	eccvalue_t value, lastIndex;
	
	ECCNSContext.assertThisType(context, ECC_VALTYPE_REGEXP);
	
	value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
	lastIndex = self->global? ECCNSValue.toInteger(context, ECCNSObject.getMember(context, &self->object, io_libecc_key_lastIndex)): ECCNSValue.integer(0);
	
	ECCNSObject.putMember(context, &self->object, io_libecc_key_lastIndex, ECCNSValue.integer(0));
	
	if (lastIndex.data.integer >= 0)
	{
		uint16_t length = ECCNSValue.stringLength(&value);
		const char *bytes = ECCNSValue.stringBytes(&value);
		const char *capture[self->count * 2];
		const char *index[self->count * 2];
		struct io_libecc_Chars *element;
		
		struct io_libecc_regexp_State state = {
			io_libecc_String.textAtIndex(bytes, length, lastIndex.data.integer, 0).bytes,
			bytes + length,
			capture,
			index
		};
		
		if (state.start >= bytes && state.start <= state.end && matchWithState(self, &state))
		{
			eccobject_t *array = io_libecc_Array.createSized(self->count);
			int32_t index, count;
			
			for (index = 0, count = self->count; index < count; ++index)
			{
				if (capture[index * 2])
				{
					element = io_libecc_Chars.createWithBytes((int32_t)(capture[index * 2 + 1] - capture[index * 2]), capture[index * 2]);
					array->element[index].value = ECCNSValue.chars(element);
				}
				else
					array->element[index].value = ECCValConstUndefined;
			}
			
			if (self->global)
				ECCNSObject.putMember(context, &self->object, io_libecc_key_lastIndex, ECCNSValue.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(capture[1] - bytes))));
			
			ECCNSObject.addMember(array, io_libecc_key_index, ECCNSValue.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(capture[0] - bytes))), 0);
			ECCNSObject.addMember(array, io_libecc_key_input, value, 0);
			
			return ECCNSValue.object(array);
		}
	}
	return ECCValConstNull;
}

static
eccvalue_t test (eccstate_t * const context)
{
	struct io_libecc_RegExp *self = context->this.data.regexp;
	eccvalue_t value, lastIndex;
	
	ECCNSContext.assertThisType(context, ECC_VALTYPE_REGEXP);
	
	value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
	lastIndex = ECCNSValue.toInteger(context, ECCNSObject.getMember(context, &self->object, io_libecc_key_lastIndex));
	
	ECCNSObject.putMember(context, &self->object, io_libecc_key_lastIndex, ECCNSValue.integer(0));
	
	if (lastIndex.data.integer >= 0)
	{
		uint16_t length = ECCNSValue.stringLength(&value);
		const char *bytes = ECCNSValue.stringBytes(&value);
		const char *capture[self->count * 2];
		const char *index[self->count * 2];
		
		struct io_libecc_regexp_State state = {
			io_libecc_String.textAtIndex(bytes, length, lastIndex.data.integer, 0).bytes,
			bytes + length,
			capture,
			index
		};
		
		if (state.start >= bytes && state.start <= state.end && matchWithState(self, &state))
		{
			if (self->global)
				ECCNSObject.putMember(context, &self->object, io_libecc_key_lastIndex, ECCNSValue.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(capture[1] - bytes))));
			
			return ECCValConstTrue;
		}
	}
	return ECCValConstFalse;
}

// MARK: - Methods

void setup ()
{
	struct io_libecc_Error *error = NULL;
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_regexp_constructor, constructor, 2,
		&io_libecc_regexp_prototype, ECCNSValue.regexp(create(io_libecc_Chars.create("/(?:)/"), &error, 0)),
		&io_libecc_regexp_type);
	
	assert(error == NULL);
	
	io_libecc_Function.addToObject(io_libecc_regexp_prototype, "toString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_regexp_prototype, "exec", exec, 1, h);
	io_libecc_Function.addToObject(io_libecc_regexp_prototype, "test", test, 1, h);
}

void teardown (void)
{
	io_libecc_regexp_prototype = NULL;
	io_libecc_regexp_constructor = NULL;
}

struct io_libecc_RegExp * create (struct io_libecc_Chars *s, struct io_libecc_Error **error, enum io_libecc_regexp_Options options)
{
	struct Parse p = { 0 };
	
	struct io_libecc_RegExp *self = malloc(sizeof(*self));
	*self = io_libecc_RegExp.identity;
	io_libecc_Pool.addObject(&self->object);
	
	ECCNSObject.initialize(&self->object, io_libecc_regexp_prototype);
	
	p.c = s->bytes;
	p.end = s->bytes + s->length;
	while (p.end > p.c && *(p.end - 1) != '/')
	{
		switch (*(--p.end)) {
			case 'i':
				p.ignoreCase = 1;
				continue;
				
			case 'm':
				p.multiline = 1;
				continue;
		}
	}
	
#if DUMP_REGEXP
	fprintf(stderr, "\n%.*s\n", s->length, s->bytes);
#endif
	
	self->pattern = s;
	self->program = pattern(&p, error);
	self->count = p.count + 1;
	self->source = io_libecc_Chars.createWithBytes((int32_t)(p.c - self->pattern->bytes - 1), self->pattern->bytes + 1);
	
//	++self->pattern->referenceCount;
//	++self->source->referenceCount;
	
	if (error && *p.c == '/')
		for (;;)
		{
			switch (*(++p.c))
			{
				case 'g': g:
					if (self->global == 1)
						*error = io_libecc_Error.syntaxError(ECCNSText.make(p.c, 1), io_libecc_Chars.create("invalid flag"));
					
					self->global = 1;
					continue;
					
				case 'i': i:
					if (self->ignoreCase == 1)
						*error = io_libecc_Error.syntaxError(ECCNSText.make(p.c, 1), io_libecc_Chars.create("invalid flag"));
					
					self->ignoreCase = 1;
					continue;
					
				case 'm': m:
					if (self->multiline == 1)
						*error = io_libecc_Error.syntaxError(ECCNSText.make(p.c, 1), io_libecc_Chars.create("invalid flag"));
					
					self->multiline = 1;
					continue;
					
				case '\0':
					break;
					
				case '\\':
					if (options & io_libecc_regexp_allowUnicodeFlags)
					{
						if (!memcmp(p.c + 1, "u0067", 5))
						{
							p.c += 5;
							goto g;
						}
						else if (!memcmp(p.c + 1, "u0069", 5))
						{
							p.c += 5;
							goto i;
						}
						else if (!memcmp(p.c + 1, "u006d", 5) || !memcmp(p.c + 1, "u006D", 5))
						{
							p.c += 5;
							goto m;
						}
					}
					
				default:
					*error = io_libecc_Error.syntaxError(ECCNSText.make(p.c, 1), io_libecc_Chars.create("invalid flag"));
			}
			break;
		}
	else if (error && !*error)
		*error = io_libecc_Error.syntaxError(ECCNSText.make(p.c, 1), io_libecc_Chars.create("invalid character '%c'", isgraph(*p.c)? *p.c: '?'));
	
	ECCNSObject.addMember(&self->object, io_libecc_key_source, ECCNSValue.chars(self->source), io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	ECCNSObject.addMember(&self->object, io_libecc_key_global, ECCNSValue.truth(self->global), io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	ECCNSObject.addMember(&self->object, io_libecc_key_ignoreCase, ECCNSValue.truth(self->ignoreCase), io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	ECCNSObject.addMember(&self->object, io_libecc_key_multiline, ECCNSValue.truth(self->multiline), io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	ECCNSObject.addMember(&self->object, io_libecc_key_lastIndex, ECCNSValue.integer(0), io_libecc_value_hidden | io_libecc_value_sealed);
	
	return self;
}

struct io_libecc_RegExp * createWith (eccstate_t *context, eccvalue_t pattern, eccvalue_t flags)
{
	struct io_libecc_Error *error = NULL;
	struct io_libecc_chars_Append chars;
	struct io_libecc_RegExp *regexp;
	eccvalue_t value;
	
	if (pattern.type == ECC_VALTYPE_REGEXP && flags.type == ECC_VALTYPE_UNDEFINED)
	{
		if (context->construct)
			value = ECCNSValue.chars(pattern.data.regexp->pattern);
		else
			return pattern.data.regexp;
	}
	else
	{
		io_libecc_Chars.beginAppend(&chars);
		
		io_libecc_Chars.append(&chars, "/");
		
		if (pattern.type == ECC_VALTYPE_REGEXP)
			io_libecc_Chars.appendValue(&chars, context, ECCNSValue.chars(pattern.data.regexp->source));
		else
		{
			if (pattern.type == ECC_VALTYPE_UNDEFINED || (ECCNSValue.isString(pattern) && !ECCNSValue.stringLength(&pattern)))
			{
				if (!context->ecc->sloppyMode)
					io_libecc_Chars.append(&chars, "(?:)");
			}
			else
				io_libecc_Chars.appendValue(&chars, context, pattern);
		}
		
		io_libecc_Chars.append(&chars, "/");
		
		if (flags.type != ECC_VALTYPE_UNDEFINED)
			io_libecc_Chars.appendValue(&chars, context, flags);
		
		value = io_libecc_Chars.endAppend(&chars);
		if (value.type != ECC_VALTYPE_CHARS)
			value = ECCNSValue.chars(io_libecc_Chars.createWithBytes(ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value)));
	}
	
	assert(value.type == ECC_VALTYPE_CHARS);
	regexp = create(value.data.chars, &error, context->ecc->sloppyMode? io_libecc_regexp_allowUnicodeFlags: 0);
	if (error)
	{
		ECCNSContext.setTextIndex(context, io_libecc_context_noIndex);
		context->ecc->ofLine = 1;
		context->ecc->ofText = ECCNSValue.textOf(&value);
		context->ecc->ofInput = "(io_libecc_RegExp)";
		ECCNSContext.throw(context, ECCNSValue.error(error));
	}
	return regexp;
}

int matchWithState (struct io_libecc_RegExp *self, struct io_libecc_regexp_State *state)
{
	int result = 0;
	uint16_t index, count;
	ecctextstring_t text = ECCNSText.make(state->start, (int32_t)(state->end - state->start));
	
#if DUMP_REGEXP
	struct io_libecc_regexp_Node *n = self->program;
	while (n->opcode != opOver)
		printNode(n++);
#endif
	
	do
	{
		memset(state->capture, 0, sizeof(*state->capture) * (self->count * 2));
		memset(state->index, 0, sizeof(*state->index) * (self->count * 2));
		state->capture[0] = state->index[0] = text.bytes;
		result = match(state, self->program, text);
		ECCNSText.nextCharacter(&text);
	}
	while (!result && text.length);
	
	/* XXX: cleanup */
	
	for (index = 0, count = nlen(self->program); index < count; ++index)
	{
		if (self->program[index].opcode == opSplit || self->program[index].opcode == opReference)
			self->program[index].bytes = NULL;
		
		self->program[index].depth = 0;
	}
	
	return result;
}
