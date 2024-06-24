//
//  global.h
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#pragma once
#include "compatibility.h"
#include "value.h"
#include "context.h"
#include "chars.h"

struct io_libecc_object_Type
{
    const struct io_libecc_Text* text;

    void (*mark)(struct io_libecc_Object*);
    void (*capture)(struct io_libecc_Object*);
    void (*finalize)(struct io_libecc_Object*);
};

enum io_libecc_object_Flags
{
    io_libecc_object_mark = 1 << 0,
    io_libecc_object_sealed = 1 << 1,
};

extern struct io_libecc_Object* io_libecc_object_prototype;
extern struct io_libecc_Function* io_libecc_object_constructor;
extern const struct io_libecc_object_Type io_libecc_object_type;

extern const uint32_t io_libecc_object_ElementMax;

struct io_libecc_Object
{
    struct io_libecc_Object* prototype;
    const struct io_libecc_object_Type* type;

    union io_libecc_object_Element
    {
        struct io_libecc_Value value;
    }* element;

    union io_libecc_object_Hashmap
    {
        struct io_libecc_Value value;
        uint16_t slot[16];
    }* hashmap;

    uint32_t elementCount;
    uint32_t elementCapacity;
    uint16_t hashmapCount;
    uint16_t hashmapCapacity;
    int16_t referenceCount;
    uint8_t flags;
};

struct type_io_libecc_Object
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Object* (*create)(struct io_libecc_Object* prototype);
    struct io_libecc_Object* (*createSized)(struct io_libecc_Object* prototype, uint16_t size);
    struct io_libecc_Object* (*createTyped)(const struct io_libecc_object_Type* type);
    struct io_libecc_Object* (*initialize)(struct io_libecc_Object* restrict, struct io_libecc_Object* restrict prototype);
    struct io_libecc_Object* (*initializeSized)(struct io_libecc_Object* restrict, struct io_libecc_Object* restrict prototype, uint16_t size);
    struct io_libecc_Object* (*finalize)(struct io_libecc_Object*);
    struct io_libecc_Object* (*copy)(const struct io_libecc_Object* original);
    void (*destroy)(struct io_libecc_Object*);
    struct io_libecc_Value (*getMember)(struct io_libecc_Context* const, struct io_libecc_Object*, struct io_libecc_Key key);
    struct io_libecc_Value (*putMember)(struct io_libecc_Context* const, struct io_libecc_Object*, struct io_libecc_Key key, struct io_libecc_Value);
    struct io_libecc_Value* (*member)(struct io_libecc_Object*, struct io_libecc_Key key, enum io_libecc_value_Flags);
    struct io_libecc_Value* (*addMember)(struct io_libecc_Object*, struct io_libecc_Key key, struct io_libecc_Value, enum io_libecc_value_Flags);
    int (*deleteMember)(struct io_libecc_Object*, struct io_libecc_Key key);
    struct io_libecc_Value (*getElement)(struct io_libecc_Context* const, struct io_libecc_Object*, uint32_t index);
    struct io_libecc_Value (*putElement)(struct io_libecc_Context* const, struct io_libecc_Object*, uint32_t index, struct io_libecc_Value);
    struct io_libecc_Value* (*element)(struct io_libecc_Object*, uint32_t index, enum io_libecc_value_Flags);
    struct io_libecc_Value* (*addElement)(struct io_libecc_Object*, uint32_t index, struct io_libecc_Value, enum io_libecc_value_Flags);
    int (*deleteElement)(struct io_libecc_Object*, uint32_t index);
    struct io_libecc_Value (*getProperty)(struct io_libecc_Context* const, struct io_libecc_Object*, struct io_libecc_Value primitive);
    struct io_libecc_Value (*putProperty)(struct io_libecc_Context* const, struct io_libecc_Object*, struct io_libecc_Value primitive, struct io_libecc_Value);
    struct io_libecc_Value* (*property)(struct io_libecc_Object*, struct io_libecc_Value primitive, enum io_libecc_value_Flags);
    struct io_libecc_Value* (*addProperty)(struct io_libecc_Object*, struct io_libecc_Value primitive, struct io_libecc_Value, enum io_libecc_value_Flags);
    int (*deleteProperty)(struct io_libecc_Object*, struct io_libecc_Value primitive);
    struct io_libecc_Value (*putValue)(struct io_libecc_Context* const, struct io_libecc_Object*, struct io_libecc_Value*, struct io_libecc_Value);
    struct io_libecc_Value (*getValue)(struct io_libecc_Context* const, struct io_libecc_Object*, struct io_libecc_Value*);
    void (*packValue)(struct io_libecc_Object*);
    void (*stripMap)(struct io_libecc_Object*);
    void (*reserveSlots)(struct io_libecc_Object*, uint16_t slots);
    int (*resizeElement)(struct io_libecc_Object*, uint32_t size);
    void (*populateElementWithCList)(struct io_libecc_Object*, uint32_t count, const char* list[]);
    struct io_libecc_Value (*toString)(struct io_libecc_Context* const);
    void (*dumpTo)(struct io_libecc_Object*, FILE* file);
    const struct io_libecc_Object identity;
} extern const io_libecc_Object;

