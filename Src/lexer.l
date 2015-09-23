%{
/*
    lexical analyzer for Leda revision August 26, 1993
*/

int     linenumber = 1;
char *      fileName = "no file";

static int input (void );

struct inputSources
{
    FILE * fd;
    struct inputSources * next;
    int linesave;
    char * filenamesave;
};

struct inputSources * inputSource = 0;

void openInputFile(char * name)
{
    struct inputSources * p;
    p = (struct inputSources *) malloc(sizeof(struct inputSources));
    if (p == 0)
        yyerror("out of memory");
    p->linesave = linenumber;
    p->filenamesave = fileName;
    p->next = inputSource;
    p->fd = fopen(name, "r");
    if (p->fd != NULL) {
        inputSource = p;
        linenumber = 1;
        fileName = name;
        }
    else
        yyserror("cannot open input file named %s", name);
    yyin = inputSource->fd;
}


int yywrap()
{
    /* should execution continue? */
    if (inputSource == 0)
        return 1;
    linenumber = inputSource->linesave;
    fileName = inputSource->filenamesave;
    inputSource = inputSource->next;
    if (inputSource == 0)
        return 1;
    yyin = inputSource->fd;
    return 0;
}

/*
    this is a silly hack to save space until we get real garbage
    collection -- intended to ensure we have only one copy of
    any string
*/
static char * strTable[1000];
static int strTop = 0;

char * newString(char * c)
{
    int i;
    char * p;

    for (i = 0; i < strTop; i++)
        if (strcmp(c, strTable[i]) == 0)
            return strTable[i];

    /* not found, make a new copy */
    p = (char *) malloc(strlen(c) + 1);
    if (p == 0)
        yyerror("out of memory for string");
    strcpy(p, c);
    strTable[strTop++] = p;
    if (strTop >= 1000)
        yyerror("too many strings");
    /*printf("created new string %s\n", c);*/
    return p;
}

char * newTextString(char * c)
{
    char * p = c;
    char * q;
    char * buffer;

        /* skip leading quote mark */
    if (p) p++;

        /* remove ending quote mark */
    p[strlen(p)-1] = '\0';

        /* make new string */
    return newString(p);
}

char * readLiteralString()
{
    char buffer[256];
    char * p = buffer;
    int c;

    while (c = input()) {
        switch(c) {
            case '\"': *p = '\0';
                return newString(buffer);

            case '\n':
                yyerror("unterminated literal string");

            case '\\':
                switch(c = input()) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'b': c = '\b'; break;
                    /* default drops into next case */
                }

            default:
                *p++ = c;
        }
    }
    yyerror("unterminated literal string");
}

static void skipComment()
{
    while(1)
        switch(input()) {
            case EOF: yyerror("unexpected eof in comment");
            case '\n': linenumber++; break;
            case '{': yyerror("nested comment");
            case '}': return;
            }
}

# define lexreturn(x) /*printf("token %s:%s\n", yytext, #x);*/ return(x)

%}

letter      [A-Za-z\_]
digit       [0-9]
id      {letter}({letter}|{digit})*
number      {digit}+(\.{digit}+)?(E[+\-]?{digit}+)?

%%

\n      linenumber++;
[ \t]+  /* white space, do nothing */;
"{"     {skipComment(); /* do nothing */ }
\"      {yylval.c = readLiteralString(); lexreturn(SCONSTANT);}
"begin"     {lexreturn(BEGINkw);}
"byName"    {lexreturn(BYNAME);}
"byRef"     {lexreturn(BYREF);}
"cfunction" {lexreturn(CFUNCTIONkw);}
"class"     {lexreturn(CLASSkw);}
"const"     {lexreturn(CONSTkw);}
"defined"   {lexreturn(DEFINEDkw);}
"do"        {lexreturn(DOkw);}
"else"      {lexreturn(ELSEkw);}
"end"       {lexreturn(ENDkw);}
"for"       {yylval.i = linenumber; lexreturn(FORkw);}
"function"  {lexreturn(FUNCTIONkw);}
"if"        {yylval.i = linenumber; lexreturn(IFkw);}
"include"   {lexreturn(INCLUDEkw);}
"is"        {lexreturn(ISkw);}
"of"        {lexreturn(OFkw);}
"return"    {yylval.i = linenumber; lexreturn(RETURNkw);}
"then"      {lexreturn(THENkw);}
"to"        {lexreturn(TOkw);}
"type"      {lexreturn(TYPEkw);}
"var"       {lexreturn(VARkw);}
"while"     {yylval.i = linenumber; lexreturn(WHILEkw);}
{id}        {yylval.c = newString(yytext); lexreturn(ID);}
{digit}+    {yylval.i = atoi(yytext); lexreturn(ICONSTANT);}
{digit}+"."{digit}+ {yylval.r = atof(yytext); lexreturn(RCONSTANT);}
":"     {lexreturn(COLON);}
";"     {lexreturn(SEMI);}
"."     {lexreturn(PERIOD);}
":="        {lexreturn(ASSIGN);}
","     {lexreturn(COMMA);}
"->"        {lexreturn(ARROW);}
"<-"        {lexreturn(LEFTARROW);}
"<"     {yylval.c = "less"; lexreturn(RELATIONALOP);}
"<="        {yylval.c = "lessEqual"; lexreturn(RELATIONALOP);}
">"     {yylval.c = "greater"; lexreturn(RELATIONALOP);}
">="        {yylval.c = "greaterEqual"; lexreturn(RELATIONALOP);}
"["     {lexreturn(LEFTBRACK);}
"]"     {lexreturn(RIGHTBRACK);}
"("     {lexreturn(LEFTPAREN);}
")"     {lexreturn(RIGHTPAREN);}
"="     {yylval.c = "equals"; lexreturn(RELATIONALOP);}
"<>"        {yylval.c = "notEquals"; lexreturn(RELATIONALOP);}
"=="        {yylval.c = "sameAs"; lexreturn(RELATIONALOP);}
"~="        {yylval.c = "notSameAs"; lexreturn(RELATIONALOP);}
"+"     {yylval.c = "plus"; lexreturn(PLUSop);}
"-"     {yylval.c = "minus"; lexreturn(MINUSop);}
"*"     {yylval.c = "times"; lexreturn(TIMESop);}
"/"     {yylval.c = "divide"; lexreturn(TIMESop);}
"%"     {yylval.c = "remainder"; lexreturn(TIMESop);}
"~"     {lexreturn(NOT);}
"&"     {yylval.c = "and"; lexreturn(ANDop);}
"|"     {yylval.c = "or"; lexreturn(ORop);}
.       {fprintf(stderr,"character %c %d\n", yytext[0], yytext[0]);
            yyerror("unknown character"); }
