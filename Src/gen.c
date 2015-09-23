/*
  code generation routines for the leda interpreter
*/

# include "lc.h"
# include "interp.h"
# include <stdio.h>

extern int linenumber;
extern char * fileName;

static struct expressionRecord * genOffset(
    struct expressionRecord * base, int i,
    struct symbolRecord * s, struct typeRecord * t);

struct statementRecord * newStatement(enum statements st)
{
    struct statementRecord * s;

    s = (struct statementRecord *) malloc(sizeof(struct statementRecord));
    if (s == 0)
        yyerror("out of memory");
    s->fileName = fileName;
    s->lineNumber = linenumber;
    s->statementType = st;
    s->next = 0;
    return s;
}

struct statementRecord * genExpressionStatement(struct expressionRecord * e)
{
    struct statementRecord * s = newStatement(expressionStatement);
    if (e->resultType != 0)
        yyerror("expression statement with nonzero return type");
    s->u.r.e = e;
    return s;
}

struct statementRecord * genAssignmentStatement(
    struct expressionRecord * left, struct expressionRecord * right)
{
    if (left->resultType->ttyp == constantType)
        yyerror("cannot assign to a constant value");

    if (! typeConformable(left->resultType, right->resultType)) {
        printf("type types %d %d\n",
        left->resultType->ttyp, right->resultType->ttyp);
        yyerror("assignment types are not conformable");
    }
    return genExpressionStatement(genAssignment(left, right));
}

static int canMakeIntoTailCall(struct expressionRecord * e,
struct typeRecord * t)
{
    /* expression must be function call */
    if (e->operator != doFunctionCall)
        return 0;
    /* there must be only one argument */
    if (length(e->u.f.args) != 1)
        return 0;
    /* there must be only one argument in current context */
    if (length(t->u.f.argumentTypes) != 1)
        return 0;
    /* the one argument must be the same as current argument */
    e = (struct expressionRecord *) e->u.f.args->value;
    if (e->operator != getOffset)
        return 0;
    if (e->u.o.base->operator != getCurrentContext)
        return 0;
    if (e->u.o.location != 4)
        return 0;
    /* ok, we can do it! */
    return 1;
}

struct statementRecord * genReturnStatement(
    struct symbolTableRecord * syms, struct expressionRecord * e)
{
    struct typeRecord * t;
    struct statementRecord * s = newStatement(returnStatement);

    /* need to check that type matches declared return type */
    if (syms->ttype != functionTable)
        yyerror("return statement not inside of function");
    t = syms->definingType->u.f.returnType;
    if (t) {
        /* see if a boolean should convert to a relation */
        if (typeConformable(relationType, t) &&
        typeConformable(booleanType, e->resultType))
            e = relationCheck(syms, e);

        /* or vice versa */
        else if (typeConformable(booleanType, t) &&
        typeConformable(relationType, e->resultType))
            e = booleanCheck(syms, e);

        if (! typeConformable(t, e->resultType)) {
            yyerror("return type does not match function definition");
        }
    }
    else if (e)
        yyerror("return expression from within function with no return type");

    s->u.r.e = e;
    if (e && canMakeIntoTailCall(e, syms->definingType))
        s->statementType = tailCall;
    return s;
}

struct statementRecord * genConditionalStatement(int ln,
struct expressionRecord * e,
struct statementRecord * tpf,
struct statementRecord * tpl,
struct statementRecord * fpf,
struct statementRecord * fpl,
struct statementRecord * nt)
{
    struct statementRecord * s = newStatement(conditionalStatement);
    s->lineNumber = ln;

    /* check that expression return type is boolean */
    /*if (e->resultType != booleanType)
      yyerror("conditional statement must have boolean type");*/

    /* fill in the statement fields */
    s->u.c.expr = e;
    s->next = tpf;
    tpl->next = nt;
    if (fpf) {  /* if there is a false part */
        s->u.c.falsePart = fpf;
        fpl->next = nt;
    }
    else        /* if there is no false part */
        s->u.c.falsePart = nt;
    return s;
}

struct statementRecord * genWhileStatement(int ln,
struct expressionRecord * e,
struct statementRecord * stateFirst,
struct statementRecord * stateLast,
struct statementRecord * nullState)
{
    struct statementRecord * s =
    genConditionalStatement(ln, e, stateFirst, stateLast, 0, 0,
    nullState);
    /* make next on statement go back to first */
    stateLast->next = s;
    return s;
}

