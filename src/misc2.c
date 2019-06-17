/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * misc2.c: Various functions.
 */
#include "vim.h"

static char_u	*username = NULL; /* cached result of mch_get_user_name() */

static int coladvance2(pos_T *pos, int addspaces, int finetune, colnr_T wcol);

/*
 * Return TRUE if in the current mode we need to use virtual.
 */
    int
virtual_active(void)
{
    /* While an operator is being executed we return "virtual_op", because
     * VIsual_active has already been reset, thus we can't check for "block"
     * being used. */
    if (virtual_op != MAYBE)
	return virtual_op;
    return (ve_flags == VE_ALL
	    || ((ve_flags & VE_BLOCK) && VIsual_active && VIsual_mode == Ctrl_V)
	    || ((ve_flags & VE_INSERT) && (State & INSERT)));
}

/*
 * Get the screen position of the cursor.
 */
    int
getviscol(void)
{
    colnr_T	x;

    getvvcol(curwin, &curwin->w_cursor, &x, NULL, NULL);
    return (int)x;
}

/*
 * Go to column "wcol", and add/insert white space as necessary to get the
 * cursor in that column.
 * The caller must have saved the cursor line for undo!
 */
    int
coladvance_force(colnr_T wcol)
{
    int rc = coladvance2(&curwin->w_cursor, TRUE, FALSE, wcol);

    if (wcol == MAXCOL)
	curwin->w_valid &= ~VALID_VIRTCOL;
    else
    {
	/* Virtcol is valid */
	curwin->w_valid |= VALID_VIRTCOL;
	curwin->w_virtcol = wcol;
    }
    return rc;
}

/*
 * Get the screen position of character col with a coladd in the cursor line.
 */
    int
getviscol2(colnr_T col, colnr_T coladd UNUSED)
{
    colnr_T	x;
    pos_T	pos;

    pos.lnum = curwin->w_cursor.lnum;
    pos.col = col;
    pos.coladd = coladd;
    getvvcol(curwin, &pos, &x, NULL, NULL);
    return (int)x;
}

/*
 * Try to advance the Cursor to the specified screen column.
 * If virtual editing: fine tune the cursor position.
 * Note that all virtual positions off the end of a line should share
 * a curwin->w_cursor.col value (n.b. this is equal to STRLEN(line)),
 * beginning at coladd 0.
 *
 * return OK if desired column is reached, FAIL if not
 */
    int
coladvance(colnr_T wcol)
{
    int rc = getvpos(&curwin->w_cursor, wcol);

    if (wcol == MAXCOL || rc == FAIL)
	curwin->w_valid &= ~VALID_VIRTCOL;
    else if (*ml_get_cursor() != TAB)
    {
	/* Virtcol is valid when not on a TAB */
	curwin->w_valid |= VALID_VIRTCOL;
	curwin->w_virtcol = wcol;
    }
    return rc;
}

/*
 * Return in "pos" the position of the cursor advanced to screen column "wcol".
 * return OK if desired column is reached, FAIL if not
 */
    int
getvpos(pos_T *pos, colnr_T wcol)
{
    return coladvance2(pos, FALSE, virtual_active(), wcol);
}

    static int
coladvance2(
    pos_T	*pos,
    int		addspaces,	/* change the text to achieve our goal? */
    int		finetune,	/* change char offset for the exact column */
    colnr_T	wcol)		/* column to move to */
{
    int		idx;
    char_u	*ptr;
    char_u	*line;
    colnr_T	col = 0;
    int		csize = 0;
    int		one_more;
#ifdef FEAT_LINEBREAK
    int		head = 0;
#endif

    one_more = (State & INSERT)
		    || restart_edit != NUL
		    || (VIsual_active && *p_sel != 'o')
		    || ((ve_flags & VE_ONEMORE) && wcol < MAXCOL) ;
    line = ml_get_buf(curbuf, pos->lnum, FALSE);

    if (wcol >= MAXCOL)
    {
	    idx = (int)STRLEN(line) - 1 + one_more;
	    col = wcol;

	    if ((addspaces || finetune) && !VIsual_active)
	    {
		curwin->w_curswant = linetabsize(line) + one_more;
		if (curwin->w_curswant > 0)
		    --curwin->w_curswant;
	    }
    }
    else
    {
	int width = curwin->w_width - win_col_off(curwin);

	if (finetune
		&& curwin->w_p_wrap
		&& curwin->w_width != 0
		&& wcol >= (colnr_T)width)
	{
	    csize = linetabsize(line);
	    if (csize > 0)
		csize--;

	    if (wcol / width > (colnr_T)csize / width
		    && ((State & INSERT) == 0 || (int)wcol > csize + 1))
	    {
		/* In case of line wrapping don't move the cursor beyond the
		 * right screen edge.  In Insert mode allow going just beyond
		 * the last character (like what happens when typing and
		 * reaching the right window edge). */
		wcol = (csize / width + 1) * width - 1;
	    }
	}

	ptr = line;
	while (col <= wcol && *ptr != NUL)
	{
	    /* Count a tab for what it's worth (if list mode not on) */
#ifdef FEAT_LINEBREAK
	    csize = win_lbr_chartabsize(curwin, line, ptr, col, &head);
	    MB_PTR_ADV(ptr);
#else
	    csize = lbr_chartabsize_adv(line, &ptr, col);
#endif
	    col += csize;
	}
	idx = (int)(ptr - line);
	/*
	 * Handle all the special cases.  The virtual_active() check
	 * is needed to ensure that a virtual position off the end of
	 * a line has the correct indexing.  The one_more comparison
	 * replaces an explicit add of one_more later on.
	 */
	if (col > wcol || (!virtual_active() && one_more == 0))
	{
	    idx -= 1;
# ifdef FEAT_LINEBREAK
	    /* Don't count the chars from 'showbreak'. */
	    csize -= head;
# endif
	    col -= csize;
	}

	if (virtual_active()
		&& addspaces
		&& ((col != wcol && col != wcol + 1) || csize > 1))
	{
	    /* 'virtualedit' is set: The difference between wcol and col is
	     * filled with spaces. */

	    if (line[idx] == NUL)
	    {
		/* Append spaces */
		int	correct = wcol - col;
		char_u	*newline = alloc(idx + correct + 1);
		int	t;

		if (newline == NULL)
		    return FAIL;

		for (t = 0; t < idx; ++t)
		    newline[t] = line[t];

		for (t = 0; t < correct; ++t)
		    newline[t + idx] = ' ';

		newline[idx + correct] = NUL;

		ml_replace(pos->lnum, newline, FALSE);
		changed_bytes(pos->lnum, (colnr_T)idx);
		idx += correct;
		col = wcol;
	    }
	    else
	    {
		/* Break a tab */
		int	linelen = (int)STRLEN(line);
		int	correct = wcol - col - csize + 1; /* negative!! */
		char_u	*newline;
		int	t, s = 0;
		int	v;

		if (-correct > csize)
		    return FAIL;

		newline = alloc(linelen + csize);
		if (newline == NULL)
		    return FAIL;

		for (t = 0; t < linelen; t++)
		{
		    if (t != idx)
			newline[s++] = line[t];
		    else
			for (v = 0; v < csize; v++)
			    newline[s++] = ' ';
		}

		newline[linelen + csize - 1] = NUL;

		ml_replace(pos->lnum, newline, FALSE);
		changed_bytes(pos->lnum, idx);
		idx += (csize - 1 + correct);
		col += correct;
	    }
	}
    }

    if (idx < 0)
	pos->col = 0;
    else
	pos->col = idx;

    pos->coladd = 0;

    if (finetune)
    {
	if (wcol == MAXCOL)
	{
	    /* The width of the last character is used to set coladd. */
	    if (!one_more)
	    {
		colnr_T	    scol, ecol;

		getvcol(curwin, pos, &scol, NULL, &ecol);
		pos->coladd = ecol - scol;
	    }
	}
	else
	{
	    int b = (int)wcol - (int)col;

	    /* The difference between wcol and col is used to set coladd. */
	    if (b > 0 && b < (MAXCOL - 2 * curwin->w_width))
		pos->coladd = b;

	    col += b;
	}
    }

    /* prevent from moving onto a trail byte */
    if (has_mbyte)
	mb_adjustpos(curbuf, pos);

    if (col < wcol)
	return FAIL;
    return OK;
}

/*
 * Increment the cursor position.  See inc() for return values.
 */
    int
inc_cursor(void)
{
    return inc(&curwin->w_cursor);
}

/*
 * Increment the line pointer "lp" crossing line boundaries as necessary.
 * Return 1 when going to the next line.
 * Return 2 when moving forward onto a NUL at the end of the line).
 * Return -1 when at the end of file.
 * Return 0 otherwise.
 */
    int
inc(pos_T *lp)
{
    char_u  *p;

    /* when searching position may be set to end of a line */
    if (lp->col != MAXCOL)
    {
	p = ml_get_pos(lp);
	if (*p != NUL)	/* still within line, move to next char (may be NUL) */
	{
	    if (has_mbyte)
	    {
		int l = (*mb_ptr2len)(p);

		lp->col += l;
		return ((p[l] != NUL) ? 0 : 2);
	    }
	    lp->col++;
	    lp->coladd = 0;
	    return ((p[1] != NUL) ? 0 : 2);
	}
    }
    if (lp->lnum != curbuf->b_ml.ml_line_count)     /* there is a next line */
    {
	lp->col = 0;
	lp->lnum++;
	lp->coladd = 0;
	return 1;
    }
    return -1;
}

/*
 * incl(lp): same as inc(), but skip the NUL at the end of non-empty lines
 */
    int
incl(pos_T *lp)
{
    int	    r;

    if ((r = inc(lp)) >= 1 && lp->col)
	r = inc(lp);
    return r;
}

/*
 * dec(p)
 *
 * Decrement the line pointer 'p' crossing line boundaries as necessary.
 * Return 1 when crossing a line, -1 when at start of file, 0 otherwise.
 */
    int
dec_cursor(void)
{
    return dec(&curwin->w_cursor);
}

    int
dec(pos_T *lp)
{
    char_u	*p;

    lp->coladd = 0;
    if (lp->col == MAXCOL)
    {
	/* past end of line */
	p = ml_get(lp->lnum);
	lp->col = (colnr_T)STRLEN(p);
	if (has_mbyte)
	    lp->col -= (*mb_head_off)(p, p + lp->col);
	return 0;
    }

    if (lp->col > 0)
    {
	/* still within line */
	lp->col--;
	if (has_mbyte)
	{
	    p = ml_get(lp->lnum);
	    lp->col -= (*mb_head_off)(p, p + lp->col);
	}
	return 0;
    }

    if (lp->lnum > 1)
    {
	/* there is a prior line */
	lp->lnum--;
	p = ml_get(lp->lnum);
	lp->col = (colnr_T)STRLEN(p);
	if (has_mbyte)
	    lp->col -= (*mb_head_off)(p, p + lp->col);
	return 1;
    }

    /* at start of file */
    return -1;
}

/*
 * decl(lp): same as dec(), but skip the NUL at the end of non-empty lines
 */
    int
decl(pos_T *lp)
{
    int	    r;

    if ((r = dec(lp)) == 1 && lp->col)
	r = dec(lp);
    return r;
}

/*
 * Get the line number relative to the current cursor position, i.e. the
 * difference between line number and cursor position. Only look for lines that
 * can be visible, folded lines don't count.
 */
    linenr_T
get_cursor_rel_lnum(
    win_T	*wp,
    linenr_T	lnum)		    /* line number to get the result for */
{
    linenr_T	cursor = wp->w_cursor.lnum;
    linenr_T	retval = 0;

#ifdef FEAT_FOLDING
    if (hasAnyFolding(wp))
    {
	if (lnum > cursor)
	{
	    while (lnum > cursor)
	    {
		(void)hasFoldingWin(wp, lnum, &lnum, NULL, TRUE, NULL);
		/* if lnum and cursor are in the same fold,
		 * now lnum <= cursor */
		if (lnum > cursor)
		    retval++;
		lnum--;
	    }
	}
	else if (lnum < cursor)
	{
	    while (lnum < cursor)
	    {
		(void)hasFoldingWin(wp, lnum, NULL, &lnum, TRUE, NULL);
		/* if lnum and cursor are in the same fold,
		 * now lnum >= cursor */
		if (lnum < cursor)
		    retval--;
		lnum++;
	    }
	}
	/* else if (lnum == cursor)
	 *     retval = 0;
	 */
    }
    else
#endif
	retval = lnum - cursor;

    return retval;
}

/*
 * Make sure "pos.lnum" and "pos.col" are valid in "buf".
 * This allows for the col to be on the NUL byte.
 */
    void
check_pos(buf_T *buf, pos_T *pos)
{
    char_u *line;
    colnr_T len;

    if (pos->lnum > buf->b_ml.ml_line_count)
	pos->lnum = buf->b_ml.ml_line_count;

    if (pos->col > 0)
    {
	line = ml_get_buf(buf, pos->lnum, FALSE);
	len = (colnr_T)STRLEN(line);
	if (pos->col > len)
	    pos->col = len;
    }
}

/*
 * Make sure curwin->w_cursor.lnum is valid.
 */
    void
check_cursor_lnum(void)
{
    if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
    {
#ifdef FEAT_FOLDING
	/* If there is a closed fold at the end of the file, put the cursor in
	 * its first line.  Otherwise in the last line. */
	if (!hasFolding(curbuf->b_ml.ml_line_count,
						&curwin->w_cursor.lnum, NULL))
#endif
	    curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
    }
    if (curwin->w_cursor.lnum <= 0)
	curwin->w_cursor.lnum = 1;
}

/*
 * Make sure curwin->w_cursor.col is valid.
 */
    void
check_cursor_col(void)
{
    check_cursor_col_win(curwin);
}

/*
 * Make sure win->w_cursor.col is valid.
 */
    void
