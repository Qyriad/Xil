// Iterator types for Nix attrsets.

#include <iterator>

// Nix headers.
#include <config.h>
#include <shared.hh>
#include <attr-set.hh>
#include <value.hh>

struct AttrKeyValueIter
{
	using iterator_category = std::input_iterator_tag;
	using difference_type = ptrdiff_t;
	using value_type = std::tuple<std::string const, nix::Value> const;
	using pointer = std::tuple<std::string const, nix::Value *> const;
	using reference = std::tuple<std::string const &, nix::Value &> const;

	// Pointer means we have to define operator= ourself.
	nix::Attr *current;
	nix::SymbolTable &symbols;

	AttrKeyValueIter(nix::Attr *current, nix::SymbolTable &symbols) :
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
static_assert(std::input_iterator<AttrKeyValueIter>);

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

	AttrKeyValueIter begin() const;

	AttrKeyValueIter end() const;

	size_t size() const noexcept;

	bool empty() const noexcept;
};
