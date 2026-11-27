#pragma once
#include "ui.h"
#define BROWSER_PLACES 11
#define BROWSER_PAGE 64
extern const char *const browser_labels[BROWSER_PLACES];
extern const char *const browser_paths[BROWSER_PLACES];
extern const int browser_icons[BROWSER_PLACES];
const char *browser_label(int);
int browser_list(const char *, Dirent *, int);
int browser_finder(const char *);
int browser_page(const char*,Dirent*,int,int,int*);
