/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 *
 * term.c: functions for controlling the terminal
 *
 * primitive termcap support for Amiga and Win32 included
 *
 * NOTE: padding and variable substitution is not performed,
 * when compiling without HAVE_TGETENT, we use tputs() and tgoto() dummies.
 */

/*
 * Some systems have a prototype for tgetstr() with (char *) instead of
 * (char **). This define removes that prototype. We include our own prototype
 * below.
 */
#define tgetstr tgetstr_defined_wrong

#include "vim.h"

#ifdef HAVE_TGETENT
#ifdef HAVE_TERMIOS_H
#include <termios.h> /* seems to be required for some Linux */
#endif
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif

/*
 * A few linux systems define outfuntype in termcap.h to be used as the third
 * argument for tputs().
 */
#ifdef VMS
#define TPUTSFUNCAST
#else
#ifdef HAVE_OUTFUNTYPE
#define TPUTSFUNCAST (outfuntype)
#else
#define TPUTSFUNCAST (int (*)())
#endif
#endif
#endif

#undef tgetstr

/*
 * Here are the builtin termcap entries.  They are not stored as complete
 * structures with all entries, as such a structure is too big.
 *
 * The entries are compact, therefore they normally are included even when
 * HAVE_TGETENT is defined. When HAVE_TGETENT is defined, the builtin entries
 * can be accessed with "builtin_amiga", "builtin_ansi", "builtin_debug", etc.
 *
 * Each termcap is a list of builtin_term structures. It always starts with
 * KS_NAME, which separates the entries.  See parse_builtin_tcap() for all
 * details.
 * bt_entry is either a KS_xxx code (>= 0), or a K_xxx code.
 *
 * Entries marked with "guessed" may be wrong.
 */
struct builtin_term
{
  int bt_entry;
  char *bt_string;
};

/* start of keys that are not directly used by Vim but can be mapped */
#define BT_EXTRA_KEYS 0x101

static void parse_builtin_tcap(char_u *s);
static void gather_termleader(void);
static void del_termcode_idx(int idx);
static int term_is_builtin(char_u *name);
static int term_7to8bit(char_u *p);

#ifdef HAVE_TGETENT
static char *tgetent_error(char_u *, char_u *);

/*
 * Here is our own prototype for tgetstr(), any prototypes from the include
 * files have been disabled by the define at the start of this file.
 */
char *tgetstr(char *, char **);

/*
 * Don't declare these variables if termcap.h contains them.
 * Autoconf checks if these variables should be declared extern (not all
 * systems have them).
 * Some versions define ospeed to be speed_t, but that is incompatible with
 * BSD, where ospeed is short and speed_t is long.
 */
#ifndef HAVE_OSPEED
#ifdef OSPEED_EXTERN
extern short ospeed;
#else
short ospeed;
#endif
#endif
#ifndef HAVE_UP_BC_PC
#ifdef UP_BC_PC_EXTERN
extern char *UP, *BC, PC;
#else
char *UP, *BC, PC;
#endif
#endif

#define TGETSTR(s, p) vim_tgetstr((s), (p))
#define TGETENT(b, t) tgetent((char *)(b), (char *)(t))
static char_u *vim_tgetstr(char *s, char_u **pp);
#endif /* HAVE_TGETENT */

static int detected_8bit = FALSE; /* detected 8-bit terminal */

