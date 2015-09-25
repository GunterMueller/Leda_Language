/// Copyright 1993-2015 Timothy A. Budd
// -----------------------------------------------------------------------------
//  This file is part of
/// ---     Leda: Multiparadigm Programming Language
// -----------------------------------------------------------------------------
//
//  Leda is free software: you can redistribute it and/or modify it under the
//  terms of the MIT license, see file "COPYING" included in this distribution.
//
// -----------------------------------------------------------------------------
/// Title: Leda interpreter
///  Description:
//    revised to use new garbage collection algorithm
// -----------------------------------------------------------------------------

#include "lc.h"
#include "interp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

// -----------------------------------------------------------------------------
///  Globals used within the interpreter
// -----------------------------------------------------------------------------

int displayFunctions = 0;
int displayStatements = 0;
int displayOperators = 0;

static struct ledaValue* integerClass;
static struct ledaValue* realClass;
static struct ledaValue* stringClass;
static struct ledaValue* trueObject;
static struct ledaValue* trueClass;
static struct ledaValue* falseObject;
static struct ledaValue* falseClass;

extern int      linenumber;
extern char*    fileName;

static int doingInitialization = 1;

// -----------------------------------------------------------------------------
///  Building initial table
// -----------------------------------------------------------------------------

void buildClassTable(struct symbolRecord* sym)
{
    if (sym->styp != classDefSymbol)
    {
        yyerror("build table on non class def");
    }

    struct typeRecord* t = sym->u.c.typ;
    if (t->ttyp == qualifiedType)
    {
        t = t->u.q.baseType;
    }

    if (t->ttyp != classType)
    {
        yyerror("build table on non class type");
    }

    struct symbolTableRecord* csyms = t->u.c.symbols;
    if (csyms->ttype != classTable)
    {
        yyerror("build table on non class form");
    }

    struct ledaValue* theTable = staticAllocate(csyms->u.c.methodTableSize);
    if (displayOperators)
    {
        printf("class table for %s is %p\n", sym->name, theTable);
        printf("class type is %p\n", sym->u.c.typ);
    }

    for (struct list* p = csyms->u.c.methodTable; p; p = p->next)
    {
        struct symbolRecord* s = (struct symbolRecord*) p->value;
        switch(s->styp)
        {
            case functionSymbol:
                theTable->data[s->u.f.location] =
                    (struct ledaValue*) s->u.f.code;
                break;
            default:
                yyerror("compiler error -- unknown value in class table");
        }
    }

    // All done now, fill in static table
    t->u.c.staticTable = theTable;
}


// -----------------------------------------------------------------------------
///  Evaluate expressions
// -----------------------------------------------------------------------------

char* specialFunctionNames[] =
{
    "Leda_object_equals",   // 0
    "Leda_string_compare",  // 1
    "Leda_string_print",    // 2
    "Leda_string_concat",   // 3
    "Leda_integer_equals",  // 4
    "Leda_integer_plus",    // 5
    "Leda_integer_minus",   // 6
    "Leda_integer_times",   // 7
    "Leda_integer_divide",  // 8
    "Leda_integer_asString",// 9
    "Leda_integer_less",    // 10
    "Leda_integer_or",      // 11
    "Leda_integer_and",     // 12
    "Leda_integer_not",     // 13
    "Leda_integer_asReal",  // 14
    "Leda_object_allocate", // 15
    "Leda_object_at",       // 16
    "Leda_object_atPut",    // 17
    "Leda_object_cast",     // 18
    "Leda_string_length",   // 19
    "Leda_string_substring",// 20
    "Leda_stdin_read",      // 21
    "Leda_object_defined",  // 22
    "Leda_real_asString",   // 23
    "Leda_real_plus",       // 24
    "Leda_real_minus",      // 25
    "Leda_real_times",      // 26
    "Leda_real_divide",     // 27
    "Leda_real_less",       // 28
    "Leda_real_asInteger",  // 29
    "Leda_real_equals",     // 30
    0
};

static void undefCheck(int x, struct ledaValue* arg, char* s)
{
    if (arg)    // If non-null, then ok
    {
        return;
    }

    if (displayOperators)
    {
        fprintf(stderr, "undef check number %d\n", x);
    }
    fprintf(stderr, "undefined value used, File %s Line %d",
    fileName, linenumber);
    if (s) fprintf(stderr,": %s\n", s);
    fprintf(stderr,"\n");

    exit(1);
}


