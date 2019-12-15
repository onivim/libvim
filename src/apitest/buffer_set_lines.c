#include "libvim.h"
#include "minunit.h"

static int updateCount = 0;
static int lastLnum = 0;
static int lastLnume = 0;
static long lastXtra = 0;
static long lastVersionAtUpdateTime = 0;

void onBufferUpdate(bufferUpdate_T update)
{
  lastLnum = update.lnum;
  lastLnume = update.lnume;
  lastXtra = update.xtra;
  lastVersionAtUpdateTime = vimBufferGetLastChangedTick(curbuf);

  updateCount++;
}

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  updateCount = 0;
  lastLnum = 0;
  lastLnume = 0;
  lastXtra = 0;
}

void test_teardown(void) {}

MU_TEST(test_append_before_buffer)
{
  char_u *lines[] = {"one"};
  vimBufferSetLines(curbuf, 0, 0, lines, 1);

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "one") == 0);
  printf("LINE 2: %s\n", vimBufferGetLine(curbuf, 2));
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "This is the first line of a test file") == 0);
}

MU_TEST(test_append_after_buffer)
{
  char_u *lines[] = {"after"};
  vimBufferSetLines(curbuf, 3, 4, lines, 1);

  printf("LINE 3: %s\n", vimBufferGetLine(curbuf, 3));
  printf("LINE 4: %s\n", vimBufferGetLine(curbuf, 4));
  mu_check(strcmp(vimBufferGetLine(curbuf, 4), "after") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3), "This is the third line of a test file") == 0);
}

MU_TEST(test_append_after_first_line)
{

  char_u *lines[] = {"after first line"};
  vimBufferSetLines(curbuf, 1, 1, lines, 1);

  printf("LINE 1: %s\n", vimBufferGetLine(curbuf, 1));
  printf("LINE 2: %s\n", vimBufferGetLine(curbuf, 2));
  printf("LINE 3: %s\n", vimBufferGetLine(curbuf, 3));
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "after first line") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3), "This is the second line of a test file") == 0);
}

MU_TEST(test_replace_second_line_multiple_lines)
{
  char_u *lines[] = {"new first line", "new second line"};
  vimBufferSetLines(curbuf, 1, 1, lines, 2);

  printf("LINE 1: %s\n", vimBufferGetLine(curbuf, 1));
  printf("LINE 2: %s\n", vimBufferGetLine(curbuf, 2));
  printf("LINE 3: %s\n", vimBufferGetLine(curbuf, 3));
  printf("LINE 4: %s\n", vimBufferGetLine(curbuf, 4));
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "new first line") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3), "new second line") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 4), "This is the second line of a test file") == 0);
}

MU_TEST(test_replace_entire_buffer_from_zero)
{
  char_u *lines[] = {"abc"};
  vimBufferSetLines(curbuf, 0, 3, lines, 1);
  mu_check(vimBufferGetLineCount(curbuf) == 1);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "abc") == 0);
}

MU_TEST(test_replace_entire_buffer_after_first_line)
{
  char_u *lines[] = {"abc"};
  vimBufferSetLines(curbuf, 1, 3, lines, 1);
  mu_check(vimBufferGetLineCount(curbuf) == 2);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "abc") == 0);
}

MU_TEST(test_replace_entire_buffer_with_more_lines)
{
  char_u *lines[] = {"line1", "line2", "line3", "line4", "line5"};
  vimBufferSetLines(curbuf, 0, 3, lines, 5);
  mu_check(vimBufferGetLineCount(curbuf) == 5);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "line1") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 5), "line5") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_append_before_buffer);
  MU_RUN_TEST(test_append_after_buffer);
  MU_RUN_TEST(test_append_after_first_line);
  MU_RUN_TEST(test_replace_second_line_multiple_lines);
  MU_RUN_TEST(test_replace_entire_buffer_from_zero);
  MU_RUN_TEST(test_replace_entire_buffer_after_first_line);
  MU_RUN_TEST(test_replace_entire_buffer_with_more_lines);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetBufferUpdateCallback(&onBufferUpdate);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
