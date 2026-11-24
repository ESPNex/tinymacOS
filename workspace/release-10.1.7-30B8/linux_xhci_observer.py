"""Read-only original xHCI runtime execution observer in the actual guest."""
import struct

def observe_xhci(guest,symbols,work):
    for name in ['tmc_xhci_xhci_irq','tmc_xhci_xhci_hub_control','xhci_urb_enqueue','tmc_xhci_usb_hcd_submit_urb','tmc_linux_xhci_control','usb_giveback_urb_bh','tmc_xhci_usb_hcd_giveback_urb','tmc_xhci_xhci_queue_intr_tx']:
        assert any(line.split()[-1].split('.')[0]==name for line in symbols),name
    address=int(next(line.split()[0] for line in symbols if line.endswith(' b xhci_stats')),16)
    path=work/'whole-xhci-counters.bin'
    guest.req('pmemsave',val=address,size=80,filename=str(path))
    result=dict(zip(['controllers','submissions','givebacks','unlinks','map_failures','irq_polls','schedule_polls','timer_fires','root_polls','errors'],struct.unpack('<10Q',path.read_bytes())))
    assert result['controllers']>=1 and result['submissions']>result['givebacks']>0,result
    assert result['irq_polls']>0 and result['root_polls']>0 and result['timer_fires']>0,result
    assert not result['map_failures'] and not result['errors'],result
    return result