static struct ledaValue* binaryValue(int i)
{
    // Used both for integers and for references
    struct ledaValue* result;
    if (doingInitialization)
    {
        result = staticAllocate(2);
    }
    else
    {
        result = gcalloc(2);
    }
    result->size = 10; // 2 << 2 + 02
    int *ip = (int*) &result->data[2];
    *ip = i;

    return result;
}


// Keeping common integers in table reduces allocations,
// but depends upon fact that integers are not relocated
// during GC
static struct ledaValue* integerTable[20] =
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static struct ledaValue* newIntegerConstant(int i)
{
    // if common number, then just do it
    if ((i >= 0) && (i < 20) && integerTable[i])
    {
        return integerTable[i];
    }

    // otherwise we have to build the value
    struct ledaValue* result = binaryValue(i);
    result->data[0] = integerClass;
    result->data[1] = globalContext;

    return result;
}


// In order to avoid allignment problems on some machines,
// only single precision floating point values are used
static struct ledaValue* newRealConstant(float r)
{
    struct ledaValue* result = gcalloc(2);
    result->size |= 02; // turn on binary flag
    result->data[0] = realClass;
    result->data[1] = globalContext;
    float* dp = (float*) & result->data[2];
    *dp = r;
    return result;
}


static float realValue(struct ledaValue* d)
{
    float* pr = (float*) &d->data[2];
    return *pr;
}


static struct ledaValue* newStringConstant(char* p)
{
    struct ledaValue* result;
    if (doingInitialization)
    {
        result = staticAllocate(3);
    }
    else
    {
        result = gcalloc(3);
    }
    result->size = 10; // 2 << 2 + 02
    result->data[0] = stringClass;
    result->data[1] = globalContext;
    result->data[2] = (struct ledaValue*) p;

    return result;
}

static struct ledaValue* evaluateStatement(struct statementRecord* s);
static struct ledaValue* evaluateExpression(struct expressionRecord* e);

