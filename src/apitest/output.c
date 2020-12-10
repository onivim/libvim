#include "libvim.h"
#include "minunit.h"
#include "vim.h"

static char_u *lastCmd = NULL;
static char_u *lastOutput = NULL;
static int outputCount = 0;

void onOutput(char_u *cmd, char_u *output)
{
  printf("onOutput - cmd: |%s| output: |%s|\n", cmd, output);

  if (lastCmd != NULL)
  {
    vim_free(lastCmd);
    lastCmd = NULL;
  }

  if (cmd != NULL) {
    lastCmd = strdup(cmd);
  }

  if (lastOutput != NULL)
  {
    vim_free(lastOutput);
    lastOutput = NULL;
  };

  if (output != NULL) {
    lastOutput = strdup(output);
  }
  outputCount++;
};

void test_setup(void)
{
  outputCount = 0;
  if (lastCmd != NULL)
  {
    vim_free(lastCmd);
    lastCmd = NULL;
  }
  if (lastOutput != NULL)
  {
    vim_free(lastOutput);
    lastOutput = NULL;
  }
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void)
{
  if (lastCmd != NULL)
  {
    vim_free(lastCmd);
    lastCmd = NULL;
  }
  if (lastOutput != NULL)
  {
    vim_free(lastOutput);
    lastOutput = NULL;
  }
}

MU_TEST(test_ex_bang_ls)
{
  vimExecute("!ls");

  mu_check(outputCount == 1);
  mu_check(strcmp(lastCmd, "ls") == 0);
  mu_check(strlen(lastOutput) > 0);
}

MU_TEST(test_ex_bang_echo)
{
  vimExecute("!echo 'hi'");

  mu_check(outputCount == 1);
  mu_check(strcmp(lastCmd, "echo 'hi'") == 0);
  mu_check(strlen(lastOutput) > 0);
}

MU_TEST(test_ex_read_cmd)
{
  size_t originalBufferLength = vimBufferGetLineCount(vimBufferGetCurrent());
  vimExecute("read !ls .");

  mu_check(outputCount == 0);
  size_t newBufferLength = vimBufferGetLineCount(vimBufferGetCurrent());
  mu_check(newBufferLength > originalBufferLength);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_ex_bang_ls);
  MU_RUN_TEST(test_ex_bang_echo);
  MU_RUN_TEST(test_ex_read_cmd);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetOutputCallback(&onOutput);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
