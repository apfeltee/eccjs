//
//  lexer.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "lexer.h"

#include "pool.h"

// MARK: - Private

static const struct {
	const char *name;
	size_t length;
	enum io_libecc_lexer_Token token;
} keywords[] = {
	#define _(X) { #X, sizeof(#X) - 1, io_libecc_lexer_##X##Token },
	_(break)
	_(case)
	_(catch)
	_(continue)
	_(debugger)
	_(default)
	_(delete)
	_(do)
	_(else)
	_(finally)
	_(for)
	_(function)
	_(if)
	_(in)
	_(instanceof)
	_(new)
	_(return)
	_(switch)
	_(typeof)
	_(throw)
	_(try)
	_(var)
	_(void)
	_(while)
	_(with)
	
	_(void)
	_(typeof)
	
	_(null)
	_(true)
	_(false)
	_(this)
	#undef _
};

static const struct {
	const char *name;
	size_t length;
} reservedKeywords[] = {
	#define _(X) { #X, sizeof(#X) - 1 },
	_(class)
	_(enum)
	_(extends)
	_(super)
	_(const)
	_(export)
	_(import)
	_(implements)
	_(let)
	_(private)
	_(public)
	_(interface)
	_(package)
	_(protected)
	_(static)
	_(yield)
	#undef _
};

// MARK: - Static Members

static struct io_libecc_Lexer* createWithInput(struct io_libecc_Input*);
static void destroy(struct io_libecc_Lexer*);
static enum io_libecc_lexer_Token nextToken(struct io_libecc_Lexer*);
static const char* tokenChars(enum io_libecc_lexer_Token token, char buffer[4]);
static struct io_libecc_Value scanBinary(struct io_libecc_Text text, enum io_libecc_lexer_ScanFlags);
static struct io_libecc_Value scanInteger(struct io_libecc_Text text, int base, enum io_libecc_lexer_ScanFlags);
static uint32_t scanElement(struct io_libecc_Text text);
static uint8_t uint8Hex(char a, char b);
static uint16_t uint16Hex(char a, char b, char c, char d);
const struct type_io_libecc_Lexer io_libecc_Lexer = {
    createWithInput, destroy, nextToken, tokenChars, scanBinary, scanInteger, scanElement, uint8Hex, uint16Hex,
};

static
void addLine(struct io_libecc_Lexer *self, uint32_t offset)
{
	if (self->input->lineCount + 1 >= self->input->lineCapacity)
	{
		self->input->lineCapacity *= 2;
		self->input->lines = realloc(self->input->lines, sizeof(*self->input->lines) * self->input->lineCapacity);
	}
	self->input->lines[++self->input->lineCount] = offset;
}

static
unsigned char previewChar(struct io_libecc_Lexer *self)
{
	if (self->offset < self->input->length)
		return self->input->bytes[self->offset];
	else
		return 0;
}

static
uint32_t nextChar(struct io_libecc_Lexer *self)
{
	if (self->offset < self->input->length)
	{
		struct io_libecc_text_Char c = io_libecc_Text.character(io_libecc_Text.make(self->input->bytes + self->offset, self->input->length - self->offset));
		
		self->offset += c.units;
		
		if ((self->allowUnicodeOutsideLiteral && io_libecc_Text.isLineFeed(c))
			|| (c.codepoint == '\r' && previewChar(self) != '\n')
			|| c.codepoint == '\n'
			)
		{
			self->didLineBreak = 1;
			addLine(self, self->offset);
			c.codepoint = '\n';
		}
		else if (self->allowUnicodeOutsideLiteral && io_libecc_Text.isSpace(c))
			c.codepoint = ' ';
		
		self->text.length += c.units;
		return c.codepoint;
	}
	else
		return 0;
}

static
int acceptChar(struct io_libecc_Lexer *self, char c)
{
	if (previewChar(self) == c)
	{
		nextChar(self);
		return 1;
	}
	else
		return 0;
}

