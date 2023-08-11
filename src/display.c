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

/*    cscope - interactive C symbol cross-reference
 *
 *    display functions
 */

#include "global.h"
#include "build.h"
#include "colors.h"

#ifdef CCS
#include "sgs.h"    /* ESG_PKG and ESG_REL */
#else
#include "version.h"    /* FILEVERSION and FIXVERSION */
#endif

#include <ncurses.h>
#include <stdarg.h>    /* va_list stuff */
#include <time.h>
#include <errno.h>
#include <stdarg.h>

/* XXX */
#define    MSGLINE    0			/* message line */
#define    MSGCOL    0			/* message column */

int subsystemlen    = sizeof("Subsystem")-1;    /* OGS subsystem name display field length */
int booklen            = sizeof("Book")-1;         /* OGS book name display field length */
int filelen            = sizeof("File")-1;         /* file name display field length */
int fcnlen            = sizeof("Function")-1;     /* function name display field length */
int numlen            = 0;                        /* line number display field length */

int    *displine;        /* screen line of displayed reference */
unsigned int disprefs;        /* displayed references */
int    field;            /* input field */
unsigned int mdisprefs;        /* maximum displayed references */
unsigned int nextline;        /* next line to be shown */
static int    bottomline;        /* bottom line of page */
long    searchcount;        /* count of files searched */
unsigned int totallines;    /* total reference lines */
unsigned int curdispline = 0;
int current_page = 0;
int input_mode = INPUT_NORMAL;
const char* prompts[] = {
	[INPUT_NORMAL] = "$ ",
	[INPUT_APPEND] = "Append to file: ",
	[INPUT_PIPE] = "Pipe to shell command: ",
	[INPUT_READ] = "Read from file: ",
	[INPUT_CHANGE_TO] = "To: ",
	[INPUT_CHANGE] = "To: "
};

unsigned int topline = 1;        /* top line of page */

/* Selectable windows */
WINDOW* winput;
WINDOW* wmode;
WINDOW* wresult;
WINDOW* whelp;
/* Selected window pointer */
WINDOW** current_window;
static WINDOW** last_window;

static int result_window_height;
static int second_col_width;
static int first_col_width;
static int input_window_height;
static int mode_window_height;

#define WRESULT_TABLE_BODY_START 4

int window_change;

/* NOTE: It's declared like this because we dont need a terminating '\00'. */
static const char dispchars[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};
int dispchar2int(const char c){
	const char fst = dispchars[0];
	const char lst = dispchars[sizeof(dispchars)-1];
	int r = c - fst;
	if(r < 0 || lst < r){ return -1; }
	return r;
}

char    lastmsg[MSGLEN + 1];    /* last message displayed */
static    const char    helpstring[] = "Press the ? key for help";
static    const char    selprompt[] =
    "Select lines to change (press the ? key for help): ";

struct    {        /* text of input fields */
    char    *text1;
    char    *text2;
} /* Paralel array to "field_searchers", indexed by "field" */
fields[FIELDS + 1] = {    /* samuel has a search that is not part of the cscope display */
    {"Find this", "C symbol"},
    {"Find this", "global definition"},
    {"Find", "functions called by this function"},
    {"Find", "functions calling this function"},
    {"Find this", "text string"},
    {"Change this", "text string"},
    {"Find this", "egrep pattern"},
    {"Find this", "file"},
    {"Find", "files #including this file"},
    {"Find", "assignments to this symbol"},
    {"Find all", "function definitions"},    /* samuel only */
};

