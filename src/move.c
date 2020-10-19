/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * move.c: Functions for moving the cursor and scrolling text.
 *
 * There are two ways to move the cursor:
 * 1. Move the cursor directly, the text is scrolled to keep the cursor in the
 *    window.
 * 2. Scroll the text, the cursor is moved into the text visible in the
 *    window.
 * The 'scrolloff' option makes this a bit complicated.
 */

#include "vim.h"

static void curs_rows(win_T *wp);

typedef struct
{
  linenr_T lnum; /* line number */
#ifdef FEAT_DIFF
  int fill; /* filler lines */
#endif
  int height; /* height of added line */
} lineoff_T;

/*
 * Compute wp->w_botline for the current wp->w_topline.  Can be called after
 * wp->w_topline changed.
 */
static void
comp_botline(win_T *wp)
{
  int n;
  linenr_T lnum;
  int done;
#ifdef FEAT_FOLDING
  linenr_T last;
  int folded;
#endif

  /*
     * If w_cline_row is valid, start there.
     * Otherwise have to start at w_topline.
     */
  check_cursor_moved(wp);
  if (wp->w_valid & VALID_CROW)
  {
    lnum = wp->w_cursor.lnum;
    done = wp->w_cline_row;
  }
  else
  {
    lnum = wp->w_topline;
    done = 0;
  }

  for (; lnum <= wp->w_buffer->b_ml.ml_line_count; ++lnum)
  {
#ifdef FEAT_FOLDING
    last = lnum;
    folded = FALSE;
    if (hasFoldingWin(wp, lnum, NULL, &last, TRUE, NULL))
    {
      n = 1;
      folded = TRUE;
    }
    else
#endif
#ifdef FEAT_DIFF
        if (lnum == wp->w_topline)
      n = plines_win_nofill(wp, lnum, TRUE) + wp->w_topfill;
    else
#endif
      n = plines_win(wp, lnum, TRUE);
    if (
#ifdef FEAT_FOLDING
        lnum <= wp->w_cursor.lnum && last >= wp->w_cursor.lnum
#else
        lnum == wp->w_cursor.lnum
#endif
    )
    {
      wp->w_cline_row = done;
      wp->w_cline_height = n;
#ifdef FEAT_FOLDING
      wp->w_cline_folded = folded;
#endif
      redraw_for_cursorline(wp);
      wp->w_valid |= (VALID_CROW | VALID_CHEIGHT);
    }
    if (done + n > wp->w_height)
      break;
    done += n;
#ifdef FEAT_FOLDING
    lnum = last;
#endif
  }

  /* wp->w_botline is the line that is just below the window */
  wp->w_botline = lnum;
  wp->w_valid |= VALID_BOTLINE | VALID_BOTLINE_AP;

  set_empty_rows(wp, done);
}

/*
 * Redraw when w_cline_row changes and 'relativenumber' or 'cursorline' is
 * set.
 */
void redraw_for_cursorline(win_T *wp)
{
  if ((wp->w_p_rnu) && (wp->w_valid & VALID_CROW) == 0)
  {
    if (wp->w_p_rnu)
      // win_line() will redraw the number column only.
      redraw_win_later(wp, VALID);
  }
}

/*
 * Update curwin->w_topline and redraw if necessary.
 * Used to update the screen before printing a message.
 */
void update_topline_redraw(void)
{
  update_topline();
  if (must_redraw)
    update_screen(0);
}

/*
 * Update curwin->w_topline to move the cursor onto the screen.
 */
void update_topline(void)
{
  // libvim: No-op
}

void update_curswant(void)
{
  if (curwin->w_set_curswant)
  {
    validate_virtcol();
    curwin->w_curswant = curwin->w_virtcol;
    curwin->w_set_curswant = FALSE;
  }
}

/*
 * Check if the cursor has moved.  Set the w_valid flag accordingly.
 */
