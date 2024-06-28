
require 'yaml'

ECC_TOK_NO = "ECC_TOK_NO"
ECC_TOK_ERROR = "ECC_TOK_ERROR"
ECC_TOK_NULL = "ECC_TOK_NULL"
ECC_TOK_TRUE = "ECC_TOK_TRUE"
ECC_TOK_FALSE = "ECC_TOK_FALSE"
ECC_TOK_INTEGER = "ECC_TOK_INTEGER"
ECC_TOK_BINARY = "ECC_TOK_BINARY"
ECC_TOK_STRING = "ECC_TOK_STRING"
ECC_TOK_ESCAPEDSTRING = "ECC_TOK_ESCAPEDSTRING"
ECC_TOK_IDENTIFIER = "ECC_TOK_IDENTIFIER"
ECC_TOK_REGEXP = "ECC_TOK_REGEXP"
ECC_TOK_BREAK = "ECC_TOK_BREAK"
ECC_TOK_CASE = "ECC_TOK_CASE"
ECC_TOK_CATCH = "ECC_TOK_CATCH"
ECC_TOK_CONTINUE = "ECC_TOK_CONTINUE"
ECC_TOK_DEBUGGER = "ECC_TOK_DEBUGGER"
ECC_TOK_DEFAULT = "ECC_TOK_DEFAULT"
ECC_TOK_DELETE = "ECC_TOK_DELETE"
ECC_TOK_DO = "ECC_TOK_DO"
ECC_TOK_ELSE = "ECC_TOK_ELSE"
ECC_TOK_FINALLY = "ECC_TOK_FINALLY"
ECC_TOK_FOR = "ECC_TOK_FOR"
ECC_TOK_FUNCTION = "ECC_TOK_FUNCTION"
ECC_TOK_IF = "ECC_TOK_IF"
ECC_TOK_IN = "ECC_TOK_IN"
ECC_TOK_INSTANCEOF = "ECC_TOK_INSTANCEOF"
ECC_TOK_NEW = "ECC_TOK_NEW"
ECC_TOK_RETURN = "ECC_TOK_RETURN"
ECC_TOK_SWITCH = "ECC_TOK_SWITCH"
ECC_TOK_THIS = "ECC_TOK_THIS"
ECC_TOK_THROW = "ECC_TOK_THROW"
ECC_TOK_TRY = "ECC_TOK_TRY"
ECC_TOK_TYPEOF = "ECC_TOK_TYPEOF"
ECC_TOK_VAR = "ECC_TOK_VAR"
ECC_TOK_VOID = "ECC_TOK_VOID"
ECC_TOK_WITH = "ECC_TOK_WITH"
ECC_TOK_WHILE = "ECC_TOK_WHILE"
ECC_TOK_EQUAL = "ECC_TOK_EQUAL"
ECC_TOK_NOTEQUAL = "ECC_TOK_NOTEQUAL"
ECC_TOK_IDENTICAL = "ECC_TOK_IDENTICAL"
ECC_TOK_NOTIDENTICAL = "ECC_TOK_NOTIDENTICAL"
ECC_TOK_LEFTSHIFTASSIGN = "ECC_TOK_LEFTSHIFTASSIGN"
ECC_TOK_RIGHTSHIFTASSIGN = "ECC_TOK_RIGHTSHIFTASSIGN"
ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN = "ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN"
ECC_TOK_LEFTSHIFT = "ECC_TOK_LEFTSHIFT"
ECC_TOK_RIGHTSHIFT = "ECC_TOK_RIGHTSHIFT"
ECC_TOK_UNSIGNEDRIGHTSHIFT = "ECC_TOK_UNSIGNEDRIGHTSHIFT"
ECC_TOK_LESSOREQUAL = "ECC_TOK_LESSOREQUAL"
ECC_TOK_MOREOREQUAL = "ECC_TOK_MOREOREQUAL"
ECC_TOK_INCREMENT = "ECC_TOK_INCREMENT"
ECC_TOK_DECREMENT = "ECC_TOK_DECREMENT"
ECC_TOK_LOGICALAND = "ECC_TOK_LOGICALAND"
ECC_TOK_LOGICALOR = "ECC_TOK_LOGICALOR"
ECC_TOK_ADDASSIGN = "ECC_TOK_ADDASSIGN"
ECC_TOK_MINUSASSIGN = "ECC_TOK_MINUSASSIGN"
ECC_TOK_MULTIPLYASSIGN = "ECC_TOK_MULTIPLYASSIGN"
ECC_TOK_DIVIDEASSIGN = "ECC_TOK_DIVIDEASSIGN"
ECC_TOK_MODULOASSIGN = "ECC_TOK_MODULOASSIGN"
ECC_TOK_ANDASSIGN = "ECC_TOK_ANDASSIGN"
ECC_TOK_ORASSIGN = "ECC_TOK_ORASSIGN"
ECC_TOK_XORASSIGN = "ECC_TOK_XORASSIGN"


