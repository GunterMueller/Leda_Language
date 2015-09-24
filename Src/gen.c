/*
  code generation routines for the leda interpreter
*/

#include "lc.h"
#include "interp.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>

extern int linenumber;
extern char* fileName;


static struct expressionRecord* genOffset
(
    struct expressionRecord* base,
    int i,
    struct symbolRecord* s,
    struct typeRecord* t
);


struct statementRecord* newStatement(enum statements st)
{
    struct statementRecord* s =
        (struct statementRecord*) malloc(sizeof(struct statementRecord));

    if (s == 0)
    {
        yyerror("out of memory");
    }

    s->fileName = fileName;
    s->lineNumber = linenumber;
    s->statementType = st;
    s->next = 0;

    return s;
}


struct statementRecord* genExpressionStatement(struct expressionRecord* e)
{
    if (e->resultType != 0)
    {
        yyerror("expression statement with nonzero return type");
    }

    struct statementRecord* s = newStatement(expressionStatement);
    s->u.r.e = e;

    return s;
}


struct statementRecord* genAssignmentStatement
(
    struct expressionRecord* left,
    struct expressionRecord* right
)
{
    if (left->resultType->ttyp == constantType)
    {
        yyerror("cannot assign to a constant value");
    }

    if (!typeConformable(left->resultType, right->resultType))
    {
        printf("type types %d %d\n",
        left->resultType->ttyp, right->resultType->ttyp);
        yyerror("assignment types are not conformable");
    }

    return genExpressionStatement(genAssignment(left, right));
}


static int canMakeIntoTailCall
(
    struct expressionRecord* e,
    struct typeRecord* t
)
{
    // Expression must be function call
    if (e->operator != doFunctionCall)
    {
        return 0;
    }

    // There must be only one argument
    if (length(e->u.f.args) != 1)
    {
        return 0;
    }

    // There must be only one argument in current context
    if (length(t->u.f.argumentTypes) != 1)
    {
        return 0;
    }

    // The one argument must be the same as current argument
    e = (struct expressionRecord*) e->u.f.args->value;
    if (e->operator != getOffset)
    {
        return 0;
    }

    if (e->u.o.base->operator != getCurrentContext)
    {
        return 0;
    }

    if (e->u.o.location != 4)
    {
        return 0;
    }

    // OK, we can do it!
    return 1;
}


struct statementRecord* genReturnStatement
(
    struct symbolTableRecord* syms,
    struct expressionRecord* e
)
{
    struct statementRecord* s = newStatement(returnStatement);

    // Need to check that type matches declared return type
    if (syms->ttype != functionTable)
    {
        yyerror("return statement not inside of function");
    }

    struct typeRecord* t = syms->definingType->u.f.returnType;
    if (t)
    {
        // See if a boolean should convert to a relation
        if
        (
            typeConformable(relationType, t) &&
            typeConformable(booleanType, e->resultType)
        )
        {
            e = relationCheck(syms, e);
        }
        // or vice versa
        else if
        (
            typeConformable(booleanType, t) &&
            typeConformable(relationType, e->resultType)
        )
        {
            e = booleanCheck(syms, e);
        }

        if (!typeConformable(t, e->resultType))
        {
            yyerror("return type does not match function definition");
        }
    }
    else if (e)
    {
        yyerror("return expression from within function with no return type");
    }

    s->u.r.e = e;
    if (e && canMakeIntoTailCall(e, syms->definingType))
    {
        s->statementType = tailCall;
    }

    return s;
}

struct statementRecord* genConditionalStatement
(
    int ln,
    struct expressionRecord* e,
    struct statementRecord* tpf,
    struct statementRecord* tpl,
    struct statementRecord* fpf,
    struct statementRecord* fpl,
    struct statementRecord* nt
)
{
    struct statementRecord* s = newStatement(conditionalStatement);
    s->lineNumber = ln;

    // Check that expression return type is boolean
    // if (e->resultType != booleanType)
    //     yyerror("conditional statement must have boolean type");

    // Fill in the statement fields
    s->u.c.expr = e;
    s->next = tpf;
    tpl->next = nt;
    if (fpf) // If there is a false part
    {
        s->u.c.falsePart = fpf;
        fpl->next = nt;
    }
    else // If there is no false part
    {
        s->u.c.falsePart = nt;
    }

    return s;
}


