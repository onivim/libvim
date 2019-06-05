#include <assert.h>
#include <stdio.h>
#include "libvim.h"

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  printf("BEFORE\n");
  vimExecute("so D:/libvim1/src/testdir/test_arglist.vim");
  vimExecute("so D:/libvim1/src/testdir/runtest.vim");
  printf("AFTER\n");
}
