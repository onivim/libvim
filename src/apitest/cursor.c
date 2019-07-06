#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_set_cursor)
{
  pos_T pos;
  pos.lnum = 5;
  pos.col = 4;
  vimCursorSetPosition(pos);

  mu_check(vimCursorGetLine() == 5);
  mu_check(vimCursorGetColumn() == 4);
}

MU_TEST(test_set_cursor_invalid_line)
{
  pos_T pos;
  pos.lnum = 500;
  pos.col = 4;
  vimCursorSetPosition(pos);

  mu_check(vimCursorGetLine() == 100);
  mu_check(vimCursorGetColumn() == 4);
}

MU_TEST(test_set_cursor_invalid_column)
{
  pos_T pos;
  pos.lnum = 7;
  pos.col = 500;
  vimCursorSetPosition(pos);

  mu_check(vimCursorGetLine() == 7);
  mu_check(vimCursorGetColumn() == 5);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_set_cursor);
  MU_RUN_TEST(test_set_cursor_invalid_line);
  MU_RUN_TEST(test_set_cursor_invalid_column);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(80);
  win_setheight(40);

  vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
