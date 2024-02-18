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

nix::Attr *AttrIterable::find_by_key(std::string_view needle)
{
	for (auto &attr : *this->attrs) {
		auto name = static_cast<std::string_view>(this->symbols[attr.name]);
		if (name == needle) {
			return &attr;
		}
	}

	return this->attrs->end();
}

nix::Attr *AttrIterable::find_by_nested_key(nix::EvalState &state, std::span<std::string_view> needleSpec)
{
	if (needleSpec.empty()) {
		eprintln("needleSpec.empty()");
		return this->attrs->end();
	}

	decltype(needleSpec)::iterator current = needleSpec.begin();
	auto *currentFound = this->find_by_key(*current);

	std::span<std::string_view> rest = needleSpec.subspan(1);
	if (rest.empty()) {
		return currentFound;
	}

	if (currentFound->value == nullptr) {
		return this->attrs->end();
	}
	state.forceValue(*currentFound->value, nix::noPos);
	if (currentFound->value->type() != nix::nAttrs) {
		return this->attrs->end();
	}

	auto *nested = AttrIterable{currentFound->value->attrs, this->symbols}.find_by_nested_key(state, rest);
	if (nested == currentFound->value->attrs->end()) {
		return this->attrs->end();
	}
	return nested;
}

AttrIterable &AttrIterable::operator=(AttrIterable &&rhs)
{
	eprintln("In copy assignment operator");
	this->attrs = rhs.attrs;
	this->symbols = rhs.symbols;
	eprintln("Returning from copy assignment operator");
	return *this;
}