static struct builtin_term builtin_termcaps[] =
    {
#ifndef NO_BUILTIN_TCAPS

#if defined(ALL_BUILTIN_TCAPS)
        /*
 * Amiga console window, default for Amiga
 */
        {(int)KS_NAME, "amiga"},
        {(int)KS_CE, "\033[K"},
        {(int)KS_CD, "\033[J"},
        {(int)KS_AL, "\033[L"},
#ifdef TERMINFO
        {(int)KS_CAL, "\033[%p1%dL"},
#else
        {(int)KS_CAL, "\033[%dL"},
#endif
        {(int)KS_DL, "\033[M"},
#ifdef TERMINFO
        {(int)KS_CDL, "\033[%p1%dM"},
#else
        {(int)KS_CDL, "\033[%dM"},
#endif
        {(int)KS_CL, "\014"},
        {(int)KS_VI, "\033[0 p"},
        {(int)KS_VE, "\033[1 p"},
        {(int)KS_ME, "\033[0m"},
        {(int)KS_MR, "\033[7m"},
        {(int)KS_MD, "\033[1m"},
        {(int)KS_SE, "\033[0m"},
        {(int)KS_SO, "\033[33m"},
        {(int)KS_US, "\033[4m"},
        {(int)KS_UE, "\033[0m"},
        {(int)KS_CZH, "\033[3m"},
        {(int)KS_CZR, "\033[0m"},
#if defined(__MORPHOS__) || defined(__AROS__)
        {(int)KS_CCO, "8"}, /* allow 8 colors */
#ifdef TERMINFO
        {(int)KS_CAB, "\033[4%p1%dm"}, /* set background color */
        {(int)KS_CAF, "\033[3%p1%dm"}, /* set foreground color */
#else
        {(int)KS_CAB, "\033[4%dm"},    /* set background color */
        {(int)KS_CAF, "\033[3%dm"},    /* set foreground color */
#endif
        {(int)KS_OP, "\033[m"}, /* reset colors */
#endif
        {(int)KS_MS, "y"},
        {(int)KS_UT, "y"}, /* guessed */
        {(int)KS_LE, "\b"},
#ifdef TERMINFO
        {(int)KS_CM, "\033[%i%p1%d;%p2%dH"},
#else
        {(int)KS_CM, "\033[%i%d;%dH"},
#endif
#if defined(__MORPHOS__)
        {(int)KS_SR, "\033M"},
#endif
#ifdef TERMINFO
        {(int)KS_CRI, "\033[%p1%dC"},
#else
        {(int)KS_CRI, "\033[%dC"},
#endif
        {K_UP, "\233A"},
        {K_DOWN, "\233B"},
        {K_LEFT, "\233D"},
        {K_RIGHT, "\233C"},
        {K_S_UP, "\233T"},
        {K_S_DOWN, "\233S"},
        {K_S_LEFT, "\233 A"},
        {K_S_RIGHT, "\233 @"},
        {K_S_TAB, "\233Z"},
        {K_F1, "\233\060~"}, /* some compilers don't dig "\2330" */
        {K_F2, "\233\061~"},
        {K_F3, "\233\062~"},
        {K_F4, "\233\063~"},
        {K_F5, "\233\064~"},
        {K_F6, "\233\065~"},
        {K_F7, "\233\066~"},
        {K_F8, "\233\067~"},
        {K_F9, "\233\070~"},
        {K_F10, "\233\071~"},
        {K_S_F1, "\233\061\060~"},
        {K_S_F2, "\233\061\061~"},
        {K_S_F3, "\233\061\062~"},
        {K_S_F4, "\233\061\063~"},
        {K_S_F5, "\233\061\064~"},
        {K_S_F6, "\233\061\065~"},
        {K_S_F7, "\233\061\066~"},
        {K_S_F8, "\233\061\067~"},
        {K_S_F9, "\233\061\070~"},
        {K_S_F10, "\233\061\071~"},
        {K_HELP, "\233?~"},
        {K_INS, "\233\064\060~"},      /* 101 key keyboard */
        {K_PAGEUP, "\233\064\061~"},   /* 101 key keyboard */
        {K_PAGEDOWN, "\233\064\062~"}, /* 101 key keyboard */
        {K_HOME, "\233\064\064~"},     /* 101 key keyboard */
        {K_END, "\233\064\065~"},      /* 101 key keyboard */

        {BT_EXTRA_KEYS, ""},
        {TERMCAP2KEY('#', '2'), "\233\065\064~"}, /* shifted home key */
        {TERMCAP2KEY('#', '3'), "\233\065\060~"}, /* shifted insert key */
        {TERMCAP2KEY('*', '7'), "\233\065\065~"}, /* shifted end key */
#endif

#if defined(__BEOS__) || defined(ALL_BUILTIN_TCAPS)
        /*
 * almost standard ANSI terminal, default for bebox
 */
        {(int)KS_NAME, "beos-ansi"},
        {(int)KS_CE, "\033[K"},
        {(int)KS_CD, "\033[J"},
        {(int)KS_AL, "\033[L"},
#ifdef TERMINFO
        {(int)KS_CAL, "\033[%p1%dL"},
#else
        {(int)KS_CAL, "\033[%dL"},
#endif
        {(int)KS_DL, "\033[M"},
#ifdef TERMINFO
        {(int)KS_CDL, "\033[%p1%dM"},
#else
        {(int)KS_CDL, "\033[%dM"},
#endif
#ifdef BEOS_PR_OR_BETTER
#ifdef TERMINFO
        {(int)KS_CS, "\033[%i%p1%d;%p2%dr"},
#else
        {(int)KS_CS, "\033[%i%d;%dr"}, /* scroll region */
#endif
#endif
        {(int)KS_CL, "\033[H\033[2J"},
#ifdef notyet
        {(int)KS_VI, "[VI]"}, /* cursor invisible, VT320: CSI ? 25 l */
        {(int)KS_VE, "[VE]"}, /* cursor visible, VT320: CSI ? 25 h */
#endif
        {(int)KS_ME, "\033[m"},    /* normal mode */
        {(int)KS_MR, "\033[7m"},   /* reverse */
        {(int)KS_MD, "\033[1m"},   /* bold */
        {(int)KS_SO, "\033[31m"},  /* standout mode: red */
        {(int)KS_SE, "\033[m"},    /* standout end */
        {(int)KS_CZH, "\033[35m"}, /* italic: purple */
        {(int)KS_CZR, "\033[m"},   /* italic end */
        {(int)KS_US, "\033[4m"},   /* underscore mode */
        {(int)KS_UE, "\033[m"},    /* underscore end */
        {(int)KS_CCO, "8"},        /* allow 8 colors */
#ifdef TERMINFO
        {(int)KS_CAB, "\033[4%p1%dm"}, /* set background color */
        {(int)KS_CAF, "\033[3%p1%dm"}, /* set foreground color */
#else
        {(int)KS_CAB, "\033[4%dm"}, /* set background color */
        {(int)KS_CAF, "\033[3%dm"}, /* set foreground color */
#endif
        {(int)KS_OP, "\033[m"}, /* reset colors */
        {(int)KS_MS, "y"},      /* safe to move cur in reverse mode */
        {(int)KS_UT, "y"},      /* guessed */
        {(int)KS_LE, "\b"},
#ifdef TERMINFO
        {(int)KS_CM, "\033[%i%p1%d;%p2%dH"},
#else
        {(int)KS_CM, "\033[%i%d;%dH"},
#endif
        {(int)KS_SR, "\033M"},
#ifdef TERMINFO
        {(int)KS_CRI, "\033[%p1%dC"},
#else
        {(int)KS_CRI, "\033[%dC"},
#endif
#if defined(BEOS_DR8)
        {(int)KS_DB, ""}, /* hack! see screen.c */
#endif

        {K_UP, "\033[A"},
        {K_DOWN, "\033[B"},
        {K_LEFT, "\033[D"},
        {K_RIGHT, "\033[C"},
#endif

#if defined(UNIX) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS)
        /*
 * standard ANSI terminal, default for unix
 */
        {(int)KS_NAME, "ansi"},
        {(int)KS_CE, IF_EB("\033[K", ESC_STR "[K")},
        {(int)KS_AL, IF_EB("\033[L", ESC_STR "[L")},
#ifdef TERMINFO
        {(int)KS_CAL, IF_EB("\033[%p1%dL", ESC_STR "[%p1%dL")},
#else
        {(int)KS_CAL, IF_EB("\033[%dL", ESC_STR "[%dL")},
#endif
        {(int)KS_DL, IF_EB("\033[M", ESC_STR "[M")},
#ifdef TERMINFO
        {(int)KS_CDL, IF_EB("\033[%p1%dM", ESC_STR "[%p1%dM")},
#else
        {(int)KS_CDL, IF_EB("\033[%dM", ESC_STR "[%dM")},
#endif
        {(int)KS_CL, IF_EB("\033[H\033[2J", ESC_STR "[H" ESC_STR_nc "[2J")},
        {(int)KS_ME, IF_EB("\033[0m", ESC_STR "[0m")},
        {(int)KS_MR, IF_EB("\033[7m", ESC_STR "[7m")},
        {(int)KS_MS, "y"},
        {(int)KS_UT, "y"}, /* guessed */
        {(int)KS_LE, "\b"},
#ifdef TERMINFO
        {(int)KS_CM, IF_EB("\033[%i%p1%d;%p2%dH", ESC_STR "[%i%p1%d;%p2%dH")},
#else
        {(int)KS_CM, IF_EB("\033[%i%d;%dH", ESC_STR "[%i%d;%dH")},
#endif
#ifdef TERMINFO
        {(int)KS_CRI, IF_EB("\033[%p1%dC", ESC_STR "[%p1%dC")},
#else
        {(int)KS_CRI, IF_EB("\033[%dC", ESC_STR "[%dC")},
#endif
#endif

#if defined(ALL_BUILTIN_TCAPS)
        /*
 * These codes are valid when nansi.sys or equivalent has been installed.
 * Function keys on a PC are preceded with a NUL. These are converted into
 * K_NUL '\316' in mch_inchar(), because we cannot handle NULs in key codes.
 * CTRL-arrow is used instead of SHIFT-arrow.
 */
        {(int)KS_NAME, "pcansi"},
        {(int)KS_DL, "\033[M"},
        {(int)KS_AL, "\033[L"},
        {(int)KS_CE, "\033[K"},
        {(int)KS_CL, "\033[2J"},
        {(int)KS_ME, "\033[0m"},
        {(int)KS_MR, "\033[5m"},      /* reverse: black on lightgrey */
        {(int)KS_MD, "\033[1m"},      /* bold: white text */
        {(int)KS_SE, "\033[0m"},      /* standout end */
        {(int)KS_SO, "\033[31m"},     /* standout: white on blue */
        {(int)KS_CZH, "\033[34;43m"}, /* italic mode: blue text on yellow */
        {(int)KS_CZR, "\033[0m"},     /* italic mode end */
        {(int)KS_US, "\033[36;41m"},  /* underscore mode: cyan text on red */
        {(int)KS_UE, "\033[0m"},      /* underscore mode end */
        {(int)KS_CCO, "8"},           /* allow 8 colors */
#ifdef TERMINFO
        {(int)KS_CAB, "\033[4%p1%dm"}, /* set background color */
        {(int)KS_CAF, "\033[3%p1%dm"}, /* set foreground color */
#else
        {(int)KS_CAB, "\033[4%dm"}, /* set background color */
        {(int)KS_CAF, "\033[3%dm"}, /* set foreground color */
#endif
        {(int)KS_OP, "\033[0m"}, /* reset colors */
        {(int)KS_MS, "y"},
        {(int)KS_UT, "y"}, /* guessed */
        {(int)KS_LE, "\b"},
#ifdef TERMINFO
        {(int)KS_CM, "\033[%i%p1%d;%p2%dH"},
#else
        {(int)KS_CM, "\033[%i%d;%dH"},
#endif
#ifdef TERMINFO
        {(int)KS_CRI, "\033[%p1%dC"},
#else
        {(int)KS_CRI, "\033[%dC"},
#endif
        {K_UP, "\316H"},
        {K_DOWN, "\316P"},
        {K_LEFT, "\316K"},
        {K_RIGHT, "\316M"},
        {K_S_LEFT, "\316s"},
        {K_S_RIGHT, "\316t"},
        {K_F1, "\316;"},
        {K_F2, "\316<"},
        {K_F3, "\316="},
        {K_F4, "\316>"},
        {K_F5, "\316?"},
        {K_F6, "\316@"},
        {K_F7, "\316A"},
        {K_F8, "\316B"},
        {K_F9, "\316C"},
        {K_F10, "\316D"},
        {K_F11, "\316\205"}, /* guessed */
        {K_F12, "\316\206"}, /* guessed */
        {K_S_F1, "\316T"},
        {K_S_F2, "\316U"},
        {K_S_F3, "\316V"},
        {K_S_F4, "\316W"},
        {K_S_F5, "\316X"},
        {K_S_F6, "\316Y"},
        {K_S_F7, "\316Z"},
        {K_S_F8, "\316["},
        {K_S_F9, "\316\\"},
        {K_S_F10, "\316]"},
        {K_S_F11, "\316\207"}, /* guessed */
        {K_S_F12, "\316\210"}, /* guessed */
        {K_INS, "\316R"},
        {K_DEL, "\316S"},
        {K_HOME, "\316G"},
        {K_END, "\316O"},
        {K_PAGEDOWN, "\316Q"},
        {K_PAGEUP, "\316I"},
#endif

#if defined(MSWIN) || defined(ALL_BUILTIN_TCAPS)
        /*
 * These codes are valid for the Win32 Console .  The entries that start with
 * ESC | are translated into console calls in os_win32.c.  The function keys
 * are also translated in os_win32.c.
 */
        {(int)KS_NAME, "win32"},
        {(int)KS_CE, "\033|K"}, // clear to end of line
        {(int)KS_AL, "\033|L"}, // add new blank line
#ifdef TERMINFO
        {(int)KS_CAL, "\033|%p1%dL"}, // add number of new blank lines
#else
        {(int)KS_CAL, "\033|%dL"}, // add number of new blank lines
#endif
        {(int)KS_DL, "\033|M"}, // delete line
#ifdef TERMINFO
        {(int)KS_CDL, "\033|%p1%dM"}, // delete number of lines
        {(int)KS_CSV, "\033|%p1%d;%p2%dV"},
#else
        {(int)KS_CDL, "\033|%dM"}, // delete number of lines
        {(int)KS_CSV, "\033|%d;%dV"},
#endif
        {(int)KS_CL, "\033|J"}, // clear screen
        {(int)KS_CD, "\033|j"}, // clear to end of display
        {(int)KS_VI, "\033|v"}, // cursor invisible
        {(int)KS_VE, "\033|V"}, // cursor visible

        {(int)KS_ME, "\033|0m"},   // normal
        {(int)KS_MR, "\033|112m"}, // reverse: black on lightgray
        {(int)KS_MD, "\033|15m"},  // bold: white on black
#if 1
        {(int)KS_SO, "\033|31m"}, // standout: white on blue
        {(int)KS_SE, "\033|0m"},  // standout end
#else
        {(int)KS_SO, "\033|F"},        // standout: high intensity
        {(int)KS_SE, "\033|f"},        // standout end
#endif
        {(int)KS_CZH, "\033|225m"}, // italic: blue text on yellow
        {(int)KS_CZR, "\033|0m"},   // italic end
        {(int)KS_US, "\033|67m"},   // underscore: cyan text on red
        {(int)KS_UE, "\033|0m"},    // underscore end
        {(int)KS_CCO, "16"},        // allow 16 colors
#ifdef TERMINFO
        {(int)KS_CAB, "\033|%p1%db"}, // set background color
        {(int)KS_CAF, "\033|%p1%df"}, // set foreground color
#else
        {(int)KS_CAB, "\033|%db"},     // set background color
        {(int)KS_CAF, "\033|%df"},     // set foreground color
#endif

        {(int)KS_MS, "y"}, // save to move cur in reverse mode
        {(int)KS_UT, "y"},
        {(int)KS_XN, "y"},
        {(int)KS_LE, "\b"},
#ifdef TERMINFO
        {(int)KS_CM, "\033|%i%p1%d;%p2%dH"}, // cursor motion
#else
        {(int)KS_CM, "\033|%i%d;%dH"}, // cursor motion
#endif
        {(int)KS_VB, "\033|B"}, // visual bell
        {(int)KS_TI, "\033|S"}, // put terminal in termcap mode
        {(int)KS_TE, "\033|E"}, // out of termcap mode
#ifdef TERMINFO
        {(int)KS_CS, "\033|%i%p1%d;%p2%dr"}, // scroll region
#else
        {(int)KS_CS, "\033|%i%d;%dr"}, // scroll region
#endif

        {K_UP, "\316H"},
        {K_DOWN, "\316P"},
        {K_LEFT, "\316K"},
        {K_RIGHT, "\316M"},
        {K_S_UP, "\316\304"},
        {K_S_DOWN, "\316\317"},
        {K_S_LEFT, "\316\311"},
        {K_C_LEFT, "\316s"},
        {K_S_RIGHT, "\316\313"},
        {K_C_RIGHT, "\316t"},
        {K_S_TAB, "\316\017"},
        {K_F1, "\316;"},
        {K_F2, "\316<"},
        {K_F3, "\316="},
        {K_F4, "\316>"},
        {K_F5, "\316?"},
        {K_F6, "\316@"},
        {K_F7, "\316A"},
        {K_F8, "\316B"},
        {K_F9, "\316C"},
        {K_F10, "\316D"},
        {K_F11, "\316\205"},
        {K_F12, "\316\206"},
        {K_S_F1, "\316T"},
        {K_S_F2, "\316U"},
        {K_S_F3, "\316V"},
        {K_S_F4, "\316W"},
        {K_S_F5, "\316X"},
        {K_S_F6, "\316Y"},
        {K_S_F7, "\316Z"},
        {K_S_F8, "\316["},
        {K_S_F9, "\316\\"},
        {K_S_F10, "\316]"},
        {K_S_F11, "\316\207"},
        {K_S_F12, "\316\210"},
        {K_INS, "\316R"},
        {K_DEL, "\316S"},
        {K_HOME, "\316G"},
        {K_S_HOME, "\316\302"},
        {K_C_HOME, "\316w"},
        {K_END, "\316O"},
        {K_S_END, "\316\315"},
        {K_C_END, "\316u"},
        {K_PAGEDOWN, "\316Q"},
        {K_PAGEUP, "\316I"},
        {K_KPLUS, "\316N"},
        {K_KMINUS, "\316J"},
        {K_KMULTIPLY, "\316\067"},
        {K_K0, "\316\332"},
        {K_K1, "\316\336"},
        {K_K2, "\316\342"},
        {K_K3, "\316\346"},
        {K_K4, "\316\352"},
        {K_K5, "\316\356"},
        {K_K6, "\316\362"},
        {K_K7, "\316\366"},
        {K_K8, "\316\372"},
        {K_K9, "\316\376"},
        {K_BS, "\316x"},
#endif

#if defined(VMS) || defined(ALL_BUILTIN_TCAPS)
        /*
 * VT320 is working as an ANSI terminal compatible DEC terminal.
 * (it covers VT1x0, VT2x0 and VT3x0 up to VT320 on VMS as well)
 * TODO:- rewrite ESC[ codes to CSI
 *      - keyboard languages (CSI ? 26 n)
 */
        {(int)KS_NAME, "vt320"},
        {(int)KS_CE, IF_EB("\033[K", ESC_STR "[K")},
        {(int)KS_AL, IF_EB("\033[L", ESC_STR "[L")},
#ifdef TERMINFO
        {(int)KS_CAL, IF_EB("\033[%p1%dL", ESC_STR "[%p1%dL")},
#else
        {(int)KS_CAL, IF_EB("\033[%dL", ESC_STR "[%dL")},
#endif
        {(int)KS_DL, IF_EB("\033[M", ESC_STR "[M")},
#ifdef TERMINFO
        {(int)KS_CDL, IF_EB("\033[%p1%dM", ESC_STR "[%p1%dM")},
#else
        {(int)KS_CDL, IF_EB("\033[%dM", ESC_STR "[%dM")},
#endif
        {(int)KS_CL, IF_EB("\033[H\033[2J", ESC_STR "[H" ESC_STR_nc "[2J")},
        {(int)KS_CD, IF_EB("\033[J", ESC_STR "[J")},
        {(int)KS_CCO, "8"}, /* allow 8 colors */
        {(int)KS_ME, IF_EB("\033[0m", ESC_STR "[0m")},
        {(int)KS_MR, IF_EB("\033[7m", ESC_STR "[7m")},
        {(int)KS_MD, IF_EB("\033[1m", ESC_STR "[1m")},            /* bold mode */
        {(int)KS_SE, IF_EB("\033[22m", ESC_STR "[22m")},          /* normal mode */
        {(int)KS_UE, IF_EB("\033[24m", ESC_STR "[24m")},          /* exit underscore mode */
        {(int)KS_US, IF_EB("\033[4m", ESC_STR "[4m")},            /* underscore mode */
        {(int)KS_CZH, IF_EB("\033[34;43m", ESC_STR "[34;43m")},   /* italic mode: blue text on yellow */
        {(int)KS_CZR, IF_EB("\033[0m", ESC_STR "[0m")},           /* italic mode end */
        {(int)KS_CAB, IF_EB("\033[4%dm", ESC_STR "[4%dm")},       /* set background color (ANSI) */
        {(int)KS_CAF, IF_EB("\033[3%dm", ESC_STR "[3%dm")},       /* set foreground color (ANSI) */
        {(int)KS_CSB, IF_EB("\033[102;%dm", ESC_STR "[102;%dm")}, /* set screen background color */
        {(int)KS_CSF, IF_EB("\033[101;%dm", ESC_STR "[101;%dm")}, /* set screen foreground color */
        {(int)KS_MS, "y"},
        {(int)KS_UT, "y"},
        {(int)KS_XN, "y"},
        {(int)KS_LE, "\b"},
#ifdef TERMINFO
        {(int)KS_CM, IF_EB("\033[%i%p1%d;%p2%dH",
                           ESC_STR "[%i%p1%d;%p2%dH")},
#else
        {(int)KS_CM, IF_EB("\033[%i%d;%dH", ESC_STR "[%i%d;%dH")},
#endif
#ifdef TERMINFO
        {(int)KS_CRI, IF_EB("\033[%p1%dC", ESC_STR "[%p1%dC")},
#else
        {(int)KS_CRI, IF_EB("\033[%dC", ESC_STR "[%dC")},
#endif
        {K_UP, IF_EB("\033[A", ESC_STR "[A")},
        {K_DOWN, IF_EB("\033[B", ESC_STR "[B")},
        {K_RIGHT, IF_EB("\033[C", ESC_STR "[C")},
        {K_LEFT, IF_EB("\033[D", ESC_STR "[D")},
        // Note: cursor key sequences for application cursor mode are omitted,
        // because they interfere with typed commands: <Esc>OA.
        {K_F1, IF_EB("\033[11~", ESC_STR "[11~")},
        {K_F2, IF_EB("\033[12~", ESC_STR "[12~")},
        {K_F3, IF_EB("\033[13~", ESC_STR "[13~")},
        {K_F4, IF_EB("\033[14~", ESC_STR "[14~")},
        {K_F5, IF_EB("\033[15~", ESC_STR "[15~")},
        {K_F6, IF_EB("\033[17~", ESC_STR "[17~")},
        {K_F7, IF_EB("\033[18~", ESC_STR "[18~")},
        {K_F8, IF_EB("\033[19~", ESC_STR "[19~")},
        {K_F9, IF_EB("\033[20~", ESC_STR "[20~")},
        {K_F10, IF_EB("\033[21~", ESC_STR "[21~")},
        {K_F11, IF_EB("\033[23~", ESC_STR "[23~")},
        {K_F12, IF_EB("\033[24~", ESC_STR "[24~")},
        {K_F13, IF_EB("\033[25~", ESC_STR "[25~")},
        {K_F14, IF_EB("\033[26~", ESC_STR "[26~")},
        {K_F15, IF_EB("\033[28~", ESC_STR "[28~")}, /* Help */
        {K_F16, IF_EB("\033[29~", ESC_STR "[29~")}, /* Select */
        {K_F17, IF_EB("\033[31~", ESC_STR "[31~")},
        {K_F18, IF_EB("\033[32~", ESC_STR "[32~")},
        {K_F19, IF_EB("\033[33~", ESC_STR "[33~")},
        {K_F20, IF_EB("\033[34~", ESC_STR "[34~")},
        {K_INS, IF_EB("\033[2~", ESC_STR "[2~")},
        {K_DEL, IF_EB("\033[3~", ESC_STR "[3~")},
        {K_HOME, IF_EB("\033[1~", ESC_STR "[1~")},
        {K_END, IF_EB("\033[4~", ESC_STR "[4~")},
        {K_PAGEUP, IF_EB("\033[5~", ESC_STR "[5~")},
        {K_PAGEDOWN, IF_EB("\033[6~", ESC_STR "[6~")},
        // These sequences starting with <Esc> O may interfere with what the user
        // is typing.  Remove these if that bothers you.
        {K_KPLUS, IF_EB("\033Ok", ESC_STR "Ok")},     /* keypad plus */
        {K_KMINUS, IF_EB("\033Om", ESC_STR "Om")},    /* keypad minus */
        {K_KDIVIDE, IF_EB("\033Oo", ESC_STR "Oo")},   /* keypad / */
        {K_KMULTIPLY, IF_EB("\033Oj", ESC_STR "Oj")}, /* keypad * */
        {K_KENTER, IF_EB("\033OM", ESC_STR "OM")},    /* keypad Enter */
        {K_K0, IF_EB("\033Op", ESC_STR "Op")},        /* keypad 0 */
        {K_K1, IF_EB("\033Oq", ESC_STR "Oq")},        /* keypad 1 */
        {K_K2, IF_EB("\033Or", ESC_STR "Or")},        /* keypad 2 */
        {K_K3, IF_EB("\033Os", ESC_STR "Os")},        /* keypad 3 */
        {K_K4, IF_EB("\033Ot", ESC_STR "Ot")},        /* keypad 4 */
        {K_K5, IF_EB("\033Ou", ESC_STR "Ou")},        /* keypad 5 */
        {K_K6, IF_EB("\033Ov", ESC_STR "Ov")},        /* keypad 6 */
        {K_K7, IF_EB("\033Ow", ESC_STR "Ow")},        /* keypad 7 */
        {K_K8, IF_EB("\033Ox", ESC_STR "Ox")},        /* keypad 8 */
        {K_K9, IF_EB("\033Oy", ESC_STR "Oy")},        /* keypad 9 */
        {K_BS, "\x7f"},                               /* for some reason 0177 doesn't work */
#endif

#if defined(ALL_BUILTIN_TCAPS) || defined(__MINT__)
        /*
 * Ordinary vt52
 */
        {(int)KS_NAME, "vt52"},
        {(int)KS_CE, IF_EB("\033K", ESC_STR "K")},
        {(int)KS_CD, IF_EB("\033J", ESC_STR "J")},
#ifdef TERMINFO
        {(int)KS_CM, IF_EB("\033Y%p1%' '%+%c%p2%' '%+%c",
                           ESC_STR "Y%p1%' '%+%c%p2%' '%+%c")},
#else
        {(int)KS_CM, IF_EB("\033Y%+ %+ ", ESC_STR "Y%+ %+ ")},
#endif
        {(int)KS_LE, "\b"},
        {(int)KS_SR, IF_EB("\033I", ESC_STR "I")},
        {(int)KS_AL, IF_EB("\033L", ESC_STR "L")},
        {(int)KS_DL, IF_EB("\033M", ESC_STR "M")},
        {K_UP, IF_EB("\033A", ESC_STR "A")},
        {K_DOWN, IF_EB("\033B", ESC_STR "B")},
        {K_LEFT, IF_EB("\033D", ESC_STR "D")},
        {K_RIGHT, IF_EB("\033C", ESC_STR "C")},
        {K_F1, IF_EB("\033P", ESC_STR "P")},
        {K_F2, IF_EB("\033Q", ESC_STR "Q")},
        {K_F3, IF_EB("\033R", ESC_STR "R")},
#ifdef __MINT__
        {(int)KS_CL, IF_EB("\033E", ESC_STR "E")},
        {(int)KS_VE, IF_EB("\033e", ESC_STR "e")},
        {(int)KS_VI, IF_EB("\033f", ESC_STR "f")},
        {(int)KS_SO, IF_EB("\033p", ESC_STR "p")},
        {(int)KS_SE, IF_EB("\033q", ESC_STR "q")},
        {K_S_UP, IF_EB("\033a", ESC_STR "a")},
        {K_S_DOWN, IF_EB("\033b", ESC_STR "b")},
        {K_S_LEFT, IF_EB("\033d", ESC_STR "d")},
        {K_S_RIGHT, IF_EB("\033c", ESC_STR "c")},
        {K_F4, IF_EB("\033S", ESC_STR "S")},
        {K_F5, IF_EB("\033T", ESC_STR "T")},
        {K_F6, IF_EB("\033U", ESC_STR "U")},
        {K_F7, IF_EB("\033V", ESC_STR "V")},
        {K_F8, IF_EB("\033W", ESC_STR "W")},
        {K_F9, IF_EB("\033X", ESC_STR "X")},
        {K_F10, IF_EB("\033Y", ESC_STR "Y")},
        {K_S_F1, IF_EB("\033p", ESC_STR "p")},
        {K_S_F2, IF_EB("\033q", ESC_STR "q")},
        {K_S_F3, IF_EB("\033r", ESC_STR "r")},
        {K_S_F4, IF_EB("\033s", ESC_STR "s")},
        {K_S_F5, IF_EB("\033t", ESC_STR "t")},
        {K_S_F6, IF_EB("\033u", ESC_STR "u")},
        {K_S_F7, IF_EB("\033v", ESC_STR "v")},
        {K_S_F8, IF_EB("\033w", ESC_STR "w")},
        {K_S_F9, IF_EB("\033x", ESC_STR "x")},
        {K_S_F10, IF_EB("\033y", ESC_STR "y")},
        {K_INS, IF_EB("\033I", ESC_STR "I")},
        {K_HOME, IF_EB("\033E", ESC_STR "E")},
        {K_PAGEDOWN, IF_EB("\033b", ESC_STR "b")},
        {K_PAGEUP, IF_EB("\033a", ESC_STR "a")},
#else
        {(int)KS_CL, IF_EB("\033H\033J", ESC_STR "H" ESC_STR_nc "J")},
        {(int)KS_MS, "y"},
#endif
#endif

#if defined(UNIX) || defined(ALL_BUILTIN_TCAPS) || defined(SOME_BUILTIN_TCAPS)
        {(int)KS_NAME, "xterm"},
        {(int)KS_CE, IF_EB("\033[K", ESC_STR "[K")},
        {(int)KS_AL, IF_EB("\033[L", ESC_STR "[L")},
#ifdef TERMINFO
        {(int)KS_CAL, IF_EB("\033[%p1%dL", ESC_STR "[%p1%dL")},
#else
        {(int)KS_CAL, IF_EB("\033[%dL", ESC_STR "[%dL")},
#endif
        {(int)KS_DL, IF_EB("\033[M", ESC_STR "[M")},
#ifdef TERMINFO
        {(int)KS_CDL, IF_EB("\033[%p1%dM", ESC_STR "[%p1%dM")},
#else
        {(int)KS_CDL, IF_EB("\033[%dM", ESC_STR "[%dM")},
#endif
#ifdef TERMINFO
        {(int)KS_CS, IF_EB("\033[%i%p1%d;%p2%dr",
                           ESC_STR "[%i%p1%d;%p2%dr")},
#else
        {(int)KS_CS, IF_EB("\033[%i%d;%dr", ESC_STR "[%i%d;%dr")},
#endif
        {(int)KS_CL, IF_EB("\033[H\033[2J", ESC_STR "[H" ESC_STR_nc "[2J")},
        {(int)KS_CD, IF_EB("\033[J", ESC_STR "[J")},
        {(int)KS_ME, IF_EB("\033[m", ESC_STR "[m")},
        {(int)KS_MR, IF_EB("\033[7m", ESC_STR "[7m")},
        {(int)KS_MD, IF_EB("\033[1m", ESC_STR "[1m")},
        {(int)KS_UE, IF_EB("\033[m", ESC_STR "[m")},
        {(int)KS_US, IF_EB("\033[4m", ESC_STR "[4m")},
        {(int)KS_STE, IF_EB("\033[29m", ESC_STR "[29m")},
        {(int)KS_STS, IF_EB("\033[9m", ESC_STR "[9m")},
        {(int)KS_MS, "y"},
        {(int)KS_UT, "y"},
        {(int)KS_LE, "\b"},
        {(int)KS_VI, IF_EB("\033[?25l", ESC_STR "[?25l")},
        {(int)KS_VE, IF_EB("\033[?25h", ESC_STR "[?25h")},
        {(int)KS_VS, IF_EB("\033[?12h", ESC_STR "[?12h")},
        {(int)KS_CVS, IF_EB("\033[?12l", ESC_STR "[?12l")},
#ifdef TERMINFO
        {(int)KS_CSH, IF_EB("\033[%p1%d q", ESC_STR "[%p1%d q")},
#else
        {(int)KS_CSH, IF_EB("\033[%d q", ESC_STR "[%d q")},
#endif
        {(int)KS_CRC, IF_EB("\033[?12$p", ESC_STR "[?12$p")},
        {(int)KS_CRS, IF_EB("\033P$q q\033\\", ESC_STR "P$q q" ESC_STR "\\")},
#ifdef TERMINFO
        {(int)KS_CM, IF_EB("\033[%i%p1%d;%p2%dH",
                           ESC_STR "[%i%p1%d;%p2%dH")},
#else
        {(int)KS_CM, IF_EB("\033[%i%d;%dH", ESC_STR "[%i%d;%dH")},
#endif
        {(int)KS_SR, IF_EB("\033M", ESC_STR "M")},
#ifdef TERMINFO
        {(int)KS_CRI, IF_EB("\033[%p1%dC", ESC_STR "[%p1%dC")},
#else
        {(int)KS_CRI, IF_EB("\033[%dC", ESC_STR "[%dC")},
#endif
        {(int)KS_KS, IF_EB("\033[?1h\033=", ESC_STR "[?1h" ESC_STR_nc "=")},
        {(int)KS_KE, IF_EB("\033[?1l\033>", ESC_STR "[?1l" ESC_STR_nc ">")},
#ifdef FEAT_XTERM_SAVE
        {(int)KS_TI, IF_EB("\0337\033[?47h", ESC_STR "7" ESC_STR_nc "[?47h")},
        {(int)KS_TE, IF_EB("\033[2J\033[?47l\0338",
                           ESC_STR "[2J" ESC_STR_nc "[?47l" ESC_STR_nc "8")},
#endif
        {(int)KS_CIS, IF_EB("\033]1;", ESC_STR "]1;")},
        {(int)KS_CIE, "\007"},
        {(int)KS_TS, IF_EB("\033]2;", ESC_STR "]2;")},
        {(int)KS_FS, "\007"},
        {(int)KS_CSC, IF_EB("\033]12;", ESC_STR "]12;")},
        {(int)KS_CEC, "\007"},
#ifdef TERMINFO
        {(int)KS_CWS, IF_EB("\033[8;%p1%d;%p2%dt",
                            ESC_STR "[8;%p1%d;%p2%dt")},
        {(int)KS_CWP, IF_EB("\033[3;%p1%d;%p2%dt",
                            ESC_STR "[3;%p1%d;%p2%dt")},
        {(int)KS_CGP, IF_EB("\033[13t", ESC_STR "[13t")},
#else
        {(int)KS_CWS, IF_EB("\033[8;%d;%dt", ESC_STR "[8;%d;%dt")},
        {(int)KS_CWP, IF_EB("\033[3;%d;%dt", ESC_STR "[3;%d;%dt")},
        {(int)KS_CGP, IF_EB("\033[13t", ESC_STR "[13t")},
#endif
        {(int)KS_CRV, IF_EB("\033[>c", ESC_STR "[>c")},
        {(int)KS_RFG, IF_EB("\033]10;?\007", ESC_STR "]10;?\007")},
        {(int)KS_RBG, IF_EB("\033]11;?\007", ESC_STR "]11;?\007")},
        {(int)KS_U7, IF_EB("\033[6n", ESC_STR "[6n")},
        {(int)KS_CBE, IF_EB("\033[?2004h", ESC_STR "[?2004h")},
        {(int)KS_CBD, IF_EB("\033[?2004l", ESC_STR "[?2004l")},
        {(int)KS_CST, IF_EB("\033[22;2t", ESC_STR "[22;2t")},
        {(int)KS_CRT, IF_EB("\033[23;2t", ESC_STR "[23;2t")},
        {(int)KS_SSI, IF_EB("\033[22;1t", ESC_STR "[22;1t")},
        {(int)KS_SRI, IF_EB("\033[23;1t", ESC_STR "[23;1t")},

        {K_UP, IF_EB("\033O*A", ESC_STR "O*A")},
        {K_DOWN, IF_EB("\033O*B", ESC_STR "O*B")},
        {K_RIGHT, IF_EB("\033O*C", ESC_STR "O*C")},
        {K_LEFT, IF_EB("\033O*D", ESC_STR "O*D")},
        /* An extra set of cursor keys for vt100 mode */
        {K_XUP, IF_EB("\033[1;*A", ESC_STR "[1;*A")},
        {K_XDOWN, IF_EB("\033[1;*B", ESC_STR "[1;*B")},
        {K_XRIGHT, IF_EB("\033[1;*C", ESC_STR "[1;*C")},
        {K_XLEFT, IF_EB("\033[1;*D", ESC_STR "[1;*D")},
        /* An extra set of function keys for vt100 mode */
        {K_XF1, IF_EB("\033O*P", ESC_STR "O*P")},
        {K_XF2, IF_EB("\033O*Q", ESC_STR "O*Q")},
        {K_XF3, IF_EB("\033O*R", ESC_STR "O*R")},
        {K_XF4, IF_EB("\033O*S", ESC_STR "O*S")},
        {K_F1, IF_EB("\033[11;*~", ESC_STR "[11;*~")},
        {K_F2, IF_EB("\033[12;*~", ESC_STR "[12;*~")},
        {K_F3, IF_EB("\033[13;*~", ESC_STR "[13;*~")},
        {K_F4, IF_EB("\033[14;*~", ESC_STR "[14;*~")},
        {K_F5, IF_EB("\033[15;*~", ESC_STR "[15;*~")},
        {K_F6, IF_EB("\033[17;*~", ESC_STR "[17;*~")},
        {K_F7, IF_EB("\033[18;*~", ESC_STR "[18;*~")},
        {K_F8, IF_EB("\033[19;*~", ESC_STR "[19;*~")},
        {K_F9, IF_EB("\033[20;*~", ESC_STR "[20;*~")},
        {K_F10, IF_EB("\033[21;*~", ESC_STR "[21;*~")},
        {K_F11, IF_EB("\033[23;*~", ESC_STR "[23;*~")},
        {K_F12, IF_EB("\033[24;*~", ESC_STR "[24;*~")},
        {K_S_TAB, IF_EB("\033[Z", ESC_STR "[Z")},
        {K_HELP, IF_EB("\033[28;*~", ESC_STR "[28;*~")},
        {K_UNDO, IF_EB("\033[26;*~", ESC_STR "[26;*~")},
        {K_INS, IF_EB("\033[2;*~", ESC_STR "[2;*~")},
        {K_HOME, IF_EB("\033[1;*H", ESC_STR "[1;*H")},
        /* {K_S_HOME,		IF_EB("\033O2H", ESC_STR "O2H")}, */
        /* {K_C_HOME,		IF_EB("\033O5H", ESC_STR "O5H")}, */
        {K_KHOME, IF_EB("\033[1;*~", ESC_STR "[1;*~")},
        {K_XHOME, IF_EB("\033O*H", ESC_STR "O*H")},     /* other Home */
        {K_ZHOME, IF_EB("\033[7;*~", ESC_STR "[7;*~")}, /* other Home */
        {K_END, IF_EB("\033[1;*F", ESC_STR "[1;*F")},
        /* {K_S_END,		IF_EB("\033O2F", ESC_STR "O2F")}, */
        /* {K_C_END,		IF_EB("\033O5F", ESC_STR "O5F")}, */
        {K_KEND, IF_EB("\033[4;*~", ESC_STR "[4;*~")},
        {K_XEND, IF_EB("\033O*F", ESC_STR "O*F")}, /* other End */
        {K_ZEND, IF_EB("\033[8;*~", ESC_STR "[8;*~")},
        {K_PAGEUP, IF_EB("\033[5;*~", ESC_STR "[5;*~")},
        {K_PAGEDOWN, IF_EB("\033[6;*~", ESC_STR "[6;*~")},
        {K_KPLUS, IF_EB("\033O*k", ESC_STR "O*k")},     /* keypad plus */
        {K_KMINUS, IF_EB("\033O*m", ESC_STR "O*m")},    /* keypad minus */
        {K_KDIVIDE, IF_EB("\033O*o", ESC_STR "O*o")},   /* keypad / */
        {K_KMULTIPLY, IF_EB("\033O*j", ESC_STR "O*j")}, /* keypad * */
        {K_KENTER, IF_EB("\033O*M", ESC_STR "O*M")},    /* keypad Enter */
        {K_KPOINT, IF_EB("\033O*n", ESC_STR "O*n")},    /* keypad . */
        {K_K0, IF_EB("\033O*p", ESC_STR "O*p")},        /* keypad 0 */
        {K_K1, IF_EB("\033O*q", ESC_STR "O*q")},        /* keypad 1 */
        {K_K2, IF_EB("\033O*r", ESC_STR "O*r")},        /* keypad 2 */
        {K_K3, IF_EB("\033O*s", ESC_STR "O*s")},        /* keypad 3 */
        {K_K4, IF_EB("\033O*t", ESC_STR "O*t")},        /* keypad 4 */
        {K_K5, IF_EB("\033O*u", ESC_STR "O*u")},        /* keypad 5 */
        {K_K6, IF_EB("\033O*v", ESC_STR "O*v")},        /* keypad 6 */
        {K_K7, IF_EB("\033O*w", ESC_STR "O*w")},        /* keypad 7 */
        {K_K8, IF_EB("\033O*x", ESC_STR "O*x")},        /* keypad 8 */
        {K_K9, IF_EB("\033O*y", ESC_STR "O*y")},        /* keypad 9 */
        {K_KDEL, IF_EB("\033[3;*~", ESC_STR "[3;*~")},  /* keypad Del */
        {K_PS, IF_EB("\033[200~", ESC_STR "[200~")},    /* paste start */
        {K_PE, IF_EB("\033[201~", ESC_STR "[201~")},    /* paste end */

        {BT_EXTRA_KEYS, ""},
        {TERMCAP2KEY('k', '0'), IF_EB("\033[10;*~", ESC_STR "[10;*~")}, /* F0 */
        {TERMCAP2KEY('F', '3'), IF_EB("\033[25;*~", ESC_STR "[25;*~")}, /* F13 */
        /* F14 and F15 are missing, because they send the same codes as the undo
     * and help key, although they don't work on all keyboards. */
        {TERMCAP2KEY('F', '6'), IF_EB("\033[29;*~", ESC_STR "[29;*~")}, /* F16 */
        {TERMCAP2KEY('F', '7'), IF_EB("\033[31;*~", ESC_STR "[31;*~")}, /* F17 */
        {TERMCAP2KEY('F', '8'), IF_EB("\033[32;*~", ESC_STR "[32;*~")}, /* F18 */
        {TERMCAP2KEY('F', '9'), IF_EB("\033[33;*~", ESC_STR "[33;*~")}, /* F19 */
        {TERMCAP2KEY('F', 'A'), IF_EB("\033[34;*~", ESC_STR "[34;*~")}, /* F20 */

        {TERMCAP2KEY('F', 'B'), IF_EB("\033[42;*~", ESC_STR "[42;*~")}, /* F21 */
        {TERMCAP2KEY('F', 'C'), IF_EB("\033[43;*~", ESC_STR "[43;*~")}, /* F22 */
        {TERMCAP2KEY('F', 'D'), IF_EB("\033[44;*~", ESC_STR "[44;*~")}, /* F23 */
        {TERMCAP2KEY('F', 'E'), IF_EB("\033[45;*~", ESC_STR "[45;*~")}, /* F24 */
        {TERMCAP2KEY('F', 'F'), IF_EB("\033[46;*~", ESC_STR "[46;*~")}, /* F25 */
        {TERMCAP2KEY('F', 'G'), IF_EB("\033[47;*~", ESC_STR "[47;*~")}, /* F26 */
        {TERMCAP2KEY('F', 'H'), IF_EB("\033[48;*~", ESC_STR "[48;*~")}, /* F27 */
        {TERMCAP2KEY('F', 'I'), IF_EB("\033[49;*~", ESC_STR "[49;*~")}, /* F28 */
        {TERMCAP2KEY('F', 'J'), IF_EB("\033[50;*~", ESC_STR "[50;*~")}, /* F29 */
        {TERMCAP2KEY('F', 'K'), IF_EB("\033[51;*~", ESC_STR "[51;*~")}, /* F30 */

        {TERMCAP2KEY('F', 'L'), IF_EB("\033[52;*~", ESC_STR "[52;*~")}, /* F31 */
        {TERMCAP2KEY('F', 'M'), IF_EB("\033[53;*~", ESC_STR "[53;*~")}, /* F32 */
        {TERMCAP2KEY('F', 'N'), IF_EB("\033[54;*~", ESC_STR "[54;*~")}, /* F33 */
        {TERMCAP2KEY('F', 'O'), IF_EB("\033[55;*~", ESC_STR "[55;*~")}, /* F34 */
        {TERMCAP2KEY('F', 'P'), IF_EB("\033[56;*~", ESC_STR "[56;*~")}, /* F35 */
        {TERMCAP2KEY('F', 'Q'), IF_EB("\033[57;*~", ESC_STR "[57;*~")}, /* F36 */
        {TERMCAP2KEY('F', 'R'), IF_EB("\033[58;*~", ESC_STR "[58;*~")}, /* F37 */
#endif

#if defined(UNIX) || defined(ALL_BUILTIN_TCAPS)
        /*
 * iris-ansi for Silicon Graphics machines.
 */
        {(int)KS_NAME, "iris-ansi"},
        {(int)KS_CE, "\033[K"},
        {(int)KS_CD, "\033[J"},
        {(int)KS_AL, "\033[L"},
#ifdef TERMINFO
        {(int)KS_CAL, "\033[%p1%dL"},
#else
        {(int)KS_CAL, "\033[%dL"},
#endif
        {(int)KS_DL, "\033[M"},
#ifdef TERMINFO
        {(int)KS_CDL, "\033[%p1%dM"},
#else
        {(int)KS_CDL, "\033[%dM"},
#endif
#if 0 /* The scroll region is not working as Vim expects. */
#ifdef TERMINFO
    {(int)KS_CS,	"\033[%i%p1%d;%p2%dr"},
#else
    {(int)KS_CS,	"\033[%i%d;%dr"},
#endif
#endif
        {(int)KS_CL, "\033[H\033[2J"},
        {(int)KS_VE, "\033[9/y\033[12/y"},         /* These aren't documented */
        {(int)KS_VS, "\033[10/y\033[=1h\033[=2l"}, /* These aren't documented */
        {(int)KS_TI, "\033[=6h"},
        {(int)KS_TE, "\033[=6l"},
        {(int)KS_SE, "\033[21;27m"},
        {(int)KS_SO, "\033[1;7m"},
        {(int)KS_ME, "\033[m"},
        {(int)KS_MR, "\033[7m"},
        {(int)KS_MD, "\033[1m"},
        {(int)KS_CCO, "8"},        /* allow 8 colors */
        {(int)KS_CZH, "\033[3m"},  /* italic mode on */
        {(int)KS_CZR, "\033[23m"}, /* italic mode off */
        {(int)KS_US, "\033[4m"},   /* underline on */
        {(int)KS_UE, "\033[24m"},  /* underline off */
#ifdef TERMINFO
        {(int)KS_CAB, "\033[4%p1%dm"},    /* set background color (ANSI) */
        {(int)KS_CAF, "\033[3%p1%dm"},    /* set foreground color (ANSI) */
        {(int)KS_CSB, "\033[102;%p1%dm"}, /* set screen background color */
        {(int)KS_CSF, "\033[101;%p1%dm"}, /* set screen foreground color */
#else
        {(int)KS_CAB, "\033[4%dm"},    /* set background color (ANSI) */
        {(int)KS_CAF, "\033[3%dm"},    /* set foreground color (ANSI) */
        {(int)KS_CSB, "\033[102;%dm"}, /* set screen background color */
        {(int)KS_CSF, "\033[101;%dm"}, /* set screen foreground color */
#endif
        {(int)KS_MS, "y"}, /* guessed */
        {(int)KS_UT, "y"}, /* guessed */
        {(int)KS_LE, "\b"},
#ifdef TERMINFO
        {(int)KS_CM, "\033[%i%p1%d;%p2%dH"},
#else
        {(int)KS_CM, "\033[%i%d;%dH"},
#endif
        {(int)KS_SR, "\033M"},
#ifdef TERMINFO
        {(int)KS_CRI, "\033[%p1%dC"},
#else
        {(int)KS_CRI, "\033[%dC"},
#endif
        {(int)KS_CIS, "\033P3.y"},
        {(int)KS_CIE, "\234"}, /* ST "String Terminator" */
        {(int)KS_TS, "\033P1.y"},
        {(int)KS_FS, "\234"}, /* ST "String Terminator" */
#ifdef TERMINFO
        {(int)KS_CWS, "\033[203;%p1%d;%p2%d/y"},
        {(int)KS_CWP, "\033[205;%p1%d;%p2%d/y"},
#else
        {(int)KS_CWS, "\033[203;%d;%d/y"},
        {(int)KS_CWP, "\033[205;%d;%d/y"},
#endif
        {K_UP, "\033[A"},
        {K_DOWN, "\033[B"},
        {K_LEFT, "\033[D"},
        {K_RIGHT, "\033[C"},
        {K_S_UP, "\033[161q"},
        {K_S_DOWN, "\033[164q"},
        {K_S_LEFT, "\033[158q"},
        {K_S_RIGHT, "\033[167q"},
        {K_F1, "\033[001q"},
        {K_F2, "\033[002q"},
        {K_F3, "\033[003q"},
        {K_F4, "\033[004q"},
        {K_F5, "\033[005q"},
        {K_F6, "\033[006q"},
        {K_F7, "\033[007q"},
        {K_F8, "\033[008q"},
        {K_F9, "\033[009q"},
        {K_F10, "\033[010q"},
        {K_F11, "\033[011q"},
        {K_F12, "\033[012q"},
        {K_S_F1, "\033[013q"},
        {K_S_F2, "\033[014q"},
        {K_S_F3, "\033[015q"},
        {K_S_F4, "\033[016q"},
        {K_S_F5, "\033[017q"},
        {K_S_F6, "\033[018q"},
        {K_S_F7, "\033[019q"},
        {K_S_F8, "\033[020q"},
        {K_S_F9, "\033[021q"},
        {K_S_F10, "\033[022q"},
        {K_S_F11, "\033[023q"},
        {K_S_F12, "\033[024q"},
        {K_INS, "\033[139q"},
        {K_HOME, "\033[H"},
        {K_END, "\033[146q"},
        {K_PAGEUP, "\033[150q"},
        {K_PAGEDOWN, "\033[154q"},
#endif

#if defined(DEBUG) || defined(ALL_BUILTIN_TCAPS)
        /*
 * for debugging
 */
        {(int)KS_NAME, "debug"},
        {(int)KS_CE, "[CE]"},
        {(int)KS_CD, "[CD]"},
        {(int)KS_AL, "[AL]"},
#ifdef TERMINFO
        {(int)KS_CAL, "[CAL%p1%d]"},
#else
        {(int)KS_CAL, "[CAL%d]"},
#endif
        {(int)KS_DL, "[DL]"},
#ifdef TERMINFO
        {(int)KS_CDL, "[CDL%p1%d]"},
#else
        {(int)KS_CDL, "[CDL%d]"},
#endif
#ifdef TERMINFO
        {(int)KS_CS, "[%p1%dCS%p2%d]"},
#else
        {(int)KS_CS, "[%dCS%d]"},
#endif
#ifdef TERMINFO
        {(int)KS_CSV, "[%p1%dCSV%p2%d]"},
#else
        {(int)KS_CSV, "[%dCSV%d]"},
#endif
#ifdef TERMINFO
        {(int)KS_CAB, "[CAB%p1%d]"},
        {(int)KS_CAF, "[CAF%p1%d]"},
        {(int)KS_CSB, "[CSB%p1%d]"},
        {(int)KS_CSF, "[CSF%p1%d]"},
#else
        {(int)KS_CAB, "[CAB%d]"},
        {(int)KS_CAF, "[CAF%d]"},
        {(int)KS_CSB, "[CSB%d]"},
        {(int)KS_CSF, "[CSF%d]"},
#endif
        {(int)KS_OP, "[OP]"},
        {(int)KS_LE, "[LE]"},
        {(int)KS_CL, "[CL]"},
        {(int)KS_VI, "[VI]"},
        {(int)KS_VE, "[VE]"},
        {(int)KS_VS, "[VS]"},
        {(int)KS_ME, "[ME]"},
        {(int)KS_MR, "[MR]"},
        {(int)KS_MB, "[MB]"},
        {(int)KS_MD, "[MD]"},
        {(int)KS_SE, "[SE]"},
        {(int)KS_SO, "[SO]"},
        {(int)KS_UE, "[UE]"},
        {(int)KS_US, "[US]"},
        {(int)KS_UCE, "[UCE]"},
        {(int)KS_UCS, "[UCS]"},
        {(int)KS_STE, "[STE]"},
        {(int)KS_STS, "[STS]"},
        {(int)KS_MS, "[MS]"},
        {(int)KS_UT, "[UT]"},
        {(int)KS_XN, "[XN]"},
#ifdef TERMINFO
        {(int)KS_CM, "[%p1%dCM%p2%d]"},
#else
        {(int)KS_CM, "[%dCM%d]"},
#endif
        {(int)KS_SR, "[SR]"},
#ifdef TERMINFO
        {(int)KS_CRI, "[CRI%p1%d]"},
#else
        {(int)KS_CRI, "[CRI%d]"},
#endif
        {(int)KS_VB, "[VB]"},
        {(int)KS_KS, "[KS]"},
        {(int)KS_KE, "[KE]"},
        {(int)KS_TI, "[TI]"},
        {(int)KS_TE, "[TE]"},
        {(int)KS_CIS, "[CIS]"},
        {(int)KS_CIE, "[CIE]"},
        {(int)KS_CSC, "[CSC]"},
        {(int)KS_CEC, "[CEC]"},
        {(int)KS_TS, "[TS]"},
        {(int)KS_FS, "[FS]"},
#ifdef TERMINFO
        {(int)KS_CWS, "[%p1%dCWS%p2%d]"},
        {(int)KS_CWP, "[%p1%dCWP%p2%d]"},
#else
        {(int)KS_CWS, "[%dCWS%d]"},
        {(int)KS_CWP, "[%dCWP%d]"},
#endif
        {(int)KS_CRV, "[CRV]"},
        {(int)KS_U7, "[U7]"},
        {(int)KS_RFG, "[RFG]"},
        {(int)KS_RBG, "[RBG]"},
        {K_UP, "[KU]"},
        {K_DOWN, "[KD]"},
        {K_LEFT, "[KL]"},
        {K_RIGHT, "[KR]"},
        {K_XUP, "[xKU]"},
        {K_XDOWN, "[xKD]"},
        {K_XLEFT, "[xKL]"},
        {K_XRIGHT, "[xKR]"},
        {K_S_UP, "[S-KU]"},
        {K_S_DOWN, "[S-KD]"},
        {K_S_LEFT, "[S-KL]"},
        {K_C_LEFT, "[C-KL]"},
        {K_S_RIGHT, "[S-KR]"},
        {K_C_RIGHT, "[C-KR]"},
        {K_F1, "[F1]"},
        {K_XF1, "[xF1]"},
        {K_F2, "[F2]"},
        {K_XF2, "[xF2]"},
        {K_F3, "[F3]"},
        {K_XF3, "[xF3]"},
        {K_F4, "[F4]"},
        {K_XF4, "[xF4]"},
        {K_F5, "[F5]"},
        {K_F6, "[F6]"},
        {K_F7, "[F7]"},
        {K_F8, "[F8]"},
        {K_F9, "[F9]"},
        {K_F10, "[F10]"},
        {K_F11, "[F11]"},
        {K_F12, "[F12]"},
        {K_S_F1, "[S-F1]"},
        {K_S_XF1, "[S-xF1]"},
        {K_S_F2, "[S-F2]"},
        {K_S_XF2, "[S-xF2]"},
        {K_S_F3, "[S-F3]"},
        {K_S_XF3, "[S-xF3]"},
        {K_S_F4, "[S-F4]"},
        {K_S_XF4, "[S-xF4]"},
        {K_S_F5, "[S-F5]"},
        {K_S_F6, "[S-F6]"},
        {K_S_F7, "[S-F7]"},
        {K_S_F8, "[S-F8]"},
        {K_S_F9, "[S-F9]"},
        {K_S_F10, "[S-F10]"},
        {K_S_F11, "[S-F11]"},
        {K_S_F12, "[S-F12]"},
        {K_HELP, "[HELP]"},
        {K_UNDO, "[UNDO]"},
        {K_BS, "[BS]"},
        {K_INS, "[INS]"},
        {K_KINS, "[KINS]"},
        {K_DEL, "[DEL]"},
        {K_KDEL, "[KDEL]"},
        {K_HOME, "[HOME]"},
        {K_S_HOME, "[C-HOME]"},
        {K_C_HOME, "[C-HOME]"},
        {K_KHOME, "[KHOME]"},
        {K_XHOME, "[XHOME]"},
        {K_ZHOME, "[ZHOME]"},
        {K_END, "[END]"},
        {K_S_END, "[C-END]"},
        {K_C_END, "[C-END]"},
        {K_KEND, "[KEND]"},
        {K_XEND, "[XEND]"},
        {K_ZEND, "[ZEND]"},
        {K_PAGEUP, "[PAGEUP]"},
        {K_PAGEDOWN, "[PAGEDOWN]"},
        {K_KPAGEUP, "[KPAGEUP]"},
        {K_KPAGEDOWN, "[KPAGEDOWN]"},
        {K_MOUSE, "[MOUSE]"},
        {K_KPLUS, "[KPLUS]"},
        {K_KMINUS, "[KMINUS]"},
        {K_KDIVIDE, "[KDIVIDE]"},
        {K_KMULTIPLY, "[KMULTIPLY]"},
        {K_KENTER, "[KENTER]"},
        {K_KPOINT, "[KPOINT]"},
        {K_PS, "[PASTE-START]"},
        {K_PE, "[PASTE-END]"},
        {K_K0, "[K0]"},
        {K_K1, "[K1]"},
        {K_K2, "[K2]"},
        {K_K3, "[K3]"},
        {K_K4, "[K4]"},
        {K_K5, "[K5]"},
        {K_K6, "[K6]"},
        {K_K7, "[K7]"},
        {K_K8, "[K8]"},
        {K_K9, "[K9]"},
#endif

#endif /* NO_BUILTIN_TCAPS */

        /*
 * The most minimal terminal: only clear screen and cursor positioning
 * Always included.
 */
        {(int)KS_NAME, "dumb"},
        {(int)KS_CL, "\014"},
#ifdef TERMINFO
        {(int)KS_CM, IF_EB("\033[%i%p1%d;%p2%dH",
                           ESC_STR "[%i%p1%d;%p2%dH")},
#else
        {(int)KS_CM, IF_EB("\033[%i%d;%dH", ESC_STR "[%i%d;%dH")},
#endif

        /*
 * end marker
 */
        {(int)KS_NAME, NULL}

}; /* end of builtin_termcaps */

