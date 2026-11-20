"""Select Recovery with an ordinary USB key at the boot logo; do not attach any other device."""
import time
class RecoveryKey:
    recovery_text='Scegli come proseguire.'
    def wait(self,*a,**k):
        end=time.monotonic()+48
        while time.monotonic()<end:
            im=self.snap()
            if (im.size==(1280,800) and im.getpixel((640,378))==(255,255,255) and im.getpixel((40,40))==(0,0,0) and im.getpixel((730,492))==(52,52,52)):break
            time.sleep(.04)
        else:raise AssertionError('Logo di avvio non osservato')
        for _ in range(9):self.key('r',.12)
        if self.recovery_text:self.visible(self.recovery_text,32)
