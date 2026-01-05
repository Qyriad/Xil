#include "build.hpp"

#include <cstdint>
#include <sstream>
#include <filesystem>

#include "xil.hpp"

// nix::KeyedBuildResult
#include <lix/libstore/build-result.hh>
// nix::Derivation
#include <lix/libstore/derivations.hh>
// nix::Realisation
#include <lix/libstore/realisation.hh>

#include <boost/algorithm/string.hpp>
#include <cppitertools/itertools.hpp>

namespace stdfs = std::filesystem;

StdStr nix::format_as(nix::StorePath const path) noexcept
{
	return path.to_string();
}

StdString wrapInColor(StdStr stringToWrap, StdStr ansiColor)
{
	return fmt::format("{}{}{}", ansiColor, stringToWrap, AnsiFg::RESET);
}

StdString logLevelToAnsiColor(nix::Verbosity level)
{
	using S = StdString;
	switch (level) {
		case nix::lvlError:
			return S(AnsiFg::RED);
		case nix::lvlWarn:
			return S(AnsiFg::YELLOW);
		case nix::lvlNotice:
			return S(AnsiFg::CYAN);
		case nix::lvlInfo:
			return S(AnsiFg::GREEN);
		case nix::lvlTalkative:
			return S(AnsiFg::GREEN);
		case nix::lvlChatty:
			return S(AnsiFg::MAGENTA);
		case nix::lvlDebug:
			return S(AnsiFg::FAINT);
		case nix::lvlVomit:
			return fmt::format("{}{}", AnsiFg::FAINT, AnsiFg::ITALIC);
		default:
			assert("unreachable" == nullptr);
	}
}

StdString logLevelName(nix::Verbosity level)
{
	switch (level) {
		case nix::lvlError:
			return "error";
		case nix::lvlWarn:
			return "warning";
		case nix::lvlNotice:
			return "notice";
		case nix::lvlInfo:
			return "info";
		case nix::lvlTalkative:
			return "talk";
		case nix::lvlChatty:
			return "chat";
		case nix::lvlDebug:
			return "debug";
		case nix::lvlVomit:
			return "trace";
		default:
			assert("unreachable" == nullptr);
	}
}

StdString logLevelToAnsi(nix::Verbosity level)
{
	auto nameLower = logLevelName(level);
	auto nameUpper = boost::algorithm::to_upper_copy(nameLower);
	return fmt::format("{}:: {}", logLevelToAnsiColor(level), AnsiFg::RESET);
}

void XilLogger::log(nix::Verbosity lvl, StdStr msg)
{
	// FIXME: make configurable!
	if (lvl <= nix::Verbosity::lvlInfo) {
		eprintln("{} {}", logLevelToAnsi(lvl), msg);
	}
}

void XilLogger::logEI(nix::ErrorInfo const &ei)
{
	std::stringstream oss;
	// FIXME: showtrace should be configurable.
	showErrorInfo(oss, ei, true);

	this->log(ei.level, oss.str());
}

void XilLogger::result([[maybe_unused]] nix::ActivityId act, nix::ResultType type, Fields const &fields)
{
	using nix::logger, nix::lvlError;
	if (type == nix::resBuildLogLine) {
		auto lastLine = fields[0].s;
		// printError(lastLine);
		this->log(nix::lvlNotice, lastLine);
	} else if (type == nix::resPostBuildLogLine) {
		auto lastLine = fields[0].s;
		// printError("post-build-hook: " + lastLine);
		this->log(
			nix::lvlError,
			fmt::format("post-build-hook: {}", lastLine)
		);
	}
}

void XilLogger::startActivity(
	[[maybe_unused]] nix::ActivityId act,
	nix::Verbosity lvl,
	[[maybe_unused]] nix::ActivityType type,
	StdString const &s,
	[[maybe_unused]] Fields const &fields,
	[[maybe_unused]] nix::ActivityId parent
)
{
	// FIXME: allow level filtering
	if (!s.empty()) {
		// log(lvl, s + "...");
		// log(lvl, fmt::format("{}: {}...", act, s));
		log(lvl, fmt::format("{}...", s));
	}
}

StdStr format_as(DerivationOutput const output)
{
	return output.outPath.to_string();
}

StdVec<std::reference_wrapper<nix::StorePath>> DerivationMeta::outPaths()
{
	StdVec<std::reference_wrapper<nix::StorePath>> res;
	for (auto &derivationOutput : this->outputs) {
		res.push_back(derivationOutput.outPath);
	}

	return res;
}

