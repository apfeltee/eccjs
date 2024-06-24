//
//  lexer.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"


// MARK: - Private
#define eccmac_stringandlen(str) str, sizeof(str)-1

static const struct {
 const char *name;
 size_t length;
 eccasttoktype_t token;
} keywords[] = {
 { eccmac_stringandlen("break"), io_libecc_lexer_breakToken },
 { eccmac_stringandlen("case"), io_libecc_lexer_caseToken },
 { eccmac_stringandlen("catch"), io_libecc_lexer_catchToken },
 { eccmac_stringandlen("continue"), io_libecc_lexer_continueToken },
 { eccmac_stringandlen("debugger"), io_libecc_lexer_debuggerToken },
 { eccmac_stringandlen("default"), io_libecc_lexer_defaultToken },
 { eccmac_stringandlen("delete"), io_libecc_lexer_deleteToken },
 { eccmac_stringandlen("do"), io_libecc_lexer_doToken },
 { eccmac_stringandlen("else"), io_libecc_lexer_elseToken },
 { eccmac_stringandlen("finally"), io_libecc_lexer_finallyToken },
 { eccmac_stringandlen("for"), io_libecc_lexer_forToken },
 { eccmac_stringandlen("function"), io_libecc_lexer_functionToken },
 { eccmac_stringandlen("if"), io_libecc_lexer_ifToken },
 { eccmac_stringandlen("in"), io_libecc_lexer_inToken },
 { eccmac_stringandlen("instanceof"), io_libecc_lexer_instanceofToken },
 { eccmac_stringandlen("new"), io_libecc_lexer_newToken },
 { eccmac_stringandlen("return"), io_libecc_lexer_returnToken },
 { eccmac_stringandlen("switch"), io_libecc_lexer_switchToken },
 { eccmac_stringandlen("typeof"), io_libecc_lexer_typeofToken },
 { eccmac_stringandlen("throw"), io_libecc_lexer_throwToken },
 { eccmac_stringandlen("try"), io_libecc_lexer_tryToken },
 { eccmac_stringandlen("var"), io_libecc_lexer_varToken },
 { eccmac_stringandlen("void"), io_libecc_lexer_voidToken },
 { eccmac_stringandlen("while"), io_libecc_lexer_whileToken },
 { eccmac_stringandlen("with"), io_libecc_lexer_withToken },
 { eccmac_stringandlen("void"), io_libecc_lexer_voidToken },
 { eccmac_stringandlen("typeof"), io_libecc_lexer_typeofToken },
 { eccmac_stringandlen("null"), io_libecc_lexer_nullToken },
 { eccmac_stringandlen("true"), io_libecc_lexer_trueToken },
 { eccmac_stringandlen("false"), io_libecc_lexer_falseToken },
 { eccmac_stringandlen("this"), io_libecc_lexer_thisToken },
};


static const struct {
 const char *name;
 size_t length;
} reservedKeywords[] = {
 { eccmac_stringandlen("class") },
 { eccmac_stringandlen("enum") },
 { eccmac_stringandlen("extends") },
 { eccmac_stringandlen("super") },
 { eccmac_stringandlen("const") },
 { eccmac_stringandlen("export") },
 { eccmac_stringandlen("import") },
 { eccmac_stringandlen("implements") },
 { eccmac_stringandlen("let") },
 { eccmac_stringandlen("private") },
 { eccmac_stringandlen("public") },
 { eccmac_stringandlen("interface") },
 { eccmac_stringandlen("package") },
 { eccmac_stringandlen("protected") },
 { eccmac_stringandlen("static") },
 { eccmac_stringandlen("yield") },
};


// MARK: - Static Members

static eccastlexer_t* createWithInput(eccioinput_t*);
static void destroy(eccastlexer_t*);
static eccasttoktype_t nextToken(eccastlexer_t*);
static const char* tokenChars(eccasttoktype_t token, char buffer[4]);
static eccvalue_t scanBinary(ecctextstring_t text, enum io_libecc_lexer_ScanFlags);
static eccvalue_t scanInteger(ecctextstring_t text, int base, enum io_libecc_lexer_ScanFlags);
static uint32_t scanElement(ecctextstring_t text);
static uint8_t uint8Hex(char a, char b);
static uint16_t uint16Hex(char a, char b, char c, char d);
const struct type_io_libecc_Lexer io_libecc_Lexer = {
    createWithInput, destroy, nextToken, tokenChars, scanBinary, scanInteger, scanElement, uint8Hex, uint16Hex,
};

