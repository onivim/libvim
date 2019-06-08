#include "vim.h"

/* libvim.c */

/*
 * vimInit
 *
 * This must be called prior to using any other methods.
 *
 * This expects an `argc` and an `argv` parameters,
 * for the command line arguments for this vim instance.
 */
void vimInit(int argc, char **argv);

/***
 * Buffer Methods
 ***/

/*
 * vimBufferOpen
 *
 * Open a buffer and set as current.
 */

buf_T *vimBufferOpen(char_u *ffname_arg, linenr_T lnum, int flags);
buf_T *vimBufferGetById(int id);
buf_T *vimBufferGetCurrent(void);
void vimBufferSetCurrent(buf_T *buf);

char_u *vimBufferGetFilename(buf_T *buf);
int vimBufferGetId(buf_T *buf);
long vimBufferGetLastChangedTick(buf_T *buf);
char_u *vimBufferGetLine(buf_T *buf, linenr_T lnum);
size_t vimBufferGetLineCount(buf_T *buf);
int vimBufferGetModified(buf_T *buf);

void vimSetBufferUpdateCallback(BufferUpdateCallback bufferUpdate);

/***
 * Autocommands
 ***/

void vimSetAutoCommandCallback(AutoCommandCallback autoCommandDispatch);

/***
 * Cursor Methods
 ***/
colnr_T vimCursorGetColumn(void);
linenr_T vimCursorGetLine(void);
pos_T vimCursorGetPosition(void);

/***
 * User Input
 ***/
void vimInput(char_u *input);

void vimExecute(char_u *cmd);

/***
 * Registers
 ***/

void vimRegisterGet(int reg_name, int *num_lines, char_u ***lines);

/***
 * Visual Mode
 ***/

int vimVisualGetType(void);
int vimVisualIsActive(void);
int vimSelectIsActive(void);

/*
 * vimVisualGetRange
 *
 * If in visual mode or select mode, returns the current range.
 * If not in visual or select mode, returns the last visual range.
 */
void vimVisualGetRange(pos_T *startPos, pos_T *endPos);

/***
 * Misc
 ***/

int vimGetMode(void);

/* vim: set ft=c : */