check_cursor_col_win(win_T *win)
{
    colnr_T len;
    colnr_T oldcol = win->w_cursor.col;
    colnr_T oldcoladd = win->w_cursor.col + win->w_cursor.coladd;

    len = (colnr_T)STRLEN(ml_get_buf(win->w_buffer, win->w_cursor.lnum, FALSE));
    if (len == 0)
	win->w_cursor.col = 0;
    else if (win->w_cursor.col >= len)
    {
	/* Allow cursor past end-of-line when:
	 * - in Insert mode or restarting Insert mode
	 * - in Visual mode and 'selection' isn't "old"
	 * - 'virtualedit' is set */
	if ((State & INSERT) || restart_edit
		|| (VIsual_active && *p_sel != 'o')
		|| (ve_flags & VE_ONEMORE)
		|| virtual_active())
	    win->w_cursor.col = len;
	else
	{
	    win->w_cursor.col = len - 1;
	    /* Move the cursor to the head byte. */
	    if (has_mbyte)
		mb_adjustpos(win->w_buffer, &win->w_cursor);
	}
    }
    else if (win->w_cursor.col < 0)
	win->w_cursor.col = 0;

    /* If virtual editing is on, we can leave the cursor on the old position,
     * only we must set it to virtual.  But don't do it when at the end of the
     * line. */
    if (oldcol == MAXCOL)
	win->w_cursor.coladd = 0;
    else if (ve_flags == VE_ALL)
    {
	if (oldcoladd > win->w_cursor.col)
	{
	    win->w_cursor.coladd = oldcoladd - win->w_cursor.col;

	    /* Make sure that coladd is not more than the char width.
	     * Not for the last character, coladd is then used when the cursor
	     * is actually after the last character. */
	    if (win->w_cursor.col + 1 < len && win->w_cursor.coladd > 0)
	    {
		int cs, ce;

		getvcol(win, &win->w_cursor, &cs, NULL, &ce);
		if (win->w_cursor.coladd > ce - cs)
		    win->w_cursor.coladd = ce - cs;
	    }
	}
	else
	    /* avoid weird number when there is a miscalculation or overflow */
	    win->w_cursor.coladd = 0;
    }
}

/*
 * make sure curwin->w_cursor in on a valid character
 */
    void
check_cursor(void)
{
    check_cursor_lnum();
    check_cursor_col();
}

#if defined(FEAT_TEXTOBJ) || defined(PROTO)
/*
 * Make sure curwin->w_cursor is not on the NUL at the end of the line.
 * Allow it when in Visual mode and 'selection' is not "old".
 */
    void
adjust_cursor_col(void)
{
    if (curwin->w_cursor.col > 0
	    && (!VIsual_active || *p_sel == 'o')
	    && gchar_cursor() == NUL)
	--curwin->w_cursor.col;
}
#endif

/*
 * When curwin->w_leftcol has changed, adjust the cursor position.
 * Return TRUE if the cursor was moved.
 */
    int
leftcol_changed(void)
{
    long	lastcol;
    colnr_T	s, e;
    int		retval = FALSE;
    long        siso = get_sidescrolloff_value();

    changed_cline_bef_curs();
    lastcol = curwin->w_leftcol + curwin->w_width - curwin_col_off() - 1;
    validate_virtcol();

    /*
     * If the cursor is right or left of the screen, move it to last or first
     * character.
     */
    if (curwin->w_virtcol > (colnr_T)(lastcol - siso))
    {
	retval = TRUE;
	coladvance((colnr_T)(lastcol - siso));
    }
    else if (curwin->w_virtcol < curwin->w_leftcol + siso)
    {
	retval = TRUE;
	(void)coladvance((colnr_T)(curwin->w_leftcol + siso));
    }

    /*
     * If the start of the character under the cursor is not on the screen,
     * advance the cursor one more char.  If this fails (last char of the
     * line) adjust the scrolling.
     */
    getvvcol(curwin, &curwin->w_cursor, &s, NULL, &e);
    if (e > (colnr_T)lastcol)
    {
	retval = TRUE;
	coladvance(s - 1);
    }
    else if (s < curwin->w_leftcol)
    {
	retval = TRUE;
	if (coladvance(e + 1) == FAIL)	/* there isn't another character */
	{
	    curwin->w_leftcol = s;	/* adjust w_leftcol instead */
	    changed_cline_bef_curs();
	}
    }

    if (retval)
	curwin->w_set_curswant = TRUE;
    redraw_later(NOT_VALID);
    return retval;
}

/**********************************************************************
 * Various routines dealing with allocation and deallocation of memory.
 */

#if defined(MEM_PROFILE) || defined(PROTO)

# define MEM_SIZES  8200
static long_u mem_allocs[MEM_SIZES];
static long_u mem_frees[MEM_SIZES];
static long_u mem_allocated;
static long_u mem_freed;
static long_u mem_peak;
static long_u num_alloc;
static long_u num_freed;

    static void
mem_pre_alloc_s(size_t *sizep)
{
    *sizep += sizeof(size_t);
}

    static void
mem_pre_alloc_l(size_t *sizep)
{
    *sizep += sizeof(size_t);
}

    static void
mem_post_alloc(
    void **pp,
    size_t size)
{
    if (*pp == NULL)
	return;
    size -= sizeof(size_t);
    *(long_u *)*pp = size;
    if (size <= MEM_SIZES-1)
	mem_allocs[size-1]++;
    else
	mem_allocs[MEM_SIZES-1]++;
    mem_allocated += size;
    if (mem_allocated - mem_freed > mem_peak)
	mem_peak = mem_allocated - mem_freed;
    num_alloc++;
    *pp = (void *)((char *)*pp + sizeof(size_t));
}

    static void
mem_pre_free(void **pp)
{
    long_u size;

    *pp = (void *)((char *)*pp - sizeof(size_t));
    size = *(size_t *)*pp;
    if (size <= MEM_SIZES-1)
	mem_frees[size-1]++;
    else
	mem_frees[MEM_SIZES-1]++;
    mem_freed += size;
    num_freed++;
}

/*
 * called on exit via atexit()
 */
    void
vim_mem_profile_dump(void)
{
    int i, j;

    printf("\r\n");
    j = 0;
    for (i = 0; i < MEM_SIZES - 1; i++)
    {
	if (mem_allocs[i] || mem_frees[i])
	{
	    if (mem_frees[i] > mem_allocs[i])
		printf("\r\n%s", _("ERROR: "));
	    printf("[%4d / %4lu-%-4lu] ", i + 1, mem_allocs[i], mem_frees[i]);
	    j++;
	    if (j > 3)
	    {
		j = 0;
		printf("\r\n");
	    }
	}
    }

    i = MEM_SIZES - 1;
    if (mem_allocs[i])
    {
	printf("\r\n");
	if (mem_frees[i] > mem_allocs[i])
	    puts(_("ERROR: "));
	printf("[>%d / %4lu-%-4lu]", i, mem_allocs[i], mem_frees[i]);
    }

    printf(_("\n[bytes] total alloc-freed %lu-%lu, in use %lu, peak use %lu\n"),
	    mem_allocated, mem_freed, mem_allocated - mem_freed, mem_peak);
    printf(_("[calls] total re/malloc()'s %lu, total free()'s %lu\n\n"),
	    num_alloc, num_freed);
}

#endif /* MEM_PROFILE */

#ifdef FEAT_EVAL
    int
alloc_does_fail(size_t size)
{
    if (alloc_fail_countdown == 0)
    {
	if (--alloc_fail_repeat <= 0)
	    alloc_fail_id = 0;
	do_outofmem_msg(size);
	return TRUE;
    }
    --alloc_fail_countdown;
    return FALSE;
}
#endif

/*
 * Some memory is reserved for error messages and for being able to
 * call mf_release_all(), which needs some memory for mf_trans_add().
 */
#define KEEP_ROOM (2 * 8192L)
#define KEEP_ROOM_KB (KEEP_ROOM / 1024L)

/*
 * The normal way to allocate memory.  This handles an out-of-memory situation
 * as well as possible, still returns NULL when we're completely out.
 */
    void *
alloc(size_t size)
{
    return lalloc(size, TRUE);
}

/*
 * alloc() with an ID for alloc_fail().
 */
    void *
alloc_id(size_t size, alloc_id_T id UNUSED)
{
#ifdef FEAT_EVAL
    if (alloc_fail_id == id && alloc_does_fail(size))
	return NULL;
#endif
    return lalloc(size, TRUE);
}

/*
 * Allocate memory and set all bytes to zero.
 */
    void *
alloc_clear(size_t size)
{
    void *p;

    p = lalloc(size, TRUE);
    if (p != NULL)
	(void)vim_memset(p, 0, size);
    return p;
}

/*
 * Same as alloc_clear() but with allocation id for testing
 */
    void *
alloc_clear_id(size_t size, alloc_id_T id UNUSED)
{
#ifdef FEAT_EVAL
    if (alloc_fail_id == id && alloc_does_fail(size))
	return NULL;
#endif
    return alloc_clear(size);
}

/*
 * Allocate memory like lalloc() and set all bytes to zero.
 */
    void *
lalloc_clear(size_t size, int message)
{
    void *p;

    p = lalloc(size, message);
    if (p != NULL)
	(void)vim_memset(p, 0, size);
    return p;
}

/*
 * Low level memory allocation function.
 * This is used often, KEEP IT FAST!
 */
    void *
lalloc(size_t size, int message)
{
    void	*p;		    /* pointer to new storage space */
    static int	releasing = FALSE;  /* don't do mf_release_all() recursive */
    int		try_again;
#if defined(HAVE_AVAIL_MEM)
    static size_t allocated = 0;    /* allocated since last avail check */
#endif

    // Safety check for allocating zero bytes
    if (size == 0)
    {
	// Don't hide this message
	emsg_silent = 0;
	iemsg(_("E341: Internal error: lalloc(0, )"));
	return NULL;
    }

#ifdef MEM_PROFILE
    mem_pre_alloc_l(&size);
#endif

    /*
     * Loop when out of memory: Try to release some memfile blocks and
     * if some blocks are released call malloc again.
     */
    for (;;)
    {
	/*
	 * Handle three kind of systems:
	 * 1. No check for available memory: Just return.
	 * 2. Slow check for available memory: call mch_avail_mem() after
	 *    allocating KEEP_ROOM amount of memory.
	 * 3. Strict check for available memory: call mch_avail_mem()
	 */
	if ((p = malloc(size)) != NULL)
	{
#ifndef HAVE_AVAIL_MEM
	    /* 1. No check for available memory: Just return. */
	    goto theend;
#else
	    /* 2. Slow check for available memory: call mch_avail_mem() after
	     *    allocating (KEEP_ROOM / 2) amount of memory. */
	    allocated += size;
	    if (allocated < KEEP_ROOM / 2)
		goto theend;
	    allocated = 0;

	    /* 3. check for available memory: call mch_avail_mem() */
	    if (mch_avail_mem(TRUE) < KEEP_ROOM_KB && !releasing)
	    {
		free(p);	/* System is low... no go! */
		p = NULL;
	    }
	    else
		goto theend;
#endif
	}
	/*
	 * Remember that mf_release_all() is being called to avoid an endless
	 * loop, because mf_release_all() may call alloc() recursively.
	 */
	if (releasing)
	    break;
	releasing = TRUE;

	clear_sb_text(TRUE);	      /* free any scrollback text */
	try_again = mf_release_all(); /* release as many blocks as possible */

	releasing = FALSE;
	if (!try_again)
	    break;
    }

    if (message && p == NULL)
	do_outofmem_msg(size);

theend:
#ifdef MEM_PROFILE
    mem_post_alloc(&p, size);
#endif
    return p;
}

/*
 * lalloc() with an ID for alloc_fail().
 */
#if defined(FEAT_SIGNS) || defined(PROTO)
    void *
lalloc_id(size_t size, int message, alloc_id_T id UNUSED)
{
#ifdef FEAT_EVAL
    if (alloc_fail_id == id && alloc_does_fail(size))
	return NULL;
#endif
    return (lalloc(size, message));
}
#endif

#if defined(MEM_PROFILE) || defined(PROTO)
/*
 * realloc() with memory profiling.
 */
    void *
mem_realloc(void *ptr, size_t size)
{
    void *p;

    mem_pre_free(&ptr);
    mem_pre_alloc_s(&size);

    p = realloc(ptr, size);

    mem_post_alloc(&p, size);

    return p;
}
#endif

/*
* Avoid repeating the error message many times (they take 1 second each).
* Did_outofmem_msg is reset when a character is read.
*/
    void
do_outofmem_msg(size_t size)
{
    if (!did_outofmem_msg)
    {
	/* Don't hide this message */
	emsg_silent = 0;

	/* Must come first to avoid coming back here when printing the error
	 * message fails, e.g. when setting v:errmsg. */
	did_outofmem_msg = TRUE;

	semsg(_("E342: Out of memory!  (allocating %lu bytes)"), (long_u)size);
    }
}

#if defined(EXITFREE) || defined(PROTO)

/*
 * Free everything that we allocated.
 * Can be used to detect memory leaks, e.g., with ccmalloc.
 * NOTE: This is tricky!  Things are freed that functions depend on.  Don't be
 * surprised if Vim crashes...
 * Some things can't be freed, esp. things local to a library function.
 */
    void
