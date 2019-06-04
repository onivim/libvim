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

/*
 * vimBufferOpen
 *
 * Open a buffer and set as current.
 */
buf_T *vimBufferOpen(char_u *ffname_arg, linenr_T lnum, int flags);

char_u *vimBufferGetLine(buf_T* buf, linenr_T lnum);

void vimInput(char_u *input);

void vimExecute(char_u *cmd);

linenr_T vimWindowGetCursorLine(void);

int vimGetMode(void);

/* vim: set ft=c : */
