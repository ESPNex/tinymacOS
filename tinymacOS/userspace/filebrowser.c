#include "filebrowser.h"
/* Shared real filesystem locations/listing with Finder, not a synthetic chooser. */
const char *const browser_labels[BROWSER_PLACES] = {"Inizio", "Scrivania", "Documenti", "Download", "Immagini", "Applicazioni", "Disco di avvio", "KernelCollections", "Libreria"};
const char *const browser_paths[BROWSER_PLACES] = {HOME, HOME "/Desktop", DOCS, HOME "/Downloads", HOME "/Pictures", "/Applications", "/", KCDIR, "/Library"};
const int browser_icons[BROWSER_PLACES] = {8, 9, 6, 10, 11, 13, 33, 32, 37};
const char *browser_label(int i){return i==0&&si.user[0]?si.user:i>=0&&i<BROWSER_PLACES?browser_labels[i]:"";}
int browser_list(const char *path, Dirent *entries, int cap)
{
    return sc(S_LIST, path, entries, cap, 0);
}
int browser_finder(const char *path)
{
    Dirent d;
    if (sc(S_STAT, path, &d, 0, 0) < 0 || !d.dir)return -1;
    return sc(S_SPAWN, FINDER, path, 0, 0);
}

/* Bounded buffers with explicit look-ahead; skip is not an inode-slot limit. */
int browser_page(const char*path,Dirent*entries,int cap,int skip,int*more)
{
 if(!more||cap<1||cap>NSLOTS||skip<0||skip>0x7fffffff-cap)return -1;*more=0;
 int n=sc(S_LIST,path,entries,cap,skip);if(n<0)return -1;
 if(n==cap){Dirent next;int r=sc(S_LIST,path,&next,1,skip+n);if(r<0)return -1;*more=r>0;}
 return n;
}
