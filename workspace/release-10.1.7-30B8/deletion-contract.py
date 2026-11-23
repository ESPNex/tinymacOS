#!/usr/bin/env python3
"""Compile and execute the actual new kernel removal and command parser with ASan/UBSan.
Kernel block I/O is a fault-injected RAM fixture, not a user-owned device.
"""
from pathlib import Path
import hashlib,json,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';w=Path(tempfile.mkdtemp(prefix='tmc1017-delete-host-',dir='/var/tmp'))
abi=(root/'include/abi.h').read_text();a=abi.index('/* S_REMOVE flags');b=abi.index('\n',abi.index('enum {REMOVE_BAD_PATH',a));defines=abi[a:b]
src=(root/'fs/btrfs.c').read_text();actual=src[src.index('int fs_remove_flags_i('):src.index('int fs_move_i(')]
base=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
#define NSLOTS 384
#define NODE0 64
typedef struct __attribute__((packed)){u8 kind,ro;u16 parent;u32 size,gen;char name[48];u32 reserved;}Node;
static Node nodes[NSLOTS];static unsigned usednodes,fsrev;static int target=1,readboot,fail_at,writes;
static unsigned char disk[sizeof nodes];
static int writeblock(u32 l,const void *p){writes++;if(fail_at && writes==fail_at)return -1;memcpy(disk+(l-NODE0)*512,p,512);return 0;}
static struct{int(*write)(u32,const void*);}block={writeblock};
#define kmalloc malloc
#define kfree free
static const char *mountedpath(const char*p){return p;}
static int bootfs_lookup(const char*p){(void)p;return readboot?1024:-2;}
static int resolve(const char*p){(void)p;return target;}
static void reset(void){memset(nodes,0,sizeof nodes);nodes[0].kind=1;nodes[1]=(Node){.kind=1,.parent=0};nodes[2]=(Node){.kind=1,.parent=1};nodes[3]=(Node){.kind=2,.parent=2};nodes[4]=(Node){.kind=2,.parent=1};nodes[9]=(Node){.kind=2,.parent=0};usednodes=5;fsrev=7;target=1;readboot=fail_at=writes=0;memcpy(disk,nodes,sizeof nodes);}
'''
tests=r'''
int main(void){
 reset();assert(fs_remove_i("/tree")==-REMOVE_NOT_EMPTY);assert(usednodes==5&&fsrev==7);
 assert(fs_remove_flags_i("/tree",REMOVE_FILE_ONLY)==-REMOVE_WRONG_TYPE);
 assert(fs_remove_flags_i("/tree",8)==-REMOVE_BAD_PATH);
 assert(fs_remove_flags_i("/tree",REMOVE_DIR_ONLY|REMOVE_RECURSIVE)==-REMOVE_BAD_PATH);
 target=0;assert(fs_remove_flags_i("/",REMOVE_RECURSIVE)==-REMOVE_BAD_PATH);
 target=1;readboot=1;assert(fs_remove_flags_i("/System",REMOVE_RECURSIVE)==-REMOVE_READ_ONLY);
 reset();nodes[3].ro=1;assert(fs_remove_flags_i("/tree",REMOVE_RECURSIVE)==-REMOVE_READ_ONLY);assert(nodes[1].kind==1&&nodes[4].kind==2&&writes==0);
 reset();nodes[0].ro=1;assert(fs_remove_flags_i("/tree",REMOVE_RECURSIVE)==-REMOVE_READ_ONLY);
 reset();assert(!fs_remove_flags_i("/tree",REMOVE_RECURSIVE));assert(!nodes[1].kind&&!nodes[2].kind&&!nodes[3].kind&&!nodes[4].kind&&nodes[9].kind==2&&usednodes==1&&fsrev==8);assert(!memcmp(disk,nodes,sizeof nodes));
 reset();target=9;assert(fs_remove_flags_i("/file",REMOVE_DIR_ONLY)==-REMOVE_WRONG_TYPE);assert(!fs_remove_flags_i("/file",REMOVE_FILE_ONLY));assert(usednodes==4);
 reset();target=3;assert(!fs_remove_i("/file"));target=2;assert(!fs_remove_flags_i("/empty",REMOVE_DIR_ONLY));
 reset();Node before[NSLOTS];memcpy(before,nodes,sizeof nodes);fail_at=1;assert(fs_remove_flags_i("/tree",REMOVE_RECURSIVE)==-REMOVE_IO);assert(!memcmp(before,nodes,sizeof nodes)&&!memcmp(before,disk,sizeof nodes)&&usednodes==5&&fsrev==7);
 reset();nodes[9].parent=1;memcpy(disk,nodes,sizeof nodes);memcpy(before,nodes,sizeof nodes);fail_at=2;assert(fs_remove_flags_i("/tree",REMOVE_RECURSIVE)==-REMOVE_IO);assert(!memcmp(before,nodes,sizeof nodes)&&!memcmp(before,disk,sizeof nodes)&&usednodes==5);
 reset();memset(nodes,0,sizeof nodes);nodes[0].kind=1;for(unsigned i=1;i<NSLOTS;i++)nodes[i]=(Node){.kind=1,.parent=i-1};usednodes=NSLOTS-1;assert(!fs_remove_flags_i("/deep",REMOVE_RECURSIVE));assert(!usednodes);for(unsigned i=1;i<NSLOTS;i++)assert(!nodes[i].kind);
 puts("PASS actual removal: empty-only rmdir, typed unlink, recursive subtree, root/RO protection, preflight before mutation, sibling preservation, first/later write rollback and 383-level bounded traversal");
}
'''
command=(root/'userspace/command.c').read_text();helper=command[command.index('static int remove_command('):command.index('static int run(void)')]
cbase=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PATHLEN 144
static struct{int argc;char argv[12][PATHLEN];}A;
#define V(i) A.argv[i]
typedef struct{int dir;}Dirent;
static char pa[PATHLEN],out[2048];static unsigned flags_seen;static int stat_missing,remove_error,stat_dir,calls;
#define S_STAT 1
#define S_REMOVE 2
#define eq(a,b) (!strcmp((a),(b)))
static void say(const char*s){strcat(out,s);}static void ln(const char*s){say(s);say("\n");}static int fail(const char*s){ln(s);return 1;}
static int path(char*p,int k){strcpy(p,V(k));return 0;}
static long sc(int op,const char*p,uintptr_t b,int c,int d){(void)p;(void)c;(void)d;if(op==S_STAT){((Dirent*)b)->dir=stat_dir;return stat_missing?-1:0;}calls++;flags_seen=b;return remove_error;}
#define sc(op,p,b,c,d) sc((op),(p),(uintptr_t)(b),(c),(d))
static void args(const char*a,const char*b,const char*c){memset(&A,0,sizeof A);A.argc=1;strcpy(V(0),a);if(b)strcpy(V(A.argc++),b);if(c)strcpy(V(A.argc++),c);out[0]=0;stat_missing=remove_error=stat_dir=calls=0;flags_seen=0;}
'''
ctest=r'''
int main(void){
 args("rmdir","empty/",0);assert(!remove_command(1)&&calls==1&&flags_seen==REMOVE_DIR_ONLY);
 args("rmdir","full",0);remove_error=-REMOVE_NOT_EMPTY;assert(remove_command(1)==1&&strstr(out,"non vuota"));
 args("rm","folder",0);remove_error=-REMOVE_WRONG_TYPE;assert(remove_command(0)==1&&flags_seen==REMOVE_FILE_ONLY&&strstr(out,"rm -r"));
 args("rm","-rf","folder");assert(!remove_command(0)&&flags_seen==REMOVE_RECURSIVE);
 args("rm","-f","absent");stat_missing=1;assert(!remove_command(0)&&!calls);
 args("rm","-f",0);assert(!remove_command(0));args("rm",0,0);assert(remove_command(0)==1);
 args("rm","-r","/");assert(remove_command(0)==1&&!calls);args("rm","-rf","..");assert(remove_command(0)==1&&!calls);args("rm","-r","dir/./");assert(remove_command(0)==1&&!calls);
 args("rm","--","-file");assert(!remove_command(0)&&calls==1);
 args("rm","-z","file");assert(remove_command(0)==1&&!calls);
 puts("PASS actual command parser: rmdir empty-only, rm/rm-r/rm-rf/rm-f, --, missing operands, invalid flags, root/dot/dotdot rejection and explicit non-empty/type diagnostics");
}
'''
r={'passed':False,'cases':[],'source_sha256':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ['fs/btrfs.c','userspace/command.c','include/abi.h']},'physical_hardware_tested':False}
try:
 for name,body in [('kernel-remove',base+defines+actual+tests),('command-remove',cbase+defines+helper+ctest)]:
  p=w/(name+'.c');p.write_text(body);exe=w/name;subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(p),'-o',str(exe)],check=True);test=subprocess.run([str(exe)],text=True,capture_output=True,check=True);print(test.stdout,flush=True);r['cases'].append({'name':name,'exit_code':test.returncode,'stdout':test.stdout,'stderr':test.stderr})
 r['passed']=True
finally:(q/'deletion-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
