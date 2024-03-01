#include <memory>

#include <nix/config.h>
#include <nix/eval.hh>
#include <nix/eval-cache.hh>
#include <nix/flake/flakeref.hh>
#include <nix/flake/lockfile.hh>
#include <nix/installable-flake.hh>

#include <boost/variant.hpp>

#include "logging.hpp"
#include "xil.hpp"
#include "std/map.hpp"
#include "std/optional.hpp"
#include "std/vector.hpp"

void collectFlakeNodes(
	StdVec<nix::flake::LockedNode> &accumulator,
	nix::flake::Node const &node
);

#pragma region LazyFlake
// Represents a flake at various stages of known information.
struct LazyFlake
{
#pragma region Types
	using Self             = LazyFlake;
	using NixEvalState     = std::shared_ptr<nix::EvalState>;
	using FlakeRef         = nix::FlakeRef;
	using InstallableFlake = nix::InstallableFlake;
	using LockedFlake      = nix::flake::LockedFlake;
	using LockedNode       = nix::flake::LockedNode;
	using LockFlags        = nix::flake::LockFlags;
	using EvalCache        = nix::eval_cache::EvalCache;
#pragma endregion

#pragma region Constants
	static const LockFlags DEFAULT_LOCK_FLAGS;
#pragma endregion

#pragma region Fields
	NixEvalState state;
	StdOpt<nix::ref<EvalCache>> _evalCache;
	XilLogger &logger;

	StdString label;

	// The lowest starting point, but we can start higher than here too.
	StdOpt<InstallableFlake> _inst;

	// The flake ref after resolving the registry.
	StdOpt<FlakeRef> _resolved;

	// The store path to this flake, once it's been fetched.
	StdOpt<nix::StorePath> _storePath;

	// The full flake after locking it.
	StdOpt<std::shared_ptr<LockedFlake>> _lockedFull;

	StdOpt<FlakeRef> _lockedRef;

	// The nix::Value for the given installable mode for this flake.
	StdMap<InstallableMode, nix::Value> _instValues;
#pragma endregion

#pragma region Specials
	LazyFlake(NixEvalState state, XilLogger &logger, InstallableFlake &&instFlake) :
		state(state), logger(logger), label(instFlake.flakeRef.to_string()), _inst(instFlake)
	{ }

	LazyFlake(NixEvalState state, XilLogger &logger, FlakeRef &&resolved) :
		state(state), logger(logger), label(resolved.to_string()), _resolved(resolved)
	{ }

	LazyFlake(NixEvalState state, XilLogger &logger, LockedFlake &&locked) :
		state(state),
		logger(logger),
		label(locked.flake.originalRef.to_string()),
		_lockedFull(std::make_optional(std::make_shared<LockedFlake>(locked)))
	{ }

	LazyFlake(NixEvalState state, XilLogger &logger, LockedNode &&node) :
		state(state),
		logger(logger),
		label(node.originalRef.to_string()),
		_lockedRef(node.lockedRef)
	{ }

#pragma endregion

#pragma region Methods

	EvalCache &evalCache();

	// Resolve the specified flake through the registry to a concrete flakeref.
	FlakeRef &resolved();

	// Fetch the resolved flake, without its inputs.
	nix::StorePath fetched();

	StdString fullStorePath();

	// Lock the flake.
	LockedFlake &locked(LockFlags lockFlags = DEFAULT_LOCK_FLAGS);
	std::shared_ptr<LockedFlake> lockedShared(LockFlags lockFlags = DEFAULT_LOCK_FLAGS);

	// Get the .locked FlakeRef.
	FlakeRef &lockedRef(LockFlags lockFlags = DEFAULT_LOCK_FLAGS);

	nix::Value &installableValue(InstallableMode installableMode = InstallableMode::ALL);
#pragma endregion
};

#pragma endregion
