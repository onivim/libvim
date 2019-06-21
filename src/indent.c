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
    return NULL;
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
    return NULL;
}

/*
 * Find the start of a raw string, not knowing if we are in one right now.
 * Search starts at w_cursor.lnum and goes backwards.
 * Return NULL when not inside a raw string.
 */
    static pos_T *
find_start_rawstring(int ind_maxcomment)	// XXX
{
    return NULL;
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
