//
//  op.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "builtins.h"

struct io_libecc_Op
{
    io_libecc_native_io_libecc_Function native;
    struct io_libecc_Value value;
    struct io_libecc_Text text;
};

struct type_io_libecc_Op
{
    struct io_libecc_Op (*make)(const io_libecc_native_io_libecc_Function native, struct io_libecc_Value value, struct io_libecc_Text text);
    const char* (*toChars)(const io_libecc_native_io_libecc_Function native);
    struct io_libecc_Value (*callFunctionArguments)(struct io_libecc_Context* const,
                                                    enum io_libecc_context_Offset,
                                                    struct io_libecc_Function* function,
                                                    struct io_libecc_Value this,
                                                    struct io_libecc_Object* arguments);
    struct io_libecc_Value (*callFunctionVA)(
    struct io_libecc_Context* const, enum io_libecc_context_Offset, struct io_libecc_Function* function, struct io_libecc_Value this, int argumentCount, va_list ap);
    struct io_libecc_Value (*noop)(struct io_libecc_Context* const);
    struct io_libecc_Value (*value)(struct io_libecc_Context* const);
    struct io_libecc_Value (*valueConstRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*text)(struct io_libecc_Context* const);
    struct io_libecc_Value (*function)(struct io_libecc_Context* const);
    struct io_libecc_Value (*object)(struct io_libecc_Context* const);
    struct io_libecc_Value (*array)(struct io_libecc_Context* const);
    struct io_libecc_Value (*regexp)(struct io_libecc_Context* const);
    struct io_libecc_Value (*this)(struct io_libecc_Context* const);
    struct io_libecc_Value (*createLocalRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getLocalRefOrNull)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getLocalRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getLocal)(struct io_libecc_Context* const);
    struct io_libecc_Value (*setLocal)(struct io_libecc_Context* const);
    struct io_libecc_Value (*deleteLocal)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getLocalSlotRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getLocalSlot)(struct io_libecc_Context* const);
    struct io_libecc_Value (*setLocalSlot)(struct io_libecc_Context* const);
    struct io_libecc_Value (*deleteLocalSlot)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getParentSlotRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getParentSlot)(struct io_libecc_Context* const);
    struct io_libecc_Value (*setParentSlot)(struct io_libecc_Context* const);
    struct io_libecc_Value (*deleteParentSlot)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getMemberRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getMember)(struct io_libecc_Context* const);
    struct io_libecc_Value (*setMember)(struct io_libecc_Context* const);
    struct io_libecc_Value (*callMember)(struct io_libecc_Context* const);
    struct io_libecc_Value (*deleteMember)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getPropertyRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*getProperty)(struct io_libecc_Context* const);
    struct io_libecc_Value (*setProperty)(struct io_libecc_Context* const);
    struct io_libecc_Value (*callProperty)(struct io_libecc_Context* const);
    struct io_libecc_Value (*deleteProperty)(struct io_libecc_Context* const);
    struct io_libecc_Value (*pushEnvironment)(struct io_libecc_Context* const);
    struct io_libecc_Value (*popEnvironment)(struct io_libecc_Context* const);
    struct io_libecc_Value (*exchange)(struct io_libecc_Context* const);
    struct io_libecc_Value (*typeOf)(struct io_libecc_Context* const);
    struct io_libecc_Value (*equal)(struct io_libecc_Context* const);
    struct io_libecc_Value (*notEqual)(struct io_libecc_Context* const);
    struct io_libecc_Value (*identical)(struct io_libecc_Context* const);
    struct io_libecc_Value (*notIdentical)(struct io_libecc_Context* const);
    struct io_libecc_Value (*less)(struct io_libecc_Context* const);
    struct io_libecc_Value (*lessOrEqual)(struct io_libecc_Context* const);
    struct io_libecc_Value (*more)(struct io_libecc_Context* const);
    struct io_libecc_Value (*moreOrEqual)(struct io_libecc_Context* const);
    struct io_libecc_Value (*instanceOf)(struct io_libecc_Context* const);
    struct io_libecc_Value (*in)(struct io_libecc_Context* const);
    struct io_libecc_Value (*add)(struct io_libecc_Context* const);
    struct io_libecc_Value (*minus)(struct io_libecc_Context* const);
    struct io_libecc_Value (*multiply)(struct io_libecc_Context* const);
    struct io_libecc_Value (*divide)(struct io_libecc_Context* const);
    struct io_libecc_Value (*modulo)(struct io_libecc_Context* const);
    struct io_libecc_Value (*leftShift)(struct io_libecc_Context* const);
    struct io_libecc_Value (*rightShift)(struct io_libecc_Context* const);
    struct io_libecc_Value (*unsignedRightShift)(struct io_libecc_Context* const);
    struct io_libecc_Value (*bitwiseAnd)(struct io_libecc_Context* const);
    struct io_libecc_Value (*bitwiseXor)(struct io_libecc_Context* const);
    struct io_libecc_Value (*bitwiseOr)(struct io_libecc_Context* const);
    struct io_libecc_Value (*logicalAnd)(struct io_libecc_Context* const);
    struct io_libecc_Value (*logicalOr)(struct io_libecc_Context* const);
    struct io_libecc_Value (*positive)(struct io_libecc_Context* const);
    struct io_libecc_Value (*negative)(struct io_libecc_Context* const);
    struct io_libecc_Value (*invert)(struct io_libecc_Context* const);
    struct io_libecc_Value (*not )(struct io_libecc_Context* const);
    struct io_libecc_Value (*construct)(struct io_libecc_Context* const);
    struct io_libecc_Value (*call)(struct io_libecc_Context* const);
    struct io_libecc_Value (*eval)(struct io_libecc_Context* const);
    struct io_libecc_Value (*incrementRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*decrementRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*postIncrementRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*postDecrementRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*addAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*minusAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*multiplyAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*divideAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*moduloAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*leftShiftAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*rightShiftAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*unsignedRightShiftAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*bitAndAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*bitXorAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*bitOrAssignRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*debugger)(struct io_libecc_Context* const);
    struct io_libecc_Value (*try)(struct io_libecc_Context* const);
    struct io_libecc_Value (*throw)(struct io_libecc_Context* const);
    struct io_libecc_Value (*with)(struct io_libecc_Context* const);
    struct io_libecc_Value (*next)(struct io_libecc_Context* const);
    struct io_libecc_Value (*nextIf)(struct io_libecc_Context* const);
    struct io_libecc_Value (*autoreleaseExpression)(struct io_libecc_Context* const);
    struct io_libecc_Value (*autoreleaseDiscard)(struct io_libecc_Context* const);
    struct io_libecc_Value (*expression)(struct io_libecc_Context* const);
    struct io_libecc_Value (*discard)(struct io_libecc_Context* const);
    struct io_libecc_Value (*discardN)(struct io_libecc_Context* const);
    struct io_libecc_Value (*jump)(struct io_libecc_Context* const);
    struct io_libecc_Value (*jumpIf)(struct io_libecc_Context* const);
    struct io_libecc_Value (*jumpIfNot)(struct io_libecc_Context* const);
    struct io_libecc_Value (*repopulate)(struct io_libecc_Context* const);
    struct io_libecc_Value (*result)(struct io_libecc_Context* const);
    struct io_libecc_Value (*resultVoid)(struct io_libecc_Context* const);
    struct io_libecc_Value (*switchOp)(struct io_libecc_Context* const);
    struct io_libecc_Value (*breaker)(struct io_libecc_Context* const);
    struct io_libecc_Value (*iterate)(struct io_libecc_Context* const);
    struct io_libecc_Value (*iterateLessRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*iterateMoreRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*iterateLessOrEqualRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*iterateMoreOrEqualRef)(struct io_libecc_Context* const);
    struct io_libecc_Value (*iterateInRef)(struct io_libecc_Context* const);
    const struct io_libecc_Op identity;
} extern const io_libecc_Op;