struct statementRecord * genBody(
    struct symbolTableRecord * syms,
    struct statementRecord * code)
{
    struct statementRecord * s;
    struct statementRecord * st;
    struct list * p;
    struct symbolRecord * sym;
    struct expressionRecord * base;

    /* make base for constants */
    base = newExpression(getCurrentContext);
    if (syms->ttype == functionTable)
        base = genOffset(base, 3, 0, 0);

    /* make all the constants into assignment statements */
    for (p = syms->firstSymbol; p; p = p->next) {
        sym = (struct symbolRecord *) p->value;
        if (sym->styp == constSymbol) {
            st = genAssignmentStatement(
                genOffset(base,
                sym->u.s.location, 0,
                sym->u.s.val->resultType),
                sym->u.s.val);
            st->lineNumber = sym->u.s.lineNumber;
            st->next = code;
            code = st;
        }
    }

    /* then make the local statement */
    s = newStatement(makeLocalsStatement);
    s->u.k.size = syms->size;
    s->next = code;
    return s;
}

/*--------------expressions-------------------*/

static struct expressionRecord * getCC = 0;

struct expressionRecord * newExpression(enum instructions opcode)
{
    struct expressionRecord * e;

    if ((opcode == getCurrentContext) && (getCC != 0))
        return getCC;

    e = (struct expressionRecord *)
    malloc(sizeof(struct expressionRecord));
    if (e == 0)
        yyerror("out of memory\n");
    e->operator = opcode;
    e->resultType = 0;
    if (opcode == getCurrentContext)
        getCC = e;
    return e;
}

struct expressionRecord * integerConstant(int v)
{
    struct expressionRecord * e =
    newExpression(genIntegerConstant);
    e->u.i.value = v;
    e->resultType = integerType;
    return e;
}

struct expressionRecord * stringConstant(char * s)
{
    struct expressionRecord * e;
    e = newExpression(genStringConstant);
    e->u.s.value = s;
    e->resultType = stringType;
    return e;
}

struct expressionRecord * realConstant(double v)
{
    struct expressionRecord * e =
    newExpression(genRealConstant);
    e->u.r.value = v;
    e->resultType = realType;
    return e;
}

static struct expressionRecord * genOffset(
    struct expressionRecord * base, int i,
    struct symbolRecord * s, struct typeRecord * t)
{
    struct expressionRecord * e = newExpression(getOffset);
    e->u.o.base = base;
    e->u.o.location = i;
    if (s)
        e->u.o.symbol = s->name;
    else
        e->u.o.symbol = 0;
    e->resultType = t;
    return e;
}

static struct expressionRecord * genFromSymbol(
    struct expressionRecord * base,
    struct symbolRecord * s, int isFunctionTable, int isGlobals)
{
    struct expressionRecord * e;
    struct expressionRecord * f;
    struct typeRecord * t;

    switch(s->styp) {
        case varSymbol:
            if (isFunctionTable) {
                e = genOffset(genOffset(base, 3, 0, 0),
                s->u.v.location, s, s->u.v.typ);
            }
            else {
                e = genOffset(base,
                s->u.v.location, s, s->u.v.typ);
                if (isGlobals)
                    e->operator = getGlobalOffset;
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
            if (s->u.a.form == byName) {
                f = newExpression(evalThunk);
                f->u.o.base = e;
                f->u.o.symbol = s->name;
                f->resultType = s->u.a.typ;
                e = f;
            }
            else if (s->u.a.form == byReference) {
                f = newExpression(evalReference);
                f->u.o.base = e;
                f->u.o.symbol = s->name;
                f->resultType = s->u.a.typ;
                e = f;
            }
            break;

        case classDefSymbol:
            e = genOffset(base, s->u.c.location, 0, 0);
            t = s->u.c.typ;

            e->resultType = newTypeRecord(classDefType);

            if (t->ttyp == qualifiedType) {
                struct typeRecord * nt;
                if (t->u.q.baseType->ttyp != classType)
                    yyerror("confusing case in class instance building");
                nt = newTypeRecord(qualifiedType);
                nt->u.q.qualifiers = t->u.q.qualifiers;
                nt->u.q.baseType = e->resultType;
                nt->u.q.baseType->u.q.baseType = t;
                e->resultType = nt;
            }
            else  {
                /* simple class def type */
                e->resultType->u.q.baseType = t;
            }
            break;

        case constSymbol:
            if (isFunctionTable)
                e = genOffset(genOffset(base, 3, 0, 0),
                s->u.s.location, s, s->u.s.typ);
            else {
                e = genOffset(base,
                s->u.s.location, s, s->u.s.typ);
                /*if (isGlobals)
                  e->operator = getGlobalOffset;*/
            }
            break;

        default:
            printf("symbol type is %d\n", s->styp);
            yyerror("compiler error: unimplemented symbol type");
    }
    return e;
}

static int tempCount = 0;

static struct expressionRecord * generateTemporary(
    struct symbolTableRecord * syms,
    struct typeRecord * t)
{
    char name[100];
    sprintf(name, "Leda_temporary_%d",  tempCount++);

