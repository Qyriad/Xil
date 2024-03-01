#include "build.hpp"

#include <cstdint>
#include <chrono>
#include <sstream>
#include <filesystem>
#include <iterator>

#include "xil.hpp"

// nix::KeyedBuildResult
#include <nix/build-result.hh>
// nix::Derivation
#include <nix/derivations.hh>
// FIXME
#include <nix/logging.hh>
// nix::Realisation
#include <nix/realisation.hh>

#include <boost/algorithm/string.hpp>
#include <cppitertools/itertools.hpp>
#include <fmt/format.h>

namespace stdfs = std::filesystem;

std::string_view nix::format_as(nix::StorePath const path) noexcept
{
	return path.to_string();
}

std::string nix::format_as(nix::Logger::Field field) noexcept
{
	switch (field.type) {
		case Logger::Field::tInt:
			return fmt::format("{}", field.i);
		case Logger::Field::tString:
			return field.s;
		default:
			assert(nullptr == "unreachable");
	}
}

std::string wrapInColor(std::string_view stringToWrap, std::string_view ansiColor)
{
	return fmt::format("{}{}{}", ansiColor, stringToWrap, AnsiFg::RESET);
}

//std::string logLevelToAnsiColor(nix::Verbosity level)
//{
//	using S = std::string;
//	switch (level) {
//		case nix::lvlError:
//			return S(AnsiFg::RED);
//		case nix::lvlWarn:
//			return S(AnsiFg::YELLOW);
//		case nix::lvlNotice:
//			return S(AnsiFg::CYAN);
//		case nix::lvlInfo:
//			return S(AnsiFg::GREEN);
//		case nix::lvlTalkative:
//			return S(AnsiFg::GREEN);
//		case nix::lvlChatty:
//			return S(AnsiFg::MAGENTA);
//		case nix::lvlDebug:
//			return S(AnsiFg::FAINT);
//		case nix::lvlVomit:
//			return fmt::format("{}{}", AnsiFg::FAINT, AnsiFg::ITALIC);
//		default:
//			assert("unreachable" == nullptr);
//	}
//}
//
//std::string logLevelName(nix::Verbosity level)
//{
//	switch (level) {
//		case nix::lvlError:
//			return "error";
//		case nix::lvlWarn:
//			return "warning";
//		case nix::lvlNotice:
//			return "notice";
//		case nix::lvlInfo:
//			return "info";
//		case nix::lvlTalkative:
//			return "talk";
//		case nix::lvlChatty:
//			return "chat";
//		case nix::lvlDebug:
//			return "debug";
//		case nix::lvlVomit:
//			return "trace";
//		default:
//			assert("unreachable" == nullptr);
//	}
//}
//
//std::string logLevelToAnsi(nix::Verbosity level)
//{
//	auto nameLower = logLevelName(level);
//	auto nameUpper = boost::algorithm::to_upper_copy(nameLower);
//	return fmt::format("{}:: {}", logLevelToAnsiColor(level), AnsiFg::RESET);
//}

