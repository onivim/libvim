/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * screen.c: code for displaying on the screen
 *
 * Output to the screen (console, terminal emulator or GUI window) is minimized
 * by remembering what is already on the screen, and only updating the parts
 * that changed.
 *
 * ScreenLines[off]  Contains a copy of the whole screen, as it is currently
 *		     displayed (excluding text written by external commands).
 * ScreenAttrs[off]  Contains the associated attributes.
 * LineOffset[row]   Contains the offset into ScreenLines*[] and ScreenAttrs[]
 *		     for each line.
 * LineWraps[row]    Flag for each line whether it wraps to the next line.
 *
 * For double-byte characters, two consecutive bytes in ScreenLines[] can form
 * one character which occupies two display cells.
 * For UTF-8 a multi-byte character is converted to Unicode and stored in
 * ScreenLinesUC[].  ScreenLines[] contains the first byte only.  For an ASCII
 * character without composing chars ScreenLinesUC[] will be 0 and
 * ScreenLinesC[][] is not used.  When the character occupies two display
 * cells the next byte in ScreenLines[] is 0.
 * ScreenLinesC[][] contain up to 'maxcombine' composing characters
 * (drawn on top of the first character).  There is 0 after the last one used.
 * ScreenLines2[] is only used for euc-jp to store the second byte if the
 * first byte is 0x8e (single-width character).
 *
 * The screen_*() functions write to the screen and handle updating
 * ScreenLines[].
 *
 * update_screen() is the function that updates all windows and status lines.
 * It is called form the main loop when must_redraw is non-zero.  It may be
 * called from other places when an immediate screen update is needed.
 *
 * The part of the buffer that is displayed in a window is set with:
 * - w_topline (first buffer line in window)
 * - w_topfill (filler lines above the first line)
 * - w_leftcol (leftmost window cell in window),
 * - w_skipcol (skipped window cells of first line)
 *
 * Commands that only move the cursor around in a window, do not need to take
 * action to update the display.  The main loop will check if w_topline is
 * valid and update it (scroll the window) when needed.
 *
 * Commands that scroll a window change w_topline and must call
 * check_cursor() to move the cursor into the visible part of the window, and
 * call redraw_later(VALID) to have the window displayed by update_screen()
 * later.
 *
 * Commands that change text in the buffer must call changed_bytes() or
 * changed_lines() to mark the area that changed and will require updating
 * later.  The main loop will call update_screen(), which will update each
 * window that shows the changed buffer.  This assumes text above the change
 * can remain displayed as it is.  Text after the change may need updating for
 * scrolling, folding and syntax highlighting.
 *
 * Commands that change how a window is displayed (e.g., setting 'list') or
 * invalidate the contents of a window in another way (e.g., change fold
 * settings), must call redraw_later(NOT_VALID) to have the whole window
 * redisplayed by update_screen() later.
 *
 * Commands that change how a buffer is displayed (e.g., setting 'tabstop')
 * must call redraw_curbuf_later(NOT_VALID) to have all the windows for the
 * buffer redisplayed by update_screen() later.
 *
 * Commands that change highlighting and possibly cause a scroll too must call
 * redraw_later(SOME_VALID) to update the whole window but still use scrolling
 * to avoid redrawing everything.  But the length of displayed lines must not
 * change, use NOT_VALID then.
 *
 * Commands that move the window position must call redraw_later(NOT_VALID).
 * TODO: should minimize redrawing by scrolling when possible.
 *
 * Commands that change everything (e.g., resizing the screen) must call
 * redraw_all_later(NOT_VALID) or redraw_all_later(CLEAR).
 *
 * Things that are handled indirectly:
 * - When messages scroll the screen up, msg_scrolled will be set and
 *   update_screen() called to redraw.
 */

#include "vim.h"

#define MB_FILLER_CHAR '<' /* character used when a double-width character * doesn't fit. */

/*
 * The attributes that are actually active for writing to the screen.
 */
static int screen_attr = 0;

/*
 * Positioning the cursor is reduced by remembering the last position.
 * Mostly used by windgoto() and screen_char().
 */
static int screen_cur_row, screen_cur_col; /* last known cursor position */

#ifdef FEAT_SEARCH_EXTRA
static match_T search_hl; /* used for 'hlsearch' highlight matching */
#endif

#ifdef FEAT_FOLDING
static foldinfo_T win_foldinfo; /* info for 'foldcolumn' */
static int compute_foldcolumn(win_T *wp, int col);
#endif

/* Flag that is set when drawing for a callback, not from the main command
 * loop. */
static int redrawing_for_callback = 0;

/*
 * Buffer for one screen line (characters and attributes).
 */
static schar_T *current_ScreenLine;

static void win_update(win_T *wp);
static void win_redr_status(win_T *wp, int ignore_pum);
static void win_draw_end(win_T *wp, int c1, int c2, int draw_margin, int row, int endrow, hlf_T hl);
#ifdef FEAT_FOLDING
static void fold_line(win_T *wp, long fold_count, foldinfo_T *foldinfo, linenr_T lnum, int row);
static void fill_foldcolumn(char_u *p, win_T *wp, int closed, linenr_T lnum);
static void copy_text_attr(int off, char_u *buf, int len, int attr);
#endif
static int win_line(win_T *, linenr_T, int, int, int nochange, int number_only);
static void draw_vsep_win(win_T *wp, int row);
#ifdef FEAT_SEARCH_EXTRA
#define SEARCH_HL_PRIORITY 0
static void start_search_hl(void);
static void end_search_hl(void);
static void init_search_hl(win_T *wp);
static void prepare_search_hl(win_T *wp, linenr_T lnum);
static void next_search_hl(win_T *win, match_T *shl, linenr_T lnum, colnr_T mincol, matchitem_T *cur);
static int next_search_hl_pos(match_T *shl, linenr_T lnum, posmatch_T *pos, colnr_T mincol);
#endif
static void screen_char(unsigned off, int row, int col);
static void screen_char_2(unsigned off, int row, int col);
static void screenclear2(void);
static void lineclear(unsigned off, int width, int attr);
static void lineinvalid(unsigned off, int width);
static int win_do_lines(win_T *wp, int row, int line_count, int mayclear, int del, int clear_attr);
static void win_rest_invalid(win_T *wp);
static void msg_pos_mode(void);
static void recording_mode(int attr);
static int fillchar_status(int *attr, win_T *wp);
static int fillchar_vsep(int *attr);

/* Ugly global: overrule attribute used by screen_char() */
static int screen_char_attr = 0;

#ifdef FEAT_RIGHTLEFT
#define HAS_RIGHTLEFT(x) x
#else
#define HAS_RIGHTLEFT(x) FALSE
#endif

// flags for screen_line()
#define SLF_RIGHTLEFT 1
#define SLF_POPUP 2

/*
 * Redraw the current window later, with update_screen(type).
 * Set must_redraw only if not already set to a higher value.
 * E.g. if must_redraw is CLEAR, type NOT_VALID will do nothing.
 */
void redraw_later(int type)
{
  redraw_win_later(curwin, type);
}

void redraw_win_later(
    win_T *wp,
    int type)
{
  if (!exiting && wp->w_redr_type < type)
  {
    wp->w_redr_type = type;
    if (type >= NOT_VALID)
      wp->w_lines_valid = 0;
    if (must_redraw < type) /* must_redraw is the maximum of all windows */
      must_redraw = type;
  }
}

/*
 * Force a complete redraw later.  Also resets the highlighting.  To be used
 * after executing a shell command that messes up the screen.
 */
void redraw_later_clear(void)
{
  redraw_all_later(CLEAR);
#ifdef FEAT_GUI
  if (gui.in_use)
    /* Use a code that will reset gui.highlight_mask in
	 * gui_stop_highlight(). */
    screen_attr = HL_ALL + 1;
  else
#endif
    /* Use attributes that is very unlikely to appear in text. */
    screen_attr = HL_BOLD | HL_UNDERLINE | HL_INVERSE | HL_STRIKETHROUGH;
}

/*
 * Mark all windows to be redrawn later.
 */
void redraw_all_later(int type)
{
  win_T *wp;

  FOR_ALL_WINDOWS(wp)
  redraw_win_later(wp, type);
  // This may be needed when switching tabs.
  if (must_redraw < type)
    must_redraw = type;
}

/*
 * Mark all windows that are editing the current buffer to be updated later.
 */
void redraw_curbuf_later(int type)
{
  redraw_buf_later(curbuf, type);
}

void redraw_buf_later(buf_T *buf, int type)
{
  win_T *wp;

  FOR_ALL_WINDOWS(wp)
  {
    if (wp->w_buffer == buf)
      redraw_win_later(wp, type);
  }
}

#if defined(FEAT_SIGNS) || defined(PROTO)
void redraw_buf_line_later(buf_T *buf, linenr_T lnum)
{
  win_T *wp;

  FOR_ALL_WINDOWS(wp)
  if (wp->w_buffer == buf && lnum >= wp->w_topline && lnum < wp->w_botline)
    redrawWinline(wp, lnum);
}
#endif

#if defined(FEAT_JOB_CHANNEL) || defined(PROTO)
void redraw_buf_and_status_later(buf_T *buf, int type)
{
  win_T *wp;

#ifdef FEAT_WILDMENU
  if (wild_menu_showing != 0)
    /* Don't redraw while the command line completion is displayed, it
	 * would disappear. */
    return;
#endif
  FOR_ALL_WINDOWS(wp)
  {
    if (wp->w_buffer == buf)
    {
      redraw_win_later(wp, type);
      wp->w_redr_status = TRUE;
    }
  }
}
#endif

/*
 * Invoked after an asynchronous callback is called.
 * If an echo command was used the cursor needs to be put back where
 * it belongs. If highlighting was changed a redraw is needed.
 * If "call_update_screen" is FALSE don't call update_screen() when at the
 * command line.
 */
void redraw_after_callback(int call_update_screen)
{
  ++redrawing_for_callback;

  if (State == HITRETURN || State == ASKMORE)
    ; // do nothing
  else if (State & CMDLINE)
  {
    // Don't redraw when in prompt_for_number().
    if (cmdline_row > 0)
    {
      // Redrawing only works when the screen didn't scroll. Don't clear
      // wildmenu entries.
      if (msg_scrolled == 0
#ifdef FEAT_WILDMENU
          && wild_menu_showing == 0
#endif
          && call_update_screen)
        update_screen(0);

      // Redraw in the same position, so that the user can continue
      // editing the command.
      redrawcmdline_ex(FALSE);
    }
  }
  else if (State & (NORMAL | INSERT | TERMINAL))
  {
    // keep the command line if possible
    update_screen(VALID_NO_UPDATE);
    setcursor();
  }
  cursor_on();

  --redrawing_for_callback;
}

/*
 * Changed something in the current window, at buffer line "lnum", that
 * requires that line and possibly other lines to be redrawn.
 * Used when entering/leaving Insert mode with the cursor on a folded line.
 * Used to remove the "$" from a change command.
 * Note that when also inserting/deleting lines w_redraw_top and w_redraw_bot
 * may become invalid and the whole window will have to be redrawn.
 */
void redrawWinline(
    win_T *wp,
    linenr_T lnum)
{
  if (wp->w_redraw_top == 0 || wp->w_redraw_top > lnum)
    wp->w_redraw_top = lnum;
  if (wp->w_redraw_bot == 0 || wp->w_redraw_bot < lnum)
    wp->w_redraw_bot = lnum;
  redraw_win_later(wp, VALID);
}

/*
 * To be called when "updating_screen" was set before and now the postponed
 * side effects may take place.
 */
void after_updating_screen(int may_resize_shell UNUSED)
{
  updating_screen = FALSE;
#ifdef FEAT_GUI
  if (may_resize_shell)
    gui_may_resize_shell();
#endif
#ifdef FEAT_TERMINAL
  term_check_channel_closed_recently();
#endif

#ifdef HAVE_DROP_FILE
  // If handle_drop() was called while updating_screen was TRUE need to
  // handle the drop now.
  handle_any_postponed_drop();
#endif
}

/*
 * Update all windows that are editing the current buffer.
 */
void update_curbuf(int type)
{
  redraw_curbuf_later(type);
  update_screen(type);
}

/*
 * Based on the current value of curwin->w_topline, transfer a screenfull
 * of stuff from Filemem to ScreenLines[], and update curwin->w_botline.
 * Return OK when the screen was updated, FAIL if it was not done.
 */
int update_screen(int type_arg)
{
  int type = type_arg;
  win_T *wp;
  static int did_intro = FALSE;
#if defined(FEAT_SEARCH_EXTRA) || defined(FEAT_CLIPBOARD)
  int did_one;
#endif
#ifdef FEAT_GUI
  int did_undraw = FALSE;
  int gui_cursor_col = 0;
  int gui_cursor_row = 0;
#endif
  int no_update = FALSE;

  /* Don't do anything if the screen structures are (not yet) valid. */
  if (!screen_valid(TRUE))
    return FAIL;

  if (type == VALID_NO_UPDATE)
  {
    no_update = TRUE;
    type = 0;
  }

#ifdef FEAT_EVAL
  {
    buf_T *buf;

    // Before updating the screen, notify any listeners of changed text.
    FOR_ALL_BUFFERS(buf)
    invoke_listeners(buf);
  }
#endif

  if (must_redraw)
  {
    if (type < must_redraw) /* use maximal type */
      type = must_redraw;

    /* must_redraw is reset here, so that when we run into some weird
	 * reason to redraw while busy redrawing (e.g., asynchronous
	 * scrolling), or update_topline() in win_update() will cause a
	 * scroll, the screen will be redrawn later or in win_update(). */
    must_redraw = 0;
  }

  /* May need to update w_lines[]. */
  if (curwin->w_lines_valid == 0 && type < NOT_VALID
#ifdef FEAT_TERMINAL
      && !term_do_update_window(curwin)
#endif
  )
    type = NOT_VALID;

  /* Postpone the redrawing when it's not needed and when being called
     * recursively. */
  if (!redrawing() || updating_screen)
  {
    redraw_later(type); /* remember type for next time */
    must_redraw = type;
    if (type > INVERTED_ALL)
      curwin->w_lines_valid = 0; /* don't use w_lines[].wl_size now */
    return FAIL;
  }

  updating_screen = TRUE;
  if (no_update)
    ++no_win_do_lines_ins;

  /*
     * if the screen was scrolled up when displaying a message, scroll it down
     */
  if (msg_scrolled)
  {
    clear_cmdline = TRUE;
    if (msg_scrolled > Rows - 5) /* clearing is faster */
      type = CLEAR;
    else if (type != CLEAR)
    {
      check_for_delay(FALSE);
      if (screen_ins_lines(0, 0, msg_scrolled, (int)Rows, 0, NULL) == FAIL)
        type = CLEAR;
      FOR_ALL_WINDOWS(wp)
      {
        if (wp->w_winrow < msg_scrolled)
        {
          if (W_WINROW(wp) + wp->w_height > msg_scrolled && wp->w_redr_type < REDRAW_TOP && wp->w_lines_valid > 0 && wp->w_topline == wp->w_lines[0].wl_lnum)
          {
            wp->w_upd_rows = msg_scrolled - W_WINROW(wp);
            wp->w_redr_type = REDRAW_TOP;
          }
          else
          {
            wp->w_redr_type = NOT_VALID;
            if (W_WINROW(wp) + wp->w_height + wp->w_status_height <= msg_scrolled)
              wp->w_redr_status = TRUE;
          }
        }
      }
      if (!no_update)
        redraw_cmdline = TRUE;
      redraw_tabline = TRUE;
    }
    msg_scrolled = 0;
    need_wait_return = FALSE;
  }

  /* reset cmdline_row now (may have been changed temporarily) */
  compute_cmdrow();

  /* Check for changed highlighting */
  if (need_highlight_changed)
    highlight_changed();

  if (type == CLEAR) /* first clear screen */
  {
    screenclear(); /* will reset clear_cmdline */
    type = NOT_VALID;
    /* must_redraw may be set indirectly, avoid another redraw later */
    must_redraw = 0;
  }

  if (clear_cmdline) /* going to clear cmdline (done below) */
    check_for_delay(FALSE);

#ifdef FEAT_LINEBREAK
  /* Force redraw when width of 'number' or 'relativenumber' column
     * changes. */
  if (curwin->w_redr_type < NOT_VALID && curwin->w_nrwidth != ((curwin->w_p_nu || curwin->w_p_rnu)
                                                                   ? number_width(curwin)
                                                                   : 0))
    curwin->w_redr_type = NOT_VALID;
#endif

  /*
     * Only start redrawing if there is really something to do.
     */
  if (type == INVERTED)
    update_curswant();
  if (curwin->w_redr_type < type && !((type == VALID && curwin->w_lines[0].wl_valid
#ifdef FEAT_DIFF
                                       && curwin->w_topfill == curwin->w_old_topfill && curwin->w_botfill == curwin->w_old_botfill
#endif
                                       && curwin->w_topline == curwin->w_lines[0].wl_lnum) ||
                                      (type == INVERTED && VIsual_active && curwin->w_old_cursor_lnum == curwin->w_cursor.lnum && curwin->w_old_visual_mode == VIsual_mode && (curwin->w_valid & VALID_VIRTCOL) && curwin->w_old_curswant == curwin->w_curswant)))
    curwin->w_redr_type = type;

  /* Redraw the tab pages line if needed. */
  if (redraw_tabline || type >= NOT_VALID)
    draw_tabline();

    /*
     * Go from top to bottom through the windows, redrawing the ones that need
     * it.
     */
#if defined(FEAT_SEARCH_EXTRA) || defined(FEAT_CLIPBOARD)
  did_one = FALSE;
#endif
#ifdef FEAT_SEARCH_EXTRA
  search_hl.rm.regprog = NULL;
#endif
  FOR_ALL_WINDOWS(wp)
  {
    if (wp->w_redr_type != 0)
    {
      cursor_off();
#if defined(FEAT_SEARCH_EXTRA) || defined(FEAT_CLIPBOARD)
      if (!did_one)
      {
        did_one = TRUE;
#ifdef FEAT_SEARCH_EXTRA
        start_search_hl();
#endif
#ifdef FEAT_CLIPBOARD
        /* When Visual area changed, may have to update selection. */
        if (clip_star.available && clip_isautosel_star())
          clip_update_selection(&clip_star);
        if (clip_plus.available && clip_isautosel_plus())
          clip_update_selection(&clip_plus);
#endif
#ifdef FEAT_GUI
        /* Remove the cursor before starting to do anything, because
		 * scrolling may make it difficult to redraw the text under
		 * it. */
        if (gui.in_use && wp == curwin)
        {
          gui_cursor_col = gui.cursor_col;
          gui_cursor_row = gui.cursor_row;
          gui_undraw_cursor();
          did_undraw = TRUE;
        }
#endif
      }
#endif
      win_update(wp);
    }

    /* redraw status line after the window to minimize cursor movement */
    if (wp->w_redr_status)
    {
      cursor_off();
      win_redr_status(wp, TRUE); // any popup menu will be redrawn below
    }
  }
#if defined(FEAT_SEARCH_EXTRA)
  end_search_hl();
#endif

  /* Reset b_mod_set flags.  Going through all windows is probably faster
     * than going through all buffers (there could be many buffers). */
  FOR_ALL_WINDOWS(wp)
  wp->w_buffer->b_mod_set = FALSE;

  after_updating_screen(TRUE);

  /* Clear or redraw the command line.  Done last, because scrolling may
     * mess up the command line. */
  if (clear_cmdline || redraw_cmdline || redraw_mode)
    showmode();

  if (no_update)
    --no_win_do_lines_ins;

  /* May put up an introductory message when not editing a file */
  if (!did_intro)
    maybe_intro_message();
  did_intro = TRUE;

#ifdef FEAT_GUI
  /* Redraw the cursor and update the scrollbars when all screen updating is
     * done. */
  if (gui.in_use)
  {
    if (did_undraw && !gui_mch_is_blink_off())
    {
      /* Put the GUI position where the cursor was, gui_update_cursor()
	     * uses that. */
      gui.col = gui_cursor_col;
      gui.row = gui_cursor_row;
      gui.col = mb_fix_col(gui.col, gui.row);
      gui_update_cursor(FALSE, FALSE);
      gui_may_flush();
      screen_cur_col = gui.col;
      screen_cur_row = gui.row;
    }
    gui_update_scrollbars(FALSE);
  }
#endif
  return OK;
}

#if defined(FEAT_GUI)
/*
 * Prepare for updating one or more windows.
 * Caller must check for "updating_screen" already set to avoid recursiveness.
 */
static void
update_prepare(void)
{
  cursor_off();
  updating_screen = TRUE;
#ifdef FEAT_GUI
  /* Remove the cursor before starting to do anything, because scrolling may
     * make it difficult to redraw the text under it. */
  if (gui.in_use)
    gui_undraw_cursor();
#endif
#ifdef FEAT_SEARCH_EXTRA
  start_search_hl();
#endif
}

/*
 * Finish updating one or more windows.
 */
static void
update_finish(void)
{
  if (redraw_cmdline || redraw_mode)
    showmode();

#ifdef FEAT_SEARCH_EXTRA
  end_search_hl();
#endif

  after_updating_screen(TRUE);

#ifdef FEAT_GUI
  /* Redraw the cursor and update the scrollbars when all screen updating is
     * done. */
  if (gui.in_use)
  {
    gui_update_scrollbars(FALSE);
  }
#endif
}
#endif

/*
 * Get 'wincolor' attribute for window "wp".  If not set and "wp" is a popup
 * window then get the "Pmenu" highlight attribute.
 */
static int
get_wcr_attr(win_T *wp)
{
  int wcr_attr = 0;

  if (*wp->w_p_wcr != NUL)
    wcr_attr = syn_name2attr(wp->w_p_wcr);
  return wcr_attr;
}

#if defined(FEAT_GUI) || defined(PROTO)
/*
 * Update a single window, its status line and maybe the command line msg.
 * Used for the GUI scrollbar.
 */
void updateWindow(win_T *wp)
{
  /* return if already busy updating */
  if (updating_screen)
    return;

  update_prepare();

#ifdef FEAT_CLIPBOARD
  /* When Visual area changed, may have to update selection. */
  if (clip_star.available && clip_isautosel_star())
    clip_update_selection(&clip_star);
  if (clip_plus.available && clip_isautosel_plus())
    clip_update_selection(&clip_plus);
#endif

  win_update(wp);

  /* When the screen was cleared redraw the tab pages line. */
  if (redraw_tabline)
    draw_tabline();

  if (wp->w_redr_status)
    win_redr_status(wp, FALSE);

  update_finish();
}
#endif

/*
 * Update a single window.
 *
 * This may cause the windows below it also to be redrawn (when clearing the
 * screen or scrolling lines).
 *
 * How the window is redrawn depends on wp->w_redr_type.  Each type also
 * implies the one below it.
 * NOT_VALID	redraw the whole window
 * SOME_VALID	redraw the whole window but do scroll when possible
 * REDRAW_TOP	redraw the top w_upd_rows window lines, otherwise like VALID
 * INVERTED	redraw the changed part of the Visual area
 * INVERTED_ALL	redraw the whole Visual area
 * VALID	1. scroll up/down to adjust for a changed w_topline
 *		2. update lines at the top when scrolled down
 *		3. redraw changed text:
 *		   - if wp->w_buffer->b_mod_set set, update lines between
 *		     b_mod_top and b_mod_bot.
 *		   - if wp->w_redraw_top non-zero, redraw lines between
 *		     wp->w_redraw_top and wp->w_redr_bot.
 *		   - continue redrawing when syntax status is invalid.
 *		4. if scrolled up, update lines at the bottom.
 * This results in three areas that may need updating:
 * top:	from first row to top_end (when scrolled down)
 * mid: from mid_start to mid_end (update inversion or changed text)
 * bot: from bot_start to last row (when scrolled up)
 */