free_all_mem(void)
{
    buf_T	*buf, *nextbuf;

    /* When we cause a crash here it is caught and Vim tries to exit cleanly.
     * Don't try freeing everything again. */
    if (entered_free_all_mem)
	return;
    entered_free_all_mem = TRUE;

    /* Don't want to trigger autocommands from here on. */
    block_autocmds();

    /* Close all tabs and windows.  Reset 'equalalways' to avoid redraws. */
    p_ea = FALSE;
    if (first_tabpage != NULL && first_tabpage->tp_next != NULL)
	do_cmdline_cmd((char_u *)"tabonly!");
    if (!ONE_WINDOW)
	do_cmdline_cmd((char_u *)"only!");

# if defined(FEAT_SPELL)
    /* Free all spell info. */
    spell_free_all();
# endif

#if defined(FEAT_INS_EXPAND) && defined(FEAT_BEVAL_TERM)
    ui_remove_balloon();
# endif

    // Clear user commands (before deleting buffers).
    ex_comclear(NULL);

    // When exiting from mainerr_arg_missing curbuf has not been initialized,
    // and not much else.
    if (curbuf != NULL)
    {
	// Clear mappings, abbreviations, breakpoints.
	do_cmdline_cmd((char_u *)"lmapclear");
	do_cmdline_cmd((char_u *)"xmapclear");
	do_cmdline_cmd((char_u *)"mapclear");
	do_cmdline_cmd((char_u *)"mapclear!");
	do_cmdline_cmd((char_u *)"abclear");
# if defined(FEAT_EVAL)
	do_cmdline_cmd((char_u *)"breakdel *");
# endif
# if defined(FEAT_PROFILE)
	do_cmdline_cmd((char_u *)"profdel *");
# endif
# if defined(FEAT_KEYMAP)
	do_cmdline_cmd((char_u *)"set keymap=");
#endif
    }

# ifdef FEAT_TITLE
    free_titles();
# endif
# if defined(FEAT_SEARCHPATH)
    free_findfile();
# endif

    /* Obviously named calls. */
    free_all_autocmds();
    clear_termcodes();
    free_all_marks();
    alist_clear(&global_alist);
    free_homedir();
# if defined(FEAT_CMDL_COMPL)
    free_users();
# endif
    free_search_patterns();
    free_old_sub();
    free_last_insert();
# if defined(FEAT_INS_EXPAND)
    free_insexpand_stuff();
# endif
    free_prev_shellcmd();
    free_regexp_stuff();
    free_tag_stuff();
    free_cd_dir();
# ifdef FEAT_SIGNS
    free_signs();
# endif
# ifdef FEAT_EVAL
    set_expr_line(NULL);
# endif
# ifdef FEAT_DIFF
    if (curtab != NULL)
	diff_clear(curtab);
# endif
    clear_sb_text(TRUE);	      /* free any scrollback text */

    /* Free some global vars. */
    vim_free(username);
# ifdef FEAT_CLIPBOARD
    vim_regfree(clip_exclude_prog);
# endif
    vim_free(last_cmdline);
# ifdef FEAT_CMDHIST
    vim_free(new_last_cmdline);
# endif
    set_keep_msg(NULL, 0);

    /* Clear cmdline history. */
    p_hi = 0;
# ifdef FEAT_CMDHIST
    init_history();
# endif
#ifdef FEAT_TEXT_PROP
    clear_global_prop_types();
#endif

#ifdef FEAT_QUICKFIX
    {
	win_T	    *win;
	tabpage_T   *tab;

	qf_free_all(NULL);
	// Free all location lists
	FOR_ALL_TAB_WINDOWS(tab, win)
	    qf_free_all(win);
    }
#endif

    // Close all script inputs.
    close_all_scripts();

    if (curwin != NULL)
	// Destroy all windows.  Must come before freeing buffers.
	win_free_all();

    /* Free all option values.  Must come after closing windows. */
    free_all_options();

    /* Free all buffers.  Reset 'autochdir' to avoid accessing things that
     * were freed already. */
#ifdef FEAT_AUTOCHDIR
    p_acd = FALSE;
#endif
    for (buf = firstbuf; buf != NULL; )
    {
	bufref_T    bufref;

	set_bufref(&bufref, buf);
	nextbuf = buf->b_next;
	close_buffer(NULL, buf, DOBUF_WIPE, FALSE);
	if (bufref_valid(&bufref))
	    buf = nextbuf;	/* didn't work, try next one */
	else
	    buf = firstbuf;
    }

# ifdef FEAT_ARABIC
    free_cmdline_buf();
# endif

    /* Clear registers. */
    clear_registers();
    ResetRedobuff();
    ResetRedobuff();

    /* highlight info */
    free_highlight();

    reset_last_sourcing();

    if (first_tabpage != NULL)
    {
	free_tabpage(first_tabpage);
	first_tabpage = NULL;
    }

# ifdef UNIX
    /* Machine-specific free. */
    mch_free_mem();
# endif

    /* message history */
    for (;;)
	if (delete_first_msg() == FAIL)
	    break;

# ifdef FEAT_JOB_CHANNEL
    channel_free_all();
# endif
# ifdef FEAT_TIMERS
    timer_free_all();
# endif
# ifdef FEAT_EVAL
    /* must be after channel_free_all() with unrefs partials */
    eval_clear();
# endif
# ifdef FEAT_JOB_CHANNEL
    /* must be after eval_clear() with unrefs jobs */
    job_free_all();
# endif

    free_termoptions();

    /* screenlines (can't display anything now!) */
    free_screenlines();

# if defined(USE_XSMP)
    xsmp_close();
# endif
# ifdef FEAT_GUI_GTK
    gui_mch_free_all();
# endif
    clear_hl_tables();

    vim_free(IObuff);
    vim_free(NameBuff);
# ifdef FEAT_QUICKFIX
    check_quickfix_busy();
# endif
}
#endif

/*
 * Copy "string" into newly allocated memory.
 */
    char_u *
vim_strsave(char_u *string)
{
    char_u	*p;
    size_t	len;

    len = STRLEN(string) + 1;
    p = alloc(len);
    if (p != NULL)
	mch_memmove(p, string, len);
    return p;
}

/*
 * Copy up to "len" bytes of "string" into newly allocated memory and
 * terminate with a NUL.
 * The allocated memory always has size "len + 1", also when "string" is
 * shorter.
 */
    char_u *
vim_strnsave(char_u *string, int len)
{
    char_u	*p;

    p = alloc(len + 1);
    if (p != NULL)
    {
	STRNCPY(p, string, len);
	p[len] = NUL;
    }
    return p;
}

/*
 * Copy "p[len]" into allocated memory, ignoring NUL characters.
 * Returns NULL when out of memory.
 */
    char_u *
vim_memsave(char_u *p, size_t len)
{
    char_u *ret = alloc(len);

    if (ret != NULL)
	mch_memmove(ret, p, len);
    return ret;
}

/*
 * Same as vim_strsave(), but any characters found in esc_chars are preceded
 * by a backslash.
 */
    char_u *
vim_strsave_escaped(char_u *string, char_u *esc_chars)
{
    return vim_strsave_escaped_ext(string, esc_chars, '\\', FALSE);
}

/*
 * Same as vim_strsave_escaped(), but when "bsl" is TRUE also escape
 * characters where rem_backslash() would remove the backslash.
 * Escape the characters with "cc".
 */
    char_u *
vim_strsave_escaped_ext(
    char_u	*string,
    char_u	*esc_chars,
    int		cc,
    int		bsl)
{
    char_u	*p;
    char_u	*p2;
    char_u	*escaped_string;
    unsigned	length;
    int		l;

    /*
     * First count the number of backslashes required.
     * Then allocate the memory and insert them.
     */
    length = 1;				/* count the trailing NUL */
    for (p = string; *p; p++)
    {
	if (has_mbyte && (l = (*mb_ptr2len)(p)) > 1)
	{
	    length += l;		/* count a multibyte char */
	    p += l - 1;
	    continue;
	}
	if (vim_strchr(esc_chars, *p) != NULL || (bsl && rem_backslash(p)))
	    ++length;			/* count a backslash */
	++length;			/* count an ordinary char */
    }
    escaped_string = alloc(length);
    if (escaped_string != NULL)
    {
	p2 = escaped_string;
	for (p = string; *p; p++)
	{
	    if (has_mbyte && (l = (*mb_ptr2len)(p)) > 1)
	    {
		mch_memmove(p2, p, (size_t)l);
		p2 += l;
		p += l - 1;		/* skip multibyte char  */
		continue;
	    }
	    if (vim_strchr(esc_chars, *p) != NULL || (bsl && rem_backslash(p)))
		*p2++ = cc;
	    *p2++ = *p;
	}
	*p2 = NUL;
    }
    return escaped_string;
}

/*
 * Return TRUE when 'shell' has "csh" in the tail.
 */
    int
csh_like_shell(void)
{
    return (strstr((char *)gettail(p_sh), "csh") != NULL);
}

/*
 * Escape "string" for use as a shell argument with system().
 * This uses single quotes, except when we know we need to use double quotes
 * (MS-DOS and MS-Windows without 'shellslash' set).
 * Escape a newline, depending on the 'shell' option.
 * When "do_special" is TRUE also replace "!", "%", "#" and things starting
 * with "<" like "<cfile>".
 * When "do_newline" is FALSE do not escape newline unless it is csh shell.
 * Returns the result in allocated memory, NULL if we have run out.
 */
    char_u *
vim_strsave_shellescape(char_u *string, int do_special, int do_newline)
{
    unsigned	length;
    char_u	*p;
    char_u	*d;
    char_u	*escaped_string;
    int		l;
    int		csh_like;

    /* Only csh and similar shells expand '!' within single quotes.  For sh and
     * the like we must not put a backslash before it, it will be taken
     * literally.  If do_special is set the '!' will be escaped twice.
     * Csh also needs to have "\n" escaped twice when do_special is set. */
    csh_like = csh_like_shell();

    /* First count the number of extra bytes required. */
    length = (unsigned)STRLEN(string) + 3;  /* two quotes and a trailing NUL */
    for (p = string; *p != NUL; MB_PTR_ADV(p))
    {
# ifdef MSWIN
	if (!p_ssl)
	{
	    if (*p == '"')
		++length;		/* " -> "" */
	}
	else
# endif
	if (*p == '\'')
	    length += 3;		/* ' => '\'' */
	if ((*p == '\n' && (csh_like || do_newline))
		|| (*p == '!' && (csh_like || do_special)))
	{
	    ++length;			/* insert backslash */
	    if (csh_like && do_special)
		++length;		/* insert backslash */
	}
	if (do_special && find_cmdline_var(p, &l) >= 0)
	{
	    ++length;			/* insert backslash */
	    p += l - 1;
	}
    }

    /* Allocate memory for the result and fill it. */
    escaped_string = alloc(length);
    if (escaped_string != NULL)
    {
	d = escaped_string;

	/* add opening quote */
# ifdef MSWIN
	if (!p_ssl)
	    *d++ = '"';
	else
# endif
	    *d++ = '\'';

	for (p = string; *p != NUL; )
	{
# ifdef MSWIN
	    if (!p_ssl)
	    {
		if (*p == '"')
		{
		    *d++ = '"';
		    *d++ = '"';
		    ++p;
		    continue;
		}
	    }
	    else
# endif
	    if (*p == '\'')
	    {
		*d++ = '\'';
		*d++ = '\\';
		*d++ = '\'';
		*d++ = '\'';
		++p;
		continue;
	    }
	    if ((*p == '\n' && (csh_like || do_newline))
		    || (*p == '!' && (csh_like || do_special)))
	    {
		*d++ = '\\';
		if (csh_like && do_special)
		    *d++ = '\\';
		*d++ = *p++;
		continue;
	    }
	    if (do_special && find_cmdline_var(p, &l) >= 0)
	    {
		*d++ = '\\';		/* insert backslash */
		while (--l >= 0)	/* copy the var */
		    *d++ = *p++;
		continue;
	    }

	    MB_COPY_CHAR(p, d);
	}

	/* add terminating quote and finish with a NUL */
# ifdef MSWIN
	if (!p_ssl)
	    *d++ = '"';
	else
# endif
	    *d++ = '\'';
	*d = NUL;
    }

    return escaped_string;
}

/*
 * Like vim_strsave(), but make all characters uppercase.
 * This uses ASCII lower-to-upper case translation, language independent.
 */
    char_u *
vim_strsave_up(char_u *string)
{
    char_u *p1;

    p1 = vim_strsave(string);
    vim_strup(p1);
    return p1;
}

/*
 * Like vim_strnsave(), but make all characters uppercase.
 * This uses ASCII lower-to-upper case translation, language independent.
 */
    char_u *
vim_strnsave_up(char_u *string, int len)
{
    char_u *p1;

    p1 = vim_strnsave(string, len);
    vim_strup(p1);
    return p1;
}

/*
 * ASCII lower-to-upper case translation, language independent.
 */
    void
vim_strup(
    char_u	*p)
{
    char_u  *p2;
    int	    c;

    if (p != NULL)
    {
	p2 = p;
	while ((c = *p2) != NUL)
#ifdef EBCDIC
	    *p2++ = isalpha(c) ? toupper(c) : c;
#else
	    *p2++ = (c < 'a' || c > 'z') ? c : (c - 0x20);
#endif
    }
}

#if defined(FEAT_EVAL) || defined(FEAT_SPELL) || defined(PROTO)
/*
 * Make string "s" all upper-case and return it in allocated memory.
 * Handles multi-byte characters as well as possible.
 * Returns NULL when out of memory.
 */
    char_u *
strup_save(char_u *orig)
{
    char_u	*p;
    char_u	*res;

    res = p = vim_strsave(orig);

    if (res != NULL)
	while (*p != NUL)
	{
	    int		l;

	    if (enc_utf8)
	    {
		int	c, uc;
		int	newl;
		char_u	*s;

		c = utf_ptr2char(p);
		l = utf_ptr2len(p);
		if (c == 0)
		{
		    /* overlong sequence, use only the first byte */
		    c = *p;
		    l = 1;
		}
		uc = utf_toupper(c);

		/* Reallocate string when byte count changes.  This is rare,
		 * thus it's OK to do another malloc()/free(). */
		newl = utf_char2len(uc);
		if (newl != l)
		{
		    s = alloc(STRLEN(res) + 1 + newl - l);
		    if (s == NULL)
		    {
			vim_free(res);
			return NULL;
		    }
		    mch_memmove(s, res, p - res);
		    STRCPY(s + (p - res) + newl, p + l);
		    p = s + (p - res);
		    vim_free(res);
		    res = s;
		}

		utf_char2bytes(uc, p);
		p += newl;
	    }
	    else if (has_mbyte && (l = (*mb_ptr2len)(p)) > 1)
		p += l;		/* skip multi-byte character */
	    else
	    {
		*p = TOUPPER_LOC(*p); /* note that toupper() can be a macro */
		p++;
	    }
	}

    return res;
}

/*
 * Make string "s" all lower-case and return it in allocated memory.
 * Handles multi-byte characters as well as possible.
 * Returns NULL when out of memory.
 */
    char_u *
strlow_save(char_u *orig)
{
    char_u	*p;
    char_u	*res;

    res = p = vim_strsave(orig);

    if (res != NULL)
	while (*p != NUL)
	{
	    int		l;

	    if (enc_utf8)
	    {
		int	c, lc;
		int	newl;
		char_u	*s;

		c = utf_ptr2char(p);
		l = utf_ptr2len(p);
		if (c == 0)
		{
		    /* overlong sequence, use only the first byte */
		    c = *p;
		    l = 1;
		}
		lc = utf_tolower(c);

		/* Reallocate string when byte count changes.  This is rare,
		 * thus it's OK to do another malloc()/free(). */
		newl = utf_char2len(lc);
		if (newl != l)
		{
		    s = alloc(STRLEN(res) + 1 + newl - l);
		    if (s == NULL)
		    {
			vim_free(res);
			return NULL;
		    }
		    mch_memmove(s, res, p - res);
		    STRCPY(s + (p - res) + newl, p + l);
		    p = s + (p - res);
		    vim_free(res);
		    res = s;
		}

		utf_char2bytes(lc, p);
		p += newl;
	    }
	    else if (has_mbyte && (l = (*mb_ptr2len)(p)) > 1)
		p += l;		/* skip multi-byte character */
	    else
	    {
		*p = TOLOWER_LOC(*p); /* note that tolower() can be a macro */
		p++;
	    }
	}

    return res;
}
#endif

/*
 * delete spaces at the end of a string
 */
    void
del_trailing_spaces(char_u *ptr)
{
    char_u	*q;

    q = ptr + STRLEN(ptr);
    while (--q > ptr && VIM_ISWHITE(q[0]) && q[-1] != '\\' && q[-1] != Ctrl_V)
	*q = NUL;
}

/*
 * Like strncpy(), but always terminate the result with one NUL.
 * "to" must be "len + 1" long!
 */
    void
vim_strncpy(char_u *to, char_u *from, size_t len)
{
    STRNCPY(to, from, len);
    to[len] = NUL;
}

/*
 * Like strcat(), but make sure the result fits in "tosize" bytes and is
 * always NUL terminated. "from" and "to" may overlap.
 */
    void