    return genFromSymbol(newExpression(getCurrentContext),
    addVariable(syms, newString(name), t), 1, 0);
}

struct expressionRecord * lookupFunction(
    struct symbolTableRecord * syms,
    char * name)
{
    struct expressionRecord * e = lookupIdentifier(syms, name);
    if (e->resultType->ttyp != functionType)
        yyserror("expecting function for symbol %s", name);
    return e;
}

static struct expressionRecord * makeMethodIntoFunction(
    struct typeRecord * ct,
    char * fieldName)
{
    struct typeRecord * bt = checkClass(ct);
    struct list * p;
    struct list * q;
    struct list * p2;
    struct list * args;
    struct typeRecord * t;
    struct typeRecord * nt;
    struct expressionRecord * e;
    struct expressionRecord * f;
    struct expressionRecord * g;
    struct statementRecord * st;
    int i;

    if (bt == 0)    /* see if it is a class */
        return 0;

    /* now find the method in the table */
    for (p = bt->u.c.symbols->u.c.methodTable; p; p = p->next) {
        struct symbolRecord * s = (struct symbolRecord *) p->value;
        if (strcmp(fieldName, s->name) == 0) {
            if (s->styp != functionSymbol)
                return 0;
            t = s->u.f.typ;
            i = 4 + length(t->u.f.argumentTypes);
            f = newExpression(getOffset);
            f->u.o.location = i;
            f->u.o.base = newExpression(getCurrentContext);
            f->u.o.symbol = 0;
            e = newExpression(makeClosure);
            e->u.l.context = f;
            e->u.l.code = s->u.f.code;
            f = newExpression(doFunctionCall);
            f->u.f.fun = e;
            f->u.f.symbol = s->name;
            /* make arg list */
            args = 0;
            p2 = 0;
            for (p = t->u.f.argumentTypes; p; p = p->next) {
                g = newExpression(getOffset);
                g->u.o.base = newExpression(getCurrentContext);
                g->u.o.location = --i;
                g->u.o.symbol = 0;
                args = newList((char *) g, args);
                if (p2 == 0)
                    p2 = q = newList((char *) p->value, 0);
                else {
                    q->next = newList((char *) p->value, 0);
                    q = q->next;
                }
            }
            f->u.f.args = args;
            st = newStatement(returnStatement);
            st->u.r.e = f;
            e = newExpression(makeClosure);
            e->u.l.context = newExpression(getCurrentContext);
            e->u.l.code = st;
            /* now fix up the type description */
            nt = newTypeRecord(functionType);
            nt->u.f.returnType = t->u.f.returnType;
            s = newSymbolRecord(0, argumentSymbol);
            s->u.a.form = byValue;
            s->u.a.typ = bt;
            /* add new argument to argument list */
            /* of course it has to be at the end! */
            if (p2 == 0)
                p2 = newList((char *) s, 0);
            else
                q->next = newList((char *) s, 0);
            nt->u.f.argumentTypes = p2;
            e->resultType = nt;

            return e;
        }
    }
    return 0;
}

struct expressionRecord * lookupField(struct expressionRecord * base,
struct typeRecord * t, char * fieldName)
{
    struct list * p;
    struct symbolRecord * s;
    struct expressionRecord * e;
    struct expressionRecord * f;

    if (base == 0)
        yyerror("compiler error: looking up field in zero base");

