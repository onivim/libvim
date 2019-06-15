#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_change_word) {
  vimInput("c");
  vimInput("w");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "abc is the first line of a test file") == 0);
}

MU_TEST(test_change_line_C) {
  vimInput("C");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "abc") == 0);
}

MU_TEST(test_change_line_c$) {
  vimInput("c");
  vimInput("$");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "abc") == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_change_word);
  MU_RUN_TEST(test_change_line_C);
  MU_RUN_TEST(test_change_line_c$);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
