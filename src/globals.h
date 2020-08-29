/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * definition of global variables
 */

/*
 * Number of Rows and Columns in the screen.
 * Must be long to be able to use them as options in option.c.
 * Note: Use screen_Rows and screen_Columns to access items in ScreenLines[].
 * They may have different values when the screen wasn't (re)allocated yet
 * after setting Rows or Columns (e.g., when starting up).
 */
EXTERN long Rows /* nr of rows in the screen */
#ifdef DO_INIT
#if defined(MSWIN)
    = 25L
#else
    = 24L
#endif
#endif
    ;
EXTERN long Columns INIT(= 80); /* nr of columns in the screen */

/*
 * The characters that are currently on the screen are kept in ScreenLines[].
 * It is a single block of characters, the size of the screen plus one line.
 * The attributes for those characters are kept in ScreenAttrs[].
 *
 * "LineOffset[n]" is the offset from ScreenLines[] for the start of line 'n'.
 * The same value is used for ScreenLinesUC[] and ScreenAttrs[].
 *
 * Note: before the screen is initialized and when out of memory these can be
 * NULL.
 */
EXTERN schar_T *ScreenLines INIT(= NULL);
EXTERN sattr_T *ScreenAttrs INIT(= NULL);
EXTERN unsigned *LineOffset INIT(= NULL);
EXTERN char_u *LineWraps INIT(= NULL); /* line wraps to next line */

/* libvim API callbacks */
EXTERN AutoCommandCallback autoCommandCallback INIT(= NULL);
EXTERN BufferUpdateCallback bufferUpdateCallback INIT(= NULL);
EXTERN ClipboardGetCallback clipboardGetCallback INIT(= NULL);
EXTERN FileWriteFailureCallback fileWriteFailureCallback INIT(= NULL);
EXTERN DirectoryChangedCallback directoryChangedCallback INIT(= NULL);
EXTERN FormatCallback formatCallback INIT(= NULL);
EXTERN GotoCallback gotoCallback INIT(= NULL);
EXTERN TabPageCallback tabPageCallback INIT(= NULL);
EXTERN VoidCallback displayIntroCallback INIT(= NULL);
EXTERN VoidCallback displayVersionCallback INIT(= NULL);
EXTERN AutoIndentCallback autoIndentCallback INIT(= NULL);
EXTERN ColorSchemeChangedCallback colorSchemeChangedCallback INIT(= NULL);
EXTERN ColorSchemeCompletionCallback colorSchemeCompletionCallback INIT(= NULL);
EXTERN MessageCallback messageCallback INIT(= NULL);
EXTERN MacroStartRecordCallback macroStartRecordCallback INIT(= NULL);
EXTERN MacroStopRecordCallback macroStopRecordCallback INIT(= NULL);
EXTERN OptionSetCallback optionSetCallback INIT(= NULL);
EXTERN QuitCallback quitCallback INIT(= NULL);
EXTERN TerminalCallback terminalCallback INIT(= NULL);
EXTERN VoidCallback stopSearchHighlightCallback INIT(= NULL);
EXTERN VoidCallback unhandledEscapeCallback INIT(= NULL);
EXTERN WindowSplitCallback windowSplitCallback INIT(= NULL);
EXTERN WindowMovementCallback windowMovementCallback INIT(= NULL);
EXTERN YankCallback yankCallback INIT(= NULL);

/*
 * Globals for managing the state machine
 */

EXTERN sm_T *state_current INIT(= NULL);

/*
 * When using Unicode characters (in UTF-8 encoding) the character in
 * ScreenLinesUC[] contains the Unicode for the character at this position, or
 * NUL when the character in ScreenLines[] is to be used (ASCII char).
 * The composing characters are to be drawn on top of the original character.
 * ScreenLinesC[0][off] is only to be used when ScreenLinesUC[off] != 0.
 * Note: These three are only allocated when enc_utf8 is set!
 */
EXTERN u8char_T *ScreenLinesUC INIT(= NULL); /* decoded UTF-8 characters */
EXTERN u8char_T *ScreenLinesC[MAX_MCO];      /* composing characters */
EXTERN int Screen_mco INIT(= 0);             /* value of p_mco used when
						   allocating ScreenLinesC[] */

/* Only used for euc-jp: Second byte of a character that starts with 0x8e.
 * These are single-width. */
EXTERN schar_T *ScreenLines2 INIT(= NULL);

/*
 * Indexes for tab page line:
 *	N > 0 for label of tab page N
 *	N == 0 for no label
 *	N < 0 for closing tab page -N
 *	N == -999 for closing current tab page
 */
EXTERN short *TabPageIdxs INIT(= NULL);

EXTERN int screen_Rows INIT(= 0);    /* actual size of ScreenLines[] */
EXTERN int screen_Columns INIT(= 0); /* actual size of ScreenLines[] */

/*
 * When vgetc() is called, it sets mod_mask to the set of modifiers that are
 * held down based on the MOD_MASK_* symbols that are read first.
 */
EXTERN int mod_mask INIT(= 0x0); /* current key modifiers */

/*
 * Cmdline_row is the row where the command line starts, just below the
 * last window.
 * When the cmdline gets longer than the available space the screen gets
 * scrolled up. After a CTRL-D (show matches), after hitting ':' after
 * "hit return", and for the :global command, the command line is
 * temporarily moved.  The old position is restored with the next call to
 * update_screen().
 */
EXTERN int cmdline_row;

EXTERN int redraw_cmdline INIT(= FALSE);      // cmdline must be redrawn
EXTERN int redraw_mode INIT(= FALSE);         // mode must be redrawn
EXTERN int clear_cmdline INIT(= FALSE);       // cmdline must be cleared
EXTERN int mode_displayed INIT(= FALSE);      // mode is being displayed
EXTERN int no_win_do_lines_ins INIT(= FALSE); // don't insert lines
#ifdef FEAT_EVAL
EXTERN int cmdline_star INIT(= FALSE); // cmdline is crypted
#endif

// The current cmdline_info.  It is initialized in getcmdline() and after that
// used by other functions.  When invoking getcmdline() recursively it needs
// to be saved with save_cmdline() and restored with restore_cmdline().
EXTERN struct cmdline_info ccline;

EXTERN int exec_from_reg INIT(= FALSE); /* executing register */

EXTERN int screen_cleared INIT(= FALSE); /* screen has been cleared */

/*
 * When '$' is included in 'cpoptions' option set:
 * When a change command is given that deletes only part of a line, a dollar
 * is put at the end of the changed text. dollar_vcol is set to the virtual
 * column of this '$'.  -1 is used to indicate no $ is being displayed.
 */
EXTERN colnr_T dollar_vcol INIT(= -1);

/*
 * Functions for putting characters in the command line,
 * while keeping ScreenLines[] updated.
 */
#ifdef FEAT_RIGHTLEFT
EXTERN int cmdmsg_rl INIT(= FALSE); /* cmdline is drawn right to left */
#endif
EXTERN int msg_col;
EXTERN int msg_row;
EXTERN int msg_scrolled; /* Number of screen lines that windows have
				 * scrolled because of printing messages. */
EXTERN int msg_scrolled_ign INIT(= FALSE);
/* when TRUE don't set need_wait_return in
				   msg_puts_attr() when msg_scrolled is
				   non-zero */

