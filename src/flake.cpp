#include "flake.hpp"

#include <nix/attr-path.hh>
#include <nix/flake/lockfile.hh>

#include <fmt/core.h>

#include "xil.hpp"
#include "attriter.hpp"
#include "std/optional.hpp"
#include "std/vector.hpp"

using nix::flake::LockedNode, nix::flake::Node;

const nix::flake::LockFlags LazyFlake::DEFAULT_LOCK_FLAGS{
	.recreateLockFile = false,
	.updateLockFile = false,
	.writeLockFile = false,
	.useRegistries = std::nullopt,
	.applyNixConfig = false,
	.allowUnlocked = true,
	.commitLockFile = false,
	.referenceLockFilePath = std::nullopt,
	.outputLockFilePath = std::nullopt,
	.inputOverrides = {},
	.inputUpdates = {},
};

auto LazyFlake::evalCache() -> EvalCache&
{
	if (this->_evalCache.has_value()) {
		return *this->_evalCache.value();
	}

	this->_evalCache = nix::openEvalCache(*this->state, this->lockedShared());

	return *this->_evalCache.value();
}

auto LazyFlake::resolved() -> FlakeRef&
{
	if (this->_resolved.has_value()) {
		return this->_resolved.value();
	}

	static constexpr StdStr resolveLabel{"Flake Registry"};

	// If we don't have an InstallableFlake but we do have a LockedFlake or locked FlakeRef,
	// then we can just use that.
	if (!this->_inst.has_value() && this->_lockedFull.has_value()) {
		if (this->_lockedRef.has_value() || this->_lockedFull.has_value()) {
			return this->lockedRef();
		}
	}

	this->_resolved = this->logger.withFetchLabel(resolveLabel, [&]() -> nix::FlakeRef {
		return this->_inst->flakeRef.resolve(this->state->store);
	});
	return this->_resolved.value();
}

auto LazyFlake::fetched() -> nix::StorePath
{
	if (this->_storePath.has_value()) {
		return this->_storePath.value();
	}

	auto [path, _in] = this->logger.withFetchLabel(this->label, [&]() {
		auto input = this->resolved().input;
		return input.scheme->fetch(this->state->store, input);
	});

	this->_storePath = path;

	return path;
}

auto LazyFlake::fullStorePath() -> StdString
{
	return this->state->store->printStorePath(this->fetched());
}

auto LazyFlake::locked(LockFlags lockFlags) -> LockedFlake&
{
	return *this->lockedShared(lockFlags);
}

auto LazyFlake::lockedShared(LockFlags lockFlags) -> std::shared_ptr<LockedFlake>
{
	if (this->_lockedFull.has_value()) {
		return this->_lockedFull.value();
	}

	// Fetch before locking.
	auto _ = this->fetched();

	this->_lockedFull = std::make_shared<LockedFlake>(
		nix::flake::lockFlake(*this->state, this->resolved(), lockFlags)
	);
	return this->_lockedFull.value();
}

auto LazyFlake::lockedRef(LockFlags lockFlags) -> FlakeRef&
{
	if (this->_lockedRef.has_value()) {
		return this->_lockedRef.value();
	}
	this->_lockedRef = this->locked(lockFlags).flake.lockedRef;
	return this->_lockedRef.value();
}

auto LazyFlake::installableValue(InstallableMode installableMode) -> nix::Value&
{
	if (this->_instValues.contains(installableMode)) {
		return this->_instValues.at(installableMode);
	}

	auto insertAndGet = [&](nix::Value &value) -> nix::Value& {
		// Copy construct the value into our installabe -> Value map, and then return it.
		this->_instValues.insert(std::pair{installableMode, nix::Value{value}});
		return this->_instValues.at(installableMode);
	};

	auto &evalCache = this->evalCache();
	auto rootCursor = evalCache.getRoot();
	nix::Value &asValue = rootCursor->forceValue();
	if (installableMode == InstallableMode::NONE) {
		// Copy construct the value into our installable -> Value map.
		//this->_instValues.insert(std::pair{installableMode, nix::Value{asValue}});
		//return this->_instValues.at(installableMode);
		return insertAndGet(asValue);
	}

	// Now, for each possible attrpath the fragment could refer to,
	// we'll check if it actually exists and use it if it does.
	StdVec<StdString> const requestedAttrPaths = this->_inst->getActualAttrPaths();

	for (StdStr const reqdPath : requestedAttrPaths) {
		// This is a bit of a hack.
		if (reqdPath.empty()) {
			// FIXME: move to after this loop instead.
			return insertAndGet(asValue);
		}

		// Get the requested attr path from the fragment as Symbols,
		// and then convert them to string views.
		auto const symToSv = [&](nix::Symbol const &sym) {
			return static_cast<StdStr>(this->state->symbols[sym]);
		};

		StdVec<nix::Symbol> const parsedAttrPath = nix::parseAttrPath(*this->state, reqdPath);
		auto const range = std::ranges::transform_view(parsedAttrPath, symToSv);
		StdVec<StdStr> attrPathParts{range.begin(), range.end()};

		// FIXME
		assert(asValue.type() == nix::nAttrs);

		AttrIterable currentAttrs{asValue.attrs, this->state->symbols};
		OptionalRef<nix::Attr> result = currentAttrs.find_by_nested_key(*state, attrPathParts);
		if (result.has_value()) {
			return insertAndGet(*result.value().get().value);
		}
	}

	// Finally, if none of the requested attr paths were found, and our installable mode is ALL,
	// then return the root value, as with installableMode == NONE.
	if (installableMode == InstallableMode::ALL) {
		return insertAndGet(asValue);
	}

	// Otherwise, throw an error for the not-found attrs.
	auto const &msg = fmt::format("flake '{}' does not provide any of {}",
		this->label,
		fmt::join(requestedAttrPaths, ", ")
	);
	throw nix::EvalError(msg);
}

void collectFlakeNodes(StdVec<LockedNode> &accumulator, Node const &node)
{
	for (auto const &[inputName, input] : node.inputs) {
		if (auto const *inputNode_ = std::get_if<0>(&input)) {
			LockedNode const &inputNode = **inputNode_;
			accumulator.push_back(inputNode);
			collectFlakeNodes(accumulator, inputNode);
		} else {
			eprintln("skipping node");
		}
	}
}
