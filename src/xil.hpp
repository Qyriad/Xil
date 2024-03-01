// Boilerplate header!

#pragma once

#include <concepts>
#include <functional>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits>

// Nix headers.
#include <nix/config.h>
#include <nix/eval.hh>
#include <nix/shared.hh>

#include <argparse/argparse.hpp>
#include <boost/core/demangle.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>

#include "settings.hpp"

#define RANGE(a) a.begin(), a.end()

template <typename T>
concept Iterable = requires (T v) {
	v.begin();
	v.end();
};

template <typename T, typename V, typename R>
concept Callable = requires(T v, V foo) {
	{ v(foo) -> R };
};

//extern std::ofstream LOGFILE;
extern std::shared_ptr<std::fstream> LOGFILE;

//template <
//	//typenamet RangeT,
//	//typename CallableT,
//	Callable<bool, int> CallableT,
//	//typename IteratorT = typename RangeT::iterator
//	typename CallbackT
//>
//auto mkRangeArgs(
//	auto range,
//	CallableT &&callable,
//	//std::function<typename RangeT::iterator(typename RangeT::iterator, typename RangeT::iterator, CallbackT)> callable,
//	CallbackT cb
//)
//{
//	auto &&begin = std::begin(range);
//	auto &&end = std::end(range);
//
//	//static_assert(std::is_same_v<decltype(range.begin()), std::vector<int>::iterator>);
//	//static_assert(std::is_same_v<decltype(cb), std::function<bool(int)>>);
//
//	//using ArgsT = std::tuple<decltype(begin), decltype(end), CallbackT>;
//	//ArgsT args{begin, end, cb};
//	//return std::apply(callable, args);
//
//	using IteratorT = decltype(begin);
//
//	std::function<IteratorT(IteratorT, IteratorT, CallbackT)> coerced(callable);
//	return callable(begin, end, cb);
//}

template <typename T>
struct Ext
{
	T &&self;

	Ext(T &&self) : self(self) { }
};

template <typename T>
struct Ext<std::vector<T>>
{
	//using Self = std::vector<T>;
	std::vector<T> &&self;

	Ext(std::vector<T> &&self) : self(self) { }

	template <typename FromRangeT, typename FromIterT = FromRangeT::iterator>
	static std::vector<T> from(FromRangeT &&range)
	{
		FromIterT begin = range.begin();
		FromIterT end = range.end();
		return std::vector<T>{begin, end};
	}
};

namespace nix
{
	std::string_view format_as(nix::ValueType const type) noexcept;
}

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

template <typename ... T>
void xlog(fmt::format_string<T ...> fmt, T && ...args)
{
	fmt::print(*LOGFILE, "xil: ");
	fmt::vprint(*LOGFILE, fmt, fmt::make_format_args(args ...));
	LOGFILE->flush();
}

template <typename ... T>
void xlogln(fmt::format_string<T ...> fmt, T && ...args)
{
	fmt::print(*LOGFILE, "xil: ");
	fmt::vprint(*LOGFILE, fmt, fmt::make_format_args(args ...));
	fmt::print(*LOGFILE, "\n");
	LOGFILE->flush();
}

template <>
struct fmt::formatter<argparse::ArgumentParser> : fmt::ostream_formatter { };

template <typename T>
concept ArithmeticT = std::is_arithmetic<T>::value;

/** Naïvely pluralize an english noun if the specified number is not 1. Otherwise return the as-is. */
template <ArithmeticT T>
std::string maybePluralize(T numberOfThings, std::string_view baseNoun, std::string_view suffix = "s")
{
	if (numberOfThings == 1) {
		return std::string(baseNoun);
	}
	return fmt::format("{}{}", baseNoun, suffix);
}

using OptString = std::optional<std::string>;
using OptStringView = std::optional<std::string_view>;

#define TYPENAME(expr) (boost::core::demangle(typeid(expr).name()))