/*
 * DEFAULT_TERM is used, when no terminal is specified with -T option or $TERM.
 */
#ifdef MSWIN
#define DEFAULT_TERM (char_u *)"win32"
#endif

#if defined(UNIX) && !defined(__MINT__)
#define DEFAULT_TERM (char_u *)"ansi"
#endif

#ifdef __MINT__
#define DEFAULT_TERM (char_u *)"vt52"
#endif

#ifdef VMS
#define DEFAULT_TERM (char_u *)"vt320"
#endif

#ifdef __BEOS__
#undef DEFAULT_TERM
#define DEFAULT_TERM (char_u *)"beos-ansi"
#endif

#ifndef DEFAULT_TERM
#define DEFAULT_TERM (char_u *)"dumb"
#endif

/*
 * Term_strings contains currently used terminal output strings.
 * It is initialized with the default values by parse_builtin_tcap().
 * The values can be changed by setting the option with the same name.
 */
char_u *(term_strings[(int)KS_LAST + 1]);

static int need_gather = FALSE;    /* need to fill termleader[] */
static char_u termleader[256 + 1]; /* for check_termcode() */

static struct builtin_term *
find_builtin_term(char_u *term)
{
  struct builtin_term *p;

  p = builtin_termcaps;
  while (p->bt_string != NULL)
  {
    if (p->bt_entry == (int)KS_NAME)
    {
#ifdef UNIX
      if (STRCMP(p->bt_string, "iris-ansi") == 0 && vim_is_iris(term))
        return p;
      else if (STRCMP(p->bt_string, "xterm") == 0 && vim_is_xterm(term))
        return p;
      else
#endif
#ifdef VMS
          if (STRCMP(p->bt_string, "vt320") == 0 && vim_is_vt300(term))
        return p;
      else
#endif
          if (STRCMP(term, p->bt_string) == 0)
        return p;
    }
    ++p;
  }
  return p;
}

