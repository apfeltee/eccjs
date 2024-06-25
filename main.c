//
//  main.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

static eccscriptcontext_t* ecc;

static int runTest(int verbosity);

//

static int alertUsage(void)
{
    const char error[] = "Usage";
    ECCNSEnv.printError(sizeof(error) - 1, error, "libecc [<filename> | --test | --test-verbose | --test-quiet]");

    return EXIT_FAILURE;
}

//

static eccvalue_t printhelper(eccstate_t* context, FILE* file, bool space, bool linefeed)
{
    int index, count;
    eccvalue_t value;

    for(index = 0, count = ECCNSContext.argumentCount(context); index < count; ++index)
    {
        if(index && space)
        {
            //putc(' ', file);
        }
        value = ECCNSContext.argument(context, index);
        ECCNSValue.dumpTo(ECCNSValue.toString(context, value), file);
    }
    if(linefeed)
    {
        putc('\n', file);
    }
    return ECCValConstUndefined;
}

static eccvalue_t cfn_alert(eccstate_t* context)
{
    return printhelper(context, stderr, true, true);
}

static eccvalue_t cfn_print(eccstate_t* context)
{
    return printhelper(context, stdout, false, false);
}

static eccvalue_t cfn_println(eccstate_t* context)
{
    return printhelper(context, stdout, false, true);
}

int main(int argc, const char* argv[])
{
    int result;

    ecc = ECCNSScript.create();

    ECCNSScript.addFunction(ecc, "alert", cfn_alert, -1, 0);
    ECCNSScript.addFunction(ecc, "print", cfn_print, -1, 0);
    ECCNSScript.addFunction(ecc, "println", cfn_println, -1, 0);

    if(argc <= 1 || !strcmp(argv[1], "--help"))
        result = alertUsage();
    else if(!strcmp(argv[1], "--test"))
        result = runTest(0);
    else if(!strcmp(argv[1], "--test-verbose"))
        result = runTest(1);
    else if(!strcmp(argv[1], "--test-quiet"))
        result = runTest(-1);
    else
    {
        eccobject_t* arguments = ECCNSArguments.createWithCList(argc - 2, &argv[2]);
        ECCNSScript.addValue(ecc, "arguments", ECCNSValue.object(arguments), 0);
        result = ECCNSScript.evalInput(ecc, io_libecc_Input.createFromFile(argv[1]), io_libecc_ecc_sloppyMode);
    }

    ECCNSScript.destroy(ecc), ecc = NULL;

    return result;
}

//

static int testVerbosity = 0;
static int testErrorCount = 0;
static int testCount = 0;
static double testTime = 0;

io_libecc_ecc_useframe static void actuallyruntest(const char* func, int line, const char* test, const char* expect, const char* text)
{
    const char* bytes;
    uint16_t length;
    clock_t start = clock();

    if(testVerbosity > 0 || !setjmp(*ECCNSScript.pushEnv(ecc)))
        ECCNSScript.evalInput(ecc, io_libecc_Input.createFromBytes(test, (uint32_t)strlen(test), "%s:%d", func, line), io_libecc_ecc_stringResult);

    if(testVerbosity <= 0)
        ECCNSScript.popEnv(ecc);

    testTime += (double)(clock() - start) / CLOCKS_PER_SEC;
    ++testCount;

    assert(ECCNSValue.isString(ecc->result));
    bytes = ECCNSValue.stringBytes(&ecc->result);
    length = ECCNSValue.stringLength(&ecc->result);

    if(length != strlen(expect) || memcmp(expect, bytes, length))
    {
        ++testErrorCount;
        ECCNSEnv.printColor(io_libecc_env_red, io_libecc_env_bold, "[failure]");
        ECCNSEnv.print(" %s:%d - ", func, line);
        ECCNSEnv.printColor(0, io_libecc_env_bold, "expect \"%s\" was \"%.*s\"", expect, length, bytes);
        ECCNSEnv.newline();
        goto error;
    }

    if(text)
    {
        const char* end = strrchr(text, '~');
        ptrdiff_t textStart = strchr(text, '^') - text, textLength = (end ? end - text - textStart : 0) + 1;
        eccioinput_t* input = ECCNSScript.findInput(ecc, ecc->text);

        if(ecc->text.bytes >= ecc->ofText.bytes && ecc->text.bytes < ecc->ofText.bytes + ecc->ofText.length)
            bytes = ecc->ofText.bytes;
        else if(input)
            bytes = input->bytes;
        else
            bytes = NULL;

        assert(textStart >= 0 && textStart <= strlen(test));

        if(input && (input->length - textStart == 0 || (!ecc->text.length && input->bytes[textStart] == ')')) && textLength == 1)
            textLength = 0;

        if(!bytes || ecc->text.bytes - bytes != textStart || ecc->text.length != textLength)
        {
            ++testErrorCount;
            ECCNSEnv.printColor(io_libecc_env_red, io_libecc_env_bold, "[failure]");
            ECCNSEnv.print(" %s:%d - ", func, line);
            ECCNSEnv.printColor(0, io_libecc_env_bold, "text should highlight `%.*s`", textLength, test + textStart);
            ECCNSEnv.newline();
            goto error;
        }
    }

    if(testVerbosity >= 0)
    {
        ECCNSEnv.printColor(io_libecc_env_green, io_libecc_env_bold, "[success]");
        ECCNSEnv.print(" %s:%d", func, line);
        ECCNSEnv.newline();
    }

error:
    ECCNSScript.garbageCollect(ecc);
}

#include "testcode.inc"

static int runTest(int verbosity)
{
    testVerbosity = verbosity;

    //	test("debugger", "undefined", NULL);

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

    ECCNSEnv.newline();

    if(testErrorCount)
        ECCNSEnv.printColor(0, io_libecc_env_bold, "test failure: %d", testErrorCount);
    else
        ECCNSEnv.printColor(0, io_libecc_env_bold, "all success");

    ECCNSEnv.newline();
    ECCNSEnv.newline();
    ECCNSEnv.print("%d tests, %.2f ms", testCount, testTime * 1000);

    return testErrorCount ? EXIT_FAILURE : EXIT_SUCCESS;
}
