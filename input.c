//
//  input.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

eccioinput_t* ecc_ioinput_create(void)
{
    size_t linesBytes;
    eccioinput_t* self = (eccioinput_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccioinput_t));
    self->lineCapacity = 8;
    self->lineCount = 1;
    linesBytes = sizeof(*self->lines) * self->lineCapacity;
    self->lines = (uint32_t*)malloc(linesBytes);
    memset(self->lines, 0, linesBytes);
    return self;
}

void ecc_ioinput_printinput(const char* name, uint16_t line)
{
    if(name[0] == '(')
        ecc_env_printcolor(0, ECC_ENVATTR_DIM, "%s", name);
    else
        ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "%s", name);

    ecc_env_printcolor(0, ECC_ENVATTR_BOLD, " line:%d", line);
}

eccioinput_t* ecc_ioinput_createfromfile(const char* filename)
{
    ecctextstring_t inputError = ECC_ConstString_InputErrorName;
    FILE* file;
    long size;
    eccioinput_t* self;

    assert(filename);

    file = fopen(filename, "rb");
    if(!file)
    {
        ecc_env_printerror(inputError.length, inputError.bytes, "cannot open file '%s'", filename);
        return NULL;
    }

    if(fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET))
    {
        ecc_env_printerror(inputError.length, inputError.bytes, "cannot handle file '%s'", filename);
        fclose(file);
        return NULL;
    }

    self = ecc_ioinput_create();

    strncat(self->name, filename, sizeof(self->name) - 1);
    self->bytes = (char*)malloc(size + 1);
    self->length = (uint32_t)fread(self->bytes, sizeof(char), size, file);
    fclose(file), file = NULL;
    self->bytes[size] = '\0';

    //	FILE *f = fopen("error.txt", "w");
    //	fprintf(f, "%.*s", self->length, self->bytes);
    //	fclose(f);

    return self;
}

eccioinput_t* ecc_ioinput_createfrombytes(const char* bytes, uint32_t length, const char* name, ...)
{
    eccioinput_t* self;

    assert(bytes);

    self = ecc_ioinput_create();

    if(name)
    {
        va_list ap;
        va_start(ap, name);
        vsnprintf(self->name, sizeof(self->name), name, ap);
        va_end(ap);
    }
    self->length = length;
    self->bytes = (char*)malloc(length + 1);
    memcpy(self->bytes, bytes, length);
    self->bytes[length] = '\0';

    return self;
}

void ecc_ioinput_destroy(eccioinput_t* self)
{
    assert(self);

    free(self->attached), self->attached = NULL;
    free(self->bytes), self->bytes = NULL;
    free(self->lines), self->lines = NULL;
    free(self), self = NULL;
}

void ecc_ioinput_printtext(eccioinput_t* self, ecctextstring_t text, int32_t ofLine, ecctextstring_t ofText, const char* ofInput, int fullLine)
{
    int32_t line;
    uint32_t start;
    uint16_t length;
    long index;
    long marked;
    char* mark;
    const char* bytes;
    line = -1;
    bytes = NULL;
    length = 0;
    if(ofText.length)
    {
        bytes = ofText.bytes;
        length = ofText.length;
        ecc_ioinput_printinput(ofInput ? ofInput : "native code", ofLine ? ofLine : 1);
    }
    else if(!self)
    {
        ecc_ioinput_printinput(ofInput ? ofInput : "(unknown input)", 0);
    }
    else
    {
        line = ecc_ioinput_findline(self, text);
        ecc_ioinput_printinput(ofInput ? ofInput : self->name, line > 0 ? line : 0);
        if(line > 0)
        {
            start = self->lines[line];
            bytes = self->bytes + start;
            do
            {
                if(!isblank(bytes[length]) && !isgraph(bytes[length]) && bytes[length] >= 0)
                {
                    break;
                }
                ++length;
            } while(start + length < self->length);
        }
    }
    if(!fullLine)
    {
        if(text.length)
        {
            ecc_env_printcolor(0, 0, " `%.*s`", text.length, text.bytes);
        }
    }
    else if(!length)
    {
        ecc_env_newline();
        ecc_env_printcolor(0, 0, "%.*s", text.length, text.bytes);
    }
    else
    {
        ecc_env_newline();
        ecc_env_print("%.*s", length, bytes);
        ecc_env_newline();

        if(length >= text.bytes - bytes)
        {
            //char mark[length + 2];
            index = 0;
            marked = 0;
            mark = (char*)calloc(length+2, sizeof(char));
            for(; index < text.bytes - bytes; ++index)
            {
                if(isprint(bytes[index]))
                {
                    mark[index] = ' ';
                }
                else
                {
                    mark[index] = bytes[index];
                }
            }
            if((signed char)bytes[index] >= 0)
            {
                mark[index] = '^';
                marked = 1;
            }
            else
            {
                mark[index] = bytes[index];
                if(index > 0)
                {
                    mark[index - 1] = '>';
                    marked = 1;
                }
            }
            while(++index < text.bytes - bytes + text.length && index <= length)
            {
                if(isprint(bytes[index]) || isspace(bytes[index]))
                {
                    mark[index] = '~';
                }
                else
                {
                    mark[index] = bytes[index];
                }
            }
            if(!marked)
            {
                mark[index++] = '<';
            }
            mark[index] = '\0';

            if((text.bytes - bytes) > 0)
            {
                ecc_env_printcolor(0, ECC_ENVATTR_INVISIBLE, "%.*s", (text.bytes - bytes), mark);
            }
            ecc_env_printcolor(ECC_COLOR_GREEN, ECC_ENVATTR_BOLD, "%s", mark + (text.bytes - bytes));
            free(mark);
        }
    }

    ecc_env_newline();
}

int32_t ecc_ioinput_findline(eccioinput_t* self, ecctextstring_t text)
{
    uint16_t line = self->lineCount + 1;
    while(line--)
        if((self->bytes + self->lines[line] <= text.bytes) && (self->bytes + self->lines[line] < self->bytes + self->length))
            return line;

    return -1;
}

eccvalue_t ecc_ioinput_attachvalue(eccioinput_t* self, eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        value.data.chars->referenceCount++;

    self->attached = (eccvalue_t*)realloc(self->attached, sizeof(*self->attached) * (self->attachedCount + 1));
    self->attached[self->attachedCount] = value;
    return value;
}