void check_cursor_moved(win_T *wp)
{
  if (wp->w_cursor.lnum != wp->w_valid_cursor.lnum)
  {
    wp->w_valid &= ~(VALID_WROW | VALID_WCOL | VALID_VIRTCOL | VALID_CHEIGHT | VALID_CROW | VALID_TOPLINE);
    wp->w_valid_cursor = wp->w_cursor;
    wp->w_valid_leftcol = wp->w_leftcol;
  }
  else if (wp->w_cursor.col != wp->w_valid_cursor.col || wp->w_leftcol != wp->w_valid_leftcol || wp->w_cursor.coladd != wp->w_valid_cursor.coladd)
  {
    wp->w_valid &= ~(VALID_WROW | VALID_WCOL | VALID_VIRTCOL);
    wp->w_valid_cursor.col = wp->w_cursor.col;
    wp->w_valid_leftcol = wp->w_leftcol;
    wp->w_valid_cursor.coladd = wp->w_cursor.coladd;
  }
}

/*
 * Call this function when some window settings have changed, which require
 * the cursor position, botline and topline to be recomputed and the window to
 * be redrawn.  E.g, when changing the 'wrap' option or folding.
 */
void changed_window_setting(void)
{
  changed_window_setting_win(curwin);
}

void changed_window_setting_win(win_T *wp)
{
  wp->w_lines_valid = 0;
  changed_line_abv_curs_win(wp);
  wp->w_valid &= ~(VALID_BOTLINE | VALID_BOTLINE_AP | VALID_TOPLINE);
  redraw_win_later(wp, NOT_VALID);
}

/*
 * Set wp->w_topline to a certain number.
 */
void set_topline(win_T *wp, linenr_T lnum)
{
#ifdef FEAT_FOLDING
  /* go to first of folded lines */
  (void)hasFoldingWin(wp, lnum, &lnum, NULL, TRUE, NULL);
#endif
  /* Approximate the value of w_botline */
  wp->w_botline += lnum - wp->w_topline;
  wp->w_topline = lnum;
  wp->w_topline_was_set = TRUE;
#ifdef FEAT_DIFF
  wp->w_topfill = 0;
#endif
  wp->w_valid &= ~(VALID_WROW | VALID_CROW | VALID_BOTLINE | VALID_TOPLINE);
  /* Don't set VALID_TOPLINE here, 'scrolloff' needs to be checked. */
  redraw_later(VALID);
}

/*
 * Call this function when the length of the cursor line (in screen
 * characters) has changed, and the change is before the cursor.
 * Need to take care of w_botline separately!
 */
void changed_cline_bef_curs(void)
{
  curwin->w_valid &= ~(VALID_WROW | VALID_WCOL | VALID_VIRTCOL | VALID_CHEIGHT | VALID_TOPLINE);
}

void changed_cline_bef_curs_win(win_T *wp)
{
  wp->w_valid &= ~(VALID_WROW | VALID_WCOL | VALID_VIRTCOL | VALID_CHEIGHT | VALID_TOPLINE);
}

/*
 * Call this function when the length of a line (in screen characters) above
 * the cursor have changed.
 * Need to take care of w_botline separately!
 */
void changed_line_abv_curs(void)
{
  curwin->w_valid &= ~(VALID_WROW | VALID_WCOL | VALID_VIRTCOL | VALID_CROW | VALID_CHEIGHT | VALID_TOPLINE);
}

void changed_line_abv_curs_win(win_T *wp)
{
  wp->w_valid &= ~(VALID_WROW | VALID_WCOL | VALID_VIRTCOL | VALID_CROW | VALID_CHEIGHT | VALID_TOPLINE);
}

/*
 * Make sure the value of curwin->w_botline is valid.
 */
void validate_botline(void)
{
  if (!(curwin->w_valid & VALID_BOTLINE))
    comp_botline(curwin);
}

/*
 * Mark curwin->w_botline as invalid (because of some change in the buffer).
 */
void invalidate_botline(void)
{
  curwin->w_valid &= ~(VALID_BOTLINE | VALID_BOTLINE_AP);
}

void invalidate_botline_win(win_T *wp)
{
  wp->w_valid &= ~(VALID_BOTLINE | VALID_BOTLINE_AP);
}

void approximate_botline_win(
    win_T *wp)
{
  wp->w_valid &= ~VALID_BOTLINE;
}

/*
 * Return TRUE if curwin->w_wrow and curwin->w_wcol are valid.
 */
int cursor_valid(void)
{
  check_cursor_moved(curwin);
  return ((curwin->w_valid & (VALID_WROW | VALID_WCOL)) ==
          (VALID_WROW | VALID_WCOL));
}

/*
 * Validate cursor position.  Makes sure w_wrow and w_wcol are valid.
 * w_topline must be valid, you may need to call update_topline() first!
 */