EXTERN char_u *keep_msg INIT(= NULL);   /* msg to be shown after redraw */
EXTERN int keep_msg_attr INIT(= 0);     /* highlight attr for keep_msg */
EXTERN int keep_msg_more INIT(= FALSE); /* keep_msg was set by msgmore() */
EXTERN int need_fileinfo INIT(= FALSE); /* do fileinfo() after redraw */
EXTERN int msg_scroll INIT(= FALSE);    /* msg_start() will scroll */
EXTERN int msg_didout INIT(= FALSE);    /* msg_outstr() was used in line */
EXTERN int msg_didany INIT(= FALSE);    /* msg_outstr() was used at all */
EXTERN int msg_nowait INIT(= FALSE);    /* don't wait for this msg */
EXTERN int emsg_off INIT(= 0);          /* don't display errors for now,
					       unless 'debug' is set. */
EXTERN int info_message INIT(= FALSE);  /* printing informative message */
EXTERN int msg_hist_off INIT(= FALSE);  /* don't add messages to history */
#ifdef FEAT_EVAL
EXTERN int need_clr_eos INIT(= FALSE); /* need to clear text before
					       displaying a message. */
EXTERN int emsg_skip INIT(= 0);        /* don't display errors for
					       expression that is skipped */
EXTERN int emsg_severe INIT(= FALSE);  /* use message of next of several
					       emsg() calls for throw */
EXTERN int did_endif INIT(= FALSE);    /* just had ":endif" */
EXTERN dict_T vimvardict;              /* Dictionary with v: variables */
EXTERN dict_T globvardict;             /* Dictionary with g: variables */
#endif
EXTERN int did_emsg; /* set by emsg() when the message
					       is displayed or thrown */
#ifdef FEAT_EVAL
EXTERN int called_vim_beep;   /* set if vim_beep() is called */
EXTERN int did_uncaught_emsg; /* emsg() was called and did not
					       cause an exception */
#endif
EXTERN int did_emsg_syntax;               /* did_emsg set because of a
					       syntax error */
EXTERN int called_emsg;                   /* always set by emsg() */
EXTERN int ex_exitval INIT(= 0);          /* exit value for ex mode */
EXTERN int emsg_on_display INIT(= FALSE); /* there is an error message */
EXTERN int rc_did_emsg INIT(= FALSE);     /* vim_regcomp() called emsg() */

EXTERN int no_wait_return INIT(= 0);      /* don't wait for return for now */
EXTERN int need_wait_return INIT(= 0);    /* need to wait for return later */
EXTERN int did_wait_return INIT(= FALSE); /* wait_return() was used and
						   nothing written since then */

EXTERN int quit_more INIT(= FALSE); /* 'q' hit at "--more--" msg */
#if defined(UNIX) || defined(VMS) || defined(MACOS_X)
EXTERN int newline_on_exit INIT(= FALSE); /* did msg in altern. screen */
EXTERN int intr_char INIT(= 0);           /* extra interrupt character */
#endif
#if (defined(UNIX) || defined(VMS)) && defined(FEAT_X11)
EXTERN int x_no_connect INIT(= FALSE); /* don't connect to X server */
#endif
EXTERN int ex_keep_indent INIT(= FALSE); /* getexmodeline(): keep indent */
EXTERN int vgetc_busy INIT(= 0);         /* when inside vgetc() then > 0 */

EXTERN int didset_vim INIT(= FALSE);        /* did set $VIM ourselves */
EXTERN int didset_vimruntime INIT(= FALSE); /* idem for $VIMRUNTIME */

/*
 * Lines left before a "more" message.	Ex mode needs to be able to reset this
 * after you type something.
 */
EXTERN int lines_left INIT(= -1);     /* lines left for listing */
EXTERN int msg_no_more INIT(= FALSE); /* don't use more prompt, truncate
					       messages */

EXTERN char_u *sourcing_name INIT(= NULL); /* name of error message source */
EXTERN linenr_T sourcing_lnum INIT(= 0);   /* line number of the source file */

#ifdef FEAT_EVAL
EXTERN int ex_nesting_level INIT(= 0);      /* nesting level */
EXTERN int debug_break_level INIT(= -1);    /* break below this level */
EXTERN int debug_did_msg INIT(= FALSE);     /* did "debug mode" message */
EXTERN int debug_tick INIT(= 0);            /* breakpoint change count */
EXTERN int debug_backtrace_level INIT(= 0); /* breakpoint backtrace level */
#ifdef FEAT_PROFILE
EXTERN int do_profiling INIT(= PROF_NONE); /* PROF_ values */
#endif

/*
 * The exception currently being thrown.  Used to pass an exception to
 * a different cstack.  Also used for discarding an exception before it is
 * caught or made pending.  Only valid when did_throw is TRUE.
 */
EXTERN except_T *current_exception;

/*
 * did_throw: An exception is being thrown.  Reset when the exception is caught
 * or as long as it is pending in a finally clause.
 */
EXTERN int did_throw INIT(= FALSE);

/*
 * need_rethrow: set to TRUE when a throw that cannot be handled in do_cmdline()
 * must be propagated to the cstack of the previously called do_cmdline().
 */
EXTERN int need_rethrow INIT(= FALSE);

/*
 * check_cstack: set to TRUE when a ":finish" or ":return" that cannot be
 * handled in do_cmdline() must be propagated to the cstack of the previously
 * called do_cmdline().
 */
EXTERN int check_cstack INIT(= FALSE);

/*
 * Number of nested try conditionals (across function calls and ":source"
 * commands).
 */
EXTERN int trylevel INIT(= 0);

/*
 * When "force_abort" is TRUE, always skip commands after an error message,
 * even after the outermost ":endif", ":endwhile" or ":endfor" or for a
 * function without the "abort" flag.  It is set to TRUE when "trylevel" is
 * non-zero (and ":silent!" was not used) or an exception is being thrown at
 * the time an error is detected.  It is set to FALSE when "trylevel" gets
 * zero again and there was no error or interrupt or throw.
 */
EXTERN int force_abort INIT(= FALSE);

/*
 * "msg_list" points to a variable in the stack of do_cmdline() which keeps
 * the list of arguments of several emsg() calls, one of which is to be
 * converted to an error exception immediately after the failing command
 * returns.  The message to be used for the exception value is pointed to by
 * the "throw_msg" field of the first element in the list.  It is usually the
 * same as the "msg" field of that element, but can be identical to the "msg"
 * field of a later list element, when the "emsg_severe" flag was set when the
 * emsg() call was made.
 */
EXTERN struct msglist **msg_list INIT(= NULL);

/*
 * suppress_errthrow: When TRUE, don't convert an error to an exception.  Used
 * when displaying the interrupt message or reporting an exception that is still
 * uncaught at the top level (which has already been discarded then).  Also used
 * for the error message when no exception can be thrown.
 */
EXTERN int suppress_errthrow INIT(= FALSE);

/*
 * The stack of all caught and not finished exceptions.  The exception on the
 * top of the stack is the one got by evaluation of v:exception.  The complete
 * stack of all caught and pending exceptions is embedded in the various
 * cstacks; the pending exceptions, however, are not on the caught stack.
 */
EXTERN except_T *caught_stack INIT(= NULL);

#endif

#ifdef FEAT_EVAL
/*
 * Garbage collection can only take place when we are sure there are no Lists
 * or Dictionaries being used internally.  This is flagged with
 * "may_garbage_collect" when we are at the toplevel.
 * "want_garbage_collect" is set by the garbagecollect() function, which means
 * we do garbage collection before waiting for a char at the toplevel.
 * "garbage_collect_at_exit" indicates garbagecollect(1) was called.
 */
EXTERN int may_garbage_collect INIT(= FALSE);
EXTERN int want_garbage_collect INIT(= FALSE);
EXTERN int garbage_collect_at_exit INIT(= FALSE);

// Script CTX being sourced or was sourced to define the current function.
EXTERN sctx_T current_sctx INIT(= {0 COMMA 0 COMMA 0 COMMA 0});
#endif

