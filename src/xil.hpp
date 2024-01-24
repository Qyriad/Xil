// Boilerplate header!

#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

// Nix headers.
#include <config.h>
#include <shared.hh>
#include <eval.hh>

#include <boost/core/demangle.hpp>
#include <fmt/core.h>

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

	explicit Printer(std::shared_ptr<nix::EvalState> state, bool safe, bool shortErrors) :
		state(state), safe(safe), shortErrors(shortErrors)
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
	void printRepeatedAttrs(nix::Bindings *attrs, std::ostream &out);

	/** Attempt to force a value, returning a string for the kind of error if any. */
	OptString safeForce(nix::Value &value, nix::PosIdx position = nix::noPos);
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