/* initialize display parameters */
void
dispinit(void)
{
    /* initialize the curses display package */
    initscr();    /* initialize the screen */
	start_color();
	use_default_colors();
	easy_init_pair(FRAME);
	easy_init_pair(PROMPT);
	easy_init_pair(FIELD);
	easy_init_pair(FIELD_SELECTED);
	easy_init_pair(HELP);
	easy_init_pair(TOOLTIP);
	easy_init_pair(MESSAGE);
	easy_init_pair(PATTERN);
	easy_init_pair(TABLE_HEADER);
	easy_init_pair(TABLE_ID);
	easy_init_pair(TABLE_COL_LINE);
	easy_init_pair(TABLE_COL_FILE);
	easy_init_pair(TABLE_COL_FUNCTION);
	easy_init_pair(TABLE_COL_TEXT);
	easy_init_pair(PAGER_MSG);
    entercurses();

    /* Calculate section sizes */
    result_window_height = LINES - 2;
    input_window_height = 1;
    mode_window_height = LINES - input_window_height - 2 - 1;
    first_col_width = 48; // (((COLS - 2)%2 == 0) ? ((COLS-2)/2) : (((COLS-2)/2)+1));
    second_col_width = COLS - 2 - 1 - first_col_width; //((COLS - 2) / 2) - 1;
    mdisprefs = result_window_height - WRESULT_TABLE_BODY_START - 1 - 1 - 1;

    if (mdisprefs <= 0) {
        postfatal("%s: screen too small\n", argv0);
        /* NOTREACHED */
    }
    if(mdisprefs > sizeof(dispchars)){
        mdisprefs = sizeof(dispchars);
    }

    /* allocate the displayed line array */
    displine = malloc(mdisprefs * sizeof(*displine));

    /* readline */
    rlinit();

    /* initialize windows */
    winput = newwin(input_window_height, first_col_width, 1, 1);
    wmode = newwin(mode_window_height, first_col_width, input_window_height+1 + 1, 1);
    wresult = newwin(result_window_height, second_col_width, 1, first_col_width + 1 + 1);
    whelp = newwin(LINES-2, COLS-2, 1, 1);
    refresh();

    current_window = &winput;
}

/* enter curses mode */
void
entercurses(void)
{
    incurses = true;
	window_change = CH_ALL;

    nonl();             /* don't translate an output \n to \n\r */
    cbreak();  	     	/* single character input */
    noecho();        	/* don't echo input characters */
    curs_set(0);
    clear();        	/* clear the screen */
    mouseinit();        /* initialize any mouse interface */
    //drawscrollbar(topline, nextline);
    keypad(stdscr, TRUE);    /* enable the keypad */
    //fixkeypad();    /* fix for getch() intermittently returning garbage */
    standend();    /* turn off reverse video */
}

/* exit curses mode */
void
exitcurses(void)
{
    /* clear the bottom line */
    move(LINES - 1, 0);
    clrtoeol();
    refresh();

    /* exit curses and restore the terminal modes */
    endwin();
    incurses = false;

    /* restore the mouse */
    mousecleanup();
    fflush(stdout);
}

static inline void display_help(){
	// XXX: this could be optimized by only overriding the buffer if theres an actual change
	werase(whelp);
	wmove(whelp, 0, 0);
	wattron(whelp, COLOR_PAIR(COLOR_PAIR_HELP));
	waddstr(whelp, help());
	wattroff(whelp, COLOR_PAIR(COLOR_PAIR_HELP));

	refresh();
	wrefresh(whelp);

	do_press_any_key = true;
}

static inline void display_frame(const bool border_only){
	wattron(stdscr, COLOR_PAIR(COLOR_PAIR_FRAME));

    box(stdscr, 0, 0);
    /* Title*/
    const int LEFT_PADDING = 5;
    wmove(stdscr, 0, LEFT_PADDING);
#if CCS
    wprintw(stdscr, "cscope %s", ESG_REL);
#else
    wprintw(stdscr, "Cscope version %d%s", FILEVERSION, FIXVERSION);
#endif
    wmove(stdscr, 0, COLS - (int)sizeof(helpstring) - 3);
    waddstr(stdscr, helpstring);
	/* --- */
	if(!border_only){
		/* Vertical line */
		mvaddch(0, first_col_width + 1, ACS_TTEE);
		for(int i = 0; i < LINES-2; i++){
			mvaddch(i+1, first_col_width + 1, ACS_VLINE);
		}
		mvaddch(LINES-1, first_col_width + 1, ACS_BTEE);
		/* Horizontal line */
		wmove(stdscr, input_window_height + 1, 0);
		addch(ACS_LTEE);
		for(int i = 0; i < first_col_width; i++){
			addch(ACS_HLINE);
		}
		addch(ACS_RTEE);
	}

	wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_FRAME));
}