EXTERN int did_source_packages INIT(= FALSE);

/* Magic number used for hashitem "hi_key" value indicating a deleted item.
 * Only the address is used. */
EXTERN char_u hash_removed;

EXTERN int scroll_region INIT(= FALSE); /* term supports scroll region */
EXTERN int t_colors INIT(= 0);          /* int value of T_CCO */

/*
 * When highlight_match is TRUE, highlight a match, starting at the cursor
 * position.  Search_match_lines is the number of lines after the match (0 for
 * a match within one line), search_match_endcol the column number of the
 * character just after the match in the last line.
 */
EXTERN int highlight_match INIT(= FALSE); // show search match pos
EXTERN linenr_T search_match_lines;       // lines of of matched string
EXTERN colnr_T search_match_endcol;       // col nr of match end
#ifdef FEAT_SEARCH_EXTRA
EXTERN linenr_T search_first_line INIT(= 0);      // for :{FIRST},{last}s/pat
EXTERN linenr_T search_last_line INIT(= MAXLNUM); // for :{first},{LAST}s/pat
#endif

EXTERN int no_smartcase INIT(= FALSE); /* don't use 'smartcase' once */

EXTERN int need_check_timestamps INIT(= FALSE); /* need to check file
							timestamps asap */
EXTERN int did_check_timestamps INIT(= FALSE);  /* did check timestamps
						       recently */
EXTERN int no_check_timestamps INIT(= 0);       /* Don't check timestamps */

EXTERN int highlight_attr[HLF_COUNT]; /* Highl. attr for each context. */
#ifdef USER_HIGHLIGHT
EXTERN int highlight_user[9]; /* User[1-9] attributes */
#endif
#ifdef FEAT_TERMINAL
// When TRUE skip calling terminal_loop() once.  Used when
// typing ':' at the more prompt.
EXTERN int skip_term_loop INIT(= FALSE);
#endif
EXTERN int cterm_normal_fg_color INIT(= 0);
EXTERN int cterm_normal_fg_bold INIT(= 0);
EXTERN int cterm_normal_bg_color INIT(= 0);

EXTERN int autocmd_busy INIT(= FALSE);     /* Is apply_autocmds() busy? */
EXTERN int autocmd_no_enter INIT(= FALSE); /* *Enter autocmds disabled */
EXTERN int autocmd_no_leave INIT(= FALSE); /* *Leave autocmds disabled */
EXTERN int modified_was_set;               /* did ":set modified" */
EXTERN int did_filetype INIT(= FALSE);     /* FileType event found */
EXTERN int au_did_filetype INIT(= FALSE);
EXTERN int keep_filetype INIT(= FALSE); /* value for did_filetype when
						   starting to execute
						   autocommands */

/* When deleting the current buffer, another one must be loaded.  If we know
 * which one is preferred, au_new_curbuf is set to it */
EXTERN bufref_T au_new_curbuf INIT(= {NULL COMMA 0 COMMA 0});

/* When deleting a buffer/window and autocmd_busy is TRUE, do not free the
 * buffer/window. but link it in the list starting with
 * au_pending_free_buf/ap_pending_free_win, using b_next/w_next.
 * Free the buffer/window when autocmd_busy is being set to FALSE. */
EXTERN buf_T *au_pending_free_buf INIT(= NULL);
EXTERN win_T *au_pending_free_win INIT(= NULL);

#ifdef FEAT_DIFF
/* Value set from 'diffopt'. */
EXTERN int diff_context INIT(= 6);    /* context for folds */
EXTERN int diff_foldcolumn INIT(= 2); /* 'foldcolumn' for diff mode */
EXTERN int diff_need_scrollbind INIT(= FALSE);
#endif

/* While redrawing the screen this flag is set.  It means the screen size
 * ('lines' and 'rows') must not be changed. */
EXTERN int updating_screen INIT(= FALSE);

#define CLIP_UNNAMED 1
#define CLIP_UNNAMED_PLUS 2
EXTERN int clip_unnamed INIT(= 0);       /* above two values or'ed */
EXTERN int clip_unnamed_saved INIT(= 0); /* above two values or'ed */

/*
 * All regular windows are linked in a list. "firstwin" points to the first
 * entry, "lastwin" to the last entry (can be the same as firstwin) and
 * "curwin" to the currently active window.
 * When switching tabs these swapped with the pointers in "tabpage_T".
 */
EXTERN win_T *firstwin;             /* first window */
EXTERN win_T *lastwin;              /* last window */
EXTERN win_T *prevwin INIT(= NULL); /* previous window */
#define ONE_WINDOW (firstwin == lastwin)
#define W_NEXT(wp) ((wp)->w_next)
#define FOR_ALL_WINDOWS(wp) for (wp = firstwin; wp != NULL; wp = wp->w_next)
#define FOR_ALL_FRAMES(frp, first_frame) \
  for (frp = first_frame; frp != NULL; frp = frp->fr_next)
#define FOR_ALL_TABPAGES(tp) for (tp = first_tabpage; tp != NULL; tp = tp->tp_next)
#define FOR_ALL_WINDOWS_IN_TAB(tp, wp)         \
  for ((wp) = ((tp) == NULL || (tp) == curtab) \
                  ? firstwin                   \
                  : (tp)->tp_firstwin;         \
       (wp); (wp) = (wp)->w_next)
/*
 * When using this macro "break" only breaks out of the inner loop. Use "goto"
 * to break out of the tabpage loop.
 */
#define FOR_ALL_TAB_WINDOWS(tp, wp)                              \
  for ((tp) = first_tabpage; (tp) != NULL; (tp) = (tp)->tp_next) \
    for ((wp) = ((tp) == curtab)                                 \
                    ? firstwin                                   \
                    : (tp)->tp_firstwin;                         \
         (wp); (wp) = (wp)->w_next)

EXTERN win_T *curwin; /* currently active window */

EXTERN win_T *aucmd_win;                 /* window used in aucmd_prepbuf() */
EXTERN int aucmd_win_used INIT(= FALSE); /* aucmd_win is being used */

/*
 * The window layout is kept in a tree of frames.  topframe points to the top
 * of the tree.
 */
EXTERN frame_T *topframe; /* top of the window frame tree */

/*
 * Tab pages are alternative topframes.  "first_tabpage" points to the first
 * one in the list, "curtab" is the current one.
 */
EXTERN tabpage_T *first_tabpage;
EXTERN tabpage_T *curtab;
EXTERN int redraw_tabline INIT(= FALSE); /* need to redraw tabline */

/*
 * All buffers are linked in a list. 'firstbuf' points to the first entry,
 * 'lastbuf' to the last entry and 'curbuf' to the currently active buffer.
 */
EXTERN buf_T *firstbuf INIT(= NULL); /* first buffer */
EXTERN buf_T *lastbuf INIT(= NULL);  /* last buffer */
EXTERN buf_T *curbuf INIT(= NULL);   /* currently active buffer */

#define FOR_ALL_BUFFERS(buf) for (buf = firstbuf; buf != NULL; buf = buf->b_next)

// Iterate through all the signs placed in a buffer
#define FOR_ALL_SIGNS_IN_BUF(buf, sign) \
  for (sign = buf->b_signlist; sign != NULL; sign = sign->next)

/* Flag that is set when switching off 'swapfile'.  It means that all blocks
 * are to be loaded into memory.  Shouldn't be global... */
EXTERN int mf_dont_release INIT(= FALSE); /* don't release blocks */

/*
 * List of files being edited (global argument list).  curwin->w_alist points
 * to this when the window is using the global argument list.
 */
