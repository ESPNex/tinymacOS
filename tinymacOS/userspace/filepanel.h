#pragma once
#include "filebrowser.h"
#define TEXTEDIT_LIMIT (256u*1024u)
/* Application-owned modal sheet. Return 1 only after a validated choice;
 * no file is created/truncated until the owning application writes it. */
typedef struct {
    int active, save, overwrite, selected, offset, focus, cursor, replace, newfolder;
    int count, pagebase, more, x, y, w, h, side, rows;
    u32 revision;
    char cwd[PATHLEN], name[PATHLEN], location[PATHLEN], result[PATHLEN];
    char message[96], folder[48];
    Dirent entries[BROWSER_PAGE];
} FilePanel;
void filepanel_open(FilePanel *, int, const char *);
void filepanel_draw(FilePanel *);
int filepanel_event(FilePanel *, const Event *);
void filepanel_refresh(FilePanel *);
