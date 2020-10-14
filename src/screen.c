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

/* Flag that is set when drawing for a callback, not from the main command
 * loop. */
static int redrawing_for_callback = 0;

/*
 * Buffer for one screen line (characters and attributes).
 */
static schar_T *current_ScreenLine;

static void win_redr_status(win_T *wp, int ignore_pum);
#ifdef FEAT_FOLDING
// libvim - removed
#endif
#ifdef FEAT_SEARCH_EXTRA
#define SEARCH_HL_PRIORITY 0
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
int update_screen(int type_arg UNUSED)
{

  // libvim: no-op - drawing handled by client
  return OK;
}

#if defined(PROTO)
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

  /* When the screen was cleared redraw the tab pages line. */
  if (redraw_tabline)
    draw_tabline();

  if (wp->w_redr_status)
    win_redr_status(wp, FALSE);

  update_finish();
}
#endif

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
    int row UNUSED,
    int coloff UNUSED,
    int endcol UNUSED,
    int clear_width UNUSED,
    int flags UNUSED)
{
  // libvim: noop
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
  if (has_mbyte && col > 0 && col < screen_Columns && mb_fix_col(col, row) != col)
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
#ifdef UNIX
      /* The bold trick makes a single row of pixels appear in the next
	     * character.  When a bold character is removed, the next
	     * character should be redrawn too.  This happens for our own GUI
	     * and for some xterms. */
      if (need_redraw && ScreenLines[off] != ' ' && (term_is_xterm))
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
      if (*p_ambw == 'd')
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
  screen_Rows = Rows;
  screen_Columns = Columns;

  must_redraw = CLEAR; /* need to clear the screen later */
  if (doclear)
    screenclear2();
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

  if (starting == NO_SCREEN || ScreenLines == NULL)
    return;

  screen_attr = -1;        /* force setting the Normal colors */
  screen_stop_highlight(); /* don't want highlighting here */

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
  return (*p != NUL && (t_colors <= 1 || cterm_normal_bg_color == 0 || *T_UT != NUL));
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
  if (!screen_valid(TRUE) || line_count <= 0 || line_count > p_ttyscroll)
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
  if (!screen_valid(TRUE) || line_count <= 0 || (!force && line_count > p_ttyscroll))
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

  return OK;
}

/*
 * Return TRUE when postponing displaying the mode message: when not redrawing
 * or inside a mapping.
 */
int skip_showmode()
{
  return TRUE;
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
  int len;
  int attr_sel = HL_ATTR(HLF_TPS);
  int attr_nosel = HL_ATTR(HLF_TP);
  char_u *p;
  int room;
  int use_sep_chars = t_colors < 8;

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
