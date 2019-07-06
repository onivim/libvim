#include "libvim.h"
#include "minunit.h"
#include "vim.h"

#define MAX_TEST_MESSAGE 8192

char_u tempFile[MAX_TEST_MESSAGE];
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
  printf("SETUP - 1\n");
  char_u *tmp = vim_tempname('t', FALSE);
  printf("SETUP - 2\n");
  strcpy(tempFile, tmp);
  printf("SETUP - 3\n");
  vim_free(tmp);

  printf("\nUsing testfile: %s\n", tempFile);
  vimInput("<esc>");
  vimInput("<esc>");
  vimBufferOpen(tempFile, 1, 0);
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_write_while_file_open)
{

  vimInput("i");
  vimInput("a");

  FILE *fp = fopen(tempFile, "w");
  fprintf(fp, "Hello!\n");
  fclose(fp);

  vimExecute("w");

  // Verify file did get overwritten
  char buff[255];
  fp = fopen(tempFile, "r");
  fgets(buff, 255, fp);
  fclose(fp);

  printf("BUF: %s\n", buff);
  mu_check(strcmp(buff, "Hello!\n") == 0);
  printf("test_write_while_file_open - done!\n");
}

/*MU_TEST(test_overwrite_file)
{

  vimInput("i");
  vimInput("a");

  FILE *fp = fopen(tempFile, "w");
  fprintf(fp, "Hello!\n");
  fclose(fp);

  vimExecute("w!");

  // Verify file did not get overwrite
  char buff[255];
  fp = fopen(tempFile, "r");
  fgets(buff, 255, fp);
  fclose(fp);

  printf("BUF: |%s|\n", buff);
  mu_check((strcmp(buff, "a\r\n") == 0) || (strcmp(buff, "a\n") == 0));
}*/

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_write_while_file_open);
//  MU_RUN_TEST(test_overwrite_file);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