static struct ledaValue* evaluateSpecial
(
    int index,
    struct list* args
)
{
    struct ledaValue* result = 0;
    struct ledaValue* a;
    struct ledaValue* b;
    float r1;

    switch(index) {
        case 0:     // object equality
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            if (a == b)
                result = trueObject;
            else
                result = falseObject;
            break;

        case 1:     // string compare
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            result = newIntegerConstant
            (
                strcmp((char*) a->data[2],
                (char*) b->data[2])
            );
            break;

        case 2:     // string print
            result = evaluateExpression((struct expressionRecord*) args->value);
            printf("%s", (char*)result->data[2]);
            result = 0;
            break;

        case 3:     // string concat
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            {
                char* buffer =
                    (char*)malloc
                    (
                        strlen((char*) a->data[2])
                      + strlen((char*) b->data[2])
                      + 1
                    );
                if (buffer == 0)
                {
                    yyerror("out of memory");
                }
                strcpy(buffer, (char*) a->data[2]);
                strcat(buffer, (char*) b->data[2]);
                result = newStringConstant(buffer);
            }
            break;

        case 4:     // integer equals
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            if (((int) a->data[2]) == ((int) b->data[2]))
            {
                result = trueObject;
            }
            else
            {
                result = falseObject;
            }
            break;

        case 5:     // integer add
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            result = newIntegerConstant
            (
                ((int) a->data[2]) + ((int) b->data[2])
            );
            break;

        case 6:     // integer minus
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            result = newIntegerConstant
            (
                ((int) a->data[2]) - ((int) b->data[2])
            );
            break;

        case 7:     // integer times
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            result = newIntegerConstant
            (
                ((int) a->data[2]) * ((int) b->data[2])
            );
            break;

        case 8:     // integer division
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            result = newIntegerConstant
            (
                ((int) a->data[2])/((int) b->data[2])
            );
            break;

        case 9:     // integer as string
            { char buffer[40];
                result = evaluateExpression(
                    (struct expressionRecord*) args->value);
                sprintf(buffer,"%d", (int)result->data[2]);
                result = newStringConstant(newString(buffer));
            }
            break;

        case 10:    // integer less
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            if (((int) a->data[2]) < ((int) b->data[2]))
            {
                result = trueObject;
            }
            else
            {
                result = falseObject;
            }
            break;

        case 11:    // integer or
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            result = newIntegerConstant
            (
                ((int) a->data[2]) | ((int) b->data[2])
            );
            break;

        case 12:    // integer and
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            a = rootStack[--rootTop];
            result = newIntegerConstant
            (
                ((int) a->data[2]) & ((int) b->data[2])
            );
            break;

        case 13:    // integer invert
            a = evaluateExpression((struct expressionRecord*) args->value);
            result = newIntegerConstant(~ (int) a->data[2]);
            break;

        case 14:        // integer as real
            a = evaluateExpression((struct expressionRecord*) args->value);
            result = newRealConstant((float) (int) a->data[2]);
            break;

        case 15:        // allocate new object
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            args = args->next;
            result = gcalloc((int) a->data[2]);
            a = rootStack[--rootTop];
            // now fill in any argument values
            {
                int i = 0;
                while (args != 0)
                {
                    a = evaluateExpression
                    (
                        (struct expressionRecord*) args->value
                    );
                    result->data[i++] = a;
                    args = args->next;
                }
            }
            break;

        case 16:        // index at
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            undefCheck(1, a, "subscript base");
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            undefCheck(2, b, "subscript index");
            a = rootStack[--rootTop];
            result = a->data[(int) b->data[2]];
            break;

        case 17:    // index at put
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            a = evaluateExpression((struct expressionRecord*) args->value);
            undefCheck(3, a, "subscript base");
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            undefCheck(4, b, "subscript index");
            rootStack[rootTop++] = b;
            {
                struct ledaValue* c;
                args = args->next;
                c = evaluateExpression((struct expressionRecord*) args->value);
                b = rootStack[--rootTop];
                a = rootStack[--rootTop];
                a->data[(int) b->data[2]] = c;
            }
            result = 0;
            break;

        case 18:    // just evaluate value
            result = evaluateExpression((struct expressionRecord*) args->value);
            break;

        case 19:    // string length
            a = evaluateExpression((struct expressionRecord*) args->value);
            undefCheck(5, a, "string length");
            result = newIntegerConstant(strlen((char*) a->data[2]));
            break;

        case 20:    // string substring
            a = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = a;
            undefCheck(6, a, "substring base");
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            rootStack[rootTop++] = b;
            undefCheck(7, b, "substring start");
            {
                args = args->next;
                struct ledaValue* c = evaluateExpression
                (
                    (struct expressionRecord*) args->value
                );
                undefCheck(8, c, "substring length");
                char* buffer = (char*) malloc(1 + (int) c->data[2]);
                char* p = buffer;
                if (buffer == 0)
                {
                    yyerror("out of memory");
                }
                b = rootStack[--rootTop];
                a = rootStack[--rootTop];
                char* q = (char*) a->data[2];
                int i;
                for (q = &q[(int) b->data[2]], i = (int) c->data[2]; i > 0; i--)
                {
                    *p++ = *q++;
                }
                *p = '\0';
                result = newStringConstant(buffer);
            }
            break;

        case 21:    // stdin read
            {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), stdin) == 0)
                {
                    result = 0;
                }
                else
                {
                    result = newStringConstant(newString(buffer));
                }
            }
            break;

        case 22:    // is defined
            a = evaluateExpression((struct expressionRecord*) args->value);
            if (a)
                result = trueObject;
            else
                result = falseObject;
            break;


        case 23:    // real as string
            {
                char buffer[40];
                result = evaluateExpression
                (
                    (struct expressionRecord*) args->value
                );
                sprintf(buffer,"%g",realValue(result));
                result = newStringConstant(newString(buffer));
            }
            break;

        case 24:    // real addition
            a = evaluateExpression((struct expressionRecord*) args->value);
            r1 = realValue(a);
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            result = newRealConstant(r1 + realValue(b));
            break;

        case 25:    // real subtraction
            a = evaluateExpression((struct expressionRecord*) args->value);
            r1 = realValue(a);
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            result = newRealConstant(r1 - realValue(b));
            break;

        case 26:    // real multiplication
            a = evaluateExpression((struct expressionRecord*) args->value);
            r1 = realValue(a);
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            result = newRealConstant(r1 * realValue(b));
            break;


        case 27:    // real division
            a = evaluateExpression((struct expressionRecord*) args->value);
            r1 = realValue(a);
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            result = newRealConstant(r1 / realValue(b));
            break;

        case 28:    // real comparison
            a = evaluateExpression((struct expressionRecord*) args->value);
            r1 = realValue(a);
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            if (r1 < realValue(b))
            {
                result = trueObject;
            }
            else
            {
                result = falseObject;
            }
            break;

        case 29:    // real as integer
            a = evaluateExpression((struct expressionRecord*) args->value);
            result = newIntegerConstant((int) realValue(a));
            break;

        case 30:    // real equality
            a = evaluateExpression((struct expressionRecord*) args->value);
            r1 = realValue(a);
            args = args->next;
            b = evaluateExpression((struct expressionRecord*) args->value);
            if (r1 == realValue(b))
            {
                result = trueObject;
            }
            else
            {
                result = falseObject;
            }
            break;

        default:
            printf("unimplemented special %d\n", index);
            exit(1);
            break;
    }

    return result;
}


