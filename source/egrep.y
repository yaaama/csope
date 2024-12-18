%{
/*===========================================================================
 Copyright (c) 1998-2000, The Santa Cruz Operation
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 *Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 *Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 *Neither name of The Santa Cruz Operation nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT falseT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT falseT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 DAMAGE.
 =========================================================================*/

/*
 * egrep -- find lines containing a regular expression
 */
%}

%{
#include "global.h"
#include <ctype.h>
#include <stdio.h>

#include <setjmp.h>	/* jmp_buf */

#define nextch()	(*input++)

#define MAXLIN 350
#define MAXPOS 4000
#define NCHARS 256
#define NSTATES 128
#define FINAL -1
static	char gotofn[NSTATES][NCHARS];
static	int state[NSTATES];
static	char out[NSTATES];
static	unsigned int line;
static	int name[MAXLIN];
static	unsigned int left[MAXLIN];
static	unsigned int right[MAXLIN];
static	unsigned int parent[MAXLIN];
static	int foll[MAXLIN];
static	int positions[MAXPOS];
static	char chars[MAXLIN];
static	int nxtpos;
static	int nxtchar;
static	int tmpstat[MAXLIN];
static	int initstat[MAXLIN];
static	int xstate;
static	int count;
static	int icount;
static	char *input;
static	long lnum;
static	int iflag;
static	jmp_buf	env;	/* setjmp/longjmp buffer */
static	char *message;	/* error message */

/* Internal prototypes: */
static	void cfoll(int v);
static	void cgotofn(void);
static	int cstate(int v);
static	int member(int symb, int set, int torf);
static	int notin(int n);
static	void syntax_error(void);
static	void table_overflow(void);
static	void add(int *array, int n);
static	void follow(unsigned int v);
static	int unary(int x, int d);
static	int node(int x, int l, int r);
static	unsigned int cclenter(int x);
static	unsigned int enter(int x);

static int yylex(void);
static int yyerror(char *);
%}

%token CHAR DOT CCL NCCL OR CAT STAR PLUS QUEST
%left OR
%left CHAR DOT CCL NCCL '('
%left CAT
%left STAR PLUS QUEST

%%
pattern
    : t {
        unary(FINAL, $1);
		line--;
	}
	;

t
    : b r {
        $$ = node(CAT, $1, $2);
    }
	| OR b r OR {
        $$ = node(CAT, $2, $3);
    }
	| OR b r {
        $$ = node(CAT, $2, $3);
    }
	| b r OR {
        $$ = node(CAT, $1, $2);
    }
	;

b
    : {
        /* XXX:
         *  why the fuck does this state exist?
         *  what does it stand for?
         *  can we enter it? do we always enter it?
         */
        $$ = enter(DOT);
		$$ = unary(STAR, $$);
    }
	;

r
    : CHAR {
        $$ = enter($1);
    }
	| DOT {
        $$ = enter(DOT);
    }
	| CCL {
        $$ = cclenter(CCL);
    }
	| NCCL {
        $$ = cclenter(NCCL);
    }
	;

r
    : r OR r {
        $$ = node(OR, $1, $3);
    }
	| r r %prec CAT {
        $$ = node(CAT, $1, $2);
    }
	| r STAR {
        $$ = unary(STAR, $1);
    }
	| r PLUS {
        $$ = unary(PLUS, $1);
    }
	| r QUEST {
        $$ = unary(QUEST, $1);
    }
	| '(' r ')' {
        $$ = $2;
    }
	| error
	;

%%
static
int yyerror(char *s) {
	message = s;
	longjmp(env, 1);
	return 1;		/* silence a warning */
}