//std::string_view const actTypeToString(nix::ActivityType activityType)
//{
//	switch (activityType) {
//		case nix::actUnknown:
//			return "Unknown";
//		case nix::actCopyPath:
//			return "CopyPath";
//		case nix::actFileTransfer:
//			return "FileTransfer";
//		case nix::actRealise:
//			return "Realise";
//		case nix::actCopyPaths:
//			return "CopyPaths";
//		case nix::actBuilds:
//			return "Builds";
//		case nix::actBuild:
//			return "Build";
//		case nix::actOptimiseStore:
//			return "OptimiseStore";
//		case nix::actVerifyPaths:
//			return "VerifyPaths";
//		case nix::actSubstitute:
//			return "Substitute";
//		case nix::actQueryPathInfo:
//			return "QueryPathInfo";
//		case nix::actPostBuildHook:
//			return "PostBuildHook";
//		case nix::actBuildWaiting:
//			return "Waiting";
//			break;
//		default:
//			assert(nullptr == "unreachable");
//	}
//}
//
//std::string_view const actTypePretty(nix::ActivityType activityType)
//{
//	switch (activityType) {
//		case nix::actUnknown:
//			return "unknown activity (Xil bug?)";
//		case nix::actCopyPath:
//			return "path copy";
//		case nix::actFileTransfer:
//			return "file transfer";
//		case nix::actRealise:
//			return "realization";
//		case nix::actCopyPaths:
//			return "path copies";
//		case nix::actBuilds:
//			return "builds";
//		case nix::actBuild:
//			return "build";
//		case nix::actOptimiseStore:
//			return "store optimization";
//		case nix::actVerifyPaths:
//			return "path verification";
//		case nix::actSubstitute:
//			return "substitution";
//		case nix::actQueryPathInfo:
//			return "path info query";
//		case nix::actPostBuildHook:
//			return "post build hook";
//		case nix::actBuildWaiting:
//			return "waiting for build";
//		default:
//			assert(nullptr == "unreachable");
//	}
//}

//std::string_view const resultTypeToString(nix::ResultType resultType)
//{
//	switch (resultType) {
//		case nix::resFileLinked:
//			return "FileLinked";
//		case nix::resBuildLogLine:
//			return "BuildLogLine";
//		case nix::resUntrustedPath:
//			return "UntrustedPath";
//		case nix::resCorruptedPath:
//			return "CorruptedPath";
//		case nix::resSetPhase:
//			return "SetPhase";
//		case nix::resProgress:
//			return "Progress";
//		case nix::resSetExpected:
//			return "SetExpected";
//		case nix::resPostBuildLogLine:
//			return "PostBuildLogLine";
//		default:
//			assert(nullptr == "unreachable");
//	}
//}

//std::string humanBytes(uint64_t bytes)
//{
//	constexpr uint64_t THRESHOLD = 1024 * 2;
//	double current = bytes;
//	uint32_t prefixLevel = 0;
//
//	using enum BinaryPrefixInner;
//
//
//	while (current > THRESHOLD) {
//		current /= 1024.0;
//		prefixLevel += 1;
//		if (prefixLevel == BinaryPrefix{TEBI}) {
//			// Impressive.
//			// We'll just count you as tebibytes, friend.
//			break;
//		}
//	}
//
//	auto mkSuffix = [](uint32_t prefixLevel) -> std::string_view {
//		switch (prefixLevel) {
//			case NONE:
//				return "bytes";
//			case KIBI:
//				return "KiB";
//			case MEBI:
//				return "MiB";
//			case GIBI:
//				return "GiB";
//			case TEBI:
//				return "TiB";
//			default:
//				assert("unreachable" == nullptr);
//		}
//	};
//
//	return fmt::format("{:.2f} {}", current, mkSuffix(prefixLevel), bytes);
//}

//struct ResBuildLogLine : public LogResult
//{
//	std::string currentLogLine;
//
//	explicit ResBuildLogLine(nix::Logger::Fields const &fields) :
//		currentLogLine(fields.at(0).s)
//	{
//		assert(fields.size() == 1);
//		assert(fields.at(0).type == nix::Logger::Field::tString);
//	}
//
//	std::string format() const override
//	{
//		return this->currentLogLine;
//	}
//};

//LogResult *mkLogResult(nix::ResultType type, nix::Logger::Fields const &fields)
//{
//	switch (type) {
//		case nix::resFileLinked:
//		case nix::resBuildLogLine:
//			abort();
//			//return ResBuildLogLine{fields};
//		case nix::resUntrustedPath:
//		case nix::resCorruptedPath:
//		case nix::resSetPhase:
//			abort();
//		case nix::resProgress:
//			return new ResProgress{fields};
//		case nix::resSetExpected:
//		case nix::resPostBuildLogLine:
//		default:
//			assert("unimplemented" == nullptr);
//			break;
//	}
//}