    if (t == 0)  {
        yyerror("compiler error: expression with no result type");
    }

    if (t->ttyp == constantType) {
        e = lookupField(base, t->u.u.baseType, fieldName);
        if (e) {
            /*e->resultType = newConstantType(e->resultType);*/
            return e;
        }
        return 0;
    }

    if (t->ttyp == resolvedType) {
        e = lookupField(base, t->u.r.baseType, fieldName);
        if (e)  {/* fix up qualified types */
            e->resultType =
            fixResolvedType(e->resultType, t);
            return e;
        }
    }

    else if (t->ttyp == classType) {

        /* first search instance table */
        for (p = t->u.c.symbols->firstSymbol; p; p=p->next) {
            s = (struct symbolRecord *) p->value;
            if (strcmp(fieldName, s->name) == 0)
                return genFromSymbol(base, s, 0, 0);
        }

        /* next try methods table */
        for (p = t->u.c.symbols->u.c.methodTable; p; p=p->next) {
            s = (struct symbolRecord *) p->value;
            if (strcmp(fieldName, s->name) == 0) {
                if (s->styp == functionSymbol) {
                    e = newExpression(makeMethodContext);
                    e->u.o.base = base;
                    e->u.o.location = s->u.f.location;
                    e->u.o.symbol = s->name;
                    e->resultType = s->u.f.typ;
                }
                else
                    e = genFromSymbol(base, s, 0, 0);
                return e;
            }
        }

        /* not known, return 0 */
        return 0;
    }

    else if (t->ttyp == unresolvedType) {
        return lookupField(base, t->u.u.baseType, fieldName);
    }

    else if (t->ttyp == qualifiedType) {
        return lookupField(base, t->u.q.baseType, fieldName);
    }

    else if (t->ttyp == classDefType)
        return makeMethodIntoFunction(t->u.q.baseType, fieldName);

    return 0;
}

static struct expressionRecord * lookupAddress(struct symbolTableRecord * syms,
char * name, struct expressionRecord * base)
{
    struct list * p;
    struct symbolRecord * s;
    struct expressionRecord * e;
    struct expressionRecord * f;

    switch(syms->ttype) {

        case globals:   /* see if it is in the global symbol table */
            for (p = syms->firstSymbol; p; p = p->next) {
                s = (struct symbolRecord *) p->value;
                if (strcmp(name, s->name) == 0)
                    return genFromSymbol(base, s, 0, 1);
            }
            return 0;   /* not known */

        case functionTable:
            for (p = syms->firstSymbol; p; p=p->next) {
                s = (struct symbolRecord *) p->value;
                if (strcmp(name, s->name) == 0)
                    return genFromSymbol(base, s, 1, 0);
            }
            /* not local, try next level */
            return lookupAddress(syms->surroundingContext,
            name, genOffset(base, 1, 0, 0));

        case classTable:
            e = lookupField(base, syms->definingType, name);
            if (e)
                return e;
            /* not local, try next level */
            return lookupAddress(syms->surroundingContext,
            name, genOffset(base, 1, 0, 0));
    }
    return 0;
}


struct expressionRecord * lookupIdentifier(struct symbolTableRecord * syms,
char * name)
{
    struct expressionRecord * e;
    e = lookupAddress(syms, name, newExpression(getCurrentContext));
    if (e == 0)
        yyserror("unknown identifier %s", name);
    return e;
}


static int argumentsCanMatch( struct typeRecord * t, struct list * args)
{
    /* just return true or false, if the arguments can be made
       to match the function type */
    struct list * q;
    int i;

    struct typeRecord * ft = checkFunction(t);

    if (ft == 0) {
        return 0;
    }

    q = args;


    if (length(ft->u.f.argumentTypes) != length(q))
        return 0;

    for (i = 0; q; i++, q = q->next) {
        struct symbolRecord * ps;
        struct expressionRecord * qe;
        struct typeRecord * pt;

        ps = argumentNumber(t, i);
        qe = (struct expressionRecord *) q->value;
        pt = ps->u.a.typ;
        if (ps->u.a.form == byValue) {
            if (! typeConformable(pt, qe->resultType))
                return 0;
        }
        else if (ps->u.a.form == byName) {
            if (! typeConformable(pt, qe->resultType))
                return 0;
        }
        else if (ps->u.a.form == byReference) {
            /* must match both ways */
            if (! typeConformable(pt, qe->resultType))
                return 0;
            if (! typeConformable(qe->resultType, pt))
                return 0;
        }
    }
    return 1;
}

struct expressionRecord * genThunk(struct expressionRecord * e)
{
    struct expressionRecord * ne =
    newExpression(makeClosure);
    struct statementRecord * st =
    newStatement(returnStatement);
    st->u.r.e = e;
    ne->u.l.context = newExpression(getCurrentContext);
    ne->u.l.code = st;
    return ne;
}

struct expressionRecord * generateFunctionCall(
    struct symbolTableRecord * syms,
    struct expressionRecord * base, struct list * args, int isFun)
{
    struct typeRecord * t;
    struct expressionRecord * e;
    struct list * q;
    int i;
    struct symbolRecord * ps;
    struct expressionRecord * qe;
    struct typeRecord * ft = 0;

