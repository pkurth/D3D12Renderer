// Branch-free implementation of half-precision (16 bit) floating point
// Copyright 2006 Mike Acton <macton@gmail.com>
// 
// Preferred compile flags for GCC: 
//     -O3 -fstrict-aliasing -std=c99 -pedantic -Wall -Wstrict-aliasing
//
// Half-precision floating point format
// ------------------------------------
//
//   | Field    | Last | First | Note
//   |----------|------|-------|----------
//   | Sign     | 15   | 15    |
//   | Exponent | 14   | 10    | Bias = 15
//   | Mantissa | 9    | 0     |
//
// Note: USE A BETTER COMPILER
// ---------------------------
//
//     This file is a C99 source file, intended to be compiled with a C99 
//     compliant compiler. However, for the moment it remains combatible
//     with C++98. Therefore if you are using a compiler that poorly implements
//     C standards (e.g. MSVC), it may be compiled as C++. This is not
//     guaranteed for future versions. 
//
// Features
// --------
//
//     * QNaN + <x>  = QNaN
//     * <x>  + +INF = +INF
//     * <x>  - -INF = -INF
//     * INF  - INF  = SNaN
//     * Denormalized values
//     * Difference of ZEROs is always +ZERO
//     * Sum round with guard + round + sticky bit (grs)
//     * And of course... no branching
// 
// Precision of Sum
// ----------------
//
//     uint16_t z = half_add( x, y );
//
//     Will have exactly (0 ulps difference) the same result as:
//     (For 32 bit IEEE 784 floating point and same rounding mode)
//
//     union FLOAT_32
//     {
//       float    f32;
//       uint32_t u32;
//     };
//
//     union FLOAT_32 fx = { .u32 = half_to_float( x ) };
//     union FLOAT_32 fy = { .u32 = half_to_float( y ) };
//     union FLOAT_32 fz = { .f32 = fx.f32 + fy.f32    };
//     uint16_t       z  = float_to_half( fz );
//
// Precision of Difference
// -----------------------
//
//     uint16_t z = half_add( x, -y );
//
//     Will have exactly (0 ulps difference) the same result as:
//     (For 32 bit IEEE 784 floating point and same rounding mode)
//
//     union FLOAT_32
//     {
//       float    f32;
//       uint32_t u32;
//     };
//
//     union FLOAT_32 fx = { .u32 = half_to_float( x ) };
//     union FLOAT_32 fy = { .u32 = half_to_float( y ) };
//     union FLOAT_32 fz = { .f32 = fx.f32 - fy.f32    };
//     uint16_t       z  = float_to_half( fz.u32 );

#pragma warning( disable : 4146 )

#include "int_insn.h"
#include "half.h"

