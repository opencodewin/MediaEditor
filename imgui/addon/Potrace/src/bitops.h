#ifndef BITOPS_H
#define BITOPS_H
/* ---------------------------------------------------------------------- */
/* machine specific macros */

#if (defined __i386__ || defined __x86_64__)

static inline unsigned int lobit(unsigned int x)
{
  unsigned int res;
  asm("bsf	%1,%0\n\t"
      "jnz	0f\n\t"
      "movl	$32,%0\n"
      "0:"
      : "=r"(res)
      : "r"(x)
      : "cc");
  return res;
}

static inline unsigned int hibit(unsigned int x)
{
  unsigned int res;

  asm("bsr	%1,%0\n\t"
      "jnz	0f\n\t"
      "movl	$-1,%0\n"
      "0:"
      : "=r"(res)
      : "r"(x)
      : "cc");
  return res + 1;
}

/* ---------------------------------------------------------------------- */
#else /* generic macros */

static inline unsigned int lobit(unsigned int x)
{
  unsigned int res = 32;
  while (x & 0xffffff)
  {
    x <<= 8;
    res -= 8;
  }
  while (x)
  {
    x <<= 1;
    res -= 1;
  }
  return res;
}

static inline unsigned int hibit(unsigned int x)
{
  unsigned int res = 0;
  while (x > 0xff)
  {
    x >>= 8;
    res += 8;
  }
  while (x)
  {
    x >>= 1;
    res += 1;
  }
  return res;
}

#endif

#endif /* BITOPS_H */