//bool ResProgress::operator==(ResProgress const &rhs) const
//{
//	using Fields = std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>;
//	Fields const &lhsFields{this->done, this->expected, this->running, this->failed};
//	Fields const &rhsFields{rhs.done, rhs.expected, rhs.running, rhs.failed};
//	return lhsFields == rhsFields;
//}

//std::string ResProgress::format() const
//{
//	std::vector<std::string> numbers;
//	std::vector<std::string> labels;
//
//	// Always show the downloaded amount, even if it's zero.
//	numbers.emplace_back(humanBytes(this->done));
//	labels.emplace_back("DL");
//
//	if (this->expected > 0) {
//		numbers.emplace_back(humanBytes(this->expected));
//		labels.emplace_back("total");
//	}
//
//	if (this->running > 0) {
//		numbers.emplace_back(humanBytes(this->running));
//		labels.emplace_back("running");
//	}
//
//	if (this->failed > 0) {
//		numbers.emplace_back(humanBytes(this->failed));
//		labels.emplace_back("failed");
//	}
//
//	std::string_view separator = (numbers.size() > 1) ? "\t" : " ";
//
//	return fmt::format("{}{}{}", fmt::join(numbers, "/"), separator, fmt::join(labels, "/"));
//}

//void XilLogger::prepareFetch(std::string const &&label)
//{
//	this->preparedUriLabels.push_front(label);
//}
//
//bool XilLogger::isVerbose()
//{
//	eprintln("XilLogger::isVerbose()");
//	return true;
//}
//
//void XilLogger::log(nix::Verbosity lvl, std::string_view msg)
//{
//	//eprintln("XilLogger::log()");
//	// FIXME: make configurable!
//	if (lvl <= nix::Verbosity::lvlTalkative) {
//		eprintln("{} {}", logLevelToAnsi(lvl), msg);
//	}
//}
//
//void XilLogger::logEI(nix::ErrorInfo const &ei)
//{
//	eprintln("XilLogger::logEI()");
//	std::stringstream oss;
//	// FIXME: showtrace should be configurable.
//	showErrorInfo(oss, ei, true);
//
//	this->log(ei.level, oss.str());
//}
//
//void XilLogger::warn(std::string const &msg)
//{
//	eprintln("XilLogger::warn()");
//}
//
//void XilLogger::result(nix::ActivityId act, nix::ResultType type, Fields const &fields)
//{
//	auto now = Clock::now();
//
//	if (type == nix::resBuildLogLine) {
//		eprint("resBuildLastLine: ");
//		auto lastLine = fields[0].s;
//		// printError(lastLine);
//		this->log(nix::lvlInfo, lastLine);
//	} else if (type == nix::resPostBuildLogLine) {
//		eprint("resBuildPostThing: ");
//		auto lastLine = fields[0].s;
//		// printError("post-build-hook: " + lastLine);
//		this->log(
//			nix::lvlError,
//			fmt::format("post-build-hook: {}", lastLine)
//		);
//	} else if (type == nix::resProgress) {
//		if ((now - this->prevResultUpdate) >= this->resultUpdateThreshold) {
//			ResProgress progress{fields};
//
//			if (this->lastResultType != nix::resProgress) {
//				fmt::println("");
//			}
//
//			if (progress != this->lastProgress) {
//				this->prevResultUpdate = now;
//				this->lastProgress = progress;
//				std::string &uri{this->activityUris.at(act)};
//				std::string label{uri};
//				if (this->uriLabels.contains(uri)) {
//					label = this->uriLabels.at(uri);
//				}
//				label += ":";
//				eprint("\r\x1b[2K{:<40} {}", label, progress.format());
//			}
//		}
//	} else {
//		eprintln("result: {} with {}",
//			resultTypeToString(type),
//			fmt::join(fields, ", ")
//		);
//	}
//	this->lastResultType = type;
//}
//
//void XilLogger::startActivity(
//	nix::ActivityId act,
//	nix::Verbosity lvl,
//	nix::ActivityType type,
//	std::string const &s,
//	Fields const &fields,
//	[[maybe_unused]] nix::ActivityId parent
//)
//{
//	if (type == nix::actFileTransfer) {
//		FileTransferAct ftAct{fields};
//		this->activityUris.insert(std::make_pair(act, ftAct.uri));
//
//		// If we have been prepared with a label, then associate this URI
//		// with that label.
//		if (this->preparedUriLabels.size() > 0) {
//			std::string_view const label{preparedUriLabels.front()};
//			eprintln("Got prepared label {}", label);
//			this->uriLabels.insert(std::make_pair(ftAct.uri, std::string{label}));
//			preparedUriLabels.pop_front();
//		}
//
//		log(lvl, fmt::format("Starting {}", actTypePretty(type)));
//	} else if (!s.empty()) {
//		// FIXME: allow level filtering
//
//		// log(lvl, s + "...");
//		// log(lvl, fmt::format("{}: {}...", act, s));
//
//		log(lvl, fmt::format("activity {}: {}...", actTypeToString(type), s));
//		eprintln("\tfields: {}", fmt::join(fields, ", "));
//	}
//}
//
//void XilLogger::stopActivity(nix::ActivityId act)
//{
//	eprintln("XilLogger::stopActivity()");
//}
//
//void XilLogger::setPrintBuildLogs(bool printBuildLogs)
//{
//	eprintln("XilLogger::setPrintBuildLogs()");
//}
//
//std::string_view format_as(DerivationOutput const output)
//{
//	return output.outPath.to_string();
//}

