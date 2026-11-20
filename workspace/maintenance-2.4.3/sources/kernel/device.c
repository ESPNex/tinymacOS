#include "k.h"
// Drivers register only after probing succeeds. Operations stay in the kernel;
// userspace only receives a value copy of the public device description.
static struct {
    DeviceInfo info;
    void *operations;
} devices[16];
static int count;
void device_add(const char *name, const char *kind, void *ops) {
    if (count >= 16)
        return;
    strset(devices[count].info.name, name, sizeof devices[count].info.name);
    strset(devices[count].info.kind, kind, sizeof devices[count].info.kind);
    devices[count++].operations = ops;
}
int device_list(DeviceInfo *out, int capacity) {
    int n = MIN(count, capacity);
    for (int j = 0; j < n; j++)
        out[j] = devices[j].info;
    return n;
}
