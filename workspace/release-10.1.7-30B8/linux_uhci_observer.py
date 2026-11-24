"""Read-only observation of whole-original UHCI runtime, not mere linkage."""
import struct

def observe_uhci(guest,symbols,work):
    for name in ['uhci_scan_schedule','uhci_hub_control','uhci_urb_enqueue','uhci_irq','tmc_uhci_usb_hcd_submit_urb','tmc_linux_uhci_control','tmc_linux_uhci_interrupt']:
        assert any(line.split()[-1].split('.')[0]==name for line in symbols),name
    address=int(next(line.split()[0] for line in symbols if line.endswith(' b uhci_stats')),16)
    path=work/'whole-uhci-counters.bin'
    guest.req('pmemsave',val=address,size=80,filename=str(path))
    result=dict(zip(['controllers','submissions','givebacks','unlinks','map_failures','irq_polls','schedule_polls','timer_fires','root_polls','errors'],struct.unpack('<10Q',path.read_bytes())))
    assert result['controllers']>=1 and result['submissions']>result['givebacks']>0,result
    assert result['irq_polls']>0 and result['schedule_polls']>0 and result['timer_fires']>0 and result['root_polls']>0,result
    assert not result['map_failures'] and not result['errors'],result
    return result