static
char eof(struct io_libecc_Lexer *self)
{
	return self->offset >= self->input->length;
}

static
enum io_libecc_lexer_Token syntaxError(struct io_libecc_Lexer *self, struct io_libecc_Chars *message)
{
	struct io_libecc_Error *error = io_libecc_Error.syntaxError(self->text, message);
	self->value = io_libecc_Value.error(error);
	return io_libecc_lexer_errorToken;
}

// MARK: - Methods

struct io_libecc_Lexer * createWithInput(struct io_libecc_Input *input)
{
	struct io_libecc_Lexer *self = malloc(sizeof(*self));
	*self = io_libecc_Lexer.identity;
	
	assert(input);
	self->input = input;
	
	return self;
}

void destroy (struct io_libecc_Lexer *self)
{
	assert(self);
	
	self->input = NULL;
	free(self), self = NULL;
}

enum io_libecc_lexer_Token nextToken (struct io_libecc_Lexer *self)
{
	uint32_t c;
	assert(self);
	
	self->value = io_libecc_value_undefined;
	self->didLineBreak = 0;
	
	retry:
	self->text.bytes = self->input->bytes + self->offset;
	self->text.length = 0;
	
	while (( c = nextChar(self) ))
	{
		switch (c)
		{
			case '\n':
			case '\r':
			case '\t':
			case '\v':
			case '\f':
			case ' ':
				goto retry;
			
			case '/':
			{
				if (acceptChar(self, '*'))
				{
					while (!eof(self))
						if (nextChar(self) == '*' && acceptChar(self, '/'))
							goto retry;
					
					return syntaxError(self, io_libecc_Chars.create("unterminated comment"));
				}
				else if (previewChar(self) == '/')
				{
					while (( c = nextChar(self) ))
						if (c == '\r' || c == '\n')
							goto retry;
					
					return 0;
				}
				else if (self->allowRegex)
				{
					while (!eof(self))
					{
						int c = nextChar(self);
						
						if (c == '\\')
							c = nextChar(self);
						else if (c == '/')
						{
							while (isalnum(previewChar(self)) || previewChar(self) == '\\')
								nextChar(self);
							
							return io_libecc_lexer_regexpToken;
						}
						
						if (c == '\n')
							break;
					}
					return syntaxError(self, io_libecc_Chars.create("unterminated regexp literal"));
				}
				else if (acceptChar(self, '='))
					return io_libecc_lexer_divideAssignToken;
				else
					return '/';
			}
			
			case '\'':
			case '"':
			{
				char end = c;
				int haveEscape = 0;
				int didLineBreak = self->didLineBreak;
				
				while (( c = nextChar(self) ))
				{
					if (c == '\\')
					{
						haveEscape = 1;
						nextChar(self);
						self->didLineBreak = didLineBreak;
					}
					else if (c == end)
					{
						const char *bytes = self->text.bytes++;
						uint32_t length = self->text.length -= 2;
						
						if (haveEscape)
						{
							struct io_libecc_chars_Append chars;
							uint32_t index;
							
							++bytes;
							--length;
							
							io_libecc_Chars.beginAppend(&chars);
							
							for (index = 0; index <= length; ++index)
								if (bytes[index] == '\\' && bytes[++index] != '\\')
								{
									switch (bytes[index])
									{
										case '0': io_libecc_Chars.appendCodepoint(&chars, '\0'); break;
										case 'b': io_libecc_Chars.appendCodepoint(&chars, '\b'); break;
										case 'f': io_libecc_Chars.appendCodepoint(&chars, '\f'); break;
										case 'n': io_libecc_Chars.appendCodepoint(&chars, '\n'); break;
										case 'r': io_libecc_Chars.appendCodepoint(&chars, '\r'); break;
										case 't': io_libecc_Chars.appendCodepoint(&chars, '\t'); break;
										case 'v': io_libecc_Chars.appendCodepoint(&chars, '\v'); break;
										case 'x':
											if (isxdigit(bytes[index + 1]) && isxdigit(bytes[index + 2]))
											{
												io_libecc_Chars.appendCodepoint(&chars, uint8Hex(bytes[index + 1], bytes[index + 2]));
												index += 2;
												break;
											}
											self->text = io_libecc_Text.make(self->text.bytes + index - 1, 4);
											return syntaxError(self, io_libecc_Chars.create("malformed hexadecimal character escape sequence"));
										
										case 'u':
											if (isxdigit(bytes[index + 1]) && isxdigit(bytes[index + 2]) && isxdigit(bytes[index + 3]) && isxdigit(bytes[index + 4]))
											{
												io_libecc_Chars.appendCodepoint(&chars, uint16Hex(bytes[index+ 1], bytes[index + 2], bytes[index + 3], bytes[index + 4]));
												index += 4;
												break;
											}
											self->text = io_libecc_Text.make(self->text.bytes + index - 1, 6);
											return syntaxError(self, io_libecc_Chars.create("malformed Unicode character escape sequence"));
											
										case '\r':
											if (bytes[index + 1] == '\n')
												++index;
										case '\n':
											continue;
											
										default:
											io_libecc_Chars.append(&chars, "%c", bytes[index]);
									}
								}
								else
									io_libecc_Chars.append(&chars, "%c", bytes[index]);
							
							self->value = io_libecc_Input.attachValue(self->input, io_libecc_Chars.endAppend(&chars));
							return io_libecc_lexer_escapedStringToken;
						}
						
						return io_libecc_lexer_stringToken;
					}
					else if (c == '\r' || c == '\n')
						break;
				}
				
				return syntaxError(self, io_libecc_Chars.create("unterminated string literal"));
			}
			
			case '.':
				if (!isdigit(previewChar(self)))
					return c;
				
				/*vvv*/
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				int binary = 0;
				
				if (c == '0' && (acceptChar(self, 'x') || acceptChar(self, 'X')))
				{
					while (( c = previewChar(self) ))
						if (isxdigit(c))
							nextChar(self);
						else
							break;
					
					if (self->text.length <= 2)
						return syntaxError(self, io_libecc_Chars.create("missing hexadecimal digits after '0x'"));
				}
				else
				{
					while (isdigit(previewChar(self)))
						nextChar(self);
					
					if (c == '.' || acceptChar(self, '.'))
						binary = 1;
					
					while (isdigit(previewChar(self)))
						nextChar(self);
					
					if (acceptChar(self, 'e') || acceptChar(self, 'E'))
					{
						binary = 1;
						
						if (!acceptChar(self, '+'))
							acceptChar(self, '-');
						
						if (!isdigit(previewChar(self)))
							return syntaxError(self, io_libecc_Chars.create("missing exponent"));
						
						while (isdigit(previewChar(self)))
							nextChar(self);
					}
				}
				
				if (isalpha(previewChar(self)))
				{
					self->text.bytes += self->text.length;
					self->text.length = 1;
					return syntaxError(self, io_libecc_Chars.create("identifier starts immediately after numeric literal"));
				}
				
				if (binary)
				{
					self->value = scanBinary(self->text, 0);
					return io_libecc_lexer_binaryToken;
				}
				else
				{
					self->value = scanInteger(self->text, 0, 0);
					
					if (self->value.type == io_libecc_value_integerType)
						return io_libecc_lexer_integerToken;
					else
						return io_libecc_lexer_binaryToken;
				}
			}
			
			case '}':
			case ')':
			case ']':
			case '{':
			case '(':
			case '[':
			case ';':
			case ',':
			case '~':
			case '?':
			case ':':
				return c;
			
			case '^':
				if (acceptChar(self, '='))
					return io_libecc_lexer_xorAssignToken;
				else
					return c;
			
			case '%':
				if (acceptChar(self, '='))
					return io_libecc_lexer_moduloAssignToken;
				else
					return c;
			
			case '*':
				if (acceptChar(self, '='))
					return io_libecc_lexer_multiplyAssignToken;
				else
					return c;
			
			case '=':
				if (acceptChar(self, '='))
				{
					if (acceptChar(self, '='))
						return io_libecc_lexer_identicalToken;
					else
						return io_libecc_lexer_equalToken;
				}
				else
					return c;
			
			case '!':
				if (acceptChar(self, '='))
				{
					if (acceptChar(self, '='))
						return io_libecc_lexer_notIdenticalToken;
					else
						return io_libecc_lexer_notEqualToken;
				}
				else
					return c;
			
			case '+':
				if (acceptChar(self, '+'))
					return io_libecc_lexer_incrementToken;
				else if (acceptChar(self, '='))
					return io_libecc_lexer_addAssignToken;
				else
					return c;
			
			case '-':
				if (acceptChar(self, '-'))
					return io_libecc_lexer_decrementToken;
				else if (acceptChar(self, '='))
					return io_libecc_lexer_minusAssignToken;
				else
					return c;
			
			case '&':
				if (acceptChar(self, '&'))
					return io_libecc_lexer_logicalAndToken;
				else if (acceptChar(self, '='))
					return io_libecc_lexer_andAssignToken;
				else
					return c;
			
			case '|':
				if (acceptChar(self, '|'))
					return io_libecc_lexer_logicalOrToken;
				else if (acceptChar(self, '='))
					return io_libecc_lexer_orAssignToken;
				else
					return c;
			
			case '<':
				if (acceptChar(self, '<'))
				{
					if (acceptChar(self, '='))
						return io_libecc_lexer_leftShiftAssignToken;
					else
						return io_libecc_lexer_leftShiftToken;
				}
				else if (acceptChar(self, '='))
					return io_libecc_lexer_lessOrEqualToken;
				else
					return c;
			
			case '>':
				if (acceptChar(self, '>'))
				{
					if (acceptChar(self, '>'))
					{
						if (acceptChar(self, '='))
							return io_libecc_lexer_unsignedRightShiftAssignToken;
						else
							return io_libecc_lexer_unsignedRightShiftToken;
					}
					else if (acceptChar(self, '='))
						return io_libecc_lexer_rightShiftAssignToken;
					else
						return io_libecc_lexer_rightShiftToken;
				}
				else if (acceptChar(self, '='))
					return io_libecc_lexer_moreOrEqualToken;
				else
					return c;
			
			default:
			{
				if ((c < 0x80 && isalpha(c)) || c == '$' || c == '_' || (self->allowUnicodeOutsideLiteral && (c == '\\' || c >= 0x80)))
				{
					struct io_libecc_Text text = self->text;
					int k, haveEscape = 0;
					
					do
					{
						if (c == '\\')
						{
							char
								uu = nextChar(self),
								u1 = nextChar(self),
								u2 = nextChar(self),
								u3 = nextChar(self),
								u4 = nextChar(self);
							
							if (uu == 'u' && isxdigit(u1) && isxdigit(u2) && isxdigit(u3) && isxdigit(u4))
							{
								c = uint16Hex(u1, u2, u3, u4);
								haveEscape = 1;
							}
							else
								return syntaxError(self, io_libecc_Chars.create("incomplete unicode escape"));
						}
						
						if (io_libecc_Text.isSpace((struct io_libecc_text_Char){ c }))
							break;
						
						text = self->text;
						c = nextChar(self);
					}
					while (isalnum(c) || c == '$' || c == '_' || (self->allowUnicodeOutsideLiteral && (c == '\\' || c >= 0x80)));
					
					self->text = text;
					self->offset = (uint32_t)(text.bytes + text.length - self->input->bytes);
					
					if (haveEscape)
					{
						struct io_libecc_Text text = self->text;
						struct io_libecc_chars_Append chars;
						struct io_libecc_Value value;
						
						io_libecc_Chars.beginAppend(&chars);
						
						while (text.length)
						{
							c = io_libecc_Text.nextCharacter(&text).codepoint;
							
							if (c == '\\' && io_libecc_Text.nextCharacter(&text).codepoint == 'u')
							{
								char
									u1 = io_libecc_Text.nextCharacter(&text).codepoint,
									u2 = io_libecc_Text.nextCharacter(&text).codepoint,
									u3 = io_libecc_Text.nextCharacter(&text).codepoint,
									u4 = io_libecc_Text.nextCharacter(&text).codepoint;
								
								if (isxdigit(u1) && isxdigit(u2) && isxdigit(u3) && isxdigit(u4))
									c = uint16Hex(u1, u2, u3, u4);
								else
									io_libecc_Text.advance(&text, -5);
							}
							
							io_libecc_Chars.appendCodepoint(&chars, c);
						}
						
						value = io_libecc_Input.attachValue(self->input, io_libecc_Chars.endAppend(&chars));
						self->value = io_libecc_Value.key(io_libecc_Key.makeWithText(io_libecc_Value.textOf(&value), value.type != io_libecc_value_charsType));
						return io_libecc_lexer_identifierToken;
					}
					
					if (!self->disallowKeyword)
					{
						for (k = 0; k < sizeof(keywords) / sizeof(*keywords); ++k)
							if (self->text.length == keywords[k].length && memcmp(self->text.bytes, keywords[k].name, keywords[k].length) == 0)
								return keywords[k].token;
						
						for (k = 0; k < sizeof(reservedKeywords) / sizeof(*reservedKeywords); ++k)
							if (self->text.length == reservedKeywords[k].length && memcmp(self->text.bytes, reservedKeywords[k].name, reservedKeywords[k].length) == 0)
								return syntaxError(self, io_libecc_Chars.create("'%s' is a reserved identifier", reservedKeywords[k]));
					}
					
					self->value = io_libecc_Value.key(io_libecc_Key.makeWithText(self->text, 0));
					return io_libecc_lexer_identifierToken;
				}
				else
				{
					if (c >= 0x80)
						return syntaxError(self, io_libecc_Chars.create("invalid character '%.*s'", self->text.length, self->text.bytes));
					else if (isprint(c))
						return syntaxError(self, io_libecc_Chars.create("invalid character '%c'", c));
					else
						return syntaxError(self, io_libecc_Chars.create("invalid character '\\%d'", c & 0xff));
				}
			}
		}
	}
	
	addLine(self, self->offset);
	return io_libecc_lexer_noToken;
}

