#include "hidconfig.h"
int hid_interfaces(const u8 *d,unsigned n,HidInterface *out,unsigned capacity)
{
    if(!d||!out||!capacity||n<9||n>1024||d[0]<9||d[1]!=2||
       (unsigned)(d[2]|d[3]<<8)!=n)return -1;
    unsigned found=0;int candidate=0,selected=0;
    HidInterface f={0};
    for(unsigned at=0;at<n;) {
        if(n-at<2)return -1;
        const u8 *p=d+at;unsigned z=p[0];
        if(z<2||z>n-at)return -1;
        if(p[1]==4) {
            if(z<9)return -1;
            candidate=!p[3]&&p[5]==3;selected=0;
            f=(HidInterface){.iface=p[2],.proto=p[6]==1&&(p[7]==1||p[7]==2)?p[7]:3};
        } else if(p[1]==0x21&&candidate) {
            if(z<6||p[5]>(z-6)/3)return -1;
            for(unsigned k=0;k<p[5];k++)if(p[6+3*k]==0x22)f.report_length=p[7+3*k]|p[8+3*k]<<8;
        } else if(p[1]==5&&candidate&&!selected) {
            if(z<7)return -1;
            if((p[2]&0x80)&&(p[3]&3)==3) {
                f.ep=2*(p[2]&15)+1;f.packet=(p[4]|p[5]<<8)&2047;f.interval=p[6];
                if(f.ep>=3&&f.ep<=31&&f.packet>=(f.proto==1?8u:3u)&&f.packet<=1024&&f.interval) {
                    for(unsigned k=0;k<found;k++)if(out[k].ep==f.ep||out[k].iface==f.iface)return -1;
                    if(found<capacity)out[found++]=f;
                    selected=1;
                }
            }
        }
        at+=z;
    }
    return (int)found;
}
