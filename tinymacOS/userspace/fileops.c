#include "fileops.h"
static int canonical(const char*p,char out[PATHLEN]){
 const char*aliases[]={"/Volumes/Macintosh HD","/Volumes/tinymacOS Live"};
 for(unsigned i=0;i<2;i++){size_t n=strlen(aliases[i]);if(strlen(p)>=n&&!memcmp(p,aliases[i],n)&&(!p[n]||p[n]=='/')){p=p[n]?p+n:"/";break;}}
 return absolutepath(out,"/",p);
}
static int insidepath(const char*p,const char*parent){size_t n=strlen(parent);return strlen(p)>=n&&!memcmp(p,parent,n)&&(!p[n]||p[n]=='/'||n==1);}
static int copytree(const char*src,const char*dst,unsigned depth){
 if(depth>32)return -1;Dirent from,exists;if(sc(S_STAT,src,&from,0,0)<0)return -1;
 if(!sc(S_STAT,dst,&exists,0,0))return FILEOP_EXISTS;
 if(from.dir){
  if(sc(S_CREATE,dst,1,0,0)<0)return -1;
  Dirent children[16];int skip=0,r;
  while((r=sc(S_LIST,src,children,16,skip))>0){
   for(int j=0;j<r;j++){char a[PATHLEN],b[PATHLEN];if(absolutepath(a,src,children[j].name)||absolutepath(b,dst,children[j].name)||copytree(a,b,depth+1)<0)goto bad;}
   if(skip>0x7fffffff-r)goto bad;skip+=r;
  }
  if(r<0)goto bad;return 0;
 }else{
  if(!from.size)return sc(S_CREATE,dst,0,0,0)<0?-1:0;
  int input=sc(S_OPEN,src,0,0,0);if(input<0)return -1;
  if(sc(S_CREATE,dst,0,0,0)<0){sc(S_CLOSE,input,0,0,0);return -1;}
  int output=sc(S_OPEN,dst,1,0,0),failed=output<0;static u8 buffer[8192];u64 at=0;
  if(!failed)do{int take=MIN((u64)sizeof buffer,from.size-at),n=sc(S_READ,input,buffer,take,0);if(n!=take||sc(S_WRITE,output,buffer,take,0)!=take){failed=1;break;}at+=take;}while(at<from.size);
  sc(S_CLOSE,input,0,0,0);if(output>=0)sc(S_CLOSE,output,0,0,0);
  if(!failed)return 0;
 }
 bad:sc(S_REMOVE,dst,REMOVE_RECURSIVE,0,0);return -1;
}
int fileop_copy(const char*source,const char*destination){
 char src[PATHLEN],dst[PATHLEN];if(canonical(source,src)||canonical(destination,dst))return -1;
 Dirent from;if(sc(S_STAT,src,&from,0,0)<0)return -1;
 if(eq(src,dst)||(from.dir&&insidepath(dst,src)))return FILEOP_CYCLE;
 return copytree(src,dst,0);
}
int fileop_place(const char*source,const char*folder,int move,char out[PATHLEN]){
 char src[PATHLEN],dir[PATHLEN],name[48];Dirent from,to;
 if(canonical(source,src)||canonical(folder,dir)||sc(S_STAT,src,&from,0,0)<0||sc(S_STAT,dir,&to,0,0)<0||!to.dir||to.ro||(move&&from.ro))return -1;
 if(from.dir&&insidepath(dir,src))return FILEOP_CYCLE;
 if(absolutepath(out,dir,leaf(src)))return -1;
 if(move&&eq(src,out))return FILEOP_CYCLE;
 for(unsigned i=0;!sc(S_STAT,out,&to,0,0);i++){
  if(i>=9999)return FILEOP_EXISTS;
  strset(name,"Copia ",sizeof name);if(i){char num[16];number(num,i+1);append(name,num,sizeof name);append(name," ",sizeof name);}
  const char*base=leaf(src);int end=0,room=47-strlen(name);while(base[end]){int next=text_next(base,end);if(next>room)break;end=next;}int prefix=strlen(name);memcpy(name+prefix,base,end);name[prefix+end]=0;
  if(absolutepath(out,dir,name))return -1;
 }
 return move?(int)sc(S_MOVE,src,out,0,0):fileop_copy(src,out);
}
