#pragma once
/* All eight resize directions share hit-testing, cursor selection and geometry.
 * Edges are INSIDE the owning window, so an obscured neighbor cannot steal them. */
enum {EDGE_LEFT=1,EDGE_RIGHT=2,EDGE_TOP=4,EDGE_BOTTOM=8};
static int window_edge(int x,int y,int w,int h)
{
 if(x<0||y<0||x>=w||y>=h)return 0;
 int e=(x<6?EDGE_LEFT:x>=w-6?EDGE_RIGHT:0)|(y<6?EDGE_TOP:y>=h-6?EDGE_BOTTOM:0);
 if(x>=w-18&&y>=h-17)e=EDGE_RIGHT|EDGE_BOTTOM;
 return e;
}
/* Endpoints are clamped, not sizes after an origin shift: opposite edges remain
 * fixed, including left/top resize at minimum size and screen boundaries. */
static void window_resize_geometry(int *x,int*y,int*w,int*h,int ox,int oy,int ow,int oh,int edge,int dx,int dy,int minw,int minh,int screenw,int screenh,int bar)
{
 int l=ox,r=ox+ow,t=oy,b=oy+oh;
 if(edge&EDGE_LEFT)l=MAX(0,MIN(ox+dx,r-minw));
 if(edge&EDGE_RIGHT)r=MIN(screenw,MAX(l+minw,r+dx));
 if(edge&EDGE_TOP)t=MAX(bar,MIN(oy+dy,b-minh));
 if(edge&EDGE_BOTTOM)b=MIN(screenh,MAX(t+minh,b+dy));
 *x=l;*y=t;*w=r-l;*h=b-t;
}
