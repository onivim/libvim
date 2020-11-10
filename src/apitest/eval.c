
#include "libvim.h"
#include "minunit.h"

void onEvalVariableSet(char_u *name, typval_T *val)
{
  if (val->v_type == VAR_STRING)
  {
    printf("%s set: %s\n", name, val->vval.v_string);
  }
  else if (val->v_type == VAR_NUMBER)
  {

    printf("%s set: %ld\n", name, val->vval.v_number);
  }
}

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_simple_addition)
{
  char_u *result = vimEval("2+2");

  mu_check(strcmp(result, "4") == 0);
  vim_free(result);
}

MU_TEST(test_empty)
{
  char_u *result = vimEval("");

  mu_check(result == NULL);
}

MU_TEST(test_let_expression)
{
  vimExecute("let mapleader = \"<space>\"");
  vimExecute("let g:mapleader = \"<space>\"");
  vimExecute("let mapleader = 1");

  mu_check(TRUE);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_simple_addition);
  MU_RUN_TEST(test_empty);
  MU_RUN_TEST(test_let_expression);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetEvalVariableSetCallback(&onEvalVariableSet);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
