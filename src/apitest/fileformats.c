#include "libvim.h"
#include "minunit.h"
#include "unistd.h"
#include "vim.h"

#define MAX_TEST_MESSAGE 8192

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_open_crlf_file)
{
  vimBufferOpen("collateral/test.crlf", 1, 0);

  int ff = get_fileformat(curbuf);
  mu_check(ff == EOL_DOS);
  printf("file format: %d\n", ff);
}

MU_TEST(test_open_lf_file)
{
  vimBufferOpen("collateral/test.lf", 1, 0);

  int ff = get_fileformat(curbuf);
  mu_check(ff == EOL_UNIX);
  printf("file format: %d\n", ff);
}

/*MU_TEST(test_open_cr_file)
{
  vimBufferOpen("collateral/test.cr", 1, 0);

  int ff = get_fileformat(curbuf);
  printf("file format: %d\n", ff);
  mu_check(ff == EOL_MAC);
}*/

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_open_crlf_file);
  MU_RUN_TEST(test_open_lf_file);
  //MU_RUN_TEST(test_open_cr_file);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