static void
win_update(win_T *wp)
{
  buf_T *buf = wp->w_buffer;
  int type;
  int top_end = 0;           /* Below last row of the top area that needs
				   updating.  0 when no top area updating. */
  int mid_start = 999;       /* first row of the mid area that needs
				   updating.  999 when no mid area updating. */
  int mid_end = 0;           /* Below last row of the mid area that needs
				   updating.  0 when no mid area updating. */
  int bot_start = 999;       /* first row of the bot area that needs
				   updating.  999 when no bot area updating */
  int scrolled_down = FALSE; /* TRUE when scrolled down when
					   w_topline got smaller a bit */
#ifdef FEAT_SEARCH_EXTRA
  matchitem_T *cur;       /* points to the match list */
  int top_to_mod = FALSE; /* redraw above mod_top */
#endif

  int row;       /* current window row to display */
  linenr_T lnum; /* current buffer lnum to display */
  int idx;       /* current index in w_lines[] */
  int srow;      /* starting row of the current line */

  int eof = FALSE;     /* if TRUE, we hit the end of the file */
  int didline = FALSE; /* if TRUE, we finished the last line */
  int i;
  long j;
  static int recursive = FALSE; /* being called recursively */
  int old_botline = wp->w_botline;
#ifdef FEAT_FOLDING
  long fold_count;
#endif
  linenr_T mod_top = 0;
  linenr_T mod_bot = 0;
#if defined(FEAT_SEARCH_EXTRA)
  int save_got_int;
#endif
#ifdef SYN_TIME_LIMIT
  proftime_T syntax_tm;
#endif

  type = wp->w_redr_type;

  if (type == NOT_VALID)
  {
    wp->w_redr_status = TRUE;
    wp->w_lines_valid = 0;
  }

  /* Window is zero-height: nothing to draw. */
  if (wp->w_height + WINBAR_HEIGHT(wp) == 0)
  {
    wp->w_redr_type = 0;
    return;
  }

  /* Window is zero-width: Only need to draw the separator. */
  if (wp->w_width == 0)
  {
    /* draw the vertical separator right of this window */
    draw_vsep_win(wp, 0);
    wp->w_redr_type = 0;
    return;
  }

#ifdef FEAT_TERMINAL
  // If this window contains a terminal, redraw works completely differently.
  if (term_do_update_window(wp))
  {
    term_update_window(wp);
    wp->w_redr_type = 0;
    return;
  }
#endif

#ifdef FEAT_SEARCH_EXTRA
  init_search_hl(wp);
#endif

#ifdef FEAT_LINEBREAK
  /* Force redraw when width of 'number' or 'relativenumber' column
     * changes. */
  i = (wp->w_p_nu || wp->w_p_rnu) ? number_width(wp) : 0;
  if (wp->w_nrwidth != i)
  {
    type = NOT_VALID;
    wp->w_nrwidth = i;
  }
  else
#endif

      if (buf->b_mod_set && buf->b_mod_xlines != 0 && wp->w_redraw_top != 0)
  {
    /*
	 * When there are both inserted/deleted lines and specific lines to be
	 * redrawn, w_redraw_top and w_redraw_bot may be invalid, just redraw
	 * everything (only happens when redrawing is off for while).
	 */
    type = NOT_VALID;
  }
  else
  {
    /*
	 * Set mod_top to the first line that needs displaying because of
	 * changes.  Set mod_bot to the first line after the changes.
	 */
    mod_top = wp->w_redraw_top;
    if (wp->w_redraw_bot != 0)
      mod_bot = wp->w_redraw_bot + 1;
    else
      mod_bot = 0;
    if (buf->b_mod_set)
    {
      if (mod_top == 0 || mod_top > buf->b_mod_top)
      {
        mod_top = buf->b_mod_top;
      }
      if (mod_bot == 0 || mod_bot < buf->b_mod_bot)
        mod_bot = buf->b_mod_bot;

#ifdef FEAT_SEARCH_EXTRA
      /* When 'hlsearch' is on and using a multi-line search pattern, a
	     * change in one line may make the Search highlighting in a
	     * previous line invalid.  Simple solution: redraw all visible
	     * lines above the change.
	     * Same for a match pattern.
	     */
      if (search_hl.rm.regprog != NULL && re_multiline(search_hl.rm.regprog))
        top_to_mod = TRUE;
      else
      {
        cur = wp->w_match_head;
        while (cur != NULL)
        {
          if (cur->match.regprog != NULL && re_multiline(cur->match.regprog))
          {
            top_to_mod = TRUE;
            break;
          }
          cur = cur->next;
        }
      }
#endif
    }
#ifdef FEAT_FOLDING
    if (mod_top != 0 && hasAnyFolding(wp))
    {
      linenr_T lnumt, lnumb;

      /*
	     * A change in a line can cause lines above it to become folded or
	     * unfolded.  Find the top most buffer line that may be affected.
	     * If the line was previously folded and displayed, get the first
	     * line of that fold.  If the line is folded now, get the first
	     * folded line.  Use the minimum of these two.
	     */

      /* Find last valid w_lines[] entry above mod_top.  Set lnumt to
	     * the line below it.  If there is no valid entry, use w_topline.
	     * Find the first valid w_lines[] entry below mod_bot.  Set lnumb
	     * to this line.  If there is no valid entry, use MAXLNUM. */
      lnumt = wp->w_topline;
      lnumb = MAXLNUM;
      for (i = 0; i < wp->w_lines_valid; ++i)
        if (wp->w_lines[i].wl_valid)
        {
          if (wp->w_lines[i].wl_lastlnum < mod_top)
            lnumt = wp->w_lines[i].wl_lastlnum + 1;
          if (lnumb == MAXLNUM && wp->w_lines[i].wl_lnum >= mod_bot)
          {
            lnumb = wp->w_lines[i].wl_lnum;
            /* When there is a fold column it might need updating
			 * in the next line ("J" just above an open fold). */
            if (compute_foldcolumn(wp, 0) > 0)
              ++lnumb;
          }
        }

      (void)hasFoldingWin(wp, mod_top, &mod_top, NULL, TRUE, NULL);
      if (mod_top > lnumt)
        mod_top = lnumt;

      /* Now do the same for the bottom line (one above mod_bot). */
      --mod_bot;
      (void)hasFoldingWin(wp, mod_bot, NULL, &mod_bot, TRUE, NULL);
      ++mod_bot;
      if (mod_bot < lnumb)
        mod_bot = lnumb;
    }
#endif

    /* When a change starts above w_topline and the end is below
	 * w_topline, start redrawing at w_topline.
	 * If the end of the change is above w_topline: do like no change was
	 * made, but redraw the first line to find changes in syntax. */
    if (mod_top != 0 && mod_top < wp->w_topline)
    {
      if (mod_bot > wp->w_topline)
        mod_top = wp->w_topline;
    }

    /* When line numbers are displayed need to redraw all lines below
	 * inserted/deleted lines. */
    if (mod_top != 0 && buf->b_mod_xlines != 0 && wp->w_p_nu)
      mod_bot = MAXLNUM;
  }
  wp->w_redraw_top = 0; // reset for next time
  wp->w_redraw_bot = 0;

  /*
     * When only displaying the lines at the top, set top_end.  Used when
     * window has scrolled down for msg_scrolled.
     */
  if (type == REDRAW_TOP)
  {
    j = 0;
    for (i = 0; i < wp->w_lines_valid; ++i)
    {
      j += wp->w_lines[i].wl_size;
      if (j >= wp->w_upd_rows)
      {
        top_end = j;
        break;
      }
    }
    if (top_end == 0)
      /* not found (cannot happen?): redraw everything */
      type = NOT_VALID;
    else
      /* top area defined, the rest is VALID */
      type = VALID;
  }

  /* Trick: we want to avoid clearing the screen twice.  screenclear() will
     * set "screen_cleared" to TRUE.  The special value MAYBE (which is still
     * non-zero and thus not FALSE) will indicate that screenclear() was not
     * called. */
  if (screen_cleared)
    screen_cleared = MAYBE;

  /*
     * If there are no changes on the screen that require a complete redraw,
     * handle three cases:
     * 1: we are off the top of the screen by a few lines: scroll down
     * 2: wp->w_topline is below wp->w_lines[0].wl_lnum: may scroll up
     * 3: wp->w_topline is wp->w_lines[0].wl_lnum: find first entry in
     *    w_lines[] that needs updating.
     */
  if ((type == VALID || type == SOME_VALID || type == INVERTED || type == INVERTED_ALL)
#ifdef FEAT_DIFF
      && !wp->w_botfill && !wp->w_old_botfill
#endif
  )
  {
    if (mod_top != 0 && wp->w_topline == mod_top)
    {
      /*
	     * w_topline is the first changed line, the scrolling will be done
	     * further down.
	     */
    }
    else if (wp->w_lines[0].wl_valid && (wp->w_topline < wp->w_lines[0].wl_lnum
#ifdef FEAT_DIFF
                                         || (wp->w_topline == wp->w_lines[0].wl_lnum && wp->w_topfill > wp->w_old_topfill)
#endif
                                             ))
    {
      /*
	     * New topline is above old topline: May scroll down.
	     */
#ifdef FEAT_FOLDING
      if (hasAnyFolding(wp))
      {
        linenr_T ln;

        /* count the number of lines we are off, counting a sequence
		 * of folded lines as one */
        j = 0;
        for (ln = wp->w_topline; ln < wp->w_lines[0].wl_lnum; ++ln)
        {
          ++j;
          if (j >= wp->w_height - 2)
            break;
          (void)hasFoldingWin(wp, ln, NULL, &ln, TRUE, NULL);
        }
      }
      else
#endif
        j = wp->w_lines[0].wl_lnum - wp->w_topline;
      if (j < wp->w_height - 2) /* not too far off */
      {
        i = plines_m_win(wp, wp->w_topline, wp->w_lines[0].wl_lnum - 1);
#ifdef FEAT_DIFF
        /* insert extra lines for previously invisible filler lines */
        if (wp->w_lines[0].wl_lnum != wp->w_topline)
          i += diff_check_fill(wp, wp->w_lines[0].wl_lnum) - wp->w_old_topfill;
#endif
        if (i < wp->w_height - 2) /* less than a screen off */
        {
          /*
		     * Try to insert the correct number of lines.
		     * If not the last window, delete the lines at the bottom.
		     * win_ins_lines may fail when the terminal can't do it.
		     */
          if (i > 0)
            check_for_delay(FALSE);
          if (win_ins_lines(wp, 0, i, FALSE, wp == firstwin) == OK)
          {
            if (wp->w_lines_valid != 0)
            {
              /* Need to update rows that are new, stop at the
			     * first one that scrolled down. */
              top_end = i;
              scrolled_down = TRUE;

              /* Move the entries that were scrolled, disable
			     * the entries for the lines to be redrawn. */
              if ((wp->w_lines_valid += j) > wp->w_height)
                wp->w_lines_valid = wp->w_height;
              for (idx = wp->w_lines_valid; idx - j >= 0; idx--)
                wp->w_lines[idx] = wp->w_lines[idx - j];
              while (idx >= 0)
                wp->w_lines[idx--].wl_valid = FALSE;
            }
          }
          else
            mid_start = 0; /* redraw all lines */
        }
        else
          mid_start = 0; /* redraw all lines */
      }
      else
        mid_start = 0; /* redraw all lines */
    }
    else
    {
      /*
	     * New topline is at or below old topline: May scroll up.
	     * When topline didn't change, find first entry in w_lines[] that
	     * needs updating.
	     */

      /* try to find wp->w_topline in wp->w_lines[].wl_lnum */
      j = -1;
      row = 0;
      for (i = 0; i < wp->w_lines_valid; i++)
      {
        if (wp->w_lines[i].wl_valid && wp->w_lines[i].wl_lnum == wp->w_topline)
        {
          j = i;
          break;
        }
        row += wp->w_lines[i].wl_size;
      }
      if (j == -1)
      {
        /* if wp->w_topline is not in wp->w_lines[].wl_lnum redraw all
		 * lines */
        mid_start = 0;
      }
      else
      {
        /*
		 * Try to delete the correct number of lines.
		 * wp->w_topline is at wp->w_lines[i].wl_lnum.
		 */
#ifdef FEAT_DIFF
        /* If the topline didn't change, delete old filler lines,
		 * otherwise delete filler lines of the new topline... */
        if (wp->w_lines[0].wl_lnum == wp->w_topline)
          row += wp->w_old_topfill;
        else
          row += diff_check_fill(wp, wp->w_topline);
        /* ... but don't delete new filler lines. */
        row -= wp->w_topfill;
#endif
        if (row > 0)
        {
          check_for_delay(FALSE);
          if (win_del_lines(wp, 0, row, FALSE, wp == firstwin, 0) == OK)
            bot_start = wp->w_height - row;
          else
            mid_start = 0; /* redraw all lines */
        }
        if ((row == 0 || bot_start < 999) && wp->w_lines_valid != 0)
        {
          /*
		     * Skip the lines (below the deleted lines) that are still
		     * valid and don't need redrawing.	Copy their info
		     * upwards, to compensate for the deleted lines.  Set
		     * bot_start to the first row that needs redrawing.
		     */
          bot_start = 0;
          idx = 0;
          for (;;)
          {
            wp->w_lines[idx] = wp->w_lines[j];
            /* stop at line that didn't fit, unless it is still
			 * valid (no lines deleted) */
            if (row > 0 && bot_start + row + (int)wp->w_lines[j].wl_size > wp->w_height)
            {
              wp->w_lines_valid = idx + 1;
              break;
            }
            bot_start += wp->w_lines[idx++].wl_size;

            /* stop at the last valid entry in w_lines[].wl_size */
            if (++j >= wp->w_lines_valid)
            {
              wp->w_lines_valid = idx;
              break;
            }
          }
#ifdef FEAT_DIFF
          /* Correct the first entry for filler lines at the top
		     * when it won't get updated below. */
          if (wp->w_p_diff && bot_start > 0)
            wp->w_lines[0].wl_size =
                plines_win_nofill(wp, wp->w_topline, TRUE) + wp->w_topfill;
#endif
        }
      }
    }

    /* When starting redraw in the first line, redraw all lines.  When
	 * there is only one window it's probably faster to clear the screen
	 * first. */
    if (mid_start == 0)
    {
      mid_end = wp->w_height;
      if (ONE_WINDOW)
      {
        /* Clear the screen when it was not done by win_del_lines() or
		 * win_ins_lines() above, "screen_cleared" is FALSE or MAYBE
		 * then. */
        if (screen_cleared != TRUE)
          screenclear();
        /* The screen was cleared, redraw the tab pages line. */
        if (redraw_tabline)
          draw_tabline();
      }
    }

    /* When win_del_lines() or win_ins_lines() caused the screen to be
	 * cleared (only happens for the first window) or when screenclear()
	 * was called directly above, "must_redraw" will have been set to
	 * NOT_VALID, need to reset it here to avoid redrawing twice. */
    if (screen_cleared == TRUE)
      must_redraw = 0;
  }
  else
  {
    /* Not VALID or INVERTED: redraw all lines. */
    mid_start = 0;
    mid_end = wp->w_height;
  }

  if (type == SOME_VALID)
  {
    /* SOME_VALID: redraw all lines. */
    mid_start = 0;
    mid_end = wp->w_height;
    type = NOT_VALID;
  }

  /* check if we are updating or removing the inverted part */
  if ((VIsual_active && buf == curwin->w_buffer) || (wp->w_old_cursor_lnum != 0 && type != NOT_VALID))
  {
    linenr_T from, to;

    if (VIsual_active)
    {
      if (VIsual_active && (VIsual_mode != wp->w_old_visual_mode || type == INVERTED_ALL))
      {
        /*
		 * If the type of Visual selection changed, redraw the whole
		 * selection.  Also when the ownership of the X selection is
		 * gained or lost.
		 */
        if (curwin->w_cursor.lnum < VIsual.lnum)
        {
          from = curwin->w_cursor.lnum;
          to = VIsual.lnum;
        }
        else
        {
          from = VIsual.lnum;
          to = curwin->w_cursor.lnum;
        }
        /* redraw more when the cursor moved as well */
        if (wp->w_old_cursor_lnum < from)
          from = wp->w_old_cursor_lnum;
        if (wp->w_old_cursor_lnum > to)
          to = wp->w_old_cursor_lnum;
        if (wp->w_old_visual_lnum < from)
          from = wp->w_old_visual_lnum;
        if (wp->w_old_visual_lnum > to)
          to = wp->w_old_visual_lnum;
      }
      else
      {
        /*
		 * Find the line numbers that need to be updated: The lines
		 * between the old cursor position and the current cursor
		 * position.  Also check if the Visual position changed.
		 */
        if (curwin->w_cursor.lnum < wp->w_old_cursor_lnum)
        {
          from = curwin->w_cursor.lnum;
          to = wp->w_old_cursor_lnum;
        }
        else
        {
          from = wp->w_old_cursor_lnum;
          to = curwin->w_cursor.lnum;
          if (from == 0) /* Visual mode just started */
            from = to;
        }

        if (VIsual.lnum != wp->w_old_visual_lnum || VIsual.col != wp->w_old_visual_col)
        {
          if (wp->w_old_visual_lnum < from && wp->w_old_visual_lnum != 0)
            from = wp->w_old_visual_lnum;
          if (wp->w_old_visual_lnum > to)
            to = wp->w_old_visual_lnum;
          if (VIsual.lnum < from)
            from = VIsual.lnum;
          if (VIsual.lnum > to)
            to = VIsual.lnum;
        }
      }

      /*
	     * If in block mode and changed column or curwin->w_curswant:
	     * update all lines.
	     * First compute the actual start and end column.
	     */
      if (VIsual_mode == Ctrl_V)
      {
        colnr_T fromc, toc;
#if defined(FEAT_LINEBREAK)
        int save_ve_flags = ve_flags;

        if (curwin->w_p_lbr)
          ve_flags = VE_ALL;
#endif
        getvcols(wp, &VIsual, &curwin->w_cursor, &fromc, &toc);
#if defined(FEAT_LINEBREAK)
        ve_flags = save_ve_flags;
#endif
        ++toc;
        if (curwin->w_curswant == MAXCOL)
          toc = MAXCOL;

        if (fromc != wp->w_old_cursor_fcol || toc != wp->w_old_cursor_lcol)
        {
          if (from > VIsual.lnum)
            from = VIsual.lnum;
          if (to < VIsual.lnum)
            to = VIsual.lnum;
        }
        wp->w_old_cursor_fcol = fromc;
        wp->w_old_cursor_lcol = toc;
      }
    }
    else
    {
      /* Use the line numbers of the old Visual area. */
      if (wp->w_old_cursor_lnum < wp->w_old_visual_lnum)
      {
        from = wp->w_old_cursor_lnum;
        to = wp->w_old_visual_lnum;
      }
      else
      {
        from = wp->w_old_visual_lnum;
        to = wp->w_old_cursor_lnum;
      }
    }

    /*
	 * There is no need to update lines above the top of the window.
	 */
    if (from < wp->w_topline)
      from = wp->w_topline;

    /*
	 * If we know the value of w_botline, use it to restrict the update to
	 * the lines that are visible in the window.
	 */
    if (wp->w_valid & VALID_BOTLINE)
    {
      if (from >= wp->w_botline)
        from = wp->w_botline - 1;
      if (to >= wp->w_botline)
        to = wp->w_botline - 1;
    }

    /*
	 * Find the minimal part to be updated.
	 * Watch out for scrolling that made entries in w_lines[] invalid.
	 * E.g., CTRL-U makes the first half of w_lines[] invalid and sets
	 * top_end; need to redraw from top_end to the "to" line.
	 * A middle mouse click with a Visual selection may change the text
	 * above the Visual area and reset wl_valid, do count these for
	 * mid_end (in srow).
	 */
    if (mid_start > 0)
    {
      lnum = wp->w_topline;
      idx = 0;
      srow = 0;
      if (scrolled_down)
        mid_start = top_end;
      else
        mid_start = 0;
      while (lnum < from && idx < wp->w_lines_valid) /* find start */
      {
        if (wp->w_lines[idx].wl_valid)
          mid_start += wp->w_lines[idx].wl_size;
        else if (!scrolled_down)
          srow += wp->w_lines[idx].wl_size;
        ++idx;
#ifdef FEAT_FOLDING
        if (idx < wp->w_lines_valid && wp->w_lines[idx].wl_valid)
          lnum = wp->w_lines[idx].wl_lnum;
        else
#endif
          ++lnum;
      }
      srow += mid_start;
      mid_end = wp->w_height;
      for (; idx < wp->w_lines_valid; ++idx) /* find end */
      {
        if (wp->w_lines[idx].wl_valid && wp->w_lines[idx].wl_lnum >= to + 1)
        {
          /* Only update until first row of this line */
          mid_end = srow;
          break;
        }
        srow += wp->w_lines[idx].wl_size;
      }
    }
  }

  if (VIsual_active && buf == curwin->w_buffer)
  {
    wp->w_old_visual_mode = VIsual_mode;
    wp->w_old_cursor_lnum = curwin->w_cursor.lnum;
    wp->w_old_visual_lnum = VIsual.lnum;
    wp->w_old_visual_col = VIsual.col;
    wp->w_old_curswant = curwin->w_curswant;
  }
  else
  {
    wp->w_old_visual_mode = 0;
    wp->w_old_cursor_lnum = 0;
    wp->w_old_visual_lnum = 0;
    wp->w_old_visual_col = 0;
  }

#if defined(FEAT_SEARCH_EXTRA)
  /* reset got_int, otherwise regexp won't work */
  save_got_int = got_int;
  got_int = 0;
#endif
#ifdef SYN_TIME_LIMIT
  /* Set the time limit to 'redrawtime'. */
  profile_setlimit(p_rdt, &syntax_tm);
  syn_set_timeout(&syntax_tm);
#endif
#ifdef FEAT_FOLDING
  win_foldinfo.fi_level = 0;
#endif

  /*
     * Update all the window rows.
     */
  idx = 0; /* first entry in w_lines[].wl_size */
  row = 0;
  srow = 0;
  lnum = wp->w_topline; /* first line shown in window */
  for (;;)
  {
    /* stop updating when reached the end of the window (check for _past_
	 * the end of the window is at the end of the loop) */
    if (row == wp->w_height)
    {
      didline = TRUE;
      break;
    }

    /* stop updating when hit the end of the file */
    if (lnum > buf->b_ml.ml_line_count)
    {
      eof = TRUE;
      break;
    }

    /* Remember the starting row of the line that is going to be dealt
	 * with.  It is used further down when the line doesn't fit. */
    srow = row;

    /*
	 * Update a line when it is in an area that needs updating, when it
	 * has changes or w_lines[idx] is invalid.
	 * "bot_start" may be halfway a wrapped line after using
	 * win_del_lines(), check if the current line includes it.
	 * When syntax folding is being used, the saved syntax states will
	 * already have been updated, we can't see where the syntax state is
	 * the same again, just update until the end of the window.
	 */
    if (row < top_end || (row >= mid_start && row < mid_end)
#ifdef FEAT_SEARCH_EXTRA
        || top_to_mod
#endif
        || idx >= wp->w_lines_valid || (row + wp->w_lines[idx].wl_size > bot_start) || (mod_top != 0 && (lnum == mod_top || (lnum >= mod_top && (lnum < mod_bot
#ifdef FEAT_SEARCH_EXTRA
                                                                                                                                                 /* match in fixed position might need redraw
				 * if lines were inserted or deleted */
                                                                                                                                                 || (wp->w_match_head != NULL && buf->b_mod_xlines != 0)
#endif
                                                                                                                                                     )))))
    {
#ifdef FEAT_SEARCH_EXTRA
      if (lnum == mod_top)
        top_to_mod = FALSE;
#endif

      /*
	     * When at start of changed lines: May scroll following lines
	     * up or down to minimize redrawing.
	     * Don't do this when the change continues until the end.
	     * Don't scroll when dollar_vcol >= 0, keep the "$".
	     */
      if (lnum == mod_top && mod_bot != MAXLNUM && !(dollar_vcol >= 0 && mod_bot == mod_top + 1))
      {
        int old_rows = 0;
        int new_rows = 0;
        int xtra_rows;
        linenr_T l;

        /* Count the old number of window rows, using w_lines[], which
		 * should still contain the sizes for the lines as they are
		 * currently displayed. */
        for (i = idx; i < wp->w_lines_valid; ++i)
        {
          /* Only valid lines have a meaningful wl_lnum.  Invalid
		     * lines are part of the changed area. */
          if (wp->w_lines[i].wl_valid && wp->w_lines[i].wl_lnum == mod_bot)
            break;
          old_rows += wp->w_lines[i].wl_size;
#ifdef FEAT_FOLDING
          if (wp->w_lines[i].wl_valid && wp->w_lines[i].wl_lastlnum + 1 == mod_bot)
          {
            /* Must have found the last valid entry above mod_bot.
			 * Add following invalid entries. */
            ++i;
            while (i < wp->w_lines_valid && !wp->w_lines[i].wl_valid)
              old_rows += wp->w_lines[i++].wl_size;
            break;
          }
#endif
        }

        if (i >= wp->w_lines_valid)
        {
          /* We can't find a valid line below the changed lines,
		     * need to redraw until the end of the window.
		     * Inserting/deleting lines has no use. */
          bot_start = 0;
        }
        else
        {
          /* Able to count old number of rows: Count new window
		     * rows, and may insert/delete lines */
          j = idx;
          for (l = lnum; l < mod_bot; ++l)
          {
#ifdef FEAT_FOLDING
            if (hasFoldingWin(wp, l, NULL, &l, TRUE, NULL))
              ++new_rows;
            else
#endif
#ifdef FEAT_DIFF
                if (l == wp->w_topline)
              new_rows += plines_win_nofill(wp, l, TRUE) + wp->w_topfill;
            else
#endif
              new_rows += plines_win(wp, l, TRUE);
            ++j;
            if (new_rows > wp->w_height - row - 2)
            {
              /* it's getting too much, must redraw the rest */
              new_rows = 9999;
              break;
            }
          }
          xtra_rows = new_rows - old_rows;
          if (xtra_rows < 0)
          {
            /* May scroll text up.  If there is not enough
			 * remaining text or scrolling fails, must redraw the
			 * rest.  If scrolling works, must redraw the text
			 * below the scrolled text. */
            if (row - xtra_rows >= wp->w_height - 2)
              mod_bot = MAXLNUM;
            else
            {
              check_for_delay(FALSE);
              if (win_del_lines(wp, row,
                                -xtra_rows, FALSE, FALSE, 0) == FAIL)
                mod_bot = MAXLNUM;
              else
                bot_start = wp->w_height + xtra_rows;
            }
          }
          else if (xtra_rows > 0)
          {
            /* May scroll text down.  If there is not enough
			 * remaining text of scrolling fails, must redraw the
			 * rest. */
            if (row + xtra_rows >= wp->w_height - 2)
              mod_bot = MAXLNUM;
            else
            {
              check_for_delay(FALSE);
              if (win_ins_lines(wp, row + old_rows,
                                xtra_rows, FALSE, FALSE) == FAIL)
                mod_bot = MAXLNUM;
              else if (top_end > row + old_rows)
                /* Scrolled the part at the top that requires
				 * updating down. */
                top_end += xtra_rows;
            }
          }

          /* When not updating the rest, may need to move w_lines[]
		     * entries. */
          if (mod_bot != MAXLNUM && i != j)
          {
            if (j < i)
            {
              int x = row + new_rows;

              /* move entries in w_lines[] upwards */
              for (;;)
              {
                /* stop at last valid entry in w_lines[] */
                if (i >= wp->w_lines_valid)
                {
                  wp->w_lines_valid = j;
                  break;
                }
                wp->w_lines[j] = wp->w_lines[i];
                /* stop at a line that won't fit */
                if (x + (int)wp->w_lines[j].wl_size > wp->w_height)
                {
                  wp->w_lines_valid = j + 1;
                  break;
                }
                x += wp->w_lines[j++].wl_size;
                ++i;
              }
              if (bot_start > x)
                bot_start = x;
            }
            else /* j > i */
            {
              /* move entries in w_lines[] downwards */
              j -= i;
              wp->w_lines_valid += j;
              if (wp->w_lines_valid > wp->w_height)
                wp->w_lines_valid = wp->w_height;
              for (i = wp->w_lines_valid; i - j >= idx; --i)
                wp->w_lines[i] = wp->w_lines[i - j];

              /* The w_lines[] entries for inserted lines are
			     * now invalid, but wl_size may be used above.
			     * Reset to zero. */
              while (i >= idx)
              {
                wp->w_lines[i].wl_size = 0;
                wp->w_lines[i--].wl_valid = FALSE;
              }
            }
          }
        }
      }

#ifdef FEAT_FOLDING
      /*
	     * When lines are folded, display one line for all of them.
	     * Otherwise, display normally (can be several display lines when
	     * 'wrap' is on).
	     */
      fold_count = foldedCount(wp, lnum, &win_foldinfo);
      if (fold_count != 0)
      {
        fold_line(wp, fold_count, &win_foldinfo, lnum, row);
        ++row;
        --fold_count;
        wp->w_lines[idx].wl_folded = TRUE;
        wp->w_lines[idx].wl_lastlnum = lnum + fold_count;
      }
      else
#endif
          if (idx < wp->w_lines_valid && wp->w_lines[idx].wl_valid && wp->w_lines[idx].wl_lnum == lnum && lnum > wp->w_topline && !(dy_flags & (DY_LASTLINE | DY_TRUNCATE)) && srow + wp->w_lines[idx].wl_size > wp->w_height
#ifdef FEAT_DIFF
              && diff_check_fill(wp, lnum) == 0
#endif
          )
      {
        /* This line is not going to fit.  Don't draw anything here,
		 * will draw "@  " lines below. */
        row = wp->w_height + 1;
      }
      else
      {
#ifdef FEAT_SEARCH_EXTRA
        prepare_search_hl(wp, lnum);
#endif

        /*
		 * Display one line.
		 */
        row = win_line(wp, lnum, srow, wp->w_height,
                       mod_top == 0, FALSE);

#ifdef FEAT_FOLDING
        wp->w_lines[idx].wl_folded = FALSE;
        wp->w_lines[idx].wl_lastlnum = lnum;
#endif
      }

      wp->w_lines[idx].wl_lnum = lnum;
      wp->w_lines[idx].wl_valid = TRUE;

      /* Past end of the window or end of the screen. Note that after
	     * resizing wp->w_height may be end up too big. That's a problem
	     * elsewhere, but prevent a crash here. */
      if (row > wp->w_height || row + wp->w_winrow >= Rows)
      {
        /* we may need the size of that too long line later on */
        if (dollar_vcol == -1)
          wp->w_lines[idx].wl_size = plines_win(wp, lnum, TRUE);
        ++idx;
        break;
      }
      if (dollar_vcol == -1)
        wp->w_lines[idx].wl_size = row - srow;
      ++idx;
#ifdef FEAT_FOLDING
      lnum += fold_count + 1;
#else
      ++lnum;
#endif
    }
    else
    {
      if (wp->w_p_rnu)
      {
#ifdef FEAT_FOLDING
        // 'relativenumber' set: The text doesn't need to be drawn, but
        // the number column nearly always does.
        fold_count = foldedCount(wp, lnum, &win_foldinfo);
        if (fold_count != 0)
          fold_line(wp, fold_count, &win_foldinfo, lnum, row);
        else
#endif
          (void)win_line(wp, lnum, srow, wp->w_height, TRUE, TRUE);
      }

      // This line does not need to be drawn, advance to the next one.
      row += wp->w_lines[idx++].wl_size;
      if (row > wp->w_height) /* past end of screen */
        break;
#ifdef FEAT_FOLDING
      lnum = wp->w_lines[idx - 1].wl_lastlnum + 1;
#else
      ++lnum;
#endif
    }

    if (lnum > buf->b_ml.ml_line_count)
    {
      eof = TRUE;
      break;
    }
  }
  /*
     * End of loop over all window lines.
     */

  if (idx > wp->w_lines_valid)
    wp->w_lines_valid = idx;

  /*
     * If we didn't hit the end of the file, and we didn't finish the last
     * line we were working on, then the line didn't fit.
     */
  wp->w_empty_rows = 0;
#ifdef FEAT_DIFF
  wp->w_filler_rows = 0;
#endif
  if (!eof && !didline)
  {
    if (lnum == wp->w_topline)
    {
      /*
	     * Single line that does not fit!
	     * Don't overwrite it, it can be edited.
	     */
      wp->w_botline = lnum + 1;
    }
#ifdef FEAT_DIFF
    else if (diff_check_fill(wp, lnum) >= wp->w_height - srow)
    {
      /* Window ends in filler lines. */
      wp->w_botline = lnum;
      wp->w_filler_rows = wp->w_height - srow;
    }
#endif
    else if (dy_flags & DY_TRUNCATE) /* 'display' has "truncate" */
    {
      int scr_row = W_WINROW(wp) + wp->w_height - 1;

      /*
	     * Last line isn't finished: Display "@@@" in the last screen line.
	     */
      screen_puts_len((char_u *)"@@", 2, scr_row, wp->w_wincol,
                      HL_ATTR(HLF_AT));
      screen_fill(scr_row, scr_row + 1,
                  (int)wp->w_wincol + 2, (int)W_ENDCOL(wp),
                  '@', ' ', HL_ATTR(HLF_AT));
      set_empty_rows(wp, srow);
      wp->w_botline = lnum;
    }
    else if (dy_flags & DY_LASTLINE) /* 'display' has "lastline" */
    {
      /*
	     * Last line isn't finished: Display "@@@" at the end.
	     */
      screen_fill(W_WINROW(wp) + wp->w_height - 1,
                  W_WINROW(wp) + wp->w_height,
                  (int)W_ENDCOL(wp) - 3, (int)W_ENDCOL(wp),
                  '@', '@', HL_ATTR(HLF_AT));
      set_empty_rows(wp, srow);
      wp->w_botline = lnum;
    }
    else
    {
      win_draw_end(wp, '@', ' ', TRUE, srow, wp->w_height, HLF_AT);
      wp->w_botline = lnum;
    }
  }
  else
  {
    draw_vsep_win(wp, row);
    if (eof) /* we hit the end of the file */
    {
      wp->w_botline = buf->b_ml.ml_line_count + 1;
#ifdef FEAT_DIFF
      j = diff_check_fill(wp, wp->w_botline);
      if (j > 0 && !wp->w_botfill)
      {
        // Display filler lines at the end of the file.
        if (char2cells(fill_diff) > 1)
          i = '-';
        else
          i = fill_diff;
        if (row + j > wp->w_height)
          j = wp->w_height - row;
        win_draw_end(wp, i, i, TRUE, row, row + (int)j, HLF_DED);
        row += j;
      }
#endif
    }
    else if (dollar_vcol == -1)
      wp->w_botline = lnum;

    // Make sure the rest of the screen is blank
    // put '~'s on rows that aren't part of the file.
    win_draw_end(wp,
                 '~',
                 ' ', FALSE, row, wp->w_height, HLF_EOB);
  }

#ifdef SYN_TIME_LIMIT
  syn_set_timeout(NULL);
#endif

  /* Reset the type of redrawing required, the window has been updated. */
  wp->w_redr_type = 0;
#ifdef FEAT_DIFF
  wp->w_old_topfill = wp->w_topfill;
  wp->w_old_botfill = wp->w_botfill;
#endif

  if (dollar_vcol == -1)
  {
    /*
	 * There is a trick with w_botline.  If we invalidate it on each
	 * change that might modify it, this will cause a lot of expensive
	 * calls to plines() in update_topline() each time.  Therefore the
	 * value of w_botline is often approximated, and this value is used to
	 * compute the value of w_topline.  If the value of w_botline was
	 * wrong, check that the value of w_topline is correct (cursor is on
	 * the visible part of the text).  If it's not, we need to redraw
	 * again.  Mostly this just means scrolling up a few lines, so it
	 * doesn't look too bad.  Only do this for the current window (where
	 * changes are relevant).
	 */
    wp->w_valid |= VALID_BOTLINE;
    if (wp == curwin && wp->w_botline != old_botline && !recursive)
    {
      recursive = TRUE;
      curwin->w_valid &= ~VALID_TOPLINE;
      update_topline(); /* may invalidate w_botline again */
      if (must_redraw != 0)
      {
        /* Don't update for changes in buffer again. */
        i = curbuf->b_mod_set;
        curbuf->b_mod_set = FALSE;
        win_update(curwin);
        must_redraw = 0;
        curbuf->b_mod_set = i;
      }
      recursive = FALSE;
    }
  }

#if defined(FEAT_SEARCH_EXTRA)
  /* restore got_int, unless CTRL-C was hit while redrawing */
  if (!got_int)
    got_int = save_got_int;
#endif
}

/*
 * Call screen_fill() with the columns adjusted for 'rightleft' if needed.
 * Return the new offset.
 */
static int
screen_fill_end(
    win_T *wp,
    int c1,
    int c2,
    int off,
    int width,
    int row,
    int endrow,
    int attr)
{
  int nn = off + width;

  if (nn > wp->w_width)
    nn = wp->w_width;
#ifdef FEAT_RIGHTLEFT
  if (wp->w_p_rl)
  {
    screen_fill(W_WINROW(wp) + row, W_WINROW(wp) + endrow,
                W_ENDCOL(wp) - nn, (int)W_ENDCOL(wp) - off,
                c1, c2, attr);
  }
  else
#endif
    screen_fill(W_WINROW(wp) + row, W_WINROW(wp) + endrow,
                wp->w_wincol + off, (int)wp->w_wincol + nn,
                c1, c2, attr);
  return nn;
}

/*
 * Clear lines near the end the window and mark the unused lines with "c1".
 * use "c2" as the filler character.
 * When "draw_margin" is TRUE then draw the sign, fold and number columns.
 */
static void
win_draw_end(
    win_T *wp,
    int c1,
    int c2,
    int draw_margin,
    int row,
    int endrow,
    hlf_T hl)
{
  int n = 0;
  int attr = HL_ATTR(hl);
  int wcr_attr = get_wcr_attr(wp);

  attr = hl_combine_attr(wcr_attr, attr);

  if (draw_margin)
  {
#ifdef FEAT_FOLDING
    int fdc = compute_foldcolumn(wp, 0);

    if (fdc > 0)
      // draw the fold column
      n = screen_fill_end(wp, ' ', ' ', n, fdc,
                          row, endrow, hl_combine_attr(wcr_attr, HL_ATTR(HLF_FC)));
#endif
#ifdef FEAT_SIGNS
    if (signcolumn_on(wp))
      // draw the sign column
      n = screen_fill_end(wp, ' ', ' ', n, 2,
                          row, endrow, hl_combine_attr(wcr_attr, HL_ATTR(HLF_SC)));
#endif
    if ((wp->w_p_nu || wp->w_p_rnu) && vim_strchr(p_cpo, CPO_NUMCOL) == NULL)
      // draw the number column
      n = screen_fill_end(wp, ' ', ' ', n, number_width(wp) + 1,
                          row, endrow, hl_combine_attr(wcr_attr, HL_ATTR(HLF_N)));
  }

#ifdef FEAT_RIGHTLEFT
  if (wp->w_p_rl)
  {
    screen_fill(W_WINROW(wp) + row, W_WINROW(wp) + endrow,
                wp->w_wincol, W_ENDCOL(wp) - 1 - n,
                c2, c2, attr);
    screen_fill(W_WINROW(wp) + row, W_WINROW(wp) + endrow,
                W_ENDCOL(wp) - 1 - n, W_ENDCOL(wp) - n,
                c1, c2, attr);
  }
  else
#endif
  {
    screen_fill(W_WINROW(wp) + row, W_WINROW(wp) + endrow,
                wp->w_wincol + n, (int)W_ENDCOL(wp),
                c1, c2, attr);
  }

  set_empty_rows(wp, row);
}

#ifdef FEAT_FOLDING
/*
 * Compute the width of the foldcolumn.  Based on 'foldcolumn' and how much
 * space is available for window "wp", minus "col".
 */
static int
compute_foldcolumn(win_T *wp, int col)
{
  int fdc = wp->w_p_fdc;
  int wmw = wp == curwin && p_wmw == 0 ? 1 : p_wmw;
  int wwidth = wp->w_width;

  if (fdc > wwidth - (col + wmw))
    fdc = wwidth - (col + wmw);
  return fdc;
}

#if defined(FEAT_FOLDING)
/*	
 * Copy "text" to ScreenLines using "attr".	
 * Returns the next screen column.	
 */
static int
text_to_screenline(win_T *wp, char_u *text, int col)
{
  int off = (int)(current_ScreenLine - ScreenLines);

  if (has_mbyte)
  {
    int cells;
    int u8c, u8cc[MAX_MCO];
    int i;
    int idx;
    int c_len;
    char_u *p;
#ifdef FEAT_ARABIC
    int prev_c = 0;  /* previous Arabic character */
    int prev_c1 = 0; /* first composing char for prev_c */
#endif

#ifdef FEAT_RIGHTLEFT
    if (wp->w_p_rl)
      idx = off;
    else
#endif
      idx = off + col;

    /* Store multibyte characters in ScreenLines[] et al. correctly. */
    for (p = text; *p != NUL;)
    {
      cells = (*mb_ptr2cells)(p);
      c_len = (*mb_ptr2len)(p);
      if (col + cells > wp->w_width
#ifdef FEAT_RIGHTLEFT
                            - (wp->w_p_rl ? col : 0)
#endif
      )
        break;
      ScreenLines[idx] = *p;
      if (enc_utf8)
      {
        u8c = utfc_ptr2char(p, u8cc);
        if (*p < 0x80 && u8cc[0] == 0)
        {
          ScreenLinesUC[idx] = 0;
#ifdef FEAT_ARABIC
          prev_c = u8c;
#endif
        }
        else
        {
#ifdef FEAT_ARABIC
          if (p_arshape && !p_tbidi && ARABIC_CHAR(u8c))
          {
            /* Do Arabic shaping. */
            int pc, pc1, nc;
            int pcc[MAX_MCO];
            int firstbyte = *p;

            /* The idea of what is the previous and next	
			 * character depends on 'rightleft'. */
            if (wp->w_p_rl)
            {
              pc = prev_c;
              pc1 = prev_c1;
              nc = utf_ptr2char(p + c_len);
              prev_c1 = u8cc[0];
            }
            else
            {
              pc = utfc_ptr2char(p + c_len, pcc);
              nc = prev_c;
              pc1 = pcc[0];
            }
            prev_c = u8c;

            u8c = arabic_shape(u8c, &firstbyte, &u8cc[0],
                               pc, pc1, nc);
            ScreenLines[idx] = firstbyte;
          }
          else
            prev_c = u8c;
#endif
          /* Non-BMP character: display as ? or fullwidth ?. */
          ScreenLinesUC[idx] = u8c;
          for (i = 0; i < Screen_mco; ++i)
          {
            ScreenLinesC[i][idx] = u8cc[i];
            if (u8cc[i] == 0)
              break;
          }
        }
        if (cells > 1)
          ScreenLines[idx + 1] = 0;
      }
      else if (enc_dbcs == DBCS_JPNU && *p == 0x8e)
        /* double-byte single width character */
        ScreenLines2[idx] = p[1];
      else if (cells > 1)
        /* double-width character */
        ScreenLines[idx + 1] = p[1];
      col += cells;
      idx += cells;
      p += c_len;
    }
  }
  else
  {
    int len = (int)STRLEN(text);

    if (len > wp->w_width - col)
      len = wp->w_width - col;
    if (len > 0)
    {
#ifdef FEAT_RIGHTLEFT
      if (wp->w_p_rl)
        mch_memmove(current_ScreenLine, text, len);
      else
#endif
        mch_memmove(current_ScreenLine + col, text, len);
      col += len;
    }
  }
  return col;
}
#endif
/*
 * Display one folded line.
 */
