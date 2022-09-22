%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "syntax_analyzer.h"

int lines;
int pos_start;
int pos_end;

void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}

/*****************声明和选项设置  end*****************/

%}


%%
 /* to do for students */
 /* two cases for you, pass_node will send flex's token to bison */
\+ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ADD;}


 /****请在此补全所有flex的模式与动作  end******/

"if" {pos_start = pos_end; pos_end += 2; pass_node(yytext); return IF;}
"else" {pos_start = pos_end; pos_end += 4; pass_node(yytext); return ELSE;} 
"int" {pos_start = pos_end; pos_end += 3; pass_node(yytext); return TYPE_INT;}
"float" {pos_start = pos_end; pos_end += 5; pass_node(yytext); return TYPE_FLOAT;}
"void" {pos_start = pos_end; pos_end += 4; pass_node(yytext); return TYPE_VOID;}
"return" {pos_start = pos_end; pos_end += 6; pass_node(yytext); return RETURN;}
"while" {pos_start = pos_end; pos_end += 5; pass_node(yytext); return WHILE;}

[0-9]+\.|[0-9]*\.[0-9]+ {pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return FLOATPOINT;}
[0-9]+ {pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return INTEGER;} 
[a-zA-Z]+ {pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return ID;}

\- {pos_start = pos_end; pos_end += 1; pass_node(yytext); return MINUS;}
\* {pos_start = pos_end; pos_end += 1; pass_node(yytext); return STAR;}
\/ {pos_start = pos_end; pos_end += 1; pass_node(yytext); return DIV;}
\< {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LESS;}
"<=" {pos_start = pos_end; pos_end += 2; pass_node(yytext); return LESSEQU;}
\> {pos_start = pos_end; pos_end += 1; pass_node(yytext); return MORE;}
">=" {pos_start = pos_end; pos_end += 2; pass_node(yytext); return MOREEQU;}
"==" {pos_start = pos_end; pos_end += 2; pass_node(yytext); return EQUAL;}
"!=" {pos_start = pos_end; pos_end += 2; pass_node(yytext); return NOTEQU;}
\= {pos_start = pos_end; pos_end += 1; pass_node(yytext); return ASSIGNOP;}
\; {pos_start = pos_end; pos_end += 1; pass_node(yytext); return SEMI;}
\, {pos_start = pos_end; pos_end += 1; pass_node(yytext); return COMMA;}
\( {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LP;}
\) {pos_start = pos_end; pos_end += 1; pass_node(yytext); return RP;}
\[ {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LB;}
\] {pos_start = pos_end; pos_end += 1; pass_node(yytext); return RB;}
\{ {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LC;}
\} {pos_start = pos_end; pos_end += 1; pass_node(yytext); return RC;}

" "|\t {pos_end+=yyleng;}
\r\n|\r|\n {lines++;pos_end = 0;}

\/\*([^\*]|\*+[^\/])*\*+\/ {}


. { pos_start = pos_end; pos_end++; return ERROR; }
%%