static inline void display_mode(){
	werase(wmode);

    for(int i = 0; i < FIELDS; ++i){
		if(i == field){
			wattron(wmode, COLOR_PAIR(COLOR_PAIR_FIELD_SELECTED) | ATTRIBUTE_FIELD_SELECTED);
			mvwprintw(wmode, i, 0, "%s %s", fields[i].text1, fields[i].text2);
			wattroff(wmode, COLOR_PAIR(COLOR_PAIR_FIELD_SELECTED) | ATTRIBUTE_FIELD_SELECTED);
		}else{
			wattron(wmode, COLOR_PAIR(COLOR_PAIR_FIELD));
			mvwprintw(wmode, i, 0, "%s %s", fields[i].text1, fields[i].text2);
			wattroff(wmode, COLOR_PAIR(COLOR_PAIR_FIELD));
		}
    }
}

static inline void display_command_field(){
    werase(winput);
	wattron(winput, COLOR_PAIR(COLOR_PAIR_PROMPT));
    mvwaddstr(winput, 0, 0, prompts[input_mode]);
	wattroff(winput, COLOR_PAIR(COLOR_PAIR_PROMPT));
    waddstr(winput, rl_line_buffer);
}
static inline void display_results(){
    int     i;
    char    *s;
    int     screenline;             /* screen line number */
    int     srctxtw;                /* source line display width */
	/* column headings */
    char    *subsystem;             /* OGS subsystem name */
    char    *book;                  /* OGS book name */
    char    file[PATHLEN + 1];      /* file name */
    char    function[PATLEN + 1];   /* function name */
    char    linenum[NUMLEN + 1];    /* line number */

	werase(wresult);

	/* --- Display the message --- */
    if (totallines == 0) { // Its a real message
    	wmove(wresult, MSGLINE, 0);
    	wclrtoeol(wresult);
		wattron(wresult, COLOR_PAIR(COLOR_PAIR_MESSAGE));
        waddstr(wresult, lastmsg);
		wattroff(wresult, COLOR_PAIR(COLOR_PAIR_MESSAGE));
        return;
    }
    if (input_mode == INPUT_CHANGE) { // Its a pattern
		snprintf(lastmsg, MSGLEN, "Change \"%s\" to \"%s\"", input_line, newpat);
    } else {
		snprintf(lastmsg, MSGLEN, "%c%s: %s", toupper((unsigned char)fields[field].text2[0]),
           fields[field].text2 + 1, input_line);
    }
	wattron(wresult, COLOR_PAIR(COLOR_PAIR_PATTERN));
    waddstr(wresult, lastmsg);
	wattroff(wresult, COLOR_PAIR(COLOR_PAIR_PATTERN));

    /* --- Display the column headings --- */
	wattron(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_HEADER));
    wmove(wresult, 2, 2);
    if (ogs == true && field != FILENAME) {
        wprintw(wresult, "%-*s ", subsystemlen, "Subsystem");
        wprintw(wresult, "%-*s ", booklen, "Book");
    }
    if (dispcomponents > 0)
        wprintw(wresult, "%-*s ", filelen, "File");

    if (field == SYMBOL || field == CALLEDBY || field == CALLING) {
        wprintw(wresult, "%-*s ", fcnlen, "Function");
    }
    if (field != FILENAME) {
        waddstr(wresult, "Line");
    }
	wattroff(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_HEADER));

	/* --- Display table entries --- */
    wmove(wresult, WRESULT_TABLE_BODY_START, 0);

    /* calculate the source text column */
    /* NOTE: the +1s are column gaps */
    srctxtw = second_col_width;
    srctxtw -= 1+1; // dispchars
    if (ogs == true) {
        srctxtw -= subsystemlen+1 + booklen+1;
    }
    if (dispcomponents > 0) {
        srctxtw -= filelen+1;
    }
    if (field == SYMBOL || field == CALLEDBY || field == CALLING) {
        srctxtw -= fcnlen+1;
    }
    srctxtw -= numlen+1;

	/* decide where to list from */
	/* XXX: this error handling migth be redundant*/
	{
		int seekerr;
		do{
			seekerr = seekpage(current_page);
		}while(seekerr == -1 && current_page--);
	}

    /* until the max references have been displayed or
       there is no more room */
    for (disprefs = 0, screenline = WRESULT_TABLE_BODY_START;
         disprefs < mdisprefs && screenline <= result_window_height;
         ++disprefs, ++screenline)
    {
        /* read the reference line */
        if (
                fscanf(refsfound, "%" PATHLEN_STR "s%" PATHLEN_STR "s%" NUMLEN_STR "s %" TEMPSTRING_LEN_STR "[^\n]",
                        file,
                        function,
                        linenum,
                        tempstring
                    )
                <
                    4
            ){ break; }
        ++nextline;
        displine[disprefs] = screenline;

		wattron(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_ID));
        wprintw(wresult, "%c", dispchars[disprefs]);
		wattroff(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_ID));

        /* display any change mark */
        if (input_mode == INPUT_CHANGE && change[topref + disprefs]) {
        	waddch(wresult, '>');
        } else {
            waddch(wresult, ' ');
        }

        /* display the file name */
		wattron(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_FILE));
        if (field == FILENAME) {
			wprintw(wresult, "%-*s ", filelen, file);
        } else {
			/* if OGS, display the subsystem and book names */
			if (ogs == true) {
				ogsnames(file, &subsystem, &book);
				wprintw(wresult, "%-*.*s ", subsystemlen, subsystemlen, subsystem);
				wprintw(wresult, "%-*.*s ", booklen, booklen, book);
			}
			/* display the requested path components */
			if (dispcomponents > 0) {
				wprintw(wresult, "%-*.*s ", filelen, filelen,
				   pathcomponents(file, dispcomponents));
			}
        } /* else(field == FILENAME) */
		wattroff(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_FILE));

        /* display the function name */
        if(field == SYMBOL || field == CALLEDBY || field == CALLING){
			wattron(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_FUNCTION));
        	wprintw(wresult, "%-*.*s ", fcnlen, fcnlen, function);
			wattroff(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_FUNCTION));
        }
        if(field == FILENAME){
			waddch(wresult, '\n');    /* go to next line */
			continue;
        }

        /* display the line number */
		wattron(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_LINE));
        wprintw(wresult, "%*s ", numlen, linenum);
		wattroff(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_LINE));
        /* there may be tabs in egrep output */
        while((s = strchr(tempstring, '\t')) != NULL){
			*s = ' ';
        }

        /* display the source line */
		wattron(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_TEXT));
        s = tempstring;
        for (;;) {
        /* if the source line does not fit */
        if ((i = strlen(s)) > srctxtw) {

            /* find the nearest blank */
            for (i = srctxtw; s[i] != ' ' && i > 0; --i) {
                ;
            }

            if (i == 0) {
                i = srctxtw;    /* no blank */
            }
        }
        /* print up to this point */
        wprintw(wresult, "%.*s", i, s);
        s += i;

        /* if line didn't wrap around */
        if (i < srctxtw) {
            waddch(wresult, '\n');    /* go to next line */
        }
        /* skip blanks */
        while (*s == ' ') {
            ++s;
        }
        /* see if there is more text */
        if (*s == '\0') {
            break;
        }
        /* if the source line is too long */
        if (++screenline > result_window_height) {

            /* if this is the first displayed line,
               display what will fit on the screen */
            if (topref == nextline-1) {
				disprefs++;
				/* break out of two loops */
				goto endrefs;
            }

            /* erase the reference */
            while (--screenline >= displine[disprefs]) {
                wmove(wresult, screenline, 0);
                wclrtoeol(wresult);
            }
            ++screenline;

            /* go back to the beginning of this reference */
            --nextline;
			fseek(refsfound, 0, SEEK_SET);
            goto endrefs;
        }
        /* indent the continued source line */
        wmove(wresult, screenline, second_col_width - srctxtw);
        } /* for(ever) */
    } /* for(reference output lines) */
	wattron(wresult, COLOR_PAIR(COLOR_PAIR_TABLE_COL_TEXT));