static void
fold_line(
    win_T *wp,
    long fold_count,
    foldinfo_T *foldinfo,
    linenr_T lnum,
    int row)
{
  char_u buf[FOLD_TEXT_LEN];
  pos_T *top, *bot;
  linenr_T lnume = lnum + fold_count - 1;
  int len;
  char_u *text;
  int fdc;
  int col;
  int txtcol;
  int off = (int)(current_ScreenLine - ScreenLines);
  int ri;

  /* Build the fold line:
     * 1. Add the cmdwin_type for the command-line window
     * 2. Add the 'foldcolumn'
     * 3. Add the 'number' or 'relativenumber' column
     * 4. Compose the text
     * 5. Add the text
     * 6. set highlighting for the Visual area an other text
     */
  col = 0;

  /*
     * 1. Add the cmdwin_type for the command-line window
     * Ignores 'rightleft', this window is never right-left.
     */
  /*
     * 2. Add the 'foldcolumn'
     *    Reduce the width when there is not enough space.
     */
  fdc = compute_foldcolumn(wp, col);
  if (fdc > 0)
  {
    fill_foldcolumn(buf, wp, TRUE, lnum);
#ifdef FEAT_RIGHTLEFT
    if (wp->w_p_rl)
    {
      int i;

      copy_text_attr(off + wp->w_width - fdc - col, buf, fdc,
                     HL_ATTR(HLF_FC));
      /* reverse the fold column */
      for (i = 0; i < fdc; ++i)
        ScreenLines[off + wp->w_width - i - 1 - col] = buf[i];
    }
    else
#endif
      copy_text_attr(off + col, buf, fdc, HL_ATTR(HLF_FC));
    col += fdc;
  }

#ifdef FEAT_RIGHTLEFT
#define RL_MEMSET(p, v, l)                                     \
  do                                                           \
  {                                                            \
    if (wp->w_p_rl)                                            \
      for (ri = 0; ri < l; ++ri)                               \
        ScreenAttrs[off + (wp->w_width - (p) - (l)) + ri] = v; \
    else                                                       \
      for (ri = 0; ri < l; ++ri)                               \
        ScreenAttrs[off + (p) + ri] = v;                       \
  } while (0)
#else
#define RL_MEMSET(p, v, l)             \
  do                                   \
  {                                    \
    for (ri = 0; ri < l; ++ri)         \
      ScreenAttrs[off + (p) + ri] = v; \
  } while (0)
#endif

  /* Set all attributes of the 'number' or 'relativenumber' column and the
     * text */
  RL_MEMSET(col, HL_ATTR(HLF_FL), wp->w_width - col);

#ifdef FEAT_SIGNS
  /* If signs are being displayed, add two spaces. */
  if (signcolumn_on(wp))
  {
    len = wp->w_width - col;
    if (len > 0)
    {
      if (len > 2)
        len = 2;
#ifdef FEAT_RIGHTLEFT
      if (wp->w_p_rl)
        /* the line number isn't reversed */
        copy_text_attr(off + wp->w_width - len - col,
                       (char_u *)"  ", len, HL_ATTR(HLF_FL));
      else
#endif
        copy_text_attr(off + col, (char_u *)"  ", len, HL_ATTR(HLF_FL));
      col += len;
    }
  }
#endif

  /*
     * 3. Add the 'number' or 'relativenumber' column
     */
  if (wp->w_p_nu || wp->w_p_rnu)
  {
    len = wp->w_width - col;
    if (len > 0)
    {
      int w = number_width(wp);
      long num;
      char *fmt = "%*ld ";

      if (len > w + 1)
        len = w + 1;

      if (wp->w_p_nu && !wp->w_p_rnu)
        /* 'number' + 'norelativenumber' */
        num = (long)lnum;
      else
      {
        /* 'relativenumber', don't use negative numbers */
        num = labs((long)get_cursor_rel_lnum(wp, lnum));
        if (num == 0 && wp->w_p_nu && wp->w_p_rnu)
        {
          /* 'number' + 'relativenumber': cursor line shows absolute
		     * line number */
          num = lnum;
          fmt = "%-*ld ";
        }
      }

      sprintf((char *)buf, fmt, w, num);
#ifdef FEAT_RIGHTLEFT
      if (wp->w_p_rl)
        /* the line number isn't reversed */
        copy_text_attr(off + wp->w_width - len - col, buf, len,
                       HL_ATTR(HLF_FL));
      else
#endif
        copy_text_attr(off + col, buf, len, HL_ATTR(HLF_FL));
      col += len;
    }
  }

  /*
     * 4. Compose the folded-line string with 'foldtext', if set.
     */
  text = get_foldtext(wp, lnum, lnume, foldinfo, buf);

  txtcol = col; /* remember where text starts */

  /*
     * 5. move the text to current_ScreenLine.  Fill up with "fill_fold".
     *    Right-left text is put in columns 0 - number-col, normal text is put
     *    in columns number-col - window-width.
     */
  col = text_to_screenline(wp, text, col);

  /* Fill the rest of the line with the fold filler */
#ifdef FEAT_RIGHTLEFT
  if (wp->w_p_rl)
    col -= txtcol;
#endif
  while (col < wp->w_width
#ifdef FEAT_RIGHTLEFT
                   - (wp->w_p_rl ? txtcol : 0)
#endif
  )
  {
    if (enc_utf8)
    {
      if (fill_fold >= 0x80)
      {
        ScreenLinesUC[off + col] = fill_fold;
        ScreenLinesC[0][off + col] = 0;
        ScreenLines[off + col] = 0x80; /* avoid storing zero */
      }
      else
      {
        ScreenLinesUC[off + col] = 0;
        ScreenLines[off + col] = fill_fold;
      }
      col++;
    }
    else
      ScreenLines[off + col++] = fill_fold;
  }

  if (text != buf)
    vim_free(text);

  /*
     * 6. set highlighting for the Visual area an other text.
     * If all folded lines are in the Visual area, highlight the line.
     */
  if (VIsual_active && wp->w_buffer == curwin->w_buffer)
  {
    if (LTOREQ_POS(curwin->w_cursor, VIsual))
    {
      /* Visual is after curwin->w_cursor */
      top = &curwin->w_cursor;
      bot = &VIsual;
    }
    else
    {
      /* Visual is before curwin->w_cursor */
      top = &VIsual;
      bot = &curwin->w_cursor;
    }
    if (lnum >= top->lnum && lnume <= bot->lnum && (VIsual_mode != 'v' || ((lnum > top->lnum || (lnum == top->lnum && top->col == 0)) && (lnume < bot->lnum || (lnume == bot->lnum && (bot->col - (*p_sel == 'e')) >= (colnr_T)STRLEN(ml_get_buf(wp->w_buffer, lnume, FALSE)))))))
    {
      if (VIsual_mode == Ctrl_V)
      {
        /* Visual block mode: highlight the chars part of the block */
        if (wp->w_old_cursor_fcol + txtcol < (colnr_T)wp->w_width)
        {
          if (wp->w_old_cursor_lcol != MAXCOL && wp->w_old_cursor_lcol + txtcol < (colnr_T)wp->w_width)
            len = wp->w_old_cursor_lcol;
          else
            len = wp->w_width - txtcol;
          RL_MEMSET(wp->w_old_cursor_fcol + txtcol, HL_ATTR(HLF_V),
                    len - (int)wp->w_old_cursor_fcol);
        }
      }
      else
      {
        /* Set all attributes of the text */
        RL_MEMSET(txtcol, HL_ATTR(HLF_V), wp->w_width - txtcol);
      }
    }
  }

  screen_line(row + W_WINROW(wp), wp->w_wincol, (int)wp->w_width,
              (int)wp->w_width, 0);

  /*
     * Update w_cline_height and w_cline_folded if the cursor line was
     * updated (saves a call to plines() later).
     */
  if (wp == curwin && lnum <= curwin->w_cursor.lnum && lnume >= curwin->w_cursor.lnum)
  {
    curwin->w_cline_row = row;
    curwin->w_cline_height = 1;
    curwin->w_cline_folded = TRUE;
    curwin->w_valid |= (VALID_CHEIGHT | VALID_CROW);
  }
}

/*
 * Copy "buf[len]" to ScreenLines["off"] and set attributes to "attr".
 */
static void
copy_text_attr(
    int off,
    char_u *buf,
    int len,
    int attr)
{
  int i;

  mch_memmove(ScreenLines + off, buf, (size_t)len);
  if (enc_utf8)
    vim_memset(ScreenLinesUC + off, 0, sizeof(u8char_T) * (size_t)len);
  for (i = 0; i < len; ++i)
    ScreenAttrs[off + i] = attr;
}

/*
 * Fill the foldcolumn at "p" for window "wp".
 * Only to be called when 'foldcolumn' > 0.
 */
static void
fill_foldcolumn(
    char_u *p,
    win_T *wp,
    int closed,    /* TRUE of FALSE */
    linenr_T lnum) /* current line number */
{
  int i = 0;
  int level;
  int first_level;
  int empty;
  int fdc = compute_foldcolumn(wp, 0);

  /* Init to all spaces. */
  vim_memset(p, ' ', (size_t)fdc);

  level = win_foldinfo.fi_level;
  if (level > 0)
  {
    /* If there is only one column put more info in it. */
    empty = (fdc == 1) ? 0 : 1;

    /* If the column is too narrow, we start at the lowest level that
	 * fits and use numbers to indicated the depth. */
    first_level = level - fdc - closed + 1 + empty;
    if (first_level < 1)
      first_level = 1;

    for (i = 0; i + empty < fdc; ++i)
    {
      if (win_foldinfo.fi_lnum == lnum && first_level + i >= win_foldinfo.fi_low_level)
        p[i] = '-';
      else if (first_level == 1)
        p[i] = '|';
      else if (first_level + i <= 9)
        p[i] = '0' + first_level + i;
      else
        p[i] = '>';
      if (first_level + i == level)
        break;
    }
  }
  if (closed)
    p[i >= fdc ? i - 1 : i] = '+';
}
#endif /* FEAT_FOLDING */

/*
 * Display line "lnum" of window 'wp' on the screen.
 * Start at row "startrow", stop when "endrow" is reached.
 * wp->w_virtcol needs to be valid.
 *
 * Return the number of last row the line occupies.
 */
