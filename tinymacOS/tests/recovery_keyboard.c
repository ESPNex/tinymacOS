/* Execute unmodified Recovery keyboard logic, with firmware/port imports only
 * replaced by the host runner. No real I/O instructions or synthetic PASS. */
#include TMC_REC_KBD_TEST_SOURCE
static int idle_requests,protocol_requests,led_requests,armed,deny_idle;
static EFI_STATUS ctl(KUsb *io,KRequest *q,int dir,UINT32 timeout,VOID *dst,UINTN n,UINT32 *err){
 (void)io;*err=0;
 if(q->request==11){assert(q->rt==0x21&&q->value==0&&q->index==3&&!q->length&&dir==2&&timeout==250&&!dst&&!n);protocol_requests++;return 0;}
 if(q->request==10){assert(q->rt==0x21&&q->value==(8u<<8)&&q->index==3&&!q->length&&dir==2&&!dst&&!n);idle_requests++;return deny_idle?1:0;}
 assert(q->request==9&&q->value==0x200&&dir==1&&n==1);led_requests++;return 0;
}
static EFI_STATUS iface(KUsb *io,KInterface *f){(void)io;*f=(KInterface){9,4,3,0,1,3,1,1,0};return 0;}
static EFI_STATUS ep(KUsb *io,UINT8 i,KEndpoint *e){(void)io;assert(!i);*e=(KEndpoint){7,5,0x81,3,8,10};return 0;}
static EFI_STATUS start(KUsb *io,UINT8 addr,BOOLEAN add,UINTN interval,UINTN len,KNotify cb,VOID *ctx){
 (void)io;assert(addr==0x81&&add&&interval==8&&len==8&&cb==keyboard_report&&ctx==previous[0]);armed++;return 0;
}
static KUsb io={.control=ctl,.async=start,.interface_desc=iface,.endpoint_desc=ep};
static EFI_STATUS locate(int by,EFI_GUID *g,void *key,UINTN *n,EFI_HANDLE **h){
 (void)by;(void)key;if(!g->a)return 1;*n=1;*h=malloc(sizeof **h);(*h)[0]=(void*)1;return 0;
}
static EFI_STATUS opened(EFI_HANDLE h,EFI_GUID *g,KTestOpenInfo **p,UINTN *n){(void)h;(void)g;*p=0;*n=0;return 0;}
static EFI_STATUS pool(void *p){free(p);return 0;}
static EFI_STATUS handle(EFI_HANDLE h,EFI_GUID *g,void **p){(void)h;(void)g;*p=&io;return 0;}
static EFI_STATUS disconnect(EFI_HANDLE h,EFI_HANDLE a,EFI_HANDLE *c){(void)h;(void)a;(void)c;assert(0);return 1;}
static EFI_TPL raise(EFI_TPL t){assert(t==TPL_NOTIFY);return 4;}
static void restore(EFI_TPL t){assert(t==4);}
static KTestServices bs={locate,opened,pool,handle,disconnect,raise,restore};
static void report(u8 mods,u8 key){u8 r[8]={mods,0,key};assert(!keyboard_report(r,8,previous[0],0));}
int main(void){
 assert(rec_keyboard_start(&bs)==1&&idle_requests==1&&protocol_requests==1&&armed==1);
 deny_idle=1;assert(rec_keyboard_start(&bs)==1&&idle_requests==2&&armed==2);
 puts("PASS Recovery Boot keyboard: SET_PROTOCOL Boot + SET_IDLE 32 ms, optional SET_IDLE STALL does not reject a keyboard");
 int c=-1;assert(!rec_keyboard_next(&bs,&c));report(0,22);
 assert(rec_keyboard_next(&bs,&c)&&c=='s');
 for(int i=0;i<20;i++){report(0,22);assert(!rec_keyboard_next(&bs,&c));}
 report(0,0);for(int i=0;i<80;i++)assert(!rec_keyboard_next(&bs,&c));
 report(2,56);assert(rec_keyboard_next(&bs,&c)&&c=='_');report(0,0);
 report(0x40,8);assert(rec_keyboard_next(&bs,&c)&&c==0x20ac);report(0,0);
 report(0,40);assert(rec_keyboard_next(&bs,&c)&&c==10);report(0,0);
 for(int i=0;i<80;i++)assert(!rec_keyboard_next(&bs,&c));
 report(0,4);assert(rec_keyboard_next(&bs,&c)&&c=='a');ticks=repeat_at;
 assert(rec_keyboard_next(&bs,&c)&&c=='a');report(0,0);
 for(int i=0;i<80;i++)assert(!rec_keyboard_next(&bs,&c));
 assert(led_requests==1);
 puts("PASS Recovery keyboard refreshed identical reports: no duplicate press, Italian/AltGr intact, release cancels repeat, local held-key repeat preserved");
}
