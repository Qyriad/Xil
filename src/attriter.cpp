#include "attriter.hpp"

#include <span>
#include <value.hh>

#include "xil.hpp"

AttrKeyValueIter::reference AttrKeyValueIter::operator*() const
{
	std::string const name = (*this->symbols)[this->current->name];
	// We need a persistent reference, so keep this string alive in this class's vector.
	this->referredNames.push_back(name);
	nix::Value &value = *this->current->value;
	return std::tie(this->referredNames.back(), value);
}

AttrKeyValueIter::pointer AttrKeyValueIter::operator->()
{
	std::string const name = (*this->symbols)[this->current->name];
	// We need a persistent reference, so keep this string alive in this class's vector.
	this->referredNames.push_back(name);
	nix::Value *value = this->current->value;
	return std::tie(this->referredNames.back(), value);
}

AttrKeyValueIter &AttrKeyValueIter::operator++()
{
	this->current++;
	return *this;
}

AttrKeyValueIter AttrKeyValueIter::operator++(int)
{
	AttrKeyValueIter prev = *this;
	++(*this);
	return prev;
}

AttrKeyValueIter & AttrKeyValueIter::operator=(AttrKeyValueIter const &rhs) noexcept {
	this->current = rhs.current;
	this->symbols = rhs.symbols;
	return *this;
}

AttrKeyValueIter AttrIterable::begin() const
{
	return AttrKeyValueIter(this->attrs->begin(), &this->symbols);
}

AttrKeyValueIter AttrIterable::end() const
{
	return AttrKeyValueIter(this->attrs->end(), &this->symbols);
}

size_t AttrIterable::size() const
{
	return this->attrs->size();
}

bool AttrIterable::empty() const {
	return this->size() == 0;
}

bool operator==(AttrKeyValueIter const &lhs, AttrKeyValueIter const &rhs) noexcept
{
	return lhs.current == rhs.current;
}

bool operator!=(AttrKeyValueIter const &lhs, AttrKeyValueIter const &rhs) noexcept
{
	return lhs.current != rhs.current;
}

OptionalRef<nix::Attr> AttrIterable::find_by_key(std::string_view needle)
{
	for (auto &attr : *this->attrs) {
		auto name = static_cast<std::string_view>(this->symbols[attr.name]);
		if (name == needle) {
			return std::make_optional(std::ref(attr));
		}
	}

	return std::nullopt;
}

OptionalRef<nix::Attr> AttrIterable::find_by_nested_key(nix::EvalState &state, std::span<std::string_view> needleSpec)
{
	if (needleSpec.empty()) {
		return std::nullopt;
	}

	decltype(needleSpec)::iterator current = needleSpec.begin();
	OptionalRef<nix::Attr> currentFound = this->find_by_key(*current);
	if (!currentFound.has_value()) {
		return std::nullopt;
	}

	std::span<std::string_view> rest = needleSpec.subspan(1);

	// If we don't have any more attrs to recurse into, then this is The One.
	if (rest.empty()) {
		return currentFound;
	}

	assert(currentFound->get().value != nullptr);
	state.forceValue(*currentFound->get().value, nix::noPos);
	if (currentFound->get().value->type() != nix::nAttrs) {
		return std::nullopt;
	}

	return AttrIterable{currentFound->get().value->attrs, this->symbols}.find_by_nested_key(state, rest);
}

AttrIterable &AttrIterable::operator=(AttrIterable &&rhs)
{
	eprintln("In copy assignment operator");
	this->attrs = rhs.attrs;
	this->symbols = rhs.symbols;
	eprintln("Returning from copy assignment operator");
	return *this;
}
