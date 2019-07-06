#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_curswant_column2)
{
  mu_check(vimCursorGetLine() == 1);

  // Move one character right
  vimInput("l");

  mu_check(vimCursorGetColumn() == 1);
  mu_check(vimCursorGetLine() == 1);

  // Move two characters down
  vimInput("j");
  vimInput("j");

  mu_check(vimCursorGetColumn() == 0);
  mu_check(vimCursorGetLine() == 3);

  vimInput("j");
  mu_check(vimCursorGetColumn() == 1);
  mu_check(vimCursorGetLine() == 4);
}

MU_TEST(test_curswant_maxcolumn)
{
  mu_check(vimCursorGetLine() == 1);

  // Move all the way to the right
  vimInput("$");

  mu_check(vimCursorGetColumn() == 2);
  mu_check(vimCursorGetLine() == 1);

  // Move three characters down
  vimInput("j");
  vimInput("j");
  vimInput("j");

  // Cursor should be all the way to the right
  mu_check(vimCursorGetColumn() == 3);
  mu_check(vimCursorGetLine() == 4);
}

MU_TEST(test_curswant_reset)
{
  mu_check(vimCursorGetLine() == 1);

  // Move all the way to the right...
  vimInput("$");
  // And the once to the left...
  // This should reset curswant
  vimInput("h");

  mu_check(vimCursorGetColumn() == 1);
  mu_check(vimCursorGetLine() == 1);

  // Move three characters down
  vimInput("j");
  vimInput("j");
  vimInput("j");

  mu_check(vimCursorGetColumn() == 1);
  mu_check(vimCursorGetLine() == 4);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_curswant_column2);
  MU_RUN_TEST(test_curswant_maxcolumn);
  MU_RUN_TEST(test_curswant_reset);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/curswant.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
