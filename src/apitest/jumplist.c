#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_jumplist_openfile) {
  buf_T *firstBuf = vimBufferOpen("collateral/testfile.txt", 1, 0);
  buf_T *secondBuf = vimBufferOpen("collateral/lines_100.txt", 1, 0);

  mu_check(firstBuf != secondBuf);

  mu_check(curbuf == secondBuf);

  vimInput("<c-o>");
  mu_check(curbuf == firstBuf);

  vimInput("<c-i>");
  mu_check(curbuf == secondBuf);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_jumplist_openfile);
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