static int
win_line(
    win_T *wp,
    linenr_T lnum,
    int startrow,
    int endrow,
    int nochange UNUSED, // not updating for changed text
    int number_only)     // only update the number column
{
  int col = 0;   /* visual column on screen */
  unsigned off;  /* offset in ScreenLines/ScreenAttrs */
  int c = 0;     /* init for GCC */
  long vcol = 0; /* virtual column (for tabs) */
#ifdef FEAT_LINEBREAK
  long vcol_sbr = -1; /* virtual column after showbreak */
#endif
  long vcol_prev = -1; /* "vcol" of previous character */
  char_u *line;        /* current line */
  char_u *ptr;         /* current position in "line" */
  int row;             /* row in the window, excl w_winrow */
  int screen_row;      /* row on the screen, incl w_winrow */

  char_u extra[18];                         /* "%ld" and 'fdc' must fit in here */
  int n_extra = 0;                          /* number of extra chars */
  char_u *p_extra = NULL;                   /* string of extra chars, plus NUL */
  char_u *p_extra_free = NULL;              /* p_extra needs to be freed */
  int c_extra = NUL;                        /* extra chars, all the same */
  int c_final = NUL;                        /* final char, mandatory if set */
  int extra_attr = 0;                       /* attributes when n_extra != 0 */
  static char_u *at_end_str = (char_u *)""; /* used for p_extra when
					   displaying lcs_eol at end-of-line */
  int lcs_eol_one = lcs_eol;                /* lcs_eol until it's been used */
  int lcs_prec_todo = lcs_prec;             /* lcs_prec until it's been used */

  /* saved "extra" items for when draw_state becomes WL_LINE (again) */
  int saved_n_extra = 0;
  char_u *saved_p_extra = NULL;
  int saved_c_extra = 0;
  int saved_c_final = 0;
  int saved_char_attr = 0;

  int n_attr = 0;      /* chars with special attr */
  int saved_attr2 = 0; /* char_attr saved for n_attr */
  int n_attr3 = 0;     /* chars with overruling special attr */
  int saved_attr3 = 0; /* char_attr saved for n_attr3 */

  int n_skip = 0; /* nr of chars to skip for 'nowrap' */

  int fromcol, tocol;    /* start/end of inverting */
  int fromcol_prev = -2; /* start of inverting after cursor */
  int noinvcur = FALSE;  /* don't invert the cursor */
  pos_T *top, *bot;
  int lnum_in_visual_area = FALSE;
  pos_T pos;
  long v;

  int char_attr = 0;             // attributes for next character
  int attr_pri = FALSE;          // char_attr has priority
  int area_highlighting = FALSE; // Visual or incsearch highlighting
                                 // in this line
  int vi_attr = 0;               // attributes for Visual and incsearch
                                 // highlighting
  int wcr_attr = 0;              // attributes from 'wincolor'
  int win_attr = 0;              // background for whole window, except
                                 // margins and "~" lines.
  int area_attr = 0;             // attributes desired by highlighting
  int search_attr = 0;           // attributes desired by 'hlsearch'
  int extra_check = 0;           // has extra highlighting
  int multi_attr = 0;            /* attributes desired by multibyte */
  int mb_l = 1;                  /* multi-byte byte length */
  int mb_c = 0;                  /* decoded multi-byte character */
  int mb_utf8 = FALSE;           /* screen char is UTF-8 char */
  int u8cc[MAX_MCO];             /* composing UTF-8 chars */
#ifdef FEAT_DIFF
  int filler_lines;          /* nr of filler lines to be drawn */
  int filler_todo;           /* nr of filler lines still to do + 1 */
  hlf_T diff_hlf = (hlf_T)0; /* type of diff highlighting */
  int change_start = MAXCOL; /* first col of changed area */
  int change_end = -1;       /* last col of changed area */
#endif
  colnr_T trailcol = MAXCOL; /* start of trailing spaces */
#ifdef FEAT_LINEBREAK
  int need_showbreak = FALSE; /* overlong line, skipping first x
					   chars */
#endif
#if defined(FEAT_SIGNS) || defined(FEAT_QUICKFIX) || defined(FEAT_DIFF)
#define LINE_ATTR
  int line_attr = 0; /* attribute for the whole line */
#endif
#ifdef FEAT_SEARCH_EXTRA
  matchitem_T *cur;    /* points to the match list */
  match_T *shl;        /* points to search_hl or a match */
  int shl_flag;        /* flag to indicate whether search_hl
					   has been processed or not */
  int pos_inprogress;  /* marks that position match search is
					   in progress */
  int prevcol_hl_flag; /* flag to indicate whether prevcol
					   equals startcol of search_hl or one
					   of the matches */
#endif
#ifdef FEAT_ARABIC
  int prev_c = 0;  /* previous Arabic character */
  int prev_c1 = 0; /* first composing char for prev_c */
#endif
#if defined(LINE_ATTR)
  int did_line_attr = 0;
#endif

  /* draw_state: items that are drawn in sequence: */
#define WL_START 0 /* nothing done yet */
#define WL_CMDLINE WL_START
#ifdef FEAT_FOLDING
#define WL_FOLD WL_CMDLINE + 1 /* 'foldcolumn' */
#else
#define WL_FOLD WL_CMDLINE
#endif
#ifdef FEAT_SIGNS
#define WL_SIGN WL_FOLD + 1 /* column for signs */
#else
#define WL_SIGN WL_FOLD /* column for signs */
#endif
#define WL_NR WL_SIGN + 1 /* line number */
#ifdef FEAT_LINEBREAK
#define WL_BRI WL_NR + 1 /* 'breakindent' */
#else
#define WL_BRI WL_NR
#endif
#if defined(FEAT_LINEBREAK) || defined(FEAT_DIFF)
#define WL_SBR WL_BRI + 1 /* 'showbreak' or 'diff' */
#else
#define WL_SBR WL_BRI
#endif
#define WL_LINE WL_SBR + 1   /* text in the line */
  int draw_state = WL_START; /* what to draw next */
#if defined(FEAT_XIM) && defined(FEAT_GUI_GTK)
  int feedback_col = 0;
  int feedback_old_attr = -1;
#endif
  int screen_line_flags = 0;

#define VCOL_HLC (vcol)

  if (startrow > endrow) /* past the end already! */
    return startrow;

  row = startrow;
  screen_row = row + W_WINROW(wp);

  if (!number_only)
  {
    /*
	 * To speed up the loop below, set extra_check when there is linebreak,
	 * trailing white space and/or syntax processing to be done.
	 */
#ifdef FEAT_LINEBREAK
    extra_check = wp->w_p_lbr;
#endif

#ifdef FEAT_TERMINAL
    if (term_show_buffer(wp->w_buffer))
    {
      extra_check = TRUE;
      win_attr = term_get_attr(wp->w_buffer, lnum, -1);
    }
#endif

    /*
	 * handle visual active in this window
	 */
    fromcol = -10;
    tocol = MAXCOL;
    if (VIsual_active && wp->w_buffer == curwin->w_buffer)
    {
      /* Visual is after curwin->w_cursor */
      if (LTOREQ_POS(curwin->w_cursor, VIsual))
      {
        top = &curwin->w_cursor;
        bot = &VIsual;
      }
      else /* Visual is before curwin->w_cursor */
      {
        top = &VIsual;
        bot = &curwin->w_cursor;
      }
      lnum_in_visual_area = (lnum >= top->lnum && lnum <= bot->lnum);
      if (VIsual_mode == Ctrl_V) /* block mode */
      {
        if (lnum_in_visual_area)
        {
          fromcol = wp->w_old_cursor_fcol;
          tocol = wp->w_old_cursor_lcol;
        }
      }
      else /* non-block mode */
      {
        if (lnum > top->lnum && lnum <= bot->lnum)
          fromcol = 0;
        else if (lnum == top->lnum)
        {
          if (VIsual_mode == 'V') /* linewise */
            fromcol = 0;
          else
          {
            getvvcol(wp, top, (colnr_T *)&fromcol, NULL, NULL);
            if (gchar_pos(top) == NUL)
              tocol = fromcol + 1;
          }
        }
        if (VIsual_mode != 'V' && lnum == bot->lnum)
        {
          if (*p_sel == 'e' && bot->col == 0 && bot->coladd == 0)
          {
            fromcol = -10;
            tocol = MAXCOL;
          }
          else if (bot->col == MAXCOL)
            tocol = MAXCOL;
          else
          {
            pos = *bot;
            if (*p_sel == 'e')
              getvvcol(wp, &pos, (colnr_T *)&tocol, NULL, NULL);
            else
            {
              getvvcol(wp, &pos, NULL, NULL, (colnr_T *)&tocol);
              ++tocol;
            }
          }
        }
      }

      /* Check if the character under the cursor should not be inverted */
      if (!highlight_match && lnum == curwin->w_cursor.lnum && wp == curwin
#ifdef FEAT_GUI
          && !gui.in_use
#endif
      )
        noinvcur = TRUE;

      /* if inverting in this line set area_highlighting */
      if (fromcol >= 0)
      {
        area_highlighting = TRUE;
        vi_attr = HL_ATTR(HLF_V);
#if defined(FEAT_CLIPBOARD) && defined(FEAT_X11)
        if ((clip_star.available && !clip_star.owned && clip_isautosel_star()) || (clip_plus.available && !clip_plus.owned && clip_isautosel_plus()))
          vi_attr = HL_ATTR(HLF_VNC);
#endif
      }
    }

    /*
	 * handle 'incsearch' and ":s///c" highlighting
	 */
    else if (highlight_match && wp == curwin && lnum >= curwin->w_cursor.lnum && lnum <= curwin->w_cursor.lnum + search_match_lines)
    {
      if (lnum == curwin->w_cursor.lnum)
        getvcol(curwin, &(curwin->w_cursor),
                (colnr_T *)&fromcol, NULL, NULL);
      else
        fromcol = 0;
      if (lnum == curwin->w_cursor.lnum + search_match_lines)
      {
        pos.lnum = lnum;
        pos.col = search_match_endcol;
        getvcol(curwin, &pos, (colnr_T *)&tocol, NULL, NULL);
      }
      else
        tocol = MAXCOL;
      /* do at least one character; happens when past end of line */
      if (fromcol == tocol)
        tocol = fromcol + 1;
      area_highlighting = TRUE;
      vi_attr = HL_ATTR(HLF_I);
    }
  }

#ifdef FEAT_DIFF
  filler_lines = diff_check(wp, lnum);
  if (filler_lines < 0)
  {
    if (filler_lines == -1)
    {
      if (diff_find_change(wp, lnum, &change_start, &change_end))
        diff_hlf = HLF_ADD; /* added line */
      else if (change_start == 0)
        diff_hlf = HLF_TXD; /* changed text */
      else
        diff_hlf = HLF_CHD; /* changed line */
    }
    else
      diff_hlf = HLF_ADD; /* added line */
    filler_lines = 0;
    area_highlighting = TRUE;
  }
  if (lnum == wp->w_topline)
    filler_lines = wp->w_topfill;
  filler_todo = filler_lines;
#endif

#ifdef LINE_ATTR
#ifdef FEAT_SIGNS
  /* If this line has a sign with line highlighting set line_attr. */
  v = buf_getsigntype(wp->w_buffer, lnum, SIGN_LINEHL);
  if (v != 0)
    line_attr = sign_get_attr((int)v, TRUE);
#endif
#if defined(FEAT_QUICKFIX)
  /* Highlight the current line in the quickfix window. */
  if (bt_quickfix(wp->w_buffer) && qf_current_entry(wp) == lnum)
    line_attr = HL_ATTR(HLF_QFL);
#endif
  if (line_attr != 0)
    area_highlighting = TRUE;
#endif

  line = ml_get_buf(wp->w_buffer, lnum, FALSE);
  ptr = line;

  if (wp->w_p_list)
  {
    if (lcs_space || lcs_trail || lcs_nbsp)
      extra_check = TRUE;
    /* find start of trailing whitespace */
    if (lcs_trail)
    {
      trailcol = (colnr_T)STRLEN(ptr);
      while (trailcol > (colnr_T)0 && VIM_ISWHITE(ptr[trailcol - 1]))
        --trailcol;
      trailcol += (colnr_T)(ptr - line);
    }
  }

  wcr_attr = get_wcr_attr(wp);
  if (wcr_attr != 0)
  {
    win_attr = wcr_attr;
    area_highlighting = TRUE;
  }

  /*
     * 'nowrap' or 'wrap' and a single line that doesn't fit: Advance to the
     * first character to be displayed.
     */
  if (wp->w_p_wrap)
    v = wp->w_skipcol;
  else
    v = wp->w_leftcol;
  if (v > 0 && !number_only)
  {
    char_u *prev_ptr = ptr;

    while (vcol < v && *ptr != NUL)
    {
      c = win_lbr_chartabsize(wp, line, ptr, (colnr_T)vcol, NULL);
      vcol += c;
      prev_ptr = ptr;
      MB_PTR_ADV(ptr);
    }

    /* When:
	 * - 'cuc' is set, or
	 * - 'colorcolumn' is set, or
	 * - 'virtualedit' is set, or
	 * - the visual mode is active,
	 * the end of the line may be before the start of the displayed part.
	 */
    if (vcol < v && (virtual_active() ||
                     (VIsual_active && wp->w_buffer == curwin->w_buffer)))
      vcol = v;

    /* Handle a character that's not completely on the screen: Put ptr at
	 * that character but skip the first few screen characters. */
    if (vcol > v)
    {
      vcol -= c;
      ptr = prev_ptr;
      /* If the character fits on the screen, don't need to skip it.
	     * Except for a TAB. */
      if (((*mb_ptr2cells)(ptr) >= c || *ptr == TAB) && col == 0)
        n_skip = v - vcol;
    }

    /*
	 * Adjust for when the inverted text is before the screen,
	 * and when the start of the inverted text is before the screen.
	 */
    if (tocol <= vcol)
      fromcol = 0;
    else if (fromcol >= 0 && fromcol < vcol)
      fromcol = vcol;

#ifdef FEAT_LINEBREAK
    /* When w_skipcol is non-zero, first line needs 'showbreak' */
    if (wp->w_p_wrap)
      need_showbreak = TRUE;
#endif
  }

  /*
     * Correct highlighting for cursor that can't be disabled.
     * Avoids having to check this for each character.
     */
  if (fromcol >= 0)
  {
    if (noinvcur)
    {
      if ((colnr_T)fromcol == wp->w_virtcol)
      {
        /* highlighting starts at cursor, let it start just after the
		 * cursor */
        fromcol_prev = fromcol;
        fromcol = -1;
      }
      else if ((colnr_T)fromcol < wp->w_virtcol)
        /* restart highlighting after the cursor */
        fromcol_prev = wp->w_virtcol;
    }
    if (fromcol >= tocol)
      fromcol = -1;
  }

#ifdef FEAT_SEARCH_EXTRA
  /*
     * Handle highlighting the last used search pattern and matches.
     * Do this for both search_hl and the match list.
     * Not in a popup window.
     */
  cur = wp->w_match_head;
  shl_flag = FALSE;
  while ((cur != NULL || shl_flag == FALSE) && !number_only && !(screen_line_flags & SLF_POPUP))
  {
    if (shl_flag == FALSE)
    {
      shl = &search_hl;
      shl_flag = TRUE;
    }
    else
      shl = &cur->hl;
    shl->startcol = MAXCOL;
    shl->endcol = MAXCOL;
    shl->attr_cur = 0;
    shl->is_addpos = FALSE;
    v = (long)(ptr - line);
    if (cur != NULL)
      cur->pos.cur = 0;
    next_search_hl(wp, shl, lnum, (colnr_T)v,
                   shl == &search_hl ? NULL : cur);

    /* Need to get the line again, a multi-line regexp may have made it
	 * invalid. */
    line = ml_get_buf(wp->w_buffer, lnum, FALSE);
    ptr = line + v;

    if (shl->lnum != 0 && shl->lnum <= lnum)
    {
      if (shl->lnum == lnum)
        shl->startcol = shl->rm.startpos[0].col;
      else
        shl->startcol = 0;
      if (lnum == shl->lnum + shl->rm.endpos[0].lnum - shl->rm.startpos[0].lnum)
        shl->endcol = shl->rm.endpos[0].col;
      else
        shl->endcol = MAXCOL;
      /* Highlight one character for an empty match. */
      if (shl->startcol == shl->endcol)
      {
        if (has_mbyte && line[shl->endcol] != NUL)
          shl->endcol += (*mb_ptr2len)(line + shl->endcol);
        else
          ++shl->endcol;
      }
      if ((long)shl->startcol < v) /* match at leftcol */
      {
        shl->attr_cur = shl->attr;
        search_attr = shl->attr;
      }
      area_highlighting = TRUE;
    }
    if (shl != &search_hl && cur != NULL)
      cur = cur->next;
  }
#endif

  off = (unsigned)(current_ScreenLine - ScreenLines);
  col = 0;

#ifdef FEAT_RIGHTLEFT
  if (wp->w_p_rl)
  {
    /* Rightleft window: process the text in the normal direction, but put
	 * it in current_ScreenLine[] from right to left.  Start at the
	 * rightmost column of the window. */
    col = wp->w_width - 1;
    off += col;
    screen_line_flags |= SLF_RIGHTLEFT;
  }
#endif

  /*
     * Repeat for the whole displayed line.
     */
  for (;;)
  {
    /* Skip this quickly when working on the text. */
    if (draw_state != WL_LINE)
    {

#ifdef FEAT_FOLDING
      if (draw_state == WL_FOLD - 1 && n_extra == 0)
      {
        int fdc = compute_foldcolumn(wp, 0);

        draw_state = WL_FOLD;
        if (fdc > 0)
        {
          /* Draw the 'foldcolumn'.  Allocate a buffer, "extra" may
		     * already be in use. */
          vim_free(p_extra_free);
          p_extra_free = alloc(12 + 1);

          if (p_extra_free != NULL)
          {
            fill_foldcolumn(p_extra_free, wp, FALSE, lnum);
            n_extra = fdc;
            p_extra_free[n_extra] = NUL;
            p_extra = p_extra_free;
            c_extra = NUL;
            c_final = NUL;
            char_attr = hl_combine_attr(wcr_attr, HL_ATTR(HLF_FC));
          }
        }
      }
#endif

#ifdef FEAT_SIGNS
      if (draw_state == WL_SIGN - 1 && n_extra == 0)
      {
        draw_state = WL_SIGN;
        /* Show the sign column when there are any signs in this
		 * buffer or when using Netbeans. */
        if (signcolumn_on(wp))
        {
          int text_sign;
#ifdef FEAT_SIGN_ICONS
          int icon_sign;
#endif

          /* Draw two cells with the sign value or blank. */
          c_extra = ' ';
          c_final = NUL;
          char_attr = hl_combine_attr(wcr_attr, HL_ATTR(HLF_SC));
          n_extra = 2;

          if (row == startrow
#ifdef FEAT_DIFF
                         + filler_lines &&
              filler_todo <= 0
#endif
          )
          {
            text_sign = buf_getsigntype(wp->w_buffer, lnum,
                                        SIGN_TEXT);
#ifdef FEAT_SIGN_ICONS
            icon_sign = buf_getsigntype(wp->w_buffer, lnum,
                                        SIGN_ICON);
            if (gui.in_use && icon_sign != 0)
            {
              /* Use the image in this position. */
              c_extra = SIGN_BYTE;
              c_final = NUL;
              char_attr = icon_sign;
            }
            else
#endif
                if (text_sign != 0)
            {
              p_extra = sign_get_text(text_sign);
              if (p_extra != NULL)
              {
                c_extra = NUL;
                c_final = NUL;
                n_extra = (int)STRLEN(p_extra);
              }
              char_attr = sign_get_attr(text_sign, FALSE);
            }
          }
        }
      }
#endif

      if (draw_state == WL_NR - 1 && n_extra == 0)
      {
        draw_state = WL_NR;
        /* Display the absolute or relative line number. After the
		 * first fill with blanks when the 'n' flag isn't in 'cpo' */
        if ((wp->w_p_nu || wp->w_p_rnu) && (row == startrow
#ifdef FEAT_DIFF
                                                       + filler_lines
#endif
                                            || vim_strchr(p_cpo, CPO_NUMCOL) == NULL))
        {
          /* Draw the line number (empty space after wrapping). */
          if (row == startrow
#ifdef FEAT_DIFF
                         + filler_lines
#endif
          )
          {
            long num;
            char *fmt = "%*ld ";

            if (wp->w_p_nu && !wp->w_p_rnu)
              /* 'number' + 'norelativenumber' */
              num = (long)lnum;
            else
            {
              /* 'relativenumber', don't use negative numbers */
              num = labs((long)get_cursor_rel_lnum(wp, lnum));
              if (num == 0 && wp->w_p_nu && wp->w_p_rnu)
              {
                /* 'number' + 'relativenumber' */
                num = lnum;
                fmt = "%-*ld ";
              }
            }

            sprintf((char *)extra, fmt,
                    number_width(wp), num);
            if (wp->w_skipcol > 0)
              for (p_extra = extra; *p_extra == ' '; ++p_extra)
                *p_extra = '-';
#ifdef FEAT_RIGHTLEFT
            if (wp->w_p_rl) /* reverse line numbers */
            {
              char_u *p1, *p2;
              int t;

              // like rl_mirror(), but keep the space at the end
              p2 = skiptowhite(extra) - 1;
              for (p1 = extra; p1 < p2; ++p1, --p2)
              {
                t = *p1;
                *p1 = *p2;
                *p2 = t;
              }
            }
#endif
            p_extra = extra;
            c_extra = NUL;
            c_final = NUL;
          }
          else
          {
            c_extra = ' ';
            c_final = NUL;
          }
          n_extra = number_width(wp) + 1;
          char_attr = hl_combine_attr(wcr_attr, HL_ATTR(HLF_N));
        }
      }

#ifdef FEAT_LINEBREAK
      if (wp->w_p_brisbr && draw_state == WL_BRI - 1 && n_extra == 0 && *p_sbr != NUL)
        /* draw indent after showbreak value */
        draw_state = WL_BRI;
      else if (wp->w_p_brisbr && draw_state == WL_SBR && n_extra == 0)
        /* After the showbreak, draw the breakindent */
        draw_state = WL_BRI - 1;

      /* draw 'breakindent': indent wrapped text accordingly */
      if (draw_state == WL_BRI - 1 && n_extra == 0)
      {
        draw_state = WL_BRI;
        /* if need_showbreak is set, breakindent also applies */
        if (wp->w_p_bri && n_extra == 0 && (row != startrow || need_showbreak)
#ifdef FEAT_DIFF
            && filler_lines == 0
#endif
        )
        {
          char_attr = 0;
#ifdef FEAT_DIFF
          if (diff_hlf != (hlf_T)0)
          {
            char_attr = HL_ATTR(diff_hlf);
          }
#endif
          p_extra = NULL;
          c_extra = ' ';
          n_extra = get_breakindent_win(wp,
                                        ml_get_buf(wp->w_buffer, lnum, FALSE));
          /* Correct end of highlighted area for 'breakindent',
		     * required when 'linebreak' is also set. */
          if (tocol == vcol)
            tocol += n_extra;
        }
      }
#endif

#if defined(FEAT_LINEBREAK) || defined(FEAT_DIFF)
      if (draw_state == WL_SBR - 1 && n_extra == 0)
      {
        draw_state = WL_SBR;
#ifdef FEAT_DIFF
        if (filler_todo > 0)
        {
          /* Draw "deleted" diff line(s). */
          if (char2cells(fill_diff) > 1)
          {
            c_extra = '-';
            c_final = NUL;
          }
          else
          {
            c_extra = fill_diff;
            c_final = NUL;
          }
#ifdef FEAT_RIGHTLEFT
          if (wp->w_p_rl)
            n_extra = col + 1;
          else
#endif
            n_extra = wp->w_width - col;
          char_attr = HL_ATTR(HLF_DED);
        }
#endif
#ifdef FEAT_LINEBREAK
        if (*p_sbr != NUL && need_showbreak)
        {
          /* Draw 'showbreak' at the start of each broken line. */
          p_extra = p_sbr;
          c_extra = NUL;
          c_final = NUL;
          n_extra = (int)STRLEN(p_sbr);
          char_attr = HL_ATTR(HLF_AT);
          need_showbreak = FALSE;
          vcol_sbr = vcol + MB_CHARLEN(p_sbr);
          /* Correct end of highlighted area for 'showbreak',
		     * required when 'linebreak' is also set. */
          if (tocol == vcol)
            tocol += n_extra;
        }
#endif
      }
#endif

      if (draw_state == WL_LINE - 1 && n_extra == 0)
      {
        draw_state = WL_LINE;
        if (saved_n_extra)
        {
          /* Continue item from end of wrapped line. */
          n_extra = saved_n_extra;
          c_extra = saved_c_extra;
          c_final = saved_c_final;
          p_extra = saved_p_extra;
          char_attr = saved_char_attr;
        }
        else
          char_attr = win_attr;
      }
    }

    // When still displaying '$' of change command, stop at cursor.
    // When only displaying the (relative) line number and that's done,
    // stop here.
    if ((dollar_vcol >= 0 && wp == curwin && lnum == wp->w_cursor.lnum && vcol >= (long)wp->w_virtcol
#ifdef FEAT_DIFF
         && filler_todo <= 0
#endif
         ) ||
        (number_only && draw_state > WL_NR))
    {
      screen_line(screen_row, wp->w_wincol, col, -(int)wp->w_width,
                  screen_line_flags);
      /* Pretend we have finished updating the window.  Except when
	     * 'cursorcolumn' is set. */
      row = wp->w_height;
      break;
    }

    if (draw_state == WL_LINE && (area_highlighting))
    {
      /* handle Visual or match highlighting in this line */
      if (vcol == fromcol || (has_mbyte && vcol + 1 == fromcol && n_extra == 0 && (*mb_ptr2cells)(ptr) > 1) || ((int)vcol_prev == fromcol_prev && vcol_prev < vcol /* not at margin */
                                                                                                                && vcol < tocol))
        area_attr = vi_attr; /* start highlighting */
      else if (area_attr != 0 && (vcol == tocol || (noinvcur && (colnr_T)vcol == wp->w_virtcol)))
        area_attr = 0; /* stop highlighting */

#ifdef FEAT_SEARCH_EXTRA
      if (!n_extra)
      {
        /*
		 * Check for start/end of search pattern match.
		 * After end, check for start/end of next match.
		 * When another match, have to check for start again.
		 * Watch out for matching an empty string!
		 * Do this for 'search_hl' and the match list (ordered by
		 * priority).
		 */
        v = (long)(ptr - line);
        cur = wp->w_match_head;
        shl_flag = FALSE;
        while (cur != NULL || shl_flag == FALSE)
        {
          if (shl_flag == FALSE && ((cur != NULL && cur->priority > SEARCH_HL_PRIORITY) || cur == NULL))
          {
            shl = &search_hl;
            shl_flag = TRUE;
          }
          else
            shl = &cur->hl;
          if (cur != NULL)
            cur->pos.cur = 0;
          pos_inprogress = TRUE;
          while (shl->rm.regprog != NULL || (cur != NULL && pos_inprogress))
          {
            if (shl->startcol != MAXCOL && v >= (long)shl->startcol && v < (long)shl->endcol)
            {
              int tmp_col = v + MB_PTR2LEN(ptr);

              if (shl->endcol < tmp_col)
                shl->endcol = tmp_col;
              shl->attr_cur = shl->attr;
            }
            else if (v == (long)shl->endcol)
            {
              shl->attr_cur = 0;
              next_search_hl(wp, shl, lnum, (colnr_T)v,
                             shl == &search_hl ? NULL : cur);
              pos_inprogress = cur == NULL || cur->pos.cur == 0
                                   ? FALSE
                                   : TRUE;

              /* Need to get the line again, a multi-line regexp
			     * may have made it invalid. */
              line = ml_get_buf(wp->w_buffer, lnum, FALSE);
              ptr = line + v;

              if (shl->lnum == lnum)
              {
                shl->startcol = shl->rm.startpos[0].col;
                if (shl->rm.endpos[0].lnum == 0)
                  shl->endcol = shl->rm.endpos[0].col;
                else
                  shl->endcol = MAXCOL;

                if (shl->startcol == shl->endcol)
                {
                  /* highlight empty match, try again after
				     * it */
                  if (has_mbyte)
                    shl->endcol += (*mb_ptr2len)(line + shl->endcol);
                  else
                    ++shl->endcol;
                }

                /* Loop to check if the match starts at the
				 * current position */
                continue;
              }
            }
            break;
          }
          if (shl != &search_hl && cur != NULL)
            cur = cur->next;
        }

        /* Use attributes from match with highest priority among
		 * 'search_hl' and the match list. */
        search_attr = search_hl.attr_cur;
        cur = wp->w_match_head;
        shl_flag = FALSE;
        while (cur != NULL || shl_flag == FALSE)
        {
          if (shl_flag == FALSE && ((cur != NULL && cur->priority > SEARCH_HL_PRIORITY) || cur == NULL))
          {
            shl = &search_hl;
            shl_flag = TRUE;
          }
          else
            shl = &cur->hl;
          if (shl->attr_cur != 0)
            search_attr = shl->attr_cur;
          if (shl != &search_hl && cur != NULL)
            cur = cur->next;
        }
        /* Only highlight one character after the last column. */
        if (*ptr == NUL && (did_line_attr >= 1 || (wp->w_p_list && lcs_eol_one == -1)))
          search_attr = 0;
      }
#endif

#ifdef FEAT_DIFF
      if (diff_hlf != (hlf_T)0)
      {
        if (diff_hlf == HLF_CHD && ptr - line >= change_start && n_extra == 0)
          diff_hlf = HLF_TXD; /* changed text */
        if (diff_hlf == HLF_TXD && ptr - line > change_end && n_extra == 0)
          diff_hlf = HLF_CHD; /* changed line */
        line_attr = HL_ATTR(diff_hlf);
      }
#endif

      /* Decide which of the highlight attributes to use. */
      attr_pri = TRUE;
#ifdef LINE_ATTR
      if (area_attr != 0)
        char_attr = hl_combine_attr(line_attr, area_attr);
      else if (search_attr != 0)
        char_attr = hl_combine_attr(line_attr, search_attr);
      else if (line_attr != 0 && ((fromcol == -10 && tocol == MAXCOL) || vcol < fromcol || vcol_prev < fromcol_prev || vcol >= tocol))
        // Use line_attr when not in the Visual or 'incsearch' area
        // (area_attr may be 0 when "noinvcur" is set).
        char_attr = line_attr;
#else
      if (area_attr != 0)
        char_attr = area_attr;
      else if (search_attr != 0)
        char_attr = search_attr;
#endif
      else
      {
        attr_pri = FALSE;
        char_attr = 0;
      }
    }
    if (char_attr == 0)
      char_attr = win_attr;

    /*
	 * Get the next character to put on the screen.
	 */
    /*
	 * The "p_extra" points to the extra stuff that is inserted to
	 * represent special characters (non-printable stuff) and other
	 * things.  When all characters are the same, c_extra is used.
	 * If c_final is set, it will compulsorily be used at the end.
	 * "p_extra" must end in a NUL to avoid mb_ptr2len() reads past
	 * "p_extra[n_extra]".
	 * For the '$' of the 'list' option, n_extra == 1, p_extra == "".
	 */
    if (n_extra > 0)
    {
      if (c_extra != NUL || (n_extra == 1 && c_final != NUL))
      {
        c = (n_extra == 1 && c_final != NUL) ? c_final : c_extra;
        mb_c = c; /* doesn't handle non-utf-8 multi-byte! */
        if (enc_utf8 && utf_char2len(c) > 1)
        {
          mb_utf8 = TRUE;
          u8cc[0] = 0;
          c = 0xc0;
        }
        else
          mb_utf8 = FALSE;
      }
      else
      {
        c = *p_extra;
        if (has_mbyte)
        {
          mb_c = c;
          if (enc_utf8)
          {
            /* If the UTF-8 character is more than one byte:
			 * Decode it into "mb_c". */
            mb_l = utfc_ptr2len(p_extra);
            mb_utf8 = FALSE;
            if (mb_l > n_extra)
              mb_l = 1;
            else if (mb_l > 1)
            {
              mb_c = utfc_ptr2char(p_extra, u8cc);
              mb_utf8 = TRUE;
              c = 0xc0;
            }
          }
          else
          {
            /* if this is a DBCS character, put it in "mb_c" */
            mb_l = MB_BYTE2LEN(c);
            if (mb_l >= n_extra)
              mb_l = 1;
            else if (mb_l > 1)
              mb_c = (c << 8) + p_extra[1];
          }
          if (mb_l == 0) /* at the NUL at end-of-line */
            mb_l = 1;

          /* If a double-width char doesn't fit display a '>' in the
		     * last column. */
          if ((
#ifdef FEAT_RIGHTLEFT
                  wp->w_p_rl ? (col <= 0) :
#endif
                             (col >= wp->w_width - 1)) &&
              (*mb_char2cells)(mb_c) == 2)
          {
            c = '>';
            mb_c = c;
            mb_l = 1;
            mb_utf8 = FALSE;
            multi_attr = HL_ATTR(HLF_AT);
            /* put the pointer back to output the double-width
			 * character at the start of the next line. */
            ++n_extra;
            --p_extra;
          }
          else
          {
            n_extra -= mb_l - 1;
            p_extra += mb_l - 1;
          }
        }
        ++p_extra;
      }
      --n_extra;
    }
    else
    {
#ifdef FEAT_LINEBREAK
      int c0;
#endif

      if (p_extra_free != NULL)
        VIM_CLEAR(p_extra_free);
      /*
	     * Get a character from the line itself.
	     */
      c = *ptr;
#ifdef FEAT_LINEBREAK
      c0 = *ptr;
#endif
      if (has_mbyte)
      {
        mb_c = c;
        if (enc_utf8)
        {
          /* If the UTF-8 character is more than one byte: Decode it
		     * into "mb_c". */
          mb_l = utfc_ptr2len(ptr);
          mb_utf8 = FALSE;
          if (mb_l > 1)
          {
            mb_c = utfc_ptr2char(ptr, u8cc);
            /* Overlong encoded ASCII or ASCII with composing char
			 * is displayed normally, except a NUL. */
            if (mb_c < 0x80)
            {
              c = mb_c;
#ifdef FEAT_LINEBREAK
              c0 = mb_c;
#endif
            }
            mb_utf8 = TRUE;

            /* At start of the line we can have a composing char.
			 * Draw it as a space with a composing char. */
            if (utf_iscomposing(mb_c))
            {
              int i;

              for (i = Screen_mco - 1; i > 0; --i)
                u8cc[i] = u8cc[i - 1];
              u8cc[0] = mb_c;
              mb_c = ' ';
            }
          }

          if ((mb_l == 1 && c >= 0x80) || (mb_l >= 1 && mb_c == 0) || (mb_l > 1 && (!vim_isprintc(mb_c))))
          {
            /*
			 * Illegal UTF-8 byte: display as <xx>.
			 * Non-BMP character : display as ? or fullwidth ?.
			 */
            transchar_hex(extra, mb_c);
#ifdef FEAT_RIGHTLEFT
            if (wp->w_p_rl) /* reverse */
              rl_mirror(extra);
#endif
            p_extra = extra;
            c = *p_extra;
            mb_c = mb_ptr2char_adv(&p_extra);
            mb_utf8 = (c >= 0x80);
            n_extra = (int)STRLEN(p_extra);
            c_extra = NUL;
            c_final = NUL;
            if (area_attr == 0 && search_attr == 0)
            {
              n_attr = n_extra + 1;
              extra_attr = HL_ATTR(HLF_8);
              saved_attr2 = char_attr; /* save current attr */
            }
          }
          else if (mb_l == 0) /* at the NUL at end-of-line */
            mb_l = 1;
#ifdef FEAT_ARABIC
          else if (p_arshape && !p_tbidi && ARABIC_CHAR(mb_c))
          {
            /* Do Arabic shaping. */
            int pc, pc1, nc;
            int pcc[MAX_MCO];

            /* The idea of what is the previous and next
			 * character depends on 'rightleft'. */
            if (wp->w_p_rl)
            {
              pc = prev_c;
              pc1 = prev_c1;
              nc = utf_ptr2char(ptr + mb_l);
              prev_c1 = u8cc[0];
            }
            else
            {
              pc = utfc_ptr2char(ptr + mb_l, pcc);
              nc = prev_c;
              pc1 = pcc[0];
            }
            prev_c = mb_c;

            mb_c = arabic_shape(mb_c, &c, &u8cc[0], pc, pc1, nc);
          }
          else
            prev_c = mb_c;
#endif
        }
        else /* enc_dbcs */
        {
          mb_l = MB_BYTE2LEN(c);
          if (mb_l == 0) /* at the NUL at end-of-line */
            mb_l = 1;
          else if (mb_l > 1)
          {
            /* We assume a second byte below 32 is illegal.
			 * Hopefully this is OK for all double-byte encodings!
			 */
            if (ptr[1] >= 32)
              mb_c = (c << 8) + ptr[1];
            else
            {
              if (ptr[1] == NUL)
              {
                /* head byte at end of line */
                mb_l = 1;
                transchar_nonprint(extra, c);
              }
              else
              {
                /* illegal tail byte */
                mb_l = 2;
                STRCPY(extra, "XX");
              }
              p_extra = extra;
              n_extra = (int)STRLEN(extra) - 1;
              c_extra = NUL;
              c_final = NUL;
              c = *p_extra++;
              if (area_attr == 0 && search_attr == 0)
              {
                n_attr = n_extra + 1;
                extra_attr = HL_ATTR(HLF_8);
                saved_attr2 = char_attr; /* save current attr */
              }
              mb_c = c;
            }
          }
        }
        /* If a double-width char doesn't fit display a '>' in the
		 * last column; the character is displayed at the start of the
		 * next line. */
        if ((
#ifdef FEAT_RIGHTLEFT
                wp->w_p_rl ? (col <= 0) :
#endif
                           (col >= wp->w_width - 1)) &&
            (*mb_char2cells)(mb_c) == 2)
        {
          c = '>';
          mb_c = c;
          mb_utf8 = FALSE;
          mb_l = 1;
          multi_attr = HL_ATTR(HLF_AT);
          // Put pointer back so that the character will be
          // displayed at the start of the next line.
          --ptr;
        }
        else if (*ptr != NUL)
          ptr += mb_l - 1;

        /* If a double-width char doesn't fit at the left side display
		 * a '<' in the first column.  Don't do this for unprintable
		 * characters. */
        if (n_skip > 0 && mb_l > 1 && n_extra == 0)
        {
          n_extra = 1;
          c_extra = MB_FILLER_CHAR;
          c_final = NUL;
          c = ' ';
          if (area_attr == 0 && search_attr == 0)
          {
            n_attr = n_extra + 1;
            extra_attr = HL_ATTR(HLF_AT);
            saved_attr2 = char_attr; /* save current attr */
          }
          mb_c = c;
          mb_utf8 = FALSE;
          mb_l = 1;
        }
      }
      ++ptr;

      if (extra_check)
      {

#ifdef FEAT_LINEBREAK
        /*
		 * Found last space before word: check for line break.
		 */
        if (wp->w_p_lbr && c0 == c && VIM_ISBREAK(c) && !VIM_ISBREAK((int)*ptr))
        {
          int mb_off = has_mbyte ? (*mb_head_off)(line, ptr - 1) : 0;
          char_u *p = ptr - (mb_off + 1);

          /* TODO: is passing p for start of the line OK? */
          n_extra = win_lbr_chartabsize(wp, line, p, (colnr_T)vcol,
                                        NULL) -
                    1;
          if (c == TAB && n_extra + col > wp->w_width)
#ifdef FEAT_VARTABS
            n_extra = tabstop_padding(vcol, wp->w_buffer->b_p_ts,
                                      wp->w_buffer->b_p_vts_array) -
                      1;
#else
            n_extra = (int)wp->w_buffer->b_p_ts - vcol % (int)wp->w_buffer->b_p_ts - 1;
#endif

          c_extra = mb_off > 0 ? MB_FILLER_CHAR : ' ';
          c_final = NUL;
          if (VIM_ISWHITE(c))
          {
            if (!wp->w_p_list)
              c = ' ';
          }
        }
#endif

        // 'list': Change char 160 to lcs_nbsp and space to lcs_space.
        // But not when the character is followed by a composing
        // character (use mb_l to check that).
        if (wp->w_p_list && ((((c == 160 && mb_l == 1) || (mb_utf8 && ((mb_c == 160 && mb_l == 2) || (mb_c == 0x202f && mb_l == 3)))) && lcs_nbsp) || (c == ' ' && mb_l == 1 && lcs_space && ptr - line <= trailcol)))
        {
          c = (c == ' ') ? lcs_space : lcs_nbsp;
          if (area_attr == 0 && search_attr == 0)
          {
            n_attr = 1;
            extra_attr = HL_ATTR(HLF_8);
            saved_attr2 = char_attr; /* save current attr */
          }
          mb_c = c;
          if (enc_utf8 && utf_char2len(c) > 1)
          {
            mb_utf8 = TRUE;
            u8cc[0] = 0;
            c = 0xc0;
          }
          else
            mb_utf8 = FALSE;
        }

        if (trailcol != MAXCOL && ptr > line + trailcol && c == ' ')
        {
          c = lcs_trail;
          if (!attr_pri)
          {
            n_attr = 1;
            extra_attr = HL_ATTR(HLF_8);
            saved_attr2 = char_attr; /* save current attr */
          }
          mb_c = c;
          if (enc_utf8 && utf_char2len(c) > 1)
          {
            mb_utf8 = TRUE;
            u8cc[0] = 0;
            c = 0xc0;
          }
          else
            mb_utf8 = FALSE;
        }
      }

      /*
	     * Handling of non-printable characters.
	     */
      if (!vim_isprintc(c))
      {
        /*
		 * when getting a character from the file, we may have to
		 * turn it into something else on the way to putting it
		 * into "ScreenLines".
		 */
        if (c == TAB && (!wp->w_p_list || lcs_tab1))
        {
          int tab_len = 0;
          long vcol_adjusted = vcol; /* removed showbreak length */
#ifdef FEAT_LINEBREAK
          /* only adjust the tab_len, when at the first column
		     * after the showbreak value was drawn */
          if (*p_sbr != NUL && vcol == vcol_sbr && wp->w_p_wrap)
            vcol_adjusted = vcol - MB_CHARLEN(p_sbr);
#endif
            /* tab amount depends on current column */
#ifdef FEAT_VARTABS
          tab_len = tabstop_padding(vcol_adjusted,
                                    wp->w_buffer->b_p_ts,
                                    wp->w_buffer->b_p_vts_array) -
                    1;
#else
          tab_len = (int)wp->w_buffer->b_p_ts - vcol_adjusted % (int)wp->w_buffer->b_p_ts - 1;
#endif

#ifdef FEAT_LINEBREAK
          if (!wp->w_p_lbr || !wp->w_p_list)
#endif
            /* tab amount depends on current column */
            n_extra = tab_len;
#ifdef FEAT_LINEBREAK
          else
          {
            char_u *p;
            int len;
            int i;
            int saved_nextra = n_extra;

            /* if n_extra > 0, it gives the number of chars, to
			 * use for a tab, else we need to calculate the width
			 * for a tab */
            len = (tab_len * mb_char2len(lcs_tab2));
            if (n_extra > 0)
              len += n_extra - tab_len;
            c = lcs_tab1;
            p = alloc(len + 1);
            vim_memset(p, ' ', len);
            p[len] = NUL;
            vim_free(p_extra_free);
            p_extra_free = p;
            for (i = 0; i < tab_len; i++)
            {
              if (*p == NUL)
              {
                tab_len = i;
                break;
              }
              mb_char2bytes(lcs_tab2, p);
              p += mb_char2len(lcs_tab2);
              n_extra += mb_char2len(lcs_tab2) - (saved_nextra > 0 ? 1 : 0);
            }
            p_extra = p_extra_free;
          }
#endif
          mb_utf8 = FALSE; /* don't draw as UTF-8 */
          if (wp->w_p_list)
          {
            c = (n_extra == 0 && lcs_tab3) ? lcs_tab3 : lcs_tab1;
#ifdef FEAT_LINEBREAK
            if (wp->w_p_lbr)
              c_extra = NUL; /* using p_extra from above */
            else
#endif
              c_extra = lcs_tab2;
            c_final = lcs_tab3;
            n_attr = tab_len + 1;
            extra_attr = HL_ATTR(HLF_8);
            saved_attr2 = char_attr; /* save current attr */
            mb_c = c;
            if (enc_utf8 && utf_char2len(c) > 1)
            {
              mb_utf8 = TRUE;
              u8cc[0] = 0;
              c = 0xc0;
            }
          }
          else
          {
            c_final = NUL;
            c_extra = ' ';
            c = ' ';
          }
        }
        else if (c == NUL && (wp->w_p_list || ((fromcol >= 0 || fromcol_prev >= 0) && tocol > vcol && VIsual_mode != Ctrl_V && (
#ifdef FEAT_RIGHTLEFT
                                                                                                                                   wp->w_p_rl ? (col >= 0) :
#endif
                                                                                                                                              (col < wp->w_width)) &&
                                               !(noinvcur && lnum == wp->w_cursor.lnum && (colnr_T)vcol == wp->w_virtcol))) &&
                 lcs_eol_one > 0)
        {
          /* Display a '$' after the line or highlight an extra
		     * character if the line break is included. */
#if defined(FEAT_DIFF) || defined(LINE_ATTR)
          /* For a diff line the highlighting continues after the
		     * "$". */
          if (
#ifdef FEAT_DIFF
              diff_hlf == (hlf_T)0
#ifdef LINE_ATTR
              &&
#endif
#endif
#ifdef LINE_ATTR
              line_attr == 0
#endif
          )
#endif
          {
            /* In virtualedit, visual selections may extend
			 * beyond end of line. */
            if (area_highlighting && virtual_active() && tocol != MAXCOL && vcol < tocol)
              n_extra = 0;
            else
            {
              p_extra = at_end_str;
              n_extra = 1;
              c_extra = NUL;
              c_final = NUL;
            }
          }
          if (wp->w_p_list && lcs_eol > 0)
            c = lcs_eol;
          else
            c = ' ';
          lcs_eol_one = -1;
          --ptr; /* put it back at the NUL */
          if (!attr_pri)
          {
            extra_attr = HL_ATTR(HLF_AT);
            n_attr = 1;
          }
          mb_c = c;
          if (enc_utf8 && utf_char2len(c) > 1)
          {
            mb_utf8 = TRUE;
            u8cc[0] = 0;
            c = 0xc0;
          }
          else
            mb_utf8 = FALSE; /* don't draw as UTF-8 */
        }
        else if (c != NUL)
        {
          p_extra = transchar(c);
          if (n_extra == 0)
            n_extra = byte2cells(c) - 1;
#ifdef FEAT_RIGHTLEFT
          if ((dy_flags & DY_UHEX) && wp->w_p_rl)
            rl_mirror(p_extra); /* reverse "<12>" */
#endif
          c_extra = NUL;
          c_final = NUL;
#ifdef FEAT_LINEBREAK
          if (wp->w_p_lbr)
          {
            char_u *p;

            c = *p_extra;
            p = alloc(n_extra + 1);
            vim_memset(p, ' ', n_extra);
            STRNCPY(p, p_extra + 1, STRLEN(p_extra) - 1);
            p[n_extra] = NUL;
            vim_free(p_extra_free);
            p_extra_free = p_extra = p;
          }
          else
#endif
          {
            n_extra = byte2cells(c) - 1;
            c = *p_extra++;
          }
          if (!attr_pri)
          {
            n_attr = n_extra + 1;
            extra_attr = HL_ATTR(HLF_8);
            saved_attr2 = char_attr; /* save current attr */
          }
          mb_utf8 = FALSE; /* don't draw as UTF-8 */
        }
        else if (VIsual_active && (VIsual_mode == Ctrl_V || VIsual_mode == 'v') && virtual_active() && tocol != MAXCOL && vcol < tocol && (
#ifdef FEAT_RIGHTLEFT
                                                                                                                                              wp->w_p_rl ? (col >= 0) :
#endif
                                                                                                                                                         (col < wp->w_width)))
        {
          c = ' ';
          --ptr; /* put it back at the NUL */
        }
#if defined(LINE_ATTR)
        else if ((
#ifdef FEAT_DIFF
                     diff_hlf != (hlf_T)0 ||
#endif
#ifdef FEAT_TERMINAL
                     win_attr != 0 ||
#endif
                     line_attr != 0) &&
                 (
#ifdef FEAT_RIGHTLEFT
                     wp->w_p_rl ? (col >= 0) :
#endif
                                (col < wp->w_width)))
        {
          /* Highlight until the right side of the window */
          c = ' ';
          --ptr; /* put it back at the NUL */

          /* Remember we do the char for line highlighting. */
          ++did_line_attr;

          /* don't do search HL for the rest of the line */
          if (line_attr != 0 && char_attr == search_attr && (did_line_attr > 1 || (wp->w_p_list && lcs_eol > 0)))
            char_attr = line_attr;
#ifdef FEAT_DIFF
          if (diff_hlf == HLF_TXD)
          {
            diff_hlf = HLF_CHD;
            if (vi_attr == 0 || char_attr != vi_attr)
            {
              char_attr = HL_ATTR(diff_hlf);
            }
          }
#endif
#ifdef FEAT_TERMINAL
          if (win_attr != 0)
          {
            char_attr = win_attr;
          }
#endif
        }
#endif
      }
    }

    /* Don't override visual selection highlighting. */
    if (n_attr > 0 && draw_state == WL_LINE && !attr_pri)
      char_attr = extra_attr;

#if defined(FEAT_XIM) && defined(FEAT_GUI_GTK)
    /* XIM don't send preedit_start and preedit_end, but they send
	 * preedit_changed and commit.  Thus Vim can't set "im_is_active", use
	 * im_is_preediting() here. */
    if (p_imst == IM_ON_THE_SPOT && xic != NULL && lnum == wp->w_cursor.lnum && (State & INSERT) && !p_imdisable && im_is_preediting() && draw_state == WL_LINE)
    {
      colnr_T tcol;

      if (preedit_end_col == MAXCOL)
        getvcol(curwin, &(wp->w_cursor), &tcol, NULL, NULL);
      else
        tcol = preedit_end_col;
      if ((long)preedit_start_col <= vcol && vcol < (long)tcol)
      {
        if (feedback_old_attr < 0)
        {
          feedback_col = 0;
          feedback_old_attr = char_attr;
        }
        char_attr = im_get_feedback_attr(feedback_col);
        if (char_attr < 0)
          char_attr = feedback_old_attr;
        feedback_col++;
      }
      else if (feedback_old_attr >= 0)
      {
        char_attr = feedback_old_attr;
        feedback_old_attr = -1;
        feedback_col = 0;
      }
    }
#endif
    /*
	 * Handle the case where we are in column 0 but not on the first
	 * character of the line and the user wants us to show us a
	 * special character (via 'listchars' option "precedes:<char>".
	 */
    if (lcs_prec_todo != NUL && wp->w_p_list && (wp->w_p_wrap ? wp->w_skipcol > 0 : wp->w_leftcol > 0)
#ifdef FEAT_DIFF
        && filler_todo <= 0
#endif
        && draw_state > WL_NR && c != NUL)
    {
      c = lcs_prec;
      lcs_prec_todo = NUL;
      if (has_mbyte && (*mb_char2cells)(mb_c) > 1)
      {
        /* Double-width character being overwritten by the "precedes"
		 * character, need to fill up half the character. */
        c_extra = MB_FILLER_CHAR;
        c_final = NUL;
        n_extra = 1;
        n_attr = 2;
        extra_attr = HL_ATTR(HLF_AT);
      }
      mb_c = c;
      if (enc_utf8 && utf_char2len(c) > 1)
      {
        mb_utf8 = TRUE;
        u8cc[0] = 0;
        c = 0xc0;
      }
      else
        mb_utf8 = FALSE; /* don't draw as UTF-8 */
      if (!attr_pri)
      {
        saved_attr3 = char_attr;     /* save current attr */
        char_attr = HL_ATTR(HLF_AT); /* later copied to char_attr */
        n_attr3 = 1;
      }
    }

    /*
	 * At end of the text line or just after the last character.
	 */
    if (c == NUL
#if defined(LINE_ATTR)
        || did_line_attr == 1
#endif
    )
    {
#ifdef FEAT_SEARCH_EXTRA
      long prevcol = (long)(ptr - line) - (c == NUL);

      /* we're not really at that column when skipping some text */
      if ((long)(wp->w_p_wrap ? wp->w_skipcol : wp->w_leftcol) > prevcol)
        ++prevcol;
#endif

        /* Invert at least one char, used for Visual and empty line or
	     * highlight match at end of line. If it's beyond the last
	     * char on the screen, just overwrite that one (tricky!)  Not
	     * needed when a '$' was displayed for 'list'. */
#ifdef FEAT_SEARCH_EXTRA
      prevcol_hl_flag = FALSE;
      if (!search_hl.is_addpos && prevcol == (long)search_hl.startcol)
        prevcol_hl_flag = TRUE;
      else
      {
        cur = wp->w_match_head;
        while (cur != NULL)
        {
          if (!cur->hl.is_addpos && prevcol == (long)cur->hl.startcol)
          {
            prevcol_hl_flag = TRUE;
            break;
          }
          cur = cur->next;
        }
      }
#endif
      if (lcs_eol == lcs_eol_one && ((area_attr != 0 && vcol == fromcol && (VIsual_mode != Ctrl_V || lnum == VIsual.lnum || lnum == curwin->w_cursor.lnum) && c == NUL)
#ifdef FEAT_SEARCH_EXTRA
                                     /* highlight 'hlsearch' match at end of line */
                                     || (prevcol_hl_flag == TRUE
#ifdef FEAT_DIFF
                                         && diff_hlf == (hlf_T)0
#endif
#if defined(LINE_ATTR)
                                         && did_line_attr <= 1
#endif
                                         )
#endif
                                         ))
      {
        int n = 0;

#ifdef FEAT_RIGHTLEFT
        if (wp->w_p_rl)
        {
          if (col < 0)
            n = 1;
        }
        else
#endif
        {
          if (col >= wp->w_width)
            n = -1;
        }
        if (n != 0)
        {
          /* At the window boundary, highlight the last character
		     * instead (better than nothing). */
          off += n;
          col += n;
        }
        else
        {
          /* Add a blank character to highlight. */
          ScreenLines[off] = ' ';
          if (enc_utf8)
            ScreenLinesUC[off] = 0;
        }
#ifdef FEAT_SEARCH_EXTRA
        if (area_attr == 0)
        {
          /* Use attributes from match with highest priority among
		     * 'search_hl' and the match list. */
          char_attr = search_hl.attr;
          cur = wp->w_match_head;
          shl_flag = FALSE;
          while (cur != NULL || shl_flag == FALSE)
          {
            if (shl_flag == FALSE && ((cur != NULL && cur->priority > SEARCH_HL_PRIORITY) || cur == NULL))
            {
              shl = &search_hl;
              shl_flag = TRUE;
            }
            else
              shl = &cur->hl;
            if ((ptr - line) - 1 == (long)shl->startcol && (shl == &search_hl || !shl->is_addpos))
              char_attr = shl->attr;
            if (shl != &search_hl && cur != NULL)
              cur = cur->next;
          }
        }
#endif
        ScreenAttrs[off] = char_attr;
#ifdef FEAT_RIGHTLEFT
        if (wp->w_p_rl)
        {
          --col;
          --off;
        }
        else
#endif
        {
          ++col;
          ++off;
        }
        ++vcol;
      }
    }

    /*
	 * At end of the text line.
	 */
    if (c == NUL)
    {

      screen_line(screen_row, wp->w_wincol, col,
                  (int)wp->w_width, screen_line_flags);
      row++;

      /*
	     * Update w_cline_height and w_cline_folded if the cursor line was
	     * updated (saves a call to plines() later).
	     */
      if (wp == curwin && lnum == curwin->w_cursor.lnum)
      {
        curwin->w_cline_row = startrow;
        curwin->w_cline_height = row - startrow;
#ifdef FEAT_FOLDING
        curwin->w_cline_folded = FALSE;
#endif
        curwin->w_valid |= (VALID_CHEIGHT | VALID_CROW);
      }

      break;
    }

    // Show "extends" character from 'listchars' if beyond the line end and
    // 'list' is set.
    if (lcs_ext != NUL && wp->w_p_list && !wp->w_p_wrap
#ifdef FEAT_DIFF
        && filler_todo <= 0
#endif
        && (
#ifdef FEAT_RIGHTLEFT
               wp->w_p_rl ? col == 0 :
#endif
                          col == wp->w_width - 1) &&
        (*ptr != NUL || (wp->w_p_list && lcs_eol_one > 0) || (n_extra && (c_extra != NUL || *p_extra != NUL))))
    {
      c = lcs_ext;
      char_attr = HL_ATTR(HLF_AT);
      mb_c = c;
      if (enc_utf8 && utf_char2len(c) > 1)
      {
        mb_utf8 = TRUE;
        u8cc[0] = 0;
        c = 0xc0;
      }
      else
        mb_utf8 = FALSE;
    }

    /*
	 * Store character to be displayed.
	 * Skip characters that are left of the screen for 'nowrap'.
	 */
    vcol_prev = vcol;
    if (draw_state < WL_LINE || n_skip <= 0)
    {
      /*
	     * Store the character.
	     */
#if defined(FEAT_RIGHTLEFT)
      if (has_mbyte && wp->w_p_rl && (*mb_char2cells)(mb_c) > 1)
      {
        /* A double-wide character is: put first halve in left cell. */
        --off;
        --col;
      }
#endif
      ScreenLines[off] = c;
      if (enc_dbcs == DBCS_JPNU)
      {
        if ((mb_c & 0xff00) == 0x8e00)
          ScreenLines[off] = 0x8e;
        ScreenLines2[off] = mb_c & 0xff;
      }
      else if (enc_utf8)
      {
        if (mb_utf8)
        {
          int i;

          ScreenLinesUC[off] = mb_c;
          if ((c & 0xff) == 0)
            ScreenLines[off] = 0x80; /* avoid storing zero */
          for (i = 0; i < Screen_mco; ++i)
          {
            ScreenLinesC[i][off] = u8cc[i];
            if (u8cc[i] == 0)
              break;
          }
        }
        else
          ScreenLinesUC[off] = 0;
      }
      if (multi_attr)
      {
        ScreenAttrs[off] = multi_attr;
        multi_attr = 0;
      }
      else
        ScreenAttrs[off] = char_attr;

      if (has_mbyte && (*mb_char2cells)(mb_c) > 1)
      {
        /* Need to fill two screen columns. */
        ++off;
        ++col;
        if (enc_utf8)
          /* UTF-8: Put a 0 in the second screen char. */
          ScreenLines[off] = 0;
        else
          /* DBCS: Put second byte in the second screen char. */
          ScreenLines[off] = mb_c & 0xff;
        if (draw_state > WL_NR
#ifdef FEAT_DIFF
            && filler_todo <= 0
#endif
        )
          ++vcol;
        /* When "tocol" is halfway a character, set it to the end of
		 * the character, otherwise highlighting won't stop. */
        if (tocol == vcol)
          ++tocol;
#ifdef FEAT_RIGHTLEFT
        if (wp->w_p_rl)
        {
          /* now it's time to backup one cell */
          --off;
          --col;
        }
#endif
      }
#ifdef FEAT_RIGHTLEFT
      if (wp->w_p_rl)
      {
        --off;
        --col;
      }
      else
#endif
      {
        ++off;
        ++col;
      }
    }
    else
      --n_skip;

    /* Only advance the "vcol" when after the 'number' or 'relativenumber'
	 * column. */
    if (draw_state > WL_NR
#ifdef FEAT_DIFF
        && filler_todo <= 0
#endif
    )
      ++vcol;

    /* restore attributes after "predeces" in 'listchars' */
    if (draw_state > WL_NR && n_attr3 > 0 && --n_attr3 == 0)
      char_attr = saved_attr3;

    /* restore attributes after last 'listchars' or 'number' char */
    if (n_attr > 0 && draw_state == WL_LINE && --n_attr == 0)
      char_attr = saved_attr2;

    /*
	 * At end of screen line and there is more to come: Display the line
	 * so far.  If there is no more to display it is caught above.
	 */
    if ((
#ifdef FEAT_RIGHTLEFT
            wp->w_p_rl ? (col < 0) :
#endif
                       (col >= wp->w_width)) &&
        (*ptr != NUL
#ifdef FEAT_DIFF
         || filler_todo > 0
#endif
         || (wp->w_p_list && lcs_eol != NUL && p_extra != at_end_str) || (n_extra != 0 && (c_extra != NUL || *p_extra != NUL))))
    {
      screen_line(screen_row, wp->w_wincol, col,
                  (int)wp->w_width, screen_line_flags);
      ++row;
      ++screen_row;

      /* When not wrapping and finished diff lines, or when displayed
	     * '$' and highlighting until last column, break here. */
      if ((!wp->w_p_wrap
#ifdef FEAT_DIFF
           && filler_todo <= 0
#endif
           ) ||
          lcs_eol_one == -1)
        break;

      /* When the window is too narrow draw all "@" lines. */
      if (draw_state != WL_LINE
#ifdef FEAT_DIFF
          && filler_todo <= 0
#endif
      )
      {
        win_draw_end(wp, '@', ' ', TRUE, row, wp->w_height, HLF_AT);
        draw_vsep_win(wp, row);
        row = endrow;
      }

      /* When line got too long for screen break here. */
      if (row == endrow)
      {
        ++row;
        break;
      }

      if (screen_cur_row == screen_row - 1
#ifdef FEAT_DIFF
          && filler_todo <= 0
#endif
          && wp->w_width == Columns)
      {
        /* Remember that the line wraps, used for modeless copy. */
        LineWraps[screen_row - 1] = TRUE;

        /*
		 * Special trick to make copy/paste of wrapped lines work with
		 * xterm/screen: write an extra character beyond the end of
		 * the line. This will work with all terminal types
		 * (regardless of the xn,am settings).
		 * Only do this on a fast tty.
		 * Only do this if the cursor is on the current line
		 * (something has been written in it).
		 * Don't do this for the GUI.
		 * Don't do this for double-width characters.
		 * Don't do this for a window not at the right screen border.
		 */
        if (p_tf
#ifdef FEAT_GUI
            && !gui.in_use
#endif
            && !(has_mbyte && ((*mb_off2cells)(LineOffset[screen_row],
                                               LineOffset[screen_row] + screen_Columns) == 2 ||
                               (*mb_off2cells)(LineOffset[screen_row - 1] + (int)Columns - 2,
                                               LineOffset[screen_row] + screen_Columns) == 2)))
        {
          /* First make sure we are at the end of the screen line,
		     * then output the same character again to let the
		     * terminal know about the wrap.  If the terminal doesn't
		     * auto-wrap, we overwrite the character. */
          if (screen_cur_col != wp->w_width)
            screen_char(LineOffset[screen_row - 1] + (unsigned)Columns - 1,
                        screen_row - 1, (int)(Columns - 1));

          /* force a redraw of the first char on the next line */
          ScreenAttrs[LineOffset[screen_row]] = (sattr_T)-1;
          screen_start(); /* don't know where cursor is now */
        }
      }

      col = 0;
      off = (unsigned)(current_ScreenLine - ScreenLines);
#ifdef FEAT_RIGHTLEFT
      if (wp->w_p_rl)
      {
        col = wp->w_width - 1; /* col is not used if breaking! */
        off += col;
      }
#endif

      /* reset the drawing state for the start of a wrapped line */
      draw_state = WL_START;
      saved_n_extra = n_extra;
      saved_p_extra = p_extra;
      saved_c_extra = c_extra;
      saved_c_final = c_final;
      saved_char_attr = char_attr;
      n_extra = 0;
      lcs_prec_todo = lcs_prec;
#ifdef FEAT_LINEBREAK
#ifdef FEAT_DIFF
      if (filler_todo <= 0)
#endif
        need_showbreak = TRUE;
#endif
#ifdef FEAT_DIFF
      --filler_todo;
      /* When the filler lines are actually below the last line of the
	     * file, don't draw the line itself, break here. */
      if (filler_todo == 0 && wp->w_botfill)
        break;
#endif
    }

  } /* for every character in the line */

  vim_free(p_extra_free);
  return row;
}

