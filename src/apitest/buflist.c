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

MU_TEST(buffer_load)
{
  buf_T *buf = vimBufferOpen("collateral/curswant.txt", 1, 0);
  long lines = vimBufferGetLineCount(buf);

  mu_check(lines == 4);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(buflist_get_id);
  MU_RUN_TEST(buffer_load);
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
