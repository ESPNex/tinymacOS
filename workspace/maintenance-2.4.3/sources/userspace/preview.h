#pragma once
#include "ui.h"
void preview_read(const char *path, const Dirent *file, int icon_id);
void preview_draw(void);
void inspect_open(const char *, const Dirent *);
void inspect_close(void);
int inspect_step(void);
void inspect_draw(void);
void inspect_nav(int);
int inspect_click(int, int);
