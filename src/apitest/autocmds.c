#include "libvim.h"
#include "minunit.h"

static event_T events[256];
static int idx = 0;

void reset() { idx = 0; }

void onAutoCommand(event_T event, buf_T *buf)
{
  events[idx] = event;
  idx++;
}

int didEvent(event_T evt)
{
  for (int i = 0; i < idx; i++)
  {
    if (events[i] == evt)
    {
      return TRUE;
    }
  }

  return FALSE;
}

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  reset();
}

void test_teardown(void) {}

MU_TEST(test_insertenter_insertleave)
{
  vimInput("i");
  mu_check(didEvent(EVENT_INSERTENTER));

  vimInput("<esc>");
  mu_check(didEvent(EVENT_INSERTLEAVE));
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_insertenter_insertleave);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetAutoCommandCallback(&onAutoCommand);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
