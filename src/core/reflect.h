#pragma once

#include "preprocessor_for_each.h"

#include <type_traits>
#include <ostream>

template <typename T> struct type_descriptor {};

struct member_list_base {};

template <auto... member_pointers>
struct member_list : member_list_base
{
	static inline const uint32 numMembers = sizeof...(member_pointers);

	template <typename T, typename F>
	constexpr static auto applyImpl(F f, T& v, const char* memberNames[numMembers])
	{
		uint32 i = 0;
		([&]
		{
			f(memberNames[i], v.*member_pointers);
			++i;
		} (), ...);
	}
};

#define EXTRACT_MEMBER(member, ...) member			// Extract the member from the tuple.
#define EXTRACT_MEMBER_NAME_1(member) #member		// If no name is supplied, extract and stringify the member name.
#define EXTRACT_MEMBER_NAME_2(member, name) name	// Else, extract the supplied name.

// member_tuple is either (member) or (member, custom name).
// EXTRACT_MEMBER member_tuple expands to EXTRACT_MEMBER (member, name). The parentheses come from the tuple itself (ugly af).
#define MEMBER_POINTER_1(struct_name, member_tuple) &struct_name::EXTRACT_MEMBER member_tuple // Used for one-element reflection (or last loop iteration).
#define MEMBER_POINTER_N(struct_name, member_tuple) MEMBER_POINTER_1(struct_name, member_tuple),

#define MEMBER_NAME_(...) EXPAND(CHOOSE_MACRO(EXTRACT_MEMBER_NAME_, __VA_ARGS__)(__VA_ARGS__))	// Choose EXTRACT_MEMBER_NAME 1 or 2 depending on whether a name is supplied.
#define MEMBER_NAME(struct_name, member_tuple) MEMBER_NAME_ member_tuple, // Same trick with the parenthesis as above.

#define REFLECT_STRUCT(name, ...)																									\
	template<> struct type_descriptor<name> : member_list<MACRO_FOR_EACH(MEMBER_POINTER_1, MEMBER_POINTER_N, name, __VA_ARGS__)>	\
	{																																\
		static inline const char* memberNames[] = { MACRO_FOR_EACH(MEMBER_NAME, MEMBER_NAME, , __VA_ARGS__) };						\
		static inline const char* structName = #name;																				\
		template <typename T, typename F> constexpr static auto apply(F f, T& v) { return applyImpl(f, v, memberNames); }			\
    };


template <typename T> struct is_reflected : std::is_base_of<member_list_base, type_descriptor<T>> {};
template <typename T> inline constexpr bool is_reflected_v = is_reflected<T>::value;


template <typename T, typename = std::enable_if_t<is_reflected_v<T>>>
static std::ostream& operator<<(std::ostream& o, const T& v)
{
	o << type_descriptor<T>::structName << " = {\n";
	type_descriptor<T>::apply(
		[&o](const char* name, const auto& member)
		{
			o << "   " << name << " = " << member << ";\n";
		},
		v
	);
	o << "}\n";
	return o;
}


// USAGE:

#if 0

struct test_reflect
{
	float v = 1.f;
	int a = 1;
	std::string test = "asd";
	vec3 lightDirection;
};
REFLECT_STRUCT(test_reflect,
	(v), // Reflect member v.
	(a, "A"), // You can optionally supply a custom name.
	(test),
	(lightDirection, "Light direction")
);




// Printing the reflected struct.
test_reflect test = { 2.f, 6123 };
std::cout << test << '\n';



#endif


