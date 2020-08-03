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

MU_TEST(test_multiple_undo)
{
  // Delete first line
  vimInput("d");
  vimInput("d");

  // Delete second line
  vimInput("d");
  vimInput("d");

  buf_T *cur = vimBufferGetCurrent();
  int count = vimBufferGetLineCount(cur);
  mu_check(count == 1);

  // Undo last change - the second line should be back
  vimInput("u");

  count = vimBufferGetLineCount(cur);
  mu_check(count == 2);
  mu_check(strcmp(vimBufferGetLine(cur, 1),
                  "This is the second line of a test file") == 0);

  // Undo again - the first line should be back
  vimInput("u");

  count = vimBufferGetLineCount(cur);
  mu_check(count == 3);
  mu_check(strcmp(vimBufferGetLine(cur, 1),
                  "This is the first line of a test file") == 0);
}

MU_TEST(test_multiple_undo_redo)
{
  // Delete first line
  vimInput("d");
  vimInput("d");

  // Delete second line
  vimInput("d");
  vimInput("d");

  buf_T *cur = vimBufferGetCurrent();
  int count = vimBufferGetLineCount(cur);
  mu_check(count == 1);

  // Undo twice
  vimInput("u");
  vimInput("u");

  // Redo the last change
  vimKey("<C-r>");

  count = vimBufferGetLineCount(cur);
  mu_check(count == 2);

  // Redo again
  vimKey("<C-r>");

  count = vimBufferGetLineCount(cur);
  mu_check(count == 1);
}

MU_TEST(test_undo_save)
{
  // Save buffer before changing

  vimUndoSaveRegion(0, 3);

  // Replace first line with 'one'
  char_u *lines[] = {"one"};
  vimBufferSetLines(curbuf, 0, 1, lines, 1);

  vimUndoSaveRegion(0, 3);

  char_u *linesAgain[] = {"two"};
  vimBufferSetLines(curbuf, 0, 1, linesAgain, 1);

  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "two") == 0);

  vimInput("u");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test file") == 0);
}

MU_TEST(test_undo_sync)
{
  // Save buffer before changing

  vimUndoSaveRegion(0, 3);

  // Replace first line with 'one'
  char_u *lines[] = {"one"};
  vimBufferSetLines(curbuf, 0, 1, lines, 1);

  // Create sync point (new undo level)
  vimUndoSync(0);
  vimUndoSaveRegion(0, 3);

  char_u *linesAgain[] = {"two"};
  vimBufferSetLines(curbuf, 0, 1, linesAgain, 1);

  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "two") == 0);

  vimInput("u");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "one") == 0);

  vimInput("u");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test file") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_multiple_undo);
  MU_RUN_TEST(test_multiple_undo_redo);
  MU_RUN_TEST(test_undo_save);
  MU_RUN_TEST(test_undo_sync);
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
