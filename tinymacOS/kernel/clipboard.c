#include "base.h"
#include "clipboard.h"
static Clipboard contents;
void clipboard_clear(void){memset(&contents,0,sizeof contents);}
int clipboard_access(int op,Clipboard*q){
 if(op==2){clipboard_clear();return 0;}
 if(!q)return -1;
 if(!op){*q=contents;return 0;}
 if(op!=1||(q->kind!=CLIP_TEXT&&q->kind!=CLIP_FILE)||(q->cut!=0&&q->cut!=1)||q->data[CLIP_BYTES-1]||(q->kind==CLIP_FILE&&(q->data[0]!='/'||strlen(q->data)>=144))||(q->kind==CLIP_TEXT&&q->cut))return -1;
 contents=*q;return 0;
}
