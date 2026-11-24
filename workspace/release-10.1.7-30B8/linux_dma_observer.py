"""Observe original DMA allocator use by actual native EHCI/UHCI TD consumers."""
import struct

def observe_dma(guest,symbols,work):
    for name in ['dma_pool_create','dma_pool_alloc','dma_pool_free','dma_pool_destroy','dmam_pool_create','dmam_pool_destroy','tmc_linux_dma_pool_alloc','tmc_dma_os_coherent']:
        assert any(line.endswith(' '+name) for line in symbols),name
    address=int(next(line.split()[0] for line in symbols if line.endswith(' b tmc_linux_dma_counters')),16)
    path=work/'dma-pool-counters.bin'
    guest.req('pmemsave',val=address,size=96,filename=str(path))
    counts=dict(zip(['devices','pools','allocations','frees','coherent_allocations','coherent_frees','live_bytes','highwater','rejected','failures','attributes','devres'],struct.unpack('<12Q',path.read_bytes())))
    # Mixed EHCI+UHCI: EHCI owns one persistent control-chain allocation,
    # while its companion's TD/QH allocations go directly through original
    # dmapool, not the old native facade counters. Read actual runtime state;
    # symbol presence alone does not prove that a controller is active.
    uhci_controllers=0
    symbol=next((line for line in symbols if line.endswith(' b uhci_stats')),None)
    if symbol:
        path=work/'dma-uhci-controller-count.bin'
        guest.req('pmemsave',val=int(symbol.split()[0],16),size=8,filename=str(path))
        uhci_controllers=struct.unpack('<Q',path.read_bytes())[0]
    ehci_controllers=0
    symbol=next((line for line in symbols if line.endswith(' b ehci_stats')),None)
    if symbol:
        path=work/'dma-ehci-controller-count.bin'
        guest.req('pmemsave',val=int(symbol.split()[0],16),size=8,filename=str(path))
        ehci_controllers=struct.unpack('<Q',path.read_bytes())[0]
    xhci_controllers=0
    symbol=next((line for line in symbols if line.endswith(' b xhci_stats')),None)
    if symbol:
        path=work/'dma-xhci-controller-count.bin'
        guest.req('pmemsave',val=int(symbol.split()[0],16),size=8,filename=str(path))
        xhci_controllers=struct.unpack('<Q',path.read_bytes())[0]
    assert counts['pools']>=2*counts['devices'],counts
    if uhci_controllers or ehci_controllers or xhci_controllers:
        assert counts['attributes']>=counts['devices']+uhci_controllers+ehci_controllers+xhci_controllers,counts
        assert counts['allocations']>=counts['devices'],counts
    else:
        assert counts['devices']>=1 and counts['allocations']>=2,counts
        assert counts['allocations']>counts['frees'],counts
    assert counts['coherent_allocations']>counts['coherent_frees'] and counts['live_bytes']>0,counts
    assert counts['highwater']>=counts['live_bytes'] and not counts['failures'] and not counts['rejected'],counts
    return counts
