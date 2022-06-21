#pragma once



#define EXPAND(x) x

#define NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,N,...) N
#define NARGS(...) EXPAND(NARGS_SEQ(__VA_ARGS__, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))

#define CHOOSE_MACRO(macro, ...) CONCATENATE(macro, NARGS(__VA_ARGS__))

#define CONCATENATE__(a, b) a##b
#define CONCATENATE_(a, b) CONCATENATE__(a, b)
#define CONCATENATE(a, b) CONCATENATE_(a, b)



#define FOR_EACH_1( macro_1, macro_n, constant, value, ...)	EXPAND(macro_1(constant, value))
#define FOR_EACH_2( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_1 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_3( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_2 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_4( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_3 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_5( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_4 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_6( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_5 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_7( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_6 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_8( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_7 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_9( macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_8 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_10(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_9 (macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_11(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_10(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_12(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_11(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_13(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_12(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_14(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_13(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_15(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_14(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_16(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_15(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_17(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_16(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_18(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_17(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_19(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_18(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_20(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_19(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_21(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_20(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_22(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_21(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_23(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_22(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_24(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_23(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_25(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_24(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_26(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_25(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_27(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_26(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_28(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_27(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_29(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_28(macro_1, macro_n, constant, __VA_ARGS__)))
#define FOR_EACH_30(macro_1, macro_n, constant, value, ...)	EXPAND(macro_n(constant, value)	EXPAND(FOR_EACH_29(macro_1, macro_n, constant, __VA_ARGS__)))


#define MACRO_FOR_EACH(macro_1, macro_n, constant, ...) EXPAND(CHOOSE_MACRO(FOR_EACH_, __VA_ARGS__)(macro_1, macro_n, constant, __VA_ARGS__))


