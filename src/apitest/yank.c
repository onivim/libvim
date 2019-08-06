#include "libvim.h"
#include "minunit.h"
#include "vim.h"

static int yankCount = 0;
static int lastYankLineCount = -1;
static int lastRegname = 0;
static char **lastYankLines = NULL;
static pos_T lastStart;
static pos_T lastEnd;
static int lastYankType = -1;
static int lastOpChar = -1;

void disposeLastYankInfo(void)
{
  if (lastYankLines != NULL)
  {

    for (int i = 0; i < lastYankLineCount; i++)
    {
      vim_free(lastYankLines[i]);
    }

    vim_free(lastYankLines);
  }

  lastYankLineCount = -1;
  lastYankLines = NULL;
  lastRegname = 0;
  lastOpChar = 0;
}

void onYank(yankInfo_T *yankInfo)
{
  disposeLastYankInfo();

  lastYankLineCount = yankInfo->numLines;
  lastYankLines = alloc(sizeof(char_u *) * yankInfo->numLines);
  lastStart = yankInfo->start;
  lastEnd = yankInfo->end;
  lastYankType = yankInfo->blockType;
  lastOpChar = yankInfo->op_char;
  lastRegname = yankInfo->regname;

  for (int i = 0; i < lastYankLineCount; i++)
  {
    int len = strlen(yankInfo->lines[i]);
    char_u *sz = alloc(sizeof(char_u) * (len + 1));
    strcpy(sz, yankInfo->lines[i]);
    lastYankLines[i] = sz;
  };
  yankCount++;
};

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  yankCount = 0;
}

void test_teardown(void)
{
  disposeLastYankInfo();
}

MU_TEST(test_yank_line)
{

  vimInput("y");
  vimInput("y");

  mu_check(yankCount == 1);
  mu_check(lastYankLineCount == 1);
  mu_check(lastOpChar == 'y');
  mu_check(lastYankType == MLINE);
  mu_check(lastRegname == 0);
  mu_check(strcmp(lastYankLines[0], "This is the first line of a test file") == 0);
};

MU_TEST(test_yank_register)
{

  vimInput("\"");
  vimInput("c");
  vimInput("y");
  vimInput("y");

  mu_check(yankCount == 1);
  mu_check(lastYankLineCount == 1);
  mu_check(lastOpChar == 'y');
  mu_check(lastYankType == MLINE);
  mu_check(lastRegname == 'c');
  mu_check(strcmp(lastYankLines[0], "This is the first line of a test file") == 0);
};

MU_TEST(test_clipboard_registers)
{

  vimInput("\"");
  vimInput("+");
  vimInput("y");
  vimInput("y");

  printf("LASTREGNAME: %c|%d\n", lastRegname, lastRegname);
  mu_check(yankCount == 1);
  mu_check(lastRegname == '+');

  vimInput("\"");
  vimInput("*");
  vimInput("y");
  vimInput("y");

  mu_check(yankCount == 2);
  mu_check(lastRegname == '*');
};

MU_TEST(test_delete_line)
{

  vimInput("d");
  vimInput("d");

  mu_check(yankCount == 1);
  mu_check(lastYankLineCount == 1);
  mu_check(lastYankType == MLINE);
  mu_check(strcmp(lastYankLines[0], "This is the first line of a test file") == 0);
};

MU_TEST(test_delete_two_lines)
{
  vimInput("d");
  vimInput("j");

  mu_check(yankCount == 1);
  mu_check(lastYankLineCount == 2);
  mu_check(lastYankType == MLINE);
  mu_check(lastOpChar == 'd');
  mu_check(strcmp(lastYankLines[0], "This is the first line of a test file") == 0);
  mu_check(strcmp(lastYankLines[1], "This is the second line of a test file") == 0);
};

MU_TEST(test_delete_char)
{
  vimInput("x");

  mu_check(yankCount == 1);

  mu_check(lastYankLineCount == 1);
  mu_check(lastYankType == MCHAR);
  mu_check(lastOpChar == 'd');
  mu_check(strcmp(lastYankLines[0], "T") == 0);
};

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_delete_char);
  MU_RUN_TEST(test_delete_line);
  MU_RUN_TEST(test_delete_two_lines);
  MU_RUN_TEST(test_yank_line);
  MU_RUN_TEST(test_yank_register);
  MU_RUN_TEST(test_clipboard_registers);
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
