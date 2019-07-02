#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_get_set_tab_options)
{
  vimOptionSetTabSize(4);
  mu_check(vimOptionGetTabSize() == 4);

  vimOptionSetTabSize(2);
  mu_check(vimOptionGetTabSize() == 2);

  vimOptionSetInsertSpaces(TRUE);
  mu_check(vimOptionGetInsertSpaces() == TRUE);

  vimOptionSetInsertSpaces(FALSE);
  mu_check(vimOptionGetInsertSpaces() == FALSE);
}

MU_TEST(test_insert_spaces)
{
  vimOptionSetTabSize(3);
  vimOptionSetInsertSpaces(TRUE);

  vimInput("I");
  vimInput("<tab>");

  char_u *line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "   Line 1") == 0);

  vimInput("<bs>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "Line 1") == 0);

  vimOptionSetTabSize(4);

  vimInput("<tab>");
  vimInput("<tab>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "        Line 1") == 0);

  vimInput("<bs>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "    Line 1") == 0);
}

MU_TEST(test_insert_tabs)
{
  vimOptionSetTabSize(3);
  vimOptionSetInsertSpaces(FALSE);

  vimInput("I");
  vimInput("<tab>");

  char_u *line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "\tLine 1") == 0);

  vimInput("<bs>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "Line 1") == 0);

  vimOptionSetTabSize(4);

  vimInput("<tab>");
  vimInput("<tab>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "\t\tLine 1") == 0);

  vimInput("<bs>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "\tLine 1") == 0);
}

MU_TEST(test_tab_size)
{
  vimOptionSetTabSize(3);
  int calculatedTabSize = chartabsize("\t", 0);
  mu_check(calculatedTabSize == 3);

  vimOptionSetTabSize(4);
  calculatedTabSize = chartabsize("\t", 0);
  mu_check(calculatedTabSize == 4);
}

MU_TEST(test_encoding_cannot_change)
{

  mu_check(strcmp(p_enc, "utf-8") == 0);
  vimExecute("set encoding=latin1");
  mu_check(strcmp(p_enc, "utf-8") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_get_set_tab_options);
  MU_RUN_TEST(test_insert_spaces);
  MU_RUN_TEST(test_insert_tabs);
  MU_RUN_TEST(test_tab_size);
  MU_RUN_TEST(test_encoding_cannot_change);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