static
int yylex(void) {
    int cclcnt, x;
    char c, d;

    switch(c = nextch()) {
        case '*': return STAR;
        case '+': return PLUS;
        case '?': return QUEST;
        case '.': return DOT;
        case '\0': return 0;
        // ---
        case '|':
        case '\n':
            return OR;
        case '(':
        case ')':
            return c;
        // ---
        case '[':
            x = CCL;
            cclcnt = 0;
            count = nxtchar++;
            if ((c = nextch()) == '^') {
                x = NCCL;
                c = nextch();
            }
            do {
                if (c == '\0')
                syntax_error();
                if (   (c == '-')
                && (cclcnt > 0)
                && (chars[nxtchar-1] != 0)) {
                    if ((d = nextch()) != 0) {
                        c = chars[nxtchar-1];
                        while ((unsigned int)c < (unsigned int)d) {
                            if (nxtchar >= MAXLIN)
                                table_overflow();
                            chars[nxtchar++] = ++c;
                            cclcnt++;
                        }
                        continue;
                    } /* if() */
                } /* if() */
                if (nxtchar >= MAXLIN)
                table_overflow();
                chars[nxtchar++] = c;
                cclcnt++;
            } while ((c = nextch()) != ']');
            chars[count] = cclcnt;
            return (x);
        case '\\':
            if ((c = nextch()) == '\0')
                syntax_error();
            yylval = c;
            return (CHAR);
        case '$':
        case '^':
            c = '\n';
            yylval = c;
            return (CHAR);
        default:
            yylval = c;
            return (CHAR);
    }
}

static
void syntax_error(void) {
    yyerror("Syntax error");
}

static
unsigned int enter(int x) {
    if(line >= MAXLIN)
	table_overflow();
    name[line] = x;
    left[line] = 0;
    right[line] = 0;
    return(line++);
}

static
unsigned int cclenter(int x) {
    unsigned int linno = enter(x);
    right[linno] = count;
    return (linno);
}

static
int node(int x, int l, int r) {
    if(line >= MAXLIN)
	table_overflow();
    name[line] = x;
    left[line] = l;
    right[line] = r;
    parent[l] = line;
    parent[r] = line;
    return(line++);
}

static
int unary(int x, int d) {
    if(line >= MAXLIN)
	table_overflow();
    name[line] = x;
    left[line] = d;
    right[line] = 0;
    parent[d] = line;
    return(line++);
}

static
void table_overflow(void) {
    yyerror("internal table overflow");
}

static
void cfoll(int v) {
    unsigned int i;

    if (left[v] == 0) {
        count = 0;
        for (i = 1; i <= line; i++)
            tmpstat[i] = 0;
        follow(v);
        add(foll, v);
    } else if (right[v] == 0) {
        cfoll(left[v]);
    } else {
        cfoll(left[v]);
        cfoll(right[v]);
    }
}

