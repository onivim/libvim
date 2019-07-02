#include "libvim.h"
#include "minunit.h"
#include "vim.h"

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
  printf("a\n");
  vimInput("<esc>");
  vimInput("<esc>");
  printf("b\n");
  vimExecute("e!");
  printf("e\n");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_msg2_put)
{
  msg_T *msg = msg2_create(MSG_INFO);
  msg2_put("a", msg);

  mu_check(strcmp(msg2_get_contents(msg), "a") == 0);

  msg2_free(msg);
};

MU_TEST(test_msg2_put_multiple)
{
  msg_T *msg = msg2_create(MSG_INFO);
  msg2_put("ab", msg);
  msg2_put("\n", msg);
  msg2_put("c", msg);

  mu_check(strcmp(msg2_get_contents(msg), "ab\nc") == 0);

  msg2_free(msg);
};

MU_TEST(test_msg2_send_triggers_callback)
{

  msg_T *msg = msg2_create(MSG_INFO);
  msg2_put("testing", msg);
  msg2_send(msg);
  msg2_free(msg);

  mu_check(strcmp(lastMessage, "testing") == 0);
  mu_check(lastPriority == MSG_INFO);
};

MU_TEST(test_msg2_title)
{
  msg_T *msg = msg2_create(MSG_INFO);
  msg2_set_title("test-title", msg);
  msg2_put("test-contents", msg);
  msg2_send(msg);
  msg2_free(msg);

  mu_check(strcmp(lastMessage, "test-contents") == 0);
  mu_check(strcmp(lastTitle, "test-title") == 0);
  mu_check(lastPriority == MSG_INFO);
};

MU_TEST(test_echo)
{
  vimExecute("echo 'hello'");

  mu_check(strcmp(lastMessage, "hello") == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST(test_echom)
{
  vimExecute("echomsg 'hi'");

  mu_check(strcmp(lastMessage, "hi") == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST(test_buffers)
{
  vimExecute("buffers");

  char_u *expected = "\n  2 %a   \"collateral/testfile.txt\"      line 1";
  mu_check(strcmp(lastMessage, expected) == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST(test_files)
{
  vimExecute("files");

  char_u *expected = "\n  2 %a   \"collateral/testfile.txt\"      line 1";
  mu_check(strcmp(lastMessage, expected) == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST(test_error)
{
  vimExecute("buf 999");

  mu_check(strcmp(lastMessage, "E86: Buffer 999 does not exist") == 0);
  mu_check(lastPriority == MSG_ERROR);
}

MU_TEST(test_readonly_warning)
{
  vimExecute("set readonly");

  vimInput("i");
  vimInput("a");

  mu_check(strcmp(lastMessage, "W10: Warning: Changing a readonly file") == 0);
  mu_check(lastPriority == MSG_WARNING);
}

MU_TEST(test_set_print)
{
  vimExecute("set relativenumber?");

  mu_check(strcmp(lastMessage, "norelativenumber") == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST(test_print_marks)
{
  /* Set a mark */
  vimInput("m");
  vimInput("a");

  vimExecute("marks a");

  mu_check(strcmp(lastTitle, "mark line  col file/text") == 0);
  mu_check(strcmp(lastMessage,
                  "\n a      1    0 This is the first line of a test file") ==
           0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST(test_print_jumps)
{
  vimExecute("jumps");

  mu_check(strcmp(lastTitle, " jump line  col file/text") == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST(test_print_changes)
{
  vimExecute("changes");

  mu_check(strcmp(lastTitle, " change line  col text") == 0);
  mu_check(lastPriority == MSG_INFO);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_msg2_put);
  MU_RUN_TEST(test_msg2_put_multiple);
  MU_RUN_TEST(test_msg2_send_triggers_callback);
  MU_RUN_TEST(test_msg2_title);
  MU_RUN_TEST(test_echo);
  MU_RUN_TEST(test_echom);
  MU_RUN_TEST(test_buffers);
  MU_RUN_TEST(test_files);
  MU_RUN_TEST(test_error);
  MU_RUN_TEST(test_readonly_warning);
  MU_RUN_TEST(test_set_print);
  MU_RUN_TEST(test_print_marks);
  MU_RUN_TEST(test_print_jumps);
  MU_RUN_TEST(test_print_changes);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
