/* vi:set ts=8 sts=4 sw=4 noet:
 */

/*
 * auto_closing_pairs.c: functions for working with auto-closing pairs
 */

#include "vim.h"

/*
 * Return TRUE if the cursor should 'pass-through' this character
 * (if it is the closing character of an auto-closing pair)
 */
int acp_should_pass_through(char_u c) {
    return FALSE;
}