/*
 * Parsing of the builtin termcap entries.
 * Caller should check if 'name' is a valid builtin term.
 * The terminal's name is not set, as this is already done in termcapinit().
 */
static void
parse_builtin_tcap(char_u *term)
{
  struct builtin_term *p;
  char_u name[2];
  int term_8bit;

  p = find_builtin_term(term);
  term_8bit = term_is_8bit(term);

  /* Do not parse if builtin term not found */
  if (p->bt_string == NULL)
    return;

  for (++p; p->bt_entry != (int)KS_NAME && p->bt_entry != BT_EXTRA_KEYS; ++p)
  {
    if ((int)p->bt_entry >= 0) /* KS_xx entry */
    {
      /* Only set the value if it wasn't set yet. */
      if (term_strings[p->bt_entry] == NULL || term_strings[p->bt_entry] == empty_option)
      {
#ifdef FEAT_EVAL
        int opt_idx = -1;
#endif
        /* 8bit terminal: use CSI instead of <Esc>[ */
        if (term_8bit && term_7to8bit((char_u *)p->bt_string) != 0)
        {
          char_u *s, *t;

          s = vim_strsave((char_u *)p->bt_string);
          if (s != NULL)
          {
            for (t = s; *t; ++t)
              if (term_7to8bit(t))
              {
                *t = term_7to8bit(t);
                STRMOVE(t + 1, t + 2);
              }
            term_strings[p->bt_entry] = s;
#ifdef FEAT_EVAL
            opt_idx =
#endif
                set_term_option_alloced(
                    &term_strings[p->bt_entry]);
          }
        }
        else
        {
          term_strings[p->bt_entry] = (char_u *)p->bt_string;
#ifdef FEAT_EVAL
          opt_idx = get_term_opt_idx(&term_strings[p->bt_entry]);
#endif
        }
#ifdef FEAT_EVAL
        set_term_option_sctx_idx(NULL, opt_idx);
#endif
      }
    }
    else
    {
      name[0] = KEY2TERMCAP0((int)p->bt_entry);
      name[1] = KEY2TERMCAP1((int)p->bt_entry);
      if (find_termcode(name) == NULL)
        add_termcode(name, (char_u *)p->bt_string, term_8bit);
    }
  }
}

/*
 * Set number of colors.
 * Store it as a number in t_colors.
 * Store it as a string in T_CCO (using nr_colors[]).
 */
static void
set_color_count(int nr)
{
  char_u nr_colors[20]; /* string for number of colors */

  t_colors = nr;
  if (t_colors > 1)
    sprintf((char *)nr_colors, "%d", t_colors);
  else
    *nr_colors = NUL;
  set_string_option_direct((char_u *)"t_Co", -1, nr_colors, OPT_FREE, 0);
}

#ifdef HAVE_TGETENT
static char *(key_names[]) =
    {
        "ku", "kd", "kr", "kl",
        "#2", "#4", "%i", "*7",
        "k1", "k2", "k3", "k4", "k5", "k6",
        "k7", "k8", "k9", "k;", "F1", "F2",
        "%1", "&8", "kb", "kI", "kD", "kh",
        "@7", "kP", "kN", "K1", "K3", "K4", "K5", "kB",
        NULL};
#endif

#ifdef HAVE_TGETENT
static void
get_term_entries(int *height, int *width)
{
  static struct
  {
    enum SpecialKey dest; /* index in term_strings[] */
    char *name;           /* termcap name for string */
  } string_names[] =
      {{KS_CE, "ce"}, {KS_AL, "al"}, {KS_CAL, "AL"}, {KS_DL, "dl"}, {KS_CDL, "DL"}, {KS_CS, "cs"}, {KS_CL, "cl"}, {KS_CD, "cd"}, {KS_VI, "vi"}, {KS_VE, "ve"}, {KS_MB, "mb"}, {KS_ME, "me"}, {KS_MR, "mr"}, {KS_MD, "md"}, {KS_SE, "se"}, {KS_SO, "so"}, {KS_CZH, "ZH"}, {KS_CZR, "ZR"}, {KS_UE, "ue"}, {KS_US, "us"}, {KS_UCE, "Ce"}, {KS_UCS, "Cs"}, {KS_STE, "Te"}, {KS_STS, "Ts"}, {KS_CM, "cm"}, {KS_SR, "sr"}, {KS_CRI, "RI"}, {KS_VB, "vb"}, {KS_KS, "ks"}, {KS_KE, "ke"}, {KS_TI, "ti"}, {KS_TE, "te"}, {KS_BC, "bc"}, {KS_CSB, "Sb"}, {KS_CSF, "Sf"}, {KS_CAB, "AB"}, {KS_CAF, "AF"}, {KS_LE, "le"}, {KS_ND, "nd"}, {KS_OP, "op"}, {KS_CRV, "RV"}, {KS_VS, "vs"}, {KS_CVS, "VS"}, {KS_CIS, "IS"}, {KS_CIE, "IE"}, {KS_CSC, "SC"}, {KS_CEC, "EC"}, {KS_TS, "ts"}, {KS_FS, "fs"}, {KS_CWP, "WP"}, {KS_CWS, "WS"}, {KS_CSI, "SI"}, {KS_CEI, "EI"}, {KS_U7, "u7"}, {KS_RFG, "RF"}, {KS_RBG, "RB"}, {KS_8F, "8f"}, {KS_8B, "8b"}, {KS_CBE, "BE"}, {KS_CBD, "BD"}, {KS_CPS, "PS"}, {KS_CPE, "PE"}, {KS_CST, "ST"}, {KS_CRT, "RT"}, {KS_SSI, "Si"}, {KS_SRI, "Ri"}, {(enum SpecialKey)0, NULL}};
  int i;
  char_u *p;
  static char_u tstrbuf[TBUFSZ];
  char_u *tp = tstrbuf;

  /*
     * get output strings
     */
  for (i = 0; string_names[i].name != NULL; ++i)
  {
    if (TERM_STR(string_names[i].dest) == NULL || TERM_STR(string_names[i].dest) == empty_option)
    {
      TERM_STR(string_names[i].dest) = TGETSTR(string_names[i].name, &tp);
#ifdef FEAT_EVAL
      set_term_option_sctx_idx(string_names[i].name, -1);
#endif
    }
  }

  /* tgetflag() returns 1 if the flag is present, 0 if not and
     * possibly -1 if the flag doesn't exist. */
  if ((T_MS == NULL || T_MS == empty_option) && tgetflag("ms") > 0)
    T_MS = (char_u *)"y";
  if ((T_XS == NULL || T_XS == empty_option) && tgetflag("xs") > 0)
    T_XS = (char_u *)"y";
  if ((T_XN == NULL || T_XN == empty_option) && tgetflag("xn") > 0)
    T_XN = (char_u *)"y";
  if ((T_DB == NULL || T_DB == empty_option) && tgetflag("db") > 0)
    T_DB = (char_u *)"y";
  if ((T_DA == NULL || T_DA == empty_option) && tgetflag("da") > 0)
    T_DA = (char_u *)"y";
  if ((T_UT == NULL || T_UT == empty_option) && tgetflag("ut") > 0)
    T_UT = (char_u *)"y";

  /*
     * get key codes
     */
  for (i = 0; key_names[i] != NULL; ++i)
    if (find_termcode((char_u *)key_names[i]) == NULL)
    {
      p = TGETSTR(key_names[i], &tp);
      /* if cursor-left == backspace, ignore it (televideo 925) */
      if (p != NULL && (*p != Ctrl_H || key_names[i][0] != 'k' || key_names[i][1] != 'l'))
        add_termcode((char_u *)key_names[i], p, FALSE);
    }

  if (*height == 0)
    *height = tgetnum("li");
  if (*width == 0)
    *width = tgetnum("co");

  /*
     * Get number of colors (if not done already).
     */
  if (TERM_STR(KS_CCO) == NULL || TERM_STR(KS_CCO) == empty_option)
  {
    set_color_count(tgetnum("Co"));
#ifdef FEAT_EVAL
    set_term_option_sctx_idx("Co", -1);
#endif
  }

#ifndef hpux
  BC = (char *)TGETSTR("bc", &tp);
  UP = (char *)TGETSTR("up", &tp);
  p = TGETSTR("pc", &tp);
  if (p)
    PC = *p;
#endif
}
#endif