struct statementRecord* genWhileStatement
(
    int ln,
    struct expressionRecord* e,
    struct statementRecord* stateFirst,
    struct statementRecord* stateLast,
    struct statementRecord* nullState
)
{
    struct statementRecord* s =
        genConditionalStatement(ln, e, stateFirst, stateLast, 0, 0, nullState);
    // Make next on statement go back to first
    stateLast->next = s;

    return s;
}

struct statementRecord* genBody
(
    struct symbolTableRecord* syms,
    struct statementRecord* code
)
{
    // Make base for constants
    struct expressionRecord* base = newExpression(getCurrentContext);
    if (syms->ttype == functionTable)
    {
        base = genOffset(base, 3, 0, 0);
    }

    // Make all the constants into assignment statements
    for (struct list* p = syms->firstSymbol; p; p = p->next)
    {
        struct symbolRecord* sym = (struct symbolRecord*) p->value;
        if (sym->styp == constSymbol)
        {
            struct statementRecord* st = genAssignmentStatement
            (
                genOffset(base,
                sym->u.s.location, 0,
                sym->u.s.val->resultType),
                sym->u.s.val
            );
            st->lineNumber = sym->u.s.lineNumber;
            st->next = code;
            code = st;
        }
    }

    // Then make the local statement
    struct statementRecord* s = newStatement(makeLocalsStatement);
    s->u.k.size = syms->size;
    s->next = code;

    return s;
}


/// Expressions

static struct expressionRecord* getCC = 0;

struct expressionRecord* newExpression(enum instructions opcode)
{
    if ((opcode == getCurrentContext) && (getCC != 0))
    {
        return getCC;
    }

    struct expressionRecord* e =
        (struct expressionRecord*)malloc(sizeof(struct expressionRecord));
    if (e == 0)
    {
        yyerror("out of memory\n");
    }
    e->operator = opcode;
    e->resultType = 0;
    if (opcode == getCurrentContext)
    {
        getCC = e;
    }

    return e;
}


struct expressionRecord* integerConstant(int v)
{
    struct expressionRecord* e = newExpression(genIntegerConstant);
    e->u.i.value = v;
    e->resultType = integerType;

    return e;
}


struct expressionRecord* stringConstant(char* s)
{
    struct expressionRecord* e = newExpression(genStringConstant);
    e->u.s.value = s;
    e->resultType = stringType;

    return e;
}


struct expressionRecord* realConstant(double v)
{
    struct expressionRecord* e = newExpression(genRealConstant);
    e->u.r.value = v;
    e->resultType = realType;

    return e;
}

static struct expressionRecord* genOffset
(
    struct expressionRecord* base,
    int i,
    struct symbolRecord* s,
    struct typeRecord* t
)
{
    struct expressionRecord* e = newExpression(getOffset);
    e->u.o.base = base;
    e->u.o.location = i;
    if (s)
    {
        e->u.o.symbol = s->name;
    }
    else
    {
        e->u.o.symbol = 0;
    }
    e->resultType = t;

    return e;
}


static struct expressionRecord* genFromSymbol
(
    struct expressionRecord* base,
    struct symbolRecord* s,
    int isFunctionTable,
    int isGlobals
)
{
    struct expressionRecord* e = 0;

    switch(s->styp)
    {
        case varSymbol:
            if (isFunctionTable)
            {
                e = genOffset(genOffset(base, 3, 0, 0),
                s->u.v.location, s, s->u.v.typ);
            }
            else
            {
                e = genOffset(base,
                s->u.v.location, s, s->u.v.typ);
                if (isGlobals)
                {
                    e->operator = getGlobalOffset;
                }
            }
            break;

        case functionSymbol:
            e = newExpression(makeClosure);
            e->u.l.context = base;
            e->u.l.code = s->u.f.code;
            e->resultType = s->u.f.typ;
            break;

        case argumentSymbol:
            e = genOffset(base, s->u.a.location, s, s->u.a.typ);
            if (s->u.a.form == byName)
            {
                struct expressionRecord* f = newExpression(evalThunk);
                f->u.o.base = e;
                f->u.o.symbol = s->name;
                f->resultType = s->u.a.typ;
                e = f;
            }
            else if (s->u.a.form == byReference)
            {
                struct expressionRecord* f = newExpression(evalReference);
                f->u.o.base = e;
                f->u.o.symbol = s->name;
                f->resultType = s->u.a.typ;
                e = f;
            }
            break;

        case classDefSymbol:
            e = genOffset(base, s->u.c.location, 0, 0);
            struct typeRecord* t = s->u.c.typ;

            e->resultType = newTypeRecord(classDefType);

            if (t->ttyp == qualifiedType)
            {
                struct typeRecord* nt;
                if (t->u.q.baseType->ttyp != classType)
                {
                    yyerror("confusing case in class instance building");
                }
                nt = newTypeRecord(qualifiedType);
                nt->u.q.qualifiers = t->u.q.qualifiers;
                nt->u.q.baseType = e->resultType;
                nt->u.q.baseType->u.q.baseType = t;
                e->resultType = nt;
            }
            else
            {
                // Simple class def type
                e->resultType->u.q.baseType = t;
            }
            break;

        case constSymbol:
            if (isFunctionTable)
            {
                e = genOffset
                (
                    genOffset(base, 3, 0, 0),
                    s->u.s.location,
                    s,
                    s->u.s.typ
                );
            }
            else
            {
                e = genOffset(base,
                s->u.s.location, s, s->u.s.typ);
                //if (isGlobals)
                //  e->operator = getGlobalOffset;
            }
            break;

        default:
            printf("symbol type is %d\n", s->styp);
            yyerror("Compiler error: unimplemented symbol type");
    }

    return e;
}


