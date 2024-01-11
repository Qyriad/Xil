// Boilerplate header!

#include <functional>
#include <optional>

#include <fmt/core.h>

// Exactly like fmt::print, but prints to stderr.
template <typename ... T>
void eprint(fmt::format_string<T ...> fmt, T && ...args)
{
	fmt::vprint(stderr, fmt, fmt::make_format_args(args ...));
}

// Exactly like fmt::println, but prints to stderr.
template <typename ... T>
void eprintln(fmt::format_string<T ...> fmt, T && ...args)
{
	fmt::vprint(stderr, fmt, fmt::make_format_args(args ...));
	eprint("\n");
}

template <typename T>
std::optional<T *> make_optional_ptr(T *v)
{
	if (v == nullptr) {
		return std::nullopt;
	}
	return std::make_optional(v);
}

// std::optional with a std::reference_wrapper
template <typename T>
using OptionalRef = std::optional<std::reference_wrapper<T>>;

// A combination of std::make_optional() and std::ref()
template <typename T>
OptionalRef<T> make_optional_ref(T v)
{
	return std::make_optional(std::ref(v));
}

// Same as above, but if v == nullptr returns std::nullopt.
template <typename T>
OptionalRef<T> make_optional_ref(T *v)
{
	if (v == nullptr) {
		return std::nullopt;
	}

	return std::make_optional(std::ref(*v));
}
