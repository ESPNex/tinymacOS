#pragma once
#include "ui.h"
#define FILEOP_EXISTS -2
#define FILEOP_CYCLE -3
/* Exclusive no-overwrite recursive copy, bounded streaming buffers. */
int fileop_copy(const char*,const char*);
/* Place into a folder; collision names never overwrite. move uses S_MOVE. */
int fileop_place(const char*,const char*,int,char[PATHLEN]);