static struct ledaValue* evaluateExpression
(
    struct expressionRecord* e
)
{
    register struct ledaValue* result;
    register struct ledaValue* arg;

    if (e == 0)
    {
        yyerror("internal run-time error: null expression");
    }

    switch(e->operator)
    {

        case getCurrentContext:
            if (displayOperators)
            {
                printf("getCurrentContext yields %p\n", currentContext);
            }
            return currentContext;
            break;

        case getOffset:
            if (e->u.o.base->operator == getCurrentContext)
            {
                arg = currentContext;
            }
            else
            {
                arg = evaluateExpression(e->u.o.base);
                undefCheck(9, arg, e->u.o.symbol);
            }
            result = arg->data[e->u.o.location];
            if (displayOperators)
            {
                printf
                (
                    "getOffset %d from %p yields %p\n",
                    e->u.o.location, arg, result
                );
            }
            break;

        case getGlobalOffset:
            if (displayOperators)
            {
                printf("get global offset %d\n", e->u.o.location);
            }
            result = globalContext->data[e->u.o.location];
            break;

        case makeReference:
            arg = evaluateExpression(e->u.o.base);
            undefCheck(10, arg, e->u.a.symbol);
            rootStack[rootTop++] = arg;
            result = binaryValue(e->u.o.location);
            arg = rootStack[--rootTop];
            result->data[0] = arg;
            break;

        case assignment:
            if (e->u.a.left->operator == makeReference)
            {
                if
                (
                    e->u.a.left->u.o.base->operator == getCurrentContext
                )
                {
                    arg = currentContext;
                }
                else
                {
                    arg = evaluateExpression(e->u.a.left->u.o.base);
                    undefCheck(11, arg,
                    e->u.a.left->u.a.symbol);
                }
                rootStack[rootTop++] = arg;
                result = evaluateExpression(e->u.a.right);
                arg = rootStack[--rootTop];
                arg->data[e->u.a.left->u.o.location] = result;
            }
            else
            {
                arg = evaluateExpression(e->u.a.left);
                rootStack[rootTop++] = arg;
                result = evaluateExpression(e->u.a.right);
                arg = rootStack[--rootTop];
                arg->data[0]->data[(int) arg->data[2]] = result;
            }
            if (displayOperators)
            {
                printf("assignment gets %p\n", result);
            }
            result = 0;
            break;


        case makeMethodContext:
            // get the context
            arg = evaluateExpression(e->u.o.base);
            rootStack[rootTop++] = arg;
            result = gcalloc(3);
            arg = rootStack[--rootTop];
            undefCheck(12, arg, e->u.o.symbol);
            undefCheck(13, arg->data[0], "method table");
            if (displayOperators)
            {
                printf
                (
                    "make method %d context %p code\n",
                    e->u.o.location, arg
                    //(arg->data[0])[e->u.o.location]
                );
                printf("method table %p\n", arg->data[0]);
            }
            result->data[1] = arg;
            result->data[2] = (arg->data[0])->data[e->u.o.location];
            break;

        case makeClosure:
            result = gcalloc(2);
            if (e->u.l.context->operator == getCurrentContext)
            {
                arg = currentContext;
            }
            else
            {
                rootStack[rootTop++] = result;
                arg = evaluateExpression(e->u.l.context);
                result = rootStack[--rootTop];
                undefCheck(14, arg, "<context>");
            }
            if (displayOperators)
            {
                printf
                (
                    "make closure %p, context = %p code = %p\n",
                    result, arg, e->u.l.code
                );
            }
            result->data[1] = arg;
            result->data[2] = (struct ledaValue*) e->u.l.code;
            break;

        case doFunctionCall:
            {
                struct ledaValue* code;
                struct ledaValue* context;
                struct ledaValue* newContext;

                char* functionName = e->u.f.symbol;
                if (displayOperators)
                {
                    printf("beginning function call operator\n");
                }

                if (functionName == 0)
                {
                    functionName = " ? ";
                }

                if (e->u.f.fun->operator == makeClosure)
                {
                    if (e->u.f.fun->u.l.context->operator == getCurrentContext)
                    {
                        context = currentContext;
                    }
                    else
                    {
                        context = evaluateExpression(e->u.f.fun->u.l.context);
                        undefCheck(15, context,"context");
                    }
                    code = (struct ledaValue*)e->u.f.fun->u.l.code;
                }
                else if (e->u.f.fun->operator == makeMethodContext)
                {
                    context = evaluateExpression(e->u.f.fun->u.o.base);
                    undefCheck(16, context,"context");
                    undefCheck(17, context->data[0],"method table");
                    code = (context->data[0])->data
                    [e->u.f.fun->u.o.location];
                }
                else
                {
                    arg = evaluateExpression(e->u.f.fun);
                    undefCheck(18, arg, e->u.f.symbol);
                    context = arg->data[1];
                    code = arg->data[2];
                }

                // create the activation record
                struct list* p = e->u.f.args;

                rootStack[rootTop++] = context;
                newContext = gcalloc(length(p) + 4);
                context = rootStack[--rootTop];
                if (displayOperators)
                {
                    printf
                    (
                        "do function call (%p) fun context %p, "
                        "new context %p args:\n",
                        currentContext, context, newContext
                    );
                }
                newContext->data[1] = context;
                newContext->data[2] = currentContext;
                for (int i=4 ; p ; i++,p = p->next)
                {
                    // tricky gc insurance
                    rootStack[rootTop++] = newContext;
                    arg = evaluateExpression
                    (
                        (struct expressionRecord*) p->value
                    );
                    newContext = rootStack[--rootTop];
                    newContext->data[i] = arg;
                    if (displayOperators)
                    {
                        printf
                        (
                            "argument %d in %p is %p\n",
                            i, newContext, newContext->data[i]
                        );
                    }
                }
                if (displayFunctions)
                {
                    printf
                    (
                        "do function (%p) call %s(%p), now do call\n",
                        currentContext, functionName, code
                    );
                }
                currentContext = newContext;
                result = evaluateStatement((struct statementRecord*) code);
                currentContext = currentContext->data[2];
                if (displayFunctions)
                {
                    printf("return from function %s(%p)\n",
                    functionName, code);
                }
            }
            break;

        case evalThunk:
            // get the context
            arg = evaluateExpression(e->u.o.base);
            undefCheck(19, arg, "thunk");
            if (displayOperators)
            {
                printf("evaluate thunk\n");
            }

            // then evaluate the statement
            rootStack[rootTop++] = currentContext;
            currentContext = arg->data[1];
            result = evaluateStatement((struct statementRecord*) arg->data[2]);
            currentContext = rootStack[--rootTop];
            break;

        case evalReference:
            arg = evaluateExpression(e->u.o.base);
            if (displayOperators)
            {
                printf("evaluate reference\n");
            }
            result = arg->data[0]->data[(int) arg->data[2]];
            break;

        case genIntegerConstant:
            result = newIntegerConstant(e->u.i.value);
            if (displayOperators)
            {
                printf("make integer constant %d\n", e->u.i.value);
            }
            break;

        case genStringConstant:
            result = newStringConstant(e->u.s.value);
            if (displayOperators)
            {
                printf("make string constant %p %s\n", result, e->u.s.value);
            }
            break;

        case genRealConstant:
            result = newRealConstant(e->u.r.value);
            if (displayOperators)
            {
                printf("make real constant %g\n", e->u.r.value);
            }
            break;

        case doSpecialCall:
            if (displayOperators)
            {
                printf("do special operator %d\n", e->u.c.index);
            }
            result = evaluateSpecial(e->u.c.index, e->u.c.args);
            break;

        case buildInstance:
            arg = evaluateExpression(e->u.n.table);
            rootStack[rootTop++] = arg;
            result = gcalloc(e->u.n.size);
            arg = rootStack[--rootTop];
            undefCheck(20, arg, "build instance table");
            result->data[0] = arg;
            result->data[1] = globalContext;
            if (displayOperators)
            {
                printf
                (
                    "build an instance %p, size %d table %p\n",
                    result, e->u.n.size, result->data[0]
                );
            }
            {
                int max = e->u.n.size;
                struct list* p = e->u.n.args;
                int i = 2;
                for (; p; i++, p = p->next)
                {
                    // Tricky gc insurance
                    if (i > max)
                    {
                        yyerror("filling instance too big\n");
                    }
                    rootStack[rootTop++] = result;
                    arg = evaluateExpression
                    (
                        (struct expressionRecord*)p->value
                    );
                    result = rootStack[--rootTop];
                    result->data[i] = arg;
                    if (displayOperators)
                    {
                        printf
                        (
                            "in instance %p location %d is %p\n",
                            result, i, result->data[i]
                        );
                    }
                }
            }
            break;

        case commaOp:
            arg = evaluateExpression(e->u.a.left);
            // toss away arg
            result = evaluateExpression(e->u.a.right);
            break;

        case patternMatch:
            {
                struct ledaValue* b = evaluateExpression(e->u.p.base);
                undefCheck(30, b, "pattern base");
                rootStack[rootTop++] = b;
                struct ledaValue* a = evaluateExpression(e->u.p.class);
                undefCheck(30, a, "pattern class");
                b = rootStack[--rootTop];
                // get the class of the value
                arg = b->data[0];
                result = falseObject;
                while (1)
                {
                    if (a == arg)
                    {
                        result = trueObject;
                        struct list* p = e->u.p.args;
                        int i = 2;
                        while (p)
                        {
                            rootStack[rootTop++] = a;
                            rootStack[rootTop++] = b;
                            arg = evaluateExpression
                            (
                                (struct expressionRecord*) p->value
                            );
                            b = rootStack[--rootTop];
                            a = rootStack[--rootTop];
                            arg->data[0]->data[(int) arg->data[2]] =
                                b->data[i++];
                            p = p->next;
                        }
                        break;
                    }
                    if (arg == arg->data[4]) break;
                    arg = arg->data[4];
                }
            }
            break;

        default:
            printf("unimplemented expression type %d\n", e->operator);
            exit(1);
    }
    return result;
}