const char * tokenChars (enum io_libecc_lexer_Token token, char buffer[4])
{
	int index;
    struct
    {
        const char* name;
        const enum io_libecc_lexer_Token token;
    } static const tokenList[] =
    {
        { (sizeof("end of script") > sizeof("") ? "end of script" : "no"), io_libecc_lexer_noToken },
        { (sizeof("") > sizeof("") ? "" : "error"), io_libecc_lexer_errorToken },
        { (sizeof("") > sizeof("") ? "" : "null"), io_libecc_lexer_nullToken },
        { (sizeof("") > sizeof("") ? "" : "true"), io_libecc_lexer_trueToken },
        { (sizeof("") > sizeof("") ? "" : "false"), io_libecc_lexer_falseToken },
        { (sizeof("number") > sizeof("") ? "number" : "integer"), io_libecc_lexer_integerToken },
        { (sizeof("number") > sizeof("") ? "number" : "binary"), io_libecc_lexer_binaryToken },
        { (sizeof("") > sizeof("") ? "" : "string"), io_libecc_lexer_stringToken },
        { (sizeof("string") > sizeof("") ? "string" : "escapedString"), io_libecc_lexer_escapedStringToken },
        { (sizeof("") > sizeof("") ? "" : "identifier"), io_libecc_lexer_identifierToken },
        { (sizeof("") > sizeof("") ? "" : "regexp"), io_libecc_lexer_regexpToken },
        { (sizeof("") > sizeof("") ? "" : "break"), io_libecc_lexer_breakToken },
        { (sizeof("") > sizeof("") ? "" : "case"), io_libecc_lexer_caseToken },
        { (sizeof("") > sizeof("") ? "" : "catch"), io_libecc_lexer_catchToken },
        { (sizeof("") > sizeof("") ? "" : "continue"), io_libecc_lexer_continueToken },
        { (sizeof("") > sizeof("") ? "" : "debugger"), io_libecc_lexer_debuggerToken },
        { (sizeof("") > sizeof("") ? "" : "default"), io_libecc_lexer_defaultToken },
        { (sizeof("") > sizeof("") ? "" : "delete"), io_libecc_lexer_deleteToken },
        { (sizeof("") > sizeof("") ? "" : "do"), io_libecc_lexer_doToken },
        { (sizeof("") > sizeof("") ? "" : "else"), io_libecc_lexer_elseToken },
        { (sizeof("") > sizeof("") ? "" : "finally"), io_libecc_lexer_finallyToken },
        { (sizeof("") > sizeof("") ? "" : "for"), io_libecc_lexer_forToken },
        { (sizeof("") > sizeof("") ? "" : "function"), io_libecc_lexer_functionToken },
        { (sizeof("") > sizeof("") ? "" : "if"), io_libecc_lexer_ifToken },
        { (sizeof("") > sizeof("") ? "" : "in"), io_libecc_lexer_inToken },
        { (sizeof("") > sizeof("") ? "" : "instanceof"), io_libecc_lexer_instanceofToken },
        { (sizeof("") > sizeof("") ? "" : "new"), io_libecc_lexer_newToken },
        { (sizeof("") > sizeof("") ? "" : "return"), io_libecc_lexer_returnToken },
        { (sizeof("") > sizeof("") ? "" : "switch"), io_libecc_lexer_switchToken },
        { (sizeof("") > sizeof("") ? "" : "this"), io_libecc_lexer_thisToken },
        { (sizeof("") > sizeof("") ? "" : "throw"), io_libecc_lexer_throwToken },
        { (sizeof("") > sizeof("") ? "" : "try"), io_libecc_lexer_tryToken },
        { (sizeof("") > sizeof("") ? "" : "typeof"), io_libecc_lexer_typeofToken },
        { (sizeof("") > sizeof("") ? "" : "var"), io_libecc_lexer_varToken },
        { (sizeof("") > sizeof("") ? "" : "void"), io_libecc_lexer_voidToken },
        { (sizeof("") > sizeof("") ? "" : "with"), io_libecc_lexer_withToken },
        { (sizeof("") > sizeof("") ? "" : "while"), io_libecc_lexer_whileToken },
        { (sizeof("'=='") > sizeof("") ? "'=='" : "equal"), io_libecc_lexer_equalToken },
        { (sizeof("'!='") > sizeof("") ? "'!='" : "notEqual"), io_libecc_lexer_notEqualToken },
        { (sizeof("'==='") > sizeof("") ? "'==='" : "identical"), io_libecc_lexer_identicalToken },
        { (sizeof("'!=='") > sizeof("") ? "'!=='" : "notIdentical"), io_libecc_lexer_notIdenticalToken },
        { (sizeof("'<<='") > sizeof("") ? "'<<='" : "leftShiftAssign"), io_libecc_lexer_leftShiftAssignToken },
        { (sizeof("'>>='") > sizeof("") ? "'>>='" : "rightShiftAssign"), io_libecc_lexer_rightShiftAssignToken },
        { (sizeof("'>>>='") > sizeof("") ? "'>>>='" : "unsignedRightShiftAssign"), io_libecc_lexer_unsignedRightShiftAssignToken },
        { (sizeof("'<<'") > sizeof("") ? "'<<'" : "leftShift"), io_libecc_lexer_leftShiftToken },
        { (sizeof("'>>'") > sizeof("") ? "'>>'" : "rightShift"), io_libecc_lexer_rightShiftToken },
        { (sizeof("'>>>'") > sizeof("") ? "'>>>'" : "unsignedRightShift"), io_libecc_lexer_unsignedRightShiftToken },
        { (sizeof("'<='") > sizeof("") ? "'<='" : "lessOrEqual"), io_libecc_lexer_lessOrEqualToken },
        { (sizeof("'>='") > sizeof("") ? "'>='" : "moreOrEqual"), io_libecc_lexer_moreOrEqualToken },
        { (sizeof("'++'") > sizeof("") ? "'++'" : "increment"), io_libecc_lexer_incrementToken },
        { (sizeof("'--'") > sizeof("") ? "'--'" : "decrement"), io_libecc_lexer_decrementToken },
        { (sizeof("'&&'") > sizeof("") ? "'&&'" : "logicalAnd"), io_libecc_lexer_logicalAndToken },
        { (sizeof("'||'") > sizeof("") ? "'||'" : "logicalOr"), io_libecc_lexer_logicalOrToken },
        { (sizeof("'+='") > sizeof("") ? "'+='" : "addAssign"), io_libecc_lexer_addAssignToken },
        { (sizeof("'-='") > sizeof("") ? "'-='" : "minusAssign"), io_libecc_lexer_minusAssignToken },
        { (sizeof("'*='") > sizeof("") ? "'*='" : "multiplyAssign"), io_libecc_lexer_multiplyAssignToken },
        { (sizeof("'/='") > sizeof("") ? "'/='" : "divideAssign"), io_libecc_lexer_divideAssignToken },
        { (sizeof("'%='") > sizeof("") ? "'%='" : "moduloAssign"), io_libecc_lexer_moduloAssignToken },
        { (sizeof("'&='") > sizeof("") ? "'&='" : "andAssign"), io_libecc_lexer_andAssignToken },
        { (sizeof("'|='") > sizeof("") ? "'|='" : "orAssign"), io_libecc_lexer_orAssignToken },
        { (sizeof("'^='") > sizeof("") ? "'^='" : "xorAssign"), io_libecc_lexer_xorAssignToken },
    };
        

	if (token > io_libecc_lexer_noToken && token < io_libecc_lexer_errorToken)
	{
		buffer[0] = '\'';
		buffer[1] = token;
		buffer[2] = '\'';
		buffer[3] = '\0';
		return buffer;
	}
	
	for (index = 0; index < sizeof(tokenList); ++index)
		if (tokenList[index].token == token)
			return tokenList[index].name;
	
	assert(0);
	return "unknow";
}