void validate_cursor(void)
{
  check_cursor_moved(curwin);
  if ((curwin->w_valid & (VALID_WCOL | VALID_WROW)) != (VALID_WCOL | VALID_WROW))
    curs_columns(TRUE);
}

#if defined(PROTO)
/*
 * validate w_cline_row.
 */
void validate_cline_row(void)
{
  /*
     * First make sure that w_topline is valid (after moving the cursor).
     */
  update_topline();
  check_cursor_moved(curwin);
  if (!(curwin->w_valid & VALID_CROW))
    curs_rows(curwin);
}
#endif

/*
 * Compute wp->w_cline_row and wp->w_cline_height, based on the current value
 * of wp->w_topline.
 */
static void
curs_rows(win_T *wp)
{
  linenr_T lnum;
  int i;
  int all_invalid;
  int valid;
#ifdef FEAT_FOLDING
  long fold_count;
#endif

  /* Check if wp->w_lines[].wl_size is invalid */
  all_invalid = (!redrawing() || wp->w_lines_valid == 0 || wp->w_lines[0].wl_lnum > wp->w_topline);
  i = 0;
  wp->w_cline_row = 0;
  for (lnum = wp->w_topline; lnum < wp->w_cursor.lnum; ++i)
  {
    valid = FALSE;
    if (!all_invalid && i < wp->w_lines_valid)
    {
      if (wp->w_lines[i].wl_lnum < lnum || !wp->w_lines[i].wl_valid)
        continue; /* skip changed or deleted lines */
      if (wp->w_lines[i].wl_lnum == lnum)
      {
#ifdef FEAT_FOLDING
        /* Check for newly inserted lines below this row, in which
		 * case we need to check for folded lines. */
        if (!wp->w_buffer->b_mod_set || wp->w_lines[i].wl_lastlnum < wp->w_cursor.lnum || wp->w_buffer->b_mod_top > wp->w_lines[i].wl_lastlnum + 1)
#endif
          valid = TRUE;
      }
      else if (wp->w_lines[i].wl_lnum > lnum)
        --i; /* hold at inserted lines */
    }
    if (valid
#ifdef FEAT_DIFF
        && (lnum != wp->w_topline || !wp->w_p_diff)
#endif
    )
    {
#ifdef FEAT_FOLDING
      lnum = wp->w_lines[i].wl_lastlnum + 1;
      /* Cursor inside folded lines, don't count this row */
      if (lnum > wp->w_cursor.lnum)
        break;
#else
      ++lnum;
#endif
      wp->w_cline_row += wp->w_lines[i].wl_size;
    }
    else
    {
#ifdef FEAT_FOLDING
      fold_count = foldedCount(wp, lnum, NULL);
      if (fold_count)
      {
        lnum += fold_count;
        if (lnum > wp->w_cursor.lnum)
          break;
        ++wp->w_cline_row;
      }
      else
#endif
#ifdef FEAT_DIFF
          if (lnum == wp->w_topline)
        wp->w_cline_row += plines_win_nofill(wp, lnum++, TRUE) + wp->w_topfill;
      else
#endif
        wp->w_cline_row += plines_win(wp, lnum++, TRUE);
    }
  }

  check_cursor_moved(wp);
  if (!(wp->w_valid & VALID_CHEIGHT))
  {
    if (all_invalid || i == wp->w_lines_valid || (i < wp->w_lines_valid && (!wp->w_lines[i].wl_valid || wp->w_lines[i].wl_lnum != wp->w_cursor.lnum)))
    {
#ifdef FEAT_DIFF
      if (wp->w_cursor.lnum == wp->w_topline)
        wp->w_cline_height = plines_win_nofill(wp, wp->w_cursor.lnum,
                                               TRUE) +
                             wp->w_topfill;
      else
#endif
        wp->w_cline_height = plines_win(wp, wp->w_cursor.lnum, TRUE);
#ifdef FEAT_FOLDING
      wp->w_cline_folded = hasFoldingWin(wp, wp->w_cursor.lnum,
                                         NULL, NULL, TRUE, NULL);
#endif
    }
    else if (i > wp->w_lines_valid)
    {
      /* a line that is too long to fit on the last screen line */
      wp->w_cline_height = 0;
#ifdef FEAT_FOLDING
      wp->w_cline_folded = hasFoldingWin(wp, wp->w_cursor.lnum,
                                         NULL, NULL, TRUE, NULL);
#endif
    }
    else
    {
      wp->w_cline_height = wp->w_lines[i].wl_size;
#ifdef FEAT_FOLDING
      wp->w_cline_folded = wp->w_lines[i].wl_folded;
#endif
    }
  }

  redraw_for_cursorline(curwin);
  wp->w_valid |= VALID_CROW | VALID_CHEIGHT;
}