static
void addLine(eccastlexer_t *self, uint32_t offset)
{
	if (self->input->lineCount + 1 >= self->input->lineCapacity)
	{
		self->input->lineCapacity *= 2;
		self->input->lines = realloc(self->input->lines, sizeof(*self->input->lines) * self->input->lineCapacity);
	}
	self->input->lines[++self->input->lineCount] = offset;
}

static
unsigned char previewChar(eccastlexer_t *self)
{
	if (self->offset < self->input->length)
		return self->input->bytes[self->offset];
	else
		return 0;
}

static
uint32_t nextChar(eccastlexer_t *self)
{
	if (self->offset < self->input->length)
	{
		ecctextchar_t c = ECCNSText.character(ECCNSText.make(self->input->bytes + self->offset, self->input->length - self->offset));
		
		self->offset += c.units;
		
		if ((self->allowUnicodeOutsideLiteral && ECCNSText.isLineFeed(c))
			|| (c.codepoint == '\r' && previewChar(self) != '\n')
			|| c.codepoint == '\n'
			)
		{
			self->didLineBreak = 1;
			addLine(self, self->offset);
			c.codepoint = '\n';
		}
		else if (self->allowUnicodeOutsideLiteral && ECCNSText.isSpace(c))
			c.codepoint = ' ';
		
		self->text.length += c.units;
		return c.codepoint;
	}
	else
		return 0;
}

static
int acceptChar(eccastlexer_t *self, char c)
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
char eof(eccastlexer_t *self)
{
	return self->offset >= self->input->length;
}

static
eccasttoktype_t syntaxError(eccastlexer_t *self, struct io_libecc_Chars *message)
{
	struct io_libecc_Error *error = io_libecc_Error.syntaxError(self->text, message);
	self->value = ECCNSValue.error(error);
	return io_libecc_lexer_errorToken;
}

// MARK: - Methods

eccastlexer_t * createWithInput(eccioinput_t *input)
{
	eccastlexer_t *self = malloc(sizeof(*self));
	*self = io_libecc_Lexer.identity;
	
	assert(input);
	self->input = input;
	
	return self;
}

void destroy (eccastlexer_t *self)
{
	assert(self);
	
	self->input = NULL;
	free(self), self = NULL;
}

