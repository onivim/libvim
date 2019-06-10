#include "libvim.h"

int main(int argc, char **argv) {
  /* Simple test to validate we can run with no args */
  char *c[0];
  vimInit(0, c);


  printf("TEST: %d\n", curbuf);

  printf("Initialized without crashing\n");
}
