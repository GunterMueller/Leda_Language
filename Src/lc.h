/*
  structures used in the lc leda compiler
*/


/*
  list is a polymorphic list structure.
  casts are used to convert pointers of various types to
  pointers to char
*/

struct list
{
    char* value;
    struct list * next;
};

struct list* newList(char*, struct list*);

struct list* reverse(struct list*);

///  Symbol Tables
// are represented just as lists of symbol records

enum forms
{
    byValue,
    byName,
    byReference
};

enum symbolTypes
{
    varSymbol,
    typeSymbol,
    classDefSymbol,
    functionSymbol,
    argumentSymbol,
    constSymbol
};

struct symbolRecord
{
    char* name;
    enum symbolTypes styp;
    union
    {
        struct         // variable Symbols
        {
            int location;
            struct typeRecord* typ;
        } v;

        struct         // type Symbols
        {
            struct typeRecord* typ;
        } t;

        struct         // class definition symbols
        {
            int location;
            struct typeRecord* typ;
        } c;

        struct         // function symbols
        {
            int location;
            int inherited;
            struct typeRecord* typ;
            struct statementRecord* code;
        } f;

        struct         // arguments
        {
            int location;
            struct typeRecord* typ;
            enum forms form;
        } a;

        struct         // constant symbols
        {
            int location;
            struct expressionRecord* val;
            struct typeRecord* typ;
            int lineNumber;
        } s;
    } u;
};

enum tableTypes
{
    globals,
    functionTable,
    classTable
};

struct symbolTableRecord
{
    enum tableTypes ttype;
    struct symbolTableRecord* surroundingContext;
    struct typeRecord* definingType;
    struct list* firstSymbol;
    int size;

    union
    {
        struct         //  function tables
        {
            struct symbolRecord* theFunctionSymbol;
            int argumentLocation;
        } f;

        struct        // class tables
        {
            struct list* methodTable;
            int methodTableSize;
        } c;
    } u;
};

struct symbolTableRecord* newSymbolTable
(
    enum tableTypes,
    struct symbolTableRecord*
);

struct symbolRecord* newSymbolRecord(char*, enum symbolTypes);

struct symbolRecord* addVariable
(
    struct symbolTableRecord*,
    char*,
    struct typeRecord*
);

void addNewSymbol(struct symbolTableRecord*, struct symbolRecord*);

struct symbolRecord* lookupSymbol(struct symbolTableRecord*, char*);

struct symbolTableRecord* addFunctionSymbol
(
    struct symbolTableRecord* syms,
    char* name,
    struct list* ta
);

struct symbolRecord* argumentNumber(struct typeRecord* t, int n);

///  Types

struct typeRecord* newFunctionType(struct list*, struct typeRecord*);

enum typeForms
{
    functionType,
    classType,
    qualifiedType,
    unresolvedType,
    resolvedType,
    classDefType,
    constantType
};

struct typeRecord
{
    enum typeForms ttyp;
    union
    {

        struct  // functionType
        {
            struct list* argumentTypes;
            struct typeRecord* returnType;
        }f;

        struct         // class type
        {
            struct typeRecord* parent;
            struct symbolTableRecord* symbols;
            struct ledaValue * staticTable;
        }c;

        struct  // qualified types
        {
            struct list* qualifiers;
            struct typeRecord* baseType;
        } q;

        struct  // unresolved types, constant types
        {
            struct typeRecord* baseType;
        } u;

        struct  // resolved types
        {
            struct list* patterns;
            struct list* replacements;
            struct typeRecord* baseType;
        } r;
    } u ;
};

struct typeRecord* newTypeRecord(enum typeForms);

struct typeRecord* newConstantType(struct typeRecord*);

void addFunctionArguments
(
    struct symbolTableRecord*,
    struct list*,
    struct typeRecord*
);

struct list* enterFunctionArguments
(
    struct symbolTableRecord*,
    struct list*
);

struct symbolRecord* newClassSymbol
(
    struct symbolTableRecord*,
    struct symbolTableRecord*,
    char*
);

struct typeRecord* newQualifiedType
(
    struct symbolTableRecord* syms,
    struct list* qualifiers,
    struct typeRecord* t
);

struct typeRecord* fixResolvedType(struct typeRecord*, struct typeRecord*);

void fillInClass(struct typeRecord*, struct list*, struct typeRecord*);

struct typeRecord* checkFunction(struct typeRecord* t);

struct list* newTypelist
(
    struct typeRecord* t,
    enum forms stform,
    struct list* old
);

extern struct typeRecord* objectType;
extern struct typeRecord* booleanType;
extern struct typeRecord* integerType;
extern struct typeRecord* realType;
extern struct typeRecord* stringType;
extern struct typeRecord* trueType;
extern struct typeRecord* falseType;
extern struct typeRecord* relationType;;
extern struct typeRecord* undefinedType;;
extern struct typeRecord* ClassType;

///  Arguments

struct argumentRecord
{
    char* name;
    enum forms stform;
    struct typeRecord* theType;
};

struct argumentRecord* newArgument
(
    char*,
    struct typeRecord*,
    enum forms
);

struct list* buildArgumentList
(
    struct list*,
    enum forms,
    struct typeRecord*,
    struct list*
);


// =============================================


struct symbolTableRecord* initialCreation();

struct typeRecord* checkType(struct symbolRecord*);
struct typeRecord* checkClass(struct typeRecord*);
struct typeRecord* checkQualifications(struct typeRecord*, struct list*);
struct expressionRecord* checkExpression(struct symbolRecord*);

char* newString(char*);

int yyerror(char*);
void yyserror(char*, char*);

void addConstant
(
    struct symbolTableRecord* syms,
    char* name,
    struct expressionRecord* value
);

void addTypeDeclaration
(
    struct symbolTableRecord* syms,
    char* name,
    struct typeRecord* typ
);

void fillInParent
(
    struct typeRecord* theClass,
    struct typeRecord* theParent,
    struct list* typeArgs
);

int typeConformable(struct typeRecord* a, struct typeRecord* b);

int length(struct list* p);
