#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimKey("<Esc>");
  vimKey("<Esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

int simulateAddCommentCallback(buf_T *buf, linenr_T start, linenr_T end,
                               linenr_T *outCount, char_u ***outLines)
{
  linenr_T count = end - start + 1;
  *outCount = count;

  char_u **lines = malloc((sizeof(char_u **)) * count);
  if (count >= 1)
  {
    lines[0] = strdup("//This is the first line of a test file");
  }
  if (count >= 2)
  {
    lines[1] = strdup("//This is the second line of a test file");
  }

  if (count >= 3)
  {
    lines[2] = strdup("//This is the third line of a test file");
  }

  *outLines = lines;
  return OK;
};

int simulateRemoveCommentCallback(buf_T *buf, linenr_T start, linenr_T end,
                                  linenr_T *outCount, char_u ***outLines)
{
  linenr_T count = end - start + 1;
  *outCount = count;

  char_u **lines = malloc((sizeof(char_u **)) * count);
  if (count >= 1)
  {
    lines[0] = strdup("This is the first line of a test file");
  }
  if (count >= 2)
  {
    lines[1] = strdup("This is the second line of a test file");
  }

  if (count >= 3)
  {
    lines[2] = strdup("This is the third line of a test file");
  }

  *outLines = lines;
  return OK;
};

MU_TEST(test_toggle_uncommented)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_there_and_back_again)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);

  vimSetToggleCommentsCallback(&simulateRemoveCommentCallback);
  vimInput("g");
  vimInput("c");
  vimInput("c");

  line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_uncommented_visual)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("V");
  vimInput("g");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_uncommented_visual_multi)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);

  vimInput("g");
  vimInput("c");

  char_u *line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  mu_check(strcmp(line1, "//This is the first line of a test file") == 0);

  char_u *line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);
  mu_check(strcmp(line2, "//This is the second line of a test file") == 0);

  char_u *line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3: |%s|\n", line3);
  mu_check(strcmp(line3, "This is the third line of a test file") == 0);
}

MU_TEST(test_toggle_there_and_back_again_visual_multi)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("V");
  vimInput("j");
  vimInput("g");
  vimInput("c");

  char_u *line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  mu_check(strcmp(line1, "//This is the first line of a test file") == 0);

  char_u *line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);
  mu_check(strcmp(line2, "//This is the second line of a test file") == 0);

  char_u *line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3: |%s|\n", line3);
  mu_check(strcmp(line3, "This is the third line of a test file") == 0);

  // and back again
  vimSetToggleCommentsCallback(&simulateRemoveCommentCallback);

  vimInput("V");
  vimInput("j");
  vimInput("g");
  vimInput("c");

  line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  mu_check(strcmp(line1, "This is the first line of a test file") == 0);

  line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);
  mu_check(strcmp(line2, "This is the second line of a test file") == 0);

  line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3: |%s|\n", line3);
  mu_check(strcmp(line3, "This is the third line of a test file") == 0);
}

MU_TEST(test_undo)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);

  vimInput("u");

  line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE, after undo: %s\n", line);

  mu_check(strcmp(line, "This is the first line of a test file") == 0);
}

MU_TEST(test_undo_visual_multi)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);

  vimInput("g");
  vimInput("c");

  char_u *line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  mu_check(strcmp(line1, "//This is the first line of a test file") == 0);

  char_u *line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);
  mu_check(strcmp(line2, "//This is the second line of a test file") == 0);

  char_u *line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3: |%s|\n", line3);
  mu_check(strcmp(line3, "This is the third line of a test file") == 0);

  // and back again

  vimInput("u");

  line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1, after undo: |%s|\n", line1);
  mu_check(strcmp(line1, "This is the first line of a test file") == 0);

  line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2, after undo: |%s|\n", line2);
  mu_check(strcmp(line2, "This is the second line of a test file") == 0);

  line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3, after undo: |%s|\n", line3);
  mu_check(strcmp(line3, "This is the third line of a test file") == 0);
}

MU_TEST(test_cursor_toggle_there_and_back_again)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("g");
  vimInput("c");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  // and back again
  vimSetToggleCommentsCallback(&simulateRemoveCommentCallback);

  vimInput("g");
  vimInput("c");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_cursor_toggle_there_and_back_again_visual_multi)
{
  vimSetToggleCommentsCallback(&simulateAddCommentCallback);
  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("g");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  // and back again

  vimSetToggleCommentsCallback(&simulateRemoveCommentCallback);
  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("g");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_regression_Vc)
{
  vimInput("V");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: |%s|\n", line);

  mu_check(strcmp(line, "") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_toggle_uncommented);
  MU_RUN_TEST(test_toggle_there_and_back_again);
  MU_RUN_TEST(test_toggle_uncommented_visual);
  MU_RUN_TEST(test_toggle_uncommented_visual_multi);
  MU_RUN_TEST(test_toggle_there_and_back_again_visual_multi);
  MU_RUN_TEST(test_undo);
  MU_RUN_TEST(test_undo_visual_multi);
  MU_RUN_TEST(test_cursor_toggle_there_and_back_again);
  MU_RUN_TEST(test_cursor_toggle_there_and_back_again_visual_multi);
  MU_RUN_TEST(test_regression_Vc);
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
