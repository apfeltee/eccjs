
/*
//  main.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

static eccstate_t* ecc;
static int g_testverbosity = 0;
static int g_testerrorcount = 0;
static int g_testtotalcount = 0;
static double g_testtime = 0;


static void ecc_unittest_actuallyruntest(const char* func, int line, const char* test, const char* expect, const char* text)
{
    uint16_t length;
    clock_t start;
    ptrdiff_t textStart;
    ptrdiff_t textLength;
    const char* bytes;
    const char* end;
    start = clock();
    if(g_testverbosity > 0 || !setjmp(*ecc_script_pushenv(ecc)))
    {
        ecc_script_evalinput(ecc, ecc_ioinput_createfrombytes(test, (uint32_t)strlen(test), "%s:%d", func, line), ECC_SCRIPTEVAL_STRINGRESULT);
    }
    if(g_testverbosity <= 0)
    {
        ecc_script_popenv(ecc);
    }
    g_testtime += (double)(clock() - start) / CLOCKS_PER_SEC;
    ++g_testtotalcount;
    assert(ecc_value_isstring(ecc->result));
    bytes = ecc_value_stringbytes(&ecc->result);
    length = ecc_value_stringlength(&ecc->result);
    if(length != strlen(expect) || memcmp(expect, bytes, length))
    {
        ++g_testerrorcount;
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
            ++g_testerrorcount;
            ecc_env_printcolor(ECC_COLOR_RED, ECC_ENVATTR_BOLD, "[failure]");
            ecc_env_print(" %s:%d - ", func, line);
            ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "text should highlight `%.*s`", textLength, test + textStart);
            ecc_env_newline();
            goto error;
        }
    }
    if(g_testverbosity >= 0)
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

#define test(i, e, t) ecc_unittest_actuallyruntest(__func__, __LINE__, i, e, t)

#include "testcode.h"

static int ecc_unittest_runtests(int verbosity)
{
    g_testverbosity = verbosity;
    /* test("debugger", "undefined", NULL); */
    ecc_unittest_testlexer();
    ecc_unittest_testparser();
    ecc_unittest_testeval();
    ecc_unittest_testconvertion();
    ecc_unittest_testexception();
    ecc_unittest_testoperator();
    ecc_unittest_testequality();
    ecc_unittest_testrelational();
    ecc_unittest_testconditional();
    ecc_unittest_testswitch();
    ecc_unittest_testdelete();
    ecc_unittest_testglobal();
    ecc_unittest_testfunction();
    ecc_unittest_testloop();
    ecc_unittest_testthis();
    ecc_unittest_testobject();
    ecc_unittest_testerror();
    ecc_unittest_testaccessor();
    ecc_unittest_testarray();
    ecc_unittest_testboolean();
    ecc_unittest_testnumber();
    ecc_unittest_testdate();
    ecc_unittest_teststring();
    ecc_unittest_testregexp();
    ecc_unittest_testjson();
    ecc_env_newline();
    if(g_testerrorcount)
    {
        ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "test failure: %d", g_testerrorcount);
    }
    else
    {
        ecc_env_printcolor(0, ECC_ENVATTR_BOLD, "all success");
    }
    ecc_env_newline();
    ecc_env_newline();
    ecc_env_print("%d tests, %.2f ms", g_testtotalcount, g_testtime * 1000);
    return g_testerrorcount ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int ecc_cli_printusage(void)
{
    const char error[] = "Usage";
    ecc_env_printerror(sizeof(error) - 1, error, "libecc [<filename> | --test | --test-verbose | --test-quiet]");

    return EXIT_FAILURE;
}

static eccvalue_t ecc_cliutil_printhelper(ecccontext_t* context, FILE* file, bool space, bool linefeed)
{
    int index;
    int count;
    eccvalue_t value;
    for(index = 0, count = ecc_context_argumentcount(context); index < count; ++index)
    {
        if(index && space)
        {
            #if 0
            putc(' ', file);
            #endif
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

static eccvalue_t ecc_clifn_alert(ecccontext_t* context)
{
    return ecc_cliutil_printhelper(context, stderr, true, true);
}

static eccvalue_t ecc_clifn_print(ecccontext_t* context)
{
    return ecc_cliutil_printhelper(context, stdout, false, false);
}

static eccvalue_t ecc_clifn_println(ecccontext_t* context)
{
    return ecc_cliutil_printhelper(context, stdout, false, true);
}

int main(int argc, const char* argv[])
{
    int result;
    ecc = ecc_script_create();
    ecc_script_addfunction(ecc, "alert", ecc_clifn_alert, -1, 0);
    ecc_script_addfunction(ecc, "print", ecc_clifn_print, -1, 0);
    ecc_script_addfunction(ecc, "println", ecc_clifn_println, -1, 0);
    if(argc <= 1 || !strcmp(argv[1], "--help"))
    {
        result = ecc_cli_printusage();
    }
    else if(!strcmp(argv[1], "--test"))
    {
        result = ecc_unittest_runtests(0);
    }
    else if(!strcmp(argv[1], "--test-verbose"))
    {
        result = ecc_unittest_runtests(1);
    }
    else if(!strcmp(argv[1], "--test-quiet"))
    {
        result = ecc_unittest_runtests(-1);
    }
    else
    {
        eccobject_t* arguments = ecc_args_createwithclist(argc - 2, &argv[2]);
        ecc_script_addvalue(ecc, "arguments", ecc_value_object(arguments), 0);
        ecc_script_addvalue(ecc, "SHELLARGV", ecc_value_object(arguments), 0);
        result = ecc_script_evalinput(ecc, ecc_ioinput_createfromfile(argv[1]), ECC_SCRIPTEVAL_SLOPPYMODE);
    }
    ecc_script_destroy(ecc), ecc = NULL;
    return result;
}
