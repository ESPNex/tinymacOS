#define main old_resize_main
#include "native_resize_host.c"
#undef main
int main(void){
 media=malloc(TOTAL*512);assert(media);fixture();load_driver();persistent=1;
 assert(fs_create_i("/Users",1)>0&&fs_create_i(HOME,1)>0&&fs_create_i(DOCS,1)>0);int id=fs_create_i(DOCS "/proof",0);assert(id>0);
 const char*name="Elena Ròssi";u8 account[512]={0};memcpy(account,"TMCUSER2",8);u32 v=1,r=120000;memcpy(account+8,&v,4);memcpy(account+12,&r,4);strset((char*)account+16,name,32);sha_bytes(account,480,account+480);assert(!account_sector_i(0,account,1));
 for(int encrypted=0;encrypted<2;encrypted++){
  if(encrypted){enc_state=1;enc_locked=0;memset(enc_key,0x3a,32);nodes[id].reserved=1;}
  u8 plain[512],cipher[512],got[512];memset(plain,0x63,512);assert(bytes(id,0,plain,512,1)==512);memcpy(cipher,at(DATA+id*16),512);
  if(encrypted)assert(memcmp(cipher,plain,512));
  assert(!legacy_home_bind(name));assert(resolve(DOCS "/proof")==id&&resolve("/Users/Elena Ròssi/Documents/proof")==id);
  assert(!memcmp(cipher,at(DATA+id*16),512));assert(bytes(id,0,got,512,0)==512&&!memcmp(got,plain,512));
  int before=writes;assert(!legacy_home_bind(name)&&writes==before);assert(legacy_home_bind("../bad")<0&&writes==before);
  int home=resolve(HOME);strset(nodes[home].name,"User",48);assert(!syncnode(home));
 }
 assert(fs_create_i("/Users/Elena Ròssi",1)>0);int before=writes;assert(legacy_home_bind(name)==-2&&writes==before&&resolve(HOME)>0);
 free(media);puts("PASS actual legacy home migration: UTF8 inode rename, verified-account old path alias, payload sectors byte-identical in plaintext and legacy cipher, no rewrite on repeat, invalid/colliding names refuse without writes");return 0;
}
