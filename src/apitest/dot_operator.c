#include "libvim.h"
#include "minunit.h"

void test_setup(void) { vimExecute("new"); }

void test_teardown(void) {}

MU_TEST(test_basic_redo)
{

  vimInput("I");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "abc") == 0);

  vimInput(".");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "abcabc") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_basic_redo);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