/*
 * Validate curwin->w_virtcol only.
 */
void validate_virtcol(void)
{
  validate_virtcol_win(curwin);
}

/*
 * Validate wp->w_virtcol only.
 */
void validate_virtcol_win(win_T *wp)
{
  check_cursor_moved(wp);
  if (!(wp->w_valid & VALID_VIRTCOL))
  {
    getvvcol(wp, &wp->w_cursor, NULL, &(wp->w_virtcol), NULL);
    wp->w_valid |= VALID_VIRTCOL;
  }
}

/*
 * Validate w_wcol and w_virtcol only.
 */
void validate_cursor_col(void)
{
  colnr_T off;
  colnr_T col;
  int width;

  validate_virtcol();
  if (!(curwin->w_valid & VALID_WCOL))
  {
    col = curwin->w_virtcol;
    off = curwin_col_off();
    col += off;
    width = curwin->w_width - off + curwin_col_off2();

    /* long line wrapping, adjust curwin->w_wrow */
    if (curwin->w_p_wrap && col >= (colnr_T)curwin->w_width && width > 0)
      /* use same formula as what is used in curs_columns() */
      col -= ((col - curwin->w_width) / width + 1) * width;
    if (col > (int)curwin->w_leftcol)
      col -= curwin->w_leftcol;
    else
      col = 0;
    curwin->w_wcol = col;

    curwin->w_valid |= VALID_WCOL;
  }
}

/*
 * Compute offset of a window, occupied by absolute or relative line number,
 * fold column and sign column (these don't move when scrolling horizontally).
 */
int win_col_off(win_T *wp)
{
  return (((wp->w_p_nu || wp->w_p_rnu) ? number_width(wp) + 1 : 0)
#ifdef FEAT_FOLDING
          + wp->w_p_fdc
#endif
#ifdef FEAT_SIGNS
          + (signcolumn_on(wp) ? 2 : 0)
#endif
  );
}

int curwin_col_off(void)
{
  return win_col_off(curwin);
}

/*
 * Return the difference in column offset for the second screen line of a
 * wrapped line.  It's 8 if 'number' or 'relativenumber' is on and 'n' is in
 * 'cpoptions'.
 */
int win_col_off2(win_T *wp)
{
  if ((wp->w_p_nu || wp->w_p_rnu) && vim_strchr(p_cpo, CPO_NUMCOL) != NULL)
    return number_width(wp) + 1;
  return 0;
}

int curwin_col_off2(void)
{
  return win_col_off2(curwin);
}

/*
 * Compute curwin->w_wcol and curwin->w_virtcol.
 * Also updates curwin->w_wrow and curwin->w_cline_row.
 * Also updates curwin->w_leftcol.
 */