EXTERN alist_T global_alist;           /* global argument list */
EXTERN int max_alist_id INIT(= 0);     /* the previous argument list id */
EXTERN int arg_had_last INIT(= FALSE); /* accessed last file in
					       global_alist */

EXTERN int ru_col; /* column for ruler */
EXTERN int sc_col; /* column for shown command */

#ifdef TEMPDIRNAMES
EXTERN char_u *vim_tempdir INIT(= NULL); /* Name of Vim's own temp dir.
					      Ends in a slash. */
#endif

/*
 * When starting or exiting some things are done differently (e.g. screen
 * updating).
 */
EXTERN int starting INIT(= NO_SCREEN);
/* first NO_SCREEN, then NO_BUFFERS and then
				 * set to 0 when starting up finished */
EXTERN int exiting INIT(= FALSE);
/* TRUE when planning to exit Vim.  Might
				 * still keep on running if there is a changed
				 * buffer. */
EXTERN int really_exiting INIT(= FALSE);
/* TRUE when we are sure to exit, e.g., after
				 * a deadly signal */
EXTERN int v_dying INIT(= 0);          /* internal value of v:dying */
EXTERN int stdout_isatty INIT(= TRUE); /* is stdout a terminal? */

#if defined(FEAT_AUTOCHDIR)
EXTERN int test_autochdir INIT(= FALSE);
#endif
#if defined(EXITFREE)
EXTERN int entered_free_all_mem INIT(= FALSE);
/* TRUE when in or after free_all_mem() */
#endif
/* volatile because it is used in signal handler deathtrap(). */
EXTERN volatile sig_atomic_t full_screen INIT(= FALSE);
/* TRUE when doing full-screen output
				 * otherwise only writing some messages */

EXTERN int restricted INIT(= FALSE);
/* TRUE when started as "rvim" */
EXTERN int secure INIT(= FALSE);
/* non-zero when only "safe" commands are
				 * allowed, e.g. when sourcing .exrc or .vimrc
				 * in current directory */

EXTERN int textlock INIT(= 0);
/* non-zero when changing text and jumping to
				 * another window or buffer is not allowed */

EXTERN int curbuf_lock INIT(= 0);
/* non-zero when the current buffer can't be
				 * changed.  Used for FileChangedRO. */
EXTERN int allbuf_lock INIT(= 0);
/* non-zero when no buffer name can be
				 * changed, no buffer can be deleted and
				 * current directory can't be changed.
				 * Used for SwapExists et al. */
#ifdef HAVE_SANDBOX
EXTERN int sandbox INIT(= 0);
/* Non-zero when evaluating an expression in a
				 * "sandbox".  Several things are not allowed
				 * then. */
#endif

EXTERN int silent_mode INIT(= FALSE);
/* set to TRUE when "-s" commandline argument
				 * used for ex */

EXTERN pos_T VIsual; /* start position of active Visual selection */
EXTERN int VIsual_active INIT(= FALSE);
/* whether Visual mode is active */
EXTERN int VIsual_select INIT(= FALSE);
/* whether Select mode is active */
EXTERN int VIsual_reselect;
/* whether to restart the selection after a
				 * Select mode mapping or menu */

EXTERN int VIsual_mode INIT(= 'v');
/* type of Visual mode */

EXTERN int redo_VIsual_busy INIT(= FALSE);
/* TRUE when redoing Visual */

/*
 * This flag is used to make auto-indent work right on lines where only a
 * <RETURN> or <ESC> is typed. It is set when an auto-indent is done, and
 * reset when any other editing is done on the line. If an <ESC> or <RETURN>
 * is received, and did_ai is TRUE, the line is truncated.
 */
EXTERN int did_ai INIT(= FALSE);

/*
 * Column of first char after autoindent.  0 when no autoindent done.  Used
 * when 'backspace' is 0, to avoid backspacing over autoindent.
 */
EXTERN colnr_T ai_col INIT(= 0);

#ifdef FEAT_COMMENTS
/*
 * This is a character which will end a start-middle-end comment when typed as
 * the first character on a new line.  It is taken from the last character of
 * the "end" comment leader when the COM_AUTO_END flag is given for that
 * comment end in 'comments'.  It is only valid when did_ai is TRUE.
 */
EXTERN int end_comment_pending INIT(= NUL);
#endif

/*
 * This flag is set after a ":syncbind" to let the check_scrollbind() function
 * know that it should not attempt to perform scrollbinding due to the scroll
 * that was a result of the ":syncbind." (Otherwise, check_scrollbind() will
 * undo some of the work done by ":syncbind.")  -ralston
 */
EXTERN int did_syncbind INIT(= FALSE);

#ifdef FEAT_SMARTINDENT
/*
 * This flag is set when a smart indent has been performed. When the next typed
 * character is a '{' the inserted tab will be deleted again.
 */
EXTERN int did_si INIT(= FALSE);

/*
 * This flag is set after an auto indent. If the next typed character is a '}'
 * one indent will be removed.
 */
EXTERN int can_si INIT(= FALSE);

/*
 * This flag is set after an "O" command. If the next typed character is a '{'
 * one indent will be removed.
 */
EXTERN int can_si_back INIT(= FALSE);
#endif

EXTERN pos_T saved_cursor /* w_cursor before formatting text. */
#ifdef DO_INIT
    = {0, 0, 0}
#endif
;

/*
 * Stuff for insert mode.
 */
EXTERN pos_T Insstart; /* This is where the latest
					 * insert/append mode started. */

/* This is where the latest insert/append mode started. In contrast to
 * Insstart, this won't be reset by certain keys and is needed for
 * op_insert(), to detect correctly where inserting by the user started. */
EXTERN pos_T Insstart_orig;

/*
 * Stuff for VREPLACE mode.
 */
EXTERN int orig_line_count INIT(= 0);  /* Line count when "gR" started */
EXTERN int vr_lines_changed INIT(= 0); /* #Lines changed by "gR" so far */

#if defined(FEAT_X11) && defined(FEAT_XCLIPBOARD)
/* argument to SETJMP() for handling X IO errors */
EXTERN JMP_BUF x_jump_env;
#endif

/*
 * These flags are set based upon 'fileencoding'.
 * Note that "enc_utf8" is also set for "unicode", because the characters are
 * internally stored as UTF-8 (to avoid trouble with NUL bytes).
 */
#define DBCS_JPN 932   /* japan */
#define DBCS_JPNU 9932 /* euc-jp */
#define DBCS_KOR 949   /* korea */
#define DBCS_KORU 9949 /* euc-kr */
#define DBCS_CHS 936   /* chinese */
#define DBCS_CHSU 9936 /* euc-cn */
#define DBCS_CHT 950   /* taiwan */
#define DBCS_CHTU 9950 /* euc-tw */
#define DBCS_2BYTE 1   /* 2byte- */
#define DBCS_DEBUG -1

EXTERN int enc_dbcs INIT(= 0);          /* One of DBCS_xxx values if
						   DBCS encoding */
EXTERN int enc_unicode INIT(= 0);       /* 2: UCS-2 or UTF-16, 4: UCS-4 */
EXTERN int enc_utf8 INIT(= FALSE);      /* UTF-8 encoded Unicode */
EXTERN int enc_latin1like INIT(= TRUE); /* 'encoding' is latin1 comp. */
#if defined(MSWIN) || defined(FEAT_CYGWIN_WIN32_CLIPBOARD)
/* Codepage nr of 'encoding'.  Negative means it's not been set yet, zero
 * means 'encoding' is not a valid codepage. */
EXTERN int enc_codepage INIT(= -1);
EXTERN int enc_latin9 INIT(= FALSE); /* 'encoding' is latin9 */
#endif
EXTERN int has_mbyte INIT(= 0); /* any multi-byte encoding */