// -----------------------------------------------------------------------------
///  Evaluate statements
// -----------------------------------------------------------------------------

static struct ledaValue* evaluateStatement
(
    struct statementRecord* st
)
{
    register struct statementRecord* s = st;

    if (s == 0)
    {
        yyerror("internal run-time error: empty statement");
    }

    if (rootTop >= ROOTSTACKLIMIT)
    {
        yyerror("root stack overflow\n");
    }

    struct ledaValue* result;

    // Store debugging information in case it is needed
    while (s)
    {
        linenumber = s->lineNumber;
        fileName = s->fileName;

        // Then do the statement
        switch(s->statementType)
        {
            default:
                printf("statement type is %d", s->statementType);
                yyerror("unimplemented statement type");

            case makeLocalsStatement:
                if (s->u.k.size > 0)
                {
                    currentContext->data[3] = gcalloc(s->u.k.size);
                }
                else
                {
                    currentContext->data[3] = 0;
                }
                if (displayOperators)
                {
                    printf
                    (
                        "Make locals %p size %d\n",
                        currentContext->data[3], s->u.k.size
                    );
                }
                s = s->next;
                break;

            case expressionStatement:
                if (displayStatements)
                {
                    printf
                    (
                        "File %s Line %d: expression statement\n",
                        s->fileName, s->lineNumber
                    );
                }
                result = evaluateExpression(s->u.r.e);
                // result should be empty
                if (result)
                {
                    yyerror
                    (
                        "internal run-time error: "
                        "expression statement is non-empty"
                    );
                }
                s = s->next;
                break;

            case returnStatement:
                if (displayOperators)
                {
                    printf
                    (
                        "File %s Line %d: (%p,%p) Starting return statement\n",
                        s->fileName, s->lineNumber,
                        currentContext, currentContext->data[2]
                    );
                }
                if (s->u.r.e)
                {
                    result = evaluateExpression(s->u.r.e);
                }
                else
                {
                    result = 0;
                }
                if (displayStatements)
                {
                    printf
                    (
                        "File %s Line %d: return statement, yields %p\n",
                        s->fileName, s->lineNumber, result
                    );
                }
                return result;

            case tailCall:
                {
                    struct ledaValue* code;
                    struct ledaValue* context;
                    struct ledaValue* newContext;

                    struct expressionRecord* e =  s->u.r.e;
                    char* functionName = e->u.f.symbol;

                    if (displayOperators)
                    {
                        printf("beginning function call operator\n");
                    }

                    if (functionName == 0)
                    {
                        functionName = " ? ";
                    }

                    if (e->u.f.fun->operator == makeClosure)
                    {
                        if
                        (
                            e->u.f.fun->u.l.context->operator
                         == getCurrentContext
                        )
                        {
                            context = currentContext;
                        }
                        else
                        {
                            context = evaluateExpression
                            (
                                e->u.f.fun->u.l.context
                            );
                            undefCheck(15, context,"context");
                        }
                        code = (struct ledaValue*)e->u.f.fun->u.l.code;
                    }
                    else if (e->u.f.fun->operator == makeMethodContext)
                    {
                        context = evaluateExpression(e->u.f.fun->u.o.base);
                        undefCheck(16, context,"context");
                        undefCheck(17, context->data[0],"method table");
                        code =
                            (context->data[0])->data[e->u.f.fun->u.o.location];
                    }
                    else
                    {
                        struct ledaValue* arg = evaluateExpression(e->u.f.fun);
                        undefCheck(18, arg, e->u.f.symbol);
                        context = arg->data[1];
                        code = arg->data[2];
                    }

                    // create the activation record
                    struct list* p = e->u.f.args;

                    rootStack[rootTop++] = context;
                    newContext = gcalloc(length(p) + 4);
                    context = rootStack[--rootTop];
                    if (displayOperators)
                    {
                        printf
                        (
                            "do tail call fun context %p, "
                            "new context %p args:\n",
                            context, newContext
                        );
                    }
                    newContext->data[1] = context;
                    newContext->data[2] = currentContext->data[2];
                    for (int i=4 ; p ; i++,p = p->next)
                    {
                        // tricky gc insurance
                        rootStack[rootTop++] = newContext;
                        struct ledaValue* arg = evaluateExpression
                        (
                            (struct expressionRecord*)p->value
                        );
                        newContext = rootStack[--rootTop];
                        newContext->data[i] = arg;
                        if (displayOperators)
                        {
                            printf
                            (
                                "argument %d in %p is %p\n",
                                i, newContext, newContext->data[i]
                            );
                        }
                    }
                    if (displayFunctions)
                    {
                        printf
                        (
                            "tail function call %s(%p), now do call\n",
                            functionName, code
                        );
                    }
                    currentContext = newContext;
                    s = (struct statementRecord*) code;
                }
                break;

            case conditionalStatement:
                if (displayStatements)
                {
                    printf
                    (
                        "File %s Line %d: conditional statement\n",
                        s->fileName, s->lineNumber
                    );
                }
                result = evaluateExpression(s->u.c.expr);
                if (result == trueObject)
                {
                    s = s->next;
                }
                else
                {
                    s = s->u.c.falsePart;
                }
                break;

            case nullStatement:
                s = s->next;
                break;
        };
    };

    return 0;
}