vim_strcat(char_u *to, char_u *from, size_t tosize)
{
    size_t tolen = STRLEN(to);
    size_t fromlen = STRLEN(from);

    if (tolen + fromlen + 1 > tosize)
    {
	mch_memmove(to + tolen, from, tosize - tolen - 1);
	to[tosize - 1] = NUL;
    }
    else
	mch_memmove(to + tolen, from, fromlen + 1);
}

/*
 * Isolate one part of a string option where parts are separated with
 * "sep_chars".
 * The part is copied into "buf[maxlen]".
 * "*option" is advanced to the next part.
 * The length is returned.
 */
    int
copy_option_part(
    char_u	**option,
    char_u	*buf,
    int		maxlen,
    char	*sep_chars)
{
    int	    len = 0;
    char_u  *p = *option;

    /* skip '.' at start of option part, for 'suffixes' */
    if (*p == '.')
	buf[len++] = *p++;
    while (*p != NUL && vim_strchr((char_u *)sep_chars, *p) == NULL)
    {
	/*
	 * Skip backslash before a separator character and space.
	 */
	if (p[0] == '\\' && vim_strchr((char_u *)sep_chars, p[1]) != NULL)
	    ++p;
	if (len < maxlen - 1)
	    buf[len++] = *p;
	++p;
    }
    buf[len] = NUL;

    if (*p != NUL && *p != ',')	/* skip non-standard separator */
	++p;
    p = skip_to_option_part(p);	/* p points to next file name */

    *option = p;
    return len;
}

/*
 * Replacement for free() that ignores NULL pointers.
 * Also skip free() when exiting for sure, this helps when we caught a deadly
 * signal that was caused by a crash in free().
 * If you want to set NULL after calling this function, you should use
 * VIM_CLEAR() instead.
 */
    void
vim_free(void *x)
{
    if (x != NULL && !really_exiting)
    {
#ifdef MEM_PROFILE
	mem_pre_free(&x);
#endif
	free(x);
    }
}

#ifndef HAVE_MEMSET
    void *
vim_memset(void *ptr, int c, size_t size)
{
    char *p = ptr;

    while (size-- > 0)
	*p++ = c;
    return ptr;
}
#endif

#if (!defined(HAVE_STRCASECMP) && !defined(HAVE_STRICMP)) || defined(PROTO)
/*
 * Compare two strings, ignoring case, using current locale.
 * Doesn't work for multi-byte characters.
 * return 0 for match, < 0 for smaller, > 0 for bigger
 */
    int
vim_stricmp(char *s1, char *s2)
{
    int		i;

    for (;;)
    {
	i = (int)TOLOWER_LOC(*s1) - (int)TOLOWER_LOC(*s2);
	if (i != 0)
	    return i;			    /* this character different */
	if (*s1 == NUL)
	    break;			    /* strings match until NUL */
	++s1;
	++s2;
    }
    return 0;				    /* strings match */
}
#endif

#if (!defined(HAVE_STRNCASECMP) && !defined(HAVE_STRNICMP)) || defined(PROTO)
/*
 * Compare two strings, for length "len", ignoring case, using current locale.
 * Doesn't work for multi-byte characters.
 * return 0 for match, < 0 for smaller, > 0 for bigger
 */
    int
vim_strnicmp(char *s1, char *s2, size_t len)
{
    int		i;

    while (len > 0)
    {
	i = (int)TOLOWER_LOC(*s1) - (int)TOLOWER_LOC(*s2);
	if (i != 0)
	    return i;			    /* this character different */
	if (*s1 == NUL)
	    break;			    /* strings match until NUL */
	++s1;
	++s2;
	--len;
    }
    return 0;				    /* strings match */
}
#endif

/*
 * Version of strchr() and strrchr() that handle unsigned char strings
 * with characters from 128 to 255 correctly.  It also doesn't return a
 * pointer to the NUL at the end of the string.
 */
    char_u  *
vim_strchr(char_u *string, int c)
{
    char_u	*p;
    int		b;

    p = string;
    if (enc_utf8 && c >= 0x80)
    {
	while (*p != NUL)
	{
	    int l = utfc_ptr2len(p);

	    /* Avoid matching an illegal byte here. */
	    if (utf_ptr2char(p) == c && l > 1)
		return p;
	    p += l;
	}
	return NULL;
    }
    if (enc_dbcs != 0 && c > 255)
    {
	int	n2 = c & 0xff;

	c = ((unsigned)c >> 8) & 0xff;
	while ((b = *p) != NUL)
	{
	    if (b == c && p[1] == n2)
		return p;
	    p += (*mb_ptr2len)(p);
	}
	return NULL;
    }
    if (has_mbyte)
    {
	while ((b = *p) != NUL)
	{
	    if (b == c)
		return p;
	    p += (*mb_ptr2len)(p);
	}
	return NULL;
    }
    while ((b = *p) != NUL)
    {
	if (b == c)
	    return p;
	++p;
    }
    return NULL;
}

/*
 * Version of strchr() that only works for bytes and handles unsigned char
 * strings with characters above 128 correctly. It also doesn't return a
 * pointer to the NUL at the end of the string.
 */
    char_u  *
vim_strbyte(char_u *string, int c)
{
    char_u	*p = string;

    while (*p != NUL)
    {
	if (*p == c)
	    return p;
	++p;
    }
    return NULL;
}

/*
 * Search for last occurrence of "c" in "string".
 * Return NULL if not found.
 * Does not handle multi-byte char for "c"!
 */
    char_u  *
vim_strrchr(char_u *string, int c)
{
    char_u	*retval = NULL;
    char_u	*p = string;

    while (*p)
    {
	if (*p == c)
	    retval = p;
	MB_PTR_ADV(p);
    }
    return retval;
}

/*
 * Vim's version of strpbrk(), in case it's missing.
 * Don't generate a prototype for this, causes problems when it's not used.
 */
#ifndef PROTO
# ifndef HAVE_STRPBRK
#  ifdef vim_strpbrk
#   undef vim_strpbrk
#  endif
    char_u *
vim_strpbrk(char_u *s, char_u *charset)
{
    while (*s)
    {
	if (vim_strchr(charset, *s) != NULL)
	    return s;
	MB_PTR_ADV(s);
    }
    return NULL;
}
# endif
#endif

/*
 * Vim has its own isspace() function, because on some machines isspace()
 * can't handle characters above 128.
 */
    int
vim_isspace(int x)
{
    return ((x >= 9 && x <= 13) || x == ' ');
}

/************************************************************************
 * Functions for handling growing arrays.
 */

/*
 * Clear an allocated growing array.
 */
    void
ga_clear(garray_T *gap)
{
    vim_free(gap->ga_data);
    ga_init(gap);
}

/*
 * Clear a growing array that contains a list of strings.
 */
    void
ga_clear_strings(garray_T *gap)
{
    int		i;

    for (i = 0; i < gap->ga_len; ++i)
	vim_free(((char_u **)(gap->ga_data))[i]);
    ga_clear(gap);
}

/*
 * Initialize a growing array.	Don't forget to set ga_itemsize and
 * ga_growsize!  Or use ga_init2().
 */
    void
ga_init(garray_T *gap)
{
    gap->ga_data = NULL;
    gap->ga_maxlen = 0;
    gap->ga_len = 0;
}

    void
ga_init2(garray_T *gap, int itemsize, int growsize)
{
    ga_init(gap);
    gap->ga_itemsize = itemsize;
    gap->ga_growsize = growsize;
}

/*
 * Make room in growing array "gap" for at least "n" items.
 * Return FAIL for failure, OK otherwise.
 */
    int
ga_grow(garray_T *gap, int n)
{
    size_t	old_len;
    size_t	new_len;
    char_u	*pp;

    if (gap->ga_maxlen - gap->ga_len < n)
    {
	if (n < gap->ga_growsize)
	    n = gap->ga_growsize;

	// A linear growth is very inefficient when the array grows big.  This
	// is a compromise between allocating memory that won't be used and too
	// many copy operations. A factor of 1.5 seems reasonable.
	if (n < gap->ga_len / 2)
	    n = gap->ga_len / 2;

	new_len = gap->ga_itemsize * (gap->ga_len + n);
	pp = vim_realloc(gap->ga_data, new_len);
	if (pp == NULL)
	    return FAIL;
	old_len = gap->ga_itemsize * gap->ga_maxlen;
	vim_memset(pp + old_len, 0, new_len - old_len);
	gap->ga_maxlen = gap->ga_len + n;
	gap->ga_data = pp;
    }
    return OK;
}

#if defined(FEAT_EVAL) || defined(FEAT_SEARCHPATH) || defined(PROTO)
/*
 * For a growing array that contains a list of strings: concatenate all the
 * strings with a separating "sep".
 * Returns NULL when out of memory.
 */
    char_u *
ga_concat_strings(garray_T *gap, char *sep)
{
    int		i;
    int		len = 0;
    int		sep_len = (int)STRLEN(sep);
    char_u	*s;
    char_u	*p;

    for (i = 0; i < gap->ga_len; ++i)
	len += (int)STRLEN(((char_u **)(gap->ga_data))[i]) + sep_len;

    s = alloc(len + 1);
    if (s != NULL)
    {
	*s = NUL;
	p = s;
	for (i = 0; i < gap->ga_len; ++i)
	{
	    if (p != s)
	    {
		STRCPY(p, sep);
		p += sep_len;
	    }
	    STRCPY(p, ((char_u **)(gap->ga_data))[i]);
	    p += STRLEN(p);
	}
    }
    return s;
}
#endif

#if defined(FEAT_VIMINFO) || defined(FEAT_EVAL) || defined(PROTO)
/*
 * Make a copy of string "p" and add it to "gap".
 * When out of memory nothing changes.
 */
    void
ga_add_string(garray_T *gap, char_u *p)
{
    char_u *cp = vim_strsave(p);

    if (cp != NULL)
    {
	if (ga_grow(gap, 1) == OK)
	    ((char_u **)(gap->ga_data))[gap->ga_len++] = cp;
	else
	    vim_free(cp);
    }
}
#endif

/*
 * Concatenate a string to a growarray which contains bytes.
 * When "s" is NULL does not do anything.
 * Note: Does NOT copy the NUL at the end!
 */
    void
ga_concat(garray_T *gap, char_u *s)
{
    int    len;

    if (s == NULL || *s == NUL)
	return;
    len = (int)STRLEN(s);
    if (ga_grow(gap, len) == OK)
    {
	mch_memmove((char *)gap->ga_data + gap->ga_len, s, (size_t)len);
	gap->ga_len += len;
    }
}

/*
 * Append one byte to a growarray which contains bytes.
 */
    void
ga_append(garray_T *gap, int c)
{
    if (ga_grow(gap, 1) == OK)
    {
	*((char *)gap->ga_data + gap->ga_len) = c;
	++gap->ga_len;
    }
}

#if (defined(UNIX) && !defined(USE_SYSTEM)) || defined(MSWIN) \
	|| defined(PROTO)
/*
 * Append the text in "gap" below the cursor line and clear "gap".
 */
    void
append_ga_line(garray_T *gap)
{
    /* Remove trailing CR. */
    if (gap->ga_len > 0
	    && !curbuf->b_p_bin
	    && ((char_u *)gap->ga_data)[gap->ga_len - 1] == CAR)
	--gap->ga_len;
    ga_append(gap, NUL);
    ml_append(curwin->w_cursor.lnum++, gap->ga_data, 0, FALSE);
    gap->ga_len = 0;
}
#endif

/************************************************************************
 * functions that use lookup tables for various things, generally to do with
 * special key codes.
 */

/*
 * Some useful tables.
 */

static struct modmasktable
{
    short	mod_mask;	/* Bit-mask for particular key modifier */
    short	mod_flag;	/* Bit(s) for particular key modifier */
    char_u	name;		/* Single letter name of modifier */
} mod_mask_table[] =
{
    {MOD_MASK_ALT,		MOD_MASK_ALT,		(char_u)'M'},
    {MOD_MASK_META,		MOD_MASK_META,		(char_u)'T'},
    {MOD_MASK_CTRL,		MOD_MASK_CTRL,		(char_u)'C'},
    {MOD_MASK_SHIFT,		MOD_MASK_SHIFT,		(char_u)'S'},
    {MOD_MASK_MULTI_CLICK,	MOD_MASK_2CLICK,	(char_u)'2'},
    {MOD_MASK_MULTI_CLICK,	MOD_MASK_3CLICK,	(char_u)'3'},
    {MOD_MASK_MULTI_CLICK,	MOD_MASK_4CLICK,	(char_u)'4'},
#ifdef MACOS_X
    {MOD_MASK_CMD,		MOD_MASK_CMD,		(char_u)'D'},
#endif
    /* 'A' must be the last one */
    {MOD_MASK_ALT,		MOD_MASK_ALT,		(char_u)'A'},
    {0, 0, NUL}
    /* NOTE: when adding an entry, update MAX_KEY_NAME_LEN! */
};

/*
 * Shifted key terminal codes and their unshifted equivalent.
 * Don't add mouse codes here, they are handled separately!
 */
#define MOD_KEYS_ENTRY_SIZE 5

static char_u modifier_keys_table[] =
{
/*  mod mask	    with modifier		without modifier */
    MOD_MASK_SHIFT, '&', '9',			'@', '1',	/* begin */
    MOD_MASK_SHIFT, '&', '0',			'@', '2',	/* cancel */
    MOD_MASK_SHIFT, '*', '1',			'@', '4',	/* command */
    MOD_MASK_SHIFT, '*', '2',			'@', '5',	/* copy */
    MOD_MASK_SHIFT, '*', '3',			'@', '6',	/* create */
    MOD_MASK_SHIFT, '*', '4',			'k', 'D',	/* delete char */
    MOD_MASK_SHIFT, '*', '5',			'k', 'L',	/* delete line */
    MOD_MASK_SHIFT, '*', '7',			'@', '7',	/* end */
    MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_END,	'@', '7',	/* end */
    MOD_MASK_SHIFT, '*', '9',			'@', '9',	/* exit */
    MOD_MASK_SHIFT, '*', '0',			'@', '0',	/* find */
    MOD_MASK_SHIFT, '#', '1',			'%', '1',	/* help */
    MOD_MASK_SHIFT, '#', '2',			'k', 'h',	/* home */
    MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_HOME,	'k', 'h',	/* home */
    MOD_MASK_SHIFT, '#', '3',			'k', 'I',	/* insert */
    MOD_MASK_SHIFT, '#', '4',			'k', 'l',	/* left arrow */
    MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_LEFT,	'k', 'l',	/* left arrow */
    MOD_MASK_SHIFT, '%', 'a',			'%', '3',	/* message */
    MOD_MASK_SHIFT, '%', 'b',			'%', '4',	/* move */
    MOD_MASK_SHIFT, '%', 'c',			'%', '5',	/* next */
    MOD_MASK_SHIFT, '%', 'd',			'%', '7',	/* options */
    MOD_MASK_SHIFT, '%', 'e',			'%', '8',	/* previous */
    MOD_MASK_SHIFT, '%', 'f',			'%', '9',	/* print */
    MOD_MASK_SHIFT, '%', 'g',			'%', '0',	/* redo */
    MOD_MASK_SHIFT, '%', 'h',			'&', '3',	/* replace */
    MOD_MASK_SHIFT, '%', 'i',			'k', 'r',	/* right arr. */
    MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_RIGHT,	'k', 'r',	/* right arr. */
    MOD_MASK_SHIFT, '%', 'j',			'&', '5',	/* resume */
    MOD_MASK_SHIFT, '!', '1',			'&', '6',	/* save */
    MOD_MASK_SHIFT, '!', '2',			'&', '7',	/* suspend */
    MOD_MASK_SHIFT, '!', '3',			'&', '8',	/* undo */
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_UP,	'k', 'u',	/* up arrow */
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_DOWN,	'k', 'd',	/* down arrow */

								/* vt100 F1 */
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF1,	KS_EXTRA, (int)KE_XF1,
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF2,	KS_EXTRA, (int)KE_XF2,
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF3,	KS_EXTRA, (int)KE_XF3,
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF4,	KS_EXTRA, (int)KE_XF4,

    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F1,	'k', '1',	/* F1 */
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F2,	'k', '2',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F3,	'k', '3',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F4,	'k', '4',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F5,	'k', '5',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F6,	'k', '6',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F7,	'k', '7',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F8,	'k', '8',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F9,	'k', '9',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F10,	'k', ';',	/* F10 */

    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F11,	'F', '1',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F12,	'F', '2',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F13,	'F', '3',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F14,	'F', '4',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F15,	'F', '5',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F16,	'F', '6',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F17,	'F', '7',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F18,	'F', '8',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F19,	'F', '9',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F20,	'F', 'A',

    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F21,	'F', 'B',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F22,	'F', 'C',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F23,	'F', 'D',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F24,	'F', 'E',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F25,	'F', 'F',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F26,	'F', 'G',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F27,	'F', 'H',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F28,	'F', 'I',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F29,	'F', 'J',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F30,	'F', 'K',

    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F31,	'F', 'L',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F32,	'F', 'M',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F33,	'F', 'N',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F34,	'F', 'O',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F35,	'F', 'P',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F36,	'F', 'Q',
    MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F37,	'F', 'R',

							    /* TAB pseudo code*/
    MOD_MASK_SHIFT, 'k', 'B',			KS_EXTRA, (int)KE_TAB,

    NUL
};

static struct key_name_entry
{
    int	    key;	/* Special key code or ascii value */
    char_u  *name;	/* Name of key */
} key_names_table[] =
{
    {' ',		(char_u *)"Space"},
    {TAB,		(char_u *)"Tab"},
    {K_TAB,		(char_u *)"Tab"},
    {NL,		(char_u *)"NL"},
    {NL,		(char_u *)"NewLine"},	/* Alternative name */
    {NL,		(char_u *)"LineFeed"},	/* Alternative name */
    {NL,		(char_u *)"LF"},	/* Alternative name */
    {CAR,		(char_u *)"CR"},
    {CAR,		(char_u *)"Return"},	/* Alternative name */
    {CAR,		(char_u *)"Enter"},	/* Alternative name */
    {K_BS,		(char_u *)"BS"},
    {K_BS,		(char_u *)"BackSpace"},	/* Alternative name */
    {ESC,		(char_u *)"Esc"},
    {CSI,		(char_u *)"CSI"},
    {K_CSI,		(char_u *)"xCSI"},
    {'|',		(char_u *)"Bar"},
    {'\\',		(char_u *)"Bslash"},
    {K_DEL,		(char_u *)"Del"},
    {K_DEL,		(char_u *)"Delete"},	/* Alternative name */
    {K_KDEL,		(char_u *)"kDel"},
    {K_UP,		(char_u *)"Up"},
    {K_DOWN,		(char_u *)"Down"},
    {K_LEFT,		(char_u *)"Left"},
    {K_RIGHT,		(char_u *)"Right"},
    {K_XUP,		(char_u *)"xUp"},
    {K_XDOWN,		(char_u *)"xDown"},
    {K_XLEFT,		(char_u *)"xLeft"},
    {K_XRIGHT,		(char_u *)"xRight"},
    {K_PS,		(char_u *)"PasteStart"},
    {K_PE,		(char_u *)"PasteEnd"},

    {K_F1,		(char_u *)"F1"},
    {K_F2,		(char_u *)"F2"},
    {K_F3,		(char_u *)"F3"},
    {K_F4,		(char_u *)"F4"},
    {K_F5,		(char_u *)"F5"},
    {K_F6,		(char_u *)"F6"},
    {K_F7,		(char_u *)"F7"},
    {K_F8,		(char_u *)"F8"},
    {K_F9,		(char_u *)"F9"},
    {K_F10,		(char_u *)"F10"},

    {K_F11,		(char_u *)"F11"},
    {K_F12,		(char_u *)"F12"},
    {K_F13,		(char_u *)"F13"},
    {K_F14,		(char_u *)"F14"},
    {K_F15,		(char_u *)"F15"},
    {K_F16,		(char_u *)"F16"},
    {K_F17,		(char_u *)"F17"},
    {K_F18,		(char_u *)"F18"},
    {K_F19,		(char_u *)"F19"},
    {K_F20,		(char_u *)"F20"},

    {K_F21,		(char_u *)"F21"},
    {K_F22,		(char_u *)"F22"},
    {K_F23,		(char_u *)"F23"},
    {K_F24,		(char_u *)"F24"},
    {K_F25,		(char_u *)"F25"},
    {K_F26,		(char_u *)"F26"},
    {K_F27,		(char_u *)"F27"},
    {K_F28,		(char_u *)"F28"},
    {K_F29,		(char_u *)"F29"},
    {K_F30,		(char_u *)"F30"},

    {K_F31,		(char_u *)"F31"},
    {K_F32,		(char_u *)"F32"},
    {K_F33,		(char_u *)"F33"},
    {K_F34,		(char_u *)"F34"},
    {K_F35,		(char_u *)"F35"},
    {K_F36,		(char_u *)"F36"},
    {K_F37,		(char_u *)"F37"},

    {K_XF1,		(char_u *)"xF1"},
    {K_XF2,		(char_u *)"xF2"},
    {K_XF3,		(char_u *)"xF3"},
    {K_XF4,		(char_u *)"xF4"},

    {K_HELP,		(char_u *)"Help"},
    {K_UNDO,		(char_u *)"Undo"},
    {K_INS,		(char_u *)"Insert"},
    {K_INS,		(char_u *)"Ins"},	/* Alternative name */
    {K_KINS,		(char_u *)"kInsert"},
    {K_HOME,		(char_u *)"Home"},
    {K_KHOME,		(char_u *)"kHome"},
    {K_XHOME,		(char_u *)"xHome"},
    {K_ZHOME,		(char_u *)"zHome"},
    {K_END,		(char_u *)"End"},
    {K_KEND,		(char_u *)"kEnd"},
    {K_XEND,		(char_u *)"xEnd"},
    {K_ZEND,		(char_u *)"zEnd"},
    {K_PAGEUP,		(char_u *)"PageUp"},
    {K_PAGEDOWN,	(char_u *)"PageDown"},
    {K_KPAGEUP,		(char_u *)"kPageUp"},
    {K_KPAGEDOWN,	(char_u *)"kPageDown"},

    {K_KPLUS,		(char_u *)"kPlus"},
    {K_KMINUS,		(char_u *)"kMinus"},
    {K_KDIVIDE,		(char_u *)"kDivide"},
    {K_KMULTIPLY,	(char_u *)"kMultiply"},
    {K_KENTER,		(char_u *)"kEnter"},
    {K_KPOINT,		(char_u *)"kPoint"},

    {K_K0,		(char_u *)"k0"},
    {K_K1,		(char_u *)"k1"},
    {K_K2,		(char_u *)"k2"},
    {K_K3,		(char_u *)"k3"},
    {K_K4,		(char_u *)"k4"},
    {K_K5,		(char_u *)"k5"},
    {K_K6,		(char_u *)"k6"},
    {K_K7,		(char_u *)"k7"},
    {K_K8,		(char_u *)"k8"},
    {K_K9,		(char_u *)"k9"},

    {'<',		(char_u *)"lt"},

    {K_MOUSE,		(char_u *)"Mouse"},
    {K_SGR_MOUSE,	(char_u *)"SgrMouse"},
    {K_SGR_MOUSERELEASE, (char_u *)"SgrMouseRelelase"},
    {K_LEFTMOUSE,	(char_u *)"LeftMouse"},
    {K_LEFTMOUSE_NM,	(char_u *)"LeftMouseNM"},
    {K_LEFTDRAG,	(char_u *)"LeftDrag"},
    {K_LEFTRELEASE,	(char_u *)"LeftRelease"},
    {K_LEFTRELEASE_NM,	(char_u *)"LeftReleaseNM"},
    {K_MOUSEMOVE,	(char_u *)"MouseMove"},
    {K_MIDDLEMOUSE,	(char_u *)"MiddleMouse"},
    {K_MIDDLEDRAG,	(char_u *)"MiddleDrag"},
    {K_MIDDLERELEASE,	(char_u *)"MiddleRelease"},
    {K_RIGHTMOUSE,	(char_u *)"RightMouse"},
    {K_RIGHTDRAG,	(char_u *)"RightDrag"},
    {K_RIGHTRELEASE,	(char_u *)"RightRelease"},
    {K_MOUSEDOWN,	(char_u *)"ScrollWheelUp"},
    {K_MOUSEUP,		(char_u *)"ScrollWheelDown"},
    {K_MOUSELEFT,	(char_u *)"ScrollWheelRight"},
    {K_MOUSERIGHT,	(char_u *)"ScrollWheelLeft"},
    {K_MOUSEDOWN,	(char_u *)"MouseDown"}, /* OBSOLETE: Use	  */
    {K_MOUSEUP,		(char_u *)"MouseUp"},	/* ScrollWheelXXX instead */
    {K_X1MOUSE,		(char_u *)"X1Mouse"},
    {K_X1DRAG,		(char_u *)"X1Drag"},
    {K_X1RELEASE,		(char_u *)"X1Release"},
    {K_X2MOUSE,		(char_u *)"X2Mouse"},
    {K_X2DRAG,		(char_u *)"X2Drag"},
    {K_X2RELEASE,		(char_u *)"X2Release"},
    {K_DROP,		(char_u *)"Drop"},
    {K_ZERO,		(char_u *)"Nul"},
#ifdef FEAT_EVAL
    {K_SNR,		(char_u *)"SNR"},
#endif
    {K_PLUG,		(char_u *)"Plug"},
    {K_CURSORHOLD,	(char_u *)"CursorHold"},
    {K_IGNORE,		(char_u *)"Ignore"},
    {0,			NULL}
    /* NOTE: When adding a long name update MAX_KEY_NAME_LEN. */
};

#define KEY_NAMES_TABLE_LEN (sizeof(key_names_table) / sizeof(struct key_name_entry))

/*
 * Return the modifier mask bit (MOD_MASK_*) which corresponds to the given
 * modifier name ('S' for Shift, 'C' for Ctrl etc).
 */
    int
name_to_mod_mask(int c)
{
    int	    i;

    c = TOUPPER_ASC(c);
    for (i = 0; mod_mask_table[i].mod_mask != 0; i++)
	if (c == mod_mask_table[i].name)
	    return mod_mask_table[i].mod_flag;
    return 0;
}

/*
 * Check if if there is a special key code for "key" that includes the
 * modifiers specified.
 */
    int
simplify_key(int key, int *modifiers)
{
    int	    i;
    int	    key0;
    int	    key1;

    if (*modifiers & (MOD_MASK_SHIFT | MOD_MASK_CTRL | MOD_MASK_ALT))
    {
	/* TAB is a special case */
	if (key == TAB && (*modifiers & MOD_MASK_SHIFT))
	{
	    *modifiers &= ~MOD_MASK_SHIFT;
	    return K_S_TAB;
	}
	key0 = KEY2TERMCAP0(key);
	key1 = KEY2TERMCAP1(key);
	for (i = 0; modifier_keys_table[i] != NUL; i += MOD_KEYS_ENTRY_SIZE)
	    if (key0 == modifier_keys_table[i + 3]
		    && key1 == modifier_keys_table[i + 4]
		    && (*modifiers & modifier_keys_table[i]))
	    {
		*modifiers &= ~modifier_keys_table[i];
		return TERMCAP2KEY(modifier_keys_table[i + 1],
						   modifier_keys_table[i + 2]);
	    }
    }
    return key;
}

/*
 * Change <xHome> to <Home>, <xUp> to <Up>, etc.
 */
    int
handle_x_keys(int key)
{
    switch (key)
    {
	case K_XUP:	return K_UP;
	case K_XDOWN:	return K_DOWN;
	case K_XLEFT:	return K_LEFT;
	case K_XRIGHT:	return K_RIGHT;
	case K_XHOME:	return K_HOME;
	case K_ZHOME:	return K_HOME;
	case K_XEND:	return K_END;
	case K_ZEND:	return K_END;
	case K_XF1:	return K_F1;
	case K_XF2:	return K_F2;
	case K_XF3:	return K_F3;
	case K_XF4:	return K_F4;
	case K_S_XF1:	return K_S_F1;
	case K_S_XF2:	return K_S_F2;
	case K_S_XF3:	return K_S_F3;
	case K_S_XF4:	return K_S_F4;
    }
    return key;
}

/*
 * Return a string which contains the name of the given key when the given
 * modifiers are down.
 */
    char_u *
get_special_key_name(int c, int modifiers)
{
    static char_u string[MAX_KEY_NAME_LEN + 1];

    int	    i, idx;
    int	    table_idx;
    char_u  *s;

    string[0] = '<';
    idx = 1;

    /* Key that stands for a normal character. */
    if (IS_SPECIAL(c) && KEY2TERMCAP0(c) == KS_KEY)
	c = KEY2TERMCAP1(c);

    /*
     * Translate shifted special keys into unshifted keys and set modifier.
     * Same for CTRL and ALT modifiers.
     */
    if (IS_SPECIAL(c))
    {
	for (i = 0; modifier_keys_table[i] != 0; i += MOD_KEYS_ENTRY_SIZE)
	    if (       KEY2TERMCAP0(c) == (int)modifier_keys_table[i + 1]
		    && (int)KEY2TERMCAP1(c) == (int)modifier_keys_table[i + 2])
	    {
		modifiers |= modifier_keys_table[i];
		c = TERMCAP2KEY(modifier_keys_table[i + 3],
						   modifier_keys_table[i + 4]);
		break;
	    }
    }

    /* try to find the key in the special key table */
    table_idx = find_special_key_in_table(c);

    /*
     * When not a known special key, and not a printable character, try to
     * extract modifiers.
     */
    if (c > 0 && (*mb_char2len)(c) == 1)
    {
	if (table_idx < 0
		&& (!vim_isprintc(c) || (c & 0x7f) == ' ')
		&& (c & 0x80))
	{
	    c &= 0x7f;
	    modifiers |= MOD_MASK_ALT;
	    /* try again, to find the un-alted key in the special key table */
	    table_idx = find_special_key_in_table(c);
	}
	if (table_idx < 0 && !vim_isprintc(c) && c < ' ')
	{
#ifdef EBCDIC
	    c = CtrlChar(c);
#else
	    c += '@';
#endif
	    modifiers |= MOD_MASK_CTRL;
	}
    }

    /* translate the modifier into a string */
    for (i = 0; mod_mask_table[i].name != 'A'; i++)
	if ((modifiers & mod_mask_table[i].mod_mask)
						== mod_mask_table[i].mod_flag)
	{
	    string[idx++] = mod_mask_table[i].name;
	    string[idx++] = (char_u)'-';
	}

    if (table_idx < 0)		/* unknown special key, may output t_xx */
    {
	if (IS_SPECIAL(c))
	{
	    string[idx++] = 't';
	    string[idx++] = '_';
	    string[idx++] = KEY2TERMCAP0(c);
	    string[idx++] = KEY2TERMCAP1(c);
	}
	/* Not a special key, only modifiers, output directly */
	else
	{
	    if (has_mbyte && (*mb_char2len)(c) > 1)
		idx += (*mb_char2bytes)(c, string + idx);
	    else if (vim_isprintc(c))
		string[idx++] = c;
	    else
	    {
		s = transchar(c);
		while (*s)
		    string[idx++] = *s++;
	    }
	}
    }
    else		/* use name of special key */
    {
	size_t len = STRLEN(key_names_table[table_idx].name);

	if (len + idx + 2 <= MAX_KEY_NAME_LEN)
	{
	    STRCPY(string + idx, key_names_table[table_idx].name);
	    idx += (int)len;
	}
    }
    string[idx++] = '>';
    string[idx] = NUL;
    return string;
}

/*
 * Try translating a <> name at (*srcp)[] to dst[].
 * Return the number of characters added to dst[], zero for no match.
 * If there is a match, srcp is advanced to after the <> name.
 * dst[] must be big enough to hold the result (up to six characters)!
 */
    int
trans_special(
    char_u	**srcp,
    char_u	*dst,
    int		keycode,    // prefer key code, e.g. K_DEL instead of DEL
    int		in_string)  // TRUE when inside a double quoted string
{
    int		modifiers = 0;
    int		key;

    key = find_special_key(srcp, &modifiers, keycode, FALSE, in_string);
    if (key == 0)
	return 0;

    return special_to_buf(key, modifiers, keycode, dst);
}

/*
 * Put the character sequence for "key" with "modifiers" into "dst" and return
 * the resulting length.
 * When "keycode" is TRUE prefer key code, e.g. K_DEL instead of DEL.
 * The sequence is not NUL terminated.
 * This is how characters in a string are encoded.
 */
    int
special_to_buf(int key, int modifiers, int keycode, char_u *dst)
{
    int		dlen = 0;

    /* Put the appropriate modifier in a string */
    if (modifiers != 0)
    {
	dst[dlen++] = K_SPECIAL;
	dst[dlen++] = KS_MODIFIER;
	dst[dlen++] = modifiers;
    }

    if (IS_SPECIAL(key))
    {
	dst[dlen++] = K_SPECIAL;
	dst[dlen++] = KEY2TERMCAP0(key);
	dst[dlen++] = KEY2TERMCAP1(key);
    }
    else if (has_mbyte && !keycode)
	dlen += (*mb_char2bytes)(key, dst + dlen);
    else if (keycode)
	dlen = (int)(add_char2buf(key, dst + dlen) - dst);
    else
	dst[dlen++] = key;

    return dlen;
}

/*
 * Try translating a <> name at (*srcp)[], return the key and modifiers.
 * srcp is advanced to after the <> name.
 * returns 0 if there is no match.
 */
    int
find_special_key(
    char_u	**srcp,
    int		*modp,
    int		keycode,     /* prefer key code, e.g. K_DEL instead of DEL */
    int		keep_x_key,  /* don't translate xHome to Home key */
    int		in_string)   /* TRUE in string, double quote is escaped */
{
    char_u	*last_dash;
    char_u	*end_of_name;
    char_u	*src;
    char_u	*bp;
    int		modifiers;
    int		bit;
    int		key;
    uvarnumber_T	n;
    int		l;

    src = *srcp;
    if (src[0] != '<')
	return 0;

    /* Find end of modifier list */
    last_dash = src;
    for (bp = src + 1; *bp == '-' || vim_isIDc(*bp); bp++)
    {
	if (*bp == '-')
	{
	    last_dash = bp;
	    if (bp[1] != NUL)
	    {
		if (has_mbyte)
		    l = mb_ptr2len(bp + 1);
		else
		    l = 1;
		/* Anything accepted, like <C-?>.
		 * <C-"> or <M-"> are not special in strings as " is
		 * the string delimiter. With a backslash it works: <M-\"> */
		if (!(in_string && bp[1] == '"') && bp[2] == '>')
		    bp += l;
		else if (in_string && bp[1] == '\\' && bp[2] == '"'
							       && bp[3] == '>')
		    bp += 2;
	    }
	}
	if (bp[0] == 't' && bp[1] == '_' && bp[2] && bp[3])
	    bp += 3;	/* skip t_xx, xx may be '-' or '>' */
	else if (STRNICMP(bp, "char-", 5) == 0)
	{
	    vim_str2nr(bp + 5, NULL, &l, STR2NR_ALL, NULL, NULL, 0, TRUE);
	    if (l == 0)
	    {
		emsg(_(e_invarg));
		return 0;
	    }
	    bp += l + 5;
	    break;
	}
    }

    if (*bp == '>')	/* found matching '>' */
    {
	end_of_name = bp + 1;

	/* Which modifiers are given? */
	modifiers = 0x0;
	for (bp = src + 1; bp < last_dash; bp++)
	{
	    if (*bp != '-')
	    {
		bit = name_to_mod_mask(*bp);
		if (bit == 0x0)
		    break;	/* Illegal modifier name */
		modifiers |= bit;
	    }
	}

	/*
	 * Legal modifier name.
	 */
	if (bp >= last_dash)
	{
	    if (STRNICMP(last_dash + 1, "char-", 5) == 0
						 && VIM_ISDIGIT(last_dash[6]))
	    {
		/* <Char-123> or <Char-033> or <Char-0x33> */
		vim_str2nr(last_dash + 6, NULL, &l, STR2NR_ALL, NULL, &n, 0, TRUE);
		if (l == 0)
		{
		    emsg(_(e_invarg));
		    return 0;
		}
		key = (int)n;
	    }
	    else
	    {
		int off = 1;

		/* Modifier with single letter, or special key name.  */
		if (in_string && last_dash[1] == '\\' && last_dash[2] == '"')
		    off = 2;
		if (has_mbyte)
		    l = mb_ptr2len(last_dash + off);
		else
		    l = 1;
		if (modifiers != 0 && last_dash[l + off] == '>')
		    key = PTR2CHAR(last_dash + off);
		else
		{
		    key = get_special_key_code(last_dash + off);
		    if (!keep_x_key)
			key = handle_x_keys(key);
		}
	    }

	    /*
	     * get_special_key_code() may return NUL for invalid
	     * special key name.
	     */
	    if (key != NUL)
	    {
		/*
		 * Only use a modifier when there is no special key code that
		 * includes the modifier.
		 */
		key = simplify_key(key, &modifiers);

		if (!keycode)
		{
		    /* don't want keycode, use single byte code */
		    if (key == K_BS)
			key = BS;
		    else if (key == K_DEL || key == K_KDEL)
			key = DEL;
		}

		/*
		 * Normal Key with modifier: Try to make a single byte code.
		 */
		if (!IS_SPECIAL(key))
		    key = extract_modifiers(key, &modifiers);

		*modp = modifiers;
		*srcp = end_of_name;
		return key;
	    }
	}
    }
    return 0;
}

/*
 * Try to include modifiers in the key.
 * Changes "Shift-a" to 'A', "Alt-A" to 0xc0, etc.
 */
    int
extract_modifiers(int key, int *modp)
{
    int	modifiers = *modp;

#ifdef MACOS_X
    /* Command-key really special, no fancynest */
    if (!(modifiers & MOD_MASK_CMD))
#endif
    if ((modifiers & MOD_MASK_SHIFT) && ASCII_ISALPHA(key))
    {
	key = TOUPPER_ASC(key);
	modifiers &= ~MOD_MASK_SHIFT;
    }
    if ((modifiers & MOD_MASK_CTRL)
#ifdef EBCDIC
	    /* * TODO: EBCDIC Better use:
	     * && (Ctrl_chr(key) || key == '?')
	     * ???  */
	    && strchr("?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_", key)
						       != NULL
#else
	    && ((key >= '?' && key <= '_') || ASCII_ISALPHA(key))
#endif
	    )
    {
	key = Ctrl_chr(key);
	modifiers &= ~MOD_MASK_CTRL;
	/* <C-@> is <Nul> */
	if (key == 0)
	    key = K_ZERO;
    }
#ifdef MACOS_X
    /* Command-key really special, no fancynest */
    if (!(modifiers & MOD_MASK_CMD))
#endif
    if ((modifiers & MOD_MASK_ALT) && key < 0x80
	    && !enc_dbcs)		// avoid creating a lead byte
    {
	key |= 0x80;
	modifiers &= ~MOD_MASK_ALT;	/* remove the META modifier */
    }

    *modp = modifiers;
    return key;
}

/*
 * Try to find key "c" in the special key table.
 * Return the index when found, -1 when not found.
 */
    int
find_special_key_in_table(int c)
{
    int	    i;

    for (i = 0; key_names_table[i].name != NULL; i++)
	if (c == key_names_table[i].key)
	    break;
    if (key_names_table[i].name == NULL)
	i = -1;
    return i;
}

/*
 * Find the special key with the given name (the given string does not have to
 * end with NUL, the name is assumed to end before the first non-idchar).
 * If the name starts with "t_" the next two characters are interpreted as a
 * termcap name.
 * Return the key code, or 0 if not found.
 */
    int
get_special_key_code(char_u *name)
{
    char_u  *table_name;
    char_u  string[3];
    int	    i, j;

    /*
     * If it's <t_xx> we get the code for xx from the termcap
     */
    if (name[0] == 't' && name[1] == '_' && name[2] != NUL && name[3] != NUL)
    {
	string[0] = name[2];
	string[1] = name[3];
	string[2] = NUL;
	if (add_termcap_entry(string, FALSE) == OK)
	    return TERMCAP2KEY(name[2], name[3]);
    }
    else
	for (i = 0; key_names_table[i].name != NULL; i++)
	{
	    table_name = key_names_table[i].name;
	    for (j = 0; vim_isIDc(name[j]) && table_name[j] != NUL; j++)
		if (TOLOWER_ASC(table_name[j]) != TOLOWER_ASC(name[j]))
		    break;
	    if (!vim_isIDc(name[j]) && table_name[j] == NUL)
		return key_names_table[i].key;
	}
    return 0;
}

#if defined(FEAT_CMDL_COMPL) || defined(PROTO)
    char_u *
get_key_name(int i)
{
    if (i >= (int)KEY_NAMES_TABLE_LEN)
	return NULL;
    return  key_names_table[i].name;
}
#endif

/*
 * Return the current end-of-line type: EOL_DOS, EOL_UNIX or EOL_MAC.
 */
    int
get_fileformat(buf_T *buf)
{
    int		c = *buf->b_p_ff;

    if (buf->b_p_bin || c == 'u')
	return EOL_UNIX;
    if (c == 'm')
	return EOL_MAC;
    return EOL_DOS;
}

/*
 * Like get_fileformat(), but override 'fileformat' with "p" for "++opt=val"
 * argument.
 */
    int
get_fileformat_force(
    buf_T	*buf,
    exarg_T	*eap)	    /* can be NULL! */
{
    int		c;

    if (eap != NULL && eap->force_ff != 0)
	c = eap->force_ff;
    else
    {
	if ((eap != NULL && eap->force_bin != 0)
			       ? (eap->force_bin == FORCE_BIN) : buf->b_p_bin)
	    return EOL_UNIX;
	c = *buf->b_p_ff;
    }
    if (c == 'u')
	return EOL_UNIX;
    if (c == 'm')
	return EOL_MAC;
    return EOL_DOS;
}

/*
 * Set the current end-of-line type to EOL_DOS, EOL_UNIX or EOL_MAC.
 * Sets both 'textmode' and 'fileformat'.
 * Note: Does _not_ set global value of 'textmode'!
 */
    void
set_fileformat(
    int		t,
    int		opt_flags)	/* OPT_LOCAL and/or OPT_GLOBAL */
{
    char	*p = NULL;

    switch (t)
    {
    case EOL_DOS:
	p = FF_DOS;
	curbuf->b_p_tx = TRUE;
	break;
    case EOL_UNIX:
	p = FF_UNIX;
	curbuf->b_p_tx = FALSE;
	break;
    case EOL_MAC:
	p = FF_MAC;
	curbuf->b_p_tx = FALSE;
	break;
    }
    if (p != NULL)
	set_string_option_direct((char_u *)"ff", -1, (char_u *)p,
						     OPT_FREE | opt_flags, 0);

    /* This may cause the buffer to become (un)modified. */
    check_status(curbuf);
    redraw_tabline = TRUE;
}

/*
 * Return the default fileformat from 'fileformats'.
 */
    int
default_fileformat(void)
{
    switch (*p_ffs)
    {
	case 'm':   return EOL_MAC;
	case 'd':   return EOL_DOS;
    }
    return EOL_UNIX;
}

/*
 * Call shell.	Calls mch_call_shell, with 'shellxquote' added.
 */
    int