static void
report_term_error(char *error_msg, char_u *term)
{
  struct builtin_term *termp;

  mch_errmsg("\r\n");
  if (error_msg != NULL)
  {
    mch_errmsg(error_msg);
    mch_errmsg("\r\n");
  }
  mch_errmsg("'");
  mch_errmsg((char *)term);
  mch_errmsg(_("' not known. Available builtin terminals are:"));
  mch_errmsg("\r\n");
  for (termp = &(builtin_termcaps[0]); termp->bt_string != NULL; ++termp)
  {
    if (termp->bt_entry == (int)KS_NAME)
    {
#ifdef HAVE_TGETENT
      mch_errmsg("    builtin_");
#else
      mch_errmsg("    ");
#endif
      mch_errmsg(termp->bt_string);
      mch_errmsg("\r\n");
    }
  }
}

static void
report_default_term(char_u *term)
{
  mch_errmsg(_("defaulting to '"));
  mch_errmsg((char *)term);
  mch_errmsg("'\r\n");
  if (emsg_silent == 0)
  {
    screen_start(); /* don't know where cursor is now */
  }
}

/*
 * Set terminal options for terminal "term".
 * Return OK if terminal 'term' was found in a termcap, FAIL otherwise.
 *
 * While doing this, until ttest(), some options may be NULL, be careful.
 */
int set_termname(char_u *term)
{
  struct builtin_term *termp;
#ifdef HAVE_TGETENT
  int builtin_first = p_tbi;
  int
  try
    ;
  int termcap_cleared = FALSE;
#endif
  int width = 0, height = 0;
  char *error_msg = NULL;
  char_u *bs_p, *del_p;

  /* In silect mode (ex -s) we don't use the 'term' option. */
  if (silent_mode)
    return OK;

  detected_8bit = FALSE; /* reset 8-bit detection */

  if (term_is_builtin(term))
  {
    term += 8;
#ifdef HAVE_TGETENT
    builtin_first = 1;
#endif
  }

/*
 * If HAVE_TGETENT is not defined, only the builtin termcap is used, otherwise:
 *   If builtin_first is TRUE:
 *     0. try builtin termcap
 *     1. try external termcap
 *     2. if both fail default to a builtin terminal
 *   If builtin_first is FALSE:
 *     1. try external termcap
 *     2. try builtin termcap, if both fail default to a builtin terminal
 */
#ifdef HAVE_TGETENT
  for (try = builtin_first ? 0 : 1; try < 3; ++try)
  {
    /*
	 * Use external termcap
	 */
    if (try == 1)
    {
      char_u tbuf[TBUFSZ];

      /*
	     * If the external termcap does not have a matching entry, try the
	     * builtin ones.
	     */
      if ((error_msg = tgetent_error(tbuf, term)) == NULL)
      {
        if (!termcap_cleared)
        {
          clear_termoptions(); /* clear old options */
          termcap_cleared = TRUE;
        }

        get_term_entries(&height, &width);
      }
    }
    else /* try == 0 || try == 2 */
#endif   /* HAVE_TGETENT */
    /*
	 * Use builtin termcap
	 */
    {
#ifdef HAVE_TGETENT
      /*
	     * If builtin termcap was already used, there is no need to search
	     * for the builtin termcap again, quit now.
	     */
      if (try == 2 && builtin_first && termcap_cleared)
        break;
#endif
      /*
	     * search for 'term' in builtin_termcaps[]
	     */
      termp = find_builtin_term(term);
      if (termp->bt_string == NULL) /* did not find it */
      {
#ifdef HAVE_TGETENT
        /*
		 * If try == 0, first try the external termcap. If that is not
		 * found we'll get back here with try == 2.
		 * If termcap_cleared is set we used the external termcap,
		 * don't complain about not finding the term in the builtin
		 * termcap.
		 */
        if (try == 0) /* try external one */
          continue;
        if (termcap_cleared) /* found in external termcap */
          break;
#endif
        report_term_error(error_msg, term);

        /* when user typed :set term=xxx, quit here */
        if (starting != NO_SCREEN)
        {
          screen_start(); /* don't know where cursor is now */
          wait_return(TRUE);
          return FAIL;
        }
        term = DEFAULT_TERM;
        report_default_term(term);
        set_string_option_direct((char_u *)"term", -1, term,
                                 OPT_FREE, 0);
      }
#ifdef HAVE_TGETENT
      if (!termcap_cleared)
      {
#endif
        clear_termoptions(); /* clear old options */
#ifdef HAVE_TGETENT
        termcap_cleared = TRUE;
      }
#endif
      parse_builtin_tcap(term);
    }
#ifdef HAVE_TGETENT
  }
#endif

  /*
 * special: There is no info in the termcap about whether the cursor
 * positioning is relative to the start of the screen or to the start of the
 * scrolling region.  We just guess here. Only msdos pcterm is known to do it
 * relative.
 */
  if (STRCMP(term, "pcterm") == 0)
    T_CCS = (char_u *)"yes";
  else
    T_CCS = empty_option;

#ifdef UNIX
  /*
 * Any "stty" settings override the default for t_kb from the termcap.
 * This is in os_unix.c, because it depends a lot on the version of unix that
 * is being used.
 * Don't do this when the GUI is active, it uses "t_kb" and "t_kD" directly.
 */
  get_stty();
#endif

  /*
 * If the termcap has no entry for 'bs' and/or 'del' and the ioctl() also
 * didn't work, use the default CTRL-H
 * The default for t_kD is DEL, unless t_kb is DEL.
 * The vim_strsave'd strings are probably lost forever, well it's only two
 * bytes.  Don't do this when the GUI is active, it uses "t_kb" and "t_kD"
 * directly.
 */
  {
    bs_p = find_termcode((char_u *)"kb");
    del_p = find_termcode((char_u *)"kD");
    if (bs_p == NULL || *bs_p == NUL)
      add_termcode((char_u *)"kb", (bs_p = (char_u *)CTRL_H_STR), FALSE);
    if ((del_p == NULL || *del_p == NUL) &&
        (bs_p == NULL || *bs_p != DEL))
      add_termcode((char_u *)"kD", (char_u *)DEL_STR, FALSE);
  }

#if defined(UNIX) || defined(VMS)
  term_is_xterm = vim_is_xterm(term);
#endif

#ifdef USE_TERM_CONSOLE
  /* DEFAULT_TERM indicates that it is the machine console. */
  if (STRCMP(term, DEFAULT_TERM) != 0)
    term_console = FALSE;
  else
  {
    term_console = TRUE;
  }
#endif

#if defined(UNIX) || defined(VMS)
  /*
     * 'ttyfast' is default on for xterm, iris-ansi and a few others.
     */
  if (vim_is_fastterm(term))
    p_tf = TRUE;
#endif
#ifdef USE_TERM_CONSOLE
  /*
     * 'ttyfast' is default on consoles
     */
  if (term_console)
    p_tf = TRUE;
#endif

  ttest(TRUE); /* make sure we have a valid set of terminal codes */

  full_screen = TRUE;  /* we can use termcap codes from now on */
  set_term_defaults(); /* use current values as defaults */

  /*
     * Initialize the terminal with the appropriate termcap codes.
     * Set the mouse and window title if possible.
     * Don't do this when starting, need to parse the .vimrc first, because it
     * may redefine t_TI etc.
     */
  if (starting != NO_SCREEN)
  {
    starttermcap(); /* may change terminal mode */
  }

  /* display initial screen after ttest() checking. jw. */
  if (width <= 0 || height <= 0)
  {
    /* termcap failed to report size */
    /* set defaults, in case ui_get_shellsize() also fails */
    width = 80;
#if defined(MSWIN)
    height = 25; /* console is often 25 lines */
#else
    height = 24; /* most terminals are 24 lines */
#endif
  }
  set_shellsize(width, height, FALSE); /* may change Rows */
  if (starting != NO_SCREEN)
  {
    if (scroll_region)
      scroll_region_reset(); /* In case Rows changed */
    check_map_keycodes();    /* check mappings for terminal codes used */

    {
      bufref_T old_curbuf;

      /*
	     * Execute the TermChanged autocommands for each buffer that is
	     * loaded.
	     */
      set_bufref(&old_curbuf, curbuf);
      FOR_ALL_BUFFERS(curbuf)
      {
        if (curbuf->b_ml.ml_mfp != NULL)
          apply_autocmds(EVENT_TERMCHANGED, NULL, NULL, FALSE,
                         curbuf);
      }
      if (bufref_valid(&old_curbuf))
        curbuf = old_curbuf.br_buf;
    }
  }

  return OK;
}

#ifdef HAVE_TGETENT
/*
 * Call tgetent()
 * Return error message if it fails, NULL if it's OK.
 */
static char *
tgetent_error(char_u *tbuf, char_u *term)
{
  int i;

  i = TGETENT(tbuf, term);
  if (i < 0 /* -1 is always an error */
#ifdef TGETENT_ZERO_ERR
      || i == 0 /* sometimes zero is also an error */
#endif
  )
  {
    /* On FreeBSD tputs() gets a SEGV after a tgetent() which fails.  Call
	 * tgetent() with the always existing "dumb" entry to avoid a crash or
	 * hang. */
    (void)TGETENT(tbuf, "dumb");

    if (i < 0)
#ifdef TGETENT_ZERO_ERR
      return _("E557: Cannot open termcap file");
    if (i == 0)
#endif
#ifdef TERMINFO
      return _("E558: Terminal entry not found in terminfo");
#else
      return _("E559: Terminal entry not found in termcap");
#endif
  }
  return NULL;
}

/*
 * Some versions of tgetstr() have been reported to return -1 instead of NULL.
 * Fix that here.
 */
static char_u *
vim_tgetstr(char *s, char_u **pp)
{
  char *p;

  p = tgetstr(s, (char **)pp);
  if (p == (char *)-1)
    p = NULL;
  return (char_u *)p;
}
#endif /* HAVE_TGETENT */

#if defined(HAVE_TGETENT) && (defined(UNIX) || defined(VMS) || defined(MACOS_X))
/*
 * Get Columns and Rows from the termcap. Used after a window signal if the
 * ioctl() fails. It doesn't make sense to call tgetent each time if the "co"
 * and "li" entries never change. But on some systems this works.
 * Errors while getting the entries are ignored.
 */
void getlinecol(
    long *cp, /* pointer to columns */
    long *rp) /* pointer to rows */
{
  char_u tbuf[TBUFSZ];

  if (T_NAME != NULL && *T_NAME != NUL && tgetent_error(tbuf, T_NAME) == NULL)
  {
    if (*cp == 0)
      *cp = tgetnum("co");
    if (*rp == 0)
      *rp = tgetnum("li");
  }
}
#endif /* defined(HAVE_TGETENT) && defined(UNIX) */

/*
 * Get a string entry from the termcap and add it to the list of termcodes.
 * Used for <t_xx> special keys.
 * Give an error message for failure when not sourcing.
 * If force given, replace an existing entry.
 * Return FAIL if the entry was not found, OK if the entry was added.
 */
int add_termcap_entry(char_u *name, int force)
{
  char_u *term;
  int key;
  struct builtin_term *termp;
#ifdef HAVE_TGETENT
  char_u *string;
  int i;
  int builtin_first;
  char_u tbuf[TBUFSZ];
  char_u tstrbuf[TBUFSZ];
  char_u *tp = tstrbuf;
  char *error_msg = NULL;
#endif

  if (!force && find_termcode(name) != NULL) /* it's already there */
    return OK;

  term = T_NAME;
  if (term == NULL || *term == NUL) /* 'term' not defined yet */
    return FAIL;

  if (term_is_builtin(term)) /* name starts with "builtin_" */
  {
    term += 8;
#ifdef HAVE_TGETENT
    builtin_first = TRUE;
#endif
  }
#ifdef HAVE_TGETENT
  else
    builtin_first = p_tbi;
#endif

#ifdef HAVE_TGETENT
  /*
 * We can get the entry from the builtin termcap and from the external one.
 * If 'ttybuiltin' is on or the terminal name starts with "builtin_", try
 * builtin termcap first.
 * If 'ttybuiltin' is off, try external termcap first.
 */
  for (i = 0; i < 2; ++i)
  {
    if ((!builtin_first) == i)
#endif
    /*
	 * Search in builtin termcap
	 */
    {
      termp = find_builtin_term(term);
      if (termp->bt_string != NULL) /* found it */
      {
        key = TERMCAP2KEY(name[0], name[1]);
        ++termp;
        while (termp->bt_entry != (int)KS_NAME)
        {
          if ((int)termp->bt_entry == key)
          {
            add_termcode(name, (char_u *)termp->bt_string,
                         term_is_8bit(term));
            return OK;
          }
          ++termp;
        }
      }
    }
#ifdef HAVE_TGETENT
    else
    /*
	 * Search in external termcap
	 */
    {
      error_msg = tgetent_error(tbuf, term);
      if (error_msg == NULL)
      {
        string = TGETSTR((char *)name, &tp);
        if (string != NULL && *string != NUL)
        {
          add_termcode(name, string, FALSE);
          return OK;
        }
      }
    }
  }
#endif

  if (sourcing_name == NULL)
  {
#ifdef HAVE_TGETENT
    if (error_msg != NULL)
      emsg(error_msg);
    else
#endif
      semsg(_("E436: No \"%s\" entry in termcap"), name);
  }
  return FAIL;
}

static int
term_is_builtin(char_u *name)
{
  return (STRNCMP(name, "builtin_", (size_t)8) == 0);
}

/*
 * Return TRUE if terminal "name" uses CSI instead of <Esc>[.
 * Assume that the terminal is using 8-bit controls when the name contains
 * "8bit", like in "xterm-8bit".
 */
int term_is_8bit(char_u *name)
{
  return (detected_8bit || strstr((char *)name, "8bit") != NULL);
}

/*
 * Translate terminal control chars from 7-bit to 8-bit:
 * <Esc>[ -> CSI  <M_C_[>
 * <Esc>] -> OSC  <M-C-]>
 * <Esc>O -> <M-C-O>
 */
static int
term_7to8bit(char_u *p)
{
  if (*p == ESC)
  {
    if (p[1] == '[')
      return CSI;
    if (p[1] == ']')
      return OSC;
    if (p[1] == 'O')
      return 0x8f;
  }
  return 0;
}

#if defined(PROTO)
int term_is_gui(char_u *name)
{
  return (STRCMP(name, "builtin_gui") == 0 || STRCMP(name, "gui") == 0);
}
#endif

#if !defined(HAVE_TGETENT) || defined(PROTO)

char_u *
tltoa(unsigned long i)
{
  static char_u buf[16];
  char_u *p;

  p = buf + 15;
  *p = '\0';
  do
  {
    --p;
    *p = (char_u)(i % 10 + '0');
    i /= 10;
  } while (i > 0 && p > buf);
  return p;
}
#endif

#ifndef HAVE_TGETENT

/*
 * minimal tgoto() implementation.
 * no padding and we only parse for %i %d and %+char
 */
static char *
tgoto(char *cm, int x, int y)
{
  static char buf[30];
  char *p, *s, *e;

  if (!cm)
    return "OOPS";
  e = buf + 29;
  for (s = buf; s < e && *cm; cm++)
  {
    if (*cm != '%')
    {
      *s++ = *cm;
      continue;
    }
    switch (*++cm)
    {
    case 'd':
      p = (char *)tltoa((unsigned long)y);
      y = x;
      while (*p)
        *s++ = *p++;
      break;
    case 'i':
      x++;
      y++;
      break;
    case '+':
      *s++ = (char)(*++cm + y);
      y = x;
      break;
    case '%':
      *s++ = *cm;
      break;
    default:
      return "OOPS";
    }
  }
  *s = '\0';
  return buf;
}

#endif /* HAVE_TGETENT */

/*
 * Set the terminal name and initialize the terminal options.
 * If "name" is NULL or empty, get the terminal name from the environment.
 * If that fails, use the default terminal name.
 */
void termcapinit(char_u *name)
{
  char_u *term;

  if (name != NULL && *name == NUL)
    name = NULL; /* empty name is equal to no name */
  term = name;

#ifdef __BEOS__
  /*
     * TERM environment variable is normally set to 'ansi' on the Bebox;
     * Since the BeBox doesn't quite support full ANSI yet, we use our
     * own custom 'ansi-beos' termcap instead, unless the -T option has
     * been given on the command line.
     */
  if (term == NULL && strcmp((char *)mch_getenv((char_u *)"TERM"), "ansi") == 0)
    term = DEFAULT_TERM;
#endif
#ifndef MSWIN
  if (term == NULL)
    term = mch_getenv((char_u *)"TERM");
#endif
  if (term == NULL || *term == NUL)
    term = DEFAULT_TERM;
  set_string_option_direct((char_u *)"term", -1, term, OPT_FREE, 0);

  /* Set the default terminal name. */
  set_string_default("term", term);
  set_string_default("ttytype", term);

  /*
     * Avoid using "term" here, because the next mch_getenv() may overwrite it.
     */
  set_termname(T_NAME != NULL ? T_NAME : term);
}

