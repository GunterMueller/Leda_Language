/*
  types.c -- routines having to do with type record
  for the leda compiler
*/

# include "lc.h"

struct typeRecord * newConstantType(struct typeRecord * b)
{
    struct typeRecord * t = newTypeRecord(constantType);
    t->u.u.baseType = b;
    return t;
}

struct typeRecord * checkType(struct symbolRecord * s)
{
    switch(s->styp) {

        case typeSymbol:
            return s->u.t.typ;

        case classDefSymbol:
            return s->u.c.typ;
    }
    yyserror("non-type identifier %s used where type expected", s->name);
    return 0;
}

struct typeRecord * checkClass(struct typeRecord * t)
{
    switch(t->ttyp) {

        case classType:
            return t;

        case qualifiedType:
            return checkClass(t->u.q.baseType);
    }

    /* not a class */
    return 0;
}

struct typeRecord * checkQualifications(
    struct typeRecord * qt, struct list * args)
{
    struct list * p;
    struct list * q;
    struct typeRecord * a;
    struct argumentRecord * b;
    struct typeRecord * nt;
    struct symbolRecord * s;

    /* make sure qt is a qualified type and sizes match */
    if (qt->ttyp != qualifiedType) {
        printf("typs is %d\n", qt->ttyp);
        yyerror("cannot parameterize nonqualified type");
    }

    if (length(qt->u.q.qualifiers) != length(args))
        yyerror("wrong number of qualifiers");

    /* check that types are conformable */
    p = qt->u.q.qualifiers;
    q = args;

    for (; p; p = p->next, q = q->next) {
        a = (struct typeRecord *) p->value;
        if (a->ttyp != unresolvedType)
            yyerror("internal compiler error: qualifications");
        s = (struct symbolRecord *) q->value;
        if (s->styp != argumentSymbol)
            yyerror("compiler error in checkQualifications");
        if (s->u.a.form != byValue)
            yyerror("cannot use storage form in this context");
        if (! typeConformable(a->u.u.baseType, s->u.a.typ)) {
            printf("ttyps are %d %d\n", a->u.u.baseType->ttyp, s->u.a.typ->ttyp);
            yyerror("invalid type parameterization");
        }
    }

    nt = newTypeRecord(resolvedType);
    nt->u.r.baseType = qt->u.q.baseType;
    nt->u.r.patterns = qt->u.q.qualifiers;
    nt->u.r.replacements = args;
    return nt;
}

struct typeRecord * fixResolvedType(struct typeRecord * t,
struct typeRecord * rt)
{
    struct typeRecord * nt;

    if ((t == 0) || (rt == 0))
        return t;
    else {
        struct list * p = rt->u.r.patterns;
        struct list * q = rt->u.r.replacements;

        for (; p; p = p->next, q = q->next) {
            struct typeRecord * pa = (struct typeRecord *) p->value;
            struct symbolRecord * qa =
            (struct symbolRecord *) q->value;

            if (t == pa)
                return qa->u.a.typ;
        }

        nt = newTypeRecord(resolvedType);

        nt->u.r.patterns = rt->u.r.patterns;
        nt->u.r.replacements = rt->u.r.replacements;
        nt->u.r.baseType = t;
    }
    return nt;
}

struct typeRecord * checkFunction(struct typeRecord * t)
{
    if (t->ttyp == functionType)
        return t;
    if (t->ttyp == resolvedType)
        return checkFunction(t->u.r.baseType);
    return 0;
}

struct symbolRecord * argumentNumber(struct typeRecord * t, int n)
{
    if (t->ttyp == functionType) {
        struct list * p;
        for (p = t->u.f.argumentTypes; n > 0; n--, p = p->next) ;
        return (struct symbolRecord *) p->value;
    }
    else if (t->ttyp == resolvedType) {
        struct symbolRecord * s = argumentNumber(t->u.r.baseType, n);
        struct symbolRecord * s2 = newSymbolRecord(s->name, s->styp);
        s2->u.a.location = s->u.a.location;
        s2->u.a.form = s->u.a.form;
        s2->u.a.typ = fixResolvedType(s->u.a.typ, t);
        return s2;
    }
    yyerror("argumentNumber impossible case");
}

struct typeRecord * newTypeRecord(enum typeForms tt)
{
    struct typeRecord * t = (struct typeRecord *)
    malloc(sizeof(struct typeRecord));
    if (t == 0)
        yyerror("out of memory");
    t->ttyp = tt;
    switch(tt) {
        case functionType:
            t->u.f.argumentTypes = 0;
            t->u.f.returnType = 0;
            break;
        case classType:
            t->u.c.parent = 0;
            t->u.c.symbols = 0;
            break;
        case qualifiedType:
            t->u.q.qualifiers = 0;
            t->u.q.baseType = 0;
            break;
        case unresolvedType:
            t->u.u.baseType = 0;
    };
    return t;
}

