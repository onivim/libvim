#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
}

void test_teardown(void) {}

MU_TEST(test_matching_bracket)
{
  pos_T *bracket = vimSearchGetMatchingPair(0);

  mu_check(bracket->lnum == 6);
  mu_check(bracket->col == 0);
}

MU_TEST(test_matching_parentheses_cursor)
{
  vimInput("l");
  vimInput("l");

  pos_T *bracket = vimSearchGetMatchingPair(0);

  mu_check(bracket->lnum == 3);
  mu_check(bracket->col == 38);
}

MU_TEST(test_no_match)
{
  vimInput("j");

  pos_T *bracket = vimSearchGetMatchingPair(0);

  mu_check(bracket == NULL);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_matching_bracket);
  MU_RUN_TEST(test_matching_parentheses_cursor);
  MU_RUN_TEST(test_no_match);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/brackets.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
