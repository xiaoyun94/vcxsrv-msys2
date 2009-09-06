/* 
 *   Create a dependency that should be immune from the effect of register
 *   renaming as is commonly seen in superscalar processors.  This should
 *   insert a minimum of 100-ns delays between reads/writes at clock rates
 *   up to 100 MHz---GGL
 */ 

#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include <X11/X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "compiler.h"

static int really_slow_bcopy;

void
xf86SetReallySlowBcopy(void)
{
	really_slow_bcopy = 1;
}

#if defined(__i386__) || defined(__amd64__)
static void xf86_really_slow_bcopy(unsigned char *src, unsigned char *dst, int len)
{
    while(len--)
    {
	*dst++ = *src++;
	outb(0x80, 0x00);
    }
}
#endif

/* The outb() isn't needed on my machine, but who knows ... -- ost */
void
xf86SlowBcopy(unsigned char *src, unsigned char *dst, int len)
{
#if defined(__i386__) || defined(__amd64__)
    if (really_slow_bcopy) {
	xf86_really_slow_bcopy(src, dst, len);
	return;
    }
#endif
    while(len--)
	*dst++ = *src++;
}
