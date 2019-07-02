#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_G_gg)
{
  mu_check(vimCursorGetLine() == 1);

  vimInput("G");

  mu_check(vimCursorGetLine() == 3);

  vimInput("g");
  vimInput("g");

  mu_check(vimCursorGetLine() == 1);
}

MU_TEST(test_j_k)
{
  mu_check(vimCursorGetLine() == 1);

  vimInput("j");

  mu_check(vimCursorGetLine() == 2);

  vimInput("k");

  mu_check(vimCursorGetLine() == 1);
}

MU_TEST(test_2j_2k)
{
  mu_check(vimCursorGetLine() == 1);

  vimInput("2");
  vimInput("j");

  mu_check(vimCursorGetLine() == 3);

  vimInput("2");
  vimInput("k");

  mu_check(vimCursorGetLine() == 1);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_G_gg);
  MU_RUN_TEST(test_j_k);
  MU_RUN_TEST(test_2j_2k);
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
