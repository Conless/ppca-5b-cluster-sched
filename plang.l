%{
/*	This file is part of the software similarity tester SIM.
  Written by Dick Grune, Vrije Universiteit, Amsterdam.
  $Id: lisplang.l,v 2.23 2017-12-11 14:12:35 dick Exp $
*/

/*
  LISP language front end for the similarity tester.
  Author:	Gertjan Akkerman <akkerm@cs.vu.nl>
  Date:	Thu, 9 Apr 87 11:15:23 MDT
*/

#include	"token.h"
#include	"properties.h"
#include	"lex.h"
#include	"lang.h"

/* General language front end data */
Token lex_token;
size_t lex_nl_cnt;
size_t lex_tk_cnt;
size_t lex_non_ASCII_cnt;

/* Language-dependent data */
#include	"idf.h"

static const struct idf reserved[] = {
  {"block",     NORM('b')},
  {"array.create", NORM('c')},
  {"for",       NORM('f')},
  {"array.get", NORM('g')},
  {"if",        NORM('i')},
  {"print",     NORM('p')},
  {"return",    NORM('r')},
  {"scan",      NORM('s')},
  {"function",  NORM('F')},
  {"set",       NORM('S')},
  {"array.set", NORM('T')},
};

/* Token sets for properties module */
const Token Non_Finals[] = {
  NORM('('),
  No_Token
};
const Token Non_Initials[] = {
  NORM(')'),
  No_Token
};
const Token Openers[] = {
  NORM('('),
  No_Token
};
const Token Closers[] = {
  NORM(')'),
  No_Token
};

/* Language-dependent code */

const char *Subject = "PL programs";

void
Init_Language(void) {
  Init_Language_Properties(Non_Finals, Non_Initials, Openers, Closers);
}

%}

%option	noyywrap

%Start	Comment

Layout		([ \t\r\f])
ASCII95		([\040-\176])

IdfChar		([-!"#$%&'*+,./0-9:;<=>?@A-Z\[\]\\^_`a-z{}~])

Idf		(({IdfChar})+)

%%

";".*$	{				/* comment */
  }

{Idf}	{				/* identifier */
    return_tk(idf_in_list(yytext, reserved, sizeof reserved, IDF));
  }

\n	{				/* count newlines */
    return_eol();
  }

{Layout}	{			/* ignore layout */
  }

{ASCII95}	{			/* copy other text */
    return_ch(yytext[0]);
  }

.	{				/* count non-ASCII chars */
    lex_non_ASCII_cnt++;
  }

%%

/* More language-dependent code */

void
yystart(void) {
  BEGIN INITIAL;
}
