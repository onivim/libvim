#include "libvim.h"
#include "minunit.h"

static formatRequestType_T lastRequestType = INDENTATION;
static int lastReturnCursor = -1;
static linenr_T lastStartLine = 0;
static linenr_T lastEndLine = 0;
static buf_T *lastBuf = NULL;
static char_u *lastCmd = NULL;
static int callCount = 0;

void onFormat(formatRequest_T *formatRequest)
{
  printf("onFormat - type: |%d| returnCursor: |%d| startLine: |%ld| endLine: |%ld|",
         formatRequest->formatType,
         formatRequest->returnCursor,
         formatRequest->start.lnum,
         formatRequest->end.lnum);

  lastRequestType = formatRequest->formatType;
  lastReturnCursor = formatRequest->returnCursor;
  lastStartLine = formatRequest->start.lnum;
  lastEndLine = formatRequest->end.lnum;
  lastBuf = formatRequest->buf;
  lastCmd = formatRequest->cmd;
  callCount++;
};

void test_setup(void)
{
  lastRequestType = INDENTATION;
  lastReturnCursor = -1;
  lastStartLine = -1;
  lastEndLine = -1;
  lastBuf = NULL;
  callCount = 0;

  // Reset formatexpr, formatprg, and equalprg to defaults
  vimExecute("set formatexpr&");
  vimExecute("set formatprg&");
  vimExecute("setlocal formatprg&");
  vimExecute("set equalprg&");

  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(regression_test_no_crash_after_set_si)
{
  vimInput(":set si");
  vimKey("<CR>");
  vimInput("o");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "") == 0);
}

MU_TEST(indent_line)
{
  vimInput("=");
  vimInput("=");

  // format callback should've been called
  mu_check(callCount == 1);
  mu_check(lastStartLine == 1);
  mu_check(lastEndLine == 1);
  mu_check(lastRequestType == INDENTATION);
  mu_check(*lastCmd == NUL);
}

MU_TEST(indent_line_range)
{
  vimInput("=");
  vimInput("2");
  vimInput("j");

  mu_check(callCount == 1);
  mu_check(lastStartLine == 1);
  mu_check(lastEndLine == 3);
  mu_check(lastRequestType == INDENTATION);
  mu_check(*lastCmd == NUL);
}

MU_TEST(indent_line_equalprg)
{
  vimExecute("set equalprg=indent");
  vimInput("=");
  vimInput("2");
  vimInput("j");

  mu_check(callCount == 1);
  mu_check(lastStartLine == 1);
  mu_check(lastEndLine == 3);
  mu_check(lastRequestType == INDENTATION);
  printf("EQUALPRG: %s\n", lastCmd);
  mu_check(strcmp(lastCmd, "indent") == 0);
}

MU_TEST(format_gq)
{
  vimInput("g");
  vimInput("q");
  vimInput("2");
  vimInput("j");

  mu_check(callCount == 1);
  mu_check(lastStartLine == 1);
  mu_check(lastEndLine == 3);
  mu_check(lastRequestType == FORMATTING);
  mu_check(lastReturnCursor == 0);
  mu_check(*lastCmd == NUL);
}

MU_TEST(format_gw)
{
  vimInput("g");
  vimInput("w");
  vimInput("j");

  mu_check(callCount == 1);
  mu_check(lastStartLine == 1);
  mu_check(lastEndLine == 2);
  mu_check(lastRequestType == FORMATTING);
  mu_check(lastReturnCursor == 1);
  mu_check(*lastCmd == NUL);
}

MU_TEST(format_gq_buflocal_formatprg)
{
  vimExecute("setlocal formatprg=format");
  vimInput("g");
  vimInput("w");
  vimInput("j");

  mu_check(callCount == 1);
  mu_check(lastStartLine == 1);
  mu_check(lastEndLine == 2);
  mu_check(lastRequestType == FORMATTING);
  mu_check(lastReturnCursor == 1);
  printf("FORMATPRG: %s\n", lastCmd);
  mu_check(strcmp(lastCmd, "format") == 0);
}

MU_TEST(format_gq_buflocal_and_global_formatprg)
{
  vimExecute("set formatprg=format1");
  vimExecute("setlocal formatprg=format2");
  vimInput("g");
  vimInput("w");
  vimInput("j");

  mu_check(callCount == 1);
  mu_check(lastStartLine == 1);
  mu_check(lastEndLine == 2);
  mu_check(lastRequestType == FORMATTING);
  mu_check(lastReturnCursor == 1);
  printf("FORMATPRG: %s\n", lastCmd);
  mu_check(strcmp(lastCmd, "format2") == 0);
}

MU_TEST(formatexpr_overrides_callback)
{
  vimExecute("set formatexpr=noop");
  vimInput("g");
  vimInput("q");
  vimInput("j");

  mu_check(callCount == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(regression_test_no_crash_after_set_si);
  MU_RUN_TEST(indent_line);
  MU_RUN_TEST(indent_line_range);
  MU_RUN_TEST(indent_line_equalprg);

  MU_RUN_TEST(format_gq);
  MU_RUN_TEST(format_gw);
  MU_RUN_TEST(format_gq_buflocal_formatprg);
  MU_RUN_TEST(format_gq_buflocal_and_global_formatprg);
  MU_RUN_TEST(formatexpr_overrides_callback);
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