/*
 * The number of calls to ui_write is reduced by using "out_buf".
 */
#define OUT_SIZE 2047

// Since the maximum number of SGR parameters shown as a normal value range is
// 16, the escape sequence length can be 4 * 16 + lead + tail.
#define MAX_ESC_SEQ_LEN 80

/*
 * A conditional-flushing out_str, mainly for visualbell.
 * Handles a delay internally, because termlib may not respect the delay or do
 * it at the wrong time.
 * Note: Only for terminal strings.
 */
void out_str_cf(char_u *s)
{
  /* No-op */
}
/*
 * out_str(s): Put a character string a byte at a time into the output buffer.
 * If HAVE_TGETENT is defined use the termcap parser. (jw)
 * This should only be used for writing terminal codes, not for outputting
 * normal text (use functions like msg_puts() and screen_putchar() for that).
 */
void out_str(char_u *s)
{
  /* No-op */
}

/*
 * cursor positioning using termcap parser. (jw)
 */
void term_windgoto(int row, int col)
{
  OUT_STR(tgoto((char *)T_CM, col, row));
}

void term_cursor_right(int i)
{
  OUT_STR(tgoto((char *)T_CRI, 0, i));
}

void term_append_lines(int line_count)
{
  OUT_STR(tgoto((char *)T_CAL, 0, line_count));
}

void term_delete_lines(int line_count)
{
  OUT_STR(tgoto((char *)T_CDL, 0, line_count));
}

#if defined(HAVE_TGETENT) || defined(PROTO)
void term_set_winpos(int x, int y)
{
  /* Can't handle a negative value here */
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  OUT_STR(tgoto((char *)T_CWP, y, x));
}

void term_set_winsize(int height, int width)
{
  OUT_STR(tgoto((char *)T_CWS, width, height));
}
#endif

static void
term_color(char_u *s, int n)
{
  char buf[20];
  int i = *s == CSI ? 1 : 2;
  /* index in s[] just after <Esc>[ or CSI */

  /* Special handling of 16 colors, because termcap can't handle it */
  /* Also accept "\e[3%dm" for TERMINFO, it is sometimes used */
  /* Also accept CSI instead of <Esc>[ */
  if (n >= 8 && t_colors >= 16 && ((s[0] == ESC && s[1] == '[') || (s[0] == CSI && (i = 1) == 1)) && s[i] != NUL && (STRCMP(s + i + 1, "%p1%dm") == 0 || STRCMP(s + i + 1, "%dm") == 0) && (s[i] == '3' || s[i] == '4'))
  {
#ifdef TERMINFO
    char *format = "%s%s%%p1%%dm";
#else
    char *format = "%s%s%%dm";
#endif
    char *lead = i == 2 ? (
                              IF_EB("\033[", ESC_STR "["))
                        : "\233";
    char *tail = s[i] == '3' ? (n >= 16 ? "38;5;" : "9")
                             : (n >= 16 ? "48;5;" : "10");

    sprintf(buf, format, lead, tail);
    OUT_STR(tgoto(buf, 0, n >= 16 ? n : n - 8));
  }
  else
    OUT_STR(tgoto((char *)s, 0, n));
}

void term_fg_color(int n)
{
  /* Use "AF" termcap entry if present, "Sf" entry otherwise */
  if (*T_CAF)
    term_color(T_CAF, n);
  else if (*T_CSF)
    term_color(T_CSF, n);
}

void term_bg_color(int n)
{
  /* Use "AB" termcap entry if present, "Sb" entry otherwise */
  if (*T_CAB)
    term_color(T_CAB, n);
  else if (*T_CSB)
    term_color(T_CSB, n);
}

#if (defined(VMS) || defined(MACOS_X)) || defined(PROTO)
/*
 * Generic function to set window title, using t_ts and t_fs.
 */
void term_settitle(char_u *title)
{
  /* libvim - no-op */
}

/*
 * Tell the terminal to push (save) the title and/or icon, so that it can be
 * popped (restored) later.
 */
void term_push_title(int which)
{
  /* libvim - no-op */
}

/*
 * Tell the terminal to pop the title and/or icon.
 */
void term_pop_title(int which)
{
  /* libvim - no-op */
}
#endif

/*
 * Make sure we have a valid set or terminal options.
 * Replace all entries that are NULL by empty_option
 */
void ttest(int pairs)
{
  char_u *env_colors;

  check_options(); /* make sure no options are NULL */

  /*
     * MUST have "cm": cursor motion.
     */
  if (*T_CM == NUL)
    emsg(_("E437: terminal capability \"cm\" required"));

  /*
     * if "cs" defined, use a scroll region, it's faster.
     */
  if (*T_CS != NUL)
    scroll_region = TRUE;
  else
    scroll_region = FALSE;

  if (pairs)
  {
    /*
	 * optional pairs
	 */
    /* TP goes to normal mode for TI (invert) and TB (bold) */
    if (*T_ME == NUL)
      T_ME = T_MR = T_MD = T_MB = empty_option;
    if (*T_SO == NUL || *T_SE == NUL)
      T_SO = T_SE = empty_option;
    if (*T_US == NUL || *T_UE == NUL)
      T_US = T_UE = empty_option;
    if (*T_CZH == NUL || *T_CZR == NUL)
      T_CZH = T_CZR = empty_option;

    /* T_VE is needed even though T_VI is not defined */
    if (*T_VE == NUL)
      T_VI = empty_option;

    /* if 'mr' or 'me' is not defined use 'so' and 'se' */
    if (*T_ME == NUL)
    {
      T_ME = T_SE;
      T_MR = T_SO;
      T_MD = T_SO;
    }

    /* if 'so' or 'se' is not defined use 'mr' and 'me' */
    if (*T_SO == NUL)
    {
      T_SE = T_ME;
      if (*T_MR == NUL)
        T_SO = T_MD;
      else
        T_SO = T_MR;
    }

    /* if 'ZH' or 'ZR' is not defined use 'mr' and 'me' */
    if (*T_CZH == NUL)
    {
      T_CZR = T_ME;
      if (*T_MR == NUL)
        T_CZH = T_MD;
      else
        T_CZH = T_MR;
    }

    /* "Sb" and "Sf" come in pairs */
    if (*T_CSB == NUL || *T_CSF == NUL)
    {
      T_CSB = empty_option;
      T_CSF = empty_option;
    }

    /* "AB" and "AF" come in pairs */
    if (*T_CAB == NUL || *T_CAF == NUL)
    {
      T_CAB = empty_option;
      T_CAF = empty_option;
    }

    /* if 'Sb' and 'AB' are not defined, reset "Co" */
    if (*T_CSB == NUL && *T_CAB == NUL)
      free_one_termoption(T_CCO);

    /* Set 'weirdinvert' according to value of 't_xs' */
    p_wiv = (*T_XS != NUL);
  }
  need_gather = TRUE;

  /* Set t_colors to the value of $COLORS or t_Co. */
  t_colors = atoi((char *)T_CCO);
  env_colors = mch_getenv((char_u *)"COLORS");
  if (env_colors != NULL && isdigit(*env_colors))
  {
    int colors = atoi((char *)env_colors);

    if (colors != t_colors)
      set_color_count(colors);
  }
}

/*
 * Check if the new shell size is valid, correct it if it's too small or way
 * too big.
 */
void check_shellsize(void)
{
  if (Rows < min_rows()) /* need room for one window and command line */
    Rows = min_rows();
  limit_screen_size();
}

/*
 * Limit Rows and Columns to avoid an overflow in Rows * Columns.
 */
void limit_screen_size(void)
{
  if (Columns < MIN_COLUMNS)
    Columns = MIN_COLUMNS;
  else if (Columns > 10000)
    Columns = 10000;
  if (Rows > 1000)
    Rows = 1000;
}

/*
 * Invoked just before the screen structures are going to be (re)allocated.
 */
void win_new_shellsize(void)
{
  static int old_Rows = 0;
  static int old_Columns = 0;

  if (old_Rows != Rows || old_Columns != Columns)
    ui_new_shellsize();
  if (old_Rows != Rows)
  {
    /* if 'window' uses the whole screen, keep it using that */
    if (p_window == old_Rows - 1 || old_Rows == 0)
      p_window = Rows - 1;
    old_Rows = Rows;
    shell_new_rows(); /* update window sizes */
  }
  if (old_Columns != Columns)
  {
    old_Columns = Columns;
    shell_new_columns(); /* update window sizes */
  }
}

/*
 * Call this function when the Vim shell has been resized in any way.
 * Will obtain the current size and redraw (also when size didn't change).
 */
void shell_resized(void)
{
  set_shellsize(0, 0, FALSE);
}

/*
 * Check if the shell size changed.  Handle a resize.
 * When the size didn't change, nothing happens.
 */
void shell_resized_check(void)
{
  int old_Rows = Rows;
  int old_Columns = Columns;

  if (!exiting)
  {
    (void)ui_get_shellsize();
    check_shellsize();
    if (old_Rows != Rows || old_Columns != Columns)
      shell_resized();
  }
}

/*
 * Set size of the Vim shell.
 * If 'mustset' is TRUE, we must set Rows and Columns, do not get the real
 * window size (this is used for the :win command).
 * If 'mustset' is FALSE, we may try to get the real window size and if
 * it fails use 'width' and 'height'.
 */
void set_shellsize(int width, int height, int mustset)
{
  static int busy = FALSE;

  /*
     * Avoid recursiveness, can happen when setting the window size causes
     * another window-changed signal.
     */
  if (busy)
    return;

  if (width < 0 || height < 0) /* just checking... */
    return;

  if (State == HITRETURN || State == SETWSIZE)
  {
    /* postpone the resizing */
    State = SETWSIZE;
    return;
  }

  /* curwin->w_buffer can be NULL when we are closing a window and the
     * buffer has already been closed and removing a scrollbar causes a resize
     * event. Don't resize then, it will happen after entering another buffer.
     */
  if (curwin->w_buffer == NULL)
    return;

  ++busy;

  if (mustset || (ui_get_shellsize() == FAIL && height != 0))
  {
    Rows = height;
    Columns = width;
    check_shellsize();
    ui_set_shellsize(mustset);
  }
  else
    check_shellsize();

  /* The window layout used to be adjusted here, but it now happens in
     * screenalloc() (also invoked from screenclear()).  That is because the
     * "busy" check above may skip this, but not screenalloc(). */

  if (State != ASKMORE && State != EXTERNCMD && State != CONFIRM)
    screenclear();
  else
    screen_start(); /* don't know where cursor is now */

  if (starting != NO_SCREEN)
  {
    changed_line_abv_curs();
    invalidate_botline();

    /*
	 * We only redraw when it's needed:
	 * - While at the more prompt or executing an external command, don't
	 *   redraw, but position the cursor.
	 * - While editing the command line, only redraw that.
	 * - in Ex mode, don't redraw anything.
	 * - Otherwise, redraw right now, and position the cursor.
	 * Always need to call update_screen() or screenalloc(), to make
	 * sure Rows/Columns and the size of ScreenLines[] is correct!
	 */
    if (State == ASKMORE || State == EXTERNCMD || State == CONFIRM || exmode_active)
    {
      screenalloc(FALSE);
      repeat_message();
    }
    else
    {
      if (curwin->w_p_scb)
        do_check_scrollbind(TRUE);
      if (State & CMDLINE)
      {
        update_screen(NOT_VALID);
        redrawcmdline();
      }
      else
      {
        update_topline();
        update_screen(NOT_VALID);
        if (redrawing())
          setcursor();
      }
    }
    cursor_on(); /* redrawing may have switched it off */
  }
  --busy;
}

/*
 * Set the terminal to TMODE_RAW (for Normal mode) or TMODE_COOK (for external
 * commands and Ex mode).
 */
void settmode(int tmode)
{
  // libvim - NOOP
}

void starttermcap(void)
{
  // libvim - NOOP
}

void stoptermcap(void)
{
  // libvim - NOOP
}

/*
 * Return TRUE when saving and restoring the screen.
 */
int swapping_screen(void)
{
  return (full_screen && *T_TI != NUL);
}

/*
 * By outputting the 'cursor very visible' termcap code, for some windowed
 * terminals this makes the screen scrolled to the correct position.
 * Used when starting Vim or returning from a shell.
 */
void scroll_start(void)
{
  if (*T_VS != NUL && *T_CVS != NUL)
  {
    out_str(T_VS);
    out_str(T_CVS);
    screen_start(); /* don't know where cursor is now */
  }
}

static int cursor_is_off = FALSE;

/*
 * Enable the cursor without checking if it's already enabled.
 */
void cursor_on_force(void)
{
  out_str(T_VE);
  cursor_is_off = FALSE;
}

/*
 * Enable the cursor if it's currently off.
 */
void cursor_on(void)
{
  if (cursor_is_off)
    cursor_on_force();
}

/*
 * Disable the cursor.
 */
void cursor_off(void)
{
  if (full_screen && !cursor_is_off)
  {
    out_str(T_VI); /* disable cursor */
    cursor_is_off = TRUE;
  }
}

/*
 * Set scrolling region for window 'wp'.
 * The region starts 'off' lines from the start of the window.
 * Also set the vertical scroll region for a vertically split window.  Always
 * the full width of the window, excluding the vertical separator.
 */
void scroll_region_set(win_T *wp, int off)
{
  OUT_STR(tgoto((char *)T_CS, W_WINROW(wp) + wp->w_height - 1,
                W_WINROW(wp) + off));
  if (*T_CSV != NUL && wp->w_width != Columns)
    OUT_STR(tgoto((char *)T_CSV, wp->w_wincol + wp->w_width - 1,
                  wp->w_wincol));
  screen_start(); /* don't know where cursor is now */
}

/*
 * Reset scrolling region to the whole screen.
 */
void scroll_region_reset(void)
{
  OUT_STR(tgoto((char *)T_CS, (int)Rows - 1, 0));
  if (*T_CSV != NUL)
    OUT_STR(tgoto((char *)T_CSV, (int)Columns - 1, 0));
  screen_start(); /* don't know where cursor is now */
}

/*
 * List of terminal codes that are currently recognized.
 */

static struct termcode
{
  char_u name[2]; /* termcap name of entry */
  char_u *code;   /* terminal code (in allocated memory) */
  int len;        /* STRLEN(code) */
  int modlen;     /* length of part before ";*~". */
} *termcodes = NULL;

static int tc_max_len = 0; /* number of entries that termcodes[] can hold */
static int tc_len = 0;     /* current number of entries in termcodes[] */

static int termcode_star(char_u *code, int len);

void clear_termcodes(void)
{
  while (tc_len > 0)
    vim_free(termcodes[--tc_len].code);
  VIM_CLEAR(termcodes);
  tc_max_len = 0;

#ifdef HAVE_TGETENT
  BC = (char *)empty_option;
  UP = (char *)empty_option;
  PC = NUL; /* set pad character to NUL */
  ospeed = 0;
#endif

  need_gather = TRUE; /* need to fill termleader[] */
}

#define ATC_FROM_TERM 55

/*
 * Add a new entry to the list of terminal codes.
 * The list is kept alphabetical for ":set termcap"
 * "flags" is TRUE when replacing 7-bit by 8-bit controls is desired.
 * "flags" can also be ATC_FROM_TERM for got_code_from_term().
 */
void add_termcode(char_u *name, char_u *string, int flags)
{
  struct termcode *new_tc;
  int i, j;
  char_u *s;
  int len;

  if (string == NULL || *string == NUL)
  {
    del_termcode(name);
    return;
  }

#if defined(MSWIN)
  s = vim_strnsave(string, (int)STRLEN(string) + 1);
#else
#ifdef VIMDLL
  if (!gui.in_use)
    s = vim_strnsave(string, (int)STRLEN(string) + 1);
  else
#endif
    s = vim_strsave(string);
#endif
  if (s == NULL)
    return;

  /* Change leading <Esc>[ to CSI, change <Esc>O to <M-O>. */
  if (flags != 0 && flags != ATC_FROM_TERM && term_7to8bit(string) != 0)
  {
    STRMOVE(s, s + 1);
    s[0] = term_7to8bit(string);
  }

#if defined(MSWIN)
#ifdef VIMDLL
  if (!gui.in_use)
#endif
  {
    if (s[0] == K_NUL)
    {
      STRMOVE(s + 1, s);
      s[1] = 3;
    }
  }
#endif

  len = (int)STRLEN(s);

  need_gather = TRUE; /* need to fill termleader[] */

  /*
     * need to make space for more entries
     */
  if (tc_len == tc_max_len)
  {
    tc_max_len += 20;
    new_tc = ALLOC_MULT(struct termcode, tc_max_len);
    if (new_tc == NULL)
    {
      tc_max_len -= 20;
      return;
    }
    for (i = 0; i < tc_len; ++i)
      new_tc[i] = termcodes[i];
    vim_free(termcodes);
    termcodes = new_tc;
  }

  /*
     * Look for existing entry with the same name, it is replaced.
     * Look for an existing entry that is alphabetical higher, the new entry
     * is inserted in front of it.
     */
  for (i = 0; i < tc_len; ++i)
  {
    if (termcodes[i].name[0] < name[0])
      continue;
    if (termcodes[i].name[0] == name[0])
    {
      if (termcodes[i].name[1] < name[1])
        continue;
      /*
	     * Exact match: May replace old code.
	     */
      if (termcodes[i].name[1] == name[1])
      {
        if (flags == ATC_FROM_TERM && (j = termcode_star(
                                           termcodes[i].code, termcodes[i].len)) > 0)
        {
          /* Don't replace ESC[123;*X or ESC O*X with another when
		     * invoked from got_code_from_term(). */
          if (len == termcodes[i].len - j && STRNCMP(s, termcodes[i].code, len - 1) == 0 && s[len - 1] == termcodes[i].code[termcodes[i].len - 1])
          {
            /* They are equal but for the ";*": don't add it. */
            vim_free(s);
            return;
          }
        }
        else
        {
          /* Replace old code. */
          vim_free(termcodes[i].code);
          --tc_len;
          break;
        }
      }
    }
    /*
	 * Found alphabetical larger entry, move rest to insert new entry
	 */
    for (j = tc_len; j > i; --j)
      termcodes[j] = termcodes[j - 1];
    break;
  }

  termcodes[i].name[0] = name[0];
  termcodes[i].name[1] = name[1];
  termcodes[i].code = s;
  termcodes[i].len = len;

  /* For xterm we recognize special codes like "ESC[42;*X" and "ESC O*X" that
     * accept modifiers. */
  termcodes[i].modlen = 0;
  j = termcode_star(s, len);
  if (j > 0)
    termcodes[i].modlen = len - 1 - j;
  ++tc_len;
}

