/* Production window-surface cache. Included after compositor helpers. */
static int cache(View *v, u64 tag, int focus)
{
    Rect q = v->box;
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
    /* Mission Control transforms a window texture, not its material backdrop.
     * Keep that texture through dim/scale frames; live client updates still
     * invalidate it. The first settled frame rekeys the real backdrop. */
    if (v->motion.kind == AN_MC && v->key && v->surfaceContent == content)return 1;
    copyout(q, undo);
    wincontent(v, focus);
    copyout(q, v->surface);
    copyin(q, undo);
    v->key = tag;
    v->surfaceContent = content;
    return 1;
}
