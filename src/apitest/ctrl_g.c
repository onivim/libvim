#include "libvim.h"
#include "minunit.h"

#define MAX_TEST_MESSAGE 8192

char_u lastMessage[MAX_TEST_MESSAGE];
char_u lastTitle[MAX_TEST_MESSAGE];
msgPriority_T lastPriority;

void onMessage(char_u *title, char_u *msg, msgPriority_T priority)
{
  printf("onMessage - title: |%s| contents: |%s|", title, msg);

  assert(strlen(msg) < MAX_TEST_MESSAGE);
  assert(strlen(title) < MAX_TEST_MESSAGE);

  strcpy(lastMessage, msg);
  strcpy(lastTitle, title);
  lastPriority = priority;
};

void test_setup(void)
{
  vimSetMessageCallback(&onMessage);

  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_fileinfo)
{
  vimKey("<c-g>");

  char_u *expected = "\"collateral/testfile.txt\" line 1 of 3 --33\%-- col 1";
  mu_check(strcmp(lastMessage, expected) == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_fileinfo);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