struct io_libecc_Value scanBinary (struct io_libecc_Text text, enum io_libecc_lexer_ScanFlags flags)
{
	int lazy = flags & io_libecc_lexer_scanLazy;
	char buffer[text.length + 1];
	char *end = buffer;
	double binary = NAN;
	
	if (flags & io_libecc_lexer_scanSloppy)
	{
		struct io_libecc_Text tail = io_libecc_Text.make(text.bytes + text.length, text.length);
		
		while (tail.length && io_libecc_Text.isSpace(io_libecc_Text.prevCharacter(&tail)))
			text.length = tail.length;
		
		while (text.length && io_libecc_Text.isSpace(io_libecc_Text.character(text)))
			io_libecc_Text.nextCharacter(&text);
	}
	else
		while (text.length && isspace(*text.bytes))
			io_libecc_Text.advance(&text, 1);
	
	memcpy(buffer, text.bytes, text.length);
	buffer[text.length] = '\0';
	
	if (text.length)
	{
		if (lazy && text.length >= 2 && buffer[0] == '0' && buffer[1] == 'x')
				return io_libecc_Value.binary(0);
		
		if (text.length >= io_libecc_text_infinity.length && !memcmp(buffer, io_libecc_text_infinity.bytes, io_libecc_text_infinity.length))
			binary = INFINITY, end += io_libecc_text_infinity.length;
		else if (text.length >= io_libecc_text_negativeInfinity.length && !memcmp(buffer, io_libecc_text_negativeInfinity.bytes, io_libecc_text_negativeInfinity.length))
			binary = -INFINITY, end += io_libecc_text_negativeInfinity.length;
		else if (!isalpha(buffer[0]))
			binary = strtod(buffer, &end);
		
		if ((!lazy && *end && !isspace(*end)) || (lazy && end == buffer))
			binary = NAN;
	}
	else if (!lazy)
		binary = 0;
	
