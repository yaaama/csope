#include "global.h"

#include "build.h"
#include "vpath.h"
#include "version.inc"
#include "auto_vararg.h"
#include "help.h"

#include <stdlib.h>	 /* atoi */
#include <getopt.h>

/* defaults for unset environment variables */
#define DEFAULT_EDITOR	 "vi"
#define DEFAULT_HOME	 "/"   /* no $HOME --> use root directory */
#define DEFAULT_SHELL	 "sh"
#define DEFAULT_LINEFLAG "+%s" /* default: used by vi and emacs */
#define DEFAULT_TMPDIR	 "/tmp"

/* environment variable holders */
char * editor;
char * home;
char * shell;
char * lineflag;
char * tmpdir;
bool lineflagafterfile;
/* XXX: this might be a war crime;
 *      its currently required for respecting kernel mode
 */
const char * incdir = NULL;

/* option holders */
bool  remove_symfile_onexit = false;
bool  onesearch;                        /* one search only in line mode */
char *reflines;                         /* symbol reference lines file */
bool  invertedindex;                    /* the database has an inverted index */
bool  preserve_database = false;            /* consider the crossref up-to-date */
bool  kernelmode;                       /* don't use DEFAULT_INCLUDE_DIRECTORY - bad for kernels */
bool  linemode     = false;             /* use line oriented user interface */
bool  verbosemode = false;              /* print extra information on line mode */
bool  recurse_dir = false;              /* recurse dirs when searching for src files */
char *namefile;                         /* file of file names */

/* From a list of envirnment variable names,
 *  return the first valid variable value
 *  or the user given default.
 */
#define coalesce_env(def, ...) _coalesce_env(def, PP_NARG(__VA_ARGS__), __VA_ARGS__)
static inline
char * _coalesce_env(char * mydefault, size_t argc, ...) {
    char * r = mydefault;
    va_list va;
    va_start(va, argc);

    for (int i = 0; i < argc; i++) {
        char * value = va_arg(va, char*);
        value = getenv(value);

        if (value != NULL
        &&  *value != '\0') {
            r = value;
            goto end;
        }
    }

  end:
    va_end(va);
    return r;
}

/* XXX: Add CSOPE_* equivalents while preserving the originals.
 *       DO NOT do it without writting documentation
 */
void readenv(bool preserve_database) {
    editor   = coalesce_env(DEFAULT_EDITOR, "CSCOPE_EDITOR", "VIEWER", "EDITOR");
    home     = coalesce_env(DEFAULT_HOME, "HOME");
    shell    = coalesce_env(DEFAULT_SHELL, "SHELL");
    lineflag = coalesce_env(DEFAULT_LINEFLAG, "CSCOPE_LINEFLAG");
    tmpdir   = coalesce_env(DEFAULT_TMPDIR, "TMPDIR");

    lineflagafterfile = getenv("CSCOPE_LINEFLAG_AFTER_FILE") ? 1 : 0;

    if (!preserve_database) {
        incdir = coalesce_env(DEFAULT_INCLUDE_DIRECTORY, "INCDIR");
        /* get source directories from the environment */
        const char * const my_source_dirs = getenv("SOURCEDIRS");
        if(my_source_dirs) { sourcedir(my_source_dirs); }
        /* get include directories from the environment */
        const char * const my_include_dirs = getenv("INCLUDEDIRS");
        if(my_include_dirs) { includedir(my_source_dirs); }
    }
}

char * * parse_options(const int argc, const char * const * const argv) {
	int	  opt;
	int	  longind;
	char  path[PATHLEN + 1]; /* file path */
	char *s;

	struct option lopts[] = {
		{"help",    0, NULL, 'h'},
		{"version", 0, NULL, 'V'},
		{0,         0,    0,  0 },
	};

	while((opt = getopt_long(argc, (char**)argv,
			   "hVbcCdeF:f:I:i:kLl0:1:2:3:4:5:6:7:8:9:P:p:qRs:TUuvX",
			   lopts,
			   &longind)) != -1) {
		switch(opt) {
			case '?':
				usage();
				myexit(1);
				break;
			case 'h':
				longusage();
				myexit(1);
				break;
			case 'V':
				fprintf(stderr, PROGRAM_NAME ": version %d%s\n", FILEVERSION, FIXVERSION);
				myexit(0);
				break;
			case 'X':
				remove_symfile_onexit = true;
				break;
			case ASCII_DIGIT:
				/* The input fields numbers for line mode operation */
				field = opt - '0';
				if(strlen(optarg) > PATHLEN) {
					postfatal("\
        			cscope: pattern too long, cannot be > \
        			%d characters\n",
						PATLEN);
				}
				strcpy(input_line, optarg);
				break;
			case 'b': /* only build the cross-reference */
				buildonly = true;
				linemode  = true;
				break;
			case 'c': /* ASCII characters only in crossref */
				compress = false;
				break;
			case 'C':					 /* turn on caseless mode for symbol searches */
				caseless = true;
				egrepcaseless(caseless); /* simulate egrep -i flag */
				break;
			case 'd':					 /* consider crossref up-to-date */
				preserve_database = true;
				break;
			case 'e': /* suppress ^E prompt between files */
				editallprompt = false;
				break;
			case 'k': /* ignore DEFAULT_INCLUDE_DIRECTORY */
				kernelmode = true;
				break;
			case 'L':
				onesearch = true;
				/* FALLTHROUGH */
			case 'l':
				linemode = true;
				break;
			case 'v':
				verbosemode = true;
				break;
			case 'q': /* quick search */
				invertedindex = true;
				break;
			case 'T': /* truncate symbols to 8 characters */
				trun_syms = true;
				break;
			case 'u': /* unconditionally build the cross-reference */
				unconditional = true;
				break;
			case 'U': /* assume some files have changed */
				fileschanged = true;
				break;
			case 'R':
				recurse_dir = true;
				break;
			case 'f': /* alternate cross-reference file */
				reffile = optarg;
				if(strlen(reffile) > sizeof(path) - 3) {
					postfatal("\
        			cscope: reffile too long, cannot \
        			be > %d characters\n",
						sizeof(path) - 3);
					/* NOTREACHED */
				}
				strcpy(path, reffile);

				s = path + strlen(path);
				strcpy(s, ".in");
				/*coverity[overwrite_var]*/
				invname = strdup(path);
				strcpy(s, ".po");
				/*coverity[overwrite_var]*/
				invpost = strdup(path);
				break;

			case 'F': /* symbol reference lines file */
				reflines = optarg;
				break;
			case 'i': /* file containing file names */
				namefile = optarg;
				break;
			case 'I': /* #include file directory */
				includedir(optarg);
				break;
			case 'p': /* file path components to display */
				dispcomponents = atoi(optarg);
				break;
			case 'P': /* prepend path to file names */
				prependpath = optarg;
				break;
			case 's': /* additional source file directory */
				sourcedir(optarg);
				break;
		}
	}

    // Sanity checks
	/* XXX remove if/when clearerr() in dir.c does the right thing. */
	if(namefile && strcmp(namefile, "-") == 0 && !buildonly) {
		postfatal(PROGRAM_NAME ": Must use -b if file list comes from stdin\n");
		/* NOTREACHED */
	}

	/* NOTE:
     *  we return where option arguments stop,
     *  assuming that heres a list of files after them,
     *  we process these later
	 */
	return (char**)(argv + optind);
}
