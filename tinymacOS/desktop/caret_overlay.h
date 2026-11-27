/* Included by main.c after scanrect(). Caret is a final compositor overlay,
 * never part of a window's cached glass/content. Only its tiny damage rectangle
 * (and an overlapping pointer) is presented on a blink, not a full composition. */
static u32 caret_under[8*32];
static Rect caret_box;
static u32 caret_ink;
static int caret_valid,caret_source_pid;
static Rect caret_source_box;
static u32 caret_source_ink;
static int caret_pick(Rect *box,u32 *ink)
{
    int i=topwin();
    if(gate||mc||mcdim||drag>=0||filedrag.started||(sys.recovery?recovery_overlay_open():shell_overlay_open())||i<0){caret_source_pid=0;return 0;}
    View *v=views+i;
    if(v->mini||v->dead||v->motion.kind){caret_source_pid=0;return 0;}
    const Draw *q=0;
    for(int j=0;j<v->n;j++)if(v->cmd[j].type==C_CARET)q=v->cmd+j;
    if(!q||!caret_dimensions(q->w,q->h)||q->x<0||q->y<0||
       q->x+q->w>v->box.w-2||q->y+q->h>v->box.h-TITLE-FOOT){caret_source_pid=0;return 0;}
    *box=(Rect){v->box.x+1+q->x,v->box.y+TITLE+q->y,q->w,q->h};
    if(box->x<0||box->y<0||box->x+box->w>W||box->y+box->h>H){caret_source_pid=0;return 0;}
    *ink=color(q->ink);
    if(caret_source_pid!=v->s.pid||memcmp(box,&caret_source_box,sizeof *box)||*ink!=caret_source_ink){
        caret_activity=sys.ticks;caret_source_pid=v->s.pid;caret_source_box=*box;caret_source_ink=*ink;
    }
    return caret_phase(sys.ticks,caret_activity,sys.reduce_motion);
}
static void caret_erase(void)
{
    if(!caret_valid)return;
    for(int y=0;y<caret_box.h;y++)memcpy(canvas+(caret_box.y+y)*W+caret_box.x,caret_under+y*caret_box.w,caret_box.w*4);
    caret_valid=0;
}
static void caret_paint(void)
{
    Rect q;u32 ink;
    if(!caret_pick(&q,&ink))return;
    caret_box=q;caret_ink=ink;
    for(int y=0;y<q.h;y++){
        u32 *row=canvas+(q.y+y)*W+q.x;
        memcpy(caret_under+y*q.w,row,q.w*4);
        for(int x=0;x<q.w;x++)row[x]=ink;
    }
    caret_valid=1;
}
static int caret_pending(void)
{
    Rect q;u32 ink;int visible=caret_pick(&q,&ink);
    return visible!=caret_valid||(visible&&(memcmp(&q,&caret_box,sizeof q)||ink!=caret_ink));
}
static void caret_present_partial(void)
{
    Rect old=caret_box,mouse={curx,cury,curw,curh};int had=caret_valid;
    /* Pointer can overlap the caret: restore in reverse paint order. */
    undocursor();caret_erase();caret_paint();drawcursor();
    if(had)scanrect(old.x,old.y,old.w,old.h);
    if(caret_valid)scanrect(caret_box.x,caret_box.y,caret_box.w,caret_box.h);
    if(mouse.w>0&&mouse.h>0)scanrect(mouse.x,mouse.y,mouse.w,mouse.h);
    if(curw>0&&curh>0)scanrect(curx,cury,curw,curh);
    cursorDirty=0;
}