/*
 * Return if the composing characters at "off_from" and "off_to" differ.
 * Only to be used when ScreenLinesUC[off_from] != 0.
 */
static int
comp_char_differs(int off_from, int off_to)
{
  int i;

  for (i = 0; i < Screen_mco; ++i)
  {
    if (ScreenLinesC[i][off_from] != ScreenLinesC[i][off_to])
      return TRUE;
    if (ScreenLinesC[i][off_from] == 0)
      break;
  }
  return FALSE;
}

/*
 * Check whether the given character needs redrawing:
 * - the (first byte of the) character is different
 * - the attributes are different
 * - the character is multi-byte and the next byte is different
 * - the character is two cells wide and the second cell differs.
 */
static int
char_needs_redraw(int off_from, int off_to, int cols)
{
  if (cols > 0 && ((ScreenLines[off_from] != ScreenLines[off_to] || ScreenAttrs[off_from] != ScreenAttrs[off_to]) || (enc_dbcs != 0 && MB_BYTE2LEN(ScreenLines[off_from]) > 1 && (enc_dbcs == DBCS_JPNU && ScreenLines[off_from] == 0x8e ? ScreenLines2[off_from] != ScreenLines2[off_to] : (cols > 1 && ScreenLines[off_from + 1] != ScreenLines[off_to + 1]))) || (enc_utf8 && (ScreenLinesUC[off_from] != ScreenLinesUC[off_to] || (ScreenLinesUC[off_from] != 0 && comp_char_differs(off_from, off_to)) || ((*mb_off2cells)(off_from, off_from + cols) > 1 && ScreenLines[off_from + 1] != ScreenLines[off_to + 1])))))
    return TRUE;
  return FALSE;
}

#if defined(FEAT_TERMINAL) || defined(PROTO)
/*
 * Return the index in ScreenLines[] for the current screen line.
 */
int screen_get_current_line_off()
{
  return (int)(current_ScreenLine - ScreenLines);
}
#endif

/*
 * Move one "cooked" screen line to the screen, but only the characters that
 * have actually changed.  Handle insert/delete character.
 * "coloff" gives the first column on the screen for this line.
 * "endcol" gives the columns where valid characters are.
 * "clear_width" is the width of the window.  It's > 0 if the rest of the line
 * needs to be cleared, negative otherwise.
 * "flags" can have bits:
 * SLF_POPUP	    popup window
 * SLF_RIGHTLEFT    rightleft window:
 *    When TRUE and "clear_width" > 0, clear columns 0 to "endcol"
 *    When FALSE and "clear_width" > 0, clear columns "endcol" to "clear_width"
 */
void screen_line(
    int row,
    int coloff,
    int endcol,
    int clear_width,
    int flags UNUSED)
{
  unsigned off_from;
  unsigned off_to;
  unsigned max_off_from;
  unsigned max_off_to;
  int col = 0;
  int hl;
  int force = FALSE; /* force update rest of the line */
  int redraw_this    /* bool: does character need redraw? */
#ifdef FEAT_GUI
      = TRUE /* For GUI when while-loop empty */
#endif
      ;
  int redraw_next; /* redraw_this for next character */
  int clear_next = FALSE;
  int char_cells; /* 1: normal char */
                  /* 2: occupies two display cells */
#define CHAR_CELLS char_cells

  /* Check for illegal row and col, just in case. */
  if (row >= Rows)
    row = Rows - 1;
  if (endcol > Columns)
    endcol = Columns;

#ifdef FEAT_CLIPBOARD
  clip_may_clear_selection(row, row);
#endif

  off_from = (unsigned)(current_ScreenLine - ScreenLines);
  off_to = LineOffset[row] + coloff;
  max_off_from = off_from + screen_Columns;
  max_off_to = LineOffset[row] + screen_Columns;

#ifdef FEAT_RIGHTLEFT
  if (flags & SLF_RIGHTLEFT)
  {
    /* Clear rest first, because it's left of the text. */
    if (clear_width > 0)
    {
      while (col <= endcol && ScreenLines[off_to] == ' ' && ScreenAttrs[off_to] == 0 && (!enc_utf8 || ScreenLinesUC[off_to] == 0))
      {
        ++off_to;
        ++col;
      }
      if (col <= endcol)
        screen_fill(row, row + 1, col + coloff,
                    endcol + coloff + 1, ' ', ' ', 0);
    }
    col = endcol + 1;
    off_to = LineOffset[row] + col + coloff;
    off_from += col;
    endcol = (clear_width > 0 ? clear_width : -clear_width);
  }
#endif /* FEAT_RIGHTLEFT */

  redraw_next = char_needs_redraw(off_from, off_to, endcol - col);

  while (col < endcol)
  {
    if (has_mbyte && (col + 1 < endcol))
      char_cells = (*mb_off2cells)(off_from, max_off_from);
    else
      char_cells = 1;

    redraw_this = redraw_next;
    redraw_next = force || char_needs_redraw(off_from + CHAR_CELLS,
                                             off_to + CHAR_CELLS, endcol - col - CHAR_CELLS);

#ifdef FEAT_GUI
    /* If the next character was bold, then redraw the current character to
	 * remove any pixels that might have spilt over into us.  This only
	 * happens in the GUI.
	 */
    if (redraw_next && gui.in_use)
    {
      hl = ScreenAttrs[off_to + CHAR_CELLS];
      if (hl > HL_ALL)
        hl = syn_attr2attr(hl);
      if (hl & HL_BOLD)
        redraw_this = TRUE;
    }
#endif

    if (redraw_this)
    {
      /*
	     * Special handling when 'xs' termcap flag set (hpterm):
	     * Attributes for characters are stored at the position where the
	     * cursor is when writing the highlighting code.  The
	     * start-highlighting code must be written with the cursor on the
	     * first highlighted character.  The stop-highlighting code must
	     * be written with the cursor just after the last highlighted
	     * character.
	     * Overwriting a character doesn't remove its highlighting.  Need
	     * to clear the rest of the line, and force redrawing it
	     * completely.
	     */
      if (p_wiv && !force
#ifdef FEAT_GUI
          && !gui.in_use
#endif
          && ScreenAttrs[off_to] != 0 && ScreenAttrs[off_from] != ScreenAttrs[off_to])
      {
        /*
		 * Need to remove highlighting attributes here.
		 */
        windgoto(row, col + coloff);
        out_str(T_CE);      /* clear rest of this screen line */
        screen_start();     /* don't know where cursor is now */
        force = TRUE;       /* force redraw of rest of the line */
        redraw_next = TRUE; /* or else next char would miss out */

        /*
		 * If the previous character was highlighted, need to stop
		 * highlighting at this character.
		 */
        if (col + coloff > 0 && ScreenAttrs[off_to - 1] != 0)
        {
          screen_attr = ScreenAttrs[off_to - 1];
          term_windgoto(row, col + coloff);
          screen_stop_highlight();
        }
        else
          screen_attr = 0; /* highlighting has stopped */
      }
      if (enc_dbcs != 0)
      {
        /* Check if overwriting a double-byte with a single-byte or
		 * the other way around requires another character to be
		 * redrawn.  For UTF-8 this isn't needed, because comparing
		 * ScreenLinesUC[] is sufficient. */
        if (char_cells == 1 && col + 1 < endcol && (*mb_off2cells)(off_to, max_off_to) > 1)
        {
          /* Writing a single-cell character over a double-cell
		     * character: need to redraw the next cell. */
          ScreenLines[off_to + 1] = 0;
          redraw_next = TRUE;
        }
        else if (char_cells == 2 && col + 2 < endcol && (*mb_off2cells)(off_to, max_off_to) == 1 && (*mb_off2cells)(off_to + 1, max_off_to) > 1)
        {
          /* Writing the second half of a double-cell character over
		     * a double-cell character: need to redraw the second
		     * cell. */
          ScreenLines[off_to + 2] = 0;
          redraw_next = TRUE;
        }

        if (enc_dbcs == DBCS_JPNU)
          ScreenLines2[off_to] = ScreenLines2[off_from];
      }
      /* When writing a single-width character over a double-width
	     * character and at the end of the redrawn text, need to clear out
	     * the right halve of the old character.
	     * Also required when writing the right halve of a double-width
	     * char over the left halve of an existing one. */
      if (has_mbyte && col + char_cells == endcol && ((char_cells == 1 && (*mb_off2cells)(off_to, max_off_to) > 1) || (char_cells == 2 && (*mb_off2cells)(off_to, max_off_to) == 1 && (*mb_off2cells)(off_to + 1, max_off_to) > 1)))
        clear_next = TRUE;

      ScreenLines[off_to] = ScreenLines[off_from];
      if (enc_utf8)
      {
        ScreenLinesUC[off_to] = ScreenLinesUC[off_from];
        if (ScreenLinesUC[off_from] != 0)
        {
          int i;

          for (i = 0; i < Screen_mco; ++i)
            ScreenLinesC[i][off_to] = ScreenLinesC[i][off_from];
        }
      }
      if (char_cells == 2)
        ScreenLines[off_to + 1] = ScreenLines[off_from + 1];

#if defined(FEAT_GUI) || defined(UNIX)
      /* The bold trick makes a single column of pixels appear in the
	     * next character.  When a bold character is removed, the next
	     * character should be redrawn too.  This happens for our own GUI
	     * and for some xterms. */
      if (
#ifdef FEAT_GUI
          gui.in_use
#endif
#if defined(FEAT_GUI) && defined(UNIX)
          ||
#endif
#ifdef UNIX
          term_is_xterm
#endif
      )
      {
        hl = ScreenAttrs[off_to];
        if (hl > HL_ALL)
          hl = syn_attr2attr(hl);
        if (hl & HL_BOLD)
          redraw_next = TRUE;
      }
#endif
      ScreenAttrs[off_to] = ScreenAttrs[off_from];

      /* For simplicity set the attributes of second half of a
	     * double-wide character equal to the first half. */
      if (char_cells == 2)
        ScreenAttrs[off_to + 1] = ScreenAttrs[off_from];

      if (enc_dbcs != 0 && char_cells == 2)
        screen_char_2(off_to, row, col + coloff);
      else
        screen_char(off_to, row, col + coloff);
    }
    else if (p_wiv
#ifdef FEAT_GUI
             && !gui.in_use
#endif
             && col + coloff > 0)
    {
      if (ScreenAttrs[off_to] == ScreenAttrs[off_to - 1])
      {
        /*
		 * Don't output stop-highlight when moving the cursor, it will
		 * stop the highlighting when it should continue.
		 */
        screen_attr = 0;
      }
      else if (screen_attr != 0)
        screen_stop_highlight();
    }

    off_to += CHAR_CELLS;
    off_from += CHAR_CELLS;
    col += CHAR_CELLS;
  }

  if (clear_next)
  {
    /* Clear the second half of a double-wide character of which the left
	 * half was overwritten with a single-wide character. */
    ScreenLines[off_to] = ' ';
    if (enc_utf8)
      ScreenLinesUC[off_to] = 0;
    screen_char(off_to, row, col + coloff);
  }

  if (clear_width > 0
#ifdef FEAT_RIGHTLEFT
      && !(flags & SLF_RIGHTLEFT)
#endif
  )
  {
#ifdef FEAT_GUI
    int startCol = col;
#endif

    /* blank out the rest of the line */
    while (col < clear_width && ScreenLines[off_to] == ' ' && ScreenAttrs[off_to] == 0 && (!enc_utf8 || ScreenLinesUC[off_to] == 0))
    {
      ++off_to;
      ++col;
    }
    if (col < clear_width)
    {
#ifdef FEAT_GUI
      /*
	     * In the GUI, clearing the rest of the line may leave pixels
	     * behind if the first character cleared was bold.  Some bold
	     * fonts spill over the left.  In this case we redraw the previous
	     * character too.  If we didn't skip any blanks above, then we
	     * only redraw if the character wasn't already redrawn anyway.
	     */
      if (gui.in_use && (col > startCol || !redraw_this))
      {
        hl = ScreenAttrs[off_to];
        if (hl > HL_ALL || (hl & HL_BOLD))
        {
          int prev_cells = 1;

          if (enc_utf8)
            /* for utf-8, ScreenLines[char_offset + 1] == 0 means
			 * that its width is 2. */
            prev_cells = ScreenLines[off_to - 1] == 0 ? 2 : 1;
          else if (enc_dbcs != 0)
          {
            /* find previous character by counting from first
			 * column and get its width. */
            unsigned off = LineOffset[row];
            unsigned max_off = LineOffset[row] + screen_Columns;

            while (off < off_to)
            {
              prev_cells = (*mb_off2cells)(off, max_off);
              off += prev_cells;
            }
          }

          if (enc_dbcs != 0 && prev_cells > 1)
            screen_char_2(off_to - prev_cells, row,
                          col + coloff - prev_cells);
          else
            screen_char(off_to - prev_cells, row,
                        col + coloff - prev_cells);
        }
      }
#endif
      screen_fill(row, row + 1, col + coloff, clear_width + coloff,
                  ' ', ' ', 0);
      off_to += clear_width - col;
      col = clear_width;
    }
  }

  if (clear_width > 0)
  {
    // For a window that has a right neighbor, draw the separator char
    // right of the window contents.
    if (coloff + col < Columns)
    {
      int c;

      c = fillchar_vsep(&hl);
      if (ScreenLines[off_to] != (schar_T)c || (enc_utf8 && (int)ScreenLinesUC[off_to] != (c >= 0x80 ? c : 0)) || ScreenAttrs[off_to] != hl)
      {
        ScreenLines[off_to] = c;
        ScreenAttrs[off_to] = hl;
        if (enc_utf8)
        {
          if (c >= 0x80)
          {
            ScreenLinesUC[off_to] = c;
            ScreenLinesC[0][off_to] = 0;
          }
          else
            ScreenLinesUC[off_to] = 0;
        }
        screen_char(off_to, row, col + coloff);
      }
    }
    else
      LineWraps[row] = FALSE;
  }
}

#if defined(FEAT_RIGHTLEFT) || defined(PROTO)
/*
 * Mirror text "str" for right-left displaying.
 * Only works for single-byte characters (e.g., numbers).
 */
void rl_mirror(char_u *str)
{
  char_u *p1, *p2;
  int t;

  for (p1 = str, p2 = str + STRLEN(str) - 1; p1 < p2; ++p1, --p2)
  {
    t = *p1;
    *p1 = *p2;
    *p2 = t;
  }
}
#endif

/*
 * mark all status lines for redraw; used after first :cd
 */
void status_redraw_all(void)
{
  win_T *wp;

  FOR_ALL_WINDOWS(wp)
  if (wp->w_status_height)
  {
    wp->w_redr_status = TRUE;
    redraw_later(VALID);
  }
}

/*
 * mark all status lines of the current buffer for redraw
 */
void status_redraw_curbuf(void)
{
  win_T *wp;

  FOR_ALL_WINDOWS(wp)
  if (wp->w_status_height != 0 && wp->w_buffer == curbuf)
  {
    wp->w_redr_status = TRUE;
    redraw_later(VALID);
  }
}

/*
 * Redraw all status lines that need to be redrawn.
 */
void redraw_statuslines(void)
{
  win_T *wp;

  FOR_ALL_WINDOWS(wp)
  if (wp->w_redr_status)
    win_redr_status(wp, FALSE);
  if (redraw_tabline)
    draw_tabline();
}

#if defined(FEAT_WILDMENU) || defined(PROTO)
/*
 * Redraw all status lines at the bottom of frame "frp".
 */
void win_redraw_last_status(frame_T *frp)
{
  if (frp->fr_layout == FR_LEAF)
    frp->fr_win->w_redr_status = TRUE;
  else if (frp->fr_layout == FR_ROW)
  {
    FOR_ALL_FRAMES(frp, frp->fr_child)
    win_redraw_last_status(frp);
  }
  else /* frp->fr_layout == FR_COL */
  {
    frp = frp->fr_child;
    while (frp->fr_next != NULL)
      frp = frp->fr_next;
    win_redraw_last_status(frp);
  }
}
#endif

/*
 * Draw the verticap separator right of window "wp" starting with line "row".
 */
static void
draw_vsep_win(win_T *wp, int row)
{
  int hl;
  int c;

  if (wp->w_vsep_width)
  {
    /* draw the vertical separator right of this window */
    c = fillchar_vsep(&hl);
    screen_fill(W_WINROW(wp) + row, W_WINROW(wp) + wp->w_height,
                W_ENDCOL(wp), W_ENDCOL(wp) + 1,
                c, ' ', hl);
  }
}

#ifdef FEAT_WILDMENU
static int skip_status_match_char(expand_T *xp, char_u *s);

/*
 * Get the length of an item as it will be shown in the status line.
 */
static int
status_match_len(expand_T *xp, char_u *s)
{
  int len = 0;

  while (*s != NUL)
  {
    s += skip_status_match_char(xp, s);
    len += ptr2cells(s);
    MB_PTR_ADV(s);
  }

  return len;
}

/*
 * Return the number of characters that should be skipped in a status match.
 * These are backslashes used for escaping.  Do show backslashes in help tags.
 */
static int
skip_status_match_char(expand_T *xp, char_u *s)
{
  if ((rem_backslash(s) && xp->xp_context != EXPAND_HELP))
  {
#ifndef BACKSLASH_IN_FILENAME
    if (xp->xp_shell && csh_like_shell() && s[1] == '\\' && s[2] == '!')
      return 2;
#endif
    return 1;
  }
  return 0;
}

/*
 * Show wildchar matches in the status line.
 * Show at least the "match" item.
 * We start at item 'first_match' in the list and show all matches that fit.
 *
 * If inversion is possible we use it. Else '=' characters are used.
 */
void win_redr_status_matches(
    expand_T *xp,
    int num_matches,
    char_u **matches, /* list of matches */
    int match,
    int showtail)
{
#define L_MATCH(m) (showtail ? sm_gettail(matches[m]) : matches[m])
  int row;
  char_u *buf;
  int len;
  int clen; /* length in screen cells */
  int fillchar;
  int attr;
  int i;
  int highlight = TRUE;
  char_u *selstart = NULL;
  int selstart_col = 0;
  char_u *selend = NULL;
  static int first_match = 0;
  int add_left = FALSE;
  char_u *s;
  int l;

  if (matches == NULL) /* interrupted completion? */
    return;

  if (has_mbyte)
    buf = alloc(Columns * MB_MAXBYTES + 1);
  else
    buf = alloc(Columns + 1);
  if (buf == NULL)
    return;

  if (match == -1) /* don't show match but original text */
  {
    match = 0;
    highlight = FALSE;
  }
  /* count 1 for the ending ">" */
  clen = status_match_len(xp, L_MATCH(match)) + 3;
  if (match == 0)
    first_match = 0;
  else if (match < first_match)
  {
    /* jumping left, as far as we can go */
    first_match = match;
    add_left = TRUE;
  }
  else
  {
    /* check if match fits on the screen */
    for (i = first_match; i < match; ++i)
      clen += status_match_len(xp, L_MATCH(i)) + 2;
    if (first_match > 0)
      clen += 2;
    /* jumping right, put match at the left */
    if ((long)clen > Columns)
    {
      first_match = match;
      /* if showing the last match, we can add some on the left */
      clen = 2;
      for (i = match; i < num_matches; ++i)
      {
        clen += status_match_len(xp, L_MATCH(i)) + 2;
        if ((long)clen >= Columns)
          break;
      }
      if (i == num_matches)
        add_left = TRUE;
    }
  }
  if (add_left)
    while (first_match > 0)
    {
      clen += status_match_len(xp, L_MATCH(first_match - 1)) + 2;
      if ((long)clen >= Columns)
        break;
      --first_match;
    }

  fillchar = fillchar_status(&attr, curwin);

  if (first_match == 0)
  {
    *buf = NUL;
    len = 0;
  }
  else
  {
    STRCPY(buf, "< ");
    len = 2;
  }
  clen = len;

  i = first_match;
  while ((long)(clen + status_match_len(xp, L_MATCH(i)) + 2) < Columns)
  {
    if (i == match)
    {
      selstart = buf + len;
      selstart_col = clen;
    }

    s = L_MATCH(i);
    /* Check for menu separators - replace with '|' */
    for (; *s != NUL; ++s)
    {
      s += skip_status_match_char(xp, s);
      clen += ptr2cells(s);
      if (has_mbyte && (l = (*mb_ptr2len)(s)) > 1)
      {
        STRNCPY(buf + len, s, l);
        s += l - 1;
        len += l;
      }
      else
      {
        STRCPY(buf + len, transchar_byte(*s));
        len += (int)STRLEN(buf + len);
      }
    }
    if (i == match)
      selend = buf + len;

    *(buf + len++) = ' ';
    *(buf + len++) = ' ';
    clen += 2;
    if (++i == num_matches)
      break;
  }

  if (i != num_matches)
  {
    *(buf + len++) = '>';
    ++clen;
  }

  buf[len] = NUL;

  row = cmdline_row - 1;
  if (row >= 0)
  {
    if (wild_menu_showing == 0)
    {
      if (msg_scrolled > 0)
      {
        /* Put the wildmenu just above the command line.  If there is
		 * no room, scroll the screen one line up. */
        if (cmdline_row == Rows - 1)
        {
          screen_del_lines(0, 0, 1, (int)Rows, TRUE, 0, NULL);
          ++msg_scrolled;
        }
        else
        {
          ++cmdline_row;
          ++row;
        }
        wild_menu_showing = WM_SCROLLED;
      }
      else
      {
        /* Create status line if needed by setting 'laststatus' to 2.
		 * Set 'winminheight' to zero to avoid that the window is
		 * resized. */
        if (lastwin->w_status_height == 0)
        {
          save_p_ls = p_ls;
          save_p_wmh = p_wmh;
          p_ls = 2;
          p_wmh = 0;
          last_status(FALSE);
        }
        wild_menu_showing = WM_SHOWN;
      }
    }

    screen_puts(buf, row, 0, attr);
    if (selstart != NULL && highlight)
    {
      *selend = NUL;
      screen_puts(selstart, row, selstart_col, HL_ATTR(HLF_WM));
    }

    screen_fill(row, row + 1, clen, (int)Columns, fillchar, fillchar, attr);
  }

  win_redraw_last_status(topframe);
  vim_free(buf);
}
#endif

/*
 * Redraw the status line of window wp.
 *
 * If inversion is possible we use it. Else '=' characters are used.
 * If "ignore_pum" is TRUE, also redraw statusline when the popup menu is
 * displayed.
 */
