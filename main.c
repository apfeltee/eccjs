//
//  main.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

static eccstate_t* ecc;
static int testVerbosity = 0;
static int testErrorCount = 0;
static int testCount = 0;
static double testTime = 0;


static void actuallyruntest(const char* func, int line, const char* test, const char* expect, const char* text)
{
    uint16_t length;
    clock_t start;
    ptrdiff_t textStart;
    ptrdiff_t textLength;
    const char* bytes;
    const char* end;
    start = clock();
    if(testVerbosity > 0 || !setjmp(*ecc_script_pushenv(ecc)))
    {
        ecc_script_evalinput(ecc, ecc_ioinput_createfrombytes(test, (uint32_t)strlen(test), "%s:%d", func, line), ECC_SCRIPTEVAL_STRINGRESULT);
    }
    if(testVerbosity <= 0)
    {
        ecc_script_popenv(ecc);
    }
    testTime += (double)(clock() - start) / CLOCKS_PER_SEC;
    ++testCount;
    assert(ecc_value_isstring(ecc->result));
    bytes = ecc_value_stringbytes(&ecc->result);
    length = ecc_value_stringlength(&ecc->result);
    if(length != strlen(expect) || memcmp(expect, bytes, length))
    {
        ++testErrorCount;
        ecc_env_printcolor(ECC_COLOR_RED, ECC_ENVATTR_BOLD, "[failure]");
        ecc_env_print(" %s:%d - ", func, line);
        ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "expect \"%s\" was \"%.*s\"", expect, length, bytes);
        ecc_env_newline();
        goto error;
    }
    if(text)
    {
        end = strrchr(text, '~');
        textStart = strchr(text, '^') - text;
        textLength = (end ? end - text - textStart : 0) + 1;
        eccioinput_t* input = ecc_script_findinput(ecc, ecc->text);
        if(ecc->text.bytes >= ecc->ofText.bytes && ecc->text.bytes < ecc->ofText.bytes + ecc->ofText.length)
        {
            bytes = ecc->ofText.bytes;
        }
        else if(input)
        {
            bytes = input->bytes;
        }
        else
        {
            bytes = NULL;
        }
        assert((textStart >= 0) && (textStart <= (ptrdiff_t)strlen(test)));
        if(input && (input->length - textStart == 0 || (!ecc->text.length && input->bytes[textStart] == ')')) && textLength == 1)
        {
            textLength = 0;
        }
        if(!bytes || ecc->text.bytes - bytes != textStart || ecc->text.length != textLength)
        {
            ++testErrorCount;
            ecc_env_printcolor(ECC_COLOR_RED, ECC_ENVATTR_BOLD, "[failure]");
            ecc_env_print(" %s:%d - ", func, line);
            ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "text should highlight `%.*s`", textLength, test + textStart);
            ecc_env_newline();
            goto error;
        }
    }
    if(testVerbosity >= 0)
    {
        #if 0
        ecc_env_printcolor(ECC_COLOR_GREEN, ECC_ENVATTR_BOLD, "[success]");
        ecc_env_print(" %s:%d", func, line);
        ecc_env_newline();
        #endif
    }
error:
    ecc_script_garbagecollect(ecc);
}

#include "testcode.inc"

static int runTest(int verbosity)
{
    testVerbosity = verbosity;
    //test("debugger", "undefined", NULL);
    testLexer();
    testParser();
    testEval();
    testConvertion();
    testException();
    testOperator();
    testEquality();
    testRelational();
    testConditional();
    testSwitch();
    testDelete();
    testGlobal();
    testFunction();
    testLoop();
    testThis();
    testObject();
    testError();
    testAccessor();
    testArray();
    testBoolean();
    testNumber();
    testDate();
    testString();
    testRegExp();
    testJSON();
    ecc_env_newline();
    if(testErrorCount)
    {
        ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "test failure: %d", testErrorCount);
    }
    else
    {
        ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "all success");
    }
    ecc_env_newline();
    ecc_env_newline();
    ecc_env_print("%d tests, %.2f ms", testCount, testTime * 1000);
    return testErrorCount ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int alertUsage(void)
{
    const char error[] = "Usage";
    ecc_env_printerror(sizeof(error) - 1, error, "libecc [<filename> | --test | --test-verbose | --test-quiet]");

    return EXIT_FAILURE;
}

static eccvalue_t printhelper(ecccontext_t* context, FILE* file, bool space, bool linefeed)
{
    int index;
    int count;
    eccvalue_t value;
    for(index = 0, count = ecc_context_argumentcount(context); index < count; ++index)
    {
        if(index && space)
        {
            //putc(' ', file);
        }
        value = ecc_context_argument(context, index);
        ecc_value_dumpto(ecc_value_tostring(context, value), file);
    }
    if(linefeed)
    {
        putc('\n', file);
    }
    return ECCValConstUndefined;
}

static eccvalue_t cfn_alert(ecccontext_t* context)
{
    return printhelper(context, stderr, true, true);
}

static eccvalue_t cfn_print(ecccontext_t* context)
{
    return printhelper(context, stdout, false, false);
}

static eccvalue_t cfn_println(ecccontext_t* context)
{
    return printhelper(context, stdout, false, true);
}

int main(int argc, const char* argv[])
{
    int result;
    ecc = ecc_script_create();
    ecc_script_addfunction(ecc, "alert", cfn_alert, -1, 0);
    ecc_script_addfunction(ecc, "print", cfn_print, -1, 0);
    ecc_script_addfunction(ecc, "println", cfn_println, -1, 0);
    if(argc <= 1 || !strcmp(argv[1], "--help"))
    {
        result = alertUsage();
    }
    else if(!strcmp(argv[1], "--test"))
    {
        result = runTest(0);
    }
    else if(!strcmp(argv[1], "--test-verbose"))
    {
        result = runTest(1);
    }
    else if(!strcmp(argv[1], "--test-quiet"))
    {
        result = runTest(-1);
    }
    else
    {
        eccobject_t* arguments = ecc_args_createwithclist(argc - 2, &argv[2]);
        ecc_script_addvalue(ecc, "arguments", ecc_value_object(arguments), 0);
        result = ecc_script_evalinput(ecc, ecc_ioinput_createfromfile(argv[1]), ECC_SCRIPTEVAL_SLOPPYMODE);
    }
    ecc_script_destroy(ecc), ecc = NULL;
    return result;
}