void curs_columns(
    int may_scroll) /* when TRUE, may scroll horizontally */
{
  int diff;
  int extra; /* offset for first screen line */
  int off_left, off_right;
  int n;
  int p_lines;
  int width = 0;
  int textwidth;
  int new_leftcol;
  colnr_T startcol;
  colnr_T endcol;
  colnr_T prev_skipcol;
  long so = get_scrolloff_value();
  long siso = get_sidescrolloff_value();

  /*
     * First make sure that w_topline is valid (after moving the cursor).
     */
  update_topline();

  /*
     * Next make sure that w_cline_row is valid.
     */
  if (!(curwin->w_valid & VALID_CROW))
    curs_rows(curwin);

    /*
     * Compute the number of virtual columns.
     */
#ifdef FEAT_FOLDING
  if (curwin->w_cline_folded)
    /* In a folded line the cursor is always in the first column */
    startcol = curwin->w_virtcol = endcol = curwin->w_leftcol;
  else
#endif
    getvvcol(curwin, &curwin->w_cursor,
             &startcol, &(curwin->w_virtcol), &endcol);

  /* remove '$' from change command when cursor moves onto it */
  if (startcol > dollar_vcol)
    dollar_vcol = -1;

  extra = curwin_col_off();
  curwin->w_wcol = curwin->w_virtcol + extra;
  endcol += extra;

  /*
     * Now compute w_wrow, counting screen lines from w_cline_row.
     */
  curwin->w_wrow = curwin->w_cline_row;

  textwidth = curwin->w_width - extra;
  if (textwidth <= 0)
  {
    /* No room for text, put cursor in last char of window. */
    curwin->w_wcol = curwin->w_width - 1;
    curwin->w_wrow = curwin->w_height - 1;
  }
  else if (curwin->w_p_wrap && curwin->w_width != 0)
  {
    width = textwidth + curwin_col_off2();

    /* long line wrapping, adjust curwin->w_wrow */
    if (curwin->w_wcol >= curwin->w_width)
    {
      /* this same formula is used in validate_cursor_col() */
      n = (curwin->w_wcol - curwin->w_width) / width + 1;
      curwin->w_wcol -= n * width;
      curwin->w_wrow += n;

#ifdef FEAT_LINEBREAK
      /* When cursor wraps to first char of next line in Insert
	     * mode, the 'showbreak' string isn't shown, backup to first
	     * column */
      if (*p_sbr && *ml_get_cursor() == NUL && curwin->w_wcol == (int)vim_strsize(p_sbr))
        curwin->w_wcol = 0;
#endif
    }
  }

  /* No line wrapping: compute curwin->w_leftcol if scrolling is on and line
     * is not folded.
     * If scrolling is off, curwin->w_leftcol is assumed to be 0 */
  else if (may_scroll
#ifdef FEAT_FOLDING
           && !curwin->w_cline_folded
#endif
  )
  {
    /*
	 * If Cursor is left of the screen, scroll rightwards.
	 * If Cursor is right of the screen, scroll leftwards
	 * If we get closer to the edge than 'sidescrolloff', scroll a little
	 * extra
	 */
    off_left = (int)startcol - (int)curwin->w_leftcol - siso;
    off_right = (int)endcol - (int)(curwin->w_leftcol + curwin->w_width - siso) + 1;
    if (off_left < 0 || off_right > 0)
    {
      if (off_left < 0)
        diff = -off_left;
      else
        diff = off_right;

      /* When far off or not enough room on either side, put cursor in
	     * middle of window. */
      if (p_ss == 0 || diff >= textwidth / 2 || off_right >= off_left)
        new_leftcol = curwin->w_wcol - extra - textwidth / 2;
      else
      {
        if (diff < p_ss)
          diff = p_ss;
        if (off_left < 0)
          new_leftcol = curwin->w_leftcol - diff;
        else
          new_leftcol = curwin->w_leftcol + diff;
      }
      if (new_leftcol < 0)
        new_leftcol = 0;
      if (new_leftcol != (int)curwin->w_leftcol)
      {
        curwin->w_leftcol = new_leftcol;
        /* screen has to be redrawn with new curwin->w_leftcol */
        redraw_later(NOT_VALID);
      }
    }
    curwin->w_wcol -= curwin->w_leftcol;
  }
  else if (curwin->w_wcol > (int)curwin->w_leftcol)
    curwin->w_wcol -= curwin->w_leftcol;
  else
    curwin->w_wcol = 0;

#ifdef FEAT_DIFF
  /* Skip over filler lines.  At the top use w_topfill, there
     * may be some filler lines above the window. */
  if (curwin->w_cursor.lnum == curwin->w_topline)
    curwin->w_wrow += curwin->w_topfill;
  else
    curwin->w_wrow += diff_check_fill(curwin, curwin->w_cursor.lnum);
#endif

  prev_skipcol = curwin->w_skipcol;

  p_lines = 0;

  if ((curwin->w_wrow >= curwin->w_height || ((prev_skipcol > 0 || curwin->w_wrow + so >= curwin->w_height) && (p_lines =
#ifdef FEAT_DIFF
                                                                                                                    plines_win_nofill
#else
                                                                                                                    plines_win
#endif
                                                                                                                (curwin, curwin->w_cursor.lnum, FALSE)) -
                                                                                                                       1 >=
                                                                                                                   curwin->w_height)) &&
      curwin->w_height != 0 && curwin->w_cursor.lnum == curwin->w_topline && width > 0 && curwin->w_width != 0)
  {
    /* Cursor past end of screen.  Happens with a single line that does
	 * not fit on screen.  Find a skipcol to show the text around the
	 * cursor.  Avoid scrolling all the time. compute value of "extra":
	 * 1: Less than 'scrolloff' lines above
	 * 2: Less than 'scrolloff' lines below
	 * 3: both of them */
    extra = 0;
    if (curwin->w_skipcol + so * width > curwin->w_virtcol)
      extra = 1;
    /* Compute last display line of the buffer line that we want at the
	 * bottom of the window. */
    if (p_lines == 0)
      p_lines = plines_win(curwin, curwin->w_cursor.lnum, FALSE);
    --p_lines;
    if (p_lines > curwin->w_wrow + so)
      n = curwin->w_wrow + so;
    else
      n = p_lines;
    if ((colnr_T)n >= curwin->w_height + curwin->w_skipcol / width)
      extra += 2;

    if (extra == 3 || p_lines < so * 2)
    {
      /* not enough room for 'scrolloff', put cursor in the middle */
      n = curwin->w_virtcol / width;
      if (n > curwin->w_height / 2)
        n -= curwin->w_height / 2;
      else
        n = 0;
      /* don't skip more than necessary */
      if (n > p_lines - curwin->w_height + 1)
        n = p_lines - curwin->w_height + 1;
      curwin->w_skipcol = n * width;
    }
    else if (extra == 1)
    {
      /* less then 'scrolloff' lines above, decrease skipcol */
      extra = (curwin->w_skipcol + so * width - curwin->w_virtcol + width - 1) / width;
      if (extra > 0)
      {
        if ((colnr_T)(extra * width) > curwin->w_skipcol)
          extra = curwin->w_skipcol / width;
        curwin->w_skipcol -= extra * width;
      }
    }
    else if (extra == 2)
    {
      /* less then 'scrolloff' lines below, increase skipcol */
      endcol = (n - curwin->w_height + 1) * width;
      while (endcol > curwin->w_virtcol)
        endcol -= width;
      if (endcol > curwin->w_skipcol)
        curwin->w_skipcol = endcol;
    }

    curwin->w_wrow -= curwin->w_skipcol / width;
    if (curwin->w_wrow >= curwin->w_height)
    {
      /* small window, make sure cursor is in it */
      extra = curwin->w_wrow - curwin->w_height + 1;
      curwin->w_skipcol += extra * width;
      curwin->w_wrow -= extra;
    }

    extra = ((int)prev_skipcol - (int)curwin->w_skipcol) / width;
    if (extra > 0)
      win_ins_lines(curwin, 0, extra, FALSE, FALSE);
    else if (extra < 0)
      win_del_lines(curwin, 0, -extra, FALSE, FALSE, 0);
  }
  else
    curwin->w_skipcol = 0;
  if (prev_skipcol != curwin->w_skipcol)
    redraw_later(NOT_VALID);

  curwin->w_valid |= VALID_WCOL | VALID_WROW | VALID_VIRTCOL;
}