extern struct io_libecc_Object* io_libecc_regexp_prototype;
extern struct io_libecc_Function* io_libecc_regexp_constructor;
extern const struct io_libecc_object_Type io_libecc_regexp_type;

struct io_libecc_regexp_State
{
    const char* const start;
    const char* const end;
    const char** capture;
    const char** index;
    int flags;
};

struct io_libecc_regexp_Node;

enum io_libecc_regexp_Options
{
    io_libecc_regexp_allowUnicodeFlags = 1 << 0,
};

struct io_libecc_RegExp
{
    struct io_libecc_Object object;
    struct io_libecc_Chars* pattern;
    struct io_libecc_Chars* source;
    struct io_libecc_regexp_Node* program;
    uint8_t count;
    uint8_t global : 1;
    uint8_t ignoreCase : 1;
    uint8_t multiline : 1;
};

struct type_io_libecc_RegExp
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_RegExp* (*create)(struct io_libecc_Chars* pattern, struct io_libecc_Error**, enum io_libecc_regexp_Options);
    struct io_libecc_RegExp* (*createWith)(struct io_libecc_Context* context, struct io_libecc_Value pattern, struct io_libecc_Value flags);
    int (*matchWithState)(struct io_libecc_RegExp*, struct io_libecc_regexp_State*);
    const struct io_libecc_RegExp identity;
} extern const io_libecc_RegExp;


extern struct io_libecc_Object* io_libecc_arguments_prototype;
extern const struct io_libecc_object_Type io_libecc_arguments_type;

struct io_libecc_Arguments
{
    struct io_libecc_Object object;
};

struct type_io_libecc_Arguments
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Object* (*createSized)(uint32_t size);
    struct io_libecc_Object* (*createWithCList)(int count, const char* list[]);
    const struct io_libecc_Arguments identity;
} extern const io_libecc_Arguments;

extern struct io_libecc_Object* io_libecc_array_prototype;
extern struct io_libecc_Function* io_libecc_array_constructor;
extern const struct io_libecc_object_Type io_libecc_array_type;

struct io_libecc_Array
{
    struct io_libecc_Object object;
};

struct type_io_libecc_Array
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Object* (*create)(void);
    struct io_libecc_Object* (*createSized)(uint32_t size);
    const struct io_libecc_Array identity;
} extern const io_libecc_Array;


/* --insertbelow-- */


extern struct io_libecc_Object* io_libecc_boolean_prototype;
extern struct io_libecc_Function* io_libecc_boolean_constructor;
extern const struct io_libecc_object_Type io_libecc_boolean_type;

struct io_libecc_Boolean
{
    struct io_libecc_Object object;
    int truth;
};

struct type_io_libecc_Boolean
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Boolean* (*create)(int);
    const struct io_libecc_Boolean identity;
} extern const io_libecc_Boolean;


extern struct io_libecc_Object* io_libecc_date_prototype;
extern struct io_libecc_Function* io_libecc_date_constructor;
extern const struct io_libecc_object_Type io_libecc_date_type;

struct io_libecc_Date
{
    struct io_libecc_Object object;
    double ms;
};

struct type_io_libecc_Date
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Date* (*create)(double);
    const struct io_libecc_Date identity;
} extern const io_libecc_Date;

extern struct io_libecc_Object* io_libecc_error_prototype;
extern struct io_libecc_Object* io_libecc_error_rangePrototype;
extern struct io_libecc_Object* io_libecc_error_referencePrototype;
extern struct io_libecc_Object* io_libecc_error_syntaxPrototype;
extern struct io_libecc_Object* io_libecc_error_typePrototype;
extern struct io_libecc_Object* io_libecc_error_uriPrototype;
extern struct io_libecc_Object* io_libecc_error_evalPrototype;

extern struct io_libecc_Function* io_libecc_error_constructor;
extern struct io_libecc_Function* io_libecc_error_rangeConstructor;
extern struct io_libecc_Function* io_libecc_error_referenceConstructor;
extern struct io_libecc_Function* io_libecc_error_syntaxConstructor;
extern struct io_libecc_Function* io_libecc_error_typeConstructor;
extern struct io_libecc_Function* io_libecc_error_uriConstructor;
extern struct io_libecc_Function* io_libecc_error_evalConstructor;

extern const struct io_libecc_object_Type io_libecc_error_type;

struct io_libecc_Error
{
    struct io_libecc_Object object;
    struct io_libecc_Text text;
};

struct type_io_libecc_Error
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Error* (*error)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*rangeError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*referenceError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*syntaxError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*typeError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    struct io_libecc_Error* (*uriError)(struct io_libecc_Text, struct io_libecc_Chars* message);
    void (*destroy)(struct io_libecc_Error*);
    const struct io_libecc_Error identity;
} extern const io_libecc_Error;


struct io_libecc_Value;
struct io_libecc_Context;