uint32_t
half_to_float( uint16_t h )
{
  const uint16_t one                  = 0x0001;
  const uint16_t two                  = 0x0002;
  const uint32_t f_e_mask             = 0x7f800000;
  const uint32_t f_m_mask             = 0x007fffff;
  const uint16_t f_e_bias             = 0x007f;
  const uint16_t h_e_bias             = 0x000f;
  const uint16_t h_s_mask             = 0x8000;
  const uint16_t h_e_mask             = 0x7c00;
  const uint16_t h_m_mask             = 0x03ff;
  const uint16_t h_e_pos              = 0x000a;
  const uint16_t f_e_pos              = 0x0017;
  const uint16_t f_s_pos              = 0x001f;
  const uint16_t h_s_pos              = 0x000f;
  const uint16_t h_f_denorm_sa_offset = 0x000a;
  const uint32_t h_f_bias_offset      = uint16_sub( h_e_bias, f_e_bias );
  const uint16_t h_f_s_pos_offset     = uint16_sub( f_s_pos, h_s_pos );
  const uint16_t h_f_m_pos_offset     = uint16_sub( f_e_pos, h_e_pos );
  const uint32_t h_s                  = uint32_and( h, h_s_mask );
  const uint32_t h_e                  = uint32_and( h, h_e_mask );
  const uint16_t h_m                  = uint32_and( h, h_m_mask );
  const uint32_t f_s                  = uint32_sll( h_s, h_f_s_pos_offset );
  const uint16_t h_e_half_bias        = uint16_srl( h_e, h_e_pos );
  const uint32_t f_e                  = uint16_sub( h_e_half_bias, h_f_bias_offset );
  const uint32_t f_m                  = uint32_sll( h_m, h_f_m_pos_offset );
  const uint32_t is_e_nezero          = uint32_nez( h_e );
  const uint32_t is_m_nezero          = uint32_nez( h_m );
  const uint32_t is_zero              = uint32_nor( is_e_nezero, is_m_nezero );
  const uint32_t is_denorm            = uint32_nor( is_zero, is_e_nezero );
  const uint32_t is_e_flagged         = uint32_eq( h_e, h_e_mask );
  const uint32_t is_inf               = uint32_andc( is_e_flagged, is_m_nezero );
  const uint32_t is_nan               = uint32_and( is_e_flagged,  is_m_nezero ); 
  const uint16_t h_m_nlz              = uint16_cntlz( h_m );
  const uint16_t h_denorm_sa          = uint32_add( h_m_nlz, h_f_denorm_sa_offset );
  const uint32_t h_f_denorm_m_sa      = uint32_sub( h_denorm_sa, two );
  const uint32_t h_f_denorm_m         = uint32_sll( h_m, h_f_denorm_m_sa );
  const uint32_t f_denorm_m           = uint32_and( h_f_denorm_m, f_m_mask );
  const uint32_t h_f_denorm_sa        = uint32_sub( one, h_denorm_sa );
  const uint32_t f_denorm_e           = uint32_add( h_f_denorm_sa, f_e_bias );
  const uint32_t f_denorm_e_packed    = uint32_sll( f_denorm_e, f_e_pos );
  const uint32_t f_denorm             = uint32_or3( f_s, f_denorm_e_packed, f_denorm_m );
  const uint32_t f_inf                = uint32_or( f_s,  f_e_mask );
  const uint32_t f_nan                = uint32_or3( f_s, f_e_mask, f_m );
  const uint32_t f_zero               = uint32_cp( f_s );
  const uint32_t f_e_packed           = uint32_sll( f_e, f_e_pos );
  const uint32_t packed               = uint32_or3( f_s, f_e_packed, f_m );
  const uint32_t check_zero_result    = uint32_mux( is_zero, f_zero, packed );
  const uint32_t check_denorm_result  = uint32_mux( is_denorm, f_denorm, check_zero_result );
  const uint32_t check_inf_result     = uint32_mux( is_inf, f_inf, check_denorm_result );
  const uint32_t check_nan_result     = uint32_mux( is_nan, f_nan, check_inf_result    );
  const uint32_t result               = check_nan_result;

  return (result);
}