/*
 * Scroll the current window down by "line_count" logical lines.  "CTRL-Y"
 */
void scrolldown(
    long line_count,
    int byfold UNUSED) /* TRUE: count a closed fold as one line */
{
  if (scrollCallback != NULL)
  {
    scrollCallback(SCROLL_LINE_DOWN, line_count);
  }
}

/*
 * Scroll the current window up by "line_count" logical lines.  "CTRL-E"
 */
void scrollup(
    long line_count,
    int byfold UNUSED) /* TRUE: count a closed fold as one line */
{
  if (scrollCallback != NULL)
  {
    scrollCallback(SCROLL_LINE_UP, line_count);
  }
}

#ifdef FEAT_DIFF
/*
 * Don't end up with too many filler lines in the window.
 */
void check_topfill(
    win_T *wp,
    int down) /* when TRUE scroll down when not enough space */
{
  int n;

  if (wp->w_topfill > 0)
  {
    n = plines_win_nofill(wp, wp->w_topline, TRUE);
    if (wp->w_topfill + n > wp->w_height)
    {
      if (down && wp->w_topline > 1)
      {
        --wp->w_topline;
        wp->w_topfill = 0;
      }
      else
      {
        wp->w_topfill = wp->w_height - n;
        if (wp->w_topfill < 0)
          wp->w_topfill = 0;
      }
    }
  }
}

