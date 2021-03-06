/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include "config.h"

#include <stdio.h>
#include <ctype.h>

#define	bool	char


#if defined _WIN32
#    define u_int unsigned int
#endif /* defined _WIN32 */


#define	YES		1
#define	NO		0
#define	EOS		'\0'

#define	ENDLINE		50		/* max length of pattern */
#define	MAXTOKEN	250		/* max size of single token */

#define	SETLINE		{++lineno;lineftell = ftell(inf);}
#define	GETC(op,exp)	((c = getc(inf)) op (int)exp)

#define	iswhite(arg)	(_wht[arg])	/* T if char is white */
#define	begtoken(arg)	(_btk[arg])	/* T if char can start token */
#define	intoken(arg)	(_itk[arg])	/* T if char can be in token */
#define	endtoken(arg)	(_etk[arg])	/* T if char ends tokens */
#define	isgood(arg)	(_gd[arg])	/* T if char can be after ')' */

typedef struct nd_st {			/* sorting structure */
	struct nd_st	*left,
			*right;		/* left and right sons */
	char	*entry,			/* function or type name */
		*file,			/* file name */
		*pat;			/* search pattern */
	int	lno;			/* for -x option */
	bool	been_warned;		/* set if noticed dup */
} NODE;

extern FILE	*inf;			/* ioptr for current input file */
extern char 	*curfile;   	    	/* name of current file */
extern long	lineftell;		/* ftell after getc( inf ) == '\n' */
extern int	lineno,			/* line number of current line */
		xflag;			/* -x: cxref style output */
extern bool	_wht[0177],_etk[0177],_itk[0177],_btk[0177],_gd[0177];
extern char	lbuf[BUFSIZ];
extern int  	wflag;	    	    	/* -w: suppress warnings */
extern int  	cflag;	    	    	/* -c: search in comments */

extern void put_entries(register NODE *node);
extern void getline(void);

/* Declare these (ugly!) global functions to clean up warnings! */
extern void pfnote(char *name, int ln);
extern void asm_entries();
extern int skip_key(register int key);
extern void l_entries(void);
extern void toss_yysec(void);
extern void y_entries(void);
extern int PF_funcs(void);
extern void c_entries(int goc);
extern void skip_comment(void);