struct typeRecord * newFunctionType(
    struct list * args,
    struct typeRecord * result)
{
    struct typeRecord * t = newTypeRecord(functionType);
    t->u.f.argumentTypes = args;
    t->u.f.returnType = result;
    return t;
}

static int functionTypeConformable(
    struct typeRecord * a,
    struct typeRecord * b)
{
    struct list * p;
    int i;
    struct typeRecord * rt = 0;

    /* safety check, make sure both a and b are function types */
    if (a->ttyp != functionType)
        yyerror("failure of safety check in functionTypeConformable");

    p = a->u.f.argumentTypes;

    /* check return types and argument lengths */
    if (b->ttyp == functionType) {
        if (! typeConformable(a->u.f.returnType,
        b->u.f.returnType))
            return 0;
        if (length(p) != length(b->u.f.argumentTypes))
            return 0;
    }

    else if (b->ttyp == resolvedType) {
        rt = checkFunction(b);
        if (rt == 0)
            return 0;
        if (a == rt)    /* if they match exactly, don't bother */
            return 1;
        if (! typeConformable(a->u.f.returnType,
        fixResolvedType(rt->u.f.returnType, b))) {
            return 0;
        }
        if (length(p) != length(rt->u.f.argumentTypes))
            return 0;
    }
    else {
        return 0;
    }

    /* finally make sure argument lists match */
    for (i = 0; p; p = p->next, i++ ) {
        struct symbolRecord * pa =
        (struct symbolRecord *)p->value;
        struct symbolRecord * qa =
        argumentNumber(b, i);
        if (pa->u.a.form != qa->u.a.form)
            return 0;
        if (! typeConformable(pa->u.a.typ, qa->u.a.typ))
            return 0;
    }

    /* everything seems to fit, lets go! */
    return 1;
}

int typeConformable(struct typeRecord * a, struct typeRecord * b)
{
    struct list * p;
    struct list * q;


    if (a == b) return 1;
    if ((a == 0) || (b == 0)) return 0;
    if (b == undefinedType) return 1; /* NIL is polymorphic */

    /* if the right is a constant, check base */
    if (b->ttyp == constantType)
        return typeConformable(a, b->u.u.baseType);

    /* with unresolved types can only check the base */
    if (a->ttyp == unresolvedType)
        return typeConformable(a->u.u.baseType, b);
    if (b->ttyp == unresolvedType)
        return typeConformable(a, b->u.u.baseType);

    /*printf("typeConformable types %d %d\n", a->ttyp, b->ttyp);*/
    switch(a->ttyp) {
        case functionType:
            /* if (b->ttyp == resolvedType)
               return typeConformable(a, b->u.r.baseType);*/
            return functionTypeConformable(a, b);

        case classType:
            if (b->ttyp == functionType) {
                /* compatible only with object */
                if (a == objectType)
                    return 1;
                return 0;
            }
            if (b->ttyp == classDefType) {
                if (a == ClassType)
                    return 1;
            }
            if (b->ttyp == classType) {
                if (a == b) return 1;
                if (b == b->u.c.parent) return 0;
                return typeConformable(a, b->u.c.parent);
            }
            else if (b->ttyp == qualifiedType)
                return typeConformable(a, b->u.q.baseType);
            else if (b->ttyp == resolvedType)
                return typeConformable(a, b->u.r.baseType);
            break;

        case qualifiedType:
            return typeConformable(a->u.q.baseType, b);

        case resolvedType:
            /* useless resolvedType record */
            return typeConformable(a->u.r.baseType, b);

        case constantType:
            return 0;
    }
    return 0;
}

struct typeRecord * newQualifiedType(
    struct symbolTableRecord * syms,
    struct list * qualifiers,
    struct typeRecord * t)
{
    struct list * p;
    struct argumentRecord * a;
    struct typeRecord * nt;
    struct symbolRecord * ns;
    struct typeRecord * q = newTypeRecord(qualifiedType);
    struct list * ql = 0;

    q->u.q.baseType = t;

    /* put each type name into symbol table */
    /* also make sure there aren't any storage modifiers */
    for (p = qualifiers; p; p = p->next) {
        a = (struct argumentRecord *) p->value;
        if (a->stform != byValue)
            yyerror("type parameters cannot have name or reference form");
        /* now make a new type record */
        nt = newTypeRecord(unresolvedType);
        nt->u.u.baseType = a->theType;
        ns = newSymbolRecord(a->name, typeSymbol);
        ns->u.t.typ = nt;
        addNewSymbol(syms, ns);
        ql = newList((char *) nt, ql);
    }

    q->u.q.qualifiers = reverse(ql);

    return q;
}

struct list * newTypelist(struct typeRecord * t, enum forms stform,
struct list * old)
{
    struct symbolRecord * s = newSymbolRecord(0, argumentSymbol);
    struct argumentRecord * a = newArgument(0, t, stform);
    s->u.a.location = 0;
    s->u.a.typ = t;
    s->u.a.form = stform;
    return newList((char *) s, old);
}
