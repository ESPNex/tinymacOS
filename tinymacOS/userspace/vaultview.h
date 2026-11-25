#pragma once
#include "ui.h"
void vault_view_init(void);
void vault_view_draw(int x,int y,int w,int h);
void vault_view_event(const Event *e);
int vault_view_tick(void);
int vault_view_can_leave(void);
int vault_view_can_close(void);
void vault_view_leave(void);
