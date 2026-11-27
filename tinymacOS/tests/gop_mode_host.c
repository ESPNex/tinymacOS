/* Real loader predicates; synthetic GOP records, NOT a physical-GPU proof. */
#include <efi.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../boot/gopmode.h"
int main(void)
{
 EFI_GRAPHICS_OUTPUT_MODE_INFORMATION m={.HorizontalResolution=1366,.VerticalResolution=768,.PixelsPerScanLine=1408,.PixelFormat=PixelBlueGreenRedReserved8BitPerColor};
 EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mode={.Info=&m,.FrameBufferBase=0xe0000000,.FrameBufferSize=1408*768*4};
 assert(gop_usable(&m)&&gop_framebuffer_usable(&mode)&&gop_rgb(&m)==0);
 m.PixelFormat=PixelRedGreenBlueReserved8BitPerColor;assert(gop_rgb(&m)==1&&gop_usable(&m));
 m.PixelFormat=PixelBitMask;m.PixelInformation=(EFI_PIXEL_BITMASK){.RedMask=0xff0000,.GreenMask=0xff00,.BlueMask=0xff,.ReservedMask=0xff000000};assert(gop_rgb(&m)==0&&gop_usable(&m));
 m.PixelInformation.RedMask=0xff;m.PixelInformation.BlueMask=0xff0000;assert(gop_rgb(&m)==1&&gop_usable(&m));
 m.PixelInformation.ReservedMask=0;assert(gop_usable(&m));
 m.PixelInformation.GreenMask=0x07e0;assert(!gop_usable(&m));
 m.PixelFormat=PixelBltOnly;assert(!gop_usable(&m));
 m.PixelFormat=PixelBlueGreenRedReserved8BitPerColor;
 for(unsigned w=800;w<=1920;w++){m.HorizontalResolution=w;m.PixelsPerScanLine=w;assert(gop_usable(&m));}
 m.HorizontalResolution=1921;m.PixelsPerScanLine=1921;assert(!gop_usable(&m));
 m.HorizontalResolution=1920;m.PixelsPerScanLine=1919;assert(!gop_usable(&m));
 m.PixelsPerScanLine=1920;m.VerticalResolution=1081;assert(!gop_usable(&m));
 m.VerticalResolution=1080;mode.FrameBufferSize=(UINTN)1920*1080*4;assert(gop_framebuffer_usable(&mode));
 mode.FrameBufferSize--;assert(!gop_framebuffer_usable(&mode));
 mode.FrameBufferSize++;mode.FrameBufferBase=0x100000000ULL-4096;assert(!gop_framebuffer_usable(&mode));
 mode.FrameBufferBase=~(UINT64)0-16;assert(!gop_framebuffer_usable(&mode));
 mode.FrameBufferBase=0;assert(!gop_framebuffer_usable(&mode));
 puts("PASS GOP: 1366x768/padded stride, all width remainders and standard RGB/BGR masks accepted; BLT-only, arbitrary masks, invalid dimensions/stride/aperture and overflowing addresses rejected. Not physical hardware acceptance.");
}
