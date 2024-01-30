#include "attriter.hpp"

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
