#include <stdio.h>
#include <readline/readline.h>
#include "global.h"
#include "build.h"

extern int LINES;	// XXX

static int input_available = 0;
static char input_char;
char input_line[PATLEN + 1];

BOOL do_terminate = NO;

BOOL interpret(int c){
	input_char = c;
	input_available = 1;
	rl_callback_read_char();

	return do_terminate;
}

static int getc_function(FILE* ignore){
	input_available = 0;
	return (int)input_char;
}

static int input_available_hook(){
	return input_available;
}

static void redisplay_function(){
	window_change |= CH_INPUT;
}

static void callback_handler(char* line){
	strncpy(input_line, line, PATLEN);
	search();
}

static int interpret_break(){
	do_terminate = YES;
}

static int ctrl_z(){
	kill(0, SIGTSTP);
}

static int toggle_caseless(){
	if (caseless == NO) {
	    caseless = YES;
	    postmsg2("Caseless mode is now ON");
	} else {
	    caseless = NO;
	    postmsg2("Caseless mode is now OFF");
	}
	egrepcaseless(caseless);	/* turn on/off -i flag */
}

static int rebuild_reference(){
	if (isuptodate == YES) {
	    postmsg("The -d option prevents rebuilding the symbol database");
	    return(NO);
	}
	exitcurses();
	freefilelist();		/* remake the source file list */
	makefilelist();
	rebuild();
	if (errorsfound == YES) {
	    errorsfound = NO;
	    askforreturn();
	}		
	entercurses();
	clearmsg();		/* clear any previous message */
	totallines = 0;
	disprefs = 0;	
	topline = nextline = 1;
	return(YES);
}

static int process_mouse(){
	int i;
	MOUSE* p;
	if ((p = getmouseaction(DUMMYCHAR)) == NULL) {
	    return(NO);	/* unknown control sequence */
	}
	/* if the button number is a scrollbar tag */
	if (p->button == '0') {
	    //scrollbar(p);	// XXX
	    return(NO);
	} 
	/* ignore a sweep */
	if (p->x2 >= 0) {
	    return(NO);
	}
	/* if this is a line selection */
	if (p->y1 > FLDLINE) {

	    /* find the selected line */
	    /* note: the selection is forced into range */
	    for (i = disprefs - 1; i > 0; --i) {
		if (p->y1 >= displine[i]) {
	    	return(NO);
		}
	    }
	    /* display it in the file with the editor */
	    editref(i);
	} else {	/* this is an input field selection */
	    field = p->y1 - FLDLINE;
	    /* force it into range */
	    if (field >= FIELDS) {
		field = FIELDS - 1;
	    }
	    resetcmd();
	    return(NO);
	}
}

void rlinit(){
	rl_catch_signals = 0;
	rl_catch_sigwinch = 0;
	rl_prep_term_function = NULL;
	rl_deprep_term_function = NULL;
	rl_change_environment = 0;

	rl_getc_function = getc_function;
	rl_input_available_hook = input_available_hook;
	rl_redisplay_function = redisplay_function;
	rl_callback_handler_install("", callback_handler);

	rl_bind_key(EOF, interpret_break);
	rl_bind_key(ctrl('D'), interpret_break);	//XXX: why the fuck does it not work if its the first char?
	rl_bind_key(ctrl('Z'), ctrl_z);
	rl_bind_key(ctrl('Z'), toggle_caseless);
	rl_bind_key(ctrl('R'), rebuild_reference);
	rl_bind_key(ESC, process_mouse);		/* possible unixpc mouse selection */
	rl_bind_key(ctrl('X'), process_mouse);	/* mouse selection */

	//rl_bind_key(ctrl('\\'), /**/);		/* bypass bindings */
}