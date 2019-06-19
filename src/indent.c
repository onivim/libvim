/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * indent.c: Indentation related functions
 */

#include "vim.h"

#if defined(FEAT_SMARTINDENT)

/*
 * Return TRUE if the string "line" starts with a word from 'cinwords'.
 */
    int
cin_is_cinword(char_u *line)
{
    char_u	*cinw;
    char_u	*cinw_buf;
    int		cinw_len;
    int		retval = FALSE;
    int		len;

    cinw_len = (int)STRLEN(curbuf->b_p_cinw) + 1;
    cinw_buf = alloc(cinw_len);
    if (cinw_buf != NULL)
    {
	line = skipwhite(line);
	for (cinw = curbuf->b_p_cinw; *cinw; )
	{
	    len = copy_option_part(&cinw, cinw_buf, cinw_len, ",");
	    if (STRNCMP(line, cinw_buf, len) == 0
		    && (!vim_iswordc(line[len]) || !vim_iswordc(line[len - 1])))
	    {
		retval = TRUE;
		break;
	    }
	}
	vim_free(cinw_buf);
    }
    return retval;
}
#endif

#if defined(FEAT_SYN_HL)

static char_u	*skip_string(char_u *p);
static pos_T *find_start_rawstring(int ind_maxcomment);

/*
 * Find the start of a comment, not knowing if we are in a comment right now.
 * Search starts at w_cursor.lnum and goes backwards.
 * Return NULL when not inside a comment.
 */
    static pos_T *
ind_find_start_comment(void)	    // XXX
{
    return find_start_comment(curbuf->b_ind_maxcomment);
}

    pos_T *
find_start_comment(int ind_maxcomment)	// XXX
{
    pos_T	*pos;
    char_u	*line;
    char_u	*p;
    int		cur_maxcomment = ind_maxcomment;

    for (;;)
    {
	pos = findmatchlimit(NULL, '*', FM_BACKWARD, cur_maxcomment);
	if (pos == NULL)
	    break;

	// Check if the comment start we found is inside a string.
	// If it is then restrict the search to below this line and try again.
	line = ml_get(pos->lnum);
	for (p = line; *p && (colnr_T)(p - line) < pos->col; ++p)
	    p = skip_string(p);
	if ((colnr_T)(p - line) <= pos->col)
	    break;
	cur_maxcomment = curwin->w_cursor.lnum - pos->lnum - 1;
	if (cur_maxcomment <= 0)
	{
	    pos = NULL;
	    break;
	}
    }
    return pos;
}

/*
 * Find the start of a comment or raw string, not knowing if we are in a
 * comment or raw string right now.
 * Search starts at w_cursor.lnum and goes backwards.
 * If is_raw is given and returns start of raw_string, sets it to true.
 * Return NULL when not inside a comment or raw string.
 * "CORS" -> Comment Or Raw String
 */
    static pos_T *
ind_find_start_CORS(linenr_T *is_raw)	    // XXX
{
    static pos_T comment_pos_copy;
    pos_T	*comment_pos;
    pos_T	*rs_pos;

    comment_pos = find_start_comment(curbuf->b_ind_maxcomment);
    if (comment_pos != NULL)
    {
	// Need to make a copy of the static pos in findmatchlimit(),
	// calling find_start_rawstring() may change it.
	comment_pos_copy = *comment_pos;
	comment_pos = &comment_pos_copy;
    }
    rs_pos = find_start_rawstring(curbuf->b_ind_maxcomment);

    // If comment_pos is before rs_pos the raw string is inside the comment.
    // If rs_pos is before comment_pos the comment is inside the raw string.
    if (comment_pos == NULL || (rs_pos != NULL
					     && LT_POS(*rs_pos, *comment_pos)))
    {
	if (is_raw != NULL && rs_pos != NULL)
	    *is_raw = rs_pos->lnum;
	return rs_pos;
    }
    return comment_pos;
}

/*
 * Find the start of a raw string, not knowing if we are in one right now.
 * Search starts at w_cursor.lnum and goes backwards.
 * Return NULL when not inside a raw string.
 */
    static pos_T *
find_start_rawstring(int ind_maxcomment)	// XXX
{
    pos_T	*pos;
    char_u	*line;
    char_u	*p;
    int		cur_maxcomment = ind_maxcomment;

    for (;;)
    {
	pos = findmatchlimit(NULL, 'R', FM_BACKWARD, cur_maxcomment);
	if (pos == NULL)
	    break;

	// Check if the raw string start we found is inside a string.
	// If it is then restrict the search to below this line and try again.
	line = ml_get(pos->lnum);
	for (p = line; *p && (colnr_T)(p - line) < pos->col; ++p)
	    p = skip_string(p);
	if ((colnr_T)(p - line) <= pos->col)
	    break;
	cur_maxcomment = curwin->w_cursor.lnum - pos->lnum - 1;
	if (cur_maxcomment <= 0)
	{
	    pos = NULL;
	    break;
	}
    }
    return pos;
}

/*
 * Skip to the end of a "string" and a 'c' character.
 * If there is no string or character, return argument unmodified.
 */
    static char_u *
skip_string(char_u *p)
{
    int	    i;

    // We loop, because strings may be concatenated: "date""time".
    for ( ; ; ++p)
    {
	if (p[0] == '\'')		    // 'c' or '\n' or '\000'
	{
	    if (!p[1])			    // ' at end of line
		break;
	    i = 2;
	    if (p[1] == '\\')		    // '\n' or '\000'
	    {
		++i;
		while (vim_isdigit(p[i - 1]))   // '\000'
		    ++i;
	    }
	    if (p[i] == '\'')		    // check for trailing '
	    {
		p += i;
		continue;
	    }
	}
	else if (p[0] == '"')		    // start of string
	{
	    for (++p; p[0]; ++p)
	    {
		if (p[0] == '\\' && p[1] != NUL)
		    ++p;
		else if (p[0] == '"')	    // end of string
		    break;
	    }
	    if (p[0] == '"')
		continue; // continue for another string
	}
	else if (p[0] == 'R' && p[1] == '"')
	{
	    // Raw string: R"[delim](...)[delim]"
	    char_u *delim = p + 2;
	    char_u *paren = vim_strchr(delim, '(');

	    if (paren != NULL)
	    {
		size_t delim_len = paren - delim;

		for (p += 3; *p; ++p)
		    if (p[0] == ')' && STRNCMP(p + 1, delim, delim_len) == 0
			    && p[delim_len + 1] == '"')
		    {
			p += delim_len + 1;
			break;
		    }
		if (p[0] == '"')
		    continue; // continue for another string
	    }
	}
	break;				    // no string found
    }
    if (!*p)
	--p;				    // backup from NUL
    return p;
}
#endif // FEAT_SYN_HL

#if defined(FEAT_LISP) || defined(PROTO)

    static int
lisp_match(char_u *p)
{
    char_u	buf[LSIZE];
    int		len;
    char_u	*word = *curbuf->b_p_lw != NUL ? curbuf->b_p_lw : p_lispwords;

    while (*word != NUL)
    {
	(void)copy_option_part(&word, buf, LSIZE, ",");
	len = (int)STRLEN(buf);
	if (STRNCMP(buf, p, len) == 0 && p[len] == ' ')
	    return TRUE;
    }
    return FALSE;
}

/*
 * When 'p' is present in 'cpoptions, a Vi compatible method is used.
 * The incompatible newer method is quite a bit better at indenting
 * code in lisp-like languages than the traditional one; it's still
 * mostly heuristics however -- Dirk van Deun, dirk@rave.org
 *
 * TODO:
 * Findmatch() should be adapted for lisp, also to make showmatch
 * work correctly: now (v5.3) it seems all C/C++ oriented:
 * - it does not recognize the #\( and #\) notations as character literals
 * - it doesn't know about comments starting with a semicolon
 * - it incorrectly interprets '(' as a character literal
 * All this messes up get_lisp_indent in some rare cases.
 * Update from Sergey Khorev:
 * I tried to fix the first two issues.
 */
    int
get_lisp_indent(void)
{
    pos_T	*pos, realpos, paren;
    int		amount;
    char_u	*that;
    colnr_T	col;
    colnr_T	firsttry;
    int		parencount, quotecount;
    int		vi_lisp;

    // Set vi_lisp to use the vi-compatible method
    vi_lisp = (vim_strchr(p_cpo, CPO_LISP) != NULL);

    realpos = curwin->w_cursor;
    curwin->w_cursor.col = 0;

    if ((pos = findmatch(NULL, '(')) == NULL)
	pos = findmatch(NULL, '[');
    else
    {
	paren = *pos;
	pos = findmatch(NULL, '[');
	if (pos == NULL || LT_POSP(pos, &paren))
	    pos = &paren;
    }
    if (pos != NULL)
    {
	// Extra trick: Take the indent of the first previous non-white
	// line that is at the same () level.
	amount = -1;
	parencount = 0;

	while (--curwin->w_cursor.lnum >= pos->lnum)
	{
	    if (linewhite(curwin->w_cursor.lnum))
		continue;
	    for (that = ml_get_curline(); *that != NUL; ++that)
	    {
		if (*that == ';')
		{
		    while (*(that + 1) != NUL)
			++that;
		    continue;
		}
		if (*that == '\\')
		{
		    if (*(that + 1) != NUL)
			++that;
		    continue;
		}
		if (*that == '"' && *(that + 1) != NUL)
		{
		    while (*++that && *that != '"')
		    {
			// skipping escaped characters in the string
			if (*that == '\\')
			{
			    if (*++that == NUL)
				break;
			    if (that[1] == NUL)
			    {
				++that;
				break;
			    }
			}
		    }
		}
		if (*that == '(' || *that == '[')
		    ++parencount;
		else if (*that == ')' || *that == ']')
		    --parencount;
	    }
	    if (parencount == 0)
	    {
		amount = get_indent();
		break;
	    }
	}

	if (amount == -1)
	{
	    curwin->w_cursor.lnum = pos->lnum;
	    curwin->w_cursor.col = pos->col;
	    col = pos->col;

	    that = ml_get_curline();

	    if (vi_lisp && get_indent() == 0)
		amount = 2;
	    else
	    {
		char_u *line = that;

		amount = 0;
		while (*that && col)
		{
		    amount += lbr_chartabsize_adv(line, &that, (colnr_T)amount);
		    col--;
		}

		// Some keywords require "body" indenting rules (the
		// non-standard-lisp ones are Scheme special forms):
		//
		// (let ((a 1))    instead    (let ((a 1))
		//   (...))	      of	   (...))

		if (!vi_lisp && (*that == '(' || *that == '[')
						      && lisp_match(that + 1))
		    amount += 2;
		else
		{
		    that++;
		    amount++;
		    firsttry = amount;

		    while (VIM_ISWHITE(*that))
		    {
			amount += lbr_chartabsize(line, that, (colnr_T)amount);
			++that;
		    }

		    if (*that && *that != ';') // not a comment line
		    {
			// test *that != '(' to accommodate first let/do
			// argument if it is more than one line
			if (!vi_lisp && *that != '(' && *that != '[')
			    firsttry++;

			parencount = 0;
			quotecount = 0;

			if (vi_lisp
				|| (*that != '"'
				    && *that != '\''
				    && *that != '#'
				    && (*that < '0' || *that > '9')))
			{
			    while (*that
				    && (!VIM_ISWHITE(*that)
					|| quotecount
					|| parencount)
				    && (!((*that == '(' || *that == '[')
					    && !quotecount
					    && !parencount
					    && vi_lisp)))
			    {
				if (*that == '"')
				    quotecount = !quotecount;
				if ((*that == '(' || *that == '[')
							       && !quotecount)
				    ++parencount;
				if ((*that == ')' || *that == ']')
							       && !quotecount)
				    --parencount;
				if (*that == '\\' && *(that+1) != NUL)
				    amount += lbr_chartabsize_adv(
						line, &that, (colnr_T)amount);
				amount += lbr_chartabsize_adv(
						line, &that, (colnr_T)amount);
			    }
			}
			while (VIM_ISWHITE(*that))
			{
			    amount += lbr_chartabsize(
						 line, that, (colnr_T)amount);
			    that++;
			}
			if (!*that || *that == ';')
			    amount = firsttry;
		    }
		}
	    }
	}
    }
    else
	amount = 0;	// no matching '(' or '[' found, use zero indent

    curwin->w_cursor = realpos;

    return amount;
}
#endif // FEAT_LISP

#if defined(FEAT_LISP) || defined(PROTO)
/*
 * Re-indent the current line, based on the current contents of it and the
 * surrounding lines. Fixing the cursor position seems really easy -- I'm very
 * confused what all the part that handles Control-T is doing that I'm not.
 * "get_the_indent" should be get_c_indent, get_expr_indent or get_lisp_indent.
 */

    void
fixthisline(int (*get_the_indent)(void))
{
    int amount = get_the_indent();

    if (amount >= 0)
    {
	change_indent(INDENT_SET, amount, FALSE, 0, TRUE);
	if (linewhite(curwin->w_cursor.lnum))
	    did_ai = TRUE;	// delete the indent if the line stays empty
    }
}

    void
fix_indent(void)
{
    if (p_paste)
	return;
# ifdef FEAT_LISP
    if (curbuf->b_p_lisp && curbuf->b_p_ai)
	fixthisline(get_lisp_indent);
# endif
}

#endif