/*
 * To speed up BYTELEN() we fill a table with the byte lengths whenever
 * enc_utf8 or enc_dbcs changes.
 */
EXTERN char mb_bytelen_tab[256];

/* Variables that tell what conversion is used for keyboard input and display
 * output. */
EXTERN vimconv_T input_conv;  /* type of input conversion */
EXTERN vimconv_T output_conv; /* type of output conversion */

/*
 * Function pointers, used to quickly get to the right function.  Each has
 * three possible values: latin_ (8-bit), utfc_ or utf_ (utf-8) and dbcs_
 * (DBCS).
 * The value is set in mb_init();
 */
/* length of char in bytes, including following composing chars */
EXTERN int (*mb_ptr2len)(char_u *p) INIT(= latin_ptr2len);
/* idem, with limit on string length */
EXTERN int (*mb_ptr2len_len)(char_u *p, int size) INIT(= latin_ptr2len_len);
/* byte length of char */
EXTERN int (*mb_char2len)(int c) INIT(= latin_char2len);
/* convert char to bytes, return the length */
EXTERN int (*mb_char2bytes)(int c, char_u *buf) INIT(= latin_char2bytes);
EXTERN int (*mb_ptr2cells)(char_u *p) INIT(= latin_ptr2cells);
EXTERN int (*mb_ptr2cells_len)(char_u *p, int size) INIT(= latin_ptr2cells_len);
EXTERN int (*mb_char2cells)(int c) INIT(= latin_char2cells);
EXTERN int (*mb_off2cells)(unsigned off, unsigned max_off) INIT(= latin_off2cells);
EXTERN int (*mb_ptr2char)(char_u *p) INIT(= latin_ptr2char);
EXTERN int (*mb_head_off)(char_u *base, char_u *p) INIT(= latin_head_off);

