%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "syntax_tree.h"

// external functions from lex
extern int yylex();
extern int yyparse();
extern int yyrestart();
extern FILE * yyin;

// external variables from lexical_analyzer module
extern int lines;
extern char * yytext;
extern int pos_end;
extern int pos_start;

// Global syntax tree
syntax_tree *gt;

// Error reporting
void yyerror(const char *s);

// Helper functions written for you with love
syntax_tree_node *node(const char *node_name, int children_num, ...);
%}

/* TODO: Complete this definition.
   Hint: See pass_node(), node(), and syntax_tree.h.
         Use forward declaring. */
%union {struct syntax_tree_node* node}

/* TODO: Your tokens here. */
%token <node> ERROR
%token <node> ADD
%token <node> IF
%token <node> ELSE
%token <node> TYPE_INT
%token <node> TYPE_FLOAT
%token <node> TYPE_VOID
%token <node> RETURN
%token <node> WHILE
%token <node> MINUS
%token <node> STAR
%token <node> DIV
%token <node> LESS
%token <node> LESSEQU
%token <node> MORE
%token <node> MOREEQU
%token <node> EQUAL
%token <node> NOTEQU
%token <node> ASSIGNOP
%token <node> SEMI
%token <node> COMMA
%token <node> LP
%token <node> RP
%token <node> LB
%token <node> RB
%token <node> LC
%token <node> RC
%token <node> INTEGER
%token <node> FLOATPOINT
%token <node> ID
%token <node> COMMENT
%type <node> program 


%start program

%%
/* TODO: Your rules here. */

/* Example:
program: declaration-list {$$ = node( "program", 1, $1); gt->root = $$;}
       ;
*/

program: declaration-list {$<node>$ = node( "program", 1, $<node>1); gt->root = $$;}
       ;
declaration-list: declaration-list declaration {$<node>$ = node("declaration-list", 2, $<node>1, $<node>2);}
                | declaration{$<node>$ = node("declaration-list", 1, $<node>1);}
                ;
declaration: var-declaration {$<node>$ = node("declaration", 1, $<node>1);}
           | fun-declaration {$<node>$ = node("declaration", 1, $<node>1);}
           ;
var-declaration: type-specifier ID SEMI {$<node>$ = node("var-declaration", 3, $<node>1, $2, $3);}
               | type-specifier ID LB INTEGER RB SEMI {$<node>$ = node("var-declaration", 6, $<node>1, $2, $3, $4, $5, $6);}
               ;
type-specifier: TYPE_INT {$<node>$ = node("type-specifier", 1, $1);}
              | TYPE_FLOAT {$<node>$ = node("type-specifier", 1, $1);}
              | TYPE_VOID {$<node>$ = node("type-specifier", 1, $1);}
              ;
fun-declaration: type-specifier ID LP params RP compound-stmt {$<node>$ = node("fun-declaration", 6, $<node>1, $2, $3, $<node>4, $5, $<node>6);}
                ;
params: param-list {$<node>$ = node("params", 1, $<node>1);}
      | TYPE_VOID {$<node>$ = node("params", 1, $1);}
      ;
param-list: param-list COMMA param {$<node>$ = node("param-list", 3, $<node>1, $2, $<node>3);}
          | param {$<node>$ = node("param-list", 1, $<node>1);}
          ;
param: type-specifier ID {$<node>$ = node("param", 2, $<node>1, $2);}
     | type-specifier ID LB RB {$<node>$ = node("param", 4, $<node>1, $2, $3, $4);}
     ;
compound-stmt: LC local-declarations statement-list RC {$<node>$ = node("compound-stmt", 4, $1, $<node>2, $<node>3, $4);}
            ;
local-declarations: local-declarations var-declaration {$<node>$ = node("local-declarations", 2, $<node>1, $<node>2);}
                  | {$<node>$ = node("local-declarations", 0);}
                  ;
statement-list: statement-list statement {$<node>$ = node("statement-list", 2, $<node>1, $<node>2);}
              | {$<node>$ = node("statement-list", 0);}
              ;
statement: expression-stmt {$<node>$ = node("statement", 1, $<node>1);}
         | compound-stmt {$<node>$ = node("statement", 1, $<node>1);}
         | selection-stmt {$<node>$ = node("statement", 1, $<node>1);}
         | iteration-stmt {$<node>$ = node("statement", 1, $<node>1);}
         | return-stmt {$<node>$ = node("statement", 1, $<node>1);}
         ;
expression-stmt: expression SEMI {$<node>$ = node("expression-stmt", 2, $<node>1, $2);}
               | SEMI {$<node>$ = node("expression-stmt", 1, $1);}
               ;
selection-stmt: IF LP expression RP statement {$<node>$ = node("selection-stmt", 5, $1, $2, $<node>3, $4, $<node>5);}
              | IF LP expression RP statement ELSE statement {$<node>$ = node("selection-stmt", 7, $1, $2, $<node>3, $4, $<node>5, $6, $<node>7);}
              ;