std::vector<std::reference_wrapper<nix::StorePath>> DerivationMeta::outPaths()
{
	std::vector<std::reference_wrapper<nix::StorePath>> res;
	for (auto &derivationOutput : this->outputs) {
		res.push_back(derivationOutput.outPath);
	}

	return res;
}

std::vector<std::string> DerivationMeta::fullOutPaths(nix::Store const &store)
{
	std::vector<std::string> res;
	for (auto const &derivationOutput : this->outputs) {
		res.push_back(store.printStorePath(derivationOutput.outPath));
	}
	return res;
}

std::vector<nix::DerivedPath> DerivationMeta::derivedPaths()
{
	auto derivationOutputToDerivedPath = [](DerivationOutput &output) {
		return output.derivedPath;
	};

	auto derivedPaths = iter::imap(derivationOutputToDerivedPath, this->outputs);

	return std::vector<nix::DerivedPath>(derivedPaths.begin(), derivedPaths.end());
}

DrvBuilder::~DrvBuilder()
{
	// Restore the original logger, as we promised in the constructor.
	nix::logger = this->originalLogger;
}

struct PathToLink
{
	std::string name;
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

		nix::Derivation thisDerivation = this->store->derivationFromPath(path);
		nix::DerivationOutputs pathOutputs = thisDerivation.outputs;

		auto derivationOutputToStorePath = [&](nix::DerivationOutputs::value_type &drvOutPair) {
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

	std::vector<nix::KeyedBuildResult> results = this->store->buildPathsWithResults(this->meta.derivedPaths());
	assert(!results.empty());

	// Now find the output paths of this build, and symlink them!
	std::vector<PathToLink> pathsToLink;

	for (nix::KeyedBuildResult &buildResult : results) {
		for (std::pair<std::string const, nix::Realisation> &outPair : buildResult.builtOutputs) {
			auto [name, realization] = outPair;
			pathsToLink.emplace_back(name, this->store->printStorePath(realization.outPath));
		}
	}

	for (auto const &[name, targetPath] : pathsToLink) {
		std::string linkBasename = (pathsToLink.size() == 1) ? "result" : fmt::format("result-{}", name);
		auto linkName = stdfs::current_path().append(linkBasename);

		maybeReplaceNixSymlink(*this->store, targetPath, linkName);
		eprintln("./{} -> {}", linkBasename, targetPath.string());
	}
}