static int tempCount = 0;

static struct expressionRecord* generateTemporary
(
    struct symbolTableRecord* syms,
    struct typeRecord* t
)
{
    char name[100];
    sprintf(name, "Leda_temporary_%d",  tempCount++);

    return genFromSymbol
    (
        newExpression(getCurrentContext),
        addVariable(syms, newString(name), t),
        1,
        0
    );
}


struct expressionRecord* lookupFunction
(
    struct symbolTableRecord* syms,
    char* name
)
{
    struct expressionRecord* e = lookupIdentifier(syms, name);
    if (e->resultType->ttyp != functionType)
    {
        yyserror("expecting function for symbol %s", name);
    }

    return e;
}

static struct expressionRecord* makeMethodIntoFunction
(
    struct typeRecord* ct,
    char* fieldName
)
{
    struct typeRecord* bt = checkClass(ct);

    if (bt == 0)    // See if it is a class
    {
        return 0;
    }

    // Now find the method in the table
    for (struct list* p = bt->u.c.symbols->u.c.methodTable; p; p = p->next)
    {
        struct symbolRecord* s = (struct symbolRecord*) p->value;
        if (strcmp(fieldName, s->name) == 0)
        {
            if (s->styp != functionSymbol)
            {
                return 0;
            }
            struct typeRecord* t = s->u.f.typ;
            int i = 4 + length(t->u.f.argumentTypes);
            struct expressionRecord* f = newExpression(getOffset);
            f->u.o.location = i;
            f->u.o.base = newExpression(getCurrentContext);
            f->u.o.symbol = 0;
            struct expressionRecord* e = newExpression(makeClosure);
            e->u.l.context = f;
            e->u.l.code = s->u.f.code;
            f = newExpression(doFunctionCall);
            f->u.f.fun = e;
            f->u.f.symbol = s->name;
            // Make arg list
            struct list* args = 0;
            struct list* p2 = 0;
            struct list* q;
            for (p = t->u.f.argumentTypes; p; p = p->next)
            {
                struct expressionRecord* g = newExpression(getOffset);
                g->u.o.base = newExpression(getCurrentContext);
                g->u.o.location = --i;
                g->u.o.symbol = 0;
                args = newList((char*) g, args);
                if (p2 == 0)
                {
                    p2 = q = newList((char*) p->value, 0);
                }
                else
                {
                    q->next = newList((char*) p->value, 0);
                    q = q->next;
                }
            }
            f->u.f.args = args;
            struct statementRecord* st = newStatement(returnStatement);
            st->u.r.e = f;
            e = newExpression(makeClosure);
            e->u.l.context = newExpression(getCurrentContext);
            e->u.l.code = st;
            // Now fix up the type description
            struct typeRecord* nt = newTypeRecord(functionType);
            nt->u.f.returnType = t->u.f.returnType;
            s = newSymbolRecord(0, argumentSymbol);
            s->u.a.form = byValue;
            s->u.a.typ = bt;
            // Add new argument to argument list
            // of course it has to be at the end!
            if (p2 == 0)
            {
                p2 = newList((char*) s, 0);
            }
            else
            {
                q->next = newList((char*) s, 0);
            }
            nt->u.f.argumentTypes = p2;
            e->resultType = nt;

            return e;
        }
    }
    return 0;
}