src=<<__eos__
[
    [ (sizeof("end of script") > sizeof("") ? "end of script" : "no"), ECC_TOK_NO ],
    [ (sizeof("") > sizeof("") ? "" : "error"), ECC_TOK_ERROR ],
    [ (sizeof("") > sizeof("") ? "" : "null"), ECC_TOK_NULL ],
    [ (sizeof("") > sizeof("") ? "" : "true"), ECC_TOK_TRUE ],
    [ (sizeof("") > sizeof("") ? "" : "false"), ECC_TOK_FALSE ],
    [ (sizeof("number") > sizeof("") ? "number" : "integer"), ECC_TOK_INTEGER ],
    [ (sizeof("number") > sizeof("") ? "number" : "binary"), ECC_TOK_BINARY ],
    [ (sizeof("") > sizeof("") ? "" : "string"), ECC_TOK_STRING ],
    [ (sizeof("string") > sizeof("") ? "string" : "escapedString"), ECC_TOK_ESCAPEDSTRING ],
    [ (sizeof("") > sizeof("") ? "" : "identifier"), ECC_TOK_IDENTIFIER ],
    [ (sizeof("") > sizeof("") ? "" : "regexp"), ECC_TOK_REGEXP ],
    [ (sizeof("") > sizeof("") ? "" : "break"), ECC_TOK_BREAK ],
    [ (sizeof("") > sizeof("") ? "" : "case"), ECC_TOK_CASE ],
    [ (sizeof("") > sizeof("") ? "" : "catch"), ECC_TOK_CATCH ],
    [ (sizeof("") > sizeof("") ? "" : "continue"), ECC_TOK_CONTINUE ],
    [ (sizeof("") > sizeof("") ? "" : "debugger"), ECC_TOK_DEBUGGER ],
    [ (sizeof("") > sizeof("") ? "" : "default"), ECC_TOK_DEFAULT ],
    [ (sizeof("") > sizeof("") ? "" : "delete"), ECC_TOK_DELETE ],
    [ (sizeof("") > sizeof("") ? "" : "do"), ECC_TOK_DO ],
    [ (sizeof("") > sizeof("") ? "" : "else"), ECC_TOK_ELSE ],
    [ (sizeof("") > sizeof("") ? "" : "finally"), ECC_TOK_FINALLY ],
    [ (sizeof("") > sizeof("") ? "" : "for"), ECC_TOK_FOR ],
    [ (sizeof("") > sizeof("") ? "" : "function"), ECC_TOK_FUNCTION ],
    [ (sizeof("") > sizeof("") ? "" : "if"), ECC_TOK_IF ],
    [ (sizeof("") > sizeof("") ? "" : "in"), ECC_TOK_IN ],
    [ (sizeof("") > sizeof("") ? "" : "instanceof"), ECC_TOK_INSTANCEOF ],
    [ (sizeof("") > sizeof("") ? "" : "new"), ECC_TOK_NEW ],
    [ (sizeof("") > sizeof("") ? "" : "return"), ECC_TOK_RETURN ],
    [ (sizeof("") > sizeof("") ? "" : "switch"), ECC_TOK_SWITCH ],
    [ (sizeof("") > sizeof("") ? "" : "this"), ECC_TOK_THIS ],
    [ (sizeof("") > sizeof("") ? "" : "throw"), ECC_TOK_THROW ],
    [ (sizeof("") > sizeof("") ? "" : "try"), ECC_TOK_TRY ],
    [ (sizeof("") > sizeof("") ? "" : "typeof"), ECC_TOK_TYPEOF ],
    [ (sizeof("") > sizeof("") ? "" : "var"), ECC_TOK_VAR ],
    [ (sizeof("") > sizeof("") ? "" : "void"), ECC_TOK_VOID ],
    [ (sizeof("") > sizeof("") ? "" : "with"), ECC_TOK_WITH ],
    [ (sizeof("") > sizeof("") ? "" : "while"), ECC_TOK_WHILE ],
    [ (sizeof("'=='") > sizeof("") ? "'=='" : "equal"), ECC_TOK_EQUAL ],
    [ (sizeof("'!='") > sizeof("") ? "'!='" : "notEqual"), ECC_TOK_NOTEQUAL ],
    [ (sizeof("'==='") > sizeof("") ? "'==='" : "identical"), ECC_TOK_IDENTICAL ],
    [ (sizeof("'!=='") > sizeof("") ? "'!=='" : "notIdentical"), ECC_TOK_NOTIDENTICAL ],
    [ (sizeof("'<<='") > sizeof("") ? "'<<='" : "leftShiftAssign"), ECC_TOK_LEFTSHIFTASSIGN ],
    [ (sizeof("'>>='") > sizeof("") ? "'>>='" : "rightShiftAssign"), ECC_TOK_RIGHTSHIFTASSIGN ],
    [ (sizeof("'>>>='") > sizeof("") ? "'>>>='" : "unsignedRightShiftAssign"), ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN ],
    [ (sizeof("'<<'") > sizeof("") ? "'<<'" : "leftShift"), ECC_TOK_LEFTSHIFT ],
    [ (sizeof("'>>'") > sizeof("") ? "'>>'" : "rightShift"), ECC_TOK_RIGHTSHIFT ],
    [ (sizeof("'>>>'") > sizeof("") ? "'>>>'" : "unsignedRightShift"), ECC_TOK_UNSIGNEDRIGHTSHIFT ],
    [ (sizeof("'<='") > sizeof("") ? "'<='" : "lessOrEqual"), ECC_TOK_LESSOREQUAL ],
    [ (sizeof("'>='") > sizeof("") ? "'>='" : "moreOrEqual"), ECC_TOK_MOREOREQUAL ],
    [ (sizeof("'++'") > sizeof("") ? "'++'" : "increment"), ECC_TOK_INCREMENT ],
    [ (sizeof("'--'") > sizeof("") ? "'--'" : "decrement"), ECC_TOK_DECREMENT ],
    [ (sizeof("'&&'") > sizeof("") ? "'&&'" : "logicalAnd"), ECC_TOK_LOGICALAND ],
    [ (sizeof("'||'") > sizeof("") ? "'||'" : "logicalOr"), ECC_TOK_LOGICALOR ],
    [ (sizeof("'+='") > sizeof("") ? "'+='" : "addAssign"), ECC_TOK_ADDASSIGN ],
    [ (sizeof("'-='") > sizeof("") ? "'-='" : "minusAssign"), ECC_TOK_MINUSASSIGN ],
    [ (sizeof("'*='") > sizeof("") ? "'*='" : "multiplyAssign"), ECC_TOK_MULTIPLYASSIGN ],
    [ (sizeof("'/='") > sizeof("") ? "'/='" : "divideAssign"), ECC_TOK_DIVIDEASSIGN ],
    [ (sizeof("'%='") > sizeof("") ? "'%='" : "moduloAssign"), ECC_TOK_MODULOASSIGN ],
    [ (sizeof("'&='") > sizeof("") ? "'&='" : "andAssign"), ECC_TOK_ANDASSIGN ],
    [ (sizeof("'|='") > sizeof("") ? "'|='" : "orAssign"), ECC_TOK_ORASSIGN ],
    [ (sizeof("'^='") > sizeof("") ? "'^='" : "xorAssign"), ECC_TOK_XORASSIGN ],
]
__eos__

def sizeof(s)
  return s.length
end

eval(src).each do |thing|
  printf("    {%p, %s},\n", thing[0], thing[1])
end