	return io_libecc_Value.binary(binary);
}

static double strtolHexFallback (struct io_libecc_Text text)
{
	double binary = 0;
	int sign = 1;
	int offset = 0;
	int c;
	
	if (text.bytes[offset] == '-')
		sign = -1, ++offset;
	
	if (text.length - offset > 1 && text.bytes[offset] == '0' && tolower(text.bytes[offset + 1]) == 'x')
	{
		offset += 2;
		
		while (text.length - offset >= 1)
		{
			c = text.bytes[offset++];
			
			binary *= 16;
			
			if (isdigit(c))
				binary += c - '0';
			else if (isxdigit(c))
				binary += tolower(c) - ('a' - 10);
			else
				return NAN;
		}
	}
	
	return binary * sign;
}

struct io_libecc_Value scanInteger (struct io_libecc_Text text, int base, enum io_libecc_lexer_ScanFlags flags)
{
	int lazy = flags & io_libecc_lexer_scanLazy;
	long integer;
	char buffer[text.length + 1];
	char *end;
	
	if (flags & io_libecc_lexer_scanSloppy)
	{
		struct io_libecc_Text tail = io_libecc_Text.make(text.bytes + text.length, text.length);
		
		while (tail.length && io_libecc_Text.isSpace(io_libecc_Text.prevCharacter(&tail)))
			text.length = tail.length;
		
		while (text.length && io_libecc_Text.isSpace(io_libecc_Text.character(text)))
			io_libecc_Text.nextCharacter(&text);
	}
	else
		while (text.length && isspace(*text.bytes))
			io_libecc_Text.advance(&text, 1);
	