struct expressionRecord* lookupField
(
    struct expressionRecord* base,
    struct typeRecord* t,
    char* fieldName
)
{
    if (base == 0)
    {
        yyerror("Compiler error: looking up field in zero base");
    }

    if (t == 0)
    {
        yyerror("Compiler error: expression with no result type");
    }

    if (t->ttyp == constantType)
    {
        struct expressionRecord* e =
            lookupField(base, t->u.u.baseType, fieldName);
        if (e)
        {
            // e->resultType = newConstantType(e->resultType);
            return e;
        }

        return 0;
    }

    if (t->ttyp == resolvedType)
    {
        struct expressionRecord* e =
            lookupField(base, t->u.r.baseType, fieldName);
        if (e)  // Fix up qualified types
        {
            e->resultType = fixResolvedType(e->resultType, t);
            return e;
        }
    }
    else if (t->ttyp == classType)
    {
        // First search instance table
        for (struct list* p = t->u.c.symbols->firstSymbol; p; p=p->next)
        {
            struct symbolRecord* s = (struct symbolRecord*) p->value;
            if (strcmp(fieldName, s->name) == 0)
            {
                return genFromSymbol(base, s, 0, 0);
            }
        }

        // Next try methods table
        for (struct list* p = t->u.c.symbols->u.c.methodTable; p; p=p->next)
        {
            struct symbolRecord* s = (struct symbolRecord*) p->value;
            if (strcmp(fieldName, s->name) == 0)
            {
                if (s->styp == functionSymbol)
                {
                    struct expressionRecord* e =
                        newExpression(makeMethodContext);
                    e->u.o.base = base;
                    e->u.o.location = s->u.f.location;
                    e->u.o.symbol = s->name;
                    e->resultType = s->u.f.typ;
                    return e;
                }
                else
                {
                    struct expressionRecord* e = genFromSymbol(base, s, 0, 0);
                    return e;
                }
            }
        }

        // Not known, return 0
        return 0;
    }
    else if (t->ttyp == unresolvedType)
    {
        return lookupField(base, t->u.u.baseType, fieldName);
    }
    else if (t->ttyp == qualifiedType)
    {
        return lookupField(base, t->u.q.baseType, fieldName);
    }
    else if (t->ttyp == classDefType)
    {
        return makeMethodIntoFunction(t->u.q.baseType, fieldName);
    }

    return 0;
}

static struct expressionRecord* lookupAddress
(
    struct symbolTableRecord* syms,
    char* name,
    struct expressionRecord* base
)
{
    switch(syms->ttype)
    {
        case globals:   // See if it is in the global symbol table
            for (struct list* p = syms->firstSymbol; p; p = p->next)
            {
                struct symbolRecord* s = (struct symbolRecord*) p->value;
                if (strcmp(name, s->name) == 0)
                {
                    return genFromSymbol(base, s, 0, 1);
                }
            }
            return 0;   // Not known

        case functionTable:
            for (struct list* p = syms->firstSymbol; p; p=p->next)
            {
                struct symbolRecord* s = (struct symbolRecord*) p->value;
                if (strcmp(name, s->name) == 0)
                {
                    return genFromSymbol(base, s, 1, 0);
                }
            }
            // Not local, try next level
            return lookupAddress
            (
                syms->surroundingContext,
                name,
                genOffset(base, 1, 0, 0)
            );

        case classTable:
            {
                struct expressionRecord* e =
                    lookupField(base, syms->definingType, name);
                if (e)
                {
                    return e;
                }

                // Not local, try next level
                return lookupAddress
                (
                    syms->surroundingContext,
                    name,
                    genOffset(base, 1, 0, 0)
                );
            }
    }

    return 0;
}


struct expressionRecord* lookupIdentifier
(
    struct symbolTableRecord* syms,
    char* name
)
{
    struct expressionRecord* e =
        lookupAddress(syms, name, newExpression(getCurrentContext));
    if (e == 0)
    {
        yyserror("unknown identifier %s", name);
    }

    return e;
}


static int argumentsCanMatch( struct typeRecord* t, struct list* args)
{
    // Just return true or false, if the arguments can be made to match the
    // function type
    struct typeRecord* ft = checkFunction(t);
    if (ft == 0) return 0;

    struct list* q = args;

    if (length(ft->u.f.argumentTypes) != length(q))
    {
        return 0;
    }

    for (int i = 0; q; i++, q = q->next)
    {
        struct symbolRecord* ps = argumentNumber(t, i);
        struct expressionRecord* qe = (struct expressionRecord*) q->value;
        struct typeRecord* pt = ps->u.a.typ;

        if (ps->u.a.form == byValue)
        {
            if (!typeConformable(pt, qe->resultType)) return 0;
        }
        else if (ps->u.a.form == byName)
        {
            if (!typeConformable(pt, qe->resultType)) return 0;
        }
        else if (ps->u.a.form == byReference)
        {
            // Must match both ways
            if (!typeConformable(pt, qe->resultType)) return 0;
            if (!typeConformable(qe->resultType, pt)) return 0;
        }
    }

    return 1;
}