uint16_t
half_from_float( uint32_t f )
{
  const uint16_t one                        = 0x0001;
  const uint32_t f_s_mask                   = 0x80000000;
  const uint32_t f_e_mask                   = 0x7f800000;
  const uint32_t f_m_mask                   = 0x007fffff;
  const uint32_t f_m_hidden_bit             = 0x00800000;
  const uint32_t f_m_round_bit              = 0x00001000;
  const uint32_t f_snan_mask                = 0x7fc00000;
  const uint16_t f_e_bias                   = 0x007f;
  const uint16_t h_e_bias                   = 0x000f;
  const uint16_t f_s_pos                    = 0x001f;
  const uint16_t h_s_pos                    = 0x000f;
  const uint16_t f_e_pos                    = 0x0017;
  const uint16_t h_e_pos                    = 0x000a;
  const uint16_t h_e_mask                   = 0x7c00;
  const uint16_t h_snan_mask                = 0x7e00;
  const uint16_t f_e_flagged_value          = 0x00ff;
  const uint16_t h_e_mask_value             = uint16_srl( h_e_mask, h_e_pos );
  const uint16_t f_h_s_pos_offset           = uint16_sub( f_s_pos, h_s_pos );
  const uint16_t f_h_bias_offset            = uint16_sub( f_e_bias, h_e_bias );
  const uint16_t f_h_m_pos_offset           = uint16_sub( f_e_pos, h_e_pos );
  const uint16_t h_nan_min                  = uint16_or( h_e_mask, one );
  const uint32_t f_s_masked                 = uint32_and( f, f_s_mask );
  const uint32_t f_e_masked                 = uint32_and( f, f_e_mask );
  const uint16_t h_s                        = uint32_srl( f_s_masked, f_h_s_pos_offset );
  const uint16_t f_e                        = uint32_srl( f_e_masked, f_e_pos );
  const uint32_t f_m                        = uint32_and( f, f_m_mask );
  const uint16_t f_e_half_bias              = uint16_sub( f_e, f_h_bias_offset );
  const uint32_t f_m_round_mask             = uint32_and( f_m, f_m_round_bit );
  const uint32_t f_m_round_offset           = uint32_sll( f_m_round_mask, one );
  const uint32_t f_m_rounded                = uint32_add( f_m, f_m_round_offset );
  const uint32_t f_m_denorm_sa              = uint32_sub( one, f_e_half_bias );
  const uint32_t f_m_with_hidden            = uint32_or( f_m_rounded, f_m_hidden_bit );
  const uint32_t f_m_denorm                 = uint32_srl( f_m_with_hidden, f_m_denorm_sa );
  const uint16_t h_m_denorm                 = uint32_srl( f_m_denorm, f_h_m_pos_offset );
  const uint16_t h_denorm                   = uint16_or( h_s, h_m_denorm );
  const uint16_t h_inf                      = uint16_or( h_s, h_e_mask );
  const uint16_t m_nan                      = uint32_srl( f_m, f_h_m_pos_offset );
  const uint16_t h_nan                      = uint16_or3( h_s, h_e_mask, m_nan );
  const uint16_t h_nan_notinf               = uint16_or( h_s, h_nan_min );
  const uint16_t h_e_norm_overflow_offset   = uint16_add( f_e_half_bias, one );
  const uint16_t h_e_norm_overflow          = uint16_sll( h_e_norm_overflow_offset, h_e_pos );
  const uint16_t h_norm_overflow            = uint16_or( h_s, h_e_norm_overflow );
  const uint16_t h_e_norm                   = uint16_sll( f_e_half_bias, h_e_pos );
  const uint16_t h_m_norm                   = uint32_srl( f_m_rounded, f_h_m_pos_offset );
  const uint16_t h_norm                     = uint16_or3( h_s, h_e_norm, h_m_norm );
  const uint16_t is_h_denorm                = uint16_gte( f_h_bias_offset, f_e );
  const uint16_t f_h_e_biased_flag          = uint16_sub( f_e_flagged_value, f_h_bias_offset );
  const uint16_t is_f_e_flagged             = uint16_eq( f_e_half_bias, f_h_e_biased_flag );
  const uint16_t is_f_m_zero                = uint32_eqz( f_m );
  const uint16_t is_h_nan_zero              = uint16_eqz( m_nan );
  const uint16_t is_f_inf                   = uint16_and( is_f_e_flagged, is_f_m_zero );
  const uint16_t is_f_nan_underflow         = uint16_and( is_f_e_flagged, is_h_nan_zero );
  const uint16_t is_f_nan                   = uint16_cp( is_f_e_flagged );
  const uint16_t is_e_overflow              = uint16_gt( f_e_half_bias, h_e_mask_value );
  const uint32_t f_m_rounded_overflow       = uint32_and( f_m_rounded, f_m_hidden_bit );
  const uint32_t is_m_norm_overflow         = uint32_nez( f_m_rounded_overflow );
  const uint16_t is_h_inf                   = uint16_or( is_e_overflow, is_f_inf );
  const uint32_t f_snan                     = uint32_and( f, f_snan_mask );
  const uint32_t is_f_snan                  = uint32_eq( f_snan, f_snan_mask );
  const uint16_t h_snan                     = uint16_or( h_s, h_snan_mask );
  const uint16_t check_overflow_result      = uint16_mux( is_m_norm_overflow, h_norm_overflow, h_norm );
  const uint16_t check_nan_result           = uint16_mux( is_f_nan, h_nan, check_overflow_result );
  const uint16_t check_nan_underflow_result = uint16_mux( is_f_nan_underflow, h_nan_notinf,    check_nan_result );
  const uint16_t check_inf_result           = uint16_mux( is_h_inf, h_inf, check_nan_underflow_result );
  const uint16_t check_denorm_result        = uint16_mux( is_h_denorm, h_denorm, check_inf_result );
  const uint16_t check_snan_result          = uint16_mux( is_f_snan, h_snan, check_denorm_result );
  const uint16_t result                     = uint16_cp( check_snan_result );

  return (result);
}

