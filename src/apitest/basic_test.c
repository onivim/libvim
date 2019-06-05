#include "libvim.h"
#include <assert.h>
#include <stdio.h>

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

  size_t len = vimBufferGetLineCount(buf);
  assert(len == 3);

  printf("cursor line: %d\n", vimWindowGetCursorLine());

  assert(vimWindowGetCursorLine() == 1);

  vimInput("G");
  printf("cursor line: %d\n", vimWindowGetCursorLine());

  assert(vimWindowGetCursorLine() > 1);

  vimInput("v");
  assert(vimGetMode() & VISUAL == VISUAL);
  vimInput("l");
  vimInput("l");
  vimInput("x");

  printf("CURSOR LINE: %d\n", vimWindowGetCursorLine());
  assert(vimGetMode() & NORMAL == NORMAL);

  line = vimBufferGetLine(buf, 1);
  printf("LINE: %s\n", line);
  printf("Completed\n");
}
