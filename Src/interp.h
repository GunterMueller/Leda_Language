/*
  data fields used by the Leda interpreter
*/

extern struct typeRecord * booleanType;
extern struct typeRecord * integerType;
extern struct typeRecord * stringType;
extern struct typeRecord * trueType;
extern struct typeRecord * falseType;

/*
  Statements =============================
*/

enum statements {
    makeLocalsStatement,
    expressionStatement,
    returnStatement,
    conditionalStatement,
    nullStatement,
    tailCall,
};

struct statementRecord {
    char * fileName;
    int lineNumber;
    enum statements statementType;
    struct statementRecord * next;

    union {
        struct {    /* if-then conditional */
            struct expressionRecord * expr;
            struct statementRecord * falsePart;
        } c;
        struct {    /* procedure call, return */
            struct expressionRecord * e;
        } r;
        struct {    /* make locals */
            int size;
        } k;
    } u;
};

struct statementRecord * newStatement(enum statements);

struct statementRecord * genAssignmentStatement(
    struct expressionRecord *, struct expressionRecord *);

struct statementRecord * genReturnStatement(
    struct symbolTableRecord *, struct expressionRecord *);

struct statementRecord * genConditionalStatement( int,
struct expressionRecord *,
struct statementRecord *,
struct statementRecord *,
struct statementRecord *,
struct statementRecord *,
struct statementRecord *);

struct statementRecord * genWhileStatement(int,
struct expressionRecord *,
struct statementRecord *,
struct statementRecord *,
struct statementRecord *);

struct statementRecord * generateArithmeticForStatement(int,
struct symbolTableRecord * syms,
struct expressionRecord * target,
struct expressionRecord * start,
struct expressionRecord * limit,
struct statementRecord * stFirst,
struct statementRecord * stLast,
struct statementRecord * nullState);

struct statementRecord * genBody(
    struct symbolTableRecord *, struct statementRecord *);

/*
  Expressions =============================
*/

enum instructions {
    getCurrentContext,
    getOffset,
    getGlobalOffset,
    makeReference,
    genIntegerConstant,
    genStringConstant,
    genRealConstant,
    assignment,
    makeMethodContext,
    makeClosure,
    doFunctionCall,
    doSpecialCall,
    evalThunk,
    evalReference,
    buildInstance,
    commaOp,
    patternMatch,
};

struct expressionRecord {
    enum instructions operator;
    struct typeRecord * resultType;
    union {

        struct {    /* offset, reference, etc */
            int location;
            struct expressionRecord * base;
            char * symbol;
        } o;

        struct {    /* assignment */
            struct expressionRecord * left;
            struct expressionRecord * right;
            char * symbol;
        } a;

        struct {    /* do special */
            int index;
            struct list * args;
        } c;

        struct {    /* make closure */
            struct expressionRecord * context;
            struct statementRecord * code;
            char * functionName;
        } l;

        struct {    /* function call */
            struct expressionRecord * fun;
            char * symbol;
            struct list * args;
        } f;

        struct {    /* genIntegerConstant */
            int value;
        } i;

        struct {    /* genStringConstant */
            char * value;
        } s;

        struct {    /* genRealConstant */
            double value;
        } r;

        struct {    /* buildInstance */
            struct expressionRecord * table;
            int size;
            struct list * args;
        } n;

        struct {    /* pattern match */
            struct expressionRecord * base;
            struct expressionRecord * class;
            struct list * args;
        } p;
    } u;

};

struct expressionRecord * newExpression(enum instructions);

extern char * specialFunctionNames[];

/*
  code generation
*/

struct statementRecord * genExpressionStatement(struct expressionRecord * e);

struct expressionRecord * lookupIdentifier(struct symbolTableRecord *, char *);

struct expressionRecord * lookupField(
    struct expressionRecord *, struct typeRecord *, char *);

struct expressionRecord * integerConstant(int);
struct expressionRecord * realConstant(double);
struct expressionRecord * stringConstant(char *);

struct expressionRecord * generateFunctionCall(
    struct symbolTableRecord *,
    struct expressionRecord *,
    struct list *, int);

struct expressionRecord * generateCFunctionCall(
    char *, struct list *, struct typeRecord *);

struct expressionRecord * generateUnaryOperator(
    struct symbolTableRecord *,
    char *, struct expressionRecord *);

struct expressionRecord * generateBinaryOperator(
    struct symbolTableRecord * syms,
    char *, struct expressionRecord *, struct expressionRecord *);

struct expressionRecord * genAssignment(
    struct expressionRecord *,
    struct expressionRecord *);

struct expressionRecord * generateLeftArrow(
    struct symbolTableRecord * syms,
    struct expressionRecord *, struct expressionRecord *);

struct symbolTableRecord * generateFunctionExpression(
    struct symbolTableRecord *,
    struct list *, struct typeRecord *);

struct expressionRecord * relationCheck(
    struct symbolTableRecord *,
    struct expressionRecord *);

struct expressionRecord * booleanCheck(
    struct symbolTableRecord *,
    struct expressionRecord *);

struct expressionRecord * generateForRelation(
    struct symbolTableRecord * syms,
    struct expressionRecord * relExp,
    struct expressionRecord * stopExp,
    struct statementRecord * stateFirst,
    struct statementRecord * stateLast);

struct expressionRecord * generateArrayLiteral(
    struct symbolTableRecord * syms,
    struct list *);

struct expressionRecord * genPatternMatch(
    struct symbolTableRecord * syms,
    struct expressionRecord * base,
    struct expressionRecord * theclass,
    struct list * args);

void buildClassTable(struct symbolRecord *);

void beginInterpreter(struct symbolTableRecord *, struct statementRecord *);