endrefs:
    /* position the screen cursor for the message */
    i = result_window_height - 1;
    if (screenline < i) {
        waddch(wresult, '\n');
    }
    else {
        wmove(wresult, i, 0);
    }
	/* --- display pager message --- */
	wattron(wresult, COLOR_PAIR(COLOR_PAIR_PAGER_MSG));
    /* check for more references */
    i = totallines - nextline + 1;
    bottomline = nextline;
    if (i > 0) {
        wprintw(wresult, "* Lines %d-%d of %d, %d more - press the space bar to display more *", topref, bottomline, totallines, i);
    }
    /* if this is the last page of references */
    else if (current_page > 0 && nextline > totallines) {
        waddstr(wresult, "* Press the space bar to display the first lines again *");
    }
	wattroff(wresult, COLOR_PAIR(COLOR_PAIR_PAGER_MSG));
}

void display_cursor(void){
    chtype i;
    int yoffset = 0, xoffset = 0;

    if(current_window == &winput){
        xoffset = strlen(prompts[input_mode]) + rl_point;
    }else if(current_window == &wmode){
        yoffset = field;
    }else if(current_window == &wresult){
        yoffset = displine[curdispline];
    }else{
        assert("No window selected.");
    }

    wmove(*current_window, yoffset, xoffset);

    i = winch(*current_window);
    i |= A_REVERSE;
    waddch(*current_window, i);
}
void
horswp_field(void){
    if(current_window != &wresult){
        if(current_window == &winput){
            window_change |= CH_INPUT;
        }else{
            window_change |= CH_MODE;
        }
        last_window = current_window;
        current_window = &wresult;
    }else{
        current_window = last_window;
    }
    window_change |= CH_RESULT;
}

