/*
  support routines for lc compiler
*/

#include "lc.h"
#include "interp.h"
#include <string.h>
#include <stdio.h>
#include <malloc.h>

struct typeRecord* objectType;
struct typeRecord* booleanType;
struct typeRecord* integerType;
struct typeRecord* realType;
struct typeRecord* stringType;
struct typeRecord* trueType;
struct typeRecord* falseType;
struct typeRecord* relationType;
struct typeRecord* undefinedType;
struct typeRecord* ClassType;

extern int linenumber;

//---------------------lists ------------------

struct list* newList(char* v, struct list* ol)
{
    struct list* nl = (struct list*) malloc(sizeof(struct list));
    if (nl == 0)
    {
        yyerror("out of memory for compiler");
    }
    nl->value = v;
    nl->next = ol;

    return nl;
}


int length(struct list* p)
{
    int i = 0;
    for (; p; p = p->next) i++;
    return i;
}


static struct list* reverse2(struct list* todo, struct list* done)
{
    if (todo)
    {
        return reverse2(todo->next, newList(todo->value, done));
    }

    return done;
}


struct list* reverse(struct list* a)
{
    return reverse2(a, 0);
}


///  Symbol tables

struct symbolTableRecord* newSymbolTable
(
    enum tableTypes tt,
    struct symbolTableRecord* ctx
)
{
    struct symbolTableRecord* nctx =
        (struct symbolTableRecord*)malloc(sizeof(struct symbolTableRecord));
    if (nctx == 0)
    {
        yyerror("out of memory for compiler");
    }
    nctx->surroundingContext = ctx;
    nctx->size = 0;
    nctx->firstSymbol = 0;
    nctx->ttype = tt;
    nctx->definingType = 0;

    switch(tt)
    {
        case globals:
            break;

        case functionTable:
            nctx->u.f.argumentLocation = 4; // Check on this
            nctx->u.f.theFunctionSymbol = 0;
            break;

        case classTable:
            nctx->u.c.methodTable = 0;
            nctx->u.c.methodTableSize = 5;
            break;
    };

    return nctx;
}


struct symbolRecord* lookupLocal
(
    struct symbolTableRecord* syms,
    char* name
)
{
    for (struct list* p = syms->firstSymbol; p; p = p->next)
    {
        struct symbolRecord* s = (struct symbolRecord*) p->value;
        if (strcmp(name, s->name) == 0)
        {
            return s;
        }
    }

    if (syms->ttype == classTable)
    {
        for (struct list* p = syms->u.c.methodTable; p; p = p->next)
        {
            struct symbolRecord* s = (struct symbolRecord*) p->value;
            if (strcmp(name, s->name) == 0)
            {
                return s;
            }
        }
    }

    return 0;
}


void uniqueName(struct symbolTableRecord* syms, char* name)
{
    if (lookupLocal(syms, name))
    {
        yyserror("name must be unique within context: %s", name);
    }
}


struct symbolRecord* lookupSymbol
(
    struct symbolTableRecord* syms,
    char* name
)
{
    for (; syms; syms = syms->surroundingContext)
    {
        struct symbolRecord* s = lookupLocal(syms, name);
        if (s) return s;
    }

    yyserror("unknown identifier: %s", name);

    return 0;
}


void addNewSymbol(struct symbolTableRecord* syms, struct symbolRecord* s)
{
    syms->firstSymbol = newList((char*) s, syms->firstSymbol);
}


///  Symbol records themselves

struct symbolRecord* newSymbolRecord(char* n, enum symbolTypes st)
{
    struct symbolRecord* p =
        (struct symbolRecord*)malloc(sizeof(struct symbolRecord));
    if (p == 0)
    {
        yyerror("out of memory");
    }
    p->name = n;
    p->styp = st;

    return p;
}


void addConstant
(
    struct symbolTableRecord* syms,
    char* name,
    struct expressionRecord* value
)
{
    struct symbolRecord* s = newSymbolRecord(name, constSymbol);

    if (syms->ttype == classTable)
    {
        yyerror("current implementation does not permit constants in classes");
    }

    uniqueName(syms, name);

    s->u.s.val = value;
    s->u.s.location = syms->size++;
    s->u.s.typ = newConstantType(value->resultType);
    s->u.s.lineNumber = linenumber;
    addNewSymbol(syms, s);
}

struct symbolRecord* addVariable
(
    struct symbolTableRecord* syms,
    char* name,
    struct typeRecord* typ
)
{
    struct symbolRecord* s = newSymbolRecord(name, varSymbol);

    uniqueName(syms, name);

    s->u.v.typ = typ;
    s->u.v.location = syms->size++;
    addNewSymbol(syms, s);

    return s;
}


void addTypeDeclaration
(
    struct symbolTableRecord* syms,
    char* name,
    struct typeRecord* typ
)
{
    struct symbolRecord* s = newSymbolRecord(name, typeSymbol);

    uniqueName(syms, name);
    s->u.t.typ = typ;
    addNewSymbol(syms, s);
}

