#include "build.hpp"

std::string_view nix::format_as(nix::StorePath const path) noexcept
{
	return path.to_string();
}

std::string wrapInColor(std::string_view stringToWrap, std::string_view ansiColor)
{
	return fmt::format("{}{}{}", ansiColor, stringToWrap, AnsiFg::RESET);
}

std::string logLevelToAnsiColor(nix::Verbosity level)
{
	using S = std::string;
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

std::string logLevelName(nix::Verbosity level)
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

std::string logLevelToAnsi(nix::Verbosity level)
{
	auto nameLower = logLevelName(level);
	auto nameUpper = boost::algorithm::to_upper_copy(nameLower);
	return fmt::format("{}:: {}", logLevelToAnsiColor(level), AnsiFg::RESET);
}

void XilLogger::log(nix::Verbosity lvl, std::string_view msg)
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
	std::string const &s,
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

std::string_view format_as(DerivationOutput const output)
{
	return output.outPath.to_string();
}

std::vector<std::reference_wrapper<nix::StorePath>> DerivationMeta::outPaths()
{
	std::vector<std::reference_wrapper<nix::StorePath>> res;
	for (auto &derivationOutput : this->outputs) {
		res.push_back(derivationOutput.outPath);
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

void DrvBuilder::realizeDerivations() {
	nix::StorePathSet willBuild;
	nix::StorePathSet willSubst;
	nix::StorePathSet unknown;
	uint64_t downloadSize;
	uint64_t narSize;

	auto outPaths = this->meta.outPaths();

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
			wrapInColor(this->store->printStorePath(path),
				AnsiFg::CYAN),
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

	// FIXME: what needs to happen for `unknown` to not be empty?
	assert(unknown.empty());

	this->store->buildPaths(this->meta.derivedPaths());
}