uint16_t
half_add( uint16_t x, uint16_t y )
{
  const uint16_t one                  = 0x0001;
  const uint16_t h_s_mask             = 0x8000;
  const uint16_t h_e_mask             = 0x7c00;
  const uint16_t h_m_mask             = 0x03ff;
  const uint16_t h_m_msb_mask         = 0x2000;
  const uint16_t h_m_hidden           = 0x0400;
  const uint16_t h_e_pos              = 0x000a;
  const uint16_t h_e_bias             = 0x000f;
  const uint16_t h_m_grs_carry        = 0x4000;
  const uint16_t h_m_grs_carry_pos    = 0x000e;
  const uint16_t h_grs_size           = 0x0003;
  const uint16_t h_snan               = 0xfe00;
  const uint16_t h_grs_round_carry    = uint16_sll( one, h_grs_size );
  const uint16_t h_grs_round_mask     = uint16_sub( h_grs_round_carry, one );

  const uint16_t x_s                  = uint16_and( x, h_s_mask );
  const uint16_t x_e                  = uint16_and( x, h_e_mask );
  const uint16_t x_m                  = uint16_and( x, h_m_mask );
  const uint16_t y_s                  = uint16_and( y, h_s_mask );
  const uint16_t y_e                  = uint16_and( y, h_e_mask );
  const uint16_t y_m                  = uint16_and( y, h_m_mask );

  const uint16_t is_y_e_larger        = uint16_gt( y_e, x_e );
  const uint16_t a_s                  = uint16_mux( is_y_e_larger, y_s, x_s );
  const uint16_t a_e                  = uint16_mux( is_y_e_larger, y_e, x_e );
  const uint16_t a_m_no_hidden_bit    = uint16_mux( is_y_e_larger, y_m, x_m );
  const uint16_t b_s                  = uint16_mux( is_y_e_larger, x_s, y_s );
  const uint16_t b_e                  = uint16_mux( is_y_e_larger, x_e, y_e );
  const uint16_t b_m_no_hidden_bit    = uint16_mux( is_y_e_larger, x_m, y_m );

  const uint16_t a_e_unbias           = uint16_sub( a_e, h_e_bias );
  const uint16_t a_e_unbias_adj       = uint16_sub( a_e_unbias, one );
  const uint16_t m_sa_unbias          = uint16_srl( a_e_unbias_adj, h_e_pos );
  const uint16_t diff_e               = uint16_sub( a_e, b_e );
  const uint16_t m_sa_default         = uint16_srl( diff_e, h_e_pos );
  const uint16_t b_e_c_mask           = uint16_eqz( b_e );
  const uint16_t a_e_nez_mask         = uint16_nez( a_e );
  const uint16_t m_sa_unbias_mask     = uint16_and( b_e_c_mask, a_e_nez_mask );
  const uint16_t m_sa                 = uint16_mux( m_sa_unbias_mask, m_sa_unbias, m_sa_default );

  const uint16_t a_e_m_no_hidden_bit  = uint16_or( a_e, a_m_no_hidden_bit );
  const uint16_t a_e_nzero            = uint16_nez( a_e );
  const uint16_t a_m_hidden_bit       = uint16_and( a_e_nzero, h_m_hidden );
  const uint16_t a_m_no_grs           = uint16_or( a_m_no_hidden_bit, a_m_hidden_bit );
  const uint16_t a_m                  = uint16_sll( a_m_no_grs, h_grs_size );

  const uint16_t b_e_m_no_hidden_bit  = uint16_or( b_e, b_m_no_hidden_bit );
  const uint16_t b_e_nzero            = uint16_nez( b_e );
  const uint16_t b_m_hidden_bit       = uint16_and( b_e_nzero, h_m_hidden );
  const uint16_t b_m_no_grs           = uint16_or( b_m_no_hidden_bit, b_m_hidden_bit );
  const uint16_t b_m_no_sticky        = uint16_sll( b_m_no_grs, h_grs_size );
  const uint16_t sh_m                 = uint16_srl( b_m_no_sticky, m_sa );
  const uint16_t sticky_overflow      = uint16_sll( one, m_sa );
  const uint16_t sticky_mask          = uint16_sub( sticky_overflow, one );
  const uint16_t sticky_collect       = uint16_and( b_m_no_sticky, sticky_mask );
  const uint16_t sticky               = uint16_nez_p( sticky_collect );
  const uint16_t b_m                  = uint16_or( sh_m, sticky );

  const uint16_t sel_sign             = uint16_xor( a_s, b_s );
  const uint16_t is_diff_sign         = uint16_nez( sel_sign );
  const uint16_t c_m_diff_ab          = uint16_sub( a_m, b_m );
  const uint16_t c_m_diff_ba          = uint16_sub( b_m, a_m );
  const uint16_t is_c_m_ab_pos        = uint16_gt( a_m, b_m );
  const uint16_t c_m_smag_diff        = uint16_mux( is_c_m_ab_pos, c_m_diff_ab, c_m_diff_ba );
  const uint16_t c_s_diff             = uint16_mux( is_c_m_ab_pos, a_s, b_s );
  const uint16_t c_s                  = uint16_mux( is_diff_sign, c_s_diff, a_s );

  const uint16_t is_a_inf             = uint16_eq( a_e_m_no_hidden_bit, h_e_mask );
  const uint16_t a_inf                = uint16_or( a_s, h_e_mask );
  const uint16_t is_b_inf             = uint16_eq( b_e_m_no_hidden_bit, h_e_mask );
  const uint16_t a_e_biased           = uint16_srl( a_e, h_e_pos );
  const uint16_t c_m_sum              = uint16_add( a_m, b_m );
  const uint16_t c_m_smag_diff_nlz    = uint16_cntlz( c_m_smag_diff );
  const uint16_t diff_norm_sa         = uint16_sub( c_m_smag_diff_nlz, one );
  const uint16_t is_diff_denorm       = uint16_lt( a_e_biased, diff_norm_sa );
  const uint16_t diff_denorm_sa       = uint16_sub( a_e_biased, one );
  const uint16_t c_m_diff_denorm      = uint16_sll( c_m_smag_diff, diff_denorm_sa );
  const uint16_t c_m_diff_norm        = uint16_sll( c_m_smag_diff, diff_norm_sa );
  const uint16_t c_e_diff_norm        = uint16_sub( a_e_biased,  diff_norm_sa );
  const uint16_t is_a_or_b_norm       = uint16_nez( a_e_biased );
  const uint16_t c_m_diff_ab_norm     = uint16_mux( is_diff_denorm, c_m_diff_denorm, c_m_diff_norm );

  const uint16_t c_e_diff_ab_norm     = uint16_andc( c_e_diff_norm, is_diff_denorm );
  const uint16_t c_m_diff_ab_denorm   = uint16_cp( c_m_smag_diff );
  const uint16_t c_e_diff_ab_denorm   = uint16_cp( a_e_biased );
  const uint16_t c_m_diff             = uint16_mux( is_a_or_b_norm, c_m_diff_ab_norm, c_m_diff_ab_denorm );
  const uint16_t c_e_diff             = uint16_mux( is_a_or_b_norm, c_e_diff_ab_norm, c_e_diff_ab_denorm );
  const uint16_t is_diff_zero         = uint16_eqz( c_m_diff );
  const uint16_t is_diff_exactly_zero = uint16_and( is_diff_sign, is_diff_zero );
  const uint16_t c_m_added            = uint16_mux( is_diff_sign, c_m_diff, c_m_sum );
  const uint16_t c_e_added            = uint16_mux( is_diff_sign, c_e_diff, a_e_biased );
  const uint16_t c_m_carry            = uint16_and( c_m_added, h_m_grs_carry );
  const uint16_t is_c_m_carry         = uint16_nez( c_m_carry );
  const uint16_t c_e_hidden_offset    = uint16_andsrl( c_m_added, h_m_grs_carry, h_m_grs_carry_pos);
  const uint16_t c_m_sub_hidden       = uint16_srl( c_m_added, one );
  const uint16_t c_m_no_hidden        = uint16_mux( is_c_m_carry, c_m_sub_hidden, c_m_added );
  const uint16_t c_e_no_hidden        = uint16_add( c_e_added, c_e_hidden_offset );
  const uint16_t is_undenorm          = uint16_eqz_p( a_e );
  const uint16_t c_m_no_hidden_msb    = uint16_and( c_m_no_hidden, h_m_msb_mask );
  const uint16_t undenorm_m_msb_odd   = uint16_nez_p( c_m_no_hidden_msb );
  const uint16_t undenorm_fix_e       = uint16_and( is_undenorm, undenorm_m_msb_odd );
  const uint16_t c_e_fixed            = uint16_add( c_e_no_hidden, undenorm_fix_e );
  const uint16_t c_m_round_amount     = uint16_and( c_m_no_hidden, h_grs_round_mask );
  const uint16_t c_m_rounded          = uint16_add( c_m_no_hidden, c_m_round_amount );
  const uint16_t c_m_round_overflow   = uint16_andsrl( c_m_rounded, h_m_grs_carry, h_m_grs_carry_pos );
  const uint16_t c_e_rounded          = uint16_add( c_e_fixed, c_m_round_overflow );
  const uint16_t c_m_no_grs           = uint16_srlm( c_m_rounded, h_grs_size,  h_m_mask );
  const uint16_t c_e                  = uint16_sll( c_e_rounded, h_e_pos );
  const uint16_t c_packed             = uint16_or3( c_s, c_e, c_m_no_grs );
  const uint16_t is_b_neg             = uint16_nez( b_s );
  const uint16_t is_both_inf          = uint16_and( is_a_inf, is_b_inf );
  const uint16_t is_invalid_inf_op    = uint16_and( is_both_inf, is_b_neg );
  const uint16_t check_inf_result     = uint16_mux( is_a_inf, a_inf, c_packed );
  const uint16_t check_zero_result    = uint16_andc( check_inf_result, is_diff_exactly_zero );
  const uint16_t check_invalid_inf_op = uint16_mux( is_invalid_inf_op, h_snan, check_zero_result );
  const uint16_t result               = uint16_cp( check_invalid_inf_op );

  return (result);
}