/*
 * Check termcode "code[len]" for ending in ;*X or *X.
 * The "X" can be any character.
 * Return 0 if not found, 2 for ;*X and 1 for *X.
 */
static int
termcode_star(char_u *code, int len)
{
  /* Shortest is <M-O>*X.  With ; shortest is <CSI>1;*X */
  if (len >= 3 && code[len - 2] == '*')
  {
    if (len >= 5 && code[len - 3] == ';')
      return 2;
    else
      return 1;
  }
  return 0;
}

char_u *
find_termcode(char_u *name)
{
  int i;

  for (i = 0; i < tc_len; ++i)
    if (termcodes[i].name[0] == name[0] && termcodes[i].name[1] == name[1])
      return termcodes[i].code;
  return NULL;
}

#if defined(FEAT_CMDL_COMPL) || defined(PROTO)
char_u *
get_termcode(int i)
{
  if (i >= tc_len)
    return NULL;
  return &termcodes[i].name[0];
}
#endif

void del_termcode(char_u *name)
{
  int i;

  if (termcodes == NULL) /* nothing there yet */
    return;

  need_gather = TRUE; /* need to fill termleader[] */

  for (i = 0; i < tc_len; ++i)
    if (termcodes[i].name[0] == name[0] && termcodes[i].name[1] == name[1])
    {
      del_termcode_idx(i);
      return;
    }
  /* not found. Give error message? */
}

static void
del_termcode_idx(int idx)
{
  int i;

  vim_free(termcodes[idx].code);
  --tc_len;
  for (i = idx; i < tc_len; ++i)
    termcodes[i] = termcodes[i + 1];
}

#ifdef CHECK_DOUBLE_CLICK
static linenr_T orig_topline = 0;
#ifdef FEAT_DIFF
static int orig_topfill = 0;
#endif
#endif
#if defined(CHECK_DOUBLE_CLICK) || defined(PROTO)
/*
 * Checking for double clicks ourselves.
 * "orig_topline" is used to avoid detecting a double-click when the window
 * contents scrolled (e.g., when 'scrolloff' is non-zero).
 */
/*
 * Set orig_topline.  Used when jumping to another window, so that a double
 * click still works.
 */
void set_mouse_topline(win_T *wp)
{
  orig_topline = wp->w_topline;
#ifdef FEAT_DIFF
  orig_topfill = wp->w_topfill;
#endif
}
#endif

/*
 * Check if typebuf.tb_buf[] contains a terminal key code.
 * Check from typebuf.tb_buf[typebuf.tb_off] to typebuf.tb_buf[typebuf.tb_off
 * + max_offset].
 * Return 0 for no match, -1 for partial match, > 0 for full match.
 * Return KEYLEN_REMOVED when a key code was deleted.
 * With a match, the match is removed, the replacement code is inserted in
 * typebuf.tb_buf[] and the number of characters in typebuf.tb_buf[] is
 * returned.
 * When "buf" is not NULL, buf[bufsize] is used instead of typebuf.tb_buf[].
 * "buflen" is then the length of the string in buf[] and is updated for
 * inserts and deletes.
 */
int check_termcode(
    int max_offset,
    char_u *buf,
    int bufsize,
    int *buflen)
{
  char_u *tp;
  char_u *p;
  int slen = 0; /* init for GCC */
  int modslen;
  int len;
  int retval = 0;
  int offset;
  char_u key_name[2];
  int modifiers;
  char_u *modifiers_start = NULL;
  int key;
  int new_slen;
  int extra;
  char_u string[MAX_KEY_CODE_LEN + 1];
  int i, j;
  int idx = 0;
  int cpo_koffset;

  cpo_koffset = (vim_strchr(p_cpo, CPO_KOFFSET) != NULL);

  /*
     * Speed up the checks for terminal codes by gathering all first bytes
     * used in termleader[].  Often this is just a single <Esc>.
     */
  if (need_gather)
    gather_termleader();

  /*
     * Check at several positions in typebuf.tb_buf[], to catch something like
     * "x<Up>" that can be mapped. Stop at max_offset, because characters
     * after that cannot be used for mapping, and with @r commands
     * typebuf.tb_buf[] can become very long.
     * This is used often, KEEP IT FAST!
     */
  for (offset = 0; offset < max_offset; ++offset)
  {
    if (buf == NULL)
    {
      if (offset >= typebuf.tb_len)
        break;
      tp = typebuf.tb_buf + typebuf.tb_off + offset;
      len = typebuf.tb_len - offset; /* length of the input */
    }
    else
    {
      if (offset >= *buflen)
        break;
      tp = buf + offset;
      len = *buflen - offset;
    }

    /*
	 * Don't check characters after K_SPECIAL, those are already
	 * translated terminal chars (avoid translating ~@^Hx).
	 */
    if (*tp == K_SPECIAL)
    {
      offset += 2; /* there are always 2 extra characters */
      continue;
    }

    /*
	 * Skip this position if the character does not appear as the first
	 * character in term_strings. This speeds up a lot, since most
	 * termcodes start with the same character (ESC or CSI).
	 */
    i = *tp;
    for (p = termleader; *p && *p != i; ++p)
      ;
    if (*p == NUL)
      continue;

    /*
	 * Skip this position if p_ek is not set and tp[0] is an ESC and we
	 * are in Insert mode.
	 */
    if (*tp == ESC && !p_ek && (State & INSERT))
      continue;

    key_name[0] = NUL; /* no key name found yet */
    key_name[1] = NUL; /* no key name found yet */
    modifiers = 0;     /* no modifiers yet */

    for (idx = 0; idx < tc_len; ++idx)
    {
      /*
		 * Ignore the entry if we are not at the start of
		 * typebuf.tb_buf[]
		 * and there are not enough characters to make a match.
		 * But only when the 'K' flag is in 'cpoptions'.
		 */
      slen = termcodes[idx].len;
      modifiers_start = NULL;
      if (cpo_koffset && offset && len < slen)
        continue;
      if (STRNCMP(termcodes[idx].code, tp,
                  (size_t)(slen > len ? len : slen)) == 0)
      {
        if (len < slen) /* got a partial sequence */
          return -1;    /* need to get more chars */

        /*
		     * When found a keypad key, check if there is another key
		     * that matches and use that one.  This makes <Home> to be
		     * found instead of <kHome> when they produce the same
		     * key code.
		     */
        if (termcodes[idx].name[0] == 'K' && VIM_ISDIGIT(termcodes[idx].name[1]))
        {
          for (j = idx + 1; j < tc_len; ++j)
            if (termcodes[j].len == slen &&
                STRNCMP(termcodes[idx].code,
                        termcodes[j].code, slen) == 0)
            {
              idx = j;
              break;
            }
        }

        key_name[0] = termcodes[idx].name[0];
        key_name[1] = termcodes[idx].name[1];
        break;
      }

      /*
		 * Check for code with modifier, like xterm uses:
		 * <Esc>[123;*X  (modslen == slen - 3)
		 * Also <Esc>O*X and <M-O>*X (modslen == slen - 2).
		 * When there is a modifier the * matches a number.
		 * When there is no modifier the ;* or * is omitted.
		 */
      if (termcodes[idx].modlen > 0)
      {
        modslen = termcodes[idx].modlen;
        if (cpo_koffset && offset && len < modslen)
          continue;
        if (STRNCMP(termcodes[idx].code, tp,
                    (size_t)(modslen > len ? len : modslen)) == 0)
        {
          int n;

          if (len <= modslen) /* got a partial sequence */
            return -1;        /* need to get more chars */

          if (tp[modslen] == termcodes[idx].code[slen - 1])
            slen = modslen + 1; /* no modifiers */
          else if (tp[modslen] != ';' && modslen == slen - 3)
            continue; /* no match */
          else
          {
            // Skip over the digits, the final char must
            // follow. URXVT can use a negative value, thus
            // also accept '-'.
            for (j = slen - 2; j < len && (isdigit(tp[j]) || tp[j] == '-' || tp[j] == ';'); ++j)
              ;
            ++j;
            if (len < j) /* got a partial sequence */
              return -1; /* need to get more chars */
            if (tp[j - 1] != termcodes[idx].code[slen - 1])
              continue; /* no match */

            modifiers_start = tp + slen - 2;

            /* Match!  Convert modifier bits. */
            n = atoi((char *)modifiers_start) - 1;
            if (n & 1)
              modifiers |= MOD_MASK_SHIFT;
            if (n & 2)
              modifiers |= MOD_MASK_ALT;
            if (n & 4)
              modifiers |= MOD_MASK_CTRL;
            if (n & 8)
              modifiers |= MOD_MASK_META;

            slen = j;
          }
          key_name[0] = termcodes[idx].name[0];
          key_name[1] = termcodes[idx].name[1];
          break;
        }
      }
    }

    if (key_name[0] == NUL)
      continue; /* No match at this position, try next one */

    /* We only get here when we have a complete termcode match */

    /*
	 * Change <xHome> to <Home>, <xUp> to <Up>, etc.
	 */
    key = handle_x_keys(TERMCAP2KEY(key_name[0], key_name[1]));

    /*
	 * Add any modifier codes to our string.
	 */
    new_slen = 0; /* Length of what will replace the termcode */
    if (modifiers != 0)
    {
      /* Some keys have the modifier included.  Need to handle that here
	     * to make mappings work. */
      key = simplify_key(key, &modifiers);
      if (modifiers != 0)
      {
        string[new_slen++] = K_SPECIAL;
        string[new_slen++] = (int)KS_MODIFIER;
        string[new_slen++] = modifiers;
      }
    }

    /* Finally, add the special key code to our string */
    key_name[0] = KEY2TERMCAP0(key);
    key_name[1] = KEY2TERMCAP1(key);
    if (key_name[0] == KS_KEY)
    {
      /* from ":set <M-b>=xx" */
      if (has_mbyte)
        new_slen += (*mb_char2bytes)(key_name[1], string + new_slen);
      else
        string[new_slen++] = key_name[1];
    }
    else if (new_slen == 0 && key_name[0] == KS_EXTRA && key_name[1] == KE_IGNORE)
    {
      /* Do not put K_IGNORE into the buffer, do return KEYLEN_REMOVED
	     * to indicate what happened. */
      retval = KEYLEN_REMOVED;
    }
    else
    {
      string[new_slen++] = K_SPECIAL;
      string[new_slen++] = key_name[0];
      string[new_slen++] = key_name[1];
    }
    string[new_slen] = NUL;
    extra = new_slen - slen;
    if (buf == NULL)
    {
      if (extra < 0)
        /* remove matched chars, taking care of noremap */
        del_typebuf(-extra, offset);
      else if (extra > 0)
        /* insert the extra space we need */
        ins_typebuf(string + slen, REMAP_YES, offset, FALSE, FALSE);

      /*
	     * Careful: del_typebuf() and ins_typebuf() may have reallocated
	     * typebuf.tb_buf[]!
	     */
      mch_memmove(typebuf.tb_buf + typebuf.tb_off + offset, string,
                  (size_t)new_slen);
    }
    else
    {
      if (extra < 0)
        /* remove matched characters */
        mch_memmove(buf + offset, buf + offset - extra,
                    (size_t)(*buflen + offset + extra));
      else if (extra > 0)
      {
        /* Insert the extra space we need.  If there is insufficient
		 * space return -1. */
        if (*buflen + extra + new_slen >= bufsize)
          return -1;
        mch_memmove(buf + offset + extra, buf + offset,
                    (size_t)(*buflen - offset));
      }
      mch_memmove(buf + offset, string, (size_t)new_slen);
      *buflen = *buflen + extra + new_slen;
    }
    return retval == 0 ? (len + extra + offset) : retval;
  }

  return 0; /* no match found */
}

/*
 * Replace any terminal code strings in from[] with the equivalent internal
 * vim representation.	This is used for the "from" and "to" part of a
 * mapping, and the "to" part of a menu command.
 * Any strings like "<C-UP>" are also replaced, unless 'cpoptions' contains
 * '<'.
 * K_SPECIAL by itself is replaced by K_SPECIAL KS_SPECIAL KE_FILLER.
 *
 * The replacement is done in result[] and finally copied into allocated
 * memory. If this all works well *bufp is set to the allocated memory and a
 * pointer to it is returned. If something fails *bufp is set to NULL and from
 * is returned.
 *
 * CTRL-V characters are removed.  When "from_part" is TRUE, a trailing CTRL-V
 * is included, otherwise it is removed (for ":map xx ^V", maps xx to
 * nothing).  When 'cpoptions' does not contain 'B', a backslash can be used
 * instead of a CTRL-V.
 */
char_u *
replace_termcodes(
    char_u *from,
    char_u **bufp,
    int from_part,
    int do_lt,   /* also translate <lt> */
    int special) /* always accept <key> notation */
{
  int i;
  int slen;
  int key;
  int dlen = 0;
  char_u *src;
  int do_backslash; /* backslash is a special character */
  int do_special;   /* recognize <> key codes */
  int do_key_code;  /* recognize raw key codes */
  char_u *result;   /* buffer for resulting string */

  do_backslash = (vim_strchr(p_cpo, CPO_BSLASH) == NULL);
  do_special = (vim_strchr(p_cpo, CPO_SPECI) == NULL) || special;
  do_key_code = (vim_strchr(p_cpo, CPO_KEYCODE) == NULL);

  /*
     * Allocate space for the translation.  Worst case a single character is
     * replaced by 6 bytes (shifted special key), plus a NUL at the end.
     */
  result = alloc(STRLEN(from) * 6 + 1);
  if (result == NULL) /* out of memory */
  {
    *bufp = NULL;
    return from;
  }

  src = from;

  /*
     * Check for #n at start only: function key n
     */
  if (from_part && src[0] == '#' && VIM_ISDIGIT(src[1])) /* function key */
  {
    result[dlen++] = K_SPECIAL;
    result[dlen++] = 'k';
    if (src[1] == '0')
      result[dlen++] = ';'; /* #0 is F10 is "k;" */
    else
      result[dlen++] = src[1]; /* #3 is F3 is "k3" */
    src += 2;
  }

  /*
     * Copy each byte from *from to result[dlen]
     */
  while (*src != NUL)
  {
    /*
	 * If 'cpoptions' does not contain '<', check for special key codes,
	 * like "<C-S-LeftMouse>"
	 */
    if (do_special && (do_lt || STRNCMP(src, "<lt>", 4) != 0))
    {
#ifdef FEAT_EVAL
      /*
	     * Replace <SID> by K_SNR <script-nr> _.
	     * (room: 5 * 6 = 30 bytes; needed: 3 + <nr> + 1 <= 14)
	     */
      if (STRNICMP(src, "<SID>", 5) == 0)
      {
        if (current_sctx.sc_sid <= 0)
          emsg(_(e_usingsid));
        else
        {
          src += 5;
          result[dlen++] = K_SPECIAL;
          result[dlen++] = (int)KS_EXTRA;
          result[dlen++] = (int)KE_SNR;
          sprintf((char *)result + dlen, "%ld",
                  (long)current_sctx.sc_sid);
          dlen += (int)STRLEN(result + dlen);
          result[dlen++] = '_';
          continue;
        }
      }
#endif

      slen = trans_special(&src, result + dlen, TRUE, FALSE);
      if (slen)
      {
        dlen += slen;
        continue;
      }
    }

    /*
	 * If 'cpoptions' does not contain 'k', see if it's an actual key-code.
	 * Note that this is also checked after replacing the <> form.
	 * Single character codes are NOT replaced (e.g. ^H or DEL), because
	 * it could be a character in the file.
	 */
    if (do_key_code)
    {
      i = find_term_bykeys(src);
      if (i >= 0)
      {
        result[dlen++] = K_SPECIAL;
        result[dlen++] = termcodes[i].name[0];
        result[dlen++] = termcodes[i].name[1];
        src += termcodes[i].len;
        /* If terminal code matched, continue after it. */
        continue;
      }
    }

#ifdef FEAT_EVAL
    if (do_special)
    {
      char_u *p, *s, len;

      /*
	     * Replace <Leader> by the value of "mapleader".
	     * Replace <LocalLeader> by the value of "maplocalleader".
	     * If "mapleader" or "maplocalleader" isn't set use a backslash.
	     */
      if (STRNICMP(src, "<Leader>", 8) == 0)
      {
        len = 8;
        p = get_var_value((char_u *)"g:mapleader");
      }
      else if (STRNICMP(src, "<LocalLeader>", 13) == 0)
      {
        len = 13;
        p = get_var_value((char_u *)"g:maplocalleader");
      }
      else
      {
        len = 0;
        p = NULL;
      }
      if (len != 0)
      {
        /* Allow up to 8 * 6 characters for "mapleader". */
        if (p == NULL || *p == NUL || STRLEN(p) > 8 * 6)
          s = (char_u *)"\\";
        else
          s = p;
        while (*s != NUL)
          result[dlen++] = *s++;
        src += len;
        continue;
      }
    }
#endif

    /*
	 * Remove CTRL-V and ignore the next character.
	 * For "from" side the CTRL-V at the end is included, for the "to"
	 * part it is removed.
	 * If 'cpoptions' does not contain 'B', also accept a backslash.
	 */
    key = *src;
    if (key == Ctrl_V || (do_backslash && key == '\\'))
    {
      ++src; /* skip CTRL-V or backslash */
      if (*src == NUL)
      {
        if (from_part)
          result[dlen++] = key;
        break;
      }
    }

    /* skip multibyte char correctly */
    for (i = (*mb_ptr2len)(src); i > 0; --i)
    {
      /*
	     * If the character is K_SPECIAL, replace it with K_SPECIAL
	     * KS_SPECIAL KE_FILLER.
	     * If compiled with the GUI replace CSI with K_CSI.
	     */
      if (*src == K_SPECIAL)
      {
        result[dlen++] = K_SPECIAL;
        result[dlen++] = KS_SPECIAL;
        result[dlen++] = KE_FILLER;
      }
      else
        result[dlen++] = *src;
      ++src;
    }
  }
  result[dlen] = NUL;

  /*
     * Copy the new string to allocated memory.
     * If this fails, just return from.
     */
  if ((*bufp = vim_strsave(result)) != NULL)
    from = *bufp;
  vim_free(result);
  return from;
}

