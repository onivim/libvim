#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<Esc>");
  vimInput("<Esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");

  vimOptionSetLineComment("//");
}

void test_teardown(void) {}

MU_TEST(test_toggle_uncommented)
{
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_there_and_back_again)
{
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);

  vimInput("g");
  vimInput("c");
  vimInput("c");

  line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_uncommented_visual)
{
  vimInput("V");
  vimInput("g");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_uncommented_visual_multi)
{
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

MU_TEST(test_set_line_comment)
{
  vimOptionSetLineComment("--");
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "--This is the first line of a test file") == 0);
}

MU_TEST(test_set_line_comment_multi_there_and_back_again)
{
  vimOptionSetLineComment("##");
  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);

  vimInput("g");
  vimInput("c");

  char_u *line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  mu_check(strcmp(line1, "##This is the first line of a test file") == 0);

  char_u *line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);
  mu_check(strcmp(line2, "##This is the second line of a test file") == 0);

  char_u *line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3: |%s|\n", line3);
  mu_check(strcmp(line3, "This is the third line of a test file") == 0);

  // and back again

  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);

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
  vimInput("g");
  vimInput("c");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  // and back again

  vimInput("g");
  vimInput("c");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_cursor_toggle_there_and_back_again_visual_multi)
{
  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("g");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  // and back again

  vimInput("V");
  vimInput("j");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("g");
  vimInput("c");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_toggle_uncommented);
  MU_RUN_TEST(test_toggle_there_and_back_again);
  MU_RUN_TEST(test_toggle_uncommented_visual);
  MU_RUN_TEST(test_toggle_uncommented_visual_multi);
  MU_RUN_TEST(test_toggle_there_and_back_again_visual_multi);
  MU_RUN_TEST(test_set_line_comment);
  MU_RUN_TEST(test_set_line_comment_multi_there_and_back_again);
  MU_RUN_TEST(test_undo);
  MU_RUN_TEST(test_undo_visual_multi);
  MU_RUN_TEST(test_cursor_toggle_there_and_back_again);
  MU_RUN_TEST(test_cursor_toggle_there_and_back_again_visual_multi);
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