uint16_t
half_mul( uint16_t x, uint16_t y )
{
  uint16_t one                  = 0x0001;
  uint16_t h_s_mask             = 0x8000;
  uint16_t h_e_mask             = 0x7c00;
  uint16_t h_m_mask             = 0x03ff;
  uint16_t h_m_msb_mask         = 0x2000;
  uint16_t h_m_hidden           = 0x0400;
  uint16_t h_e_pos              = 0x000a;
  uint16_t h_e_bias             = 0x000f;
  uint16_t h_m_grs_carry        = 0x4000;
  uint16_t h_m_grs_carry_pos    = 0x000e;
  uint16_t h_grs_size           = 0x0003;
  uint16_t h_snan               = 0xfe00;
  uint16_t h_grs_round_carry    = uint16_sll( one, h_grs_size );
  uint16_t h_grs_round_mask     = uint16_sub( h_grs_round_carry, one );
  uint16_t x_s                  = uint16_and( x, h_s_mask );
  uint16_t x_e                  = uint16_and( x, h_e_mask );
  uint16_t x_m                  = uint16_and( x, h_m_mask );
  uint16_t y_s                  = uint16_and( y, h_s_mask );
  uint16_t y_e                  = uint16_and( y, h_e_mask );
  uint16_t y_m                  = uint16_and( y, h_m_mask );
  uint16_t z_s                  = uint16_xor( x_s, y_s );

  uint16_t z_e;
  uint32_t z_m;

  if ( x_e == h_e_mask )
  {
    if ( x_s || ( ( y_e == h_e_mask ) && y_m ) )
    {
      return ( x_s | h_e_mask | x_m );
    }
 
    if ( ( y_e | y_m ) == 0 )
    {
      return ( h_e_mask | 0x0001 );
    }

    return ( z_s | h_e_mask | 0 );
  }

  if ( y_e == h_e_mask )
  {
    if ( y_m )
    {
      return ( x_s | h_e_mask | x_m );
    }

    if ( ( x_e | x_m ) == 0 )
    {
      return ( h_e_mask | 0x0001 );
    }

    return ( z_s | h_e_mask | 0 );
  }

  if ( x_e == 0 )
  {
    uint16_t t_e = y_e;
    uint16_t t_m = y_m;

    y_e = x_e;
    y_m = x_m;
    x_e = t_e;
    x_m = t_m;
  }

  if ( y_e == 0 )
  {
    if ( y_m == 0 )
    {
      return ( z_s );
    }
    
    // Normalize:
    x_m = ( x_m | h_m_hidden );
    z_m = (uint32_t)x_m * (uint32_t)y_m;
    z_e = 0;

    x_e = x_e >> h_e_pos;
    int unbias_e = (h_e_bias - x_e);

    // Only want h.10
    // Round result
    z_m += z_m & h_m_mask;
    z_m >>= 10;

    // Shift radix point
    uint32_t sa = unbias_e;
    z_m >>= sa;
    z_m &= 0x000003ff;

    return ( y_s | 0 | z_m  );
  }

  x_e = x_e >> h_e_pos;
  y_e = y_e >> h_e_pos;

  x_m = ( x_m | h_m_hidden );
  y_m = ( y_m | h_m_hidden );

  z_m = (uint32_t)x_m * (uint32_t)y_m;

  if ( ( x_e + y_e - h_e_bias ) < 0 )
  {
    z_e = 0;

    uint16_t underflow = -(x_e + y_e - h_e_bias);

    z_m >>= (underflow << 1);
  }
  else
  {
    z_e = x_e + y_e - h_e_bias;
  }

  // if exponent overflow
  if (z_e & 0x00000020)
  {
    return ( z_s | h_e_mask | 0x0000 ); // inf
  }

  // The mantissa is 10 bits so, the result is now
  // hh.(10x10)

  // Shift radix point right one unit
  z_e += 1;
  z_m >>= 1;

  // The result is now
  // h.(10x10)
  if ( ( z_m & 0x00100000 ) == 0 )
  {  
    uint16_t nlz = uint16_cntlz( z_m >> 5 );

    if ( nlz >= z_e )
    {
      z_m <<= (nlz >> 1);
      z_e = 0;
    }
    else
    {
      z_e -= nlz;
      z_m <<= nlz;
    }
  }

  // Only want h.10
  // Round result
  z_m += z_m & h_m_mask;

  // if exponent overflow
  if (z_e & 0x00000020)
  {
      return ( z_s | h_e_mask | 0x0000 ); // inf
  }
  z_m >>= 10;
  z_m &= 0x000003ff;
  z_e &= 0x0000001f;

  z_e = z_e << h_e_pos;

  return ( z_s | z_e | z_m );
}

