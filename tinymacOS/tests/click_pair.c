#include <assert.h>
#include <stdio.h>
#include "click_pair.h"
int main(void)
{
    ClickPair p={0};
    assert(!click_pair(&p,100,10,100,100));
    assert(click_pair(&p,150,10,108,108)); /* 500 ms, ordinary tap drift */
    assert(!click_pair(&p,160,10,108,108)); /* third click cannot activate */
    assert(click_pair(&p,170,10,108,108));
    p=(ClickPair){0};
    assert(!click_pair(&p,100,10,100,100));
    assert(!click_pair(&p,151,10,100,100));
    assert(click_pair(&p,170,10,100,100));
    p=(ClickPair){0};
    assert(!click_pair(&p,100,10,100,100));
    assert(!click_pair(&p,110,11,100,100)); /* different item, same location */
    assert(click_pair(&p,120,11,100,100));
    p=(ClickPair){0};
    assert(!click_pair(&p,100,10,100,100));
    assert(!click_pair(&p,110,10,109,100));
    assert(click_pair(&p,120,10,109,100));
    p=(ClickPair){0};
    assert(!click_pair(&p,100,10,100,100));
    p.armed=0; /* navigation clears outstanding pair */
    assert(!click_pair(&p,110,10,100,100));
    assert(click_pair(&p,120,10,100,100));
    p=(ClickPair){0};
    assert(!click_pair(&p,0,10,0,0)); /* zero is a valid occurrence time */
    assert(click_pair(&p,1,10,0,0));
    assert(!click_pair(&p,100,10,0,0));
    assert(!click_pair(&p,99,10,0,0)); /* time reversal is not a pair */
    p=(ClickPair){0};
    assert(!click_pair(&p,1,10,(-2147483647-1),0));
    assert(!click_pair(&p,2,10,2147483647,0)); /* no signed overflow */
    puts("PASS click pairing: first selects, exactly second activates, third starts a new pair; 500 ms, 8 px tolerance, target identity, navigation reset and overflow bounds");
}
