#include "libvim.h"
#include "minunit.h"

static int updateCount = 0;
static int lastLnum = 0;
static int lastLnume = 0;
static long lastXtra = 0;
static long lastVersionAtUpdateTime = 0;

#define MAX_TEST_MESSAGE 8192

static char_u lastMessage[MAX_TEST_MESSAGE];
static char_u lastTitle[MAX_TEST_MESSAGE];
static msgPriority_T lastPriority;
static int messageCount = 0;

void onMessage(char_u *title, char_u *msg, msgPriority_T priority)
{
  printf("onMessage - title: |%s| contents: |%s|", title, msg);

  assert(strlen(msg) < MAX_TEST_MESSAGE);
  assert(strlen(title) < MAX_TEST_MESSAGE);

  strcpy(lastMessage, msg);
  strcpy(lastTitle, title);
  lastPriority = priority;
  messageCount++;
};

void onBufferUpdate(bufferUpdate_T update)
{
  lastLnum = update.lnum;
  lastLnume = update.lnume;
  lastXtra = update.xtra;
  lastVersionAtUpdateTime = vimBufferGetLastChangedTick(curbuf);

  updateCount++;
}

void test_setup(void)
{
  vimBufferSetModifiable(curbuf, TRUE);
  vimBufferSetReadOnly(curbuf, FALSE);

  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  updateCount = 0;
  lastLnum = 0;
  lastLnume = 0;
  lastXtra = 0;

  messageCount = 0;
}

void test_teardown(void) {}

MU_TEST(test_get_set_modifiable)
{
  vimBufferSetModifiable(curbuf, FALSE);
  mu_check(vimBufferGetModifiable(curbuf) == FALSE);

  vimBufferSetModifiable(curbuf, TRUE);
  mu_check(vimBufferGetModifiable(curbuf) == TRUE);
}

MU_TEST(test_get_set_readonly)
{
  vimBufferSetReadOnly(curbuf, FALSE);
  mu_check(vimBufferGetReadOnly(curbuf) == FALSE);

  vimBufferSetReadOnly(curbuf, TRUE);
  mu_check(vimBufferGetReadOnly(curbuf) == TRUE);
}

MU_TEST(test_error_msg_nomodifiable)
{
  vimBufferSetModifiable(curbuf, FALSE);

  vimInput("o");

  // Verify no change to the buffer...
  mu_check(updateCount == 0);
  // ...but we shouldn've gotten an error message
  mu_check(messageCount == 1);
  mu_check(lastPriority == MSG_ERROR);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_get_set_modifiable);
  MU_RUN_TEST(test_get_set_readonly);
  MU_RUN_TEST(test_error_msg_nomodifiable);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetMessageCallback(&onMessage);
  vimSetBufferUpdateCallback(&onBufferUpdate);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