call_shell(char_u *cmd, int opt)
{
    char_u	*ncmd;
    int		retval;
#ifdef FEAT_PROFILE
    proftime_T	wait_time;
#endif

    if (p_verbose > 3)
    {
	verbose_enter();
	smsg(_("Calling shell to execute: \"%s\""),
						    cmd == NULL ? p_sh : cmd);
	out_char('\n');
	cursor_on();
	verbose_leave();
    }

#ifdef FEAT_PROFILE
    if (do_profiling == PROF_YES)
	prof_child_enter(&wait_time);
#endif

    if (*p_sh == NUL)
    {
	emsg(_(e_shellempty));
	retval = -1;
    }
    else
    {
#ifdef FEAT_GUI_MSWIN
	/* Don't hide the pointer while executing a shell command. */
	gui_mch_mousehide(FALSE);
#endif
#ifdef FEAT_GUI
	++hold_gui_events;
#endif
	/* The external command may update a tags file, clear cached tags. */
	tag_freematch();

	if (cmd == NULL || *p_sxq == NUL)
	    retval = mch_call_shell(cmd, opt);
	else
	{
	    char_u *ecmd = cmd;

	    if (*p_sxe != NUL && STRCMP(p_sxq, "(") == 0)
	    {
		ecmd = vim_strsave_escaped_ext(cmd, p_sxe, '^', FALSE);
		if (ecmd == NULL)
		    ecmd = cmd;
	    }
	    ncmd = alloc(STRLEN(ecmd) + STRLEN(p_sxq) * 2 + 1);
	    if (ncmd != NULL)
	    {
		STRCPY(ncmd, p_sxq);
		STRCAT(ncmd, ecmd);
		/* When 'shellxquote' is ( append ).
		 * When 'shellxquote' is "( append )". */
		STRCAT(ncmd, STRCMP(p_sxq, "(") == 0 ? (char_u *)")"
			   : STRCMP(p_sxq, "\"(") == 0 ? (char_u *)")\""
			   : p_sxq);
		retval = mch_call_shell(ncmd, opt);
		vim_free(ncmd);
	    }
	    else
		retval = -1;
	    if (ecmd != cmd)
		vim_free(ecmd);
	}
#ifdef FEAT_GUI
	--hold_gui_events;
#endif
	/*
	 * Check the window size, in case it changed while executing the
	 * external command.
	 */
	shell_resized_check();
    }

#ifdef FEAT_EVAL
    set_vim_var_nr(VV_SHELL_ERROR, (long)retval);
# ifdef FEAT_PROFILE
    if (do_profiling == PROF_YES)
	prof_child_exit(&wait_time);
# endif
#endif

    return retval;
}

/*
 * VISUAL, SELECTMODE and OP_PENDING State are never set, they are equal to
 * NORMAL State with a condition.  This function returns the real State.
 */
    int
get_real_state(void)
{
    if (State & NORMAL)
    {
	if (VIsual_active)
	{
	    if (VIsual_select)
		return SELECTMODE;
	    return VISUAL;
	}
	else if (finish_op)
	    return OP_PENDING;
    }
    return State;
}

/*
 * Return TRUE if "p" points to just after a path separator.
 * Takes care of multi-byte characters.
 * "b" must point to the start of the file name
 */
    int
after_pathsep(char_u *b, char_u *p)
{
    return p > b && vim_ispathsep(p[-1])
			     && (!has_mbyte || (*mb_head_off)(b, p - 1) == 0);
}

/*
 * Return TRUE if file names "f1" and "f2" are in the same directory.
 * "f1" may be a short name, "f2" must be a full path.
 */
    int
same_directory(char_u *f1, char_u *f2)
{
    char_u	ffname[MAXPATHL];
    char_u	*t1;
    char_u	*t2;

    /* safety check */
    if (f1 == NULL || f2 == NULL)
	return FALSE;

    (void)vim_FullName(f1, ffname, MAXPATHL, FALSE);
    t1 = gettail_sep(ffname);
    t2 = gettail_sep(f2);
    return (t1 - ffname == t2 - f2
	     && pathcmp((char *)ffname, (char *)f2, (int)(t1 - ffname)) == 0);
}

#if defined(FEAT_SESSION) || defined(FEAT_AUTOCHDIR) \
	|| defined(MSWIN) || defined(FEAT_GUI_MAC) || defined(FEAT_GUI_GTK) \
	|| defined(FEAT_NETBEANS_INTG) \
	|| defined(PROTO)
/*
 * Change to a file's directory.
 * Caller must call shorten_fnames()!
 * Return OK or FAIL.
 */
    int
vim_chdirfile(char_u *fname, char *trigger_autocmd)
{
    char_u	old_dir[MAXPATHL];
    char_u	new_dir[MAXPATHL];
    int		res;

    if (mch_dirname(old_dir, MAXPATHL) != OK)
	*old_dir = NUL;

    vim_strncpy(new_dir, fname, MAXPATHL - 1);
    *gettail_sep(new_dir) = NUL;

    if (pathcmp((char *)old_dir, (char *)new_dir, -1) == 0)
	// nothing to do
	res = OK;
    else
    {
	res = mch_chdir((char *)new_dir) == 0 ? OK : FAIL;

	if (res == OK && trigger_autocmd != NULL)
	    apply_autocmds(EVENT_DIRCHANGED, (char_u *)trigger_autocmd,
						       new_dir, FALSE, curbuf);
    }
    return res;
}
#endif

#if defined(STAT_IGNORES_SLASH) || defined(PROTO)
/*
 * Check if "name" ends in a slash and is not a directory.
 * Used for systems where stat() ignores a trailing slash on a file name.
 * The Vim code assumes a trailing slash is only ignored for a directory.
 */
    static int
illegal_slash(const char *name)
{
    if (name[0] == NUL)
	return FALSE;	    /* no file name is not illegal */
    if (name[strlen(name) - 1] != '/')
	return FALSE;	    /* no trailing slash */
    if (mch_isdir((char_u *)name))
	return FALSE;	    /* trailing slash for a directory */
    return TRUE;
}

/*
 * Special implementation of mch_stat() for Solaris.
 */
    int
vim_stat(const char *name, stat_T *stp)
{
    /* On Solaris stat() accepts "file/" as if it was "file".  Return -1 if
     * the name ends in "/" and it's not a directory. */
    return illegal_slash(name) ? -1 : stat(name, stp);
}
#endif

/*
 * Change directory to "new_dir".  If FEAT_SEARCHPATH is defined, search
 * 'cdpath' for relative directory names, otherwise just mch_chdir().
 */
    int
vim_chdir(char_u *new_dir)
{
#ifndef FEAT_SEARCHPATH
    return mch_chdir((char *)new_dir);
#else
    char_u	*dir_name;
    int		r;

    dir_name = find_directory_in_path(new_dir, (int)STRLEN(new_dir),
						FNAME_MESS, curbuf->b_ffname);
    if (dir_name == NULL)
	return -1;
    r = mch_chdir((char *)dir_name);
    vim_free(dir_name);
    return r;
#endif
}

/*
 * Get user name from machine-specific function.
 * Returns the user name in "buf[len]".
 * Some systems are quite slow in obtaining the user name (Windows NT), thus
 * cache the result.
 * Returns OK or FAIL.
 */
    int
get_user_name(char_u *buf, int len)
{
    if (username == NULL)
    {
	if (mch_get_user_name(buf, len) == FAIL)
	    return FAIL;
	username = vim_strsave(buf);
    }
    else
	vim_strncpy(buf, username, len - 1);
    return OK;
}

#ifndef HAVE_QSORT
/*
 * Our own qsort(), for systems that don't have it.
 * It's simple and slow.  From the K&R C book.
 */
    void
qsort(
    void	*base,
    size_t	elm_count,
    size_t	elm_size,
    int (*cmp)(const void *, const void *))
{
    char_u	*buf;
    char_u	*p1;
    char_u	*p2;
    int		i, j;
    int		gap;

    buf = alloc(elm_size);
    if (buf == NULL)
	return;

    for (gap = elm_count / 2; gap > 0; gap /= 2)
	for (i = gap; i < elm_count; ++i)
	    for (j = i - gap; j >= 0; j -= gap)
	    {
		/* Compare the elements. */
		p1 = (char_u *)base + j * elm_size;
		p2 = (char_u *)base + (j + gap) * elm_size;
		if ((*cmp)((void *)p1, (void *)p2) <= 0)
		    break;
		/* Exchange the elements. */
		mch_memmove(buf, p1, elm_size);
		mch_memmove(p1, p2, elm_size);
		mch_memmove(p2, buf, elm_size);
	    }

    vim_free(buf);
}
#endif

/*
 * Sort an array of strings.
 */
static int sort_compare(const void *s1, const void *s2);

    static int
sort_compare(const void *s1, const void *s2)
{
    return STRCMP(*(char **)s1, *(char **)s2);
}

    void
sort_strings(
    char_u	**files,
    int		count)
{
    qsort((void *)files, (size_t)count, sizeof(char_u *), sort_compare);
}

#if !defined(NO_EXPANDPATH) || defined(PROTO)
/*
 * Compare path "p[]" to "q[]".
 * If "maxlen" >= 0 compare "p[maxlen]" to "q[maxlen]"
 * Return value like strcmp(p, q), but consider path separators.
 */
    int
pathcmp(const char *p, const char *q, int maxlen)
{
    int		i, j;
    int		c1, c2;
    const char	*s = NULL;

    for (i = 0, j = 0; maxlen < 0 || (i < maxlen && j < maxlen);)
    {
	c1 = PTR2CHAR((char_u *)p + i);
	c2 = PTR2CHAR((char_u *)q + j);

	/* End of "p": check if "q" also ends or just has a slash. */
	if (c1 == NUL)
	{
	    if (c2 == NUL)  /* full match */
		return 0;
	    s = q;
	    i = j;
	    break;
	}

	/* End of "q": check if "p" just has a slash. */
	if (c2 == NUL)
	{
	    s = p;
	    break;
	}

	if ((p_fic ? MB_TOUPPER(c1) != MB_TOUPPER(c2) : c1 != c2)
#ifdef BACKSLASH_IN_FILENAME
		/* consider '/' and '\\' to be equal */
		&& !((c1 == '/' && c2 == '\\')
		    || (c1 == '\\' && c2 == '/'))
#endif
		)
	{
	    if (vim_ispathsep(c1))
		return -1;
	    if (vim_ispathsep(c2))
		return 1;
	    return p_fic ? MB_TOUPPER(c1) - MB_TOUPPER(c2)
		    : c1 - c2;  /* no match */
	}

	i += MB_PTR2LEN((char_u *)p + i);
	j += MB_PTR2LEN((char_u *)q + j);
    }
    if (s == NULL)	/* "i" or "j" ran into "maxlen" */
	return 0;

    c1 = PTR2CHAR((char_u *)s + i);
    c2 = PTR2CHAR((char_u *)s + i + MB_PTR2LEN((char_u *)s + i));
    /* ignore a trailing slash, but not "//" or ":/" */
    if (c2 == NUL
	    && i > 0
	    && !after_pathsep((char_u *)s, (char_u *)s + i)
#ifdef BACKSLASH_IN_FILENAME
	    && (c1 == '/' || c1 == '\\')
#else
	    && c1 == '/'
#endif
       )
	return 0;   /* match with trailing slash */
    if (s == q)
	return -1;	    /* no match */
    return 1;
}
#endif

/*
 * The putenv() implementation below comes from the "screen" program.
 * Included with permission from Juergen Weigert.
 * See pty.c for the copyright notice.
 */

/*
 *  putenv  --	put value into environment
 *
 *  Usage:  i = putenv (string)
 *    int i;
 *    char  *string;
 *
 *  where string is of the form <name>=<value>.
 *  Putenv returns 0 normally, -1 on error (not enough core for malloc).
 *
 *  Putenv may need to add a new name into the environment, or to
 *  associate a value longer than the current value with a particular
 *  name.  So, to make life simpler, putenv() copies your entire
 *  environment into the heap (i.e. malloc()) from the stack
 *  (i.e. where it resides when your process is initiated) the first
 *  time you call it.
 *
 *  (history removed, not very interesting.  See the "screen" sources.)
 */

#if !defined(HAVE_SETENV) && !defined(HAVE_PUTENV)

#define EXTRASIZE 5		/* increment to add to env. size */

static int  envsize = -1;	/* current size of environment */
extern char **environ;		/* the global which is your env. */

static int  findenv(char *name); /* look for a name in the env. */
static int  newenv(void);	/* copy env. from stack to heap */
static int  moreenv(void);	/* incr. size of env. */

    int
putenv(const char *string)
{
    int	    i;
    char    *p;

    if (envsize < 0)
    {				/* first time putenv called */
	if (newenv() < 0)	/* copy env. to heap */
	    return -1;
    }

    i = findenv((char *)string); /* look for name in environment */

    if (i < 0)
    {				/* name must be added */
	for (i = 0; environ[i]; i++);
	if (i >= (envsize - 1))
	{			/* need new slot */
	    if (moreenv() < 0)
		return -1;
	}
	p = alloc(strlen(string) + 1);
	if (p == NULL)		/* not enough core */
	    return -1;
	environ[i + 1] = 0;	/* new end of env. */
    }
    else
    {				/* name already in env. */
	p = vim_realloc(environ[i], strlen(string) + 1);
	if (p == NULL)
	    return -1;
    }
    sprintf(p, "%s", string);	/* copy into env. */
    environ[i] = p;

    return 0;
}

    static int
findenv(char *name)
{
    char    *namechar, *envchar;
    int	    i, found;

    found = 0;
    for (i = 0; environ[i] && !found; i++)
    {
	envchar = environ[i];
	namechar = name;
	while (*namechar && *namechar != '=' && (*namechar == *envchar))
	{
	    namechar++;
	    envchar++;
	}
	found = ((*namechar == '\0' || *namechar == '=') && *envchar == '=');
    }
    return found ? i - 1 : -1;
}

    static int
newenv(void)
{
    char    **env, *elem;
    int	    i, esize;

    for (i = 0; environ[i]; i++)
	;

    esize = i + EXTRASIZE + 1;
    env = ALLOC_MULT(char *, esize);
    if (env == NULL)
	return -1;

    for (i = 0; environ[i]; i++)
    {
	elem = alloc(strlen(environ[i]) + 1);
	if (elem == NULL)
	    return -1;
	env[i] = elem;
	strcpy(elem, environ[i]);
    }

    env[i] = 0;
    environ = env;
    envsize = esize;
    return 0;
}

    static int
moreenv(void)
{
    int	    esize;
    char    **env;

    esize = envsize + EXTRASIZE;
    env = vim_realloc((char *)environ, esize * sizeof (*env));
    if (env == 0)
	return -1;
    environ = env;
    envsize = esize;
    return 0;
}

# ifdef USE_VIMPTY_GETENV
/*
 * Used for mch_getenv() for Mac.
 */
    char_u *
vimpty_getenv(const char_u *string)
{
    int i;
    char_u *p;

    if (envsize < 0)
	return NULL;

    i = findenv((char *)string);

    if (i < 0)
	return NULL;

    p = vim_strchr((char_u *)environ[i], '=');
    return (p + 1);
}
# endif

#endif /* !defined(HAVE_SETENV) && !defined(HAVE_PUTENV) */

#if defined(FEAT_EVAL) || defined(FEAT_SPELL) || defined(PROTO)
/*
 * Return 0 for not writable, 1 for writable file, 2 for a dir which we have
 * rights to write into.
 */
    int
