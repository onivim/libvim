/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * proto.h: include the (automatically generated) function prototypes
 */

/*
 * Don't include these while generating prototypes.  Prevents problems when
 * files are missing.
 */
#if !defined(PROTO) && !defined(NOPROTO)

/*
 * Machine-dependent routines.
 */
/* avoid errors in function prototypes */
# if !defined(FEAT_X11) && !defined(FEAT_GUI_GTK)
#  define Display int
#  define Widget int
# endif
# ifndef FEAT_GUI_GTK
#  define GdkEvent int
#  define GdkEventKey int
# endif
# ifndef FEAT_X11
#  define XImage int
# endif

# ifdef AMIGA
#  include "os_amiga.pro"
# endif
# if defined(UNIX) || defined(VMS)
#  include "os_unix.pro"
# endif
# ifdef MSWIN
#  include "os_win32.pro"
#  include "os_mswin.pro"
#  include "winclip.pro"
#  if (defined(__GNUC__) && !defined(__MINGW32__))
extern int _stricoll(char *a, char *b);
#  endif
# endif
# ifdef VMS
#  include "os_vms.pro"
# endif
# ifdef __BEOS__
#  include "os_beos.pro"
# endif

# include "autocmd.pro"
# include "buffer.pro"
# include "change.pro"
# include "charset.pro"
# include "debugger.pro"
# include "dict.pro"
# include "diff.pro"
# include "digraph.pro"
# include "edit.pro"
# include "eval.pro"
# include "evalfunc.pro"
# include "ex_cmds.pro"
# include "ex_cmds2.pro"
# include "ex_docmd.pro"
# include "ex_eval.pro"
# include "ex_getln.pro"
# include "fileio.pro"
# include "findfile.pro"
# include "fold.pro"
# include "getchar.pro"
# ifdef FEAT_HANGULIN
#  include "hangulin.pro"
# endif
# include "hashtab.pro"
# include "indent.pro"
# ifdef FEAT_INS_EXPAND
# include "insexpand.pro"
# endif
# include "json.pro"
# include "list.pro"
# include "blob.pro"
# include "main.pro"
# include "mark.pro"
# include "memfile.pro"
# include "memline.pro"
# ifdef FEAT_ARABIC
#  include "arabic.pro"
# endif

/* These prototypes cannot be produced automatically. */
int smsg(const char *, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 1, 0)))
#endif
    ;

int smsg_attr(int, const char *, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 2, 3)))
#endif
    ;

int smsg_attr_keep(int, const char *, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* These prototypes cannot be produced automatically. */
int semsg(const char *, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 1, 0)))
#endif
    ;

/* These prototypes cannot be produced automatically. */
void siemsg(const char *, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 1, 0)))
#endif
    ;

int vim_snprintf_add(char *, size_t, const char *, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 3, 4)))
#endif
    ;

int vim_snprintf(char *, size_t, const char *, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 3, 4)))
#endif
    ;

int vim_vsnprintf(char *str, size_t str_m, const char *fmt, va_list ap);
int vim_vsnprintf_typval(char *str, size_t str_m, const char *fmt, va_list ap, typval_T *tvs);

# include "message.pro"
# include "misc1.pro"
# include "misc2.pro"
#ifndef HAVE_STRPBRK	    /* not generated automatically from misc2.c */
char_u *vim_strpbrk(char_u *s, char_u *charset);
#endif
#ifndef HAVE_QSORT
/* Use our own qsort(), don't define the prototype when not used. */
void qsort(void *base, size_t elm_count, size_t elm_size, int (*cmp)(const void *, const void *));
#endif
# include "move.pro"
# include "mbyte.pro"
# include "normal.pro"
# include "ops.pro"
# include "option.pro"
# include "popupmnu.pro"
# ifdef FEAT_QUICKFIX
#  include "quickfix.pro"
# endif
# include "regexp.pro"
# include "screen.pro"
# if defined(FEAT_PERSISTENT_UNDO)
#  include "sha256.pro"
# endif
# include "search.pro"
# ifdef FEAT_SIGNS
#  include "sign.pro"
# endif
# include "state_machine.pro"
# include "syntax.pro"
# include "tag.pro"
# include "term.pro"
# ifdef FEAT_TERMINAL
#  include "terminal.pro"
# endif
# if defined(HAVE_TGETENT) && (defined(AMIGA) || defined(VMS))
#  include "termlib.pro"
# endif
# ifdef FEAT_TEXT_PROP
#  include "popupwin.pro"
#  include "textprop.pro"
# endif
# include "ui.pro"
# include "undo.pro"
# include "usercmd.pro"
# include "userfunc.pro"
# include "version.pro"
# include "window.pro"

