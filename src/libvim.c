/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * libvim
 */

/*
 * Top-level API for libvim.
 *
 * This provides the API surface for consumers of `libvim`.
 */

#include "vim.h"

void vimInit(int argc, char **argv) {
  mparm_T params;
  vim_memset(&params, 0, sizeof(params));
  params.argc = argc;
  params.argv = argv;
  params.want_full_screen = TRUE;
  params.window_count = -1;

  mch_early_init();
  common_init(&params);
  init_normal_cmds();

  win_setwidth(80);
  win_setheight(40);
}

buf_T *vimBufferOpen(char_u *ffname_arg, linenr_T lnum, int flags) {
  buf_T *buffer = buflist_new(ffname_arg, NULL, lnum, flags);
  set_curbuf(buffer, 0);
  return buffer;
}

buf_T *vimBufferGetCurrent(void) { return curbuf; }

char_u *vimBufferGetFilename(buf_T *buf) { return buf->b_ffname; }

int vimBufferGetId(buf_T *buf) { return buf->b_fnum; }

long vimBufferGetLastChangedTick(buf_T *buf) { return CHANGEDTICK(buf); }

int vimBufferGetModified(buf_T *buf) { return buf->b_changed; }

char_u *vimBufferGetLine(buf_T *buf, linenr_T lnum) {
  char_u *result = ml_get_buf(buf, lnum, FALSE);
  return result;
}

size_t vimBufferGetLineCount(buf_T *buf) { return buf->b_ml.ml_line_count; }

void vimSetBufferUpdateCallback(BufferUpdateCallback f) {
  bufferUpdateCallback = f;
}

linenr_T vimWindowGetCursorLine(void) { return curwin->w_cursor.lnum; };
colnr_T vimWindowGetCursorColumn(void) { return curwin->w_cursor.col; };
pos_T vimWindowGetCursorPosition(void) { return curwin->w_cursor; };

void vimInput(char_u *input) { 
    char_u      *ptr = NULL;
    char_u      *cpo_save = p_cpo;

    /* Set 'cpoptions' the way we want it.
     *    B set - backslashes are *not* treated specially
     *    k set - keycodes are *not* reverse-engineered
     *    < unset - <Key> sequences *are* interpreted
     *  The last but one parameter of replace_termcodes() is TRUE so that the
     *  <lt> sequence is recognised - needed for a real backslash.
     */
    p_cpo = (char_u *)"Bk";
    input = replace_termcodes((char_u *)input, &ptr, FALSE, TRUE, FALSE);
    p_cpo = cpo_save;

    if (*ptr != NUL)	/* trailing CTRL-V results in nothing */
    {
	      sm_execute_normal(input); 
    }
    vim_free((char_u *)ptr);
}

void vimExecute(char_u *cmd) { do_cmdline_cmd(cmd); }

int vimGetMode(void) { return State; }
