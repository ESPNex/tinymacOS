/* Production window-surface cache. Included after compositor helpers. */
static int cache_scene(View *v, u64 tag, int focus, Rect destination, int projected)
{
    Rect q = v->box;
    if(projected)tag=geo(hash(tag,0x4d4350524f4aull),destination)|1;
    u64 revision=hash(hash(v->s.seq,v->s.pid),focus);
    /* Freeze the optical texture during move; during resize reuse it until a
     * new client transaction arrives. Re-key the real backdrop on release. */
    if(!projected&&window_interacting(v)&&v->surface&&v->surfaceRevision==revision)return 1;
    int correct = v->sw == q.w && v->sh == q.h;
    if (!correct || !v->surface) {
        dropbuf(&v->surface);
        v->surface = buf(q.w, q.h);
        v->sw = q.w;
        v->sh = q.h;
        v->key = 0;
    }
    if (!v->surface)return 0;
    u64 content = geo(hash(hash(v->s.seq, v->s.pid), focus), q) | 1;
    if (v->key == tag || v->dead)return 1;
    copyout(q, undo);
    if(projected)glass_project_backdrop(q,destination,1);
    wincontent(v, focus);
    copyout(q, v->surface);
    copyin(q, undo);
    if(projected)glass_project_backdrop(q,destination,0);
    v->key = tag;
    v->surfaceContent = content;
    v->surfaceRevision = revision;
    return 1;
}

static int cache(View *v,u64 tag,int focus)
{
    return cache_scene(v,tag,focus,v->box,0);
}
static int cache_projected(View *v,u64 tag,int focus,Rect destination)
{
    return cache_scene(v,tag,focus,destination,1);
}