///  Type Record Manipulation

struct argumentRecord* newArgument
(
    char* n,
    struct typeRecord* t,
    enum forms f
)
{
    struct argumentRecord* a =
        (struct argumentRecord*) malloc(sizeof(struct argumentRecord));

    if (a == 0)
    {
        yyerror("out of memory for compiler");
    }

    a->name = n;
    a->theType = t;
    a->stform = f;

    return a;
}


struct list* buildArgumentList
(
    struct list* id,
    enum forms af,
    struct typeRecord* typ,
    struct list* soFar
)
{
    // Base case, got to the end
    if (id == 0)
    {
        return soFar;
    }
    // else recurse first
    soFar = buildArgumentList(id->next, af, typ, soFar);
    // then build current symbol
    soFar = newList((char*) newArgument(id->value, typ, af), soFar);

    return soFar;
}

struct symbolRecord* newClassSymbol
(
    struct symbolTableRecord* syms,
    struct symbolTableRecord* gsyms,
    char* name
)
{
    // Make sure name is unique or forward referenced
    struct symbolRecord* s = lookupLocal(syms, name);
    struct typeRecord* t = 0;
    if (s == 0) // new name
    {
        s = newSymbolRecord(name, classDefSymbol);
        s->u.c.location = syms->size++;
        addNewSymbol(syms, s);
        t = newTypeRecord(classType);
        s->u.c.typ = t;
    }
    else        // already defined
    {
        if (s->styp != classDefSymbol)
        {
            yyserror
            (
                "non class name %s used to define class",
                s->name
            );
        }
        t = s->u.c.typ;
        if (t == 0)
        {
            yyserror
            (
                "compiler error, missing type in class %s",
                s->name
            );
        }
        if (t->ttyp != classType)
        {
            yyserror("class %s has non class type field", s->name);
        }
        if (t->u.c.symbols)
        {
            yyserror("class %s multiply defined", s->name);
        }
    }

    struct symbolTableRecord* ns = newSymbolTable(classTable, syms);
    t->u.c.symbols = ns;
    ns->definingType = t;

    return s;
}


void fillInParent
(
    struct typeRecord* theClass,
    struct typeRecord* theParent,
    struct list* typeArgs
)
{
    if (theClass->ttyp != classType)
    {
        yyerror("fill in parent on non-class");
    }

    // first just set the parent field
    theClass->u.c.parent = theParent;
    struct typeRecord* u = 0;

    // Now fill in the qualified type, if necessary
    if (typeArgs)
    {
        if (theParent->ttyp != qualifiedType)
        {
            yyerror("type parameters used on non-qualified type");
        }
        u = checkQualifications(theParent, typeArgs);
        theParent = theParent->u.q.baseType;
    }

    if (theParent->ttyp != classType)
    {
        yyerror("parent field not class type");
    }

    // Then fill in the inherited fields
    struct symbolTableRecord* syms = theParent->u.c.symbols;
    struct symbolTableRecord* nsyms = theClass->u.c.symbols;
    // Fill in inherited data fields
    nsyms->size = syms->size;
    for (struct list* p = syms->firstSymbol; p; p = p->next)
    {
        struct symbolRecord* s = (struct symbolRecord*) p->value;
        if (s->styp == varSymbol)
        {
            struct symbolRecord* ns = newSymbolRecord(s->name, s->styp);
            ns->u.v.location = s->u.v.location;
            ns->u.v.typ = s->u.v.typ;
            addNewSymbol(nsyms, ns);
        }
    }

    // Fill in inherited methods
    nsyms->u.c.methodTableSize = syms->u.c.methodTableSize;

    for (struct list* p = syms->u.c.methodTable; p; p = p->next)
    {
        struct symbolRecord* s = (struct symbolRecord*) p->value;
        if (s->styp == functionSymbol)
        {
            struct symbolRecord* ns = newSymbolRecord(s->name, s->styp);
            ns->u.f.location = s->u.f.location;
            ns->u.f.code = s->u.f.code;
            ns->u.f.inherited = 1;
            ns->u.f.typ = fixResolvedType(s->u.f.typ, u);
            nsyms->u.c.methodTable =
            newList((char*) ns,
            nsyms->u.c.methodTable);
        }
    }
}


