#pragma once

#include <cassert>
#include <functional>
#include <memory>

// Lix headers.
#include <lix/config.h> // IWYU pragma: keep
// nix::Bindings
#include <lix/libexpr/attr-set.hh>
// nix::EvalState
#include <lix/libexpr/eval.hh>
// nix::noPos
#include <lix/libexpr/nixexpr.hh>
// nix::DrvInfo
#include <lix/libexpr/get-drvs.hh>
// nix::Value
#include <lix/libexpr/value.hh>
// nix::{DerivedPath, makeConstantStorePathRef}
#include <lix/libstore/derived-path.hh>
// nix::OutputsSpec
#include <lix/libstore/outputs-spec.hh>
// nix::Store
#include <lix/libstore/store-api.hh>
// nix::StorePath
#include <lix/libstore/path.hh>
// nix::{Verbosity, ErrorInfo}
#include <lix/libutil/error.hh>
// nix::ref
#include <lix/libutil/ref.hh>
// nix::{Logger, ActivityId, ActivityType, ResultType, Fields}
#include <lix/libutil/logging.hh>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include "std/string.hpp"
#include "std/string_view.hpp"
#include "std/vector.hpp"

using namespace std::literals::string_literals;

namespace nix
{
	StdStr format_as(nix::StorePath const path) noexcept;
}

#define STRINGIFY__(a) #a
#define STRINGIFY(a) STRINGIFY__(a)
#define SGR8(n) "\x1b[" STRINGIFY(n) "m"

struct AnsiFg
{
	using Sv = StdStr;
	constexpr static Sv RESET   = SGR8(0);
	constexpr static Sv BOLD    = SGR8(1);
	constexpr static Sv FAINT   = SGR8(2);
	constexpr static Sv ITALIC  = SGR8(3);

	constexpr static Sv RED     = SGR8(31);
	constexpr static Sv GREEN   = SGR8(32);
	constexpr static Sv YELLOW  = SGR8(33);
	constexpr static Sv BLUE    = SGR8(34);
	constexpr static Sv MAGENTA = SGR8(35);
	constexpr static Sv CYAN    = SGR8(36);
	constexpr static Sv WHITE   = SGR8(37);
};

#undef SGR
#undef STRINGIFY
#undef STRINGIFY__

/** Wraps a string in an ANSI color, adding the RESET code at the end. */
StdString wrapInColor(StdStr stringToWrap, StdStr ansiColor);

template <typename Range>
StdString wrapInColorAndJoin(Range &&range, StdStr separator, StdStr ansiColor)
{
	return fmt::format("{}{}{}",
		ansiColor,
		fmt::join(range, fmt::format("{}{}{}", AnsiFg::RESET, separator, ansiColor)),
		AnsiFg::RESET
	);
}

StdString logLevelToAnsiColor(nix::Verbosity level);

StdString logLevelName(nix::Verbosity level);

StdString logLevelToAnsi(nix::Verbosity level);

struct XilLogger : public nix::Logger
{
	void log(nix::Verbosity lvl, StdStr msg) override;

	void logEI(nix::ErrorInfo const &ei) override;

	void result(nix::ActivityId act, nix::ResultType type, Fields const &fields) override;

	void startActivity(
		nix::ActivityId act,
		nix::Verbosity lvl,
		nix::ActivityType type,
		StdString const &s,
		Fields const &fields,
		nix::ActivityId parent
	) override;
};

struct DerivationOutput
{
	StdString outputName;
	nix::StorePath outPath;
	nix::DerivedPath derivedPath;
};

StdStr format_as(DerivationOutput const output);

/** All the information you could want about a derivation, in one place. …Hopefully. */
struct DerivationMeta
{
	StdVec<DerivationOutput> outputs;
	nix::DrvInfo drvInfo;
	nix::StorePath drvPath;

	/** Assumes that the value is forced.
	  This constructor will throw if the provided attrset is not a derivation or
	  does not have a drvPath.
	*/
	explicit DerivationMeta(nix::EvalState &state, nix::Bindings *attrs) :
		drvInfo(nix::DrvInfo{""s, attrs}),
		drvPath(drvInfo.requireDrvPath(state))
	{
		auto outputNamePathMap = this->drvInfo.queryOutputs(state);
		for (auto const &[outName, outPath] : outputNamePathMap) {
			assert(outPath.has_value());
			auto derivedPath = nix::DerivedPath::Built {
				.drvPath = nix::makeConstantStorePathRef(this->drvPath),
				.outputs = nix::OutputsSpec::Names{outName},
			};
			this->outputs.push_back(DerivationOutput{outName, outPath.value(), derivedPath});
		}
	}

	StdVec<std::reference_wrapper<nix::StorePath>> outPaths();
	StdVec<StdString> fullOutPaths(nix::Store const &store);

	StdVec<nix::DerivedPath> derivedPaths();
};

struct DrvBuilder
{
	std::shared_ptr<nix::EvalState> state;
	nix::ref<nix::Store> store;
	DerivationMeta meta;
	nix::Logger *originalLogger;
	XilLogger ourLogger;

	/** This constructor may throw. */
	explicit DrvBuilder(std::shared_ptr<nix::EvalState> state, nix::ref<nix::Store> store, nix::Value &drvValue) :
		state(state),
		store(store),
		meta(*state, drvValue.attrs),
		originalLogger(nix::logger)
	{
		// We will restore the original logger in the destructor.
		nix::logger = &this->ourLogger;

		assert(this->state->isDerivation(drvValue));
	}

	~DrvBuilder();

	void realizeDerivations();
};