static
void cgotofn(void) {
    unsigned int i, n, s;
    int c, k;
    char symbol[NCHARS];
    unsigned int j, l, pc, pos;
    unsigned int nc;
    int curpos;
    unsigned int num, number, newpos;

    count = 0;
    for (n=3; n<=line; n++)
	tmpstat[n] = 0;
    if (cstate(line-1)==0) {
        tmpstat[line] = 1;
        count++;
        out[0] = 1;
    }
    for (n=3; n<=line; n++)
	initstat[n] = tmpstat[n];
    count--;		/*leave out position 1 */
    icount = count;
    tmpstat[1] = 0;
    add(state, 0);
    n = 0;
    for (s = 0; s <= n; s++)  {
	if (out[s] == 1)
	    continue;
	for (i = 0; i < NCHARS; i++)
	    symbol[i] = 0;
	num = positions[state[s]];
	count = icount;
	for (i = 3; i <= line; i++)
	    tmpstat[i] = initstat[i];
	pos = state[s] + 1;
	for (i = 0; i < num; i++) {
	    curpos = positions[pos];
	    if ((c = name[curpos]) >= 0) {
		if (c < NCHARS) {
		    symbol[c] = 1;
		} else if (c == DOT) {
		    for (k = 0; k < NCHARS; k++)
			if (k != '\n')
			    symbol[k] = 1;
		} else if (c == CCL) {
		    nc = chars[right[curpos]];
		    pc = right[curpos] + 1;
		    for (j = 0; j < nc; j++)
			symbol[(unsigned char)chars[pc++]] = 1;
		} else if (c == NCCL) {
		    nc = chars[right[curpos]];
		    for (j = 0; j < NCHARS; j++) {
			pc = right[curpos] + 1;
			for (l = 0; l < nc; l++)
			    if (j==(unsigned char)chars[pc++])
				goto cont;
			if (j != '\n')
			    symbol[j] = 1;
		    cont:
			;
		    }
		}
	    }
	    pos++;
	} /* for (i) */
	for (c=0; c<NCHARS; c++) {
	    if (symbol[c] == 1) {
		/* nextstate(s,c) */
		count = icount;
		for (i=3; i <= line; i++)
		    tmpstat[i] = initstat[i];
		pos = state[s] + 1;
		for (i=0; i<num; i++) {
		    curpos = positions[pos];
		    if ((k = name[curpos]) >= 0)
			if ((k == c)
			    || (k == DOT)
			    || (k == CCL && member(c, right[curpos], 1))
			    || (k == NCCL && member(c, right[curpos], 0))
			    ) {
			    number = positions[foll[curpos]];
			    newpos = foll[curpos] + 1;
			    for (j = 0; j < number; j++) {
				if (tmpstat[positions[newpos]] != 1) {
				    tmpstat[positions[newpos]] = 1;
				    count++;
				}
				newpos++;
			    }
			}
		    pos++;
		} /* end nextstate */
		if (notin(n)) {
		    if (n >= NSTATES)
			table_overflow();
		    add(state, ++n);
		    if (tmpstat[line] == 1)
			out[n] = 1;
		    gotofn[s][c] = n;
		} else {
		    gotofn[s][c] = xstate;
		}
	    } /* if (symbol) */
	} /* for(c) */
    } /* for(s) */
}

static int
cstate(int v)
{
	int b;
	if (left[v] == 0) {
		if (tmpstat[v] != 1) {
			tmpstat[v] = 1;
			count++;
		}
		return 1;
	}
	else if (right[v] == 0) {
		if (cstate(left[v]) == 0) { return 0; }
		else if (name[v] == PLUS) { return 1; }
		else { return 0; }
	}
	else if (name[v] == CAT) {
		if (cstate(left[v]) == 0 && cstate(right[v]) == 0) { return 0; }
		else { return 1; }
	}
	else { /* name[v] == OR */
		b = cstate(right[v]);
		if (cstate(left[v]) == 0 || b == 0) { return 0; }
		else { return 1; }
	}
}

static
int member(int symb, int set, int torf) {
    unsigned num = chars[set];
    unsigned pos = set + 1;

    for (unsigned i = 0; i < num; i++) {
        if (symb == (unsigned char)(chars[pos++])) {
            return torf;
        }
    }

    return !torf;
}

static
int notin(int n) {
	int pos;
	for (int i = 0; i <= n; i++) {
		if (positions[state[i]] == count) {
			pos = state[i] + 1;
			for (int j = 0; j < count; j++)
				if (tmpstat[positions[pos++]] != 1) goto nxt;
			xstate = i;
			return (0);
		}
		nxt: ;
	}
	return (1);
}

static
void add(int *array, int n) {
    if (nxtpos + count > MAXPOS)
	table_overflow();
    array[n] = nxtpos;
    positions[nxtpos++] = count;
    for (unsigned i = 3; i <= line; i++) {
        if (tmpstat[i] == 1) {
            positions[nxtpos++] = i;
        }
    }
}

static
void follow(unsigned int v) {
    if (v == line) { return; }

    unsigned int p = parent[v];
    switch(name[p]) {
        case STAR:
        case PLUS:
            cstate(v);
            follow(p);
            break;
        case OR:
        case QUEST:
            follow(p);
            break;
        case CAT:
            if (v == left[p]
            && cstate(right[p]) != 0) {
                return;
            }
            follow(p);
            return;
        case FINAL:
            if (tmpstat[line] != 1) {
                tmpstat[line] = 1;
                count++;
            }
            break;
    }
}