#endif

/*
 * Recompute topline to put the cursor at the top of the window.
 * Scroll at least "min_scroll" lines.
 * If "always" is TRUE, always set topline (for "zt").
 */
void scroll_cursor_top(int min_scroll, int always)
{
  if (scrollCallback != NULL)
  {
    scrollCallback(SCROLL_CURSOR_TOP, 1);
  }
}

/*
 * Set w_empty_rows and w_filler_rows for window "wp", having used up "used"
 * screen lines for text lines.
 */
void set_empty_rows(win_T *wp, int used)
{
#ifdef FEAT_DIFF
  wp->w_filler_rows = 0;
#endif
  if (used == 0)
    wp->w_empty_rows = 0; /* single line that doesn't fit */
  else
  {
    wp->w_empty_rows = wp->w_height - used;
#ifdef FEAT_DIFF
    if (wp->w_botline <= wp->w_buffer->b_ml.ml_line_count)
    {
      wp->w_filler_rows = diff_check_fill(wp, wp->w_botline);
      if (wp->w_empty_rows > wp->w_filler_rows)
        wp->w_empty_rows -= wp->w_filler_rows;
      else
      {
        wp->w_filler_rows = wp->w_empty_rows;
        wp->w_empty_rows = 0;
      }
    }
#endif
  }
}

/*
 * Recompute topline to put the cursor at the bottom of the window.
 * Scroll at least "min_scroll" lines.
 * If "set_topbot" is TRUE, set topline and botline first (for "zb").
 * This is messy stuff!!!
 */
void scroll_cursor_bot(int min_scroll, int set_topbot)
{
  if (scrollCallback != NULL)
  {
    scrollCallback(SCROLL_CURSOR_BOTTOM, 1);
  }
}

/*
 * Recompute topline to put the cursor halfway the window
 * If "atend" is TRUE, also put it halfway at the end of the file.
 */
void scroll_cursor_halfway(int atend)
{
  if (scrollCallback != NULL)
  {
    scrollCallback(SCROLL_CURSOR_CENTERV, 1);
  }
}

/*
 * Correct the cursor position so that it is in a part of the screen at least
 * 'scrolloff' lines from the top and bottom, if possible.
 * If not possible, put it at the same position as scroll_cursor_halfway().
 * When called topline must be valid!
 */
void cursor_correct(void)
{
  int above = 0; /* screen lines above topline */
  linenr_T topline;
  int below = 0; /* screen lines below botline */
  linenr_T botline;
  int above_wanted, below_wanted;
  linenr_T cln; /* Cursor Line Number */
  int max_off;
  long so = get_scrolloff_value();

  /*
     * How many lines we would like to have above/below the cursor depends on
     * whether the first/last line of the file is on screen.
     */
  above_wanted = so;
  below_wanted = so;
  if (curwin->w_topline == 1)
  {
    above_wanted = 0;
    max_off = curwin->w_height / 2;
    if (below_wanted > max_off)
      below_wanted = max_off;
  }
  validate_botline();
  if (curwin->w_botline == curbuf->b_ml.ml_line_count + 1)
  {
    below_wanted = 0;
    max_off = (curwin->w_height - 1) / 2;
    if (above_wanted > max_off)
      above_wanted = max_off;
  }

  /*
     * If there are sufficient file-lines above and below the cursor, we can
     * return now.
     */
  cln = curwin->w_cursor.lnum;
  if (cln >= curwin->w_topline + above_wanted && cln < curwin->w_botline - below_wanted
#ifdef FEAT_FOLDING
      && !hasAnyFolding(curwin)
#endif
  )
    return;

  /*
     * Narrow down the area where the cursor can be put by taking lines from
     * the top and the bottom until:
     * - the desired context lines are found
     * - the lines from the top is past the lines from the bottom
     */
  topline = curwin->w_topline;
  botline = curwin->w_botline - 1;
#ifdef FEAT_DIFF
  /* count filler lines as context */
  above = curwin->w_topfill;
  below = curwin->w_filler_rows;
#endif
  while ((above < above_wanted || below < below_wanted) && topline < botline)
  {
    if (below < below_wanted && (below <= above || above >= above_wanted))
    {
#ifdef FEAT_FOLDING
      if (hasFolding(botline, &botline, NULL))
        ++below;
      else
#endif
        below += plines(botline);
      --botline;
    }
    if (above < above_wanted && (above < below || below >= below_wanted))
    {
#ifdef FEAT_FOLDING
      if (hasFolding(topline, NULL, &topline))
        ++above;
      else
#endif
        above += PLINES_NOFILL(topline);
#ifdef FEAT_DIFF
      /* Count filler lines below this line as context. */
      if (topline < botline)
        above += diff_check_fill(curwin, topline + 1);
#endif
      ++topline;
    }
  }
  if (topline == botline || botline == 0)
    curwin->w_cursor.lnum = topline;
  else if (topline > botline)
    curwin->w_cursor.lnum = botline;
  else
  {
    if (cln < topline && curwin->w_topline > 1)
    {
      curwin->w_cursor.lnum = topline;
      curwin->w_valid &=
          ~(VALID_WROW | VALID_WCOL | VALID_CHEIGHT | VALID_CROW);
    }
    if (cln > botline && curwin->w_botline <= curbuf->b_ml.ml_line_count)
    {
      curwin->w_cursor.lnum = botline;
      curwin->w_valid &=
          ~(VALID_WROW | VALID_WCOL | VALID_CHEIGHT | VALID_CROW);
    }
  }
  curwin->w_valid |= VALID_TOPLINE;
}

