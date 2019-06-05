#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_cmdline_substitution) {
    printf("here");
  vimInput(":");
    printf("after");
  vimInput("%");
  vimInput("s");
  vimInput("!");
  vimInput("s");
  vimInput("!");
  vimInput("t");
  vimInput("!");
  vimInput("g");
  vimInput("\013");

  char *line = vimBufferGetLine(curbuf, 1);
  printf("LINE: %s\n", line);
  int comp = strcmp(line, "Thit it the firtt line of a tett file");
  mu_check(comp == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_cmdline_substitution);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
