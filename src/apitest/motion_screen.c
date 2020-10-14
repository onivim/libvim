#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

void SimpleCallback(screenLineMotion_T motion, int count, linenr_T startLine, linenr_T *outLine)
{
  switch (motion)
  {
  case MOTION_H:
    *outLine = 10;
    break;
  case MOTION_M:
    *outLine = 20;
    break;
  case MOTION_L:
    *outLine = 30;
    break;
  }
}

void ErroneousCallback(screenLineMotion_T motion, int count, linenr_T startLine, linenr_T *outLine)
{
  switch (motion)
  {
  case MOTION_H:
    *outLine = -1;
    break;
  case MOTION_M:
    *outLine = 101;
    break;
  case MOTION_L:
    *outLine = 999;
    break;
  }
}

MU_TEST(test_no_callback)
{
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenLineCallback(NULL);

  vimInput("H");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("L");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("M");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("j");

  vimInput("H");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("L");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("M");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_simple_callback)
{
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenLineCallback(&SimpleCallback);

  vimInput("H");

  mu_check(vimCursorGetLine() == 10);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("L");

  mu_check(vimCursorGetLine() == 30);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("M");

  mu_check(vimCursorGetLine() == 20);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_erroneous_callback)
{
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenLineCallback(&ErroneousCallback);

  vimInput("H");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("L");
  printf("LINE: %ld\n", vimCursorGetLine());

  mu_check(vimCursorGetLine() == 100);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("M");

  mu_check(vimCursorGetLine() == 100);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_no_callback);
  MU_RUN_TEST(test_simple_callback);
  MU_RUN_TEST(test_erroneous_callback);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