filewritable(char_u *fname)
{
    int		retval = 0;
#if defined(UNIX) || defined(VMS)
    int		perm = 0;
#endif

#if defined(UNIX) || defined(VMS)
    perm = mch_getperm(fname);
#endif
    if (
# ifdef MSWIN
	    mch_writable(fname) &&
# else
# if defined(UNIX) || defined(VMS)
	    (perm & 0222) &&
#  endif
# endif
	    mch_access((char *)fname, W_OK) == 0
       )
    {
	++retval;
	if (mch_isdir(fname))
	    ++retval;
    }
    return retval;
}
#endif

#if defined(FEAT_SPELL) || defined(FEAT_PERSISTENT_UNDO) || defined(PROTO)
/*
 * Read 2 bytes from "fd" and turn them into an int, MSB first.
 * Returns -1 when encountering EOF.
 */
    int
get2c(FILE *fd)
{
    int		c, n;

    n = getc(fd);
    if (n == EOF) return -1;
    c = getc(fd);
    if (c == EOF) return -1;
    return (n << 8) + c;
}

/*
 * Read 3 bytes from "fd" and turn them into an int, MSB first.
 * Returns -1 when encountering EOF.
 */
    int
get3c(FILE *fd)
{
    int		c, n;

    n = getc(fd);
    if (n == EOF) return -1;
    c = getc(fd);
    if (c == EOF) return -1;
    n = (n << 8) + c;
    c = getc(fd);
    if (c == EOF) return -1;
    return (n << 8) + c;
}

/*
 * Read 4 bytes from "fd" and turn them into an int, MSB first.
 * Returns -1 when encountering EOF.
 */
    int
get4c(FILE *fd)
{
    int		c;
    /* Use unsigned rather than int otherwise result is undefined
     * when left-shift sets the MSB. */
    unsigned	n;

    c = getc(fd);
    if (c == EOF) return -1;
    n = (unsigned)c;
    c = getc(fd);
    if (c == EOF) return -1;
    n = (n << 8) + (unsigned)c;
    c = getc(fd);
    if (c == EOF) return -1;
    n = (n << 8) + (unsigned)c;
    c = getc(fd);
    if (c == EOF) return -1;
    n = (n << 8) + (unsigned)c;
    return (int)n;
}

/*
 * Read 8 bytes from "fd" and turn them into a time_T, MSB first.
 * Returns -1 when encountering EOF.
 */
    time_T
get8ctime(FILE *fd)
{
    int		c;
    time_T	n = 0;
    int		i;

    for (i = 0; i < 8; ++i)
    {
	c = getc(fd);
	if (c == EOF) return -1;
	n = (n << 8) + c;
    }
    return n;
}

/*
 * Read a string of length "cnt" from "fd" into allocated memory.
 * Returns NULL when out of memory or unable to read that many bytes.
 */
    char_u *
read_string(FILE *fd, int cnt)
{
    char_u	*str;
    int		i;
    int		c;

    /* allocate memory */
    str = alloc(cnt + 1);
    if (str != NULL)
    {
	/* Read the string.  Quit when running into the EOF. */
	for (i = 0; i < cnt; ++i)
	{
	    c = getc(fd);
	    if (c == EOF)
	    {
		vim_free(str);
		return NULL;
	    }
	    str[i] = c;
	}
	str[i] = NUL;
    }
    return str;
}

/*
 * Write a number to file "fd", MSB first, in "len" bytes.
 */
    int
put_bytes(FILE *fd, long_u nr, int len)
{
    int	    i;

    for (i = len - 1; i >= 0; --i)
	if (putc((int)(nr >> (i * 8)), fd) == EOF)
	    return FAIL;
    return OK;
}

#ifdef _MSC_VER
# if (_MSC_VER <= 1200)
/* This line is required for VC6 without the service pack.  Also see the
 * matching #pragma below. */
 #  pragma optimize("", off)
# endif
#endif

/*
 * Write time_T to file "fd" in 8 bytes.
 * Returns FAIL when the write failed.
 */
    int
put_time(FILE *fd, time_T the_time)
{
    char_u	buf[8];

    time_to_bytes(the_time, buf);
    return fwrite(buf, (size_t)8, (size_t)1, fd) == 1 ? OK : FAIL;
}

/*
 * Write time_T to "buf[8]".
 */
    void
time_to_bytes(time_T the_time, char_u *buf)
{
    int		c;
    int		i;
    int		bi = 0;
    time_T	wtime = the_time;

    /* time_T can be up to 8 bytes in size, more than long_u, thus we
     * can't use put_bytes() here.
     * Another problem is that ">>" may do an arithmetic shift that keeps the
     * sign.  This happens for large values of wtime.  A cast to long_u may
     * truncate if time_T is 8 bytes.  So only use a cast when it is 4 bytes,
     * it's safe to assume that long_u is 4 bytes or more and when using 8
     * bytes the top bit won't be set. */
    for (i = 7; i >= 0; --i)
    {
	if (i + 1 > (int)sizeof(time_T))
	    /* ">>" doesn't work well when shifting more bits than avail */
	    buf[bi++] = 0;
	else
	{
#if defined(SIZEOF_TIME_T) && SIZEOF_TIME_T > 4
	    c = (int)(wtime >> (i * 8));
#else
	    c = (int)((long_u)wtime >> (i * 8));
#endif
	    buf[bi++] = c;
	}
    }
}

#ifdef _MSC_VER
# if (_MSC_VER <= 1200)
 #  pragma optimize("", on)
# endif
#endif

#endif

#if defined(FEAT_QUICKFIX) || defined(FEAT_SPELL) || defined(PROTO)
/*
 * Return TRUE if string "s" contains a non-ASCII character (128 or higher).
 * When "s" is NULL FALSE is returned.
 */
    int
has_non_ascii(char_u *s)
{
    char_u	*p;

    if (s != NULL)
	for (p = s; *p != NUL; ++p)
	    if (*p >= 128)
		return TRUE;
    return FALSE;
}
#endif

#if defined(MESSAGE_QUEUE) || defined(PROTO)
# define MAX_REPEAT_PARSE 8

/*
 * Process messages that have been queued for netbeans or clientserver.
 * Also check if any jobs have ended.
 * These functions can call arbitrary vimscript and should only be called when
 * it is safe to do so.
 */
    void
parse_queued_messages(void)
{
    win_T   *old_curwin = curwin;
    int	    i;

    // Do not handle messages while redrawing, because it may cause buffers to
    // change or be wiped while they are being redrawn.
    if (updating_screen)
	return;

    // Loop when a job ended, but don't keep looping forever.
    for (i = 0; i < MAX_REPEAT_PARSE; ++i)
    {
	// For Win32 mch_breakcheck() does not check for input, do it here.
# if defined(MSWIN) && defined(FEAT_JOB_CHANNEL)
	channel_handle_events(FALSE);
# endif

# ifdef FEAT_NETBEANS_INTG
	// Process the queued netbeans messages.
	netbeans_parse_messages();
# endif
# ifdef FEAT_JOB_CHANNEL
	// Write any buffer lines still to be written.
	channel_write_any_lines();

	// Process the messages queued on channels.
	channel_parse_messages();
# endif
# ifdef FEAT_JOB_CHANNEL
	// Check if any jobs have ended.  If so, repeat the above to handle
	// changes, e.g. stdin may have been closed.
	if (job_check_ended())
	    continue;
# endif
# ifdef FEAT_TERMINAL
	free_unused_terminals();
# endif
	break;
    }

    // If the current window changed we need to bail out of the waiting loop.
    // E.g. when a job exit callback closes the terminal window.
    if (curwin != old_curwin)
	ins_char_typebuf(K_IGNORE);
}
#endif

#ifndef PROTO  /* proto is defined in vim.h */
# ifdef ELAPSED_TIMEVAL
/*
 * Return time in msec since "start_tv".
 */
    long
elapsed(struct timeval *start_tv)
{
    struct timeval  now_tv;

    gettimeofday(&now_tv, NULL);
    return (now_tv.tv_sec - start_tv->tv_sec) * 1000L
	 + (now_tv.tv_usec - start_tv->tv_usec) / 1000L;
}
# endif

# ifdef ELAPSED_TICKCOUNT
/*
 * Return time in msec since "start_tick".
 */
    long
elapsed(DWORD start_tick)
{
    DWORD	now = GetTickCount();

    return (long)now - (long)start_tick;
}
# endif
#endif

#if defined(FEAT_JOB_CHANNEL) \
	|| (defined(UNIX) && (!defined(USE_SYSTEM) \
	|| (defined(FEAT_GUI) && defined(FEAT_TERMINAL)))) \
	|| defined(PROTO)
/*
 * Parse "cmd" and put the white-separated parts in "argv".
 * "argv" is an allocated array with "argc" entries and room for 4 more.
 * Returns FAIL when out of memory.
 */
    int
mch_parse_cmd(char_u *cmd, int use_shcf, char ***argv, int *argc)
{
    int		i;
    char_u	*p, *d;
    int		inquote;

    /*
     * Do this loop twice:
     * 1: find number of arguments
     * 2: separate them and build argv[]
     */
    for (i = 0; i < 2; ++i)
    {
	p = skipwhite(cmd);
	inquote = FALSE;
	*argc = 0;
	for (;;)
	{
	    if (i == 1)
		(*argv)[*argc] = (char *)p;
	    ++*argc;
	    d = p;
	    while (*p != NUL && (inquote || (*p != ' ' && *p != TAB)))
	    {
		if (p[0] == '"')
		    // quotes surrounding an argument and are dropped
		    inquote = !inquote;
		else
		{
		    if (rem_backslash(p))
		    {
			// First pass: skip over "\ " and "\"".
			// Second pass: Remove the backslash.
			++p;
		    }
		    if (i == 1)
			*d++ = *p;
		}
		++p;
	    }
	    if (*p == NUL)
	    {
		if (i == 1)
		    *d++ = NUL;
		break;
	    }
	    if (i == 1)
		*d++ = NUL;
	    p = skipwhite(p + 1);
	}
	if (*argv == NULL)
	{
	    if (use_shcf)
	    {
		/* Account for possible multiple args in p_shcf. */
		p = p_shcf;
		for (;;)
		{
		    p = skiptowhite(p);
		    if (*p == NUL)
			break;
		    ++*argc;
		    p = skipwhite(p);
		}
	    }

	    *argv = ALLOC_MULT(char *, *argc + 4);
	    if (*argv == NULL)	    /* out of memory */
		return FAIL;
	}
    }
    return OK;
}

# if defined(FEAT_JOB_CHANNEL) || defined(PROTO)
/*
 * Build "argv[argc]" from the string "cmd".
 * "argv[argc]" is set to NULL;
 * Return FAIL when out of memory.
 */
    int
build_argv_from_string(char_u *cmd, char ***argv, int *argc)
{
    char_u	*cmd_copy;
    int		i;

    /* Make a copy, parsing will modify "cmd". */
    cmd_copy = vim_strsave(cmd);
    if (cmd_copy == NULL
	    || mch_parse_cmd(cmd_copy, FALSE, argv, argc) == FAIL)
    {
	vim_free(cmd_copy);
	return FAIL;
    }
    for (i = 0; i < *argc; i++)
	(*argv)[i] = (char *)vim_strsave((char_u *)(*argv)[i]);
    (*argv)[*argc] = NULL;
    vim_free(cmd_copy);
    return OK;
}

/*
 * Build "argv[argc]" from the list "l".
 * "argv[argc]" is set to NULL;
 * Return FAIL when out of memory.
 */
    int
build_argv_from_list(list_T *l, char ***argv, int *argc)
{
    listitem_T  *li;
    char_u	*s;

    /* Pass argv[] to mch_call_shell(). */
    *argv = ALLOC_MULT(char *, l->lv_len + 1);
    if (*argv == NULL)
	return FAIL;
    *argc = 0;
    for (li = l->lv_first; li != NULL; li = li->li_next)
    {
	s = tv_get_string_chk(&li->li_tv);
	if (s == NULL)
	{
	    int i;

	    for (i = 0; i < *argc; ++i)
		vim_free((*argv)[i]);
	    return FAIL;
	}
	(*argv)[*argc] = (char *)vim_strsave(s);
	*argc += 1;
    }
    (*argv)[*argc] = NULL;
    return OK;
}
# endif
#endif

#if defined(FEAT_SESSION) || defined(PROTO)
/*
 * Generate a script that can be used to restore the current editing session.
 * Save the value of v:this_session before running :mksession in order to make
 * automagic session save fully transparent.  Return TRUE on success.
 */
    int
write_session_file(char_u *filename)
{
    char_u	    *escaped_filename;
    char	    *mksession_cmdline;
    unsigned int    save_ssop_flags;
    int		    failed;

    /*
     * Build an ex command line to create a script that restores the current
     * session if executed.  Escape the filename to avoid nasty surprises.
     */
    escaped_filename = vim_strsave_escaped(filename, escape_chars);
    if (escaped_filename == NULL)
	return FALSE;
    mksession_cmdline = alloc(10 + (int)STRLEN(escaped_filename) + 1);
    if (mksession_cmdline == NULL)
    {
	vim_free(escaped_filename);
	return FALSE;
    }
    strcpy(mksession_cmdline, "mksession ");
    STRCAT(mksession_cmdline, escaped_filename);
    vim_free(escaped_filename);

    /*
     * Use a reasonable hardcoded set of 'sessionoptions' flags to avoid
     * unpredictable effects when the session is saved automatically.  Also,
     * we definitely need SSOP_GLOBALS to be able to restore v:this_session.
     * Don't use SSOP_BUFFERS to prevent the buffer list from becoming
     * enormously large if the GNOME session feature is used regularly.
     */
    save_ssop_flags = ssop_flags;
    ssop_flags = (SSOP_BLANK|SSOP_CURDIR|SSOP_FOLDS|SSOP_GLOBALS
		  |SSOP_HELP|SSOP_OPTIONS|SSOP_WINSIZE|SSOP_TABPAGES);

    do_cmdline_cmd((char_u *)"let Save_VV_this_session = v:this_session");
    failed = (do_cmdline_cmd((char_u *)mksession_cmdline) == FAIL);
    do_cmdline_cmd((char_u *)"let v:this_session = Save_VV_this_session");
    do_unlet((char_u *)"Save_VV_this_session", TRUE);

    ssop_flags = save_ssop_flags;
    vim_free(mksession_cmdline);

    /*
     * Reopen the file and append a command to restore v:this_session,
     * as if this save never happened.	This is to avoid conflicts with
     * the user's own sessions.  FIXME: It's probably less hackish to add
     * a "stealth" flag to 'sessionoptions' -- gotta ask Bram.
     */
    if (!failed)
    {
	FILE *fd;

	fd = open_exfile(filename, TRUE, APPENDBIN);

	failed = (fd == NULL
	       || put_line(fd, "let v:this_session = Save_VV_this_session") == FAIL
	       || put_line(fd, "unlet Save_VV_this_session") == FAIL);

	if (fd != NULL && fclose(fd) != 0)
	    failed = TRUE;

	if (failed)
	    mch_remove(filename);
    }

    return !failed;
}
#endif