# ifdef FEAT_LUA
#  include "if_lua.pro"
# endif

# ifdef FEAT_MZSCHEME
#  include "if_mzsch.pro"
# endif

# ifdef FEAT_PYTHON
#  include "if_python.pro"
# endif

# ifdef FEAT_PYTHON3
#  include "if_python3.pro"
# endif

# ifdef FEAT_RUBY
#  include "if_ruby.pro"
# endif

/* Ugly solution for "BalloonEval" not being defined while it's used in some
 * .pro files. */
#  define BalloonEval int

# ifdef FEAT_JOB_CHANNEL
#  include "channel.pro"

/* Not generated automatically, to add extra attribute. */
void ch_log(channel_T *ch, const char *fmt, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 2, 3)))
#endif
    ;

# endif

# if defined(FEAT_GUI) || defined(FEAT_JOB_CHANNEL)
#  if defined(UNIX) || defined(MACOS_X) || defined(VMS)
#   include "pty.pro"
#  endif
# endif

# ifdef FEAT_GUI
#  include "gui.pro"
#  if !defined(HAVE_SETENV) && !defined(HAVE_PUTENV) && !defined(VMS)
extern int putenv(const char *string);			/* in misc2.c */
#   ifdef USE_VIMPTY_GETENV
extern char_u *vimpty_getenv(const char_u *string);	/* in misc2.c */
#   endif
#  endif
#  ifdef FEAT_GUI_MSWIN
#   include "gui_w32.pro"
#  endif
#  ifdef FEAT_GUI_GTK
#   include "gui_gtk.pro"
#   include "gui_gtk_x11.pro"
#  endif
#  ifdef FEAT_GUI_MOTIF
#   include "gui_motif.pro"
#   include "gui_xmdlg.pro"
#  endif
#  ifdef FEAT_GUI_ATHENA
#   include "gui_athena.pro"
#   ifdef FEAT_BROWSE
extern char *vim_SelFile(Widget toplevel, char *prompt, char *init_path, int (*show_entry)(), int x, int y, guicolor_T fg, guicolor_T bg, guicolor_T scroll_fg, guicolor_T scroll_bg);
#   endif
#  endif
#  ifdef FEAT_GUI_MAC
#   include "gui_mac.pro"
#  endif
#  ifdef FEAT_GUI_X11
#   include "gui_x11.pro"
#  endif
#  ifdef FEAT_GUI_PHOTON
#   include "gui_photon.pro"
#  endif
# endif	/* FEAT_GUI */

# ifdef FEAT_OLE
#  include "if_ole.pro"
# endif
# if defined(FEAT_CLIENTSERVER) && defined(FEAT_X11)
#  include "if_xcmdsrv.pro"
# endif

/*
 * The perl include files pollute the namespace, therefore proto.h must be
 * included before the perl include files.  But then CV is not defined, which
 * is used in if_perl.pro.  To get around this, the perl prototype files are
 * not included here for the perl files.  Use a dummy define for CV for the
 * other files.
 */
#if defined(FEAT_PERL) && !defined(IN_PERL_FILE)
# define CV void
# include "if_perl.pro"
# include "if_perlsfio.pro"
#endif

#ifdef MACOS_CONVERT
# include "os_mac_conv.pro"
#endif
#if defined(MACOS_X_DARWIN) && defined(FEAT_CLIPBOARD) && !defined(FEAT_GUI)
/* functions in os_macosx.m */
void clip_mch_lose_selection(VimClipboard *cbd);
int clip_mch_own_selection(VimClipboard *cbd);
void clip_mch_request_selection(VimClipboard *cbd);
void clip_mch_set_selection(VimClipboard *cbd);
#endif
#endif /* !PROTO && !NOPROTO */
