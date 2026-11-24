#include "filebrowser.h"
/* Shared real filesystem locations/listing with Finder, not a synthetic chooser. */
const char *const browser_labels[BROWSER_PLACES] = {"Inizio", "Scrivania", "Documenti", "Download", "Immagini", "Applicazioni", "Disco di avvio", "KernelCollections", "Libreria"};
const char *const browser_paths[BROWSER_PLACES] = {HOME, HOME "/Desktop", DOCS, HOME "/Downloads", HOME "/Pictures", "/Applications", "/", KCDIR, "/Library"};
const int browser_icons[BROWSER_PLACES] = {8, 9, 6, 10, 11, 13, 33, 32, 37};
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