// Fix up the metaclass information on class objects
// can only be done once global variables have been defined

static void fixClassTable
(
    struct symbolRecord* sym,
    struct ledaValue* classClass
)
{
    struct typeRecord* t = checkClass(sym->u.c.typ);
    if (t == 0)
    {
        yyerror("trying to fix non-class??");
    }

    struct ledaValue* table = t->u.c.staticTable;
    if (table == 0)
    {
        if (strcmp(sym->name, "Leda_undefined") != 0)
        {
            printf("empty static table for class %s", sym->name);
        }
        return;
    }
    table->data[0] = classClass;
    table->data[1] = globalContext;
    table->data[2] = newStringConstant(sym->name);
    table->data[3] = newIntegerConstant(t->u.c.symbols->u.c.methodTableSize);
    struct typeRecord* pt = checkClass(t->u.c.parent);
    if (pt == 0)
    {
        yyserror("parent is not class type for table %s", sym->name);
    }
    table->data[4] = pt->u.c.staticTable;
    if (table->data[4] == 0)
    {
        printf("parent types %d\n", t->u.c.parent->ttyp);
        yyserror("parent doesn't have table for class type %s",
        sym->name);
    }
}


void beginInterpreter
(
    struct symbolTableRecord* syms,
    struct statementRecord* firstStatement
)
{
    printf("parse ok, starting execution\n");

    globalContext = staticAllocate(syms->size);

    struct ledaValue* a;
    struct typeRecord* t;
    struct ledaValue* classClass = 0;

    // First find all the necessary global symbols
    for (struct list* p = syms->firstSymbol; p; p = p->next)
    {
        struct symbolRecord* s = (struct symbolRecord*) p->value;
        switch (s->styp)
        {
            case varSymbol:
                if (strcmp(s->name, "true") == 0)
                {
                    a = staticAllocate(1);
                    a->data[0] = trueClass;
                    a->data[1] = globalContext;
                    trueObject = a;
                }
                else if (strcmp(s->name, "false") == 0)
                {
                    a = staticAllocate(1);
                    a->data[0] = falseClass;
                    a->data[1] = globalContext;
                    falseObject = a;
                }
                else if (strcmp(s->name, "NIL") == 0)
                {
                    // NIL is simply zero
                    a = 0;
                }
                else {
                    // everything else is undefined
                    a = 0;
                }
                globalContext->data[s->u.v.location] = a;
                break;

            case classDefSymbol:
                t = checkClass(s->u.c.typ);
                if (t == 0)
                    yyserror("error in building table for %s",
                    s->name);
                a = t->u.c.staticTable;
                if ((a == 0) &&
                strcmp(s->name, "Leda_undefined") != 0)
                    printf("null static table for %s\n",
                    s->name);
                globalContext->data[s->u.c.location] = a;
                if (displayOperators)
                    printf("class %s is %p\n", s->name, a);
                if (strcmp(s->name, "integer") == 0)
                    integerClass = a;
                if (strcmp(s->name, "real") == 0)
                    realClass = a;
                if (strcmp(s->name, "string") == 0)
                    stringClass = a;
                if (strcmp(s->name, "True") == 0)
                    trueClass = a;
                if (strcmp(s->name, "False") == 0)
                    falseClass = a;
                if (strcmp(s->name, "Class") == 0)
                    classClass = a;
                break;

            case functionSymbol:
                globalContext->data[s->u.c.location] =
                (struct ledaValue*) s->u.f.code;
                break;

            case typeSymbol:
                // no need to do anything
                break;

            case constSymbol:
                // starts out undefined
                globalContext->data[s->u.s.location] = 0;
                break;

            default:
                yyserror
                (
                    "found unimplemented symbol %s "
                    "in construction of global context\n",
                    s->name
                );
        }
    }

    // Allocate a few of the more common numbers
    for (int i=0; i<20; i++)
    {
        integerTable[i] = newIntegerConstant(i);
    }

    // Finally fix up all the class definitions
    for (struct list* p = syms->firstSymbol; p; p = p->next)
    {
        struct symbolRecord* s = (struct symbolRecord*) p->value;
        if (s->styp == classDefSymbol)
        {
            fixClassTable(s, classClass);
        }
    }

    // now start execution
    doingInitialization = 0;
    currentContext = globalContext;
    a = evaluateStatement(firstStatement);

    printf("\nexecution ended normally\n");
}


// -----------------------------------------------------------------------------
