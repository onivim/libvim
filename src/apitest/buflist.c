#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(buflist_get_id)
{
  buf_T *current = vimBufferGetCurrent();
  int currentId = vimBufferGetId(current);

  mu_check(vimBufferGetById(currentId) == current);
}

MU_TEST(buffer_open)
{
  buf_T *buf = vimBufferOpen("collateral/curswant.txt", 1, 0);
  long lines = vimBufferGetLineCount(buf);

  mu_check(lines == 4);
}

MU_TEST(buffer_load_nonexistent_file)
{
  buf_T *buf = vimBufferLoad("a-non-existent-file.txt", 1, 0);
  long lines = vimBufferGetLineCount(buf);
  mu_check(lines == 1);
}

MU_TEST(buffer_load_does_not_change_current)
{
  buf_T *bufOpen = vimBufferOpen("collateral/curswant.txt", 1, 0);
  buf_T *bufLoaded = vimBufferLoad("a-non-existent-file.txt", 1, 0);
  long loadedLines = vimBufferGetLineCount(bufLoaded);
  mu_check(loadedLines == 1);

  long openLines = vimBufferGetLineCount(bufOpen);
  mu_check(openLines == 4);

  buf_T *currentBuf = vimBufferGetCurrent();

  mu_check(currentBuf == bufOpen);
}

MU_TEST(buffer_load_read_lines)
{
  buf_T *bufLoaded = vimBufferLoad("collateral/testfile.txt", 1, 0);
  mu_check(strcmp(vimBufferGetLine(bufLoaded, 1), "This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(bufLoaded, 2), "This is the second line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(bufLoaded, 3), "This is the third line of a test file") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(buflist_get_id);
  MU_RUN_TEST(buffer_open);
  MU_RUN_TEST(buffer_load_nonexistent_file);
  MU_RUN_TEST(buffer_load_does_not_change_current);
  MU_RUN_TEST(buffer_load_read_lines);
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