typedef struct io_libecc_Value (*io_libecc_native_io_libecc_Function)(struct io_libecc_Context* const context);

enum io_libecc_function_Flags
{
    io_libecc_function_needHeap = 1 << 1,
    io_libecc_function_needArguments = 1 << 2,
    io_libecc_function_useBoundThis = 1 << 3,
    io_libecc_function_strictMode = 1 << 4,
};

extern struct io_libecc_Object* io_libecc_function_prototype;
extern struct io_libecc_Function* io_libecc_function_constructor;
extern const struct io_libecc_object_Type io_libecc_function_type;

struct io_libecc_Function
{
    struct io_libecc_Object object;
    struct io_libecc_Object environment;
    struct io_libecc_Object* refObject;
    struct io_libecc_OpList* oplist;
    struct io_libecc_Function* pair;
    struct io_libecc_Value boundThis;
    struct io_libecc_Text text;
    const char* name;
    int parameterCount;
    enum io_libecc_function_Flags flags;
};

struct type_io_libecc_Function
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Function* (*create)(struct io_libecc_Object* environment);
    struct io_libecc_Function* (*createSized)(struct io_libecc_Object* environment, uint32_t size);
    struct io_libecc_Function* (*createWithNative)(const io_libecc_native_io_libecc_Function native, int parameterCount);
    struct io_libecc_Function* (*copy)(struct io_libecc_Function* original);
    void (*destroy)(struct io_libecc_Function*);
    void (*addMember)(struct io_libecc_Function*, const char* name, struct io_libecc_Value value, enum io_libecc_value_Flags);
    void (*addValue)(struct io_libecc_Function*, const char* name, struct io_libecc_Value value, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addMethod)(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addFunction)(struct io_libecc_Function*, const char* name, const io_libecc_native_io_libecc_Function native, int argumentCount, enum io_libecc_value_Flags);
    struct io_libecc_Function* (*addToObject)(struct io_libecc_Object* object, const char* name, const io_libecc_native_io_libecc_Function native, int parameterCount, enum io_libecc_value_Flags);
    void (*linkPrototype)(struct io_libecc_Function*, struct io_libecc_Value prototype, enum io_libecc_value_Flags);
    void (*setupBuiltinObject)(struct io_libecc_Function**,
                               const io_libecc_native_io_libecc_Function,
                               int parameterCount,
                               struct io_libecc_Object**,
                               struct io_libecc_Value prototype,
                               const struct io_libecc_object_Type* type);
    struct io_libecc_Value (*accessor)(const io_libecc_native_io_libecc_Function getter, const io_libecc_native_io_libecc_Function setter);
    const struct io_libecc_Function identity;
} extern const io_libecc_Function;


// ----here


extern struct io_libecc_Object* io_libecc_json_object;
extern const struct io_libecc_object_Type io_libecc_json_type;

struct io_libecc_JSON
{
    char empty;
};

struct type_io_libecc_JSON
{
    void (*setup)(void);
    void (*teardown)(void);
    const struct io_libecc_JSON identity;
} extern const io_libecc_JSON;

extern struct io_libecc_Object* io_libecc_math_object;
extern const struct io_libecc_object_Type io_libecc_math_type;

struct io_libecc_Math
{
    char empty;
};

struct type_io_libecc_Math
{
    void (*setup)(void);
    void (*teardown)(void);
    const struct io_libecc_Math identity;
} extern const io_libecc_Math;

extern struct io_libecc_Object* io_libecc_number_prototype;
extern struct io_libecc_Function* io_libecc_number_constructor;
extern const struct io_libecc_object_Type io_libecc_number_type;

struct io_libecc_Number
{
    struct io_libecc_Object object;
    double value;
};

struct type_io_libecc_Number
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Number* (*create)(double);
    const struct io_libecc_Number identity;
} extern const io_libecc_Number;

extern struct io_libecc_Object* io_libecc_string_prototype;
extern struct io_libecc_Function* io_libecc_string_constructor;
extern const struct io_libecc_object_Type io_libecc_string_type;

struct io_libecc_String
{
    struct io_libecc_Object object;
    struct io_libecc_Chars* value;
};

struct type_io_libecc_String
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_String* (*create)(struct io_libecc_Chars*);
    struct io_libecc_Value (*valueAtIndex)(struct io_libecc_String*, int32_t index);
    struct io_libecc_Text (*textAtIndex)(const char* chars, int32_t length, int32_t index, int enableReverse);
    int32_t (*unitIndex)(const char* chars, int32_t max, int32_t unit);
    const struct io_libecc_String identity;
} extern const io_libecc_String;

extern const struct io_libecc_object_Type io_libecc_global_type;

extern const struct io_libecc_object_Type io_libecc_global_type;

struct io_libecc_Global
{
    char empty;
};

struct type_io_libecc_Global
{
    void (*setup)(void);
    void (*teardown)(void);
    struct io_libecc_Function* (*create)(void);
    const struct io_libecc_Global identity;
} extern const io_libecc_Global;

