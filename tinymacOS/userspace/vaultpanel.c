#include "ui.h"
#include "vault.h"
#include "vaultview.h"
/* Embedded Privacy & Security section. No window or event loop of its own.
 * No password/recovery secret is logged, saved to preferences, or clipboard. */
static VaultRequest q;
static VaultInfo vi;
static DiskInfo disks;
static int page,op,focus,fields,confirmed;
static char input[3][65],notice[128];
static void wipe_inputs(void){memset(input,0,sizeof input);memset(q.password,0,65);memset(q.replacement,0,65);}
static int fvcall(int a){int r=sc(S_FILEVAULT,a,&q,&vi,0);memset(q.password,0,65);memset(q.replacement,0,65);return r;}
static const char *error(int r){return r==FV_AUTH?"Password errata o conferma diversa.":r==FV_SPACE?"Spazio riservato insufficiente: nessun dato modificato.":r==FV_ENTROPY?"Sorgente casuale sicura non disponibile: operazione rifiutata.":r==FV_CORRUPT?"Integrità non valida. Non inizializzare il disco.":r==FV_STATE?"Volume non idoneo. Disattiva prima la vecchia cifratura dei file.":"Errore I/O. Riapri FileVault per riprendere; non inizializzare.";}
static int vx,vy,vw,vh;
static void vt(int x,int y,const char*s,int f,u32 c){Text(vx+x,vy+y,s,f,c);}
static void vb(int x,int y,int w,int h,int r,u32 c){Box(vx+x,vy+y,w,h,r,c);}
static void vbutton(int x,int y,int w,const char*s,int primary){button(vx+x,vy+y,w,s,primary);}
void vault_view_draw(int x,int y,int w,int h)
{
 vx=x;vy=y;vw=w;vh=h;
 if(page==0)fvcall(0);
 vb(0,0,vw,vh,0,CARD);vt(28,22,"FileVault",F_TITLE,TX);
 if(page==0){
  vbutton(28,82,vw-56,disks.count?q.disk>=0&&q.disk<disks.count?disks.disk[q.disk].model:"Disco di sistema":"Nessun disco idoneo",0);
  vt(28,135,"STATO",F_SMALL,SUB);vt(150,131,vi.error?"Volume non disponibile":!vi.enabled?"Disattivo":vi.locked?"Bloccato":vi.state==FV_ENCRYPTING?"Cifratura in corso":vi.state==FV_DECRYPTING?"Decifratura in corso":"Attivo",F_BODY,TX);
  vt(28,174,"FileVault protegge i dati su questo disco.",F_SMALL,SUB);
  if(vi.format==4&&vi.locked)vbutton(28,247,280,"Riprendi conversione…",1);
  else if(vi.state==FV_ENCRYPTING||vi.state==FV_DECRYPTING){vb(28,237,vw-56,12,6,SOFT);vb(28,237,(vw-56)*vi.progress/100,12,6,ACC);char z[32]="";addnum(z,vi.progress,32);append(z,"%",32);vt(28,265,z,F_BODY,TX);vt(28,306,"Non rimuovere il disco durante la conversione.",F_SMALL,SUB);}
  else if(vi.error){vbutton(28,247,210,"Riprova sblocco…",0);if(vi.format!=3)vbutton(255,247,vw-283,"Annulla attivazione…",0);}
  else if(!vi.enabled){if(vi.format==2&&!si.recovery)vt(28,247,"Conversione disponibile da Recovery.",F_SMALL,SUB);else if(vi.available)vbutton(28,247,210,"Attiva FileVault…",1);}
  else if(vi.locked)vbutton(28,247,250,"Sblocca volume…",1);
  else{if(vi.format!=3||si.recovery)vbutton(28,247,210,"Disattiva FileVault…",0);else vt(28,250,"Conversione da Recovery",F_SMALL,SUB);vbutton(255,247,230,"Cambia password…",0);vbutton(28,295,285,"Nuova chiave di recupero…",0);if(si.recovery)vbutton(330,295,180,"Blocca volume",0);}
  if(notice[0])vt(28,353,notice,F_SMALL,SUB);
 }else if(page==1){
  vt(28,90,op==1?"Attivazione FileVault":op==2?"Disattiva la cifratura":op==4?"Sblocca il volume":op==5?"Cambia password del volume":op==8?"Annulla attivazione non iniziata":"Rigenera la chiave di recupero",F_CAPTION,TX);
  if(vi.format>=2&&(op==1||op==2))vt(28,337,"Prepara una copia esterna. Ripresa possibile, annullamento no.",F_MICRO,SUB);
  vt(28,123,op==1&&!si.recovery?"Inserisci la password del tuo account.":op==8?"Inserisci la password del tuo account.":"Puoi usare anche la chiave di recupero.",F_SMALL,SUB);
  for(int i=0;i<fields;i++){const char *label=i==0?(op==4?"Password o chiave di recupero":op==1?"Nuova password del volume":"Password attuale"):i==1&&op==5?"Nuova password":"Conferma password";vt(28,159+i*57,label,F_SMALL,SUB);vb(28,179+i*57,vw-56,32,6,i==focus?SEL:PANEL);InputText(vx+39,vy+187+i*57,vw-78,input[i],strlen(input[i]),i==focus,1,TX);}
  if(notice[0])vt(28,354,notice,F_SMALL,SUB);vbutton(28,vh-58,120,"Annulla",0);vbutton(vw-180,vh-58,150,"Continua",1);
 }else{
  vt(28,90,"Salva la chiave di recupero",F_CAPTION,TX);vt(28,125,"Conservala in un luogo sicuro, fuori da questo disco.",F_BODY,TX);
  char half[33];memcpy(half,q.recovery,32);half[32]=0;vt(28,171,half,F_MONO,TX);memcpy(half,q.recovery+32,32);vt(28,198,half,F_MONO,TX);memset(half,0,sizeof half);
  vt(28,247,"Conferma gli ultimi 8 caratteri della chiave:",F_SMALL,SUB);vb(28,277,vw-56,35,6,SEL);InputText(vx+40,vy+285,vw-80,input[0],strlen(input[0]),1,0,TX);if(notice[0])vt(28,335,notice,F_SMALL,SUB);vbutton(vw-220,vh-58,190,"Ho salvato la chiave",1);
 }
}
static void begin_op(int a){op=a;page=1;focus=0;fields=a==1?2:a==5?3:1;wipe_inputs();notice[0]=0;}
static void submit(void)
{
 if(page==2){if(!eq(input[0],q.recovery+56)){strset(notice,"Gli 8 caratteri non coincidono.",sizeof notice);return;}if(op==1&&vi.format==4){strset(q.password,input[0],65);if(fvcall(9)<0){strset(notice,"Conferma non accettata.",sizeof notice);return;}}memset(q.recovery,0,65);wipe_inputs();confirmed=1;page=0;notice[0]=0;return;}
 if(page!=1)return;
 if(op==5&&!eq(input[1],input[2])){strset(notice,"Le due nuove password non coincidono.",sizeof notice);return;}
 strset(q.password,input[0],65);if(fields>1)strset(q.replacement,input[1],65);
 int r=fvcall(op);wipe_inputs();
 if(r<0){strset(notice,error(r),sizeof notice);return;}
 notice[0]=0;if(op==1||op==6){page=2;confirmed=0;}else page=0;
}
void vault_view_init(void)
{
 memset(&q,0,sizeof q);memset(&vi,0,sizeof vi);memset(notice,0,sizeof notice);page=0;confirmed=1;wipe_inputs();
 sc(S_DISK,0,&disks,0,0);q.disk=si.recovery?0:disks.current;fvcall(0);
}
int vault_view_can_leave(void){return page!=2;}
void vault_view_leave(void){if(page!=2){wipe_inputs();page=0;notice[0]=0;}}
int vault_view_can_close(void){return page!=2&&vi.state!=FV_ENCRYPTING&&vi.state!=FV_DECRYPTING;}
void vault_view_event(const Event *e)
{
 if(e->type==EV_KEY){if(e->a==27&&page==1){wipe_inputs();page=0;}else if(e->a=='\n')submit();else if(page==1){if(e->a=='\t')focus=(focus+1)%fields;else text_append_key(input[focus],65,e->a);}else if(page==2)text_append_key(input[0],9,e->a);}
 if(e->type==EV_CLICK){int x=e->a-vx,y=e->b-vy;if(x<0||y<0||x>=vw||y>=vh)return;
  if(page==2){if(y>vh-65&&x>vw-230)submit();}
  else if(page==1){for(int i=0;i<fields;i++)if(y>=179+i*57&&y<211+i*57)focus=i;if(y>vh-65){if(x<180){page=0;wipe_inputs();}else if(x>vw-190)submit();}}
  else if(y>=82&&y<116&&si.recovery&&disks.count){q.disk=(q.disk+1)%disks.count;fvcall(0);}
  else if(vi.format==4&&vi.locked&&y>=247&&y<280)begin_op(4);
  else if(vi.state!=FV_ENCRYPTING&&vi.state!=FV_DECRYPTING){if(y>=247&&y<280){if(vi.error){if(x<245||vi.format!=3)begin_op(x<245?4:8);}else if(!vi.enabled){if(vi.available)begin_op(1);}else if(vi.locked)begin_op(4);else if(x>=245||vi.format!=3||si.recovery)begin_op(x<245?2:5);}else if(y>=295&&y<329&&vi.enabled&&!vi.locked){if(x<320)begin_op(6);else if(si.recovery)fvcall(7);}}
 }
}
int vault_view_tick(void)
{
 if(page==0&&confirmed&&!vi.locked&&(vi.state==FV_ENCRYPTING||vi.state==FV_DECRYPTING)){int r=fvcall(3);if(r<0)strset(notice,error(r),sizeof notice);return 1;}return 0;
}