struct expressionRecord* genThunk(struct expressionRecord* e)
{
    struct expressionRecord* ne = newExpression(makeClosure);
    struct statementRecord* st = newStatement(returnStatement);
    st->u.r.e = e;
    ne->u.l.context = newExpression(getCurrentContext);
    ne->u.l.code = st;

    return ne;
}


struct expressionRecord* generateFunctionCall
(
    struct symbolTableRecord* syms,
    struct expressionRecord* base,
    struct list* args,
    int isFun
)
{
    // Make sure base is a function
    struct typeRecord* t = base->resultType;

    if (t->ttyp == constantType)
    {
        t = t->u.u.baseType;
    }

    if (t->ttyp == resolvedType)
    {
        struct typeRecord* ft = t->u.r.baseType;
        if (ft->ttyp == classDefType)
        {
            // Constructor
            // should actually check arguments
            struct expressionRecord* e = newExpression(buildInstance);
            e->u.n.table = base;
            e->u.n.size = ft->u.q.baseType->
            u.q.baseType->u.c.symbols->size;
            e->u.n.args = reverse(args);
            e->resultType = fixResolvedType(ft->u.q.baseType, t);
            if (isFun == 0)
            {
                yyerror("Value generated by constructor must be used");
            }

            return e;
        }
    }

    if (t->ttyp == classDefType)
    {
        // constructor
        // should actually check arguments
        struct expressionRecord* e = newExpression(buildInstance);
        e->u.n.table = base;
        e->u.n.size = t->u.q.baseType->u.c.symbols->size;
        e->u.n.args = reverse(args);
        e->resultType = t->u.q.baseType;
        if (isFun == 0)
        {
            yyerror("Value generated by constructor must be used");
        }

        return e;
    }

    struct typeRecord* ft = checkFunction(t);
    if (ft == 0)
    {
        printf("Type is %p %d\n", t, t->ttyp);
        yyerror("Attempt to evaluate non-function type");
    }

    // Need to make sure arguments are conformable
    if (!argumentsCanMatch(t, args))
    {
        yyerror("Arguments do not match function declaration");
    }

    struct list* q;
    int i;
    for (q = args, i = 0; q; i++, q = q->next)
    {
        struct symbolRecord* ps = argumentNumber(t, i);
        struct expressionRecord* qe = (struct expressionRecord*) q->value;
        if (ps->u.a.form == byName)
        {
            q->value = (char*) genThunk(qe);
        }
        else if (ps->u.a.form == byReference)
        {
            // Build a reference
            if (qe->operator == getGlobalOffset)
            {
                qe->operator = getOffset;
            }
            if (qe->operator == evalReference)
            {
                q->value = (char*) qe->u.o.base;
            }
            else if (qe->operator != getOffset)
            {
                // Make a temp
                struct expressionRecord* co = newExpression(commaOp);
                struct expressionRecord* ne = newExpression(makeReference);
                struct expressionRecord* temp =
                    generateTemporary(syms, qe->resultType);
                co->u.a.left = genAssignment(temp, qe);
                co->u.a.right = ne;
                ne->u.o.base = temp->u.o.base;
                ne->u.o.location = temp->u.o.location;
                ne->u.o.symbol = 0;
                q->value = (char*) co;
            }
            else
            {
                struct expressionRecord* ne = newExpression(makeReference);
                ne->u.o.base = qe->u.o.base;
                ne->u.o.location = qe->u.o.location;
                ne->u.o.symbol = qe->u.o.symbol;
                q->value = (char*) ne;
            }
        }
    }

    // Finally, make the function call expression
    struct expressionRecord* e = newExpression(doFunctionCall);
    e->u.f.fun = base;
    e->u.f.args = args;
    if (base->operator == getOffset)
    {
        e->u.f.symbol = base->u.o.symbol;
    }
    else if (base->operator == makeClosure)
    {
        e->u.f.symbol = base->u.l.functionName;
    }
    else
    {
        e->u.f.symbol = "the unknown function";
    }

    if (t->ttyp == resolvedType)
    {
        e->resultType = fixResolvedType(ft->u.f.returnType, t);
    }
    else
    {
        e->resultType = ft->u.f.returnType;
    }

    if (isFun)
    {
        if (e->resultType == 0)
        {
            yyerror("Using non-value returning function where value expected");
        }
    }
    else
    {
        if (e->resultType != 0)
        {
            yyerror
            (
                "Using value returning function where no value is expected"
            );
        }
    }

    return e;
}