static void
win_redr_status(win_T *wp, int ignore_pum UNUSED)
{
  int row;
  char_u *p;
  int len;
  int fillchar;
  int attr;
  int this_ru_col;
  static int busy = FALSE;

  /* It's possible to get here recursively when 'statusline' (indirectly)
     * invokes ":redrawstatus".  Simply ignore the call then. */
  if (busy)
    return;
  busy = TRUE;

  wp->w_redr_status = FALSE;
  if (wp->w_status_height == 0)
  {
    /* no status line, can only be last window */
    redraw_cmdline = TRUE;
  }
  else if (!redrawing())
  {
    /* Don't redraw right now, do it later. */
    wp->w_redr_status = TRUE;
  }
  else
  {
    fillchar = fillchar_status(&attr, wp);

    get_trans_bufname(wp->w_buffer);
    p = NameBuff;
    len = (int)STRLEN(p);

    if (bt_help(wp->w_buffer)
#ifdef FEAT_QUICKFIX
        || wp->w_p_pvw
#endif
        || bufIsChanged(wp->w_buffer) || wp->w_buffer->b_p_ro)
      *(p + len++) = ' ';
    if (bt_help(wp->w_buffer))
    {
      STRCPY(p + len, _("[Help]"));
      len += (int)STRLEN(p + len);
    }
#ifdef FEAT_QUICKFIX
    if (wp->w_p_pvw)
    {
      STRCPY(p + len, _("[Preview]"));
      len += (int)STRLEN(p + len);
    }
#endif
    if (bufIsChanged(wp->w_buffer)
#ifdef FEAT_TERMINAL
        && !bt_terminal(wp->w_buffer)
#endif
    )
    {
      STRCPY(p + len, "[+]");
      len += 3;
    }
    if (wp->w_buffer->b_p_ro)
    {
      STRCPY(p + len, _("[RO]"));
      len += (int)STRLEN(p + len);
    }

    this_ru_col = ru_col - (Columns - wp->w_width);
    if (this_ru_col < (wp->w_width + 1) / 2)
      this_ru_col = (wp->w_width + 1) / 2;
    if (this_ru_col <= 1)
    {
      p = (char_u *)"<"; /* No room for file name! */
      len = 1;
    }
    else if (has_mbyte)
    {
      int clen = 0, i;

      /* Count total number of display cells. */
      clen = mb_string2cells(p, -1);

      /* Find first character that will fit.
	     * Going from start to end is much faster for DBCS. */
      for (i = 0; p[i] != NUL && clen >= this_ru_col - 1;
           i += (*mb_ptr2len)(p + i))
        clen -= (*mb_ptr2cells)(p + i);
      len = clen;
      if (i > 0)
      {
        p = p + i - 1;
        *p = '<';
        ++len;
      }
    }
    else if (len > this_ru_col - 1)
    {
      p += len - (this_ru_col - 1);
      *p = '<';
      len = this_ru_col - 1;
    }

    row = W_WINROW(wp) + wp->w_height;
    screen_puts(p, row, wp->w_wincol, attr);
    screen_fill(row, row + 1, len + wp->w_wincol,
                this_ru_col + wp->w_wincol, fillchar, fillchar, attr);

    if (get_keymap_str(wp, (char_u *)"<%s>", NameBuff, MAXPATHL) && (int)(this_ru_col - len) > (int)(STRLEN(NameBuff) + 1))
      screen_puts(NameBuff, row, (int)(this_ru_col - STRLEN(NameBuff) - 1 + wp->w_wincol), attr);
  }

  /*
     * May need to draw the character below the vertical separator.
     */
  if (wp->w_vsep_width != 0 && wp->w_status_height != 0 && redrawing())
  {
    if (stl_connected(wp))
      fillchar = fillchar_status(&attr, wp);
    else
      fillchar = fillchar_vsep(&attr);
    screen_putchar(fillchar, W_WINROW(wp) + wp->w_height, W_ENDCOL(wp),
                   attr);
  }
  busy = FALSE;
}

/*
 * Return TRUE if the status line of window "wp" is connected to the status
 * line of the window right of it.  If not, then it's a vertical separator.
 * Only call if (wp->w_vsep_width != 0).
 */
int stl_connected(win_T *wp)
{
  frame_T *fr;

  fr = wp->w_frame;
  while (fr->fr_parent != NULL)
  {
    if (fr->fr_parent->fr_layout == FR_COL)
    {
      if (fr->fr_next != NULL)
        break;
    }
    else
    {
      if (fr->fr_next != NULL)
        return TRUE;
    }
    fr = fr->fr_parent;
  }
  return FALSE;
}

/*
 * Get the value to show for the language mappings, active 'keymap'.
 */
int get_keymap_str(
    win_T *wp,
    char_u *fmt, /* format string containing one %s item */
    char_u *buf, /* buffer for the result */
    int len)     /* length of buffer */
{
  char_u *p;

  if (wp->w_buffer->b_p_iminsert != B_IMODE_LMAP)
    return FALSE;

  {
#ifdef FEAT_EVAL
    buf_T *old_curbuf = curbuf;
    win_T *old_curwin = curwin;
    char_u *s;

    curbuf = wp->w_buffer;
    curwin = wp;
    STRCPY(buf, "b:keymap_name"); /* must be writable */
    ++emsg_skip;
    s = p = eval_to_string(buf, NULL, FALSE);
    --emsg_skip;
    curbuf = old_curbuf;
    curwin = old_curwin;
    if (p == NULL || *p == NUL)
#endif
    {
#ifdef FEAT_KEYMAP
      if (wp->w_buffer->b_kmap_state & KEYMAP_LOADED)
        p = wp->w_buffer->b_p_keymap;
      else
#endif
        p = (char_u *)"lang";
    }
    if (vim_snprintf((char *)buf, len, (char *)fmt, p) > len - 1)
      buf[0] = NUL;
#ifdef FEAT_EVAL
    vim_free(s);
#endif
  }
  return buf[0] != NUL;
}

/*
 * Output a single character directly to the screen and update ScreenLines.
 */
void screen_putchar(int c, int row, int col, int attr)
{
  char_u buf[MB_MAXBYTES + 1];

  if (has_mbyte)
    buf[(*mb_char2bytes)(c, buf)] = NUL;
  else
  {
    buf[0] = c;
    buf[1] = NUL;
  }
  screen_puts(buf, row, col, attr);
}

/*
 * Get a single character directly from ScreenLines into "bytes[]".
 * Also return its attribute in *attrp;
 */
void screen_getbytes(int row, int col, char_u *bytes, int *attrp)
{
  unsigned off;

  /* safety check */
  if (ScreenLines != NULL && row < screen_Rows && col < screen_Columns)
  {
    off = LineOffset[row] + col;
    *attrp = ScreenAttrs[off];
    bytes[0] = ScreenLines[off];
    bytes[1] = NUL;

    if (enc_utf8 && ScreenLinesUC[off] != 0)
      bytes[utfc_char2bytes(off, bytes)] = NUL;
    else if (enc_dbcs == DBCS_JPNU && ScreenLines[off] == 0x8e)
    {
      bytes[0] = ScreenLines[off];
      bytes[1] = ScreenLines2[off];
      bytes[2] = NUL;
    }
    else if (enc_dbcs && MB_BYTE2LEN(bytes[0]) > 1)
    {
      bytes[1] = ScreenLines[off + 1];
      bytes[2] = NUL;
    }
  }
}

/*
 * Return TRUE if composing characters for screen posn "off" differs from
 * composing characters in "u8cc".
 * Only to be used when ScreenLinesUC[off] != 0.
 */
static int
screen_comp_differs(int off, int *u8cc)
{
  int i;

  for (i = 0; i < Screen_mco; ++i)
  {
    if (ScreenLinesC[i][off] != (u8char_T)u8cc[i])
      return TRUE;
    if (u8cc[i] == 0)
      break;
  }
  return FALSE;
}

/*
 * Put string '*text' on the screen at position 'row' and 'col', with
 * attributes 'attr', and update ScreenLines[] and ScreenAttrs[].
 * Note: only outputs within one row, message is truncated at screen boundary!
 * Note: if ScreenLines[], row and/or col is invalid, nothing is done.
 */
void screen_puts(
    char_u *text,
    int row,
    int col,
    int attr)
{
  screen_puts_len(text, -1, row, col, attr);
}

/*
 * Like screen_puts(), but output "text[len]".  When "len" is -1 output up to
 * a NUL.
 */
void screen_puts_len(
    char_u *text,
    int textlen,
    int row,
    int col,
    int attr)
{
  unsigned off;
  char_u *ptr = text;
  int len = textlen;
  int c;
  unsigned max_off;
  int mbyte_blen = 1;
  int mbyte_cells = 1;
  int u8c = 0;
  int u8cc[MAX_MCO];
  int clear_next_cell = FALSE;
#ifdef FEAT_ARABIC
  int prev_c = 0; /* previous Arabic character */
  int pc, nc, nc1;
  int pcc[MAX_MCO];
#endif
  int force_redraw_this;
  int force_redraw_next = FALSE;
  int need_redraw;

  if (ScreenLines == NULL || row >= screen_Rows) /* safety check */
    return;
  off = LineOffset[row] + col;

  /* When drawing over the right halve of a double-wide char clear out the
     * left halve.  Only needed in a terminal. */
  if (has_mbyte && col > 0 && col < screen_Columns
#ifdef FEAT_GUI
      && !gui.in_use
#endif
      && mb_fix_col(col, row) != col)
  {
    ScreenLines[off - 1] = ' ';
    ScreenAttrs[off - 1] = 0;
    if (enc_utf8)
    {
      ScreenLinesUC[off - 1] = 0;
      ScreenLinesC[0][off - 1] = 0;
    }
    /* redraw the previous cell, make it empty */
    screen_char(off - 1, row, col - 1);
    /* force the cell at "col" to be redrawn */
    force_redraw_next = TRUE;
  }

  max_off = LineOffset[row] + screen_Columns;
  while (col < screen_Columns && (len < 0 || (int)(ptr - text) < len) && *ptr != NUL)
  {
    c = *ptr;
    /* check if this is the first byte of a multibyte */
    if (has_mbyte)
    {
      if (enc_utf8 && len > 0)
        mbyte_blen = utfc_ptr2len_len(ptr, (int)((text + len) - ptr));
      else
        mbyte_blen = (*mb_ptr2len)(ptr);
      if (enc_dbcs == DBCS_JPNU && c == 0x8e)
        mbyte_cells = 1;
      else if (enc_dbcs != 0)
        mbyte_cells = mbyte_blen;
      else /* enc_utf8 */
      {
        if (len >= 0)
          u8c = utfc_ptr2char_len(ptr, u8cc,
                                  (int)((text + len) - ptr));
        else
          u8c = utfc_ptr2char(ptr, u8cc);
        mbyte_cells = utf_char2cells(u8c);
#ifdef FEAT_ARABIC
        if (p_arshape && !p_tbidi && ARABIC_CHAR(u8c))
        {
          /* Do Arabic shaping. */
          if (len >= 0 && (int)(ptr - text) + mbyte_blen >= len)
          {
            /* Past end of string to be displayed. */
            nc = NUL;
            nc1 = NUL;
          }
          else
          {
            nc = utfc_ptr2char_len(ptr + mbyte_blen, pcc,
                                   (int)((text + len) - ptr - mbyte_blen));
            nc1 = pcc[0];
          }
          pc = prev_c;
          prev_c = u8c;
          u8c = arabic_shape(u8c, &c, &u8cc[0], nc, nc1, pc);
        }
        else
          prev_c = u8c;
#endif
        if (col + mbyte_cells > screen_Columns)
        {
          /* Only 1 cell left, but character requires 2 cells:
		     * display a '>' in the last column to avoid wrapping. */
          c = '>';
          mbyte_cells = 1;
        }
      }
    }

    force_redraw_this = force_redraw_next;
    force_redraw_next = FALSE;

    need_redraw = ScreenLines[off] != c || (mbyte_cells == 2 && ScreenLines[off + 1] != (enc_dbcs ? ptr[1] : 0)) || (enc_dbcs == DBCS_JPNU && c == 0x8e && ScreenLines2[off] != ptr[1]) || (enc_utf8 && (ScreenLinesUC[off] != (u8char_T)(c < 0x80 && u8cc[0] == 0 ? 0 : u8c) || (ScreenLinesUC[off] != 0 && screen_comp_differs(off, u8cc)))) || ScreenAttrs[off] != attr || exmode_active;

    if (need_redraw || force_redraw_this)
    {
#if defined(FEAT_GUI) || defined(UNIX)
      /* The bold trick makes a single row of pixels appear in the next
	     * character.  When a bold character is removed, the next
	     * character should be redrawn too.  This happens for our own GUI
	     * and for some xterms. */
      if (need_redraw && ScreenLines[off] != ' ' && (
#ifdef FEAT_GUI
                                                        gui.in_use
#endif
#if defined(FEAT_GUI) && defined(UNIX)
                                                        ||
#endif
#ifdef UNIX
                                                        term_is_xterm
#endif
                                                        ))
      {
        int n = ScreenAttrs[off];

        if (n > HL_ALL)
          n = syn_attr2attr(n);
        if (n & HL_BOLD)
          force_redraw_next = TRUE;
      }
#endif
      /* When at the end of the text and overwriting a two-cell
	     * character with a one-cell character, need to clear the next
	     * cell.  Also when overwriting the left halve of a two-cell char
	     * with the right halve of a two-cell char.  Do this only once
	     * (mb_off2cells() may return 2 on the right halve). */
      if (clear_next_cell)
        clear_next_cell = FALSE;
      else if (has_mbyte && (len < 0 ? ptr[mbyte_blen] == NUL : ptr + mbyte_blen >= text + len) && ((mbyte_cells == 1 && (*mb_off2cells)(off, max_off) > 1) || (mbyte_cells == 2 && (*mb_off2cells)(off, max_off) == 1 && (*mb_off2cells)(off + 1, max_off) > 1)))
        clear_next_cell = TRUE;

      /* Make sure we never leave a second byte of a double-byte behind,
	     * it confuses mb_off2cells(). */
      if (enc_dbcs && ((mbyte_cells == 1 && (*mb_off2cells)(off, max_off) > 1) || (mbyte_cells == 2 && (*mb_off2cells)(off, max_off) == 1 && (*mb_off2cells)(off + 1, max_off) > 1)))
        ScreenLines[off + mbyte_blen] = 0;
      ScreenLines[off] = c;
      ScreenAttrs[off] = attr;
      if (enc_utf8)
      {
        if (c < 0x80 && u8cc[0] == 0)
          ScreenLinesUC[off] = 0;
        else
        {
          int i;

          ScreenLinesUC[off] = u8c;
          for (i = 0; i < Screen_mco; ++i)
          {
            ScreenLinesC[i][off] = u8cc[i];
            if (u8cc[i] == 0)
              break;
          }
        }
        if (mbyte_cells == 2)
        {
          ScreenLines[off + 1] = 0;
          ScreenAttrs[off + 1] = attr;
        }
        screen_char(off, row, col);
      }
      else if (mbyte_cells == 2)
      {
        ScreenLines[off + 1] = ptr[1];
        ScreenAttrs[off + 1] = attr;
        screen_char_2(off, row, col);
      }
      else if (enc_dbcs == DBCS_JPNU && c == 0x8e)
      {
        ScreenLines2[off] = ptr[1];
        screen_char(off, row, col);
      }
      else
        screen_char(off, row, col);
    }
    if (has_mbyte)
    {
      off += mbyte_cells;
      col += mbyte_cells;
      ptr += mbyte_blen;
      if (clear_next_cell)
      {
        /* This only happens at the end, display one space next. */
        ptr = (char_u *)" ";
        len = -1;
      }
    }
    else
    {
      ++off;
      ++col;
      ++ptr;
    }
  }

  /* If we detected the next character needs to be redrawn, but the text
     * doesn't extend up to there, update the character here. */
  if (force_redraw_next && col < screen_Columns)
  {
    if (enc_dbcs != 0 && dbcs_off2cells(off, max_off) > 1)
      screen_char_2(off, row, col);
    else
      screen_char(off, row, col);
  }
}

#ifdef FEAT_SEARCH_EXTRA
/*
 * Prepare for 'hlsearch' highlighting.
 */
static void
start_search_hl(void)
{
  if (p_hls && !no_hlsearch)
  {
    last_pat_prog(&search_hl.rm);
    search_hl.attr = HL_ATTR(HLF_L);
#ifdef FEAT_RELTIME
    /* Set the time limit to 'redrawtime'. */
    profile_setlimit(p_rdt, &search_hl.tm);
#endif
  }
}

/*
 * Clean up for 'hlsearch' highlighting.
 */
static void
end_search_hl(void)
{
  if (search_hl.rm.regprog != NULL)
  {
    vim_regfree(search_hl.rm.regprog);
    search_hl.rm.regprog = NULL;
  }
}

/*
 * Init for calling prepare_search_hl().
 */
static void
init_search_hl(win_T *wp)
{
  matchitem_T *cur;

  /* Setup for match and 'hlsearch' highlighting.  Disable any previous
     * match */
  cur = wp->w_match_head;
  while (cur != NULL)
  {
    cur->hl.rm = cur->match;
    if (cur->hlg_id == 0)
      cur->hl.attr = 0;
    else
      cur->hl.attr = syn_id2attr(cur->hlg_id);
    cur->hl.buf = wp->w_buffer;
    cur->hl.lnum = 0;
    cur->hl.first_lnum = 0;
#ifdef FEAT_RELTIME
    /* Set the time limit to 'redrawtime'. */
    profile_setlimit(p_rdt, &(cur->hl.tm));
#endif
    cur = cur->next;
  }
  search_hl.buf = wp->w_buffer;
  search_hl.lnum = 0;
  search_hl.first_lnum = 0;
  /* time limit is set at the toplevel, for all windows */
}

/*
 * Advance to the match in window "wp" line "lnum" or past it.
 */
static void
prepare_search_hl(win_T *wp, linenr_T lnum)
{
  matchitem_T *cur;   /* points to the match list */
  match_T *shl;       /* points to search_hl or a match */
  int shl_flag;       /* flag to indicate whether search_hl
				   has been processed or not */
  int pos_inprogress; /* marks that position match search is
				   in progress */
  int n;

  /*
     * When using a multi-line pattern, start searching at the top
     * of the window or just after a closed fold.
     * Do this both for search_hl and the match list.
     */
  cur = wp->w_match_head;
  shl_flag = FALSE;
  while (cur != NULL || shl_flag == FALSE)
  {
    if (shl_flag == FALSE)
    {
      shl = &search_hl;
      shl_flag = TRUE;
    }
    else
      shl = &cur->hl;
    if (shl->rm.regprog != NULL && shl->lnum == 0 && re_multiline(shl->rm.regprog))
    {
      if (shl->first_lnum == 0)
      {
#ifdef FEAT_FOLDING
        for (shl->first_lnum = lnum;
             shl->first_lnum > wp->w_topline; --shl->first_lnum)
          if (hasFoldingWin(wp, shl->first_lnum - 1,
                            NULL, NULL, TRUE, NULL))
            break;
#else
        shl->first_lnum = wp->w_topline;
#endif
      }
      if (cur != NULL)
        cur->pos.cur = 0;
      pos_inprogress = TRUE;
      n = 0;
      while (shl->first_lnum < lnum && (shl->rm.regprog != NULL || (cur != NULL && pos_inprogress)))
      {
        next_search_hl(wp, shl, shl->first_lnum, (colnr_T)n,
                       shl == &search_hl ? NULL : cur);
        pos_inprogress = cur == NULL || cur->pos.cur == 0
                             ? FALSE
                             : TRUE;
        if (shl->lnum != 0)
        {
          shl->first_lnum = shl->lnum + shl->rm.endpos[0].lnum - shl->rm.startpos[0].lnum;
          n = shl->rm.endpos[0].col;
        }
        else
        {
          ++shl->first_lnum;
          n = 0;
        }
      }
    }
    if (shl != &search_hl && cur != NULL)
      cur = cur->next;
  }
}

/*
 * Search for a next 'hlsearch' or match.
 * Uses shl->buf.
 * Sets shl->lnum and shl->rm contents.
 * Note: Assumes a previous match is always before "lnum", unless
 * shl->lnum is zero.
 * Careful: Any pointers for buffer lines will become invalid.
 */
static void
next_search_hl(
    win_T *win,
    match_T *shl, /* points to search_hl or a match */
    linenr_T lnum,
    colnr_T mincol,   /* minimal column for a match */
    matchitem_T *cur) /* to retrieve match positions if any */
{
  linenr_T l;
  colnr_T matchcol;
  long nmatched;
  int save_called_emsg = called_emsg;

  // for :{range}s/pat only highlight inside the range
  if (lnum < search_first_line || lnum > search_last_line)
  {
    shl->lnum = 0;
    return;
  }

  if (shl->lnum != 0)
  {
    /* Check for three situations:
	 * 1. If the "lnum" is below a previous match, start a new search.
	 * 2. If the previous match includes "mincol", use it.
	 * 3. Continue after the previous match.
	 */
    l = shl->lnum + shl->rm.endpos[0].lnum - shl->rm.startpos[0].lnum;
    if (lnum > l)
      shl->lnum = 0;
    else if (lnum < l || shl->rm.endpos[0].col > mincol)
      return;
  }

  /*
     * Repeat searching for a match until one is found that includes "mincol"
     * or none is found in this line.
     */
  called_emsg = FALSE;
  for (;;)
  {
#ifdef FEAT_RELTIME
    /* Stop searching after passing the time limit. */
    if (profile_passed_limit(&(shl->tm)))
    {
      shl->lnum = 0; /* no match found in time */
      break;
    }
#endif
    /* Three situations:
	 * 1. No useful previous match: search from start of line.
	 * 2. Not Vi compatible or empty match: continue at next character.
	 *    Break the loop if this is beyond the end of the line.
	 * 3. Vi compatible searching: continue at end of previous match.
	 */
    if (shl->lnum == 0)
      matchcol = 0;
    else if (vim_strchr(p_cpo, CPO_SEARCH) == NULL || (shl->rm.endpos[0].lnum == 0 && shl->rm.endpos[0].col <= shl->rm.startpos[0].col))
    {
      char_u *ml;

      matchcol = shl->rm.startpos[0].col;
      ml = ml_get_buf(shl->buf, lnum, FALSE) + matchcol;
      if (*ml == NUL)
      {
        ++matchcol;
        shl->lnum = 0;
        break;
      }
      if (has_mbyte)
        matchcol += mb_ptr2len(ml);
      else
        ++matchcol;
    }
    else
      matchcol = shl->rm.endpos[0].col;

    shl->lnum = lnum;
    if (shl->rm.regprog != NULL)
    {
      /* Remember whether shl->rm is using a copy of the regprog in
	     * cur->match. */
      int regprog_is_copy = (shl != &search_hl && cur != NULL && shl == &cur->hl && cur->match.regprog == cur->hl.rm.regprog);
      int timed_out = FALSE;

      nmatched = vim_regexec_multi(&shl->rm, win, shl->buf, lnum,
                                   matchcol,
#ifdef FEAT_RELTIME
                                   &(shl->tm), &timed_out
#else
                                   NULL, NULL
#endif
      );
      /* Copy the regprog, in case it got freed and recompiled. */
      if (regprog_is_copy)
        cur->match.regprog = cur->hl.rm.regprog;

      if (called_emsg || got_int || timed_out)
      {
        /* Error while handling regexp: stop using this regexp. */
        if (shl == &search_hl)
        {
          /* don't free regprog in the match list, it's a copy */
          vim_regfree(shl->rm.regprog);
          set_no_hlsearch(TRUE);
        }
        shl->rm.regprog = NULL;
        shl->lnum = 0;
        got_int = FALSE; /* avoid the "Type :quit to exit Vim"
				     message */
        break;
      }
    }
    else if (cur != NULL)
      nmatched = next_search_hl_pos(shl, lnum, &(cur->pos), matchcol);
    else
      nmatched = 0;
    if (nmatched == 0)
    {
      shl->lnum = 0; /* no match found */
      break;
    }
    if (shl->rm.startpos[0].lnum > 0 || shl->rm.startpos[0].col >= mincol || nmatched > 1 || shl->rm.endpos[0].col > mincol)
    {
      shl->lnum += shl->rm.startpos[0].lnum;
      break; /* useful match found */
    }
  }

  // Restore called_emsg for assert_fails().
  called_emsg = save_called_emsg;
}

/*
 * If there is a match fill "shl" and return one.
 * Return zero otherwise.
 */
static int
next_search_hl_pos(
    match_T *shl, /* points to a match */
    linenr_T lnum,
    posmatch_T *posmatch, /* match positions */
    colnr_T mincol)       /* minimal column for a match */
{
  int i;
  int found = -1;

  for (i = posmatch->cur; i < MAXPOSMATCH; i++)
  {
    llpos_T *pos = &posmatch->pos[i];

    if (pos->lnum == 0)
      break;
    if (pos->len == 0 && pos->col < mincol)
      continue;
    if (pos->lnum == lnum)
    {
      if (found >= 0)
      {
        /* if this match comes before the one at "found" then swap
		 * them */
        if (pos->col < posmatch->pos[found].col)
        {
          llpos_T tmp = *pos;

          *pos = posmatch->pos[found];
          posmatch->pos[found] = tmp;
        }
      }
      else
        found = i;
    }
  }
  posmatch->cur = 0;
  if (found >= 0)
  {
    colnr_T start = posmatch->pos[found].col == 0
                        ? 0
                        : posmatch->pos[found].col - 1;
    colnr_T end = posmatch->pos[found].col == 0
                      ? MAXCOL
                      : start + posmatch->pos[found].len;

    shl->lnum = lnum;
    shl->rm.startpos[0].lnum = 0;
    shl->rm.startpos[0].col = start;
    shl->rm.endpos[0].lnum = 0;
    shl->rm.endpos[0].col = end;
    shl->is_addpos = TRUE;
    posmatch->cur = found + 1;
    return 1;
  }
  return 0;
}
#endif

static void
screen_start_highlight(int attr)
{
  attrentry_T *aep = NULL;

  screen_attr = attr;
  if (full_screen
#ifdef MSWIN
      && termcap_active
#endif
  )
  {
#ifdef FEAT_GUI
    if (gui.in_use)
    {
      char buf[20];

      /* The GUI handles this internally. */
      sprintf(buf, IF_EB("\033|%dh", ESC_STR "|%dh"), attr);
      OUT_STR(buf);
    }
    else
#endif
    {
      if (attr > HL_ALL) /* special HL attr. */
      {
        if (IS_CTERM)
          aep = syn_cterm_attr2entry(attr);
        else
          aep = syn_term_attr2entry(attr);
        if (aep == NULL) /* did ":syntax clear" */
          attr = 0;
        else
          attr = aep->ae_attr;
      }
      if ((attr & HL_BOLD) && *T_MD != NUL) /* bold */
        out_str(T_MD);
      else if (aep != NULL && cterm_normal_fg_bold && (t_colors > 1 && aep->ae_u.cterm.fg_color))
        /* If the Normal FG color has BOLD attribute and the new HL
		 * has a FG color defined, clear BOLD. */
        out_str(T_ME);
      if ((attr & HL_STANDOUT) && *T_SO != NUL) /* standout */
        out_str(T_SO);
      if ((attr & HL_UNDERCURL) && *T_UCS != NUL) /* undercurl */
        out_str(T_UCS);
      if (((attr & HL_UNDERLINE) /* underline or undercurl */
           || ((attr & HL_UNDERCURL) && *T_UCS == NUL)) &&
          *T_US != NUL)
        out_str(T_US);
      if ((attr & HL_ITALIC) && *T_CZH != NUL) /* italic */
        out_str(T_CZH);
      if ((attr & HL_INVERSE) && *T_MR != NUL) /* inverse (reverse) */
        out_str(T_MR);
      if ((attr & HL_STRIKETHROUGH) && *T_STS != NUL) /* strike */
        out_str(T_STS);

      /*
	     * Output the color or start string after bold etc., in case the
	     * bold etc. override the color setting.
	     */
      if (aep != NULL)
      {
        if (t_colors > 1)
        {
          if (aep->ae_u.cterm.fg_color)
            term_fg_color(aep->ae_u.cterm.fg_color - 1);
        }
        if (t_colors > 1)
        {
          if (aep->ae_u.cterm.bg_color)
            term_bg_color(aep->ae_u.cterm.bg_color - 1);
        }

        if (!IS_CTERM)
        {
          if (aep->ae_u.term.start != NULL)
            out_str(aep->ae_u.term.start);
        }
      }
    }
  }
}

void screen_stop_highlight(void)
{
  int do_ME = FALSE; /* output T_ME code */

  if (screen_attr != 0
#ifdef MSWIN
      && termcap_active
#endif
  )
  {
#ifdef FEAT_GUI
    if (gui.in_use)
    {
      char buf[20];

      /* use internal GUI code */
      sprintf(buf, IF_EB("\033|%dH", ESC_STR "|%dH"), screen_attr);
      OUT_STR(buf);
    }
    else
#endif
    {
      if (screen_attr > HL_ALL) /* special HL attr. */
      {
        attrentry_T *aep;

        if (IS_CTERM)
        {
          /*
		     * Assume that t_me restores the original colors!
		     */
          aep = syn_cterm_attr2entry(screen_attr);
          if (aep != NULL && ((
                                  aep->ae_u.cterm.fg_color) ||
                              (aep->ae_u.cterm.bg_color)))
            do_ME = TRUE;
        }
        else
        {
          aep = syn_term_attr2entry(screen_attr);
          if (aep != NULL && aep->ae_u.term.stop != NULL)
          {
            if (STRCMP(aep->ae_u.term.stop, T_ME) == 0)
              do_ME = TRUE;
            else
              out_str(aep->ae_u.term.stop);
          }
        }
        if (aep == NULL) /* did ":syntax clear" */
          screen_attr = 0;
        else
          screen_attr = aep->ae_attr;
      }

      /*
	     * Often all ending-codes are equal to T_ME.  Avoid outputting the
	     * same sequence several times.
	     */
      if (screen_attr & HL_STANDOUT)
      {
        if (STRCMP(T_SE, T_ME) == 0)
          do_ME = TRUE;
        else
          out_str(T_SE);
      }
      if ((screen_attr & HL_UNDERCURL) && *T_UCE != NUL)
      {
        if (STRCMP(T_UCE, T_ME) == 0)
          do_ME = TRUE;
        else
          out_str(T_UCE);
      }
      if ((screen_attr & HL_UNDERLINE) || ((screen_attr & HL_UNDERCURL) && *T_UCE == NUL))
      {
        if (STRCMP(T_UE, T_ME) == 0)
          do_ME = TRUE;
        else
          out_str(T_UE);
      }
      if (screen_attr & HL_ITALIC)
      {
        if (STRCMP(T_CZR, T_ME) == 0)
          do_ME = TRUE;
        else
          out_str(T_CZR);
      }
      if (screen_attr & HL_STRIKETHROUGH)
      {
        if (STRCMP(T_STE, T_ME) == 0)
          do_ME = TRUE;
        else
          out_str(T_STE);
      }
      if (do_ME || (screen_attr & (HL_BOLD | HL_INVERSE)))
        out_str(T_ME);

      {
        if (t_colors > 1)
        {
          /* set Normal cterm colors */
          if (cterm_normal_fg_color != 0)
            term_fg_color(cterm_normal_fg_color - 1);
          if (cterm_normal_bg_color != 0)
            term_bg_color(cterm_normal_bg_color - 1);
          if (cterm_normal_fg_bold)
            out_str(T_MD);
        }
      }
    }
  }
  screen_attr = 0;
}

/*
 * Reset the colors for a cterm.  Used when leaving Vim.
 * The machine specific code may override this again.
 */
void reset_cterm_colors(void)
{
  if (IS_CTERM)
  {
    /* set Normal cterm colors */
    if (cterm_normal_fg_color > 0 || cterm_normal_bg_color > 0)
    {
      out_str(T_OP);
      screen_attr = -1;
    }
    if (cterm_normal_fg_bold)
    {
      out_str(T_ME);
      screen_attr = -1;
    }
  }
}

/*
 * Put character ScreenLines["off"] on the screen at position "row" and "col",
 * using the attributes from ScreenAttrs["off"].
 */
static void
screen_char(unsigned off, int row, int col)
{
  int attr;

  /* Check for illegal values, just in case (could happen just after
     * resizing). */
  if (row >= screen_Rows || col >= screen_Columns)
    return;

  /* Outputting a character in the last cell on the screen may scroll the
     * screen up.  Only do it when the "xn" termcap property is set, otherwise
     * mark the character invalid (update it when scrolled up). */
  if (*T_XN == NUL && row == screen_Rows - 1 && col == screen_Columns - 1
#ifdef FEAT_RIGHTLEFT
      /* account for first command-line character in rightleft mode */
      && !cmdmsg_rl
#endif
  )
  {
    ScreenAttrs[off] = (sattr_T)-1;
    return;
  }

  /*
     * Stop highlighting first, so it's easier to move the cursor.
     */
  if (screen_char_attr != 0)
    attr = screen_char_attr;
  else
    attr = ScreenAttrs[off];
  if (screen_attr != attr)
    screen_stop_highlight();

  windgoto(row, col);

  if (screen_attr != attr)
    screen_start_highlight(attr);

  if (enc_utf8 && ScreenLinesUC[off] != 0)
  {
    char_u buf[MB_MAXBYTES + 1];

    if (utf_ambiguous_width(ScreenLinesUC[off]))
    {
      if (*p_ambw == 'd'
#ifdef FEAT_GUI
          && !gui.in_use
#endif
      )
      {
        /* Clear the two screen cells. If the character is actually
		 * single width it won't change the second cell. */
        out_str((char_u *)"  ");
        term_windgoto(row, col);
      }
      /* not sure where the cursor is after drawing the ambiguous width
	     * character */
      screen_cur_col = 9999;
    }
    else if (utf_char2cells(ScreenLinesUC[off]) > 1)
      ++screen_cur_col;

    /* Convert the UTF-8 character to bytes and write it. */
    buf[utfc_char2bytes(off, buf)] = NUL;
    out_str(buf);
  }

  screen_cur_col++;
}

/*
 * Used for enc_dbcs only: Put one double-wide character at ScreenLines["off"]
 * on the screen at position 'row' and 'col'.
 * The attributes of the first byte is used for all.  This is required to
 * output the two bytes of a double-byte character with nothing in between.
 */
static void
screen_char_2(unsigned off, int row, int col)
{
  /* Check for illegal values (could be wrong when screen was resized). */
  if (off + 1 >= (unsigned)(screen_Rows * screen_Columns))
    return;

  /* Outputting the last character on the screen may scrollup the screen.
     * Don't to it!  Mark the character invalid (update it when scrolled up) */
  if (row == screen_Rows - 1 && col >= screen_Columns - 2)
  {
    ScreenAttrs[off] = (sattr_T)-1;
    return;
  }

  /* Output the first byte normally (positions the cursor), then write the
     * second byte directly. */
  screen_char(off, row, col);
  ++screen_cur_col;
}

