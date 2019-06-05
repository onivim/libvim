#include <assert.h>
#include <stdio.h>
#include "libvim.h"

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("testfile.txt", 1, 0);
  assert(vimGetMode() & NORMAL == NORMAL);

  char *line = vimBufferGetLine(buf, 1);
  printf("LINE: %s\n", line);
  int comp = strcmp(line, "This is the first line of a test file");
  assert(comp == 0);

  printf("cursor line: %d\n", vimWindowGetCursorLine());

  assert(vimWindowGetCursorLine() == 1);

  vimInput("G");

  assert(vimWindowGetCursorLine() > 1);

  /* vimExecute("help tutor"); */
  /* assert(vimWindowGetCursorLine() == 32); */

  vimInput("g");
  vimInput("g");

  vimInput("v");
  assert(vimGetMode() & VISUAL == VISUAL);
  vimInput("l");
  vimInput("l");
  vimInput("x");
  /* vimInput("i"); */
  /* vimInput("a"); */
  /* vimInput("b"); */
  /* vimInput("c"); */
  /* vimInput("d"); */
  /* vimInput("e"); */
  /* vimInput("\010"); */
  /* vimInput("\033"); */
  /* vimInput("d"); */
  /* vimInput("d"); */

  printf("CURSOR LINE: %d\n", vimWindowGetCursorLine());
  /* assert(vimGetMode() & INSERT == INSERT); */

  line = vimBufferGetLine(buf, 1);
  printf("LINE: %s\n", line);
  printf("Completed\n");
}