struct expressionRecord* generateCFunctionCall
(
    char* name,
    struct list* args,
    struct typeRecord* rt
)
{
    struct expressionRecord* e = newExpression(doSpecialCall);

    // See if name is on approved list
    e->u.c.index = -1;
    for (int i = 0; specialFunctionNames[i]; i++)
    {
        if (strcmp(name, specialFunctionNames[i]) == 0)
        {
            e->u.c.index = i;
        }
    }

    if (e->u.c.index == -1)
    {
        yyerror("Unknown cfunction invoked");
    }

    // Reverse args, so that they are in order
    // Makes use of cfunction for allocation easier
    e->u.c.args = reverse(args);
    e->resultType = rt;

    return e;
}


static struct expressionRecord* checkBinarySymbol
(
    struct symbolTableRecord* syms,
    struct symbolRecord* s, char* name,
    struct expressionRecord* base,
    struct list* args
)
{
    if (strcmp(name, s->name) != 0)
    {
        return 0;
    }

    struct expressionRecord* e = genFromSymbol(base, s, 0, 0);

    if (argumentsCanMatch(e->resultType, args))
    {
        return generateFunctionCall(syms, e, args, 1);
    }

    return 0;
}


static struct expressionRecord* lookupBinaryOperator
(
    struct symbolTableRecord* syms,
    struct expressionRecord* base,
    char* name,
    struct list* args
)
{
    switch(syms->ttype)
    {
        case globals:
            for (struct list* p = syms->firstSymbol; p; p = p->next)
            {
                struct expressionRecord* e = checkBinarySymbol
                (
                    syms,
                    (struct symbolRecord*) p->value,
                    name,
                    base,
                    args
                );
                if (e != 0) return e;
            }
            break;

        case functionTable:
            for (struct list* p = syms->firstSymbol; p; p = p->next)
            {
                struct expressionRecord* e = checkBinarySymbol
                (
                    syms,
                    (struct symbolRecord*) p->value,
                    name,
                    base,
                    args
                );
                if (e != 0) return e;
            }
            return lookupBinaryOperator
            (
                syms->surroundingContext,
                genOffset(base, 1, 0, 0),
                name,
                args
            );

        case classTable:
            return lookupBinaryOperator
            (
                syms->surroundingContext,
                genOffset(base, 1, 0, 0),
                name,
                args
            );
    }

    return 0;
}


struct expressionRecord* generateBinaryOperator
(
    struct symbolTableRecord* syms,
    char* name,
    struct expressionRecord* left,
    struct expressionRecord* right
)
{
    struct typeRecord* t = left->resultType;
    struct list* args = newList((char*) right, 0);

    if
    (
        (t->ttyp == classType)
     || (t->ttyp == resolvedType)
     || (t->ttyp == constantType)
     || (t->ttyp == unresolvedType)
    )
    {
        // See if it matches a method
        struct expressionRecord* e = lookupField(left, t, name);
        if (e)
        {
            t = e->resultType;
            if (argumentsCanMatch(t, args))
            {
                return generateFunctionCall(syms, e, args, 1);
            }
        }
    }

    // Didn't work as method, try to find it as a binary function
    struct expressionRecord* e = lookupBinaryOperator
    (
        syms,
        newExpression(getCurrentContext),
        name,
        newList((char*) right, newList((char*) left, 0))
    );

    if (e == 0)
    {
        yyserror("cannot find match for binary operator %s", name);
    }

    return e;
}


struct expressionRecord* generateUnaryOperator
(
    struct symbolTableRecord* syms,
    char* name,
    struct expressionRecord* arg
)
{
    struct typeRecord* t = arg->resultType;
    struct list* args = newList((char*) arg, 0);

    if
    (
        (t->ttyp == classType)
     || (t->ttyp == resolvedType)
     || (t->ttyp == unresolvedType)
    )
    {
        // See if it matches a method
        struct expressionRecord* e = lookupField(arg, t, name);
        if (e)
        {
            t = e->resultType;
            if (argumentsCanMatch(t, 0))
            {
                return generateFunctionCall(syms, e, 0, 1);
            }
        }
    }

    // Didn't work as method, try to find it as a binary function
    struct expressionRecord* e = lookupBinaryOperator
    (
        syms,
        newExpression(getCurrentContext),
        name,
        args
    );

    if (e == 0)
    {
        yyserror("cannot find match for binary operator %s", name);
    }

    return e;
}


