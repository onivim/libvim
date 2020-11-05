#include "libvim.h"
#include "minunit.h"
#include "unistd.h"
#include "vim.h"

#define MAX_TEST_MESSAGE 8192

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_open_crlf_file)
{
  vimBufferOpen("collateral/test.crlf", 1, 0);

  int ff = vimBufferGetFileFormat(curbuf);
  printf("file format: %d\n", ff);
  mu_check(ff == EOL_DOS);
}

MU_TEST(test_open_lf_file)
{
  vimBufferOpen("collateral/test.lf", 1, 0);

  int ff = vimBufferGetFileFormat(curbuf);
  printf("file format: %d\n", ff);
  mu_check(ff == EOL_UNIX);
}

MU_TEST(test_write_crlf_file)
{
  vimBufferOpen("collateral/test.crlf", 1, 0);

  char_u *tmp = vim_tempname('t', FALSE);

  char_u cmd[8192];
  sprintf(cmd, "w %s", tmp);

  vimExecute(cmd);

  // Verify file did get overwritten
  char buff[255];
  FILE *fp = fopen(tmp, "rb");
  // Get first line
  fgets(buff, 255, fp);
  fclose(fp);

  mu_check(strcmp(buff, "a\r\n") == 0);

  vim_free(tmp);
}

MU_TEST(test_write_lf_file)
{
  vimBufferOpen("collateral/test.lf", 1, 0);
  char_u *tmp = vim_tempname('t', FALSE);

  char_u cmd[8192];
  sprintf(cmd, "w %s", tmp);

  vimExecute(cmd);

  // Verify file did get overwritten
  char buff[255];
  FILE *fp = fopen(tmp, "rb");
  // Get first line
  fgets(buff, 255, fp);
  fclose(fp);

  mu_check(strcmp(buff, "a\n") == 0);
  vim_free(tmp);
}

MU_TEST(test_convert_crlf_to_lf)
{
  buf_T *buf = vimBufferOpen("collateral/test.crlf", 1, 0);
  vimBufferSetFileFormat(buf, EOL_UNIX);

  int ff = vimBufferGetFileFormat(buf);
  mu_check(ff == EOL_UNIX);
}

MU_TEST(test_convert_lf_to_crlf)
{
  buf_T *buf = vimBufferOpen("collateral/test.lf", 1, 0);
  vimBufferSetFileFormat(buf, EOL_DOS);

  int ff = vimBufferGetFileFormat(buf);
  mu_check(ff == EOL_DOS);
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
  MU_RUN_TEST(test_write_crlf_file);
  MU_RUN_TEST(test_write_lf_file);
  MU_RUN_TEST(test_convert_crlf_to_lf);
  MU_RUN_TEST(test_convert_lf_to_crlf);
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
