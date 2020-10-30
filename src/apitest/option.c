#include "libvim.h"
#include "minunit.h"

int optionSetCount = 0;
optionSet_T lastOptionSet;

void onOptionSet(optionSet_T *options)
{
  if (lastOptionSet.stringval != NULL)
  {
    vim_free(lastOptionSet.stringval);
  }

  lastOptionSet.fullname = options->fullname;
  lastOptionSet.shortname = options->shortname;
  lastOptionSet.type = options->type;
  lastOptionSet.numval = options->numval;
  if (options->stringval != NULL)
  {
    lastOptionSet.stringval = vim_strsave(options->stringval);
  }
  else
  {
    lastOptionSet.stringval = NULL;
  }
  lastOptionSet.hidden = options->hidden;
  optionSetCount++;
}

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
  optionSetCount = 0;
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
  vimKey("<tab>");

  char_u *line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "   Line 1") == 0);

  vimKey("<bs>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "Line 1") == 0);

  vimOptionSetTabSize(4);

  vimKey("<tab>");
  vimKey("<tab>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "        Line 1") == 0);

  vimKey("<bs>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "    Line 1") == 0);
}

MU_TEST(test_insert_tabs)
{
  vimOptionSetTabSize(3);
  vimOptionSetInsertSpaces(FALSE);

  vimInput("I");
  vimKey("<tab>");

  char_u *line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "\tLine 1") == 0);

  vimKey("<bs>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "Line 1") == 0);

  vimOptionSetTabSize(4);

  vimKey("<tab>");
  vimKey("<tab>");
  line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "\t\tLine 1") == 0);

  vimKey("<bs>");
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

MU_TEST(test_opt_relative_number)
{
  vimExecute("set rnu");
  mu_check(optionSetCount == 1);
  mu_check(strcmp(lastOptionSet.fullname, "relativenumber") == 0);
  mu_check(strcmp(lastOptionSet.shortname, "rnu") == 0);
  mu_check(lastOptionSet.numval == 1);
  mu_check(lastOptionSet.type == 1);

  vimExecute("set nornu");
  mu_check(optionSetCount == 2);
  mu_check(strcmp(lastOptionSet.fullname, "relativenumber") == 0);
  mu_check(strcmp(lastOptionSet.shortname, "rnu") == 0);
  mu_check(lastOptionSet.numval == 0);
  mu_check(lastOptionSet.type == 1);
}

MU_TEST(test_opt_codelens)
{
  vimExecute("set codelens");
  mu_check(optionSetCount == 1);
  mu_check(strcmp(lastOptionSet.fullname, "codelens") == 0);
  mu_check(lastOptionSet.shortname == NULL);
  mu_check(lastOptionSet.numval == 1);
  mu_check(lastOptionSet.type == 1);

  vimExecute("set nocodelens");
  mu_check(optionSetCount == 2);
  mu_check(strcmp(lastOptionSet.fullname, "codelens") == 0);
  mu_check(lastOptionSet.shortname == NULL);
  mu_check(lastOptionSet.numval == 0);
  mu_check(lastOptionSet.type == 1);
}

MU_TEST(test_opt_minimap)
{
  vimExecute("set minimap");
  mu_check(optionSetCount == 1);
  mu_check(strcmp(lastOptionSet.fullname, "minimap") == 0);
  mu_check(lastOptionSet.shortname == NULL);
  mu_check(lastOptionSet.numval == 1);
  mu_check(lastOptionSet.type == 1);

  vimExecute("set nominimap");
  mu_check(optionSetCount == 2);
  mu_check(strcmp(lastOptionSet.fullname, "minimap") == 0);
  mu_check(lastOptionSet.shortname == NULL);
  mu_check(lastOptionSet.numval == 0);
  mu_check(lastOptionSet.type == 1);
}

MU_TEST(test_opt_smoothscroll)
{
  vimExecute("set smoothscroll");
  mu_check(optionSetCount == 1);
  mu_check(strcmp(lastOptionSet.fullname, "smoothscroll") == 0);
  mu_check(lastOptionSet.shortname == NULL);
  mu_check(lastOptionSet.numval == 1);
  mu_check(lastOptionSet.type == 1);

  vimExecute("set nosmoothscroll");
  mu_check(optionSetCount == 2);
  mu_check(strcmp(lastOptionSet.fullname, "smoothscroll") == 0);
  mu_check(lastOptionSet.shortname == NULL);
  mu_check(lastOptionSet.numval == 0);
  mu_check(lastOptionSet.type == 1);
}

MU_TEST(test_opt_runtimepath)
{
  vimExecute("set runtimepath=abc");
  mu_check(optionSetCount == 1);
  mu_check(strcmp(lastOptionSet.fullname, "runtimepath") == 0);
  mu_check(strcmp(lastOptionSet.shortname, "rtp") == 0);
  mu_check(strcmp(lastOptionSet.stringval, "abc") == 0);
  mu_check(lastOptionSet.type == 0);
}

MU_TEST(test_opt_backspace_string)
{
  vimExecute("set backspace=indent,eol");
  mu_check(optionSetCount == 1);
  mu_check(strcmp(lastOptionSet.fullname, "backspace") == 0);
  mu_check(strcmp(lastOptionSet.shortname, "bs") == 0);
  mu_check(strcmp(lastOptionSet.stringval, "indent,eol") == 0);
  mu_check(lastOptionSet.type == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_get_set_tab_options);
  MU_RUN_TEST(test_insert_spaces);
  MU_RUN_TEST(test_insert_tabs);
  MU_RUN_TEST(test_tab_size);
  MU_RUN_TEST(test_encoding_cannot_change);
  MU_RUN_TEST(test_opt_relative_number);
  MU_RUN_TEST(test_opt_codelens);
  MU_RUN_TEST(test_opt_minimap);
  MU_RUN_TEST(test_opt_smoothscroll);
  MU_RUN_TEST(test_opt_runtimepath);
  MU_RUN_TEST(test_opt_backspace_string);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetOptionSetCallback(&onOptionSet);
  vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
