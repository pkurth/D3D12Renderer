#ifndef INT_INSN_H
#define INT_INSN_H

#include <stdint.h>

static inline uint32_t uint32_nor( uint32_t a, uint32_t b )
{
  return ~(a | b);
}

static inline uint32_t uint32_andc( uint32_t a, uint32_t b )
{
  return (a & (~b));
}

static inline uint32_t uint32_and( uint32_t a, uint32_t b )
{
  return (a & b);
}

static inline uint32_t uint32_or( uint32_t a, uint32_t b )
{
  return (a | b);
}

static inline uint32_t uint32_or3( uint32_t a, uint32_t b, uint32_t c )
{
  return (a | b | c);
}

static inline uint32_t uint32_mux( uint32_t mask, uint32_t a, uint32_t b )
{
  return ( ( mask & (uint32_t)a) | ( (~mask) & (uint32_t)b ) );
}

static inline uint32_t uint32_lt( uint32_t a, uint32_t b )
{
  // NOTE: Result is invalid when a=INT32_MAX, b=INT32_MIN
  // For the purposes used in half.c the result is always valid
  return (uint32_t)( (int32_t)( a - b ) >> 31 );
}

static inline uint32_t uint32_gte( uint32_t a, uint32_t b )
{
  return ~uint32_lt( a, b );
}

static inline uint32_t uint32_gt( uint32_t a, uint32_t b )
{
  // NOTE: Result is invalid when b=INT32_MIN, a=INT32_MAX
  // For the purposes used in half.c the result is always valid
  return (uint32_t)( (int32_t)( b - a ) >> 31 );
}

static inline uint32_t uint32_nez( uint32_t a )
{
  return (uint32_t)( (int32_t)( a | -a ) >> 31 );
}

static inline uint32_t uint32_eqz( uint32_t a )
{
  return ~uint32_nez( a );
}

static inline uint32_t uint32_nez_p( uint32_t a )
{
  return (uint32_t)( (uint32_t)( a | -a ) >> 31 );
}

static inline uint32_t uint32_eq( uint32_t a, uint32_t b )
{
  return ( ~uint32_nez( a-b ) );
}

static inline uint32_t uint32_srl( uint32_t a, uint32_t sa )
{
  return (a >> sa);
}

static inline uint32_t uint32_sll( uint32_t a, uint32_t sa )
{
  return (a << sa);
}

static inline uint32_t uint32_cp( uint32_t a )
{
  return (a);
}

static inline uint32_t uint32_add( uint32_t a, uint32_t b )
{
  return (a + b);
}

static inline uint32_t uint32_sub( uint32_t a, uint32_t b )
{
  return (a - b);
}

static inline uint16_t uint16_mux( uint16_t mask, uint16_t a, uint16_t b )
{
  return ( ( mask & (uint16_t)a) | ( (~mask) & (uint16_t)b ) );
}

static inline uint16_t uint16_lt( uint16_t a, uint16_t b )
{
  // NOTE: Result is invalid when a=INT16_MAX, b=INT16_MIN
  // For the purposes used in half.c the result is always valid
  return (uint16_t)( (int16_t)( a - b ) >> 15 );
}

static inline uint16_t uint16_gte( uint16_t a, uint16_t b )
{
  return ~uint16_lt( a, b );
}

static inline uint16_t uint16_gt( uint16_t a, uint16_t b )
{
  // NOTE: Result is invalid when b=INT32_MIN, a=INT32_MAX
  // For the purposes used in half.c the result is always valid
  return (uint16_t)( (int16_t)( b - a ) >> 15 );
}

static inline uint16_t uint16_nez( uint16_t a )
{
  return (uint16_t)( (int16_t)( a | -a ) >> 15 );
}

static inline uint16_t uint16_eqz( uint16_t a )
{
  return ~uint16_nez( a );
}

static inline uint16_t uint16_nez_p( uint16_t a )
{
  return (uint16_t)( (uint16_t)( a | -a ) >> 15 );
}

static inline uint16_t uint16_eqz_p( uint16_t a )
{
  return ~uint16_nez_p( a );
}

static inline uint16_t uint16_eq( uint16_t a, uint16_t b )
{
  return ( ~uint16_nez( a-b ) );
}

static inline uint16_t uint16_andc( uint16_t a, uint16_t b )
{
  return (a & (~b));
}

static inline uint16_t uint16_and( uint16_t a, uint16_t b )
{
  return (a & b);
}

static inline uint16_t uint16_andsrl( uint16_t a, uint16_t b, uint16_t sa )
{
  return ((a & b) >> sa);
}

static inline uint16_t uint16_or( uint16_t a, uint16_t b )
{
  return (a | b);
}

static inline uint16_t uint16_or3( uint16_t a, uint16_t b, uint16_t c )
{
  return (a | b | c);
}

static inline uint16_t uint16_add( uint16_t a, uint16_t b )
{
  return (a + b);
}

static inline uint16_t uint16_addm( uint16_t a, uint16_t b, uint16_t mask )
{
  return ((a + b) & mask);
}

static inline uint16_t uint16_sub( uint16_t a, uint16_t b )
{
  return (a - b);
}

static inline uint16_t uint16_xor( uint16_t a, uint16_t b )
{
  return (a ^ b);
}

static inline uint16_t uint16_srl( uint16_t a, uint16_t sa )
{
  return (a >> sa);
}

static inline uint16_t uint16_srlm( uint16_t a, uint16_t sa, uint16_t mask )
{
  return ((a >> sa) & mask);
}

static inline uint16_t uint16_sll( uint16_t a, uint16_t sa )
{
  return (a << sa);
}

static inline uint16_t uint16_not( uint16_t a )
{
  return (~a);
}

static inline uint16_t uint16_cp( uint16_t a )
{
  return (a);
}

static inline uint16_t uint16_cntlz( uint16_t x )
{
  const uint16_t x0  = uint16_srl(  x,  1 );
  const uint16_t x1  = uint16_or(   x,  x0 );
  const uint16_t x2  = uint16_srl(  x1, 2 );
  const uint16_t x3  = uint16_or(   x1, x2 );
  const uint16_t x4  = uint16_srl(  x3, 4 );
  const uint16_t x5  = uint16_or(   x3, x4 );
  const uint16_t x6  = uint16_srl(  x5, 8 );
  const uint16_t x7  = uint16_or(   x5, x6 );
  const uint16_t x8  = uint16_not(  x7 );
  const uint16_t x9  = uint16_srlm( x8, 1, 0x5555 );
  const uint16_t xA  = uint16_sub(  x8, x9 );
  const uint16_t xB  = uint16_and(  xA, 0x3333 );
  const uint16_t xC  = uint16_srlm( xA, 2, 0x3333 );
  const uint16_t xD  = uint16_add(  xB, xC );
  const uint16_t xE  = uint16_srl(  xD, 4 );
  const uint16_t xF  = uint16_addm( xD, xE, 0x0f0f );
  const uint16_t x10 = uint16_srl(  xF, 8 );
  const uint16_t x11 = uint16_addm( xF, x10, 0x001f );

  return ( x11 );
}

#endif /* INT_INSN_H */
