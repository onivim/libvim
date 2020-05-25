#include "libvim.h"
#include "minunit.h"

static formatRequestType_t lastRequestType = INDENTATION;
static int lastReturnCursor = -1;
static int lastStartLine = 0;
static int lastEndLine = 0;
static buf_T *lastBuf = NULL;

void onFormat(formatRequest_T *formatRequest)
{
  printf("onFormat - type: |%d| returnCursor: |%d| startLine: |%d| endLine: |%d|",
  formatRequest->formatType,
  formatRequest->returnCursor,
  formatRequest->start.lnum,
  formatRequest->end.lnum);

};

void test_setup(void)
{

  lastRequestType = INDENTATION;
  lastReturnCursor = -1;
  lastStartLine = -1;
  lastEndLine = -1;
  lastBuf = NULL;

  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(regression_test_no_crash_after_set_si)
{
  vimInput(":set si<CR>");
  vimInput("o");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(regression_test_no_crash_after_set_si);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);
  vimSetFormatCallback(&onFormat);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
