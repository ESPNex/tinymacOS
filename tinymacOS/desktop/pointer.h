/* Included by main.c: cursor assets are read through the real filesystem in
 * live, installed and Recovery. No alternate built-in theme except safe arrow
 * fallback for absent/corrupt resources. No filesystem reads while moving. */
#include "cursor_format.h"
#include "window_interaction.h"
static CursorImage pointer_images[CUR_COUNT];
static u8 pointer_ok[CUR_COUNT],pointer_bytes[1024*1024];
static int pointer_kind=-1,pointer_index=-1,pointer_owner=-1;
static u64 pointer_epoch;
static void pointer_load(void)
{
 int good=0;
 for(int i=0;i<CUR_COUNT;i++){
  /* S_ART resolves these real sealed filesystem files through a fixed kernel
   * allowlist, just like the font. S_OPEN stays denied before authentication. */
  long size=sc(S_ART,NWALL+1+i,0,0,0);if(size<=0||size>(long)sizeof pointer_bytes)continue;
  int ok=1;
  for(int off=0;off<size;off+=MAXFILE){int n=MIN(MAXFILE,size-off);if(sc(S_ART,NWALL+1+i,off,pointer_bytes+off,n)!=n){ok=0;break;}}
  pointer_ok[i]=ok&&cursor_decode(pointer_bytes,size,pointer_images+i);good+=pointer_ok[i];
 }
 char note[80]="cursor assets loaded ";char num[16];number(num,good);append(note,num,sizeof note);trace(note);
}
static int edge_cursor(int e)
{
 if((e&(EDGE_LEFT|EDGE_RIGHT))&&(e&(EDGE_TOP|EDGE_BOTTOM)))return e==(EDGE_LEFT|EDGE_TOP)||e==(EDGE_RIGHT|EDGE_BOTTOM)?CUR_DIAGONAL1:CUR_DIAGONAL2;
 return e&(EDGE_LEFT|EDGE_RIGHT)?CUR_HORIZONTAL:CUR_VERTICAL;
}
static int cursor_commands(View*v,int x,int y)
{
 int k=CUR_NORMAL;
 for(int j=0;j<v->n;j++){
  Draw*d=v->cmd+j;
  if(d->w<=0||d->h<=0)continue;
  if(d->type==C_CURSOR&&d->style<CUR_COUNT&&inside(x,y,(Rect){d->x,d->y,d->w,d->h}))k=d->style;
  else if(d->type==C_BUTTON&&inside(x,y,(Rect){d->x,d->y,d->w,d->h}))k=CUR_SELECT;
 }
 return k;
}
/* Pointer-context enter/leave/update events are local to WindowServer. Clients
 * publish C_CURSOR regions with their Draw transaction; no mouse-motion IPC
 * floods, stale async replies or redraws just to select a pointer shape. */
enum {POINTER_CONTEXT_ENTER=1,POINTER_CONTEXT_LEAVE,POINTER_CONTEXT_UPDATE};
static unsigned pointer_context_events[4];
static int pointer_context(int*owner)
{
 *owner=-1;
 if(gate)return login_cursor(mx,my);
 if(mc)return CUR_NORMAL;
 if(drag>=0){*owner=views[drag].s.pid;return mode==1?CUR_MOVE:edge_cursor(mode>>1);}
 if(filedrag.started)return CUR_MOVE;
 if(sys.recovery?recovery_overlay_open():shell_overlay_open())return sys.recovery?CUR_NORMAL:shell_cursor(mx,my);
 for(int j=nz;j--;){View*v=views+order[j];if(v->dead||v->mini||!inside(mx,my,v->box))continue;
  if(standalone_dialog(v)&&!inside(mx,my,dialog_bounds(v)))continue;
  *owner=v->s.pid;int x=mx-v->box.x,y=my-v->box.y;
  int e=!v->max&&!standalone_dialog(v)?window_edge(x,y,v->box.w,v->box.h):0;
  if(e)return edge_cursor(e);
  if(y<TITLE)return CUR_NORMAL;
  int k=cursor_commands(v,x-1,y-TITLE);
  if(v->s.app==TERMINAL&&k==CUR_NORMAL)return CUR_TEXT;
  return k;
 }
 return CUR_NORMAL;
}
static void pointer_step(void)
{
 int owner,k=pointer_context(&owner);
 if(k<0||k>=CUR_COUNT||!pointer_ok[k])k=CUR_NORMAL;
 if(k!=pointer_kind||owner!=pointer_owner){
  if(pointer_kind>=0)pointer_context_events[POINTER_CONTEXT_LEAVE]++;
  pointer_context_events[POINTER_CONTEXT_ENTER]++;pointer_kind=k;pointer_owner=owner;pointer_epoch=sys.ticks;pointer_index=-1;cursorDirty=1;
 }
 if(!pointer_ok[k])return;
 unsigned frame=sys.reduce_motion?0:cursor_frame(pointer_images+k,sys.ticks-pointer_epoch);
 if((int)frame!=pointer_index){pointer_index=frame;pointer_context_events[POINTER_CONTEXT_UPDATE]++;cursorDirty=1;}
}