eccasttoktype_t nextToken (eccastlexer_t *self)
{
	uint32_t c;
	assert(self);
	
	self->value = ECCValConstUndefined;
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
											self->text = ECCNSText.make(self->text.bytes + index - 1, 4);
											return syntaxError(self, io_libecc_Chars.create("malformed hexadecimal character escape sequence"));
										
										case 'u':
											if (isxdigit(bytes[index + 1]) && isxdigit(bytes[index + 2]) && isxdigit(bytes[index + 3]) && isxdigit(bytes[index + 4]))
											{
												io_libecc_Chars.appendCodepoint(&chars, uint16Hex(bytes[index+ 1], bytes[index + 2], bytes[index + 3], bytes[index + 4]));
												index += 4;
												break;
											}
											self->text = ECCNSText.make(self->text.bytes + index - 1, 6);
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
					
					if (self->value.type == ECC_VALTYPE_INTEGER)
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
					ecctextstring_t text = self->text;
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
						
						if (ECCNSText.isSpace((ecctextchar_t){ c }))
							break;
						
						text = self->text;
						c = nextChar(self);
					}
					while (isalnum(c) || c == '$' || c == '_' || (self->allowUnicodeOutsideLiteral && (c == '\\' || c >= 0x80)));
					
					self->text = text;
					self->offset = (uint32_t)(text.bytes + text.length - self->input->bytes);
					
					if (haveEscape)
					{
						ecctextstring_t text = self->text;
						struct io_libecc_chars_Append chars;
						eccvalue_t value;
						
						io_libecc_Chars.beginAppend(&chars);
						
						while (text.length)
						{
							c = ECCNSText.nextCharacter(&text).codepoint;
							
							if (c == '\\' && ECCNSText.nextCharacter(&text).codepoint == 'u')
							{
								char
									u1 = ECCNSText.nextCharacter(&text).codepoint,
									u2 = ECCNSText.nextCharacter(&text).codepoint,
									u3 = ECCNSText.nextCharacter(&text).codepoint,
									u4 = ECCNSText.nextCharacter(&text).codepoint;
								
								if (isxdigit(u1) && isxdigit(u2) && isxdigit(u3) && isxdigit(u4))
									c = uint16Hex(u1, u2, u3, u4);
								else
									ECCNSText.advance(&text, -5);
							}
							
							io_libecc_Chars.appendCodepoint(&chars, c);
						}
						
						value = io_libecc_Input.attachValue(self->input, io_libecc_Chars.endAppend(&chars));
						self->value = ECCNSValue.key(io_libecc_Key.makeWithText(ECCNSValue.textOf(&value), value.type != ECC_VALTYPE_CHARS));
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
					
					self->value = ECCNSValue.key(io_libecc_Key.makeWithText(self->text, 0));
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

const char * tokenChars (eccasttoktype_t token, char buffer[4])
{
	int index;
    struct
    {
        const char* name;
        const eccasttoktype_t token;
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

eccvalue_t scanBinary (ecctextstring_t text, enum io_libecc_lexer_ScanFlags flags)
{
	int lazy = flags & io_libecc_lexer_scanLazy;
	char buffer[text.length + 1];
	char *end = buffer;
	double binary = NAN;
	
	if (flags & io_libecc_lexer_scanSloppy)
	{
		ecctextstring_t tail = ECCNSText.make(text.bytes + text.length, text.length);
		
		while (tail.length && ECCNSText.isSpace(ECCNSText.prevCharacter(&tail)))
			text.length = tail.length;
		
		while (text.length && ECCNSText.isSpace(ECCNSText.character(text)))
			ECCNSText.nextCharacter(&text);
	}
	else
		while (text.length && isspace(*text.bytes))
			ECCNSText.advance(&text, 1);
	
	memcpy(buffer, text.bytes, text.length);
	buffer[text.length] = '\0';
	
	if (text.length)
	{
		if (lazy && text.length >= 2 && buffer[0] == '0' && buffer[1] == 'x')
				return ECCNSValue.binary(0);
		
		if (text.length >= ECC_ConstString_Infinity.length && !memcmp(buffer, ECC_ConstString_Infinity.bytes, ECC_ConstString_Infinity.length))
			binary = INFINITY, end += ECC_ConstString_Infinity.length;
		else if (text.length >= ECC_ConstString_NegativeInfinity.length && !memcmp(buffer, ECC_ConstString_NegativeInfinity.bytes, ECC_ConstString_NegativeInfinity.length))
			binary = -INFINITY, end += ECC_ConstString_NegativeInfinity.length;
		else if (!isalpha(buffer[0]))
			binary = strtod(buffer, &end);
		
		if ((!lazy && *end && !isspace(*end)) || (lazy && end == buffer))
			binary = NAN;
	}
	else if (!lazy)
		binary = 0;
	
	return ECCNSValue.binary(binary);
}

static double strtolHexFallback (ecctextstring_t text)
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

eccvalue_t scanInteger (ecctextstring_t text, int base, enum io_libecc_lexer_ScanFlags flags)
{
	int lazy = flags & io_libecc_lexer_scanLazy;
	long integer;
	char buffer[text.length + 1];
	char *end;
	
	if (flags & io_libecc_lexer_scanSloppy)
	{
		ecctextstring_t tail = ECCNSText.make(text.bytes + text.length, text.length);
		
		while (tail.length && ECCNSText.isSpace(ECCNSText.prevCharacter(&tail)))
			text.length = tail.length;
		
		while (text.length && ECCNSText.isSpace(ECCNSText.character(text)))
			ECCNSText.nextCharacter(&text);
	}
	else
		while (text.length && isspace(*text.bytes))
			ECCNSText.advance(&text, 1);
	
	memcpy(buffer, text.bytes, text.length);
	buffer[text.length] = '\0';
	
	errno = 0;
	integer = strtol(buffer, &end, base);
	
	if ((lazy && end == buffer) || (!lazy && *end && !isspace(*end)))
		return ECCNSValue.binary(NAN);
	else if (errno == ERANGE)
	{
		if (!base || base == 10 || base == 16)
		{
			double binary = strtod(buffer, NULL);
			if (!binary && (!base || base == 16))
				binary = strtolHexFallback(text);
			
			return ECCNSValue.binary(binary);
		}
		
		ECCNSEnv.printWarning("`parseInt('%.*s', %d)` out of bounds; only long int are supported by radices other than 10 or 16", text.length, text.bytes, base);
		return ECCNSValue.binary(NAN);
	}
	else if (integer < INT32_MIN || integer > INT32_MAX)
		return ECCNSValue.binary(integer);
	else
		return ECCNSValue.integer((int32_t)integer);
}

uint32_t scanElement (ecctextstring_t text)
{
	eccvalue_t value;
	uint16_t index;
	
	if (!text.length)
		return UINT32_MAX;
	
	for (index = 0; index < text.length; ++index)
		if (!isdigit(text.bytes[index]))
			return UINT32_MAX;
	
	value = scanInteger(text, 0, 0);
	
	if (value.type == ECC_VALTYPE_INTEGER)
		return value.data.integer;
	if (value.type == ECC_VALTYPE_BINARY && value.data.binary >= 0 && value.data.binary < UINT32_MAX && value.data.binary == (uint32_t)value.data.binary)
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