void
verswp_field(void){
    if(current_window == &wresult){ return; }
    current_window = (current_window == &winput) ? &wmode : &winput;
    window_change |= CH_INPUT | CH_MODE;
}

/* display search progress with default custom format */
void
progress(char *what, long current, long max)
{
    static    long    start;
    long    now;
    int    i;

    /* save the start time */
    if (searchcount == 0) {
        start = time(NULL);
    }
    if ((now = time(NULL)) - start >= 1)
    {
        if (linemode == false)
        {
            wmove(wresult, MSGLINE, MSGCOL);
            wclrtoeol(wresult);
            waddstr(wresult, what);
            snprintf(lastmsg, sizeof(lastmsg), "%ld", current);
            wmove(wresult, MSGLINE, (COLS / 2) - (strlen(lastmsg) / 2));
            waddstr(wresult, lastmsg);
            snprintf(lastmsg, sizeof(lastmsg), "%ld", max);
            wmove(wresult, MSGLINE, COLS - strlen(lastmsg));
            waddstr(wresult, lastmsg);
            refresh();
        }
        else if (verbosemode == true)
        {
            snprintf(lastmsg, sizeof(lastmsg), "> %s %ld of %ld", what, current, max);
        }

        start = now;
        if ((linemode == false) && (incurses == true))
        {
            wmove(wresult, MSGLINE, MSGCOL);
            i = (float)COLS * (float)current / (float)max;

            standout();
            for (; i > 0; i--)
                waddch(wresult, inch());
            standend();
            refresh();
        }
        else
        	if(linemode == false || verbosemode == true){
        		postmsg(lastmsg);
			}
    }
    ++searchcount;
}