struct symbolTableRecord* addFunctionSymbol
(
    struct symbolTableRecord* syms,
    char* name,
    struct list* ta
)
{
    struct symbolTableRecord* ns = newSymbolTable(functionTable, syms);
    struct symbolRecord* fs = lookupLocal(syms, name);

    if (fs && syms->ttype != globals)
    {
        // Name already in symbol table
        if (fs->styp != functionSymbol)
        {
            yyerror("non function name redefined as function");
        }
        if (fs->u.f.inherited == 0)
        {
            yyerror("function multiply defined");
        }
        fs->u.f.inherited = 0;  // Now it is overridden
        fs->u.f.code = newStatement(nullStatement);
    }
    else       // Enter name into symbol table
    {
        fs = newSymbolRecord(name, functionSymbol);
        fs->u.f.code = newStatement(nullStatement);
        switch(syms->ttype)
        {
            case functionTable:
            case globals:
                fs->u.f.location = syms->size++;
                addNewSymbol(syms, fs);
                break;

            case classTable:
                // Add as a method, not a variable
                fs->u.f.location = syms->u.c.methodTableSize++;
                fs->u.f.inherited = 0;
                syms->u.c.methodTable =
                newList((char*) fs,
                syms->u.c.methodTable);
                break;
        }
    }

    // Make an empty function type for defining type
    fs->u.f.typ = ns->definingType = newTypeRecord(functionType);

    // If there are type arguments, put them into the symbol table
    if (ta)
    {
        fs->u.f.typ = newQualifiedType(ns, ta, fs->u.f.typ);
    }

    // If a method, add ``self'' to the symbol table
    if (syms->ttype == classTable)
    {
        struct symbolRecord* s = newSymbolRecord(newString("self"),
        argumentSymbol);
        s->u.a.location = 1;
        s->u.a.typ = newConstantType(syms->definingType);
        addNewSymbol(ns, s);
    }

    // Put symbol into the function symbol table
    ns->u.f.theFunctionSymbol = fs;

    // Return the new function symbol
    return ns;
}


struct list* enterFunctionArguments
(
    struct symbolTableRecord* syms,
    struct list* args
)
{
    // Enter function arguments from the list args
    // into the function symbol table
    // return a new list of the argument symbols

    struct list* typeList = 0;

    for (; args; args = args->next)
    {
        struct argumentRecord* ar = (struct argumentRecord*) args->value;
        struct symbolRecord* s = newSymbolRecord(ar->name, argumentSymbol);
        s->u.a.typ = ar->theType;
        s->u.a.form = ar->stform;
        s->u.a.location = syms->u.f.argumentLocation++;
        addNewSymbol(syms, s);
        typeList = newList((char*) s, typeList);
    }

    return reverse(typeList);
}


void addFunctionArguments
(
    struct symbolTableRecord* syms,
    struct list* args,
    struct typeRecord* rt
)
{
    struct typeRecord* t = syms->definingType;
    t->u.f.returnType = rt;
    t->u.f.argumentTypes = enterFunctionArguments(syms, args);
}


///  Initial creation

struct typeRecord* makeInitialClass
(
    struct symbolTableRecord* syms,
    char* name,
    struct typeRecord* p
)
{
    struct symbolRecord* s = newSymbolRecord(newString(name), classDefSymbol);
    struct typeRecord* t = newTypeRecord(classType);
    t->u.c.symbols = 0;
    t->u.c.parent = p;
    s->u.c.typ = t;
    s->u.c.location = syms->size++;
    addNewSymbol(syms, s);
    return t;
}


struct symbolTableRecord* initialCreation()
{
    struct symbolTableRecord* gs = newSymbolTable(globals, 0);

    // Create the constants NIL, true and false, but can't fill in types
    struct symbolRecord* nilsym = newSymbolRecord(newString("NIL"), varSymbol);
    nilsym->u.v.location = gs->size++;
    addNewSymbol(gs, nilsym);
    struct symbolRecord* truesym =
        newSymbolRecord(newString("true"), varSymbol);
    truesym->u.v.location = gs->size++;
    addNewSymbol(gs, truesym);
    struct symbolRecord* falsesym =
        newSymbolRecord(newString("false"), varSymbol);
    falsesym->u.v.location = gs->size++;
    addNewSymbol(gs, falsesym);

    // Create the initial classes
    objectType = makeInitialClass(gs, "object", 0);
    ClassType = makeInitialClass(gs, "Class", objectType);
    booleanType = makeInitialClass(gs, "boolean", objectType);
    integerType = makeInitialClass(gs, "integer", 0);
    realType = makeInitialClass(gs, "real", 0);
    stringType = makeInitialClass(gs, "string", objectType);
    trueType = makeInitialClass(gs, "True", booleanType);
    falseType = makeInitialClass(gs, "False", booleanType);
    undefinedType = makeInitialClass(gs, "Leda_undefined", objectType);

    // Now fill in types for symbols
    truesym->u.v.typ = trueType;
    falsesym->u.v.typ = falseType;
    nilsym->u.v.typ = undefinedType;

    // Finally, make the data-type relation
    relationType = newTypeRecord(functionType);
    struct symbolRecord* a =
        newSymbolRecord(newString("future"), argumentSymbol);
    a->u.a.location = 4;
    a->u.a.typ = relationType;
    a->u.a.form = byValue;
    relationType->u.f.argumentTypes = newList((char*) a, 0);
    relationType->u.f.returnType = booleanType;
    struct symbolRecord* s = newSymbolRecord(newString("relation"), typeSymbol);
    s->u.t.typ = relationType;
    addNewSymbol(gs, s);

    return gs;
}