    /* make sure base is a function */
    t = base->resultType;

    if (t->ttyp == constantType)
        t = t->u.u.baseType;

    if (t->ttyp == resolvedType) {
        ft = t->u.r.baseType;
        if (ft->ttyp == classDefType) {
            /* constructor */
            /* should actually check arguments */
            e = newExpression(buildInstance);
            e->u.n.table = base;
            e->u.n.size = ft->u.q.baseType->
            u.q.baseType->u.c.symbols->size;
            e->u.n.args = reverse(args);
            e->resultType = fixResolvedType(ft->u.q.baseType, t);
            if (isFun == 0)
                yyerror("value generated by constructor must be used");
            return e;
        }
    }

    if (t->ttyp == classDefType) {
        /* constructor */
        /* should actually check arguments */
        e = newExpression(buildInstance);
        e->u.n.table = base;
        e->u.n.size = t->u.q.baseType->u.c.symbols->size;
        e->u.n.args = reverse(args);
        e->resultType = t->u.q.baseType;
        if (isFun == 0)
            yyerror("value generated by constructor must be used");
        return e;
    }

    ft = checkFunction(t);
    if (ft == 0) {
        printf("type is %d %d\n", t, t->ttyp);
        yyerror("attempt to evaluate non-function type");
    }

    /* need to make sure arguments are conformable */
    if (! argumentsCanMatch(t, args))
        yyerror("arguments do not match function declaration");

    for (q = args, i = 0; q; i++, q = q->next) {
        ps = argumentNumber(t, i);
        qe = (struct expressionRecord *) q->value;
        if (ps->u.a.form == byName)
            q->value = (char *) genThunk(qe);
        else if (ps->u.a.form == byReference) {
            /* build a reference */
            if (qe->operator == getGlobalOffset)
                qe->operator = getOffset;
            if (qe->operator == evalReference) {
                q->value = (char *) qe->u.o.base;
            }
            else if (qe->operator != getOffset) {
                /* make a temp */
                struct expressionRecord * co =
                newExpression(commaOp);
                struct expressionRecord * ne =
                newExpression(makeReference);
                struct expressionRecord * temp =
                generateTemporary(syms,
                qe->resultType);
                co->u.a.left = genAssignment(temp, qe);
                co->u.a.right = ne;
                ne->u.o.base = temp->u.o.base;
                ne->u.o.location = temp->u.o.location;
                ne->u.o.symbol = 0;
                q->value = (char *) co;
            }
            else {
                struct expressionRecord * ne =
                newExpression(makeReference);
                ne->u.o.base = qe->u.o.base;
                ne->u.o.location = qe->u.o.location;
                ne->u.o.symbol = qe->u.o.symbol;
                q->value = (char *) ne;
            }
        }
    }

    /* finally, make the function call expression */
    e = newExpression(doFunctionCall);
    e->u.f.fun = base;
    e->u.f.args = args;
    if (base->operator == getOffset)
        e->u.f.symbol = base->u.o.symbol;
    else if (base->operator == makeClosure)
        e->u.f.symbol = base->u.l.functionName;
    else
        e->u.f.symbol = "the unknown function";
    if (t->ttyp == resolvedType)
        e->resultType = fixResolvedType(ft->u.f.returnType, t);
    else
        e->resultType = ft->u.f.returnType;
    if (isFun) {
        if (e->resultType == 0)
            yyerror("using non-value returning function where value expected");
    }
    else {
        if (e->resultType != 0)
            yyerror("using value returning function where no value is expected");
    }
    return e;
}

struct expressionRecord * generateCFunctionCall(char * name,
struct list * args, struct typeRecord * rt)
{
    struct expressionRecord * e;
    int i;

