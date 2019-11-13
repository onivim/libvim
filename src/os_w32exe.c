/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *				GUI support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * Windows GUI: main program (EXE) entry point:
 *
 * Ron Aaron <ronaharon@yahoo.com> wrote this and the (now deleted) DLL support
 * code.
 */
#include "vim.h"

// cproto doesn't create a prototype for VimMain()
int VimMain(int argc, char **argv);
void SaveInst(HINSTANCE hInst);

#ifndef PROTO
int wmain(int argc UNUSED, wchar_t **argv UNUSED)
{
  SaveInst(GetModuleHandleW(NULL));
  VimMain(0, NULL);

  return 0;
}
#endif
