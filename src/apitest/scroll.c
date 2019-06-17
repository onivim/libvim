#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  vimInput(":");
  vimInput("5");
  vimInput("0");
  vimInput("<cr>");
}

void test_teardown(void) {}

MU_TEST(test_set_get_metrics) {
  vimWindowSetWidth(80);
  vimWindowSetHeight(10);

  mu_check(vimWindowGetWidth() == 80);
  mu_check(vimWindowGetHeight() == 10);

  vimWindowSetWidth(20);
  vimWindowSetHeight(21);

  mu_check(vimWindowGetWidth() == 20);
  mu_check(vimWindowGetHeight() == 21);

  vimWindowSetWidth(1000);
  vimWindowSetHeight(2000);

  mu_check(vimWindowGetWidth() == 1000);
  mu_check(vimWindowGetHeight() == 2000);
}

MU_TEST(test_simple_scroll) {
  vimWindowSetWidth(80);
  vimWindowSetHeight(40);

  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("z");
  mu_check(vimWindowGetTopLine() == 31);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("b");
  mu_check(vimWindowGetTopLine() == 11);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("t");
  mu_check(vimWindowGetTopLine() == 50);
  mu_check(vimCursorGetLine() == 50);
}

MU_TEST(test_small_screen_scroll) {
  vimWindowSetWidth(80);
  vimWindowSetHeight(3);

  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("z");
  mu_check(vimWindowGetTopLine() == 49);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("b");
  mu_check(vimWindowGetTopLine() == 48);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("t");
  mu_check(vimWindowGetTopLine() == 50);
  mu_check(vimCursorGetLine() == 50);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_set_get_metrics);
  MU_RUN_TEST(test_simple_scroll);
  MU_RUN_TEST(test_small_screen_scroll);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(80);
  win_setheight(40);

  buf_T *buf = vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