StdVec<StdString> DerivationMeta::fullOutPaths(nix::Store const &store)
{
	StdVec<StdString> res;
	for (auto const &derivationOutput : this->outputs) {
		res.push_back(store.printStorePath(derivationOutput.outPath));
	}
	return res;
}

StdVec<nix::DerivedPath> DerivationMeta::derivedPaths()
{
	auto derivationOutputToDerivedPath = [](DerivationOutput &output) {
		return output.derivedPath;
	};

	auto derivedPaths = iter::imap(derivationOutputToDerivedPath, this->outputs);

	return StdVec<nix::DerivedPath>(derivedPaths.begin(), derivedPaths.end());
}

DrvBuilder::~DrvBuilder()
{
	// Restore the original logger, as we promised in the constructor.
	nix::logger = this->originalLogger;
}

struct PathToLink
{
	StdString name;
	stdfs::path path;
};

/** Automatically remove an existing symlink if and only if it is already a symlink to the Nix store. */
void maybeReplaceNixSymlink(nix::Store &store, stdfs::path const &target, stdfs::path const &linkName)
{
	// If `linkName` is already a symlink *and* it's to the Nix store, then replace it.
	// Otherwise, we don't want to replace other symlinks the user might have made.
	if (stdfs::is_symlink(linkName)) {
		auto followed = stdfs::read_symlink(linkName);
		if (store.isInStore(followed.string())) {
			stdfs::remove(linkName);
		}
	}

	stdfs::create_directory_symlink(target, linkName);
}

void DrvBuilder::realizeDerivations()
{
	nix::StorePathSet willBuild;
	nix::StorePathSet willSubst;
	nix::StorePathSet unknown;
	uint64_t downloadSize;
	uint64_t narSize;

	auto outPaths = this->meta.outPaths();
	auto fullOutPaths = this->meta.fullOutPaths(*this->store);

	this->store->queryMissing(
		this->meta.derivedPaths(),
		willBuild,
		willSubst,
		unknown,
		downloadSize,
		narSize
	);

	if (willBuild.size() > 0) {
		eprintln("building {} {}:", willBuild.size(), maybePluralize(willBuild.size(), "path"));
	}
	for (nix::StorePath const &path : willBuild) {

		nix::Derivation thisDerivation = this->state->aio.blockOn(this->store->derivationFromPath(path));
		nix::DerivationOutputs pathOutputs = thisDerivation.outputs;

		auto derivationOutputToStorePath = [&](nix::DerivationOutputs::value_type const &drvOutPair) {
			auto [outName, derivationOutput] = drvOutPair;
			auto drvOutPath = derivationOutput.path(
				*this->store, thisDerivation.name, outName);
			return this->store->printStorePath(drvOutPath.value());
		};

		auto outPaths = iter::imap(derivationOutputToStorePath, pathOutputs);

		eprintln("    {} -> {}",
			wrapInColor(this->store->printStorePath(path), AnsiFg::CYAN),
			wrapInColorAndJoin(outPaths, ", ", AnsiFg::MAGENTA));
	}

	if (willSubst.size() > 0) {
		eprintln("substituting {} paths:", willSubst.size());
	}
	for (auto const &path : willSubst) {
		// I would love to print in the `.drv -> {output}` format, but
		// for substituted paths we generally don't *have* the deriver,
		// and realizing it just to print the path would probably be sily.
		eprintln("    {}", wrapInColor(this->store->printStorePath(path), AnsiFg::MAGENTA));
	}

	if (willSubst.empty() && willBuild.empty()) {
		eprintln("Requested outputs {} are already realized.",
			wrapInColorAndJoin(fullOutPaths, ", ", AnsiFg::MAGENTA)
		);
	}

	// FIXME: what needs to happen for `unknown` to not be empty?
	assert(unknown.empty());

	StdVec<nix::KeyedBuildResult> results = this->state->aio.blockOn(this->store->buildPathsWithResults(this->meta.derivedPaths()));
	assert(!results.empty());

	// Now find the output paths of this build, and symlink them!
	StdVec<PathToLink> pathsToLink;

	for (nix::KeyedBuildResult &buildResult : results) {
		for (std::pair<StdString const, nix::Realisation> &outPair : buildResult.builtOutputs) {
			auto [name, realization] = outPair;
			pathsToLink.emplace_back(name, this->store->printStorePath(realization.outPath));
		}
	}

	for (auto const &[name, targetPath] : pathsToLink) {
		StdString linkBasename = (pathsToLink.size() == 1) ? "result" : fmt::format("result-{}", name);
		auto linkName = stdfs::current_path().append(linkBasename);

		maybeReplaceNixSymlink(*this->store, targetPath, linkName);
		eprintln("./{} -> {}", linkBasename, targetPath.string());
	}
}