size_t const EXPR_VAR = typeid(nix::ExprVar).hash_code();
size_t const EXPR_SELECT = typeid(nix::ExprSelect).hash_code();
size_t const EXPR_OP_HAS_ATTR = typeid(nix::ExprOpHasAttr).hash_code();
size_t const EXPR_ATTRS = typeid(nix::ExprAttrs).hash_code();
size_t const EXPR_LIST = typeid(nix::ExprList).hash_code();
size_t const EXPR_LAMBDA = typeid(nix::ExprLambda).hash_code();
size_t const EXPR_CALL = typeid(nix::ExprCall).hash_code();
size_t const EXPR_LET = typeid(nix::ExprLet).hash_code();
size_t const EXPR_STRING = typeid(nix::ExprString).hash_code();
// FIXME: more

#define HANDLER(kind_constant, kind_type, lambda) { \
	kind_constant, [&](nix::Expr &rawExpr) -> std::optional<std::string> { \
		auto &expr = dynamic_cast<kind_type>(rawExpr); \
		return lambda(expr); \
	} \
}

struct ExprT
{
	using VariantT = std::variant<
		nix::ExprVar *,
		nix::ExprCall *,
		nix::ExprString *,
		nix::ExprLambda *,
		nix::ExprList *,
		nix::ExprSelect *
	>;

	static VariantT from(nix::Expr *value)
	{
		using namespace nix;
		std::unordered_map<size_t, VariantT> handlers = {
			{ EXPR_VAR, dynamic_cast<ExprVar *>(value) },
			{ EXPR_CALL, dynamic_cast<ExprCall *>(value) },
			{ EXPR_STRING, dynamic_cast<ExprString *>(value) },
			{ EXPR_LAMBDA, dynamic_cast<ExprLambda *>(value) },
			{ EXPR_LIST, dynamic_cast<ExprList *>(value) },
			{ EXPR_SELECT, dynamic_cast<ExprSelect *>(value) },
		};

		try {
			return handlers.at(typeid(*value).hash_code());
		} catch ([[maybe_unused]] std::out_of_range &ex) {
			eprintln("No handler for {}", TYPENAME(*value));
			throw;
		}
	}
};

template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/** Version of nix::ExprState::eval that returns its nix::Value instead of taking an out parameter.
 * Can be called on lvalue or rvalue references.
 */
template <typename ExprT> requires(std::convertible_to<ExprT &&, nix::Expr>)
nix::Value nixEval(nix::EvalState &state, ExprT &&expr)
{
	nix::Value out;
	state.eval(&expr, out);
	return out;
}

/** Version of nix::ExprState::callFunction that returns its nix::Value instead of taking an outparameter.
 * Can be called on lvalue or rvalue references.
 */
template <typename ValueT> requires(std::convertible_to<ValueT &&, nix::Value>)
nix::Value nixCallFunction(nix::EvalState &state, ValueT &&fun, ValueT &&callOn, nix::PosIdx pos = nix::noPos)
{
	nix::Value out;
	state.callFunction(fun, callOn, out, pos);
	return out;
}

/** Constructs a std::vector<T> from a std::ranges::range of the same type. */
template <
	typename RangeT
>
requires requires (RangeT range) {
	range.begin();
	range.end();
}
auto iterToVec(RangeT &&range) -> std::vector<typename decltype(range.begin())::value_type>
{
	// Calls vector's "from range" constructor (containers.sequences.vectors.cons/9).
	return std::vector{range.begin(), range.end()};
}

template <typename IterT>
struct ToVec
{
	IterT begin;
	IterT end;

	using value_type = std::iter_value_t<IterT>;

	template <typename RangeT>
	ToVec(RangeT &&range) : begin(range.begin()), end(range.end()) { }

	template <typename ValueT>
	operator std::vector<ValueT>() const
	{
		return std::vector{this->begin, this->end()};
	}