/*
 * Draw a rectangle of the screen, inverted when "invert" is TRUE.
 * This uses the contents of ScreenLines[] and doesn't change it.
 */
void screen_draw_rectangle(
    int row,
    int col,
    int height,
    int width,
    int invert)
{
  int r, c;
  int off;
  int max_off;

  /* Can't use ScreenLines unless initialized */
  if (ScreenLines == NULL)
    return;

  if (invert)
    screen_char_attr = HL_INVERSE;
  for (r = row; r < row + height; ++r)
  {
    off = LineOffset[r];
    max_off = off + screen_Columns;
    for (c = col; c < col + width; ++c)
    {
      if (enc_dbcs != 0 && dbcs_off2cells(off + c, max_off) > 1)
      {
        screen_char_2(off + c, r, c);
        ++c;
      }
      else
      {
        screen_char(off + c, r, c);
        if (utf_off2cells(off + c, max_off) > 1)
          ++c;
      }
    }
  }
  screen_char_attr = 0;
}

/*
 * Redraw the characters for a vertically split window.
 */
static void
redraw_block(int row, int end, win_T *wp)
{
  int col;
  int width;

#ifdef FEAT_CLIPBOARD
  clip_may_clear_selection(row, end - 1);
#endif

  if (wp == NULL)
  {
    col = 0;
    width = Columns;
  }
  else
  {
    col = wp->w_wincol;
    width = wp->w_width;
  }
  screen_draw_rectangle(row, col, end - row, width, FALSE);
}

static void
space_to_screenline(int off, int attr)
{
  ScreenLines[off] = ' ';
  ScreenAttrs[off] = attr;
  if (enc_utf8)
    ScreenLinesUC[off] = 0;
}

/*
 * Fill the screen from 'start_row' to 'end_row', from 'start_col' to 'end_col'
 * with character 'c1' in first column followed by 'c2' in the other columns.
 * Use attributes 'attr'.
 */
void screen_fill(
    int start_row,
    int end_row,
    int start_col,
    int end_col,
    int c1,
    int c2,
    int attr)
{
  int row;
  int col;
  int off;
  int end_off;
  int did_delete;
  int c;
  int norm_term;
#if defined(FEAT_GUI) || defined(UNIX)
  int force_next = FALSE;
#endif

  if (end_row > screen_Rows) /* safety check */
    end_row = screen_Rows;
  if (end_col > screen_Columns) /* safety check */
    end_col = screen_Columns;
  if (ScreenLines == NULL || start_row >= end_row || start_col >= end_col) /* nothing to do */
    return;

  /* it's a "normal" terminal when not in a GUI or cterm */
  norm_term = (
#ifdef FEAT_GUI
      !gui.in_use &&
#endif
      !IS_CTERM);
  for (row = start_row; row < end_row; ++row)
  {
    if (has_mbyte
#ifdef FEAT_GUI
        && !gui.in_use
#endif
    )
    {
      /* When drawing over the right halve of a double-wide char clear
	     * out the left halve.  When drawing over the left halve of a
	     * double wide-char clear out the right halve.  Only needed in a
	     * terminal. */
      if (start_col > 0 && mb_fix_col(start_col, row) != start_col)
        screen_puts_len((char_u *)" ", 1, row, start_col - 1, 0);
      if (end_col < screen_Columns && mb_fix_col(end_col, row) != end_col)
        screen_puts_len((char_u *)" ", 1, row, end_col, 0);
    }
    /*
	 * Try to use delete-line termcap code, when no attributes or in a
	 * "normal" terminal, where a bold/italic space is just a
	 * space.
	 */
    did_delete = FALSE;
    if (c2 == ' ' && end_col == Columns && can_clear(T_CE) && (attr == 0 || (norm_term && attr <= HL_ALL && ((attr & ~(HL_BOLD | HL_ITALIC)) == 0))))
    {
      /*
	     * check if we really need to clear something
	     */
      col = start_col;
      if (c1 != ' ') /* don't clear first char */
        ++col;

      off = LineOffset[row] + col;
      end_off = LineOffset[row] + end_col;

      /* skip blanks (used often, keep it fast!) */
      if (enc_utf8)
        while (off < end_off && ScreenLines[off] == ' ' && ScreenAttrs[off] == 0 && ScreenLinesUC[off] == 0)
          ++off;
      else
        while (off < end_off && ScreenLines[off] == ' ' && ScreenAttrs[off] == 0)
          ++off;
      if (off < end_off) /* something to be cleared */
      {
        col = off - LineOffset[row];
        screen_stop_highlight();
        term_windgoto(row, col); /* clear rest of this screen line */
        out_str(T_CE);
        screen_start(); /* don't know where cursor is now */
        col = end_col - col;
        while (col--) /* clear chars in ScreenLines */
        {
          space_to_screenline(off, 0);
          ++off;
        }
      }
      did_delete = TRUE; /* the chars are cleared now */
    }

    off = LineOffset[row] + start_col;
    c = c1;
    for (col = start_col; col < end_col; ++col)
    {
      if (ScreenLines[off] != c || (enc_utf8 && (int)ScreenLinesUC[off] != (c >= 0x80 ? c : 0)) || ScreenAttrs[off] != attr
#if defined(FEAT_GUI) || defined(UNIX)
          || force_next
#endif
      )
      {
#if defined(FEAT_GUI) || defined(UNIX)
        /* The bold trick may make a single row of pixels appear in
		 * the next character.  When a bold character is removed, the
		 * next character should be redrawn too.  This happens for our
		 * own GUI and for some xterms.  */
        if (
#ifdef FEAT_GUI
            gui.in_use
#endif
#if defined(FEAT_GUI) && defined(UNIX)
            ||
#endif
#ifdef UNIX
            term_is_xterm
#endif
        )
        {
          if (ScreenLines[off] != ' ' && (ScreenAttrs[off] > HL_ALL || ScreenAttrs[off] & HL_BOLD))
            force_next = TRUE;
          else
            force_next = FALSE;
        }
#endif
        ScreenLines[off] = c;
        if (enc_utf8)
        {
          if (c >= 0x80)
          {
            ScreenLinesUC[off] = c;
            ScreenLinesC[0][off] = 0;
          }
          else
            ScreenLinesUC[off] = 0;
        }
        ScreenAttrs[off] = attr;
        if (!did_delete || c != ' ')
          screen_char(off, row, col);
      }
      ++off;
      if (col == start_col)
      {
        if (did_delete)
          break;
        c = c2;
      }
    }
    if (end_col == Columns)
      LineWraps[row] = FALSE;
    if (row == Rows - 1) /* overwritten the command line */
    {
      redraw_cmdline = TRUE;
      if (start_col == 0 && end_col == Columns && c1 == ' ' && c2 == ' ' && attr == 0)
        clear_cmdline = FALSE; /* command line has been cleared */
      if (start_col == 0)
        mode_displayed = FALSE; /* mode cleared or overwritten */
    }
  }
}

/*
 * Check if there should be a delay.  Used before clearing or redrawing the
 * screen or the command line.
 */
void check_for_delay(int check_msg_scroll)
{
  if ((emsg_on_display || (check_msg_scroll && msg_scroll)) && !did_wait_return && emsg_silent == 0)
  {
    emsg_on_display = FALSE;
    if (check_msg_scroll)
      msg_scroll = FALSE;
  }
}

/*
 * Init TabPageIdxs[] to zero: Clicking outside of tabs has no effect.
 */
static void
clear_TabPageIdxs(void)
{
  int scol;

  for (scol = 0; scol < Columns; ++scol)
    TabPageIdxs[scol] = 0;
}

/*
 * screen_valid -  allocate screen buffers if size changed
 *   If "doclear" is TRUE: clear screen if it has been resized.
 *	Returns TRUE if there is a valid screen to write to.
 *	Returns FALSE when starting up and screen not initialized yet.
 */
int screen_valid(int doclear)
{
  screenalloc(doclear); /* allocate screen buffers if size changed */
  return (ScreenLines != NULL);
}

/*
 * Resize the shell to Rows and Columns.
 * Allocate ScreenLines[] and associated items.
 *
 * There may be some time between setting Rows and Columns and (re)allocating
 * ScreenLines[].  This happens when starting up and when (manually) changing
 * the shell size.  Always use screen_Rows and screen_Columns to access items
 * in ScreenLines[].  Use Rows and Columns for positioning text etc. where the
 * final size of the shell is needed.
 */
void screenalloc(int doclear)
{
  int new_row, old_row;
#ifdef FEAT_GUI
  int old_Rows;
#endif
  win_T *wp;
  int outofmem = FALSE;
  int len;
  schar_T *new_ScreenLines;
  u8char_T *new_ScreenLinesUC = NULL;
  u8char_T *new_ScreenLinesC[MAX_MCO];
  schar_T *new_ScreenLines2 = NULL;
  int i;
  sattr_T *new_ScreenAttrs;
  unsigned *new_LineOffset;
  char_u *new_LineWraps;
  short *new_TabPageIdxs;
  tabpage_T *tp;
  static int entered = FALSE;           /* avoid recursiveness */
  static int done_outofmem_msg = FALSE; /* did outofmem message */
  int retry_count = 0;

retry:
  /*
     * Allocation of the screen buffers is done only when the size changes and
     * when Rows and Columns have been set and we have started doing full
     * screen stuff.
     */
  if ((ScreenLines != NULL && Rows == screen_Rows && Columns == screen_Columns && enc_utf8 == (ScreenLinesUC != NULL) && (enc_dbcs == DBCS_JPNU) == (ScreenLines2 != NULL) && p_mco == Screen_mco) || Rows == 0 || Columns == 0 || (!full_screen && ScreenLines == NULL))
    return;

  /*
     * It's possible that we produce an out-of-memory message below, which
     * will cause this function to be called again.  To break the loop, just
     * return here.
     */
  if (entered)
    return;
  entered = TRUE;

  /*
     * Note that the window sizes are updated before reallocating the arrays,
     * thus we must not redraw here!
     */
  ++RedrawingDisabled;

  win_new_shellsize(); /* fit the windows in the new sized shell */

  comp_col(); /* recompute columns for shown command and ruler */

  /*
     * We're changing the size of the screen.
     * - Allocate new arrays for ScreenLines and ScreenAttrs.
     * - Move lines from the old arrays into the new arrays, clear extra
     *	 lines (unless the screen is going to be cleared).
     * - Free the old arrays.
     *
     * If anything fails, make ScreenLines NULL, so we don't do anything!
     * Continuing with the old ScreenLines may result in a crash, because the
     * size is wrong.
     */
  FOR_ALL_TAB_WINDOWS(tp, wp)
  win_free_lsize(wp);
  if (aucmd_win != NULL)
    win_free_lsize(aucmd_win);

  new_ScreenLines = LALLOC_MULT(schar_T, (Rows + 1) * Columns);
  vim_memset(new_ScreenLinesC, 0, sizeof(u8char_T *) * MAX_MCO);
  if (enc_utf8)
  {
    new_ScreenLinesUC = LALLOC_MULT(u8char_T, (Rows + 1) * Columns);
    for (i = 0; i < p_mco; ++i)
      new_ScreenLinesC[i] = LALLOC_CLEAR_MULT(u8char_T,
                                              (Rows + 1) * Columns);
  }
  if (enc_dbcs == DBCS_JPNU)
    new_ScreenLines2 = LALLOC_MULT(schar_T, (Rows + 1) * Columns);
  new_ScreenAttrs = LALLOC_MULT(sattr_T, (Rows + 1) * Columns);
  new_LineOffset = LALLOC_MULT(unsigned, Rows);
  new_LineWraps = LALLOC_MULT(char_u, Rows);
  new_TabPageIdxs = LALLOC_MULT(short, Columns);

  FOR_ALL_TAB_WINDOWS(tp, wp)
  {
    if (win_alloc_lines(wp) == FAIL)
    {
      outofmem = TRUE;
      goto give_up;
    }
  }
  if (aucmd_win != NULL && aucmd_win->w_lines == NULL && win_alloc_lines(aucmd_win) == FAIL)
    outofmem = TRUE;

give_up:

  for (i = 0; i < p_mco; ++i)
    if (new_ScreenLinesC[i] == NULL)
      break;
  if (new_ScreenLines == NULL || (enc_utf8 && (new_ScreenLinesUC == NULL || i != p_mco)) || (enc_dbcs == DBCS_JPNU && new_ScreenLines2 == NULL) || new_ScreenAttrs == NULL || new_LineOffset == NULL || new_LineWraps == NULL || new_TabPageIdxs == NULL || outofmem)
  {
    if (ScreenLines != NULL || !done_outofmem_msg)
    {
      /* guess the size */
      do_outofmem_msg((long_u)((Rows + 1) * Columns));

      /* Remember we did this to avoid getting outofmem messages over
	     * and over again. */
      done_outofmem_msg = TRUE;
    }
    VIM_CLEAR(new_ScreenLines);
    VIM_CLEAR(new_ScreenLinesUC);
    for (i = 0; i < p_mco; ++i)
      VIM_CLEAR(new_ScreenLinesC[i]);
    VIM_CLEAR(new_ScreenLines2);
    VIM_CLEAR(new_ScreenAttrs);
    VIM_CLEAR(new_LineOffset);
    VIM_CLEAR(new_LineWraps);
    VIM_CLEAR(new_TabPageIdxs);
  }
  else
  {
    done_outofmem_msg = FALSE;

    for (new_row = 0; new_row < Rows; ++new_row)
    {
      new_LineOffset[new_row] = new_row * Columns;
      new_LineWraps[new_row] = FALSE;

      /*
	     * If the screen is not going to be cleared, copy as much as
	     * possible from the old screen to the new one and clear the rest
	     * (used when resizing the window at the "--more--" prompt or when
	     * executing an external command, for the GUI).
	     */
      if (!doclear)
      {
        (void)vim_memset(new_ScreenLines + new_row * Columns,
                         ' ', (size_t)Columns * sizeof(schar_T));
        if (enc_utf8)
        {
          (void)vim_memset(new_ScreenLinesUC + new_row * Columns,
                           0, (size_t)Columns * sizeof(u8char_T));
          for (i = 0; i < p_mco; ++i)
            (void)vim_memset(new_ScreenLinesC[i] + new_row * Columns,
                             0, (size_t)Columns * sizeof(u8char_T));
        }
        if (enc_dbcs == DBCS_JPNU)
          (void)vim_memset(new_ScreenLines2 + new_row * Columns,
                           0, (size_t)Columns * sizeof(schar_T));
        (void)vim_memset(new_ScreenAttrs + new_row * Columns,
                         0, (size_t)Columns * sizeof(sattr_T));
        old_row = new_row + (screen_Rows - Rows);
        if (old_row >= 0 && ScreenLines != NULL)
        {
          if (screen_Columns < Columns)
            len = screen_Columns;
          else
            len = Columns;
          /* When switching to utf-8 don't copy characters, they
		     * may be invalid now.  Also when p_mco changes. */
          if (!(enc_utf8 && ScreenLinesUC == NULL) && p_mco == Screen_mco)
            mch_memmove(new_ScreenLines + new_LineOffset[new_row],
                        ScreenLines + LineOffset[old_row],
                        (size_t)len * sizeof(schar_T));
          if (enc_utf8 && ScreenLinesUC != NULL && p_mco == Screen_mco)
          {
            mch_memmove(new_ScreenLinesUC + new_LineOffset[new_row],
                        ScreenLinesUC + LineOffset[old_row],
                        (size_t)len * sizeof(u8char_T));
            for (i = 0; i < p_mco; ++i)
              mch_memmove(new_ScreenLinesC[i] + new_LineOffset[new_row],
                          ScreenLinesC[i] + LineOffset[old_row],
                          (size_t)len * sizeof(u8char_T));
          }
          if (enc_dbcs == DBCS_JPNU && ScreenLines2 != NULL)
            mch_memmove(new_ScreenLines2 + new_LineOffset[new_row],
                        ScreenLines2 + LineOffset[old_row],
                        (size_t)len * sizeof(schar_T));
          mch_memmove(new_ScreenAttrs + new_LineOffset[new_row],
                      ScreenAttrs + LineOffset[old_row],
                      (size_t)len * sizeof(sattr_T));
        }
      }
    }
    /* Use the last line of the screen for the current line. */
    current_ScreenLine = new_ScreenLines + Rows * Columns;
  }

  free_screenlines();

  ScreenLines = new_ScreenLines;
  ScreenLinesUC = new_ScreenLinesUC;
  for (i = 0; i < p_mco; ++i)
    ScreenLinesC[i] = new_ScreenLinesC[i];
  Screen_mco = p_mco;
  ScreenLines2 = new_ScreenLines2;
  ScreenAttrs = new_ScreenAttrs;
  LineOffset = new_LineOffset;
  LineWraps = new_LineWraps;
  TabPageIdxs = new_TabPageIdxs;

  /* It's important that screen_Rows and screen_Columns reflect the actual
     * size of ScreenLines[].  Set them before calling anything. */
#ifdef FEAT_GUI
  old_Rows = screen_Rows;
#endif
  screen_Rows = Rows;
  screen_Columns = Columns;

  must_redraw = CLEAR; /* need to clear the screen later */
  if (doclear)
    screenclear2();
#ifdef FEAT_GUI
  else if (gui.in_use && !gui.starting && ScreenLines != NULL && old_Rows != Rows)
  {
    (void)gui_redraw_block(0, 0, (int)Rows - 1, (int)Columns - 1, 0);
    /*
	 * Adjust the position of the cursor, for when executing an external
	 * command.
	 */
    if (msg_row >= Rows)          /* Rows got smaller */
      msg_row = Rows - 1;         /* put cursor at last row */
    else if (Rows > old_Rows)     /* Rows got bigger */
      msg_row += Rows - old_Rows; /* put cursor in same place */
    if (msg_col >= Columns)       /* Columns got smaller */
      msg_col = Columns - 1;      /* put cursor at last column */
  }
#endif
  clear_TabPageIdxs();

  entered = FALSE;
  --RedrawingDisabled;

  /*
     * Do not apply autocommands more than 3 times to avoid an endless loop
     * in case applying autocommands always changes Rows or Columns.
     */
  if (starting == 0 && ++retry_count <= 3)
  {
    apply_autocmds(EVENT_VIMRESIZED, NULL, NULL, FALSE, curbuf);
    /* In rare cases, autocommands may have altered Rows or Columns,
	 * jump back to check if we need to allocate the screen again. */
    goto retry;
  }
}

void free_screenlines(void)
{
  int i;

  vim_free(ScreenLinesUC);
  for (i = 0; i < Screen_mco; ++i)
    vim_free(ScreenLinesC[i]);
  vim_free(ScreenLines2);
  vim_free(ScreenLines);
  vim_free(ScreenAttrs);
  vim_free(LineOffset);
  vim_free(LineWraps);
  vim_free(TabPageIdxs);
}

void screenclear(void)
{
  check_for_delay(FALSE);
  screenalloc(FALSE); /* allocate screen buffers if size changed */
  screenclear2();     /* clear the screen */
}

static void
screenclear2(void)
{
  int i;

  if (starting == NO_SCREEN || ScreenLines == NULL
#ifdef FEAT_GUI
      || (gui.in_use && gui.starting)
#endif
  )
    return;

#ifdef FEAT_GUI
  if (!gui.in_use)
#endif
    screen_attr = -1;      /* force setting the Normal colors */
  screen_stop_highlight(); /* don't want highlighting here */

#ifdef FEAT_CLIPBOARD
  /* disable selection without redrawing it */
  clip_scroll_selection(9999);
#endif

  /* blank out ScreenLines */
  for (i = 0; i < Rows; ++i)
  {
    lineclear(LineOffset[i], (int)Columns, 0);
    LineWraps[i] = FALSE;
  }

  if (can_clear(T_CL))
  {
    out_str(T_CL); /* clear the display */
    clear_cmdline = FALSE;
    mode_displayed = FALSE;
  }
  else
  {
    /* can't clear the screen, mark all chars with invalid attributes */
    for (i = 0; i < Rows; ++i)
      lineinvalid(LineOffset[i], (int)Columns);
    clear_cmdline = TRUE;
  }

  screen_cleared = TRUE; /* can use contents of ScreenLines now */

  win_rest_invalid(firstwin);
  redraw_cmdline = TRUE;
  redraw_tabline = TRUE;
  if (must_redraw == CLEAR) /* no need to clear again */
    must_redraw = NOT_VALID;
  compute_cmdrow();
  msg_row = cmdline_row; /* put cursor on last line for messages */
  msg_col = 0;
  screen_start();   /* don't know where cursor is now */
  msg_scrolled = 0; /* can't scroll back */
  msg_didany = FALSE;
  msg_didout = FALSE;
}

/*
 * Clear one line in ScreenLines.
 */
static void
lineclear(unsigned off, int width, int attr)
{
  (void)vim_memset(ScreenLines + off, ' ', (size_t)width * sizeof(schar_T));
  if (enc_utf8)
    (void)vim_memset(ScreenLinesUC + off, 0,
                     (size_t)width * sizeof(u8char_T));
  (void)vim_memset(ScreenAttrs + off, attr, (size_t)width * sizeof(sattr_T));
}

/*
 * Mark one line in ScreenLines invalid by setting the attributes to an
 * invalid value.
 */
static void
lineinvalid(unsigned off, int width)
{
  (void)vim_memset(ScreenAttrs + off, -1, (size_t)width * sizeof(sattr_T));
}

/*
 * Copy part of a Screenline for vertically split window "wp".
 */
static void
linecopy(int to, int from, win_T *wp)
{
  unsigned off_to = LineOffset[to] + wp->w_wincol;
  unsigned off_from = LineOffset[from] + wp->w_wincol;

  mch_memmove(ScreenLines + off_to, ScreenLines + off_from,
              wp->w_width * sizeof(schar_T));
  if (enc_utf8)
  {
    int i;

    mch_memmove(ScreenLinesUC + off_to, ScreenLinesUC + off_from,
                wp->w_width * sizeof(u8char_T));
    for (i = 0; i < p_mco; ++i)
      mch_memmove(ScreenLinesC[i] + off_to, ScreenLinesC[i] + off_from,
                  wp->w_width * sizeof(u8char_T));
  }
  if (enc_dbcs == DBCS_JPNU)
    mch_memmove(ScreenLines2 + off_to, ScreenLines2 + off_from,
                wp->w_width * sizeof(schar_T));
  mch_memmove(ScreenAttrs + off_to, ScreenAttrs + off_from,
              wp->w_width * sizeof(sattr_T));
}

/*
 * Return TRUE if clearing with term string "p" would work.
 * It can't work when the string is empty or it won't set the right background.
 */
int can_clear(char_u *p)
{
  return (*p != NUL && (t_colors <= 1
#ifdef FEAT_GUI
                        || gui.in_use
#endif
                        || cterm_normal_bg_color == 0 || *T_UT != NUL));
}

/*
 * Reset cursor position. Use whenever cursor was moved because of outputting
 * something directly to the screen (shell commands) or a terminal control
 * code.
 */
void screen_start(void)
{
  screen_cur_row = screen_cur_col = 9999;
}

/*
 * Move the cursor to position "row","col" in the screen.
 * This tries to find the most efficient way to move, minimizing the number of
 * characters sent to the terminal.
 */
void windgoto(int row, int col)
{
  sattr_T *p;
  int i;
  int plan;
  int cost;
  int wouldbe_col;
  int noinvcurs;
  char_u *bs;
  int goto_cost;
  int attr;

#define GOTO_COST 7  /* assume a term_windgoto() takes about 7 chars */
#define HIGHL_COST 5 /* assume unhighlight takes 5 chars */

#define PLAN_LE 1
#define PLAN_CR 2
#define PLAN_NL 3
#define PLAN_WRITE 4
  /* Can't use ScreenLines unless initialized */
  if (ScreenLines == NULL)
    return;

  if (col != screen_cur_col || row != screen_cur_row)
  {
    /* Check for valid position. */
    if (row < 0) /* window without text lines? */
      row = 0;
    if (row >= screen_Rows)
      row = screen_Rows - 1;
    if (col >= screen_Columns)
      col = screen_Columns - 1;

    /* check if no cursor movement is allowed in highlight mode */
    if (screen_attr && *T_MS == NUL)
      noinvcurs = HIGHL_COST;
    else
      noinvcurs = 0;
    goto_cost = GOTO_COST + noinvcurs;

    /*
	 * Plan how to do the positioning:
	 * 1. Use CR to move it to column 0, same row.
	 * 2. Use T_LE to move it a few columns to the left.
	 * 3. Use NL to move a few lines down, column 0.
	 * 4. Move a few columns to the right with T_ND or by writing chars.
	 *
	 * Don't do this if the cursor went beyond the last column, the cursor
	 * position is unknown then (some terminals wrap, some don't )
	 *
	 * First check if the highlighting attributes allow us to write
	 * characters to move the cursor to the right.
	 */
    if (row >= screen_cur_row && screen_cur_col < Columns)
    {
      /*
	     * If the cursor is in the same row, bigger col, we can use CR
	     * or T_LE.
	     */
      bs = NULL; /* init for GCC */
      attr = screen_attr;
      if (row == screen_cur_row && col < screen_cur_col)
      {
        /* "le" is preferred over "bc", because "bc" is obsolete */
        if (*T_LE)
          bs = T_LE; /* "cursor left" */
        else
          bs = T_BC; /* "backspace character (old) */
        if (*bs)
          cost = (screen_cur_col - col) * (int)STRLEN(bs);
        else
          cost = 999;
        if (col + 1 < cost) /* using CR is less characters */
        {
          plan = PLAN_CR;
          wouldbe_col = 0;
          cost = 1; /* CR is just one character */
        }
        else
        {
          plan = PLAN_LE;
          wouldbe_col = col;
        }
        if (noinvcurs) /* will stop highlighting */
        {
          cost += noinvcurs;
          attr = 0;
        }
      }

      /*
	     * If the cursor is above where we want to be, we can use CR LF.
	     */
      else if (row > screen_cur_row)
      {
        plan = PLAN_NL;
        wouldbe_col = 0;
        cost = (row - screen_cur_row) * 2; /* CR LF */
        if (noinvcurs)                     /* will stop highlighting */
        {
          cost += noinvcurs;
          attr = 0;
        }
      }

      /*
	     * If the cursor is in the same row, smaller col, just use write.
	     */
      else
      {
        plan = PLAN_WRITE;
        wouldbe_col = screen_cur_col;
        cost = 0;
      }

      /*
	     * Check if any characters that need to be written have the
	     * correct attributes.  Also avoid UTF-8 characters.
	     */
      i = col - wouldbe_col;
      if (i > 0)
        cost += i;
      if (cost < goto_cost && i > 0)
      {
        /*
		 * Check if the attributes are correct without additionally
		 * stopping highlighting.
		 */
        p = ScreenAttrs + LineOffset[row] + wouldbe_col;
        while (i && *p++ == attr)
          --i;
        if (i != 0)
        {
          /*
		     * Try if it works when highlighting is stopped here.
		     */
          if (*--p == 0)
          {
            cost += noinvcurs;
            while (i && *p++ == 0)
              --i;
          }
          if (i != 0)
            cost = 999; /* different attributes, don't do it */
        }
        if (enc_utf8)
        {
          /* Don't use an UTF-8 char for positioning, it's slow. */
          for (i = wouldbe_col; i < col; ++i)
            if (ScreenLinesUC[LineOffset[row] + i] != 0)
            {
              cost = 999;
              break;
            }
        }
      }

      /*
	     * We can do it without term_windgoto()!
	     */
      if (cost < goto_cost)
      {
        if (plan == PLAN_LE)
        {
          if (noinvcurs)
            screen_stop_highlight();
          while (screen_cur_col > col)
          {
            out_str(bs);
            --screen_cur_col;
          }
        }
        else if (plan == PLAN_CR)
        {
          if (noinvcurs)
            screen_stop_highlight();
          screen_cur_col = 0;
        }
        else if (plan == PLAN_NL)
        {
          if (noinvcurs)
            screen_stop_highlight();
          screen_cur_row = row;
          screen_cur_col = 0;
        }

        i = col - screen_cur_col;
        if (i > 0)
        {
          /*
		     * Use cursor-right if it's one character only.  Avoids
		     * removing a line of pixels from the last bold char, when
		     * using the bold trick in the GUI.
		     */
          if (T_ND[0] != NUL && T_ND[1] == NUL)
          {
          }
          else
          {
            int off;

            off = LineOffset[row] + screen_cur_col;
            while (i-- > 0)
            {
              if (ScreenAttrs[off] != screen_attr)
                screen_stop_highlight();
              if (enc_dbcs == DBCS_JPNU && ScreenLines[off] == 0x8e)
                ++off;
            }
          }
        }
      }
    }
    else
      cost = 999;

    if (cost >= goto_cost)
    {
      if (noinvcurs)
        screen_stop_highlight();
      if (row == screen_cur_row && (col > screen_cur_col) && *T_CRI != NUL)
        term_cursor_right(col - screen_cur_col);
      else
        term_windgoto(row, col);
    }
    screen_cur_row = row;
    screen_cur_col = col;
  }
}

/*
 * Set cursor to its position in the current window.
 */
void setcursor(void)
{
  setcursor_mayforce(FALSE);
}

/*
 * Set cursor to its position in the current window.
 * When "force" is TRUE also when not redrawing.
 */
void setcursor_mayforce(int force)
{
  if (force || redrawing())
  {
    validate_cursor();
    windgoto(W_WINROW(curwin) + curwin->w_wrow,
             curwin->w_wincol + (
#ifdef FEAT_RIGHTLEFT
                                    /* With 'rightleft' set and the cursor on a double-wide
		 * character, position it on the leftmost column. */
                                    curwin->w_p_rl ? ((int)curwin->w_width - curwin->w_wcol - ((has_mbyte && (*mb_ptr2cells)(ml_get_cursor()) == 2 && vim_isprintc(gchar_cursor())) ? 2 : 1)) :
#endif
                                                   curwin->w_wcol));
  }
}

/*
 * Insert 'line_count' lines at 'row' in window 'wp'.
 * If 'invalid' is TRUE the wp->w_lines[].wl_lnum is invalidated.
 * If 'mayclear' is TRUE the screen will be cleared if it is faster than
 * scrolling.
 * Returns FAIL if the lines are not inserted, OK for success.
 */
int win_ins_lines(
    win_T *wp,
    int row,
    int line_count,
    int invalid,
    int mayclear)
{
  int did_delete;
  int nextrow;
  int lastrow;
  int retval;

  if (invalid)
    wp->w_lines_valid = 0;

  if (wp->w_height < 5)
    return FAIL;

  if (line_count > wp->w_height - row)
    line_count = wp->w_height - row;

  retval = win_do_lines(wp, row, line_count, mayclear, FALSE, 0);
  if (retval != MAYBE)
    return retval;

  /*
     * If there is a next window or a status line, we first try to delete the
     * lines at the bottom to avoid messing what is after the window.
     * If this fails and there are following windows, don't do anything to avoid
     * messing up those windows, better just redraw.
     */
  did_delete = FALSE;
  if (wp->w_next != NULL || wp->w_status_height)
  {
    if (screen_del_lines(0, W_WINROW(wp) + wp->w_height - line_count,
                         line_count, (int)Rows, FALSE, 0, NULL) == OK)
      did_delete = TRUE;
    else if (wp->w_next)
      return FAIL;
  }
  /*
     * if no lines deleted, blank the lines that will end up below the window
     */
  if (!did_delete)
  {
    wp->w_redr_status = TRUE;
    redraw_cmdline = TRUE;
    nextrow = W_WINROW(wp) + wp->w_height + wp->w_status_height;
    lastrow = nextrow + line_count;
    if (lastrow > Rows)
      lastrow = Rows;
    screen_fill(nextrow - line_count, lastrow - line_count,
                wp->w_wincol, (int)W_ENDCOL(wp),
                ' ', ' ', 0);
  }

  if (screen_ins_lines(0, W_WINROW(wp) + row, line_count, (int)Rows, 0, NULL) == FAIL)
  {
    /* deletion will have messed up other windows */
    if (did_delete)
    {
      wp->w_redr_status = TRUE;
      win_rest_invalid(W_NEXT(wp));
    }
    return FAIL;
  }

  return OK;
}

/*
 * Delete "line_count" window lines at "row" in window "wp".
 * If "invalid" is TRUE curwin->w_lines[] is invalidated.
 * If "mayclear" is TRUE the screen will be cleared if it is faster than
 * scrolling
 * Return OK for success, FAIL if the lines are not deleted.
 */
