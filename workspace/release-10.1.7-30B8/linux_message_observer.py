"""Observe actual non-secret message.c counters; no guest test decoder/code."""
import struct

def observe_message(guest,symbols,work):
    for name in ['usb_control_msg','usb_control_msg_send','usb_control_msg_recv','usb_get_descriptor','tmc_linux_core_control']:
        assert any(line.endswith(' '+name) for line in symbols),name
    address=int(next(line.split()[0] for line in symbols if line.endswith(' b tmc_linux_message_counters')),16)
    path=work/'message-counters.bin'
    guest.req('pmemsave',val=address,size=56,filename=str(path))
    counts=dict(zip(['requests','completed','errors','rejected','detached','waits','timeouts'],struct.unpack('<7Q',path.read_bytes())))
    assert counts['requests']>=1 and counts['completed']>=1,counts
    assert 0<=counts['requests']-counts['completed']-counts['errors']<=1,counts
    assert counts['waits']>=counts['completed'] and not counts['timeouts'],counts
    return counts
