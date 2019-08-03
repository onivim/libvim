#include "libvim.h"
#include "minunit.h"
#include "vim.h"

void onYank(yankInfo_T *yankInfo)
{
};

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  onDirectoryChangedCount = 0;
}

void test_teardown(void) {}

MU_TEST(test_yank_line)
{
  // TODO
};

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_yank_line);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetYankCallback(&onYank);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
