#include "attriter.hpp"

#include <lix/libexpr/value.hh>

#include "std/span.hpp"

#include "xil.hpp"

AttrKeyValueIter::reference AttrKeyValueIter::operator*(this Self const &self)
{
	StdString const name = (*self.symbols)[self.current->name];
	// We need a persistent reference, so keep this string alive in this class's vector.
	self.referredNames.push_back(name);
	nix::Value &value = *self.current->value;
	return std::tie(self.referredNames.back(), value);
}

AttrKeyValueIter::pointer AttrKeyValueIter::operator->(this Self &self)
{
	StdString const name = (*self.symbols)[self.current->name];
	// We need a persistent reference, so keep this string alive in this class's vector.
	self.referredNames.push_back(name);
	nix::Value *value = self.current->value;
	return std::tie(self.referredNames.back(), value);
}

AttrKeyValueIter &AttrKeyValueIter::operator++(this Self &self)
{
	self.current++;
	return self;
}

AttrKeyValueIter AttrKeyValueIter::operator++(this Self &self, int)
{
	AttrKeyValueIter prev = self;
	++self;
	return prev;
}

AttrKeyValueIter & AttrKeyValueIter::operator=(AttrKeyValueIter const &rhs) noexcept {
	if (this == &rhs) {
		return *this;
	}
	this->current = rhs.current;
	this->symbols = rhs.symbols;
	return *this;
}

AttrKeyValueIter AttrIterable::begin() const
{
	return {this->attrs->begin(), &this->symbols};
}

AttrKeyValueIter AttrIterable::end() const
{
	return {this->attrs->end(), &this->symbols};
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

OptionalRef<nix::Attr> AttrIterable::find_by_key(StdStr needle)
{
	for (auto &attr : *this->attrs) {
		auto name = static_cast<StdStr>(this->symbols[attr.name]);
		if (name == needle) {
			return std::make_optional(std::ref(attr));
		}
	}

	return std::nullopt;
}

OptionalRef<nix::Attr> AttrIterable::find_by_nested_key(nix::EvalState &state, StdSpan<StdStr> needleSpec)
{
	if (needleSpec.empty()) {
		return std::nullopt;
	}

	decltype(needleSpec)::iterator current = needleSpec.begin();
	OptionalRef<nix::Attr> currentFound = this->find_by_key(*current);
	if (!currentFound.has_value()) {
		return std::nullopt;
	}

	StdSpan<StdStr> rest = needleSpec.subspan(1);

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
