#include "libvim.h"
#include "minunit.h"
#include "unistd.h"
#include "vim.h"

#define MAX_TEST_MESSAGE 8192

char_u tempFile[MAX_TEST_MESSAGE];
char_u lastMessage[MAX_TEST_MESSAGE];
char_u lastTitle[MAX_TEST_MESSAGE];
msgPriority_T lastPriority;

int writeFailureCount = 0;
writeFailureReason_T lastWriteFailureReason;

void onMessage(char_u *title, char_u *msg, msgPriority_T priority)
{
  printf("onMessage - title: |%s| contents: |%s|", title, msg);

  assert(strlen(msg) < MAX_TEST_MESSAGE);
  assert(strlen(title) < MAX_TEST_MESSAGE);

  strcpy(lastMessage, msg);
  strcpy(lastTitle, title);
  lastPriority = priority;
};

void onWriteFailure(writeFailureReason_T reason, buf_T *buf) {
  printf("onWriteFailure - reason: %d\n", reason);

  lastWriteFailureReason = reason;
  writeFailureCount++;
};

void test_setup(void)
{
  writeFailureCount = 0;
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
/*
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

MU_TEST(test_overwrite_file)
{

  vimInput("i");
  vimInput("a");


  mu_check(1 == 1);
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

void printFile(char_u *fileName) {
  
  FILE *fp = fopen(fileName, "r");
  char c;
  while (1) {
    c = fgetc(fp);
    if (feof(fp)) {
      break;
    }
    printf("%c", c);
  }
  fclose(fp);
};

MU_TEST(test_modify_file_externally)
{
  vimInput("i");
  vimInput("a");
  vimInput("<esc>");
  vimExecute("w");

  // HACK: This sleep is required to get different 'mtimes'
  // for Vim to realize that th ebfufer is modified
  sleep(3);

  printf("buffer name: %s\n", vimBufferGetFilename(curbuf));

  mu_check(writeFailureCount == 0);
  printf("TEMPFILE path: %s\n", tempFile);
  FILE *fp = fopen(tempFile, "w");
  fprintf(fp, "Hello!\n");
  fclose(fp);
  
  vimExecute("u");
  vimExecute("w");
  
  mu_check(writeFailureCount == 1);
  mu_check(lastWriteFailureReason == FILE_CHANGED);
}

MU_TEST(test_check_if_changed_updates_buffer)
{

  mu_check(vimBufferCheckIfChanged(curbuf) == 0);
  vimInput("i");
  vimInput("a");
  vimInput("<esc>");
  vimExecute("w");

  // HACK: This sleep is required to get different 'mtimes'
  // for Vim to realize that th ebfufer is modified
  sleep(3);

  printf("buffer name: %s\n", vimBufferGetFilename(curbuf));

  mu_check(writeFailureCount == 0);
  printf("TEMPFILE path: %s\n", tempFile);
  FILE *fp = fopen(tempFile, "w");
  fprintf(fp, "Hello!\n");
  fclose(fp);
  
  int v = vimBufferCheckIfChanged(curbuf);
  /* Should return 1 because the buffer was changed */
  /* Should we get a buffer update? */
  mu_check(v == 1);
  
  /* With auto-read, we should've picked up the change */
  char_u *line = vimBufferGetLine(curbuf, 1);
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "Hello!") == 0);
}

/* Test autoread - get latest buffer update */

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  /*MU_RUN_TEST(test_write_while_file_open); */
  /*MU_RUN_TEST(test_overwrite_file);*/
  MU_RUN_TEST(test_checkifchanged);
  MU_RUN_TEST(test_modify_file_externally);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetFileWriteFailureCallback(&onWriteFailure);
  vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
