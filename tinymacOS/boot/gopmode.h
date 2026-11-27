#pragma once
/* GOP is a firmware contract, not the Bochs DISPI device. In particular,
 * 1366x768 is legal here even though DISPI cannot switch to that width. */
static int gop_rgb(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *m)
{
    if(m->PixelFormat==PixelRedGreenBlueReserved8BitPerColor)return 1;
    if(m->PixelFormat==PixelBlueGreenRedReserved8BitPerColor)return 0;
    if(m->PixelFormat!=PixelBitMask)return -1;
    /* Only the two packed 32-bit layouts that the native rasterizer handles.
     * Do not reinterpret 16-bit or arbitrary/non-contiguous bit masks. */
    if(m->PixelInformation.GreenMask!=0x0000ff00)return -1;
    if(m->PixelInformation.ReservedMask&&m->PixelInformation.ReservedMask!=0xff000000)return -1;
    if(m->PixelInformation.RedMask==0x000000ff&&m->PixelInformation.BlueMask==0x00ff0000)return 1;
    if(m->PixelInformation.RedMask==0x00ff0000&&m->PixelInformation.BlueMask==0x000000ff)return 0;
    return -1;
}
static int gop_usable(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *m)
{
    return m&&m->HorizontalResolution>=800&&m->HorizontalResolution<=1920&&
        m->VerticalResolution>=600&&m->VerticalResolution<=1080&&
        m->PixelsPerScanLine>=m->HorizontalResolution&&m->PixelsPerScanLine<=16384&&gop_rgb(m)>=0;
}
static int gop_framebuffer_usable(const EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode)
{
    if(!mode||!gop_usable(mode->Info)||!mode->FrameBufferBase)return 0;
    UINT64 bytes=(UINT64)mode->Info->PixelsPerScanLine*mode->Info->VerticalResolution*4;
    /* The boot-stage mapping is 32-bit. Avoid overflow and writes beyond the
     * firmware-advertised aperture; never force a mode which GOP did not offer. */
    return bytes<=mode->FrameBufferSize&&mode->FrameBufferBase<0x100000000ULL&&
        bytes<=0x100000000ULL-mode->FrameBufferBase;
}