char * egrepinit(const char *egreppat) {
    /* initialize the global data */
    memset(gotofn, 0, sizeof(gotofn));
    memset(state, 0, sizeof(state));
    memset(out, 0, sizeof(out));
    line = 1;
    memset(name, 0, sizeof(name));
    memset(left, 0, sizeof(left));
    memset(right, 0, sizeof(right));
    memset(parent, 0, sizeof(parent));
    memset(foll, 0, sizeof(foll));
    memset(positions, 0, sizeof(positions));
    memset(chars, 0, sizeof(chars));
    nxtpos = 0;
    nxtchar = 0;
    memset(tmpstat, 0, sizeof(tmpstat));
    memset(initstat, 0, sizeof(initstat));
    xstate = 0;
    count = 0;
    icount = 0;
    input = egreppat;
    message = NULL;
    if (setjmp(env) == 0) {
        yyparse();
        cfoll(line-1);
        cgotofn();
    }
    return(message);
}

static char buf[2 * BUFSIZ];
static const char *buf_end = buf + (sizeof(buf) / sizeof(*buf));

static
size_t read_next_chunk(char **p, FILE *fptr)
{
    if (*p <= (buf + BUFSIZ)) {
        /* bwlow the middle, so enough space left for one entire BUFSIZ */
	return fread(*p, sizeof(**p), BUFSIZ, fptr);
    } else if (*p == buf_end) {
        /* exactly at end ... wrap around and use lower half */
	*p = buf;
	return fread(*p, sizeof(**p), BUFSIZ, fptr);
    }
    /* somewhere in second half, so do a limited read */
    return fread(*p, sizeof(**p), buf_end - *p, fptr);
}

int egrep(const char * file, FILE *output, char *format) {
    char *p;
    unsigned int cstat;
    int ccount;
    char *nlp;
    unsigned int istat;
    int in_line;
    FILE *fptr;

    if ((fptr = myfopen(file, "r")) == NULL)
	return(-1);

    lnum = 1;
    p = buf;
    nlp = p;
    ccount = read_next_chunk(&p, fptr);

    if (ccount <= 0) {
	fclose(fptr);
	return(0);
    }
    in_line = 1;
    istat = cstat = (unsigned int) gotofn[0]['\n'];
    if (out[cstat])
	goto found;
    for (;;) {
	if (!iflag) {
	    /* all input chars made positive */
	    cstat = (unsigned int) gotofn[cstat][(unsigned char)*p];
	} else {
	    /* for -i option*/
	    cstat = (unsigned int) gotofn[cstat][tolower((unsigned char)*p)];
        }
	if (out[cstat]) {
	found:
	    for(;;) {
		if (*p++ == '\n') {
		    in_line = 0;
		succeed:
		    fprintf(output, format, file, lnum);
		    if (p <= nlp) {
			while (nlp < buf_end)
			    putc(*nlp++, output);
			nlp = buf;
		    }
		    while (nlp < p)
			putc(*nlp++, output);
		    lnum++;
		    nlp = p;
		    if (out[cstat = istat] == 0)
			goto brk2;
		} /* if (p++ == \n) */
	    cfound:
		if (--ccount <= 0) {
		    ccount = read_next_chunk(&p, fptr);
		    if (ccount <= 0) {
			if (in_line) {
			    in_line = 0;
			    goto succeed;
			}
                        fclose(fptr);
                        return(0);
		    }
		} /* if(ccount <= 0) */
		in_line = 1;
	    } /* for(ever) */
	} /* if(out[cstat]) */

	if (*p++ == '\n') {
	    in_line = 0;
	    lnum++;
	    nlp = p;
	    if (out[(cstat=istat)])
		goto cfound;
	}
    brk2:
	if (--ccount <= 0) {
	    ccount = read_next_chunk(&p, fptr);
	    if (ccount <= 0)
		break;
	}
	in_line = 1;
    }
    fclose(fptr);
    return(0);
}

void egrepcaseless(int i) {
	iflag = i;	/* simulate "egrep -i" */
}