#if defined(USE_ICONV) && defined(DYNAMIC_ICONV)
/* Pointers to functions and variables to be loaded at runtime */
EXTERN size_t (*iconv)(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
EXTERN iconv_t (*iconv_open)(const char *tocode, const char *fromcode);
EXTERN int (*iconv_close)(iconv_t cd);
EXTERN int (*iconvctl)(iconv_t cd, int request, void *argument);
EXTERN int *(*iconv_errno)(void);
#endif

/*
 * "State" is the main state of Vim.
 * There are other variables that modify the state:
 * "Visual_mode"    When State is NORMAL or INSERT.
 * "finish_op"	    When State is NORMAL, after typing the operator and before
 *		    typing the motion command.
 * "motion_force"   Last motion_force  from do_pending_operator()
 * "debug_mode"	    Debug mode.
 */
EXTERN int State INIT(= NORMAL); /* This is the current state of the
					 * command interpreter. */
#ifdef FEAT_EVAL
EXTERN int debug_mode INIT(= FALSE);
#endif

EXTERN int finish_op INIT(= FALSE); /* TRUE while an operator is pending */
EXTERN long opcount INIT(= 0);      /* count for pending operator */
EXTERN int motion_force INIT(= 0);  // motion force for pending operator

/*
 * Ex mode (Q) state
 */
EXTERN int exmode_active INIT(= 0);     /* zero, EXMODE_NORMAL or EXMODE_VIM */
EXTERN int ex_no_reprint INIT(= FALSE); /* no need to print after z or p */

EXTERN int reg_recording INIT(= 0); /* register for recording  or zero */
EXTERN int reg_executing INIT(= 0); /* register being executed or zero */

EXTERN int no_mapping INIT(= FALSE);  /* currently no mapping allowed */
EXTERN int no_zero_mapping INIT(= 0); /* mapping zero not allowed */
EXTERN int allow_keys INIT(= FALSE);  /* allow key codes when no_mapping
					 * is set */
EXTERN int no_u_sync INIT(= 0);       /* Don't call u_sync() */
#ifdef FEAT_EVAL
EXTERN int u_sync_once INIT(= 0); /* Call u_sync() once when evaluating
					   an expression. */
#endif

EXTERN int restart_edit INIT(= 0);   /* call edit when next cmd finished */
EXTERN int arrow_used;               /* Normally FALSE, set to TRUE after
					 * hitting cursor key in insert mode.
					 * Used by vgetorpeek() to decide when
					 * to call u_sync() */
EXTERN int ins_at_eol INIT(= FALSE); /* put cursor after eol when
					   restarting edit after CTRL-O */

EXTERN int no_abbr INIT(= TRUE); /* TRUE when no abbreviations loaded */

#ifdef USE_EXE_NAME
EXTERN char_u *exe_name; /* the name of the executable */
#endif

EXTERN int mapped_ctrl_c INIT(= FALSE);    /* modes where CTRL-C is mapped */
EXTERN int ctrl_c_interrupts INIT(= TRUE); /* CTRL-C sets got_int */

EXTERN cmdmod_T cmdmod; /* Ex command modifiers */

EXTERN int msg_silent INIT(= 0);     /* don't print messages */
EXTERN int emsg_silent INIT(= 0);    /* don't print error messages */
EXTERN int emsg_noredir INIT(= 0);   /* don't redirect error messages */
EXTERN int cmd_silent INIT(= FALSE); /* don't echo the command line */

EXTERN int swap_exists_action INIT(= SEA_NONE);
/* For dialog when swap file already
					 * exists. */
EXTERN int swap_exists_did_quit INIT(= FALSE);
/* Selected "quit" at the dialog. */

EXTERN char_u *IObuff;            /* sprintf's are done in this buffer,
					   size is IOSIZE */
EXTERN char_u *NameBuff;          /* file names are expanded in this
					 * buffer, size is MAXPATHL */
EXTERN char msg_buf[MSG_BUF_LEN]; /* small buffer for messages */

/* When non-zero, postpone redrawing. */
EXTERN int RedrawingDisabled INIT(= 0);

EXTERN int readonlymode INIT(= FALSE); /* Set to TRUE for "view" */
EXTERN int recoverymode INIT(= FALSE); /* Set to TRUE for "-r" option */

EXTERN typebuf_T typebuf /* typeahead buffer */
#ifdef DO_INIT
    = {NULL, NULL, 0, 0, 0, 0, 0, 0, 0}
#endif
;
EXTERN int ex_normal_busy INIT(= 0); /* recursiveness of ex_normal() */
EXTERN int ex_normal_lock INIT(= 0); /* forbid use of ex_normal() */
#ifdef FEAT_EVAL
EXTERN int ignore_script INIT(= FALSE); /* ignore script input */
#endif
EXTERN int stop_insert_mode; /* for ":stopinsert" and 'insertmode' */

EXTERN int KeyTyped;          /* TRUE if user typed current char */
EXTERN int KeyStuffed;        /* TRUE if current char from stuffbuf */
EXTERN int maptick INIT(= 0); /* tick for each non-mapped char */

EXTERN int must_redraw INIT(= 0);     /* type of redraw necessary */
EXTERN int skip_redraw INIT(= FALSE); /* skip redraw once */
EXTERN int do_redraw INIT(= FALSE);   /* extra redraw once */

EXTERN int need_highlight_changed INIT(= TRUE);

#define NSCRIPT 15
EXTERN FILE *scriptin[NSCRIPT];      /* streams to read script from */
EXTERN int curscript INIT(= 0);      /* index in scriptin[] */
EXTERN FILE *scriptout INIT(= NULL); /* stream to write script to */
EXTERN int read_cmd_fd INIT(= 0);    /* fd to read commands from */

/* volatile because it is used in signal handler catch_sigint(). */
EXTERN volatile sig_atomic_t got_int INIT(= FALSE); /* set to TRUE when interrupt
						signal occurred */
#ifdef USE_TERM_CONSOLE
EXTERN int term_console INIT(= FALSE); /* set to TRUE when console used */
#endif
EXTERN int termcap_active INIT(= FALSE); /* set by starttermcap() */
EXTERN int cur_tmode INIT(= TMODE_COOK); /* input terminal mode */
EXTERN int bangredo INIT(= FALSE);       /* set to TRUE with ! command */
EXTERN int searchcmdlen;                 /* length of previous search cmd */

EXTERN int did_outofmem_msg INIT(= FALSE);
/* set after out of memory msg */
EXTERN int did_swapwrite_msg INIT(= FALSE);
/* set after swap write error msg */
EXTERN int undo_off INIT(= FALSE);     /* undo switched off for now */
EXTERN int global_busy INIT(= 0);      /* set when :global is executing */
EXTERN int listcmd_busy INIT(= FALSE); /* set when :argdo, :windo or
					       :bufdo is executing */
EXTERN int need_start_insertmode INIT(= FALSE);
/* start insert mode soon */
EXTERN char_u *last_cmdline INIT(= NULL);   /* last command line (for ":) */
EXTERN char_u *repeat_cmdline INIT(= NULL); /* command line for "." */
#ifdef FEAT_CMDHIST
EXTERN char_u *new_last_cmdline INIT(= NULL); /* new value for last_cmdline */
#endif
EXTERN char_u *autocmd_fname INIT(= NULL); /* fname for <afile> on cmdline */
EXTERN int autocmd_fname_full;             /* autocmd_fname is full path */
EXTERN int autocmd_bufnr INIT(= 0);        /* fnum for <abuf> on cmdline */
EXTERN char_u *autocmd_match INIT(= NULL); /* name for <amatch> on cmdline */
EXTERN int did_cursorhold INIT(= FALSE);   /* set when CursorHold t'gerd */
EXTERN pos_T last_cursormoved              /* for CursorMoved event */
#ifdef DO_INIT
    = {0, 0, 0}
#endif
;

EXTERN int postponed_split INIT(= 0);       /* for CTRL-W CTRL-] command */
EXTERN int postponed_split_flags INIT(= 0); /* args for win_split() */
EXTERN int postponed_split_tab INIT(= 0);   /* cmdmod.tab */
#ifdef FEAT_QUICKFIX
EXTERN int g_do_tagpreview INIT(= 0); // for tag preview commands:
                                      // height of preview window
#endif
EXTERN int g_tag_at_cursor INIT(= FALSE); // whether the tag command comes
                                          // from the command line (0) or was
                                          // invoked as a normal command (1)

EXTERN int replace_offset INIT(= 0); /* offset for replace_push() */

EXTERN char_u *escape_chars INIT(= (char_u *)" \t\\\"|");
/* need backslash in cmd line */

EXTERN int keep_help_flag INIT(= FALSE); /* doing :ta from help file */

/*
 * When a string option is NULL (which only happens in out-of-memory
 * situations), it is set to empty_option, to avoid having to check for NULL
 * everywhere.
 */
EXTERN char_u *empty_option INIT(= (char_u *)"");

EXTERN int redir_off INIT(= FALSE); /* no redirection for a moment */
EXTERN FILE *redir_fd INIT(= NULL); /* message redirection file */
#ifdef FEAT_EVAL
EXTERN int redir_reg INIT(= 0);     /* message redirection register */
EXTERN int redir_vname INIT(= 0);   /* message redirection variable */
EXTERN int redir_execute INIT(= 0); /* execute() redirection */
#endif

#ifdef FEAT_LANGMAP
EXTERN char_u langmap_mapchar[256]; /* mapping for language keys */
#endif

#ifdef FEAT_WILDMENU
EXTERN int save_p_ls INIT(= -1);  /* Save 'laststatus' setting */
EXTERN int save_p_wmh INIT(= -1); /* Save 'winminheight' setting */
EXTERN int wild_menu_showing INIT(= 0);
#define WM_SHOWN 1    /* wildmenu showing */
#define WM_SCROLLED 2 /* wildmenu showing with scroll */
#endif

#ifdef MSWIN
EXTERN char_u toupper_tab[256]; /* table for toupper() */
EXTERN char_u tolower_tab[256]; /* table for tolower() */
#endif

#ifdef FEAT_LINEBREAK
EXTERN char breakat_flags[256]; /* which characters are in 'breakat' */
#endif

/* These are in version.c, call init_longVersion() before use. */
extern char *Version;
#if defined(HAVE_DATE_TIME) && defined(VMS) && defined(VAXC)
extern char longVersion[];
#else
EXTERN char *longVersion;
#endif

/*
 * Some file names are stored in pathdef.c, which is generated from the
 * Makefile to make their value depend on the Makefile.
 */
#ifdef HAVE_PATHDEF
extern char_u *default_vim_dir;
extern char_u *default_vimruntime_dir;
extern char_u *all_cflags;
extern char_u *all_lflags;
#ifdef VMS
extern char_u *compiler_version;
extern char_u *compiled_arch;
#endif
extern char_u *compiled_user;
extern char_u *compiled_sys;
#endif

/* When a window has a local directory, the absolute path of the global
 * current directory is stored here (in allocated memory).  If the current
 * directory is not a local directory, globaldir is NULL. */
EXTERN char_u *globaldir INIT(= NULL);

/* Characters from 'listchars' option */
EXTERN int lcs_eol INIT(= '$');
EXTERN int lcs_ext INIT(= NUL);
EXTERN int lcs_prec INIT(= NUL);
EXTERN int lcs_nbsp INIT(= NUL);
EXTERN int lcs_space INIT(= NUL);
EXTERN int lcs_tab1 INIT(= NUL);
EXTERN int lcs_tab2 INIT(= NUL);
EXTERN int lcs_tab3 INIT(= NUL);
EXTERN int lcs_trail INIT(= NUL);

/* Characters from 'fillchars' option */
EXTERN int fill_stl INIT(= ' ');
EXTERN int fill_stlnc INIT(= ' ');
EXTERN int fill_vert INIT(= ' ');
EXTERN int fill_fold INIT(= '-');
EXTERN int fill_diff INIT(= '-');

#ifdef FEAT_FOLDING
EXTERN int disable_fold_update INIT(= 0);
#endif

/* Whether 'keymodel' contains "stopsel" and "startsel". */
EXTERN int km_stopsel INIT(= FALSE);
EXTERN int km_startsel INIT(= FALSE);

EXTERN char_u no_lines_msg[] INIT(= N_("--No lines in buffer--"));

/*
 * When ":global" is used to number of substitutions and changed lines is
 * accumulated until it's finished.
 * Also used for ":spellrepall".
 */
EXTERN long sub_nsubs;      /* total number of substitutions */
EXTERN linenr_T sub_nlines; /* total number of lines changed */

/* table to store parsed 'wildmode' */
EXTERN char_u wim_flags[4];

#ifdef FEAT_SEARCH_EXTRA
/* don't use 'hlsearch' temporarily */
EXTERN int no_hlsearch INIT(= FALSE);
#endif

#ifdef FEAT_XCLIPBOARD
// xterm display name
EXTERN char *xterm_display INIT(= NULL);

// whether xterm_display was allocated, when FALSE it points into argv[]
EXTERN int xterm_display_allocated INIT(= FALSE);

// xterm display pointer
EXTERN Display *xterm_dpy INIT(= NULL);
#endif
#if defined(FEAT_XCLIPBOARD)
EXTERN XtAppContext app_context INIT(= (XtAppContext)NULL);
#endif

#if defined(FEAT_EVAL)
EXTERN int typebuf_was_filled INIT(= FALSE); /* received text from client
						     or from feedkeys() */
#endif

#if defined(UNIX) || defined(VMS)
EXTERN int term_is_xterm INIT(= FALSE); /* xterm-like 'term' */
#endif

#ifdef BACKSLASH_IN_FILENAME
EXTERN char psepc INIT(= '\\'); /* normal path separator character */
EXTERN char psepcN INIT(= '/'); /* abnormal path separator character */
/* normal path separator string */
EXTERN char pseps[2] INIT(= {'\\' COMMA 0});
#endif

/* Set to TRUE when an operator is being executed with virtual editing, MAYBE
 * when no operator is being executed, FALSE otherwise. */
EXTERN int virtual_op INIT(= MAYBE);

#ifdef USE_MCH_ERRMSG
/* Grow array to collect error messages in until they can be displayed. */
EXTERN garray_T error_ga
#ifdef DO_INIT
    = {0, 0, 0, 0, NULL}
#endif
;
#endif

/*
 * The error messages that can be shared are included here.
 * Excluded are errors that are only used once and debugging messages.
 */
EXTERN char e_abort[] INIT(= N_("E470: Command aborted"));
EXTERN char e_argreq[] INIT(= N_("E471: Argument required"));
EXTERN char e_backslash[] INIT(= N_("E10: \\ should be followed by /, ? or &"));
EXTERN char e_curdir[] INIT(= N_("E12: Command not allowed from exrc/vimrc in current dir or tag search"));
#ifdef FEAT_EVAL
EXTERN char e_endif[] INIT(= N_("E171: Missing :endif"));
EXTERN char e_endtry[] INIT(= N_("E600: Missing :endtry"));
EXTERN char e_endwhile[] INIT(= N_("E170: Missing :endwhile"));
EXTERN char e_endfor[] INIT(= N_("E170: Missing :endfor"));
EXTERN char e_while[] INIT(= N_("E588: :endwhile without :while"));
EXTERN char e_for[] INIT(= N_("E588: :endfor without :for"));
#endif
EXTERN char e_exists[] INIT(= N_("E13: File exists (add ! to override)"));
EXTERN char e_failed[] INIT(= N_("E472: Command failed"));
EXTERN char e_internal[] INIT(= N_("E473: Internal error"));
EXTERN char e_intern2[] INIT(= N_("E685: Internal error: %s"));
EXTERN char e_interr[] INIT(= N_("Interrupted"));
EXTERN char e_invaddr[] INIT(= N_("E14: Invalid address"));
EXTERN char e_invarg[] INIT(= N_("E474: Invalid argument"));
EXTERN char e_invarg2[] INIT(= N_("E475: Invalid argument: %s"));
EXTERN char e_duparg2[] INIT(= N_("E983: Duplicate argument: %s"));
EXTERN char e_invargval[] INIT(= N_("E475: Invalid value for argument %s"));
EXTERN char e_invargNval[] INIT(= N_("E475: Invalid value for argument %s: %s"));
#ifdef FEAT_EVAL
EXTERN char e_invexpr2[] INIT(= N_("E15: Invalid expression: %s"));
#endif
EXTERN char e_invrange[] INIT(= N_("E16: Invalid range"));
EXTERN char e_invcmd[] INIT(= N_("E476: Invalid command"));
#if defined(UNIX)
EXTERN char e_isadir2[] INIT(= N_("E17: \"%s\" is a directory"));
#endif
#ifdef FEAT_LIBCALL
EXTERN char e_libcall[] INIT(= N_("E364: Library call failed for \"%s()\""));
#endif
#ifdef HAVE_FSYNC
EXTERN char e_fsync[] INIT(= N_("E667: Fsync failed"));
#endif
#if defined(DYNAMIC_PERL) || defined(DYNAMIC_PYTHON) || defined(DYNAMIC_PYTHON3) || defined(DYNAMIC_RUBY) || defined(DYNAMIC_TCL) || defined(DYNAMIC_ICONV) || defined(DYNAMIC_GETTEXT) || defined(DYNAMIC_MZSCHEME) || defined(DYNAMIC_LUA) || defined(FEAT_TERMINAL)
EXTERN char e_loadlib[] INIT(= N_("E370: Could not load library %s"));
EXTERN char e_loadfunc[] INIT(= N_("E448: Could not load library function %s"));
#endif
EXTERN char e_markinval[] INIT(= N_("E19: Mark has invalid line number"));
EXTERN char e_marknotset[] INIT(= N_("E20: Mark not set"));
EXTERN char e_modifiable[] INIT(= N_("E21: Cannot make changes, 'modifiable' is off"));
EXTERN char e_nesting[] INIT(= N_("E22: Scripts nested too deep"));
EXTERN char e_noalt[] INIT(= N_("E23: No alternate file"));
EXTERN char e_noabbr[] INIT(= N_("E24: No such abbreviation"));
EXTERN char e_nobang[] INIT(= N_("E477: No ! allowed"));
EXTERN char e_nogvim[] INIT(= N_("E25: GUI cannot be used: Not enabled at compile time"));
#ifndef FEAT_RIGHTLEFT
EXTERN char e_nohebrew[] INIT(= N_("E26: Hebrew cannot be used: Not enabled at compile time\n"));
#endif
EXTERN char e_nofarsi[] INIT(= N_("E27: Farsi support has been removed\n"));
#ifndef FEAT_ARABIC
EXTERN char e_noarabic[] INIT(= N_("E800: Arabic cannot be used: Not enabled at compile time\n"));
#endif
#if defined(FEAT_SEARCH_EXTRA)
EXTERN char e_nogroup[] INIT(= N_("E28: No such highlight group name: %s"));
#endif
EXTERN char e_noinstext[] INIT(= N_("E29: No inserted text yet"));
EXTERN char e_nolastcmd[] INIT(= N_("E30: No previous command line"));
EXTERN char e_nomap[] INIT(= N_("E31: No such mapping"));
EXTERN char e_nomatch[] INIT(= N_("E479: No match"));
EXTERN char e_nomatch2[] INIT(= N_("E480: No match: %s"));
EXTERN char e_noname[] INIT(= N_("E32: No file name"));
EXTERN char e_nopresub[] INIT(= N_("E33: No previous substitute regular expression"));
EXTERN char e_noprev[] INIT(= N_("E34: No previous command"));
EXTERN char e_noprevre[] INIT(= N_("E35: No previous regular expression"));
EXTERN char e_norange[] INIT(= N_("E481: No range allowed"));
EXTERN char e_noroom[] INIT(= N_("E36: Not enough room"));
EXTERN char e_notcreate[] INIT(= N_("E482: Can't create file %s"));
EXTERN char e_notmp[] INIT(= N_("E483: Can't get temp file name"));
EXTERN char e_notopen[] INIT(= N_("E484: Can't open file %s"));
EXTERN char e_notread[] INIT(= N_("E485: Can't read file %s"));
EXTERN char e_null[] INIT(= N_("E38: Null argument"));
#if defined(FEAT_DIGRAPHS) || defined(FEAT_TIMERS)
EXTERN char e_number_exp[] INIT(= N_("E39: Number expected"));
#endif
#ifdef FEAT_QUICKFIX
EXTERN char e_openerrf[] INIT(= N_("E40: Can't open errorfile %s"));
#endif
EXTERN char e_outofmem[] INIT(= N_("E41: Out of memory!"));
EXTERN char e_patnotf2[] INIT(= N_("E486: Pattern not found: %s"));
EXTERN char e_positive[] INIT(= N_("E487: Argument must be positive"));
#if defined(UNIX) || defined(FEAT_SESSION)
EXTERN char e_prev_dir[] INIT(= N_("E459: Cannot go back to previous directory"));
#endif

#ifdef FEAT_QUICKFIX
EXTERN char e_quickfix[] INIT(= N_("E42: No Errors"));
EXTERN char e_loclist[] INIT(= N_("E776: No location list"));
#endif
EXTERN char e_re_damg[] INIT(= N_("E43: Damaged match string"));
EXTERN char e_re_corr[] INIT(= N_("E44: Corrupted regexp program"));
EXTERN char e_readonly[] INIT(= N_("E45: 'readonly' option is set (add ! to override)"));
#ifdef FEAT_EVAL
EXTERN char e_readonlyvar[] INIT(= N_("E46: Cannot change read-only variable \"%s\""));
EXTERN char e_readonlysbx[] INIT(= N_("E794: Cannot set variable in the sandbox: \"%s\""));
EXTERN char e_emptykey[] INIT(= N_("E713: Cannot use empty key for Dictionary"));
EXTERN char e_dictreq[] INIT(= N_("E715: Dictionary required"));
EXTERN char e_listidx[] INIT(= N_("E684: list index out of range: %ld"));
EXTERN char e_blobidx[] INIT(= N_("E979: Blob index out of range: %ld"));
EXTERN char e_invalblob[] INIT(= N_("E978: Invalid operation for Blob"));
EXTERN char e_toomanyarg[] INIT(= N_("E118: Too many arguments for function: %s"));
EXTERN char e_dictkey[] INIT(= N_("E716: Key not present in Dictionary: %s"));
EXTERN char e_listreq[] INIT(= N_("E714: List required"));
EXTERN char e_listblobreq[] INIT(= N_("E897: List or Blob required"));
EXTERN char e_listdictarg[] INIT(= N_("E712: Argument of %s must be a List or Dictionary"));
EXTERN char e_listdictblobarg[] INIT(= N_("E896: Argument of %s must be a List, Dictionary or Blob"));
#endif
#ifdef FEAT_QUICKFIX
EXTERN char e_readerrf[] INIT(= N_("E47: Error while reading errorfile"));
#endif
#ifdef HAVE_SANDBOX
EXTERN char e_sandbox[] INIT(= N_("E48: Not allowed in sandbox"));
#endif
EXTERN char e_secure[] INIT(= N_("E523: Not allowed here"));
#if defined(MACOS_X) || defined(MSWIN) || defined(UNIX) || defined(VMS)
EXTERN char e_screenmode[] INIT(= N_("E359: Screen mode setting not supported"));
#endif
EXTERN char e_scroll[] INIT(= N_("E49: Invalid scroll size"));
EXTERN char e_shellempty[] INIT(= N_("E91: 'shell' option is empty"));
#if defined(FEAT_SIGN_ICONS)
EXTERN char e_signdata[] INIT(= N_("E255: Couldn't read in sign data!"));
#endif
EXTERN char e_swapclose[] INIT(= N_("E72: Close error on swap file"));
EXTERN char e_tagstack[] INIT(= N_("E73: tag stack empty"));
EXTERN char e_toocompl[] INIT(= N_("E74: Command too complex"));
EXTERN char e_longname[] INIT(= N_("E75: Name too long"));
EXTERN char e_toomsbra[] INIT(= N_("E76: Too many ["));
EXTERN char e_toomany[] INIT(= N_("E77: Too many file names"));
EXTERN char e_trailing[] INIT(= N_("E488: Trailing characters"));
EXTERN char e_umark[] INIT(= N_("E78: Unknown mark"));
EXTERN char e_wildexpand[] INIT(= N_("E79: Cannot expand wildcards"));
EXTERN char e_winheight[] INIT(= N_("E591: 'winheight' cannot be smaller than 'winminheight'"));
EXTERN char e_winwidth[] INIT(= N_("E592: 'winwidth' cannot be smaller than 'winminwidth'"));
EXTERN char e_write[] INIT(= N_("E80: Error while writing"));
EXTERN char e_zerocount[] INIT(= N_("E939: Positive count required"));
#ifdef FEAT_EVAL
EXTERN char e_usingsid[] INIT(= N_("E81: Using <SID> not in a script context"));
#endif
EXTERN char e_maxmempat[] INIT(= N_("E363: pattern uses more memory than 'maxmempattern'"));
EXTERN char e_emptybuf[] INIT(= N_("E749: empty buffer"));
EXTERN char e_nobufnr[] INIT(= N_("E86: Buffer %ld does not exist"));

EXTERN char e_invalpat[] INIT(= N_("E682: Invalid search pattern or delimiter"));
EXTERN char e_bufloaded[] INIT(= N_("E139: File is loaded in another buffer"));
EXTERN char e_invalidreg[] INIT(= N_("E850: Invalid register name"));
EXTERN char e_dirnotf[] INIT(= N_("E919: Directory not found in '%s': \"%s\""));
EXTERN char e_au_recursive[] INIT(= N_("E952: Autocommand caused recursive behavior"));

EXTERN char top_bot_msg[] INIT(= N_("search hit TOP, continuing at BOTTOM"));
EXTERN char bot_top_msg[] INIT(= N_("search hit BOTTOM, continuing at TOP"));

/*
 * Comms. with the session manager (XSMP)
 */
#ifdef USE_XSMP
EXTERN int xsmp_icefd INIT(= -1); /* The actual connection */
#endif

/* For undo we need to know the lowest time possible. */
EXTERN time_T starttime;

#ifdef STARTUPTIME
EXTERN FILE *time_fd INIT(= NULL); /* where to write startup timing */
#endif

/*
 * Some compilers warn for not using a return value, but in some situations we
 * can't do anything useful with the value.  Assign to this variable to avoid
 * the warning.
 */
EXTERN int vim_ignored;
EXTERN char *vim_ignoredp;

#ifdef FEAT_EVAL
/* set by alloc_fail(): ID */
EXTERN alloc_id_T alloc_fail_id INIT(= aid_none);
/* set by alloc_fail(), when zero alloc() returns NULL */
EXTERN int alloc_fail_countdown INIT(= -1);
/* set by alloc_fail(), number of times alloc() returns NULL */
EXTERN int alloc_fail_repeat INIT(= 0);

// flags set by test_override()
EXTERN int disable_char_avail_for_testing INIT(= FALSE);
EXTERN int disable_redraw_for_testing INIT(= FALSE);
EXTERN int ignore_redraw_flag_for_testing INIT(= FALSE);
EXTERN int nfa_fail_for_testing INIT(= FALSE);
EXTERN int no_query_mouse_for_testing INIT(= FALSE);

EXTERN int in_free_unref_items INIT(= FALSE);
#endif

#ifdef FEAT_TIMERS
EXTERN int did_add_timer INIT(= FALSE);
EXTERN int timer_busy INIT(= 0); /* when timer is inside vgetc() then > 0 */
#endif

#ifdef FEAT_BEVAL_TERM
EXTERN int bevalexpr_due_set INIT(= FALSE);
EXTERN proftime_T bevalexpr_due;
#endif

#ifdef FEAT_EVAL
EXTERN time_T time_for_testing INIT(= 0);

/* Abort conversion to string after a recursion error. */
EXTERN int did_echo_string_emsg INIT(= FALSE);

/* Used for checking if local variables or arguments used in a lambda. */
EXTERN int *eval_lavars_used INIT(= NULL);
#endif

#ifdef MSWIN
#ifdef PROTO
typedef int HINSTANCE;
#endif
EXTERN int ctrl_break_was_pressed INIT(= FALSE);
EXTERN HINSTANCE g_hinst INIT(= NULL);
#endif
