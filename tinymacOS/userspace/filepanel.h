#pragma once
#include "filebrowser.h"
/* Application-owned modal sheet. Return 1 only after a validated choice;
 * no file is created/truncated until the owning application writes it. */
typedef struct {
    int active, save, overwrite, selected, offset, focus, cursor, replace, newfolder;
    int count, x, y, w, h, side, rows;
    u32 revision;
    char cwd[PATHLEN], name[PATHLEN], location[PATHLEN], result[PATHLEN];
    char message[96], folder[48];
    Dirent entries[NSLOTS];
} FilePanel;
void filepanel_open(FilePanel *, int, const char *);
void filepanel_draw(FilePanel *);
int filepanel_event(FilePanel *, const Event *);
void filepanel_refresh(FilePanel *);