uint16_t
half_div( uint16_t x, uint16_t y )
{
  uint16_t one                  = 0x0001;
  uint16_t h_s_pos              = 0x000f;
  uint16_t h_s_mask             = 0x8000;
  uint16_t h_e_mask             = 0x7c00;
  uint16_t h_m_mask             = 0x03ff;
  uint16_t h_m_msb_mask         = 0x2000;
  uint16_t h_m_hidden           = 0x0400;
  uint16_t h_e_pos              = 0x000a;
  uint16_t h_e_bias             = 0x000f;
  uint16_t h_m_grs_carry        = 0x4000;
  uint16_t h_m_grs_carry_pos    = 0x000e;
  uint16_t h_grs_size           = 0x0003;
  uint16_t h_snan               = 0xfe00;
  uint16_t h_grs_round_carry    = uint16_sll( one, h_grs_size );
  uint16_t h_grs_round_mask     = uint16_sub( h_grs_round_carry, one );

  uint16_t x_s                  = uint16_and( x, h_s_mask ) >> h_s_pos;
  uint16_t x_e                  = uint16_and( x, h_e_mask ) >> h_e_pos;
  uint16_t x_f                  = uint16_and( x, h_m_mask );

  uint16_t y_s                  = uint16_and( y, h_s_mask ) >> h_s_pos;
  uint16_t y_e                  = uint16_and( y, h_e_mask ) >> h_e_pos;
  uint16_t y_f                  = uint16_and( y, h_m_mask );

  uint16_t z_s                  = uint16_xor( x_s, y_s );

  uint16_t z_e = x_e-y_e+(0x0d);

  x_f = ( x_f | h_m_hidden )<<4;
  y_f = ( y_f | h_m_hidden )<<5;

  if ( y_f <= ( x_f + x_f ) ) 
  {
    x_f >>= 1;
    ++z_e;
  }

  uint32_t z_f = ( ( (uint32_t) x_f )<<16 ) / y_f;

  if ( ( z_f & 0x1F ) == 0 ) 
  {
    z_f |= ( (uint64_t) y_f * z_f != ( (uint32_t) x_f )<<16);
  }

  uint16_t r = (z_f >> 4) | (z_e << h_e_pos) | (z_s << h_s_pos);

  return (r);

  // return roundAndPackFloat32( c_s, c_e, c_f );


#if 0
    zExp = aExp - bExp + 0x7D;
    aSig = ( aSig | 0x00800000 )<<7;
    bSig = ( bSig | 0x00800000 )<<8;
    if ( bSig <= ( aSig + aSig ) ) {
        aSig >>= 1;
        ++zExp;
    }
    zSig = ( ( (bits64) aSig )<<32 ) / bSig;
    if ( ( zSig & 0x3F ) == 0 ) {
        zSig |= ( (bits64) bSig * zSig != ( (bits64) aSig )<<32 );
    }
    return roundAndPackFloat32( zSign, zExp, zSig );
#endif
}
