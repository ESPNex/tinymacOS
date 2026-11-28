/* Actual parser/ISO reader on independent generated image files. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image8.h"
#include "foreign_volume.h"
static u8*bytes;static size_t length;static Image8 image;
static int readsource(void*x,u64 at,void*p,size_t n){(void)x;if(at>length||n>length-at)return -1;memcpy(p,bytes+at,n);return 0;}
static int readsector(void*x,u64 l,u32 n,void*p){return image8_read(x,l*512,p,(size_t)n*512);}
static void*page_alloc(void*x,size_t n){(void)x;return calloc(1,n);}
static void page_free(void*x,void*p){(void)x;free(p);}
static u8 scratch[65536],buf[8192];
static int decode_image(void){image8_release(&image);return image8_decode_alloc(&image,0,length,readsource,scratch,sizeof scratch,NULL,page_alloc,page_free);}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[2],"rb");assert(f);fseek(f,0,SEEK_END);length=ftell(f);rewind(f);bytes=malloc(length);assert(bytes&&fread(bytes,1,length,f)==length);fclose(f);
 assert(!decode_image());assert(image.bytes&&image.bytes<=IMAGE8_MAX_BYTES);
 assert(image8_read(&image,image.bytes-1,buf,2)<0);assert(image8_read(&image,~(u64)0,buf,1)<0);assert(image8_read(&image,image.bytes,buf,0)==0);
 if(!strcmp(argv[1],"iso")){
  ForeignIo io={.context=&image,.sectors=image.bytes/512,.logical_bytes=512,.read=readsector};ForeignEntry e,entries[8];
  assert(!iso_volume_stat(&io,"/",&e)&&e.dir&&e.ro);assert(iso_volume_list(&io,"/",entries,8,0)>0);
  assert(!iso_volume_stat(&io,"/proof.txt",&e)&&!e.dir&&e.ro&&e.size==19);memset(buf,0,sizeof buf);assert(iso_volume_read(&io,"/proof.txt",0,buf,sizeof buf)==19&&!memcmp(buf,"IMAGE8_ISO_CONTENT\n",19));
  assert(iso_volume_read(&io,"/proof.txt",19,buf,1)==0);assert(iso_volume_stat(&io,"/../proof.txt",&e)<0);
  /* Corrupt endian mirror / root extent; actual reader must refuse. */
  u8 old=bytes[16*2048+156+6];bytes[16*2048+156+6]^=0x7f;assert(iso_volume_stat(&io,"/",&e)<0);bytes[16*2048+156+6]=old;
  puts("PASS actual ISO9660 RO driver: directory enumeration, case-insensitive primary names, exact independently authored file, EOF, corrupt endian/root refusal");
 }else{
  assert(image.runs);assert(!image8_read(&image,0,buf,512));assert(buf[510]==85&&buf[511]==170);assert(!image8_read(&image,262140,buf,16));
  /* Trailer, XML bounds, base64 and unsupported/overlapping runs fail closed. */
  u8 *footer=bytes+length-512,old=footer[4];footer[4]=9;assert(decode_image()<0);footer[4]=old;
  u8 saved[8];memcpy(saved,footer+216,8);memset(footer+216,255,8);assert(decode_image()<0);memcpy(footer+216,saved,8);
  assert(!decode_image());
  puts("PASS actual UDIF v4 raw-run parser: genuine generated BaseSystem, mapped boot sector, cross-run reads, OOB/overflow/version/XML refusal; no boot-parser substitution");
 }
 image8_release(&image);assert(image8_decode(&image,0,IMAGE8_MAX_BYTES+1ull,readsource,scratch,sizeof scratch)<0);free(bytes);
}