int win_del_lines(
    win_T *wp,
    int row,
    int line_count,
    int invalid,
    int mayclear,
    int clear_attr) /* for clearing lines */
{
  int retval;

  if (invalid)
    wp->w_lines_valid = 0;

  if (line_count > wp->w_height - row)
    line_count = wp->w_height - row;

  retval = win_do_lines(wp, row, line_count, mayclear, TRUE, clear_attr);
  if (retval != MAYBE)
    return retval;

  if (screen_del_lines(0, W_WINROW(wp) + row, line_count,
                       (int)Rows, FALSE, clear_attr, NULL) == FAIL)
    return FAIL;

  /*
     * If there are windows or status lines below, try to put them at the
     * correct place. If we can't do that, they have to be redrawn.
     */
  if (wp->w_next || wp->w_status_height || cmdline_row < Rows - 1)
  {
    if (screen_ins_lines(0, W_WINROW(wp) + wp->w_height - line_count,
                         line_count, (int)Rows, clear_attr, NULL) == FAIL)
    {
      wp->w_redr_status = TRUE;
      win_rest_invalid(wp->w_next);
    }
  }
  /*
     * If this is the last window and there is no status line, redraw the
     * command line later.
     */
  else
    redraw_cmdline = TRUE;
  return OK;
}

/*
 * Common code for win_ins_lines() and win_del_lines().
 * Returns OK or FAIL when the work has been done.
 * Returns MAYBE when not finished yet.
 */
static int
win_do_lines(
    win_T *wp,
    int row,
    int line_count,
    int mayclear,
    int del,
    int clear_attr)
{
  int retval;

  if (!redrawing() || line_count <= 0)
    return FAIL;

  /* When inserting lines would result in loss of command output, just redraw
     * the lines. */
  if (no_win_do_lines_ins && !del)
    return FAIL;

  /* only a few lines left: redraw is faster */
  if (mayclear && Rows - line_count < 5 && wp->w_width == Columns)
  {
    if (!no_win_do_lines_ins)
      screenclear(); /* will set wp->w_lines_valid to 0 */
    return FAIL;
  }

  /*
     * Delete all remaining lines
     */
  if (row + line_count >= wp->w_height)
  {
    screen_fill(W_WINROW(wp) + row, W_WINROW(wp) + wp->w_height,
                wp->w_wincol, (int)W_ENDCOL(wp),
                ' ', ' ', 0);
    return OK;
  }

  /*
     * When scrolling, the message on the command line should be cleared,
     * otherwise it will stay there forever.
     * Don't do this when avoiding to insert lines.
     */
  if (!no_win_do_lines_ins)
    clear_cmdline = TRUE;

  /*
     * If the terminal can set a scroll region, use that.
     * Always do this in a vertically split window.  This will redraw from
     * ScreenLines[] when t_CV isn't defined.  That's faster than using
     * win_line().
     * Don't use a scroll region when we are going to redraw the text, writing
     * a character in the lower right corner of the scroll region may cause a
     * scroll-up .
     */
  if (scroll_region || wp->w_width != Columns)
  {
    if (scroll_region && (wp->w_width == Columns || *T_CSV != NUL))
      scroll_region_set(wp, row);
    if (del)
      retval = screen_del_lines(W_WINROW(wp) + row, 0, line_count,
                                wp->w_height - row, FALSE, clear_attr, wp);
    else
      retval = screen_ins_lines(W_WINROW(wp) + row, 0, line_count,
                                wp->w_height - row, clear_attr, wp);
    if (scroll_region && (wp->w_width == Columns || *T_CSV != NUL))
      scroll_region_reset();
    return retval;
  }

  if (wp->w_next != NULL && p_tf) /* don't delete/insert on fast terminal */
    return FAIL;

  return MAYBE;
}

/*
 * window 'wp' and everything after it is messed up, mark it for redraw
 */
static void
win_rest_invalid(win_T *wp)
{
  while (wp != NULL)
  {
    redraw_win_later(wp, NOT_VALID);
    wp->w_redr_status = TRUE;
    wp = wp->w_next;
  }
  redraw_cmdline = TRUE;
}

/*
 * The rest of the routines in this file perform screen manipulations. The
 * given operation is performed physically on the screen. The corresponding
 * change is also made to the internal screen image. In this way, the editor
 * anticipates the effect of editing changes on the appearance of the screen.
 * That way, when we call screenupdate a complete redraw isn't usually
 * necessary. Another advantage is that we can keep adding code to anticipate
 * screen changes, and in the meantime, everything still works.
 */

/*
 * types for inserting or deleting lines
 */
#define USE_T_CAL 1
#define USE_T_CDL 2
#define USE_T_AL 3
#define USE_T_CE 4
#define USE_T_DL 5
#define USE_T_SR 6
#define USE_NL 7
#define USE_T_CD 8
#define USE_REDRAW 9

/*
 * insert lines on the screen and update ScreenLines[]
 * 'end' is the line after the scrolled part. Normally it is Rows.
 * When scrolling region used 'off' is the offset from the top for the region.
 * 'row' and 'end' are relative to the start of the region.
 *
 * return FAIL for failure, OK for success.
 */
int screen_ins_lines(
    int off,
    int row,
    int line_count,
    int end,
    int clear_attr,
    win_T *wp) /* NULL or window to use width from */
{
  int i;
  int j;
  unsigned temp;
  int cursor_row;
  int cursor_col = 0;
  int type;
  int result_empty;
  int can_ce = can_clear(T_CE);

  /*
     * FAIL if
     * - there is no valid screen
     * - the screen has to be redrawn completely
     * - the line count is less than one
     * - the line count is more than 'ttyscroll'
     * - redrawing for a callback and there is a modeless selection
     */
  if (!screen_valid(TRUE) || line_count <= 0 || line_count > p_ttyscroll
#ifdef FEAT_CLIPBOARD
      || (clip_star.state != SELECT_CLEARED && redrawing_for_callback > 0)
#endif
  )
    return FAIL;

  /*
     * There are seven ways to insert lines:
     * 0. When in a vertically split window and t_CV isn't set, redraw the
     *    characters from ScreenLines[].
     * 1. Use T_CD (clear to end of display) if it exists and the result of
     *	  the insert is just empty lines
     * 2. Use T_CAL (insert multiple lines) if it exists and T_AL is not
     *	  present or line_count > 1. It looks better if we do all the inserts
     *	  at once.
     * 3. Use T_CDL (delete multiple lines) if it exists and the result of the
     *	  insert is just empty lines and T_CE is not present or line_count >
     *	  1.
     * 4. Use T_AL (insert line) if it exists.
     * 5. Use T_CE (erase line) if it exists and the result of the insert is
     *	  just empty lines.
     * 6. Use T_DL (delete line) if it exists and the result of the insert is
     *	  just empty lines.
     * 7. Use T_SR (scroll reverse) if it exists and inserting at row 0 and
     *	  the 'da' flag is not set or we have clear line capability.
     * 8. redraw the characters from ScreenLines[].
     *
     * Careful: In a hpterm scroll reverse doesn't work as expected, it moves
     * the scrollbar for the window. It does have insert line, use that if it
     * exists.
     */
  result_empty = (row + line_count >= end);
  if (wp != NULL && wp->w_width != Columns && *T_CSV == NUL)
    type = USE_REDRAW;
  else if (can_clear(T_CD) && result_empty)
    type = USE_T_CD;
  else if (*T_CAL != NUL && (line_count > 1 || *T_AL == NUL))
    type = USE_T_CAL;
  else if (*T_CDL != NUL && result_empty && (line_count > 1 || !can_ce))
    type = USE_T_CDL;
  else if (*T_AL != NUL)
    type = USE_T_AL;
  else if (can_ce && result_empty)
    type = USE_T_CE;
  else if (*T_DL != NUL && result_empty)
    type = USE_T_DL;
  else if (*T_SR != NUL && row == 0 && (*T_DA == NUL || can_ce))
    type = USE_T_SR;
  else
    return FAIL;

  /*
     * For clearing the lines screen_del_lines() is used. This will also take
     * care of t_db if necessary.
     */
  if (type == USE_T_CD || type == USE_T_CDL ||
      type == USE_T_CE || type == USE_T_DL)
    return screen_del_lines(off, row, line_count, end, FALSE, 0, wp);

  /*
     * If text is retained below the screen, first clear or delete as many
     * lines at the bottom of the window as are about to be inserted so that
     * the deleted lines won't later surface during a screen_del_lines.
     */
  if (*T_DB)
    screen_del_lines(off, end - line_count, line_count, end, FALSE, 0, wp);

#ifdef FEAT_CLIPBOARD
  /* Remove a modeless selection when inserting lines halfway the screen
     * or not the full width of the screen. */
  if (off + row > 0 || (wp != NULL && wp->w_width != Columns))
    clip_clear_selection(&clip_star);
  else
    clip_scroll_selection(-line_count);
#endif

#ifdef FEAT_GUI
  /* Don't update the GUI cursor here, ScreenLines[] is invalid until the
     * scrolling is actually carried out. */
  gui_dont_update_cursor(row + off <= gui.cursor_row);
#endif

  if (wp != NULL && wp->w_wincol != 0 && *T_CSV != NUL && *T_CCS == NUL)
    cursor_col = wp->w_wincol;

  if (*T_CCS != NUL) /* cursor relative to region */
    cursor_row = row;
  else
    cursor_row = row + off;

  /*
     * Shift LineOffset[] line_count down to reflect the inserted lines.
     * Clear the inserted lines in ScreenLines[].
     */
  row += off;
  end += off;
  for (i = 0; i < line_count; ++i)
  {
    if (wp != NULL && wp->w_width != Columns)
    {
      /* need to copy part of a line */
      j = end - 1 - i;
      while ((j -= line_count) >= row)
        linecopy(j + line_count, j, wp);
      j += line_count;
      if (can_clear((char_u *)" "))
        lineclear(LineOffset[j] + wp->w_wincol, wp->w_width,
                  clear_attr);
      else
        lineinvalid(LineOffset[j] + wp->w_wincol, wp->w_width);
      LineWraps[j] = FALSE;
    }
    else
    {
      j = end - 1 - i;
      temp = LineOffset[j];
      while ((j -= line_count) >= row)
      {
        LineOffset[j + line_count] = LineOffset[j];
        LineWraps[j + line_count] = LineWraps[j];
      }
      LineOffset[j + line_count] = temp;
      LineWraps[j + line_count] = FALSE;
      if (can_clear((char_u *)" "))
        lineclear(temp, (int)Columns, clear_attr);
      else
        lineinvalid(temp, (int)Columns);
    }
  }

  screen_stop_highlight();
  windgoto(cursor_row, cursor_col);
  if (clear_attr != 0)
    screen_start_highlight(clear_attr);

  /* redraw the characters */
  if (type == USE_REDRAW)
    redraw_block(row, end, wp);
  else if (type == USE_T_CAL)
  {
    term_append_lines(line_count);
    screen_start(); /* don't know where cursor is now */
  }
  else
  {
    for (i = 0; i < line_count; i++)
    {
      if (type == USE_T_AL)
      {
        if (i && cursor_row != 0)
          windgoto(cursor_row, cursor_col);
        out_str(T_AL);
      }
      else /* type == USE_T_SR */
        out_str(T_SR);
      screen_start(); /* don't know where cursor is now */
    }
  }

  /*
     * With scroll-reverse and 'da' flag set we need to clear the lines that
     * have been scrolled down into the region.
     */
  if (type == USE_T_SR && *T_DA)
  {
    for (i = 0; i < line_count; ++i)
    {
      windgoto(off + i, cursor_col);
      out_str(T_CE);
      screen_start(); /* don't know where cursor is now */
    }
  }

#ifdef FEAT_GUI
  gui_can_update_cursor();
#endif
  return OK;
}

/*
 * Delete lines on the screen and update ScreenLines[].
 * "end" is the line after the scrolled part. Normally it is Rows.
 * When scrolling region used "off" is the offset from the top for the region.
 * "row" and "end" are relative to the start of the region.
 *
 * Return OK for success, FAIL if the lines are not deleted.
 */
int screen_del_lines(
    int off,
    int row,
    int line_count,
    int end,
    int force,        /* even when line_count > p_ttyscroll */
    int clear_attr,   /* used for clearing lines */
    win_T *wp UNUSED) /* NULL or window to use width from */
{
  int j;
  int i;
  unsigned temp;
  int cursor_row;
  int cursor_col = 0;
  int cursor_end;
  int result_empty; /* result is empty until end of region */
  int can_delete;   /* deleting line codes can be used */
  int type;

  /*
     * FAIL if
     * - there is no valid screen
     * - the screen has to be redrawn completely
     * - the line count is less than one
     * - the line count is more than 'ttyscroll'
     * - redrawing for a callback and there is a modeless selection
     */
  if (!screen_valid(TRUE) || line_count <= 0 || (!force && line_count > p_ttyscroll)
#ifdef FEAT_CLIPBOARD
      || (clip_star.state != SELECT_CLEARED && redrawing_for_callback > 0)
#endif
  )
    return FAIL;

  /*
     * Check if the rest of the current region will become empty.
     */
  result_empty = row + line_count >= end;

  /*
     * We can delete lines only when 'db' flag not set or when 'ce' option
     * available.
     */
  can_delete = (*T_DB == NUL || can_clear(T_CE));

  /*
     * There are six ways to delete lines:
     * 0. When in a vertically split window and t_CV isn't set, redraw the
     *    characters from ScreenLines[].
     * 1. Use T_CD if it exists and the result is empty.
     * 2. Use newlines if row == 0 and count == 1 or T_CDL does not exist.
     * 3. Use T_CDL (delete multiple lines) if it exists and line_count > 1 or
     *	  none of the other ways work.
     * 4. Use T_CE (erase line) if the result is empty.
     * 5. Use T_DL (delete line) if it exists.
     * 6. redraw the characters from ScreenLines[].
     */
  if (wp != NULL && wp->w_width != Columns && *T_CSV == NUL)
    type = USE_REDRAW;
  else if (can_clear(T_CD) && result_empty)
    type = USE_T_CD;
#if defined(__BEOS__) && defined(BEOS_DR8)
  /*
     * USE_NL does not seem to work in Terminal of DR8 so we set T_DB="" in
     * its internal termcap... this works okay for tests which test *T_DB !=
     * NUL.  It has the disadvantage that the user cannot use any :set t_*
     * command to get T_DB (back) to empty_option, only :set term=... will do
     * the trick...
     * Anyway, this hack will hopefully go away with the next OS release.
     * (Olaf Seibert)
     */
  else if (row == 0 && T_DB == empty_option && (line_count == 1 || *T_CDL == NUL))
#else
  else if (row == 0 && (
                           /* On the Amiga, somehow '\n' on the last line doesn't always scroll
	 * up, so use delete-line command */
                           line_count == 1 ||
                           *T_CDL == NUL))
#endif
    type = USE_NL;
  else if (*T_CDL != NUL && line_count > 1 && can_delete)
    type = USE_T_CDL;
  else if (can_clear(T_CE) && result_empty && (wp == NULL || wp->w_width == Columns))
    type = USE_T_CE;
  else if (*T_DL != NUL && can_delete)
    type = USE_T_DL;
  else if (*T_CDL != NUL && can_delete)
    type = USE_T_CDL;
  else
    return FAIL;

#ifdef FEAT_CLIPBOARD
  /* Remove a modeless selection when deleting lines halfway the screen or
     * not the full width of the screen. */
  if (off + row > 0 || (wp != NULL && wp->w_width != Columns))
    clip_clear_selection(&clip_star);
  else
    clip_scroll_selection(line_count);
#endif

#ifdef FEAT_GUI
  /* Don't update the GUI cursor here, ScreenLines[] is invalid until the
     * scrolling is actually carried out. */
  gui_dont_update_cursor(gui.cursor_row >= row + off && gui.cursor_row < end + off);
#endif

  if (wp != NULL && wp->w_wincol != 0 && *T_CSV != NUL && *T_CCS == NUL)
    cursor_col = wp->w_wincol;

  if (*T_CCS != NUL) /* cursor relative to region */
  {
    cursor_row = row;
    cursor_end = end;
  }
  else
  {
    cursor_row = row + off;
    cursor_end = end + off;
  }

  /*
     * Now shift LineOffset[] line_count up to reflect the deleted lines.
     * Clear the inserted lines in ScreenLines[].
     */
  row += off;
  end += off;
  for (i = 0; i < line_count; ++i)
  {
    if (wp != NULL && wp->w_width != Columns)
    {
      /* need to copy part of a line */
      j = row + i;
      while ((j += line_count) <= end - 1)
        linecopy(j - line_count, j, wp);
      j -= line_count;
      if (can_clear((char_u *)" "))
        lineclear(LineOffset[j] + wp->w_wincol, wp->w_width,
                  clear_attr);
      else
        lineinvalid(LineOffset[j] + wp->w_wincol, wp->w_width);
      LineWraps[j] = FALSE;
    }
    else
    {
      /* whole width, moving the line pointers is faster */
      j = row + i;
      temp = LineOffset[j];
      while ((j += line_count) <= end - 1)
      {
        LineOffset[j - line_count] = LineOffset[j];
        LineWraps[j - line_count] = LineWraps[j];
      }
      LineOffset[j - line_count] = temp;
      LineWraps[j - line_count] = FALSE;
      if (can_clear((char_u *)" "))
        lineclear(temp, (int)Columns, clear_attr);
      else
        lineinvalid(temp, (int)Columns);
    }
  }

  if (screen_attr != clear_attr)
    screen_stop_highlight();
  if (clear_attr != 0)
    screen_start_highlight(clear_attr);

  /* redraw the characters */
  if (type == USE_REDRAW)
    redraw_block(row, end, wp);
  else if (type == USE_T_CD) /* delete the lines */
  {
    windgoto(cursor_row, cursor_col);
    out_str(T_CD);
    screen_start(); /* don't know where cursor is now */
  }
  else if (type == USE_T_CDL)
  {
    windgoto(cursor_row, cursor_col);
    term_delete_lines(line_count);
    screen_start(); /* don't know where cursor is now */
  }
  /*
     * Deleting lines at top of the screen or scroll region: Just scroll
     * the whole screen (scroll region) up by outputting newlines on the
     * last line.
     */
  else if (type == USE_NL)
  {
    windgoto(cursor_end - 1, cursor_col);
  }
  else
  {
    for (i = line_count; --i >= 0;)
    {
      if (type == USE_T_DL)
      {
        windgoto(cursor_row, cursor_col);
        out_str(T_DL); /* delete a line */
      }
      else /* type == USE_T_CE */
      {
        windgoto(cursor_row + i, cursor_col);
        out_str(T_CE); /* erase a line */
      }
      screen_start(); /* don't know where cursor is now */
    }
  }

  /*
     * If the 'db' flag is set, we need to clear the lines that have been
     * scrolled up at the bottom of the region.
     */
  if (*T_DB && (type == USE_T_DL || type == USE_T_CDL))
  {
    for (i = line_count; i > 0; --i)
    {
      windgoto(cursor_end - i, cursor_col);
      out_str(T_CE);  /* erase a line */
      screen_start(); /* don't know where cursor is now */
    }
  }

#ifdef FEAT_GUI
  gui_can_update_cursor();
#endif

  return OK;
}

/*
 * Return TRUE when postponing displaying the mode message: when not redrawing
 * or inside a mapping.
 */
int skip_showmode()
{
  // Call char_avail() only when we are going to show something, because it
  // takes a bit of time.  redrawing() may also call char_avail_avail().
  if (global_busy || msg_silent != 0 || !redrawing() || (char_avail() && !KeyTyped))
  {
    redraw_mode = TRUE; // show mode later
    return TRUE;
  }
  return FALSE;
}

/*
 * Show the current mode and ruler.
 *
 * If clear_cmdline is TRUE, clear the rest of the cmdline.
 * If clear_cmdline is FALSE there may be a message there that needs to be
 * cleared only if a mode is shown.
 * If redraw_mode is TRUE show or clear the mode.
 * Return the length of the message (0 if no message).
 */
int showmode(void)
{
  int need_clear;
  int length = 0;
  int do_mode;
  int attr;
  int nwr_save;

  do_mode = ((p_smd && msg_silent == 0) && ((State & INSERT) || restart_edit != NUL || VIsual_active));
  if (do_mode || reg_recording != 0)
  {
    if (skip_showmode())
      return 0; // show mode later

    nwr_save = need_wait_return;

    /* wait a bit before overwriting an important message */
    check_for_delay(FALSE);

    /* if the cmdline is more than one line high, erase top lines */
    need_clear = clear_cmdline;
    if (clear_cmdline && cmdline_row < Rows - 1)
      msg_clr_cmdline(); /* will reset clear_cmdline */

    /* Position on the last line in the window, column 0 */
    msg_pos_mode();
    cursor_off();
    attr = HL_ATTR(HLF_CM); /* Highlight mode */
    if (do_mode)
    {
      msg_puts_attr("--", attr);
#if defined(FEAT_XIM)
      if (
#ifdef FEAT_GUI_GTK
          preedit_get_status()
#else
          im_get_status()
#endif
      )
#ifdef FEAT_GUI_GTK /* most of the time, it's not XIM being used */
        msg_puts_attr(" IM", attr);
#else
        msg_puts_attr(" XIM", attr);
#endif
#endif
#if defined(FEAT_HANGULIN) && defined(FEAT_GUI)
      if (gui.in_use)
      {
        if (hangul_input_state_get())
        {
          /* HANGUL */
          if (enc_utf8)
            msg_puts_attr(" \355\225\234\352\270\200", attr);
          else
            msg_puts_attr(" \307\321\261\333", attr);
        }
      }
#endif
      {
        if (State & VREPLACE_FLAG)
          msg_puts_attr(_(" VREPLACE"), attr);
        else if (State & REPLACE_FLAG)
          msg_puts_attr(_(" REPLACE"), attr);
        else if (State & INSERT)
        {
#ifdef FEAT_RIGHTLEFT
          if (p_ri)
            msg_puts_attr(_(" REVERSE"), attr);
#endif
          msg_puts_attr(_(" INSERT"), attr);
        }
        else if (restart_edit == 'I' || restart_edit == 'A')
          msg_puts_attr(_(" (insert)"), attr);
        else if (restart_edit == 'R')
          msg_puts_attr(_(" (replace)"), attr);
        else if (restart_edit == 'V')
          msg_puts_attr(_(" (vreplace)"), attr);
#ifdef FEAT_RIGHTLEFT
        if (p_hkmap)
          msg_puts_attr(_(" Hebrew"), attr);
#endif
#ifdef FEAT_KEYMAP
        if (State & LANGMAP)
        {
#ifdef FEAT_ARABIC
          if (curwin->w_p_arab)
            msg_puts_attr(_(" Arabic"), attr);
          else
#endif
              if (get_keymap_str(curwin, (char_u *)" (%s)",
                                 NameBuff, MAXPATHL))
            msg_puts_attr((char *)NameBuff, attr);
        }
#endif
        if ((State & INSERT) && p_paste)
          msg_puts_attr(_(" (paste)"), attr);

        if (VIsual_active)
        {
          char *p;

          /* Don't concatenate separate words to avoid translation
		     * problems. */
          switch ((VIsual_select ? 4 : 0) + (VIsual_mode == Ctrl_V) * 2 + (VIsual_mode == 'V'))
          {
          case 0:
            p = N_(" VISUAL");
            break;
          case 1:
            p = N_(" VISUAL LINE");
            break;
          case 2:
            p = N_(" VISUAL BLOCK");
            break;
          case 4:
            p = N_(" SELECT");
            break;
          case 5:
            p = N_(" SELECT LINE");
            break;
          default:
            p = N_(" SELECT BLOCK");
            break;
          }
          msg_puts_attr(_(p), attr);
        }
        msg_puts_attr(" --", attr);
      }

      need_clear = TRUE;
    }
    if (reg_recording != 0)
    {
      recording_mode(attr);
      need_clear = TRUE;
    }

    mode_displayed = TRUE;
    if (need_clear || clear_cmdline || redraw_mode)
      msg_clr_eos();
    msg_didout = FALSE; /* overwrite this message */
    length = msg_col;
    msg_col = 0;
    need_wait_return = nwr_save; /* never ask for hit-return for this */
  }
  else if (clear_cmdline && msg_silent == 0)
    /* Clear the whole command line.  Will reset "clear_cmdline". */
    msg_clr_cmdline();
  else if (redraw_mode)
  {
    msg_pos_mode();
    msg_clr_eos();
  }

  redraw_cmdline = FALSE;
  redraw_mode = FALSE;
  clear_cmdline = FALSE;

  return length;
}

/*
 * Position for a mode message.
 */
static void
msg_pos_mode(void)
{
  msg_col = 0;
  msg_row = Rows - 1;
}

/*
 * Delete mode message.  Used when ESC is typed which is expected to end
 * Insert mode (but Insert mode didn't end yet!).
 * Caller should check "mode_displayed".
 */
void unshowmode(int force)
{
  /*
     * Don't delete it right now, when not redrawing or inside a mapping.
     */
  if (!redrawing() || (!force && char_avail() && !KeyTyped))
    redraw_cmdline = TRUE; /* delete mode later */
  else
    clearmode();
}

/*
 * Clear the mode message.
 */
void clearmode(void)
{
  int save_msg_row = msg_row;
  int save_msg_col = msg_col;

  msg_pos_mode();
  if (reg_recording != 0)
    recording_mode(HL_ATTR(HLF_CM));
  msg_clr_eos();

  msg_col = save_msg_col;
  msg_row = save_msg_row;
}

static void
recording_mode(int attr)
{
  msg_puts_attr(_("recording"), attr);
  if (!shortmess(SHM_RECORDING))
  {
    char s[4];

    sprintf(s, " @%c", reg_recording);
    msg_puts_attr(s, attr);
  }
}

/*
 * Draw the tab pages line at the top of the Vim window.
 */
void draw_tabline(void)
{
  int tabcount = 0;
  tabpage_T *tp;
  int tabwidth;
  int col = 0;
  int scol = 0;
  int attr;
  win_T *wp;
  win_T *cwp;
  int wincount;
  int modified;
  int c;
  int len;
  int attr_sel = HL_ATTR(HLF_TPS);
  int attr_nosel = HL_ATTR(HLF_TP);
  int attr_fill = HL_ATTR(HLF_TPF);
  char_u *p;
  int room;
  int use_sep_chars = (t_colors < 8
#ifdef FEAT_GUI
                       && !gui.in_use
#endif
  );

  if (ScreenLines == NULL)
    return;
  redraw_tabline = FALSE;

  if (tabline_height() < 1)
    return;

  {
    FOR_ALL_TABPAGES(tp)
    ++tabcount;

    tabwidth = (Columns - 1 + tabcount / 2) / tabcount;
    if (tabwidth < 6)
      tabwidth = 6;

    attr = attr_nosel;
    tabcount = 0;
    for (tp = first_tabpage; tp != NULL && col < Columns - 4;
         tp = tp->tp_next)
    {
      scol = col;

      if (tp->tp_topframe == topframe)
        attr = attr_sel;
      if (use_sep_chars && col > 0)
        screen_putchar('|', 0, col++, attr);

      if (tp->tp_topframe != topframe)
        attr = attr_nosel;

      screen_putchar(' ', 0, col++, attr);

      if (tp == curtab)
      {
        cwp = curwin;
        wp = firstwin;
      }
      else
      {
        cwp = tp->tp_curwin;
        wp = tp->tp_firstwin;
      }

      modified = FALSE;
      for (wincount = 0; wp != NULL; wp = wp->w_next, ++wincount)
        if (bufIsChanged(wp->w_buffer))
          modified = TRUE;
      if (modified || wincount > 1)
      {
        if (wincount > 1)
        {
          vim_snprintf((char *)NameBuff, MAXPATHL, "%d", wincount);
          len = (int)STRLEN(NameBuff);
          if (col + len >= Columns - 3)
            break;
          screen_puts_len(NameBuff, len, 0, col,
                          attr);
          col += len;
        }
        if (modified)
          screen_puts_len((char_u *)"+", 1, 0, col++, attr);
        screen_putchar(' ', 0, col++, attr);
      }

      room = scol - col + tabwidth - 1;
      if (room > 0)
      {
        /* Get buffer name in NameBuff[] */
        get_trans_bufname(cwp->w_buffer);
        shorten_dir(NameBuff);
        len = vim_strsize(NameBuff);
        p = NameBuff;
        if (has_mbyte)
          while (len > room)
          {
            len -= ptr2cells(p);
            MB_PTR_ADV(p);
          }
        else if (len > room)
        {
          p += len - room;
          len = room;
        }
        if (len > Columns - col - 1)
          len = Columns - col - 1;

        screen_puts_len(p, (int)STRLEN(p), 0, col, attr);
        col += len;
      }
      screen_putchar(' ', 0, col++, attr);

      /* Store the tab page number in TabPageIdxs[], so that
	     * jump_to_mouse() knows where each one is. */
      ++tabcount;
      while (scol < col)
        TabPageIdxs[scol++] = tabcount;
    }

    if (use_sep_chars)
      c = '_';
    else
      c = ' ';
    screen_fill(0, 1, col, (int)Columns, c, c, attr_fill);

    /* Put an "X" for closing the current tab if there are several. */
    if (first_tabpage->tp_next != NULL)
    {
      screen_putchar('X', 0, (int)Columns - 1, attr_nosel);
      TabPageIdxs[Columns - 1] = -999;
    }
  }

  /* Reset the flag here again, in case evaluating 'tabline' causes it to be
     * set. */
  redraw_tabline = FALSE;
}

/*
 * Get buffer name for "buf" into NameBuff[].
 * Takes care of special buffer names and translates special characters.
 */
void get_trans_bufname(buf_T *buf)
{
  if (buf_spname(buf) != NULL)
    vim_strncpy(NameBuff, buf_spname(buf), MAXPATHL - 1);
  else
    home_replace(buf, buf->b_fname, NameBuff, MAXPATHL, TRUE);
  trans_characters(NameBuff, MAXPATHL);
}

/*
 * Get the character to use in a status line.  Get its attributes in "*attr".
 */
static int
fillchar_status(int *attr, win_T *wp)
{
  int fill;

#ifdef FEAT_TERMINAL
  if (bt_terminal(wp->w_buffer))
  {
    if (wp == curwin)
    {
      *attr = HL_ATTR(HLF_ST);
      fill = fill_stl;
    }
    else
    {
      *attr = HL_ATTR(HLF_STNC);
      fill = fill_stlnc;
    }
  }
  else
#endif
      if (wp == curwin)
  {
    *attr = HL_ATTR(HLF_S);
    fill = fill_stl;
  }
  else
  {
    *attr = HL_ATTR(HLF_SNC);
    fill = fill_stlnc;
  }
  /* Use fill when there is highlighting, and highlighting of current
     * window differs, or the fillchars differ, or this is not the
     * current window */
  if (*attr != 0 && ((HL_ATTR(HLF_S) != HL_ATTR(HLF_SNC) || wp != curwin || ONE_WINDOW) || (fill_stl != fill_stlnc)))
    return fill;
  if (wp == curwin)
    return '^';
  return '=';
}

/*
 * Get the character to use in a separator between vertically split windows.
 * Get its attributes in "*attr".
 */
static int
fillchar_vsep(int *attr)
{
  *attr = HL_ATTR(HLF_C);
  if (*attr == 0 && fill_vert == ' ')
    return '|';
  else
    return fill_vert;
}

/*
 * Return TRUE if redrawing should currently be done.
 */
int redrawing(void)
{
#ifdef FEAT_EVAL
  if (disable_redraw_for_testing)
    return 0;
  else
#endif
    return ((!RedrawingDisabled
#ifdef FEAT_EVAL
             || ignore_redraw_flag_for_testing
#endif
             ) &&
            !(p_lz && char_avail() && !KeyTyped && !do_redraw));
}

/*
 * Return TRUE if printing messages should currently be done.
 */
int messaging(void)
{
  return (!(p_lz && char_avail() && !KeyTyped));
}

/*
 * Show current status info in ruler and various other places
 * If always is FALSE, only show ruler if position has changed.
 */
void showruler(int always)
{
  if (!always && !redrawing())
    return;

  /* Redraw the tab pages line if needed. */
  if (redraw_tabline)
    draw_tabline();
}

#if defined(FEAT_LINEBREAK) || defined(PROTO)
/*
 * Return the width of the 'number' and 'relativenumber' column.
 * Caller may need to check if 'number' or 'relativenumber' is set.
 * Otherwise it depends on 'numberwidth' and the line count.
 */
int number_width(win_T *wp)
{
  int n;
  linenr_T lnum;

  if (wp->w_p_rnu && !wp->w_p_nu)
    /* cursor line shows "0" */
    lnum = wp->w_height;
  else
    /* cursor line shows absolute line number */
    lnum = wp->w_buffer->b_ml.ml_line_count;

  if (lnum == wp->w_nrwidth_line_count && wp->w_nuw_cached == wp->w_p_nuw)
    return wp->w_nrwidth_width;
  wp->w_nrwidth_line_count = lnum;

  n = 0;
  do
  {
    lnum /= 10;
    ++n;
  } while (lnum > 0);

  /* 'numberwidth' gives the minimal width plus one */
  if (n < wp->w_p_nuw - 1)
    n = wp->w_p_nuw - 1;

  wp->w_nrwidth_width = n;
  wp->w_nuw_cached = wp->w_p_nuw;
  return n;
}
#endif

#if defined(FEAT_EVAL) || defined(PROTO)
/*
 * Return the current cursor column. This is the actual position on the
 * screen. First column is 0.
 */
int screen_screencol(void)
{
  return screen_cur_col;
}

/*
 * Return the current cursor row. This is the actual position on the screen.
 * First row is 0.
 */
int screen_screenrow(void)
{
  return screen_cur_row;
}
#endif
