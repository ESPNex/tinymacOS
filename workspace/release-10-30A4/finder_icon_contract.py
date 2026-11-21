#!/usr/bin/env python3
"""Execute actual Finder icon routing; only unrelated app lookup imports mocked."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]/'tinymacOS'
def function(path,signature):
 s=path.read_text();a=s.index(signature);b=s.index('{',a);level=1;i=b+1
 while level:
  if s[i]=='{':level+=1
  elif s[i]=='}':level-=1
  i+=1
 return s[a:i]
code='''#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct {const char *name;int dir;} Dirent;
static const char *cwd;
static int eq(const char *a,const char *b){return !strcmp(a,b);}
static int bundle(Dirent *f){return f->dir&&eq(f->name,"Test.app")?1:0;}
static int appicon(int app){assert(app==1);return 2;}
'''+function(root/'userspace/volume.h','static inline int bootpath(')+'\n'+function(root/'userspace/finder.c','static int idico(')+'''
int main(void){
 Dirent f={"Applications",1};cwd="/";assert(idico(&f)==13);
 cwd="/User/local";assert(idico(&f)==6);
 cwd="/";f.dir=0;assert(idico(&f)==7);
 cwd="/System";f.dir=1;assert(idico(&f)==37);
 cwd="/";f.name="Desktop";assert(idico(&f)==23);
 f.name="Documents";assert(idico(&f)==24);
 f.name="Pictures";assert(idico(&f)==26);
 cwd="/Applications";f.name="Test.app";assert(idico(&f)==2);
 puts("PASS actual Finder idico: dedicated /Applications, generic user homonym/file, protected system folder and real app-icon priority preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='tmc30a4-finder-contract-',dir='/var/tmp') as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
