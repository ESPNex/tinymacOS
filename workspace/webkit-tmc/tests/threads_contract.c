#include <assert.h>
#include <stdio.h>
#include "thread_cases.h"
static int test_vm_lifetime(void){return 0;}
int main(void)
{
 int result=run_thread_cases();
 printf("%s managed-thread contract: created=%u updates=%u first-failure-line=%u; host transport only, not native FS/scheduler\n",result?"FAIL":"PASS",created,counter,first_failure);
 assert(!result && created==70 && counter==320);
}
