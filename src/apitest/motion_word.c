#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_w)
{
  mu_check(vimCursorGetColumn() == 0);

  vimInput("w");

  mu_check(vimCursorGetColumn() == 5);

  vimInput("2");
  vimInput("w");

  mu_check(vimCursorGetColumn() == 12);

  vimInput("1");
  vimInput("0");
  vimInput("w");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 19);
}

MU_TEST(test_e)
{
  mu_check(vimCursorGetColumn() == 0);

  vimInput("e");

  mu_check(vimCursorGetColumn() == 3);

  vimInput("2");
  vimInput("e");

  mu_check(vimCursorGetColumn() == 10);

  vimInput("1");
  vimInput("0");
  vimInput("0");
  vimInput("e");

  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 36);
}

MU_TEST(test_b)
{
  mu_check(vimCursorGetColumn() == 0);

  vimInput("$");

  vimInput("b");
  mu_check(vimCursorGetColumn() == 33);

  vimInput("5");
  vimInput("b");
  mu_check(vimCursorGetColumn() == 12);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_w);
  MU_RUN_TEST(test_e);
  MU_RUN_TEST(test_b);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
