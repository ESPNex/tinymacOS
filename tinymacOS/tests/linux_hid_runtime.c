/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdlib.h>
#include <assert.h>
#include "linux_hid_parser.h"
long tmc_hid_test_alive,tmc_hid_test_fail=-1;
void (*tmc_hid_test_alloc_hook)(void);
void *tmc_hid_os_alloc(size_t n)
{
 if(tmc_hid_test_alloc_hook){void (*hook)(void)=tmc_hid_test_alloc_hook;tmc_hid_test_alloc_hook=NULL;hook();}
 if(!tmc_hid_test_fail)return NULL;if(tmc_hid_test_fail>0)tmc_hid_test_fail--;
 void *p=malloc(n);if(p)__atomic_add_fetch(&tmc_hid_test_alive,1,__ATOMIC_RELAXED);return p;
}
void tmc_hid_os_free(void *p)
{
 if(p){assert(__atomic_sub_fetch(&tmc_hid_test_alive,1,__ATOMIC_RELAXED)>=0);free(p);}
}