/* print error message on system call failure */
void
myperror(char *text)
{
    char    *s;

    s = strerror(errno);

    (void) snprintf(lastmsg, sizeof(lastmsg), "%s: %s", text, s);
    postmsg(lastmsg);
}

/* postmsg clears the message line and prints the message */

void
postmsg(char *msg)
{
    if (linemode == true || incurses == false) {
        printf("%s\n", msg);
        fflush(stdout);
    }
    else {
		window_change |= CH_RESULT;
    }
    UNUSED(strncpy(lastmsg, msg, sizeof(lastmsg) - 1));
}

/* clearmsg2 clears the second message line */
void
clearmsg2(void)
{
    if (linemode == false) {
        wmove(wresult, MSGLINE + 1, 0);
        wclrtoeol(wresult);
    }
}

/* postmsg2 clears the second message line and prints the message */
void
postmsg2(char *msg)
{
    if (linemode == true) {
        (void) printf("%s\n", msg);
    }
    else {
        clearmsg2();
        waddstr(wresult, msg);
        wrefresh(wresult);
    }
}

/* display an error mesg - stdout or on second msg line */
void
posterr(char *msg, ...)
{
    va_list ap;
    char errbuf[MSGLEN];

    va_start(ap, msg);
    if (linemode == true || incurses == false)
    {
        (void) vfprintf(stderr, msg, ap);
    (void) fputc('\n', stderr);
    } else {
        vsnprintf(errbuf, sizeof(errbuf), msg, ap);
        postmsg2(errbuf);
    }
    va_end(ap);
}

/* display a fatal error mesg -- stderr *after* shutting down curses */
void
postfatal(const char *msg, ...)
{
    va_list ap;
    char errbuf[MSGLEN];

    va_start(ap, msg);
    vsnprintf(errbuf, sizeof(errbuf), msg, ap);
    /* restore the terminal to its original mode */
    if (incurses == true) {
        exitcurses();
    }

    /* display fatal error messages */
    fprintf(stderr,"%s",errbuf);

    /* shut down */
    myexit(1);
}

/* get the OGS subsystem and book names */
void
ogsnames(char *file, char **subsystem, char **book)
{
    static    char    buf[PATHLEN + 1];
    char    *s, *slash;

    *subsystem = *book = "";
    (void) strcpy(buf,file);
    s = buf;
    if (*s == '/') {
        ++s;
    }
    while ((slash = strchr(s, '/')) != NULL) {
        *slash = '\0';
        if ((int)strlen(s) >= 3 && strncmp(slash - 3, ".ss", 3) == 0) {
            *subsystem = s;
            s = slash + 1;
            if ((slash = strchr(s, '/')) != NULL) {
                *book = s;
                *slash = '\0';
            }
            break;
        }
        s = slash + 1;
    }
}

void
display(void)
{
    //drawscrollbar(topline, nextline);    /* display the scrollbar */

    if(window_change){
		if(window_change == CH_HELP){
			display_frame(true);
			display_help();
			/* Do not display over the help msg and
			 *  rely on setting CH_ALL for the next display
			 */
			window_change = CH_ALL;
			return;
		}
		/**/
        if(window_change == CH_ALL){
            display_frame(false);
        }
        if(window_change & CH_INPUT){
            display_command_field();
        }
        if(window_change & CH_RESULT){
            display_results();
        }
        if(window_change & CH_MODE){
            display_mode();
        }

        display_cursor();

        refresh();
        wrefresh(winput);
        wrefresh(wmode);
        wrefresh(wresult);
    }

    window_change = CH_NONE;
}
