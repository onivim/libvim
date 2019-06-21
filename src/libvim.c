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

buf_T *vimBufferOpen(char_u *ffname_arg, linenr_T lnum, int flags) {
  buf_T *buffer = buflist_new(ffname_arg, NULL, lnum, flags);
  set_curbuf(buffer, DOBUF_SPLIT);
  return buffer;
}

buf_T *vimBufferGetCurrent(void) { return curbuf; }

buf_T *vimBufferGetById(int id) { return buflist_findnr(id); }

char_u *vimBufferGetFilename(buf_T *buf) { return buf->b_ffname; }
char_u *vimBufferGetFiletype(buf_T *buf) { return buf->b_p_ft; }

void vimBufferSetCurrent(buf_T *buf) { set_curbuf(buf, DOBUF_SPLIT); }

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

void vimSetAutoCommandCallback(AutoCommandCallback f) {
  autoCommandCallback = f;
}

char_u vimCommandLineGetType(void) { return ccline.cmdfirstc; }

char_u *vimCommandLineGetText(void) { return ccline.cmdbuff; }

int vimCommandLineGetPosition(void) { return ccline.cmdpos; }

void vimCommandLineGetCompletions(char ***completions, int *count) {
  /* set_expand_context(&ccline.xpc); */
  if (!ccline.xpc) {
    *count = 0;
    *completions = NULL;
    return;
  }
  expand_cmdline(ccline.xpc, ccline.cmdbuff, ccline.cmdpos, count, completions);
}

linenr_T vimCursorGetLine(void) { return curwin->w_cursor.lnum; };
colnr_T vimCursorGetColumn(void) { return curwin->w_cursor.col; };
pos_T vimCursorGetPosition(void) { return curwin->w_cursor; };
colnr_T vimCursorGetDesiredColumn(void) { return curwin->w_curswant; };

void vimCursorSetPosition(pos_T pos) {
  curwin->w_cursor.lnum = pos.lnum;
  curwin->w_cursor.col = pos.col;
  /* TODO: coladd? */
  check_cursor();
}

void vimInput(char_u *input) {
  char_u *ptr = NULL;
  char_u *cpo_save = p_cpo;

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

  if (*ptr != NUL) /* trailing CTRL-V results in nothing */
  {
    sm_execute_normal(input);
    vim_free((char_u *)ptr);
  }
  /* Trigger CursorMoved if the cursor moved. */
  if (!finish_op && (has_cursormoved()) &&
      !EQUAL_POS(last_cursormoved, curwin->w_cursor)) {
    if (has_cursormoved())
      apply_autocmds(EVENT_CURSORMOVED, NULL, NULL, FALSE, curbuf);
    last_cursormoved = curwin->w_cursor;
  }

  update_curswant();
}

int vimVisualIsActive(void) { return VIsual_active; }

int vimSelectIsActive(void) { return VIsual_select; }

int vimVisualGetType(void) { return VIsual_mode; }

void vimVisualGetRange(pos_T *startPos, pos_T *endPos) {
  if (VIsual_active || VIsual_select) {
    *startPos = VIsual;
    *endPos = curwin->w_cursor;
  } else {
    *startPos = curbuf->b_visual.vi_start;
    *endPos = curbuf->b_visual.vi_end;
  }
}

pos_T *vimSearchGetMatchingPair(int initc) { return findmatch(NULL, initc); }

typedef struct shlNode_elem shlNode_T;
struct shlNode_elem {
  searchHighlight_T highlight;
  shlNode_T *next;
};

void vimSearchGetHighlights(linenr_T start_lnum, linenr_T end_lnum,
                            int *num_highlights,
                            searchHighlight_T **highlights) {

  int v = 1;
  int count = 0;

  pos_T lastPos;
  pos_T startPos;
  pos_T endPos;

  startPos.lnum = start_lnum;
  startPos.col = 0;
  lastPos = startPos;

  char_u *pattern = get_search_pat();

  if (pattern == NULL) {
    *num_highlights = 0;
    *highlights = NULL;
    return;
  }

  shlNode_T *head = ALLOC_CLEAR_ONE(shlNode_T);
  shlNode_T *cur = head;

  while (v == 1) {
    v = searchit(NULL, curbuf, &startPos, &endPos, FORWARD, pattern, 1,
                 SEARCH_KEEP, RE_SEARCH, end_lnum, NULL, NULL);

    if (v == 0) {
      break;
    }

    if (startPos.lnum < lastPos.lnum ||
        (startPos.lnum == lastPos.lnum && startPos.col <= lastPos.col)) {
      break;
    }

    shlNode_T *new = ALLOC_CLEAR_ONE(shlNode_T);
    cur->next = new;
    cur = new;

    cur->highlight.start.lnum = startPos.lnum;
    cur->highlight.start.col = startPos.col;
    cur->highlight.end.lnum = endPos.lnum;
    cur->highlight.end.col = endPos.col;

    lastPos = startPos;
    startPos = endPos;
    startPos.col = startPos.col + 1;
    count++;
  }

  searchHighlight_T *ret = alloc(sizeof(searchHighlight_T) * count);

  cur = head->next;
  vim_free(head);

  int i = 0;
  while (cur != NULL) {
    ret[i] = cur->highlight;
    shlNode_T *prev = cur;
    vim_free(prev);
    cur = cur->next;
    i++;
  }

  *num_highlights = i;
  *highlights = ret;
}

char_u *vimSearchGetPattern(void) { return get_search_pat(); }

void vimExecute(char_u *cmd) { do_cmdline_cmd(cmd); }

void vimOptionSetTabSize(int tabSize) {
  curbuf->b_p_ts = tabSize;
  curbuf->b_p_sts = tabSize;
  curbuf->b_p_sw = tabSize;
}

void vimOptionSetInsertSpaces(int insertSpaces) {
  curbuf->b_p_et = insertSpaces;
}

int vimOptionGetTabSize() { return curbuf->b_p_ts; }

int vimOptionGetInsertSpaces(void) { return curbuf->b_p_et; }

int vimWindowGetWidth(void) { return curwin->w_width; }
int vimWindowGetHeight(void) { return curwin->w_height; }
int vimWindowGetTopLine(void) { return curwin->w_topline; }

void vimWindowSetWidth(int width) {
  if (width > Columns) {
    Columns = width;
  }

  win_new_width(curwin, width);
}

void vimWindowSetHeight(int height) {
  if (height > Rows) {
    Rows = height;
  }

  win_new_height(curwin, height);
}

int vimGetMode(void) { return get_real_state(); }

void vimRegisterGet(int reg_name, int *num_lines, char_u ***lines) {
  get_yank_register_value(reg_name, num_lines, lines);
}

void vimInit(int argc, char **argv) {
  mparm_T params;
  vim_memset(&params, 0, sizeof(params));
  params.argc = argc;
  params.argv = argv;
  params.want_full_screen = TRUE;
  params.window_count = -1;

  mch_early_init();
  common_init(&params);

  // Set to 'nocompatible' so we get the expected Vim undo / redo behavior,
  // rather than Vi's behavior.
  // See :help cpoptions and :help compatible for details
  change_compatible(FALSE);

  full_screen = TRUE;
  vimWindowSetWidth(80);
  vimWindowSetHeight(40);
  screenalloc(FALSE);
}
