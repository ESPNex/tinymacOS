/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host memory/time imports only. Original config.c is linked unchanged. */
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include "linux_usb_config.h"
long tmc_config_test_alive,tmc_config_test_fail=-1;
void *tmc_config_os_alloc(size_t n)
{
 if(!tmc_config_test_fail)return NULL;if(tmc_config_test_fail>0)tmc_config_test_fail--;
 void *p=malloc(n);if(p)tmc_config_test_alive++;return p;
}
void tmc_config_os_free(void *p){if(p){assert(tmc_config_test_alive>0);tmc_config_test_alive--;free(p);}}
void tmc_config_os_delay(unsigned ms)
{
 struct timespec delay={(time_t)(ms/1000),(long)(ms%1000)*1000000};
 while(nanosleep(&delay,&delay)&&errno==EINTR){}
}