iteration-stmt: WHILE LP expression RP statement {$<node>$ = node("iteration-stmt", 5, $1, $2, $<node>3, $4, $<node>5);}
                ;
return-stmt: RETURN SEMI {$<node>$ = node("return-stmt", 2, $1, $2);}
           | RETURN expression SEMI {$<node>$ = node("return-stmt", 3, $1, $<node>2, $3);}
           ;
expression: var ASSIGNOP expression {$<node>$ = node("expression", 3, $<node>1, $2, $<node>3);}
          | simple-expression{$<node>$ = node("expression", 1, $<node>1);}
          ;
var: ID{$<node>$ = node("var", 1, $1);}
   | ID LB expression RB {$<node>$ = node("var", 4, $1, $2, $<node>3, $4);}
   ;
simple-expression: additive-expression relop additive-expression {$<node>$ = node("simple-expression", 3, $<node>1, $<node>2, $<node>3);}
                 | additive-expression {$<node>$ = node("simple-expression", 1, $<node>1);}
                 ;
relop: LESSEQU {$<node>$ = node("relop", 1, $1);}
     | LESS {$<node>$ = node("relop", 1, $1);}
     | MORE {$<node>$ = node("relop", 1, $1);}
     | MOREEQU {$<node>$ = node("relop", 1, $1);}
     | EQUAL {$<node>$ = node("relop", 1, $1);}
     | NOTEQU {$<node>$ = node("relop", 1, $1);}
     ;
additive-expression: additive-expression addop term {$<node>$ = node("additive-expression", 3, $<node>1, $<node>2, $<node>3);}
                   | term {$<node>$ = node("additive-expression", 1, $<node>1);}
                   ;
addop: ADD {$<node>$ = node("addop", 1, $1);}
     | MINUS {$<node>$ = node("addop", 1, $1);}
     ;
term: term mulop factor {$<node>$ = node("term", 3, $<node>1, $<node>2, $<node>3);}
    | factor {$<node>$ = node("term", 1, $<node>1);}
    ;
mulop: STAR {$<node>$ = node("mulop", 1, $1);}
     | DIV {$<node>$ = node("mulop", 1, $1);}
     ;
factor: LP expression RP {$<node>$ = node("factor", 3, $1, $<node>2, $3);}
      | var {$<node>$ = node("factor", 1, $<node>1);}
      | call {$<node>$ = node("factor", 1, $<node>1);}
      | integer {$<node>$ = node("factor", 1, $<node>1);}
      | float {$<node>$ = node("factor", 1, $<node>1);}
      ;
integer: TYPE_INT {$<node>$ = node("integer", 1, $1);}
        ;
float: TYPE_FLOAT{$<node>$ = node("float", 1, $1);}
    ;
call: ID LP args RP {$<node>$ = node("call", 4, $1, $2, $<node>3, $4);}
    ;
args: arg-list {$<node>$ = node("args", 1, $<node>1);}
    | {$<node>$ = node("args", 0);}
    ;
arg-list: arg-list COMMA expression {$<node>$ = node("arg-list", 3, $<node>1, $2, $<node>3);}
        | expression {$<node>$ = node("arg-list", 1, $<node>1);}
        ;
%%

/// The error reporting function.
void yyerror(const char * s)
{
    // TO STUDENTS: This is just an example.
    // You can customize it as you like.
    fprintf(stderr, "error at line %d column %d: %s\n", lines, pos_start, s);
}

/// Parse input from file `input_path`, and prints the parsing results
/// to stdout.  If input_path is NULL, read from stdin.
///
/// This function initializes essential states before running yyparse().
syntax_tree *parse(const char *input_path)
{
    if (input_path != NULL) {
        if (!(yyin = fopen(input_path, "r"))) {
            fprintf(stderr, "[ERR] Open input file %s failed.\n", input_path);
            exit(1);
        }
    } else {
        yyin = stdin;
    }

    lines = pos_start = pos_end = 1;
    gt = new_syntax_tree();
    yyrestart(yyin);
    yyparse();
    return gt;
}

/// A helper function to quickly construct a tree node.
///
/// e.g. $$ = node("program", 1, $1);
syntax_tree_node *node(const char *name, int children_num, ...)
{
    syntax_tree_node *p = new_syntax_tree_node(name);
    syntax_tree_node *child;
    if (children_num == 0) {
        child = new_syntax_tree_node("epsilon");
        syntax_tree_add_child(p, child);
    } else {
        va_list ap;
        va_start(ap, children_num);
        for (int i = 0; i < children_num; ++i) {
            child = va_arg(ap, syntax_tree_node *);
            syntax_tree_add_child(p, child);
        }
        va_end(ap);
    }
    return p;
}
