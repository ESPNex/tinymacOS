"""Observe original DMA allocator use by actual native EHCI/UHCI TD consumers."""
import struct

def observe_dma(guest,symbols,work):
    for name in ['dma_pool_create','dma_pool_alloc','dma_pool_free','dma_pool_destroy','dmam_pool_create','dmam_pool_destroy','tmc_linux_dma_pool_alloc','tmc_dma_os_coherent']:
        assert any(line.endswith(' '+name) for line in symbols),name
    address=int(next(line.split()[0] for line in symbols if line.endswith(' b tmc_linux_dma_counters')),16)
    path=work/'dma-pool-counters.bin'
    guest.req('pmemsave',val=address,size=96,filename=str(path))
    counts=dict(zip(['devices','pools','allocations','frees','coherent_allocations','coherent_frees','live_bytes','highwater','rejected','failures','attributes','devres'],struct.unpack('<12Q',path.read_bytes())))
    whole_uhci=any(line.endswith(' b uhci_stats') for line in symbols) and counts['devices']==0
    if whole_uhci:
        assert counts['attributes']>=1 and counts['coherent_allocations']>counts['coherent_frees'],counts
    else:
        assert counts['devices']>=1 and counts['pools']>=2*counts['devices'],counts
    if not whole_uhci:assert counts['allocations']>=2 and counts['allocations']>counts['frees'],counts
    assert counts['coherent_allocations']>counts['coherent_frees'] and counts['live_bytes']>0,counts
    assert counts['highwater']>=counts['live_bytes'] and not counts['failures'] and not counts['rejected'],counts
    return counts
