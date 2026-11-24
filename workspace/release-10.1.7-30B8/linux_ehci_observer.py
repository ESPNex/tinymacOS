"""Read-only whole-original EHCI runtime execution observer, not just symbols."""
import struct

def observe_ehci(guest,symbols,work):
    for name in ['ehci_irq','ehci_hub_control','ehci_urb_enqueue','tmc_ehci_usb_hcd_submit_urb','tmc_linux_ehci_control','usb_giveback_urb_bh','tmc_ehci_usb_hcd_giveback_urb']:
        assert any(line.split()[-1].split('.')[0]==name for line in symbols),name
    address=int(next(line.split()[0] for line in symbols if line.endswith(' b ehci_stats')),16)
    path=work/'whole-ehci-counters.bin'
    guest.req('pmemsave',val=address,size=80,filename=str(path))
    result=dict(zip(['controllers','submissions','givebacks','unlinks','map_failures','irq_polls','schedule_polls','timer_fires','root_polls','errors'],struct.unpack('<10Q',path.read_bytes())))
    assert result['controllers']>=1 and result['submissions']>result['givebacks']>0,result
    assert result['irq_polls']>0 and result['root_polls']>0 and result['timer_fires']>0,result
    assert not result['map_failures'] and not result['errors'],result
    return result