struct expressionRecord* genAssignment
(
    struct expressionRecord* left,
    struct expressionRecord* right
)
{
    // Should check that left is assignable
    // Can only assign an offset or a reference
    if (left->operator == getGlobalOffset)
    {
        left->operator = getOffset;
    }

    struct expressionRecord* a = 0;

    if (left->operator == getOffset)
    {
        // Make a new node for the reference to the left side
        struct expressionRecord* l = newExpression(makeReference);
        l->u.o.location = left->u.o.location;
        l->u.o.symbol = left->u.o.symbol;
        l->u.o.base = left->u.o.base;

        // Make a new node for the assignment
        a = newExpression(assignment);
        a->u.a.left = l;
        a->u.a.symbol = left->u.o.symbol;
        a->u.a.right = right;
    }
    else if (left->operator == evalReference)
    {
        a = newExpression(assignment);
        a->u.a.left = left->u.o.base;
        a->u.a.symbol = left->u.o.symbol;
        a->u.a.right = right;
    }
    else
    {
        yyerror("only references can be assigned");
    }

    return a;
}


struct expressionRecord* generateLeftArrow
(
    struct symbolTableRecord* syms,
    struct expressionRecord* left,
    struct expressionRecord* right
)
{
    if (!typeConformable(left->resultType, right->resultType))
    {
        yyerror("Non conformable types used in <-");
    }

    struct expressionRecord* r = 0;

    if (left->operator == getOffset)
    {
        r = newExpression(makeReference);
        r->u.o.base = left->u.o.base;
        r->u.o.location = left->u.o.location;
        r->u.o.symbol = left->u.o.symbol;
    }
    else if (left->operator == evalReference)
    {
        r = left->u.o.base;
    }
    else
    {
        yyerror("Assignment <- of non-reference");
    }

    struct expressionRecord* e = newExpression(doFunctionCall);
    e->u.f.fun = lookupFunction(syms, newString("Leda_arrow"));
    e->u.f.args = newList((char*) right, newList((char*) r, 0));
    e->resultType = relationType;

    return e;
}


struct expressionRecord* generateForRelation
(
    struct symbolTableRecord* syms,
    struct expressionRecord* relExp,
    struct expressionRecord* stopExp,
    struct statementRecord* stateFirst,
    struct statementRecord* stateLast)
{
    if (!typeConformable(relationType, relExp->resultType))
    {
        yyerror("for statement must have relation type");
    }

    // Make stop condition and statements into a thunk
    if (stopExp == 0)
    {
        stopExp = lookupIdentifier(syms, newString("false"));
    }

    if (!typeConformable(booleanType, stopExp->resultType))
    {
        yyerror("stop condition in for statement must be boolean");
    }

    stateLast->next = newStatement(returnStatement);
    stateLast->next->u.r.e = stopExp;
    struct expressionRecord* s = newExpression(makeClosure);
    s->u.l.context = newExpression(getCurrentContext);
    s->u.l.code = stateFirst;

    // Now make function call
    struct expressionRecord* e = newExpression(doFunctionCall);
    e->u.f.fun = lookupFunction(syms, newString("Leda_forRelation"));
    e->u.f.args = newList((char*) s, newList((char*) relExp, 0));
    e->resultType = 0;

    return e;
}


struct expressionRecord* booleanCheck
(
    struct symbolTableRecord* syms,
    struct expressionRecord* e
)
{
    // Convert a relation into a boolean, if necessary
    if (typeConformable(relationType, e->resultType))
    {
        struct expressionRecord* f = newExpression(doFunctionCall);
        f->u.f.fun = lookupFunction(syms, newString("relationAsBoolean"));
        f->u.f.args = newList((char*) e, 0);
        f->resultType = booleanType;
        e = f;
    }

    return e;
}


struct expressionRecord* relationCheck
(
    struct symbolTableRecord* syms,
    struct expressionRecord* e
)
{
    // Convert a relation into a boolean, if necessary
    if (typeConformable(booleanType, e->resultType))
    {
        struct expressionRecord* f = newExpression(doFunctionCall);
        f->u.f.fun = lookupFunction(syms, newString("booleanAsRelation"));
        f->u.f.args = newList((char*) genThunk(e), 0);
        f->resultType = relationType;
        e = f;
    }

    return e;
}


