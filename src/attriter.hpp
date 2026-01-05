// Iterator types for Nix attrsets.

#include <iterator>
#include <span>

// Nix headers.
#include <lix/config.h>
#include <lix/libmain/shared.hh>
#include <lix/libexpr/attr-set.hh>
#include <lix/libexpr/value.hh>

#include "std/string.hpp"
#include "std/string_view.hpp"
#include "std/vector.hpp"
#include "xil.hpp"

struct AttrKeyValueIter
{
	using iterator_category = std::forward_iterator_tag;
	using difference_type = ptrdiff_t;
	using value_type = std::tuple<StdString const, nix::Value> const;
	using pointer = std::tuple<StdString const, nix::Value *> const;
	using reference = std::tuple<StdString const &, nix::Value &> const;

	// Cursed.
	StdVec<StdString> mutable referredNames;

	// Pointer means we have to define operator= ourself.
	nix::Attr *current;
	nix::SymbolTable *symbols;

    AttrKeyValueIter() = default;

	AttrKeyValueIter(nix::Attr *current, nix::SymbolTable *symbols) :
		current(current),
		symbols(symbols)
	{ }

	// Copy constructor. Required for std::input_iterator.
	AttrKeyValueIter(AttrKeyValueIter const &rhs) :
		current(rhs.current),
		symbols(rhs.symbols)
	{ }

	// Copy assignment operator. Required for std::input_iterator.
	AttrKeyValueIter &operator=(AttrKeyValueIter const &rhs) noexcept;

	reference operator*() const;

	pointer operator->();

	// Prefix increment.
	AttrKeyValueIter &operator++();

	// Postfix increment, apparently.
	AttrKeyValueIter operator++(int);

	friend bool operator==(AttrKeyValueIter const &lhs, AttrKeyValueIter const &rhs) noexcept;

	friend bool operator!=(AttrKeyValueIter const &lhs, AttrKeyValueIter const &rhs) noexcept;
};

// It cannot be a true std::forward_iterator as those require default constructors.
static_assert(std::forward_iterator<AttrKeyValueIter>);

// The thing you construct for a range-based foor loop over `AttrKeyValueIter`s.
struct AttrIterable
{
	using Iterator = AttrKeyValueIter;

	nix::Bindings *attrs;
	nix::SymbolTable &symbols;

	AttrIterable(nix::Bindings *attrs, nix::SymbolTable &symbols) :
		attrs(attrs), symbols(symbols)
	{
		assert(attrs != nullptr);
	}

	// Copy assignment operator, since the default is implicitly deleted.
	AttrIterable &operator=(AttrIterable &&rhs);

	AttrKeyValueIter begin() const;

	AttrKeyValueIter end() const;

	size_t size() const;

	bool empty() const;

	OptionalRef<nix::Attr> find_by_key(StdStr const needle);
	OptionalRef<nix::Attr> find_by_nested_key(nix::EvalState &state, std::span<StdStr> needleSpec);
};