    e = newExpression(doSpecialCall);

    /* see if name is on approved list */
    e->u.c.index = -1;
    for (i = 0; specialFunctionNames[i]; i++) {
        if (strcmp(name, specialFunctionNames[i]) == 0)
            e->u.c.index = i;
    }

    if (e->u.c.index == -1)
        yyerror("unknown cfunction invoked");

    /* reverse args, so that they are in order */
    /* makes use of cfunction for allocation easier */
    e->u.c.args = reverse(args);
    e->resultType = rt;
    return e;
}

static struct expressionRecord * checkBinarySymbol(
    struct symbolTableRecord * syms,
    struct symbolRecord * s, char * name,
    struct expressionRecord * base,
    struct list * args)
{
    struct expressionRecord * e;

    if (strcmp(name, s->name) != 0)
        return 0;
    e = genFromSymbol(base, s, 0, 0);
    if (argumentsCanMatch(e->resultType, args))
        return generateFunctionCall(syms, e, args, 1);
    return 0;
}

static struct expressionRecord * lookupBinaryOperator(
    struct symbolTableRecord * syms,
    struct expressionRecord * base,
    char * name,
    struct list * args)
{
    struct list * p;
    struct expressionRecord * e;
    struct symbolRecord * s;

    switch(syms->ttype) {
        case globals:
            for (p = syms->firstSymbol; p; p = p->next) {
                s = (struct symbolRecord *) p->value;
                e = checkBinarySymbol(syms,
                (struct symbolRecord *) p->value,
                name, base, args);
                if (e != 0) return e;
            }
            break;

        case functionTable:
            for (p = syms->firstSymbol; p; p = p->next) {
                s = (struct symbolRecord *) p->value;
                e = checkBinarySymbol(syms,
                (struct symbolRecord *) p->value,
                name, base, args);
                if (e != 0) return e;
            }
            return lookupBinaryOperator(
                syms->surroundingContext,
                genOffset(base, 1, 0, 0), name, args);

        case classTable:
            return lookupBinaryOperator(
                syms->surroundingContext,
                genOffset(base, 1, 0, 0), name, args);
    }
    return 0;
}

struct expressionRecord * generateBinaryOperator(
    struct symbolTableRecord * syms, char * name,
    struct expressionRecord * left, struct expressionRecord * right)
{
    struct typeRecord * t = left->resultType;
    struct expressionRecord * e;
    struct list * args = newList((char *) right, 0);

    if ((t->ttyp == classType) || (t->ttyp == resolvedType)
    || (t->ttyp == constantType)
    || (t->ttyp == unresolvedType)) {
        /* see if it matches a method */
        e = lookupField(left, t, name);
        if (e) {
            t = e->resultType;
            if (argumentsCanMatch(t, args))
                return generateFunctionCall(syms, e, args, 1);
        }
    }

    /* didn't work as method, try to find it as a binary function */
    e = lookupBinaryOperator(syms, newExpression(getCurrentContext),
    name,
    newList((char *) right, newList((char *) left, 0)));
    if (e == 0)
        yyserror("cannot find match for binary operator %s", name);
    return e;
}

struct expressionRecord * generateUnaryOperator(
    struct symbolTableRecord * syms, char * name,
    struct expressionRecord * arg)
{
    struct typeRecord * t = arg->resultType;
    struct expressionRecord * e;
    struct list * args = newList((char *) arg, 0);

    if ((t->ttyp == classType) || (t->ttyp == resolvedType)
    || (t->ttyp == unresolvedType)) {
        /* see if it matches a method */
        e = lookupField(arg, t, name);
        if (e) {
            t = e->resultType;
            if (argumentsCanMatch(t, 0))
                return generateFunctionCall(syms, e, 0, 1);
        }
    }

