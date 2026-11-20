#include "hidtablet.h"
/* Bounded short-item HID parser for absolute tablets and relative USB mice. Unknown HID devices
 * do not silently become relative mice. Unrelated report IDs and bounded global Push/Pop are supported.
 * One complete pointer layout is selected; arrays as axes and pens are rejected. */
static int signed_value(u32 v, unsigned bits)
{
    if (bits == 32) return (int32_t)v;
    u32 sign = (u32)1 << (bits - 1);
    return (int)(v & (sign - 1)) - (int)(v & sign);
}
static int bind_field(TabletField *f, unsigned off, unsigned size, int lo, int hi)
{
    if (f->width || !size || size > 16 || lo >= hi) return 0;
    int pm = 1 << (size - (lo < 0));
    if (lo < -pm || hi >= pm) return 0;
    *f = (TabletField) {
        off, size, lo, hi
    };
    return 1;
}
/* Track Input bit offsets independently per report ID. Vendor/keyboard reports
 * must not shift the pointer's X/Y; Push/Pop restores all HID global state. */
static int parse_layout(const u8 *d,unsigned n,unsigned packet,TabletLayout *out,int allow_relative)
{
    if(!d||!out||!n||n>1024||!packet||packet>64)return 0;
    struct Global {unsigned page,size,count,id;int lo,hi;} g={0},stack[8];
    unsigned sp=0,depth=0,mouse[16]={0},offset[256]={0},usage[32],nu=0,first=0,last=0;
    unsigned ids[8]={0},nt=0;int range=0,mode[8];TabletLayout t[8];
    memset(t,0,sizeof t);for(unsigned i=0;i<8;i++)mode[i]=-1;
    for(unsigned at=0;at<n;) {
        unsigned h=d[at++],z=h&3,type=(h>>2)&3,tag=h>>4;if(z==3)z=4;
        if(h==0xfe||z>n-at)return 0;
        u32 v=0;for(unsigned k=0;k<z;k++)v|=(u32)d[at++]<<(8*k);
        if(type==1) {
            switch(tag){
            case 0:g.page=v;break;
            case 1:if(!z)return 0;g.lo=signed_value(v,z*8);break;
            case 2:if(!z||(g.lo>=0&&v>INT32_MAX))return 0;g.hi=g.lo<0?signed_value(v,z*8):(int)v;break;
            case 7:g.size=v;break;
            case 8:if(!v||v>255)return 0;g.id=v;if(!offset[v])offset[v]=8;break;
            case 9:g.count=v;break;
            case 10:if(sp==8)return 0;stack[sp++]=g;break;
            case 11:if(!sp)return 0;g=stack[--sp];break;
            }
        } else if(type==2) {
            unsigned full=z==4?v:(g.page<<16)|v;
            if(tag==0){if(nu==32)return 0;usage[nu++]=full;}
            else if(tag==1){first=full;range=1;}else if(tag==2)last=full;
        } else if(type==0) {
            if(tag==10){
                if(depth==16)return 0;
                unsigned active=depth?mouse[depth-1]:0;
                if(v==1)active=nu==1&&(usage[0]==0x10001||usage[0]==0x10002);
                mouse[depth++]=active;
            } else if(tag==12){if(!depth)return 0;depth--;}
            else if(tag==8){
                if(!g.size||g.size>32||!g.count||g.count>(8192-offset[g.id])/g.size)return 0;
                if(depth&&mouse[depth-1]&&!(v&1)&&(v&2)) {
                    unsigned index=0;while(index<nt&&ids[index]!=g.id)index++;
                    if(index==nt){if(nt==8)return 0;ids[nt++]=g.id;t[index].id=g.id;}
                    for(unsigned j=0;j<g.count;j++) {
                        unsigned u=j<nu?usage[j]:(range&&last>=first&&j<=last-first?first+j:0);
                        int rel=!!(v&4);TabletField *f=0;
                        if(u==0x10030||u==0x10031){
                            if((rel&&!allow_relative)||(mode[index]>=0&&mode[index]!=rel)||(rel&&g.lo>=0))return 0;
                            mode[index]=rel;t[index].relative=rel;f=u==0x10030?&t[index].x:&t[index].y;
                        }
                        if(u==0x10038&&rel)f=&t[index].wheel;
                        if((u>>16)==9&&!rel&&(u&65535)>=1&&(u&65535)<=3&&g.size==1)f=&t[index].button[(u&65535)-1];
                        if(f&&!bind_field(f,offset[g.id]+j*g.size,g.size,g.lo,g.hi))return 0;
                    }
                }
                offset[g.id]+=g.size*g.count;
            }
            nu=first=last=0;range=0;
        }
    }
    if(depth||sp)return 0;
    for(unsigned i=0;i<nt;i++)if(t[i].x.width&&t[i].y.width&&t[i].button[0].width){
        t[i].bytes=(offset[ids[i]]+7)/8;if(t[i].bytes>packet)continue;*out=t[i];return 1;
    }
    return 0;
}
int pointer_layout(const u8 *d, unsigned n, unsigned packet, TabletLayout *out)
{
    return parse_layout(d,n,packet,out,1);
}
int tablet_layout(const u8 *d, unsigned n, unsigned packet, TabletLayout *out)
{
    return parse_layout(d,n,packet,out,0);
}
static int extract(const u8 *p, TabletField f)
{
    u32 v = 0;
    for (unsigned j = 0; j < f.width; j++) {
        unsigned b = f.bit + j;
        v |= (u32)((p[b / 8] >> (b % 8)) & 1) << j;
    }
    return f.low < 0 ? signed_value(v, f.width) : (int)v;
}
static int axis(const u8 *p, TabletField f, unsigned extent)
{
    int val = extract(p, f);
    return pointer_pixel(val <= f.low ? 0 : (u64)(val - f.low), 0, (unsigned)(f.high - f.low), extent);
}
int pointer_read(const TabletLayout *t, const u8 *report, unsigned received, unsigned w, unsigned h, TabletState *s)
{
    if (!t || !t->bytes || !report || received < t->bytes || !w || !h || !s || (t->id && report[0] != t->id)) return 0;
    s->x = t->relative ? extract(report, t->x) : axis(report, t->x, w);
    s->y = t->relative ? extract(report, t->y) : axis(report, t->y, h);
    s->buttons = 0;
    for (unsigned i = 0; i < 3; i++) if (extract(report, t->button[i])) s->buttons |= 1 << i;
    s->wheel = extract(report, t->wheel);
    return 1;
}

int tablet_read(const TabletLayout *t, const u8 *report, unsigned received, unsigned w, unsigned h, TabletState *s)
{
    return t && !t->relative ? pointer_read(t,report,received,w,h,s) : 0;
}
