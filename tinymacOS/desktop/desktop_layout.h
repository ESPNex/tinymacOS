#pragma once
/* Per-user desktop positions. Data only: never filesystem paths or executable
 * commands from the preference file. Every draw and hit test uses this model. */
#define DESKTOP_POSITIONS (NSLOTS+1)
typedef struct {u64 id;int used,x,y;char name[48];} DesktopPosition;
typedef struct {char magic[8];u32 version,count;DesktopPosition items[DESKTOP_POSITIONS];} DesktopLayout;
static int desktop_layout_valid(const DesktopLayout *p)
{
    if(memcmp(p->magic,"TMCDESK4",8)||p->version!=1||p->count!=DESKTOP_POSITIONS)return 0;
    for(int i=0;i<DESKTOP_POSITIONS;i++){
        const DesktopPosition *q=p->items+i;
        if(q->used!=0&&q->used!=1)return 0;
        if(q->used&&(q->x<0||q->x>1920||q->y<34||q->y>1080||!q->name[0]||q->name[47]))return 0;
    }
    return 1;
}
static void desktop_layout_init(DesktopLayout *p)
{
    memset(p,0,sizeof *p);memcpy(p->magic,"TMCDESK4",8);p->version=1;p->count=DESKTOP_POSITIONS;
}
static int desktop_position_find(DesktopLayout *p,u64 id,const char *name,int create)
{
    int empty=-1,byname=-1;
    for(int i=0;i<DESKTOP_POSITIONS;i++){
        DesktopPosition *q=p->items+i;
        if(!q->used){if(empty<0)empty=i;continue;}
        if(id&&q->id==id)return i;
        if(eq(q->name,name))byname=i;
    }
    if(byname>=0)return byname;
    if(create&&empty>=0){DesktopPosition*q=p->items+empty;q->id=id;q->used=1;strset(q->name,name,48);return empty;}
    return -1;
}
static Rect desktop_position_box(DesktopPosition *p,Rect fallback,int width,int height)
{
    Rect q=fallback;if(p&&p->used){q.x=p->x;q.y=p->y;}
    q.x=clampi(q.x,0,MAX(0,width-q.w));q.y=clampi(q.y,34,MAX(34,height-q.h));return q;
}
