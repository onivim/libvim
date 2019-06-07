#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_w) {
  mu_check(vimWindowGetCursorColumn() == 0);

  vimInput("w");

  mu_check(vimWindowGetCursorColumn() == 5);

  vimInput("2");
  vimInput("w");

  mu_check(vimWindowGetCursorColumn() == 12);

  vimInput("1");
  vimInput("0");
  vimInput("w");

  mu_check(vimWindowGetCursorLine() == 2);
  mu_check(vimWindowGetCursorColumn() == 19);
}

MU_TEST(test_e) {
  mu_check(vimWindowGetCursorColumn() == 0);

  vimInput("e");

  mu_check(vimWindowGetCursorColumn() == 3);

  vimInput("2");
  vimInput("e");

  mu_check(vimWindowGetCursorColumn() == 10);

  vimInput("1");
  vimInput("0");
  vimInput("0");
  vimInput("e");

  mu_check(vimWindowGetCursorLine() == 3);
  mu_check(vimWindowGetCursorColumn() == 36);
}

MU_TEST(test_b) {
  mu_check(vimWindowGetCursorColumn() == 0);

  vimInput("$");

  vimInput("b");
  mu_check(vimWindowGetCursorColumn() == 33);

  vimInput("5");
  vimInput("b");
  mu_check(vimWindowGetCursorColumn() == 12);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_w);
  MU_RUN_TEST(test_e);
  MU_RUN_TEST(test_b);
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
