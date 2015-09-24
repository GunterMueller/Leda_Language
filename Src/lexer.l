%{
/*
    lexical analyzer for Leda revision August 26, 1993
    Updated for flex: 23/8/2015
*/

int     linenumber = 1;
char*   fileName = "no file";

static int input(void);

struct inputSources
{
    YY_BUFFER_STATE buff;
    struct inputSources* next;
    int lineSave;
    char* fileNameSave;
};

struct inputSources * inputSource = 0;


void openInputFile(char * name)
{
    struct inputSources *p =
        (struct inputSources*) malloc(sizeof(struct inputSources));

    if (p == 0)
    {
        yyerror("out of memory");
    }

    p->lineSave = linenumber;
    p->fileNameSave = fileName;
    p->next = inputSource;

    FILE *fd = fopen(name, "r");
    if (fd != NULL)
    {
        inputSource = p;
        p->buff = yy_create_buffer(fd, YY_BUF_SIZE);

        linenumber = 1;
        fileName = name;
    }
    else
    {
        yyserror("cannot open input file named %s", name);
    }

    yy_switch_to_buffer(p->buff);
}


int yywrap(void)
{
    yy_delete_buffer(YY_CURRENT_BUFFER);

    // Should execution continue?
    if (inputSource == 0) return 1;

    linenumber = inputSource->lineSave;
    fileName = inputSource->fileNameSave;

    struct inputSources *next = inputSource->next;
    free(inputSource);
    inputSource = next;

    if (inputSource == 0) return 1;

    yy_switch_to_buffer(inputSource->buff);

    return 0;
}


// This is a silly hack to save space until we get real garbage collection --
// intended to ensure we have only one copy of any string
static char* strTable[1000];
static int strTop = 0;

char* newString(char * c)
{
    for (int i=0; i<strTop; i++)
    {
        if (strcmp(c, strTable[i]) == 0)
        {
            return strTable[i];
        }
    }

    // Not found, make a new copy
    char* p = (char *) malloc(strlen(c) + 1);
    if (p == 0)
    {
        yyerror("out of memory for string");
    }
    strcpy(p, c);
    strTable[strTop++] = p;
    if (strTop >= 1000)
    {
        yyerror("too many strings");
    }

    #ifdef DEBUG
        printf("created new string %s\n", c);
    #endif

    return p;
}


char* newTextString(char* c)
{
    char* p = c;

    // Skip leading quote mark
    if (p) p++;

    // Remove ending quote mark
    p[strlen(p)-1] = '\0';

    // Make new string
    return newString(p);
}


char* readLiteralString()
{
    char buffer[256];
    char* p = buffer;

    int c;
    while ((c = input()))
    {
        switch(c)
        {
            case '\"': *p = '\0';
                return newString(buffer);

            case '\n':
                yyerror("unterminated literal string");

            case '\\':
                switch(c = input())
                {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'b': c = '\b'; break;
                    // default drops into next case
                }

            default:
                *p++ = c;
        }
    }

    yyerror("unterminated literal string");

    return 0;
}


static void skipComment()
{
    while(1)
    {
        switch(input())
        {
            case EOF: yyerror("unexpected eof in comment");
            case '\n': linenumber++; break;
            case '{': yyerror("nested comment");
            case '}': return;
        }
    }
}

#ifdef DEBUG
    #define lexReturn(x) printf("token %s:%s\n", yytext, #x); return(x)
#else
    #define lexReturn(x) return(x)
#endif

%}

letter      [A-Za-z\_]
digit       [0-9]
id          {letter}({letter}|{digit})*
number      {digit}+(\.{digit}+)?(E[+\-]?{digit}+)?

%%

\n          linenumber++;
[ \t]+      /* white space, do nothing */;
"{"         {skipComment(); /* do nothing */ }
\"          {yylval.c = readLiteralString(); lexReturn(SCONSTANT);}
"begin"     {lexReturn(BEGINkw);}
"byName"    {lexReturn(BYNAME);}
"byRef"     {lexReturn(BYREF);}
"cfunction" {lexReturn(CFUNCTIONkw);}
"class"     {lexReturn(CLASSkw);}
"const"     {lexReturn(CONSTkw);}
"defined"   {lexReturn(DEFINEDkw);}
"do"        {lexReturn(DOkw);}
"else"      {lexReturn(ELSEkw);}
"end"       {lexReturn(ENDkw);}
"for"       {yylval.i = linenumber; lexReturn(FORkw);}
"function"  {lexReturn(FUNCTIONkw);}
"if"        {yylval.i = linenumber; lexReturn(IFkw);}
"include"   {lexReturn(INCLUDEkw);}
"is"        {lexReturn(ISkw);}
"of"        {lexReturn(OFkw);}
"return"    {yylval.i = linenumber; lexReturn(RETURNkw);}
"then"      {lexReturn(THENkw);}
"to"        {lexReturn(TOkw);}
"type"      {lexReturn(TYPEkw);}
"var"       {lexReturn(VARkw);}
"while"     {yylval.i = linenumber; lexReturn(WHILEkw);}
{id}        {yylval.c = newString(yytext); lexReturn(ID);}
{digit}+    {yylval.i = atoi(yytext); lexReturn(ICONSTANT);}
{digit}+"."{digit}+ {yylval.r = atof(yytext); lexReturn(RCONSTANT);}
":"         {lexReturn(COLON);}
";"         {lexReturn(SEMI);}
"."         {lexReturn(PERIOD);}
":="        {lexReturn(ASSIGN);}
","         {lexReturn(COMMA);}
"->"        {lexReturn(ARROW);}
"<-"        {lexReturn(LEFTARROW);}
"<"         {yylval.c = "less"; lexReturn(RELATIONALOP);}
"<="        {yylval.c = "lessEqual"; lexReturn(RELATIONALOP);}
">"         {yylval.c = "greater"; lexReturn(RELATIONALOP);}
">="        {yylval.c = "greaterEqual"; lexReturn(RELATIONALOP);}
"["         {lexReturn(LEFTBRACK);}
"]"         {lexReturn(RIGHTBRACK);}
"("         {lexReturn(LEFTPAREN);}
")"         {lexReturn(RIGHTPAREN);}
"="         {yylval.c = "equals"; lexReturn(RELATIONALOP);}
"<>"        {yylval.c = "notEquals"; lexReturn(RELATIONALOP);}
"=="        {yylval.c = "sameAs"; lexReturn(RELATIONALOP);}
"~="        {yylval.c = "notSameAs"; lexReturn(RELATIONALOP);}
"+"         {yylval.c = "plus"; lexReturn(PLUSop);}
"-"         {yylval.c = "minus"; lexReturn(MINUSop);}
"*"         {yylval.c = "times"; lexReturn(TIMESop);}
"/"         {yylval.c = "divide"; lexReturn(TIMESop);}
"%"         {yylval.c = "remainder"; lexReturn(TIMESop);}
"~"         {lexReturn(NOT);}
"&"         {yylval.c = "and"; lexReturn(ANDop);}
"|"         {yylval.c = "or"; lexReturn(ORop);}
.           {
                fprintf(stderr,"character %c %d\n", yytext[0], yytext[0]);
                yyerror("unknown character");
            }