    /* didn't work as method, try to find it as a binary function */
    e = lookupBinaryOperator(syms, newExpression(getCurrentContext),
    name, args);
    if (e == 0)
        yyserror("cannot find match for binary operator %s", name);
    return e;
}

struct expressionRecord * genAssignment(
    struct expressionRecord * left,
    struct expressionRecord * right)
{
    struct expressionRecord * a;
    struct expressionRecord * l;
    /* should check that left is assignable */
    /* can only assign an offset or a reference */
    if (left->operator == getGlobalOffset)
        left->operator = getOffset;
    if (left->operator == getOffset) {
        /* make a new node for the reference to the left side */
        l = newExpression(makeReference);
        l->u.o.location = left->u.o.location;
        l->u.o.symbol = left->u.o.symbol;
        l->u.o.base = left->u.o.base;

        /* make a new node for the assignment */
        a = newExpression(assignment);
        a->u.a.left = l;
        a->u.a.symbol = left->u.o.symbol;
        a->u.a.right = right;
    }
    else if (left->operator == evalReference) {
        a = newExpression(assignment);
        a->u.a.left = left->u.o.base;
        a->u.a.symbol = left->u.o.symbol;
        a->u.a.right = right;
    }
    else {
        yyerror("only references can be assigned");
    }

    return a;
}

struct expressionRecord * generateLeftArrow(
    struct symbolTableRecord * syms,
    struct expressionRecord * left,
    struct expressionRecord * right)
{
    struct expressionRecord * e;
    struct expressionRecord * r;

    if (! typeConformable(left->resultType, right->resultType))
        yyerror("non conformable types used in <-");

    if (left->operator == getOffset) {
        r = newExpression(makeReference);
        r->u.o.base = left->u.o.base;
        r->u.o.location = left->u.o.location;
        r->u.o.symbol = left->u.o.symbol;
    }
    else if (left->operator == evalReference) {
        r = left->u.o.base;
    }
    else
        yyerror("assignment <- of non-reference");

    e = newExpression(doFunctionCall);
    e->u.f.fun = lookupFunction(syms, newString("Leda_arrow"));
    e->u.f.args = newList((char *) right, newList((char *) r, 0));
    e->resultType = relationType;
    return e;

}

struct expressionRecord * generateForRelation(
    struct symbolTableRecord * syms,
    struct expressionRecord * relExp,
    struct expressionRecord * stopExp,
    struct statementRecord * stateFirst,
    struct statementRecord * stateLast)
{
    struct expressionRecord * s;
    struct expressionRecord * e;

    if (! typeConformable(relationType, relExp->resultType))
        yyerror("for statement must have relation type");

    /* make stop condition and statements into a thunk */
    if (stopExp == 0)
        stopExp = lookupIdentifier(syms, newString("false"));

    if (! typeConformable(booleanType, stopExp->resultType))
        yyerror("stop condition in for statement must be boolean");

    stateLast->next = newStatement(returnStatement);
    stateLast->next->u.r.e = stopExp;
    s = newExpression(makeClosure);
    s->u.l.context = newExpression(getCurrentContext);
    s->u.l.code = stateFirst;

    /* now make function call */
    e = newExpression(doFunctionCall);
    e->u.f.fun = lookupFunction(syms, newString("Leda_forRelation"));
    e->u.f.args = newList((char *) s, newList((char *) relExp, 0));
    e->resultType = 0;
    return e;
}

struct expressionRecord * booleanCheck(
    struct symbolTableRecord * syms,
    struct expressionRecord * e)
{
    /* convert a relation into a boolean, if necessary */
    if (typeConformable(relationType, e->resultType)) {
        struct expressionRecord * f;
        f = newExpression(doFunctionCall);
        f->u.f.fun = lookupFunction(syms,
        newString("relationAsBoolean"));
        f->u.f.args = newList((char *) e, 0);
        f->resultType = booleanType;
        e = f;
    }
    return e;
}

struct expressionRecord * relationCheck(
    struct symbolTableRecord * syms,
    struct expressionRecord * e)
{
    /* convert a relation into a boolean, if necessary */
    if (typeConformable(booleanType, e->resultType)) {
        struct expressionRecord * f;
        f = newExpression(doFunctionCall);
        f->u.f.fun = lookupFunction(syms,
        newString("booleanAsRelation"));
        f->u.f.args = newList((char *) genThunk(e), 0);
        f->resultType = relationType;
        e = f;
    }
    return e;
}

struct symbolTableRecord * generateFunctionExpression(
    struct symbolTableRecord * syms,
    struct list * va, struct typeRecord * rt)
{
    /* make a symbol table for the function expression */
    struct symbolTableRecord * ns = newSymbolTable(functionTable, syms);

