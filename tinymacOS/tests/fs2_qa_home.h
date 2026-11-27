#pragma once
#include "home_name.h"
static u64 qa_home(Tmc2*fs,u64 users){T2Entry e;if(!tmc2_lookup(fs,users,"User",&e))return e.key;u8 record[512];char name[48];for(int slot=0;slot<2;slot++)if(!tmc2_account(fs,slot,record,0)&&home_account_name(record,name)&&!tmc2_lookup(fs,users,name,&e)&&e.kind==T2_DIR){erase(record,512);return e.key;}erase(record,512);return 0;}
