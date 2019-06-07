#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {

}

MU_TEST(test_w) {
  mu_check(vimWindowGetCursorColumn() == 1);

  vimInput("w");

  mu_check(vimWindowGetCursorColumn() == 6);

  vimInput("2");
  vimInput("w");

  mu_check(vimWindowGetCursorColumn() == 13);

  vimInput("1");
  vimInput("0");
  vimInput("w");

  mu_check(vimWindowGetCursorLine() == 20);
  mu_check(vimWindowGetCursorColumn() == 20);
}


MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

    MU_RUN_TEST(test_w);
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
