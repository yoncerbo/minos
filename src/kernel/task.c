
#include <stdint.h>
#define BEGIN_TASK() switch (task->_pc) { case 0:
#define END_TASK() }
#define YIELD() task->_pc = __LINE__; return; case __LINE__:

typedef struct {
  uint32_t _pc;
} SimpleTask;

void run_simple_task(SimpleTask *task) {
  BEGIN_TASK();



  END_TASK();
}
