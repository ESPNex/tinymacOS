#pragma once
#define CLIP_BYTES 262145
#define CLIP_TEXT 1
#define CLIP_FILE 2
/* Operations: 0=get,1=set,2=clear. Drag payloads are separate from clipboard. */
typedef struct {int kind,cut;char data[CLIP_BYTES];} Clipboard;
int clipboard_access(int,Clipboard*);
void clipboard_clear(void);