	memcpy(buffer, text.bytes, text.length);
	buffer[text.length] = '\0';
	
	errno = 0;
	integer = strtol(buffer, &end, base);
	
	if ((lazy && end == buffer) || (!lazy && *end && !isspace(*end)))
		return io_libecc_Value.binary(NAN);
	else if (errno == ERANGE)
	{
		if (!base || base == 10 || base == 16)
		{
			double binary = strtod(buffer, NULL);
			if (!binary && (!base || base == 16))
				binary = strtolHexFallback(text);
			
			return io_libecc_Value.binary(binary);
		}
		
		io_libecc_Env.printWarning("`parseInt('%.*s', %d)` out of bounds; only long int are supported by radices other than 10 or 16", text.length, text.bytes, base);
		return io_libecc_Value.binary(NAN);
	}
	else if (integer < INT32_MIN || integer > INT32_MAX)
		return io_libecc_Value.binary(integer);
	else
		return io_libecc_Value.integer((int32_t)integer);
}

uint32_t scanElement (struct io_libecc_Text text)
{
	struct io_libecc_Value value;
	uint16_t index;
	
	if (!text.length)
		return UINT32_MAX;
	
	for (index = 0; index < text.length; ++index)
		if (!isdigit(text.bytes[index]))
			return UINT32_MAX;
	
	value = scanInteger(text, 0, 0);
	
	if (value.type == io_libecc_value_integerType)
		return value.data.integer;
	if (value.type == io_libecc_value_binaryType && value.data.binary >= 0 && value.data.binary < UINT32_MAX && value.data.binary == (uint32_t)value.data.binary)
		return value.data.binary;
	else
		return UINT32_MAX;
}

static inline int8_t hexhigit(int c)
{
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else
		return c - '0';
}

uint8_t uint8Hex(char a, char b)
{
	return hexhigit(a) << 4 | hexhigit(b);
}

uint16_t uint16Hex(char a, char b, char c, char d)
{
	return hexhigit(a) << 12 | hexhigit(b) << 8 | hexhigit(c) << 4 | hexhigit(d);
}