	operator std::vector<value_type>() const
	{
		return std::vector{this->begin(), this->end()};
	}

};

template <typename RangeT>
//ToVec(RangeT &&range) -> ToVec<typename std::iterator_traits<typename RangeT::iterator>::value_type>;
ToVec(RangeT &&range) -> ToVec<typename RangeT::iterator>;

// Has a fmt::format_as, and an operator<<.
struct Indent
{
	uint32_t level;
	friend std::ostream &operator<<(std::ostream &out, Indent const &self);
};

struct Printer
{
	std::shared_ptr<nix::EvalState> state;

	std::set<nix::Bindings *> seen;

	/** Used by function printing to be Smart™. */
	OptString currentAttrName = std::nullopt;

	/** Catch errors during evaluation instead of aborting. */
	bool safe;

	/** Print errors without their traces in attrsets and lists. */
	bool shortErrors;

	/** Print derivations as their drvPaths. */
	bool shortDerivations;

	explicit Printer(std::shared_ptr<nix::EvalState> state, bool safe, bool shortErrors, bool shortDerivations) :
		state(state), safe(safe), shortErrors(shortErrors), shortDerivations(shortDerivations)
	{ }

	// Gets a std::string for a nix::Symbol, checking if the Symbol is invalid first.
	OptString symbolStr(nix::Symbol &symbol);

	// Gets a std::string_view for a nix::Symbol.
	// TODO: Is this...safe?
	OptStringView symbolStrView(nix::Symbol &symbol);

	// Gets a nix::SymbolStr for a nix::Symbol, checking if the Symbol is invalid first.
	std::optional<nix::SymbolStr> symbol(nix::Symbol &&symbol);

	nix::Value *getAttrValue(nix::Bindings *attrs, std::string_view key);
	nix::Value *getAttrValue(nix::Bindings *attrs, nix::Symbol key);

	// Gets the name associated with an expression, if applicable.
	// Currently only applies to `ExprVar`s and `ExprLambda`s.
	OptString exprName(nix::Expr *expr);

	// Gets the name associated with a value, if applicable.
	// Currently only applies to lambdas, primops, and applications of primops.
	OptString valueName(nix::Value &expr);

	void printValue(nix::Value &value, std::ostream &out, uint32_t indentLevel, uint32_t depth);

	void printAttrs(nix::Bindings *attrs, std::ostream &out, uint32_t indentLevel, uint32_t depth);
	void printRepeatedAttrs(nix::Bindings *attrs, std::ostream &out);

	/** Attempt to force a value, returning a string for the kind of error if any. */
	OptString safeForce(nix::Value &value, nix::PosIdx position = nix::noPos);
};

// Represents the different "modes" that installables can be referenced in.
// For example `nix build` uses packages.${system} and legacyPackages.${system},
// but `nix develop` uses devShells.${system}.
// Without a mode, we don't know what attributes an installable fragment is meant
// to access.
struct InstallableMode
{
	// The wrapper struct is here so we can have things like Methods™.
	enum class Value
	{
		NONE,
		// packages.${system}
		// legacyPackages.${system}
		BUILD,
		// devShels.${system}
		DEVSHELL,
		// apps.${system}
		// packages.${system}
		// legacyPackages.${system}
		// Used for `nix run`.
		APP,
		CHECKS,
		ALL,
	};

	using enum Value;

	Value inner;

	// We specifically want this to be implicit so things like
	// `InstallableMode mode = InstallableMode::BUILD` work.
	constexpr InstallableMode(Value v) : inner(v) { }

	// This will make switch case "just work".
	constexpr operator Value() const noexcept
	{
		return this->inner;
	}

	// And this is to remove the transitive boolean coercion added by the
	// above conversion operator.
	explicit operator bool() = delete;

	std::list<std::string> defaultFlakeAttrPaths(std::string_view const system) const;
	std::list<std::string> defaultFlakeAttrPrefixes(std::string_view const system) const;
};

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