/*
 * Find a termcode with keys 'src' (must be NUL terminated).
 * Return the index in termcodes[], or -1 if not found.
 */
int find_term_bykeys(char_u *src)
{
  int i;
  int slen = (int)STRLEN(src);

  for (i = 0; i < tc_len; ++i)
  {
    if (slen == termcodes[i].len && STRNCMP(termcodes[i].code, src, (size_t)slen) == 0)
      return i;
  }
  return -1;
}

/*
 * Gather the first characters in the terminal key codes into a string.
 * Used to speed up check_termcode().
 */
static void
gather_termleader(void)
{
  int i;
  int len = 0;

  termleader[len] = NUL;

  for (i = 0; i < tc_len; ++i)
    if (vim_strchr(termleader, termcodes[i].code[0]) == NULL)
    {
      termleader[len++] = termcodes[i].code[0];
      termleader[len] = NUL;
    }

  need_gather = FALSE;
}

/*
 * Show all termcodes (for ":set termcap")
 * This code looks a lot like showoptions(), but is different.
 */
void show_termcodes(void)
{
  int col;
  int *items;
  int item_count;
  int run;
  int row, rows;
  int cols;
  int i;
  int len;

#define INC3 27 /* try to make three columns */
#define INC2 40 /* try to make two columns */
#define GAP 2   /* spaces between columns */

  if (tc_len == 0) /* no terminal codes (must be GUI) */
    return;
  items = ALLOC_MULT(int, tc_len);
  if (items == NULL)
    return;

  /* Highlight title */
  msg_puts_title(_("\n--- Terminal keys ---"));

  /*
     * do the loop two times:
     * 1. display the short items (non-strings and short strings)
     * 2. display the medium items (medium length strings)
     * 3. display the long items (remaining strings)
     */
  for (run = 1; run <= 3 && !got_int; ++run)
  {
    /*
	 * collect the items in items[]
	 */
    item_count = 0;
    for (i = 0; i < tc_len; i++)
    {
      len = show_one_termcode(termcodes[i].name,
                              termcodes[i].code, FALSE);
      if (len <= INC3 - GAP ? run == 1
                            : len <= INC2 - GAP ? run == 2
                                                : run == 3)
        items[item_count++] = i;
    }

    /*
	 * display the items
	 */
    if (run <= 2)
    {
      cols = (Columns + GAP) / (run == 1 ? INC3 : INC2);
      if (cols == 0)
        cols = 1;
      rows = (item_count + cols - 1) / cols;
    }
    else /* run == 3 */
      rows = item_count;
    for (row = 0; row < rows && !got_int; ++row)
    {
      msg_putchar('\n'); /* go to next line */
      if (got_int)       /* 'q' typed in more */
        break;
      col = 0;
      for (i = row; i < item_count; i += rows)
      {
        msg_col = col; /* make columns */
        show_one_termcode(termcodes[items[i]].name,
                          termcodes[items[i]].code, TRUE);
        if (run == 2)
          col += INC2;
        else
          col += INC3;
      }
      ui_breakcheck();
    }
  }
  vim_free(items);
}

/*
 * Show one termcode entry.
 * Output goes into IObuff[]
 */
int show_one_termcode(char_u *name, char_u *code, int printit)
{
  char_u *p;
  int len;

  if (name[0] > '~')
  {
    IObuff[0] = ' ';
    IObuff[1] = ' ';
    IObuff[2] = ' ';
    IObuff[3] = ' ';
  }
  else
  {
    IObuff[0] = 't';
    IObuff[1] = '_';
    IObuff[2] = name[0];
    IObuff[3] = name[1];
  }
  IObuff[4] = ' ';

  p = get_special_key_name(TERMCAP2KEY(name[0], name[1]), 0);
  if (p[1] != 't')
    STRCPY(IObuff + 5, p);
  else
    IObuff[5] = NUL;
  len = (int)STRLEN(IObuff);
  do
    IObuff[len++] = ' ';
  while (len < 17);
  IObuff[len] = NUL;
  if (code == NULL)
    len += 4;
  else
    len += vim_strsize(code);

  if (printit)
  {
    msg_puts((char *)IObuff);
    if (code == NULL)
      msg_puts("NULL");
    else
      msg_outtrans(code);
  }
  return len;
}

#if defined(FEAT_CMDL_COMPL) || defined(PROTO)
/*
 * Translate an internal mapping/abbreviation representation into the
 * corresponding external one recognized by :map/:abbrev commands.
 * Respects the current B/k/< settings of 'cpoption'.
 *
 * This function is called when expanding mappings/abbreviations on the
 * command-line.
 *
 * It uses a growarray to build the translation string since the latter can be
 * wider than the original description. The caller has to free the string
 * afterwards.
 *
 * Returns NULL when there is a problem.
 */
char_u *
translate_mapping(char_u *str)
{
  garray_T ga;
  int c;
  int modifiers;
  int cpo_bslash;
  int cpo_special;

  ga_init(&ga);
  ga.ga_itemsize = 1;
  ga.ga_growsize = 40;

  cpo_bslash = (vim_strchr(p_cpo, CPO_BSLASH) != NULL);
  cpo_special = (vim_strchr(p_cpo, CPO_SPECI) != NULL);

  for (; *str; ++str)
  {
    c = *str;
    if (c == K_SPECIAL && str[1] != NUL && str[2] != NUL)
    {
      modifiers = 0;
      if (str[1] == KS_MODIFIER)
      {
        str++;
        modifiers = *++str;
        c = *++str;
      }
      if (c == K_SPECIAL && str[1] != NUL && str[2] != NUL)
      {
        if (cpo_special)
        {
          ga_clear(&ga);
          return NULL;
        }
        c = TO_SPECIAL(str[1], str[2]);
        if (c == K_ZERO) /* display <Nul> as ^@ */
          c = NUL;
        str += 2;
      }
      if (IS_SPECIAL(c) || modifiers) /* special key */
      {
        if (cpo_special)
        {
          ga_clear(&ga);
          return NULL;
        }
        ga_concat(&ga, get_special_key_name(c, modifiers));
        continue; /* for (str) */
      }
    }
    if (c == ' ' || c == '\t' || c == Ctrl_J || c == Ctrl_V || (c == '<' && !cpo_special) || (c == '\\' && !cpo_bslash))
      ga_append(&ga, cpo_bslash ? Ctrl_V : '\\');
    if (c)
      ga_append(&ga, c);
  }
  ga_append(&ga, NUL);
  return (char_u *)(ga.ga_data);
}
#endif

#if defined(MSWIN) || defined(PROTO)
static char ksme_str[20];
static char ksmr_str[20];
static char ksmd_str[20];

/*
 * For Win32 console: update termcap codes for existing console attributes.
 */
void update_tcap(int attr)
{
  struct builtin_term *p;

  p = find_builtin_term(DEFAULT_TERM);
  sprintf(ksme_str, IF_EB("\033|%dm", ESC_STR "|%dm"), attr);
  sprintf(ksmd_str, IF_EB("\033|%dm", ESC_STR "|%dm"),
          attr | 0x08); /* FOREGROUND_INTENSITY */
  sprintf(ksmr_str, IF_EB("\033|%dm", ESC_STR "|%dm"),
          ((attr & 0x0F) << 4) | ((attr & 0xF0) >> 4));

  while (p->bt_string != NULL)
  {
    if (p->bt_entry == (int)KS_ME)
      p->bt_string = &ksme_str[0];
    else if (p->bt_entry == (int)KS_MR)
      p->bt_string = &ksmr_str[0];
    else if (p->bt_entry == (int)KS_MD)
      p->bt_string = &ksmd_str[0];
    ++p;
  }
}

/*
 * For Win32 console: replace the sequence immediately after termguicolors.
 */
void swap_tcap(void)
{
}

#endif

#if defined(PROTO)
static int
hex_digit(int c)
{
  if (isdigit(c))
    return c - '0';
  c = TOLOWER_ASC(c);
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return 0x1ffffff;
}

#ifdef VIMDLL
static guicolor_T
gui_adjust_rgb(guicolor_T c)
{
  if (gui.in_use)
    return c;
  else
    return ((c & 0xff) << 16) | (c & 0x00ff00) | ((c >> 16) & 0xff);
}
#else
#define gui_adjust_rgb(c) (c)
#endif

guicolor_T
gui_get_color_cmn(char_u *name)
{
  /* On MS-Windows an RGB macro is available and it produces 0x00bbggrr color
     * values as used by the MS-Windows GDI api.  It should be used only for
     * MS-Windows GDI builds. */
#if defined(RGB) && defined(MSWIN)
#undef RGB
#endif
#ifndef RGB
#define RGB(r, g, b) ((r << 16) | (g << 8) | (b))
#endif
#define LINE_LEN 100
  FILE *fd;
  char line[LINE_LEN];
  char_u *fname;
  int r, g, b, i;
  guicolor_T color;

  struct rgbcolor_table_S
  {
    char_u *color_name;
    guicolor_T color;
  };

  /* Only non X11 colors (not present in rgb.txt) and colors in
     * color_names[], useful when $VIMRUNTIME is not found,. */
  static struct rgbcolor_table_S rgb_table[] = {
      {(char_u *)"black", RGB(0x00, 0x00, 0x00)},
      {(char_u *)"blue", RGB(0x00, 0x00, 0xFF)},
      {(char_u *)"brown", RGB(0xA5, 0x2A, 0x2A)},
      {(char_u *)"cyan", RGB(0x00, 0xFF, 0xFF)},
      {(char_u *)"darkblue", RGB(0x00, 0x00, 0x8B)},
      {(char_u *)"darkcyan", RGB(0x00, 0x8B, 0x8B)},
      {(char_u *)"darkgray", RGB(0xA9, 0xA9, 0xA9)},
      {(char_u *)"darkgreen", RGB(0x00, 0x64, 0x00)},
      {(char_u *)"darkgrey", RGB(0xA9, 0xA9, 0xA9)},
      {(char_u *)"darkmagenta", RGB(0x8B, 0x00, 0x8B)},
      {(char_u *)"darkred", RGB(0x8B, 0x00, 0x00)},
      {(char_u *)"darkyellow", RGB(0x8B, 0x8B, 0x00)}, /* No X11 */
      {(char_u *)"gray", RGB(0xBE, 0xBE, 0xBE)},
      {(char_u *)"green", RGB(0x00, 0xFF, 0x00)},
      {(char_u *)"grey", RGB(0xBE, 0xBE, 0xBE)},
      {(char_u *)"grey40", RGB(0x66, 0x66, 0x66)},
      {(char_u *)"grey50", RGB(0x7F, 0x7F, 0x7F)},
      {(char_u *)"grey90", RGB(0xE5, 0xE5, 0xE5)},
      {(char_u *)"lightblue", RGB(0xAD, 0xD8, 0xE6)},
      {(char_u *)"lightcyan", RGB(0xE0, 0xFF, 0xFF)},
      {(char_u *)"lightgray", RGB(0xD3, 0xD3, 0xD3)},
      {(char_u *)"lightgreen", RGB(0x90, 0xEE, 0x90)},
      {(char_u *)"lightgrey", RGB(0xD3, 0xD3, 0xD3)},
      {(char_u *)"lightmagenta", RGB(0xFF, 0x8B, 0xFF)}, /* No X11 */
      {(char_u *)"lightred", RGB(0xFF, 0x8B, 0x8B)},     /* No X11 */
      {(char_u *)"lightyellow", RGB(0xFF, 0xFF, 0xE0)},
      {(char_u *)"magenta", RGB(0xFF, 0x00, 0xFF)},
      {(char_u *)"red", RGB(0xFF, 0x00, 0x00)},
      {(char_u *)"seagreen", RGB(0x2E, 0x8B, 0x57)},
      {(char_u *)"white", RGB(0xFF, 0xFF, 0xFF)},
      {(char_u *)"yellow", RGB(0xFF, 0xFF, 0x00)},
  };

  static struct rgbcolor_table_S *colornames_table;
  static int size = 0;

  if (name[0] == '#' && STRLEN(name) == 7)
  {
    /* Name is in "#rrggbb" format */
    color = RGB(((hex_digit(name[1]) << 4) + hex_digit(name[2])),
                ((hex_digit(name[3]) << 4) + hex_digit(name[4])),
                ((hex_digit(name[5]) << 4) + hex_digit(name[6])));
    if (color > 0xffffff)
      return INVALCOLOR;
    return gui_adjust_rgb(color);
  }

  /* Check if the name is one of the colors we know */
  for (i = 0; i < (int)(sizeof(rgb_table) / sizeof(rgb_table[0])); i++)
    if (STRICMP(name, rgb_table[i].color_name) == 0)
      return gui_adjust_rgb(rgb_table[i].color);

  /*
     * Last attempt. Look in the file "$VIMRUNTIME/rgb.txt".
     */
  if (size == 0)
  {
    int counting;

    // colornames_table not yet initialized
    fname = expand_env_save((char_u *)"$VIMRUNTIME/rgb.txt");
    if (fname == NULL)
      return INVALCOLOR;

    fd = fopen((char *)fname, "rt");
    vim_free(fname);
    if (fd == NULL)
    {
      if (p_verbose > 1)
        verb_msg(_("Cannot open $VIMRUNTIME/rgb.txt"));
      size = -1; // don't try again
      return INVALCOLOR;
    }

    for (counting = 1; counting >= 0; --counting)
    {
      if (!counting)
      {
        colornames_table = ALLOC_MULT(struct rgbcolor_table_S, size);
        if (colornames_table == NULL)
        {
          fclose(fd);
          return INVALCOLOR;
        }
        rewind(fd);
      }
      size = 0;

      while (!feof(fd))
      {
        size_t len;
        int pos;

        vim_ignoredp = fgets(line, LINE_LEN, fd);
        len = strlen(line);

        if (len <= 1 || line[len - 1] != '\n')
          continue;

        line[len - 1] = '\0';

        i = sscanf(line, "%d %d %d %n", &r, &g, &b, &pos);
        if (i != 3)
          continue;

        if (!counting)
        {
          char_u *s = vim_strsave((char_u *)line + pos);

          if (s == NULL)
          {
            fclose(fd);
            return INVALCOLOR;
          }
          colornames_table[size].color_name = s;
          colornames_table[size].color = (guicolor_T)RGB(r, g, b);
        }
        size++;

        // The distributed rgb.txt has less than 1000 entries. Limit to
        // 10000, just in case the file was messed up.
        if (size == 10000)
          break;
      }
    }
    fclose(fd);
  }

  for (i = 0; i < size; i++)
    if (STRICMP(name, colornames_table[i].color_name) == 0)
      return gui_adjust_rgb(colornames_table[i].color);

  return INVALCOLOR;
}

guicolor_T
gui_get_rgb_color_cmn(int r, int g, int b)
{
  guicolor_T color = RGB(r, g, b);

  if (color > 0xffffff)
    return INVALCOLOR;
  return gui_adjust_rgb(color);
}
#endif

#if defined(MSWIN) || defined(FEAT_TERMINAL) || defined(PROTO)
static int cube_value[] = {
    0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF};

static int grey_ramp[] = {
    0x08, 0x12, 0x1C, 0x26, 0x30, 0x3A, 0x44, 0x4E, 0x58, 0x62, 0x6C, 0x76,
    0x80, 0x8A, 0x94, 0x9E, 0xA8, 0xB2, 0xBC, 0xC6, 0xD0, 0xDA, 0xE4, 0xEE};

#ifdef FEAT_TERMINAL
#include "libvterm/include/vterm.h" // for VTERM_ANSI_INDEX_NONE
#else
#define VTERM_ANSI_INDEX_NONE 0
#endif

static char_u ansi_table[16][4] = {
    //   R    G    B   idx
    {0, 0, 0, 1},       // black
    {224, 0, 0, 2},     // dark red
    {0, 224, 0, 3},     // dark green
    {224, 224, 0, 4},   // dark yellow / brown
    {0, 0, 224, 5},     // dark blue
    {224, 0, 224, 6},   // dark magenta
    {0, 224, 224, 7},   // dark cyan
    {224, 224, 224, 8}, // light grey

    {128, 128, 128, 9},  // dark grey
    {255, 64, 64, 10},   // light red
    {64, 255, 64, 11},   // light green
    {255, 255, 64, 12},  // yellow
    {64, 64, 255, 13},   // light blue
    {255, 64, 255, 14},  // light magenta
    {64, 255, 255, 15},  // light cyan
    {255, 255, 255, 16}, // white
};

void cterm_color2rgb(int nr, char_u *r, char_u *g, char_u *b, char_u *ansi_idx)
{
  int idx;

  if (nr < 16)
  {
    *r = ansi_table[nr][0];
    *g = ansi_table[nr][1];
    *b = ansi_table[nr][2];
    *ansi_idx = ansi_table[nr][3];
  }
  else if (nr < 232)
  {
    /* 216 color cube */
    idx = nr - 16;
    *r = cube_value[idx / 36 % 6];
    *g = cube_value[idx / 6 % 6];
    *b = cube_value[idx % 6];
    *ansi_idx = VTERM_ANSI_INDEX_NONE;
  }
  else if (nr < 256)
  {
    /* 24 grey scale ramp */
    idx = nr - 232;
    *r = grey_ramp[idx];
    *g = grey_ramp[idx];
    *b = grey_ramp[idx];
    *ansi_idx = VTERM_ANSI_INDEX_NONE;
  }
  else
  {
    *r = 0;
    *g = 0;
    *b = 0;
    *ansi_idx = 0;
  }
}
#endif
