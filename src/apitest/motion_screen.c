#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimBufferOpen("collateral/lines_100.txt", 1, 0);
  vimKey("<Esc>");
  vimKey("<Esc>");
  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

void SimpleScreenLineCallback(screenLineMotion_T motion, int count, linenr_T startLine, linenr_T *outLine)
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

void ErroneousScreenLineCallback(screenLineMotion_T motion, int count, linenr_T startLine, linenr_T *outLine)
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

void SimplePositionCallback(int dir, int count, linenr_T srcLine, colnr_T srcColumn, colnr_T curswant, linenr_T *destLine, colnr_T *destColumn)
{
  if (dir == BACKWARD)
  {
    *destLine = 1;
    *destColumn = 0;
  }
  else
  {
    *destLine = 100;
    *destColumn = 0;
  }
}

void SameLinePositionCallback(int dir, int count, linenr_T srcLine, colnr_T srcColumn, colnr_T curswant, linenr_T *destLine, colnr_T *destColumn)
{
  if (dir == BACKWARD)
  {
    *destLine = srcLine;
    *destColumn = 0;
  }
  else
  {
    *destLine = srcLine;
    *destColumn = srcColumn + 1;
  }
}

void MaxColPositionCallback(int dir, int count, linenr_T srcLine, colnr_T srcColumn, colnr_T curswant, linenr_T *destLine, colnr_T *destColumn)
{
  if (dir == BACKWARD)
  {
    *destLine = srcLine - 1;
    *destColumn = MAXCOL;
  }
  else
  {
    *destLine = srcLine + 1;
    *destColumn = MAXCOL;
  }
}

void ErroneousPositionCallback(int dir, int count, linenr_T srcLine, colnr_T srcColumn, colnr_T curswant, linenr_T *destLine, colnr_T *destColumn)
{
  if (dir == BACKWARD)
  {
    *destLine = srcLine;
    *destColumn = -1;
  }
  else
  {
    *destLine = srcLine;
    *destColumn = 10000;
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
  vimSetCursorMoveScreenLineCallback(&SimpleScreenLineCallback);

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
  vimSetCursorMoveScreenLineCallback(&ErroneousScreenLineCallback);

  vimInput("H");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("L");

  mu_check(vimCursorGetLine() == 100);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("M");

  mu_check(vimCursorGetLine() == 100);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_gj_gk_motion)
{
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenPositionCallback(&SimplePositionCallback);

  vimInput("gj");

  mu_check(vimCursorGetLine() == 100);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("gk");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_gk_motion_same_line)
{
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenPositionCallback(&SameLinePositionCallback);

  vimInput("3l");
  vimInput("d");
  vimInput("gk");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "e 1") == 0);
}

MU_TEST(test_gj_motion_same_line)
{
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenPositionCallback(&SameLinePositionCallback);

  vimInput("3l");
  mu_check(vimCursorGetColumn() == 3);

  vimInput("d");
  vimInput("gj");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "Lin 1") == 0);
}

MU_TEST(test_erroneous_position_callback)
{
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenPositionCallback(&ErroneousPositionCallback);

  vimInput("gk");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("gj");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 5);
}

MU_TEST(test_curswant)
{
  vimBufferOpen("collateral/curswant.txt", 1, 0);
  // When no callback is set, the cursor should not move at all.
  vimSetCursorMoveScreenPositionCallback(&MaxColPositionCallback);

  vimInput("$");
  vimInput("gj");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 1);

  vimInput("gj");
  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("gj");
  mu_check(vimCursorGetLine() == 4);
  mu_check(vimCursorGetColumn() == 3);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_no_callback);
  MU_RUN_TEST(test_simple_callback);
  MU_RUN_TEST(test_erroneous_callback);
  MU_RUN_TEST(test_gj_gk_motion);
  MU_RUN_TEST(test_gk_motion_same_line);
  MU_RUN_TEST(test_gj_motion_same_line);
  MU_RUN_TEST(test_erroneous_position_callback);
  MU_RUN_TEST(test_curswant);
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