/*
 * move screen 'count' pages up or down and update screen
 *
 * return FAIL for failure, OK otherwise
 */
int onepage(int dir, long count)
{
  int retval = OK;

  if (scrollCallback != NULL)
  {
    scrollCallback(dir == BACKWARD ? SCROLL_PAGE_UP : SCROLL_PAGE_DOWN, count);
  }
  else
  {
    retval = FAIL;
  }

  return retval;
}

/*
 * Scroll 'scroll' lines up or down.
 */
void halfpage(int flag, linenr_T Prenum)
{
  if (scrollCallback != NULL)
  {
    scrollCallback(flag ? SCROLL_HALFPAGE_DOWN : SCROLL_HALFPAGE_UP, Prenum);
  }
}

void do_check_cursorbind(void)
{
  linenr_T line = curwin->w_cursor.lnum;
  colnr_T col = curwin->w_cursor.col;
  colnr_T coladd = curwin->w_cursor.coladd;
  colnr_T curswant = curwin->w_curswant;
  int set_curswant = curwin->w_set_curswant;
  win_T *old_curwin = curwin;
  buf_T *old_curbuf = curbuf;
  int restart_edit_save;
  int old_VIsual_select = VIsual_select;
  int old_VIsual_active = VIsual_active;

  /*
     * loop through the cursorbound windows
     */
  VIsual_select = VIsual_active = 0;
  FOR_ALL_WINDOWS(curwin)
  {
    curbuf = curwin->w_buffer;
    /* skip original window  and windows with 'noscrollbind' */
    if (curwin != old_curwin && curwin->w_p_crb)
    {
#ifdef FEAT_DIFF
      if (curwin->w_p_diff)
        curwin->w_cursor.lnum =
            diff_get_corresponding_line(old_curbuf, line);
      else
#endif
        curwin->w_cursor.lnum = line;
      curwin->w_cursor.col = col;
      curwin->w_cursor.coladd = coladd;
      curwin->w_curswant = curswant;
      curwin->w_set_curswant = set_curswant;

      /* Make sure the cursor is in a valid position.  Temporarily set
	     * "restart_edit" to allow the cursor to be beyond the EOL. */
      restart_edit_save = restart_edit;
      restart_edit = TRUE;
      check_cursor();
      restart_edit = restart_edit_save;
      /* Correct cursor for multi-byte character. */
      if (has_mbyte)
        mb_adjust_cursor();
      redraw_later(VALID);

      /* Only scroll when 'scrollbind' hasn't done this. */
      if (!curwin->w_p_scb)
        update_topline();
      curwin->w_redr_status = TRUE;
    }
  }

  /*
     * reset current-window
     */
  VIsual_select = old_VIsual_select;
  VIsual_active = old_VIsual_active;
  curwin = old_curwin;
  curbuf = old_curbuf;
}
