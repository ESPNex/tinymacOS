#include "ui.h"
#include "vault.h"
/* Shared native management window, launched from System Settings or Recovery.
 * No password/recovery secret is logged, saved to preferences, or clipboard. */
static VaultRequest q;
static VaultInfo vi;
static DiskInfo disks;
static int page,op,focus,fields,confirmed;
static char input[3][65],notice[128];
static void wipe_inputs(void){memset(input,0,sizeof input);memset(q.password,0,65);memset(q.replacement,0,65);}
static int fvcall(int a){int r=sc(S_FILEVAULT,a,&q,&vi,0);memset(q.password,0,65);memset(q.replacement,0,65);return r;}
static const char *error(int r){return r==FV_AUTH?"Password errata o conferma diversa.":r==FV_SPACE?"Spazio riservato insufficiente: nessun dato modificato.":r==FV_ENTROPY?"Sorgente casuale sicura non disponibile: operazione rifiutata.":r==FV_CORRUPT?"Integrità non valida. Non inizializzare il disco.":r==FV_STATE?"Volume non idoneo. Disattiva prima la vecchia cifratura dei file.":"Errore I/O. Riapri FileVault per riprendere; non inizializzare.";}
static void draw(void)
{
 if(page==0)fvcall(0);
 begin();Box(0,0,aw,ah,0,BG);Text(28,22,"FileVault nativo",F_TITLE,TX);
 Text(28,51,"Volume dati cifrato · non compatibile con il formato Apple",F_SMALL,SUB);
 if(page==0){
  button(28,82,aw-56,disks.count?q.disk>=0&&q.disk<disks.count?disks.disk[q.disk].model:"Disco di sistema":"Nessun disco idoneo",0);
  Text(28,135,"STATO",F_SMALL,SUB);Text(150,131,vi.error?"Volume non disponibile":!vi.enabled?"Disattivo":vi.locked?"Bloccato":vi.state==FV_ENCRYPTING?"Cifratura in corso":vi.state==FV_DECRYPTING?"Decifratura in corso":"Attivo",F_BODY,TX);
  Text(28,174,"Protegge documenti, nomi, metadati, account, snapshot e copie locali.",F_SMALL,TX);
  Text(28,199,"EFI e sistema Recovery restano avviabili. Lo sblocco precede il desktop.",F_SMALL,SUB);
  if(vi.state==FV_ENCRYPTING||vi.state==FV_DECRYPTING){Box(28,237,aw-56,12,6,SOFT);Box(28,237,(aw-56)*vi.progress/100,12,6,ACC);char z[32]="";addnum(z,vi.progress,32);append(z,"% · puoi riprendere dopo un riavvio",32);Text(28,265,z,F_BODY,TX);Text(28,306,"Non rimuovere il disco. La conversione è riprendibile con password o chiave.",F_SMALL,SUB);}
  else if(vi.error){button(28,247,210,"Riprova sblocco…",0);button(255,247,365,"Annulla attivazione interrotta…",0);}
  else if(!vi.enabled)button(28,247,210,"Attiva FileVault…",1);
  else if(vi.locked)button(28,247,250,"Sblocca volume…",1);
  else{button(28,247,210,"Disattiva FileVault…",0);button(255,247,230,"Cambia password…",0);button(28,295,285,"Nuova chiave di recupero…",0);if(si.recovery)button(330,295,180,"Blocca volume",0);}
  if(notice[0])Text(28,353,notice,F_SMALL,SUB);
  Text(28,ah-35,"Cifratura a riposo: non protegge RAM, firmware o un sistema già compromesso.",F_MICRO,SUB);
 }else if(page==1){
  Text(28,90,op==1?"Attivazione FileVault":op==2?"Disattiva la cifratura":op==4?"Sblocca il volume":op==5?"Cambia password del volume":op==8?"Annulla attivazione non iniziata":"Rigenera la chiave di recupero",F_CAPTION,TX);
  Text(28,123,op==1&&!si.recovery?"Usa la password del tuo account; confermala nel secondo campo.":"La chiave di recupero può sostituire la password attuale.",F_SMALL,SUB);
  for(int i=0;i<fields;i++){const char *label=i==0?(op==4?"Password o chiave di recupero":"Password attuale"):i==1&&op==5?"Nuova password":"Conferma password";Text(28,159+i*57,label,F_SMALL,SUB);Box(28,179+i*57,aw-56,32,6,i==focus?SEL:PANEL);char dots[65];int n=strlen(input[i]);memset(dots,'*',n);dots[n]=0;Text(39,187+i*57,dots,F_MONO,TX);}
  if(notice[0])Text(28,354,notice,F_SMALL,SUB);button(28,ah-58,120,"Annulla",0);button(aw-180,ah-58,150,"Continua",1);
 }else{
  Text(28,90,"Salva la chiave di recupero",F_CAPTION,TX);Text(28,125,"Viene mostrata solo ora. Conservala fuori da questo disco.",F_BODY,TX);
  char half[33];memcpy(half,q.recovery,32);half[32]=0;Text(28,171,half,F_MONO,TX);memcpy(half,q.recovery+32,32);Text(28,198,half,F_MONO,TX);memset(half,0,sizeof half);
  Text(28,247,"Per confermare, digita gli ultimi 8 caratteri della chiave:",F_SMALL,SUB);Box(28,277,aw-56,35,6,SEL);Text(40,285,input[0],F_MONO,TX);if(notice[0])Text(28,335,notice,F_SMALL,SUB);button(aw-220,ah-58,190,"Ho salvato la chiave",1);
 }
 finish();
}
static void begin_op(int a){op=a;page=1;focus=0;fields=a==1?2:a==5?3:1;wipe_inputs();notice[0]=0;}
static void submit(void)
{
 if(page==2){if(!eq(input[0],q.recovery+56)){strset(notice,"Gli 8 caratteri non coincidono.",sizeof notice);return;}memset(q.recovery,0,65);wipe_inputs();confirmed=1;page=0;notice[0]=0;return;}
 if(page!=1)return;
 if(op==5&&!eq(input[1],input[2])){strset(notice,"Le due nuove password non coincidono.",sizeof notice);return;}
 strset(q.password,input[0],65);if(fields>1)strset(q.replacement,input[1],65);
 strset(notice,"Verifica in corso…",sizeof notice);draw();int r=fvcall(op);wipe_inputs();
 if(r<0){strset(notice,error(r),sizeof notice);return;}
 notice[0]=0;if(op==1||op==6){page=2;confirmed=0;}else page=0;
}
void vault_panel(void)
{
 win(710,430,"FileVault");memset(&q,0,sizeof q);memset(&vi,0,sizeof vi);memset(notice,0,sizeof notice);page=0;confirmed=1;
 sc(S_DISK,0,&disks,0,0);q.disk=si.recovery?0:disks.current;fvcall(0);draw();
 for(;;){Event e;int dirty=0;while(sc(S_EVENT,&e,0,0,0)>0){dirty=1;if(baseevent(&e))continue;
   if(e.type==EV_CLOSE){if(page!=2&&vi.state!=FV_ENCRYPTING&&vi.state!=FV_DECRYPTING){wipe_inputs();memset(q.recovery,0,65);quit();}continue;}
   if(e.type==EV_KEY){if(e.a==27&&page==1){wipe_inputs();page=0;}else if(e.a=='\n')submit();else if(page==1){if(e.a=='\t')focus=(focus+1)%fields;else text_append_key(input[focus],65,e.a);}else if(page==2)text_append_key(input[0],9,e.a);}
   if(e.type==EV_CLICK){int x=e.a,y=e.b;
    if(page==2){if(y>ah-65&&x>aw-230)submit();}
    else if(page==1){for(int i=0;i<fields;i++)if(y>=179+i*57&&y<211+i*57)focus=i;if(y>ah-65){if(x<180){page=0;wipe_inputs();}else if(x>aw-190)submit();}}
    else if(y>=82&&y<116&&si.recovery&&disks.count){q.disk=(q.disk+1)%disks.count;fvcall(0);}
    else if(vi.state!=FV_ENCRYPTING&&vi.state!=FV_DECRYPTING){if(y>=247&&y<280){if(vi.error)begin_op(x<245?4:8);else if(!vi.enabled)begin_op(1);else if(vi.locked)begin_op(4);else begin_op(x<245?2:5);}else if(y>=295&&y<329&&vi.enabled&&!vi.locked){if(x<320)begin_op(6);else if(si.recovery){fvcall(7);}}}
   }
  }
  if(page==0&&confirmed&&!vi.locked&&(vi.state==FV_ENCRYPTING||vi.state==FV_DECRYPTING)){int r=fvcall(3);if(r<0)strset(notice,error(r),sizeof notice);dirty=1;}
  if(dirty)draw();slp(1);
 }
}