struct symbolTableRecord* generateFunctionExpression
(
    struct symbolTableRecord* syms,
    struct list* va,
    struct typeRecord* rt
)
{
    // Make a symbol table for the function expression
    struct symbolTableRecord* ns = newSymbolTable(functionTable, syms);

    // make a new type for the symbol table
    ns->definingType = newFunctionType(enterFunctionArguments(ns, va), rt);

    return ns;
}


struct statementRecord* generateArithmeticForStatement
(
    int ln,
    struct symbolTableRecord* syms,
    struct expressionRecord* target,
    struct expressionRecord* start,
    struct expressionRecord* limit,
    struct statementRecord* stFirst,
    struct statementRecord* stLast,
    struct statementRecord* nullState
)
{
    struct expressionRecord* temp = generateTemporary(syms, target->resultType);
    struct statementRecord* s = genAssignmentStatement(temp, limit);
    struct statementRecord* s2 = genAssignmentStatement(target, start);
    struct expressionRecord* test = generateBinaryOperator
    (
        syms,
        newString("lessEqual"),
        target,
        temp
    );
    struct statementRecord* is = genAssignmentStatement
    (
        target,
        generateBinaryOperator(syms, newString("plus"),
        target,
        integerConstant(1))
    );

    // Now put all the pieces together
    s->next = s2;
    stLast->next = is;
    s2->next = genWhileStatement(ln, test, stFirst, is, nullState);

    return s;
}


struct expressionRecord* generateArrayLiteral
(
    struct symbolTableRecord* syms,
    struct list* exps
)
{
    // There must be at least one expression
    if (length(exps) < 1)
    {
        yyerror("must be at least one expression in array literal");
    }

    // Make sure they are all the same type
    struct typeRecord* baseType = 0;
    for (struct list* p = exps; p; p = p->next)
    {
        struct expressionRecord* e = (struct expressionRecord*) p->value;
        if (baseType == 0)
        {
            baseType = e->resultType;
        }
        else if (!typeConformable(baseType, e->resultType))
        {
            yyerror("all expressions in array literal must be same type");
        }
    }

    // Find symbol for array
    struct expressionRecord* ae = lookupIdentifier(syms, "array");

    // Diddle with the types for array
    struct symbolRecord* s = newSymbolRecord(0, argumentSymbol);
    s->u.a.form = byValue;
    s->u.a.typ = baseType;
    struct typeRecord* rt = checkQualifications
    (
        ae->resultType,
        newList((char*) s, 0)
    );
    if (rt->ttyp != resolvedType)
    {
        yyerror("confusing case in generateArrayLiteral");
    }
    if (rt->u.r.baseType->ttyp != classDefType)
    {
        yyerror("another confusing case in generateArrayLiteral");
    }

    // Make the expression that represents the arguments
    struct expressionRecord* arge = newExpression(doSpecialCall);
    arge->u.c.index = 15;
    arge->u.c.args = newList((char*) integerConstant(length(exps)),
    reverse(exps));

    // Now build the array
    struct expressionRecord* e = newExpression(buildInstance);
    e->u.n.table = ae;
    e->u.n.size = 4;  // length(exps);
    e->u.n.args = newList((char*) integerConstant(1),
    newList((char*) integerConstant(length(exps)),
    newList((char*) arge, 0)));
    e->resultType = fixResolvedType(rt->u.r.baseType->u.q.baseType, rt);

    return e;
}


struct expressionRecord* genPatternMatch
(
    struct symbolTableRecord* syms,
    struct expressionRecord* base,
    struct expressionRecord* theclass,
    struct list* args
)
{
    struct expressionRecord* e = newExpression(patternMatch);
    e->u.p.base = base;
    e->u.p.class = theclass;

    // Now make references from all the expressions
    struct list* p = 0;
    for (; args; args = args->next)
    {
        struct expressionRecord* f = lookupIdentifier(syms, args->value);
        if (f == 0)
        {
            yyserror("unknown identifier ", args->value);
        }
        if (f->operator != getOffset)
        {
            yyerror("variable in pattern must be local\n");
        }
        struct expressionRecord* ne = newExpression(makeReference);
        ne->u.o.base = f->u.o.base;
        ne->u.o.location = f->u.o.location;
        ne->u.o.symbol = f->u.o.symbol;
        p = newList((char*) ne, p);
    }

    e->u.p.args = p;
    e->resultType = booleanType;

    return e;
}