    /* make a new type for the symbol table */
    ns->definingType = newFunctionType(
        enterFunctionArguments(ns, va),
        rt);

    return ns;
}

struct statementRecord * generateArithmeticForStatement(
    int ln,
    struct symbolTableRecord * syms,
    struct expressionRecord * target,
    struct expressionRecord * start,
    struct expressionRecord * limit,
    struct statementRecord * stFirst,
    struct statementRecord * stLast,
    struct statementRecord * nullState)
{
    struct expressionRecord * temp = generateTemporary(syms,
    target->resultType);
    struct statementRecord * s = genAssignmentStatement(temp, limit);
    struct statementRecord * s2 = genAssignmentStatement(target, start);
    struct expressionRecord * test = generateBinaryOperator(syms,
    newString("lessEqual"), target, temp);
    struct statementRecord * is = genAssignmentStatement(target,
    generateBinaryOperator(syms, newString("plus"),
    target, integerConstant(1)));

    /* now put all the pieces together */
    s->next = s2;
    stLast->next = is;
    s2->next = genWhileStatement(ln, test, stFirst, is, nullState);
    return s;
}

struct expressionRecord * generateArrayLiteral(
    struct symbolTableRecord * syms,
    struct list * exps)
{
    struct typeRecord * baseType = 0;
    struct typeRecord * rt;
    struct expressionRecord * ae;
    struct expressionRecord * arge;
    struct list * p;
    struct symbolRecord * s;
    struct expressionRecord * e;

    /* there must be at least one expression */
    if (length(exps) < 1)
        yyerror("must be at least one expression in array literal");

    /* make sure they are all the same type */
    for (p = exps; p; p = p->next) {
        struct expressionRecord * e =
        (struct expressionRecord *) p->value;
        if (baseType == 0)
            baseType = e->resultType;
        else if (! typeConformable(baseType, e->resultType))
            yyerror("all expressions in array literal must be same type");
    }

    /* find symbol for array */
    ae = lookupIdentifier(syms, "array");

    /* diddle with the types for array */
    s = newSymbolRecord(0, argumentSymbol);
    s->u.a.form = byValue;
    s->u.a.typ = baseType;
    rt = checkQualifications(ae->resultType, newList((char *) s, 0));
    if (rt->ttyp != resolvedType)
        yyerror("confusing case in generateArrayLiteral");
    if (rt->u.r.baseType->ttyp != classDefType) {
        yyerror("another confusing case in generateArrayLiteral");
    }

    /* make the expression that represents the arguments */
    arge = newExpression(doSpecialCall);
    arge->u.c.index = 15;
    arge->u.c.args = newList((char *) integerConstant(length(exps)),
    reverse(exps));

    /* now build the array */
    e = newExpression(buildInstance);
    e->u.n.table = ae;
    e->u.n.size = 4; /*length(exps);*/
    e->u.n.args = newList((char *) integerConstant(1),
    newList((char *) integerConstant(length(exps)),
    newList((char *) arge, 0)));
    e->resultType =
    fixResolvedType(rt->u.r.baseType->u.q.baseType, rt);

    return e;
}

struct expressionRecord * genPatternMatch(
    struct symbolTableRecord * syms,
    struct expressionRecord * base,
    struct expressionRecord * theclass,
    struct list * args)
{
    struct expressionRecord * e = newExpression(patternMatch);
    struct list * q;
    struct list * p;
    struct expressionRecord * f;
    struct expressionRecord * ne;

    e->u.p.base = base;
    e->u.p.class = theclass;

    /* now make references from all the expressions */
    p = 0;
    for (; args; args = args->next) {
        f = lookupIdentifier(syms, args->value);
        if (f == 0)
            yyserror("unknown identifier ", args->value);
        if (f->operator != getOffset)
            yyerror("variable in pattern must be local\n");
        ne = newExpression(makeReference);
        ne->u.o.base = f->u.o.base;
        ne->u.o.location = f->u.o.location;
        ne->u.o.symbol = f->u.o.symbol;
        p = newList((char *) ne, p);
    }

    e->u.p.args = p;
    e->resultType = booleanType;
    return e;
}
