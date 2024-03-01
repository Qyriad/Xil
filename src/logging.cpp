#include "logging.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>

#include <nix/config.h>
#include <nix/error.hh>
#include <nix/fetchers.hh>
#include <nix/logging.hh>

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <cppitertools/itertools.hpp>

#include "std/map.hpp"
#include "std/string.hpp"
#include "std/string_view.hpp"
#include "std/vector.hpp"
#include "build.hpp"
#include "xil.hpp"

//std::string nix::format_as(nix::Logger::Field const field) noexcept
//{
//	if (field.type == nix::Logger::Field::tInt) {
//		return fmt::format("{}", field.i);
//	} else {
//		return fmt::format("{}", field.s);
//	}
//}

StdString tabulate(StdStr const first, StdStr const second, uint32_t padding)
{
	StdString final;
	final.reserve(first.size() + second.size() + padding);

	for (auto const &ch : first) {
		final.push_back(ch);
	}

	int64_t prefixPart = padding - first.size();
	int64_t realPadding = std::max<int64_t>(prefixPart, 0LL);
	xlogln("Adding {} spaces for padding", realPadding);

	for ([[maybe_unused]] auto const &i : iter::range(realPadding)) {
		final.push_back(' ');
	}

	for (auto const &ch : second) {
		final.push_back(ch);
	}

	return final;
}

struct BinaryPrefix
{
	enum Inner
	{
		NONE = 0,
		KIBI = 1,
		MEBI = 2,
		GIBI = 3,
		TEBI = 4,
	};

	Inner inner;

	constexpr BinaryPrefix(Inner inner) : inner(inner) { }

	constexpr operator Inner() const noexcept
	{
		return this->inner;
	}

	explicit operator bool() = delete;
};

StdString humanBytes(uint64_t bytes)
{
	constexpr uint64_t THRESHOLD = 1024 * 2;
	double current = bytes;
	uint32_t prefixLevel = 0;

	using enum BinaryPrefix::Inner;


	while (current > THRESHOLD) {
		current /= 1024.0;
		prefixLevel += 1;
		if (prefixLevel == TEBI) {
			// Impressive.
			// We'll just count you as tebibytes, friend.
			break;
		}
	}

	auto mkSuffix = [](uint32_t prefixLevel) -> StdStr {
		switch (prefixLevel) {
			case NONE:
				return "bytes";
			case KIBI:
				return "KiB";
			case MEBI:
				return "MiB";
			case GIBI:
				return "GiB";
			case TEBI:
				return "TiB";
			default:
				assert("unreachable" == nullptr);
		}
	};

	return fmt::format("{:.2f} {}", current, mkSuffix(prefixLevel), bytes);
}

StdString logLevelToAnsiColor(nix::Verbosity level)
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

StdStr const actTypeToString(nix::ActivityType const activityType)
{
	switch (activityType) {
		case nix::actUnknown:
			return "Unknown";
		case nix::actCopyPath:
			return "CopyPath";
		case nix::actFileTransfer:
			return "FileTransfer";
		case nix::actRealise:
			return "Realise";
		case nix::actCopyPaths:
			return "CopyPaths";
		case nix::actBuilds:
			return "Builds";
		case nix::actBuild:
			return "Build";
		case nix::actOptimiseStore:
			return "OptimiseStore";
		case nix::actVerifyPaths:
			return "VerifyPaths";
		case nix::actSubstitute:
			return "Substitute";
		case nix::actQueryPathInfo:
			return "QueryPathInfo";
		case nix::actPostBuildHook:
			return "PostBuildHook";
		case nix::actBuildWaiting:
			return "Waiting";
			break;
		default:
			assert(nullptr == "unreachable");
	}
}

StdStr const actTypePretty(nix::ActivityType const activityType)
{
	switch (activityType) {
		case nix::actUnknown:
			return "unknown activity";
		case nix::actCopyPath:
			return "path copy";
		case nix::actFileTransfer:
			return "file transfer";
		case nix::actRealise:
			return "realization";
		case nix::actCopyPaths:
			return "path copies";
		case nix::actBuilds:
			return "builds";
		case nix::actBuild:
			return "build";
		case nix::actOptimiseStore:
			return "store optimization";
		case nix::actVerifyPaths:
			return "path verification";
		case nix::actSubstitute:
			return "substitution";
		case nix::actQueryPathInfo:
			return "path info query";
		case nix::actPostBuildHook:
			return "post build hook";
		case nix::actBuildWaiting:
			return "waiting for build";
		default:
			assert(nullptr == "unreachable");
	}
}

StdStr const resultTypeToString(nix::ResultType resultType)
{
	switch (resultType) {
		case nix::resFileLinked:
			return "FileLinked";
		case nix::resBuildLogLine:
			return "BuildLogLine";
		case nix::resUntrustedPath:
			return "UntrustedPath";
		case nix::resCorruptedPath:
			return "CorruptedPath";
		case nix::resSetPhase:
			return "SetPhase";
		case nix::resProgress:
			return "Progress";
		case nix::resSetExpected:
			return "SetExpected";
		case nix::resPostBuildLogLine:
			return "PostBuildLogLine";
		default:
			assert(nullptr == "unreachable");
	}
}

StdString LogActivity::format() const
{
	auto const &msg = fmt::format("LogActivity::format() called on abstract acitivity {} {}",
		actTypeToString(this->type),
		this->id
	);
	xlogln("{}", msg);
	//throw std::logic_error(msg);
	return "";
}

template <typename ClassT>
requires std::copy_constructible<ClassT>
ClassT copy_construct(ClassT const &construct_from)
{
	return ClassT{construct_from};
}

template <typename ClassT>
requires std::move_constructible<ClassT>
ClassT move_construct(ClassT &&construct_from)
{
	return ClassT{construct_from};
}

auto ActFileTransfer::from(nix::ActivityId id, nix::Logger::Fields const &fields) -> Self
{
	assert(fields.size() == 1);
	assert(fields.at(0).type == nix::Logger::Field::tString);

	return ActFileTransfer{id, copy_construct(fields.at(0).s)};
}

StdString ActFileTransfer::format() const
{
	//return this->uri;
	return this->label.value_or(this->uri);
}

auto ActCopyPaths::from(nix::ActivityId id, nix::Logger::Fields const &fields) -> Self
{
	assert(fields.size() == 0);
	return ActCopyPaths{id};
}

StdString ActCopyPaths::format() const
{
	return this->label.value_or("copying paths");
}

auto ActCopySinglePath::from(nix::ActivityId id, nix::Logger::Fields const &fields) -> Self
{
	assert(fields.size() == 3);
	for (auto const &field : fields) {
		assert (field.type == nix::Logger::Field::tString);
	}

	//eprintln("\nCopySinglePath{{{}, {}, {}}}\n", fields.at(0).s, fields.at(1).s, fields.at(2).s);

	return Self{id, fields.at(0).s, fields.at(1).s, fields.at(2).s};
}

StdString ActCopySinglePath::format() const
{
	return fmt::format("Copying store path {} from {}", this->destUri, this->sourceUri);
}

StdString LogResult::format() const
{
	throw std::logic_error("LogResult::format() is an abstract method");
}

ResProgress::ResProgress(nix::Logger::Fields const &fields)
{
	assert(fields.size() == 4);
	for (auto const &field : fields) {
		assert(field.type == nix::Logger::Field::tInt);
	}
	try {
		this->done = fields.at(0).i;
		this->expected = fields.at(1).i;
		this->running = fields.at(2).i;
		this->failed = fields.at(3).i;
	} catch (std::out_of_range &ex) {
		//eprintln("fields: {}", fmt::join(fields, ", "));
		eprintln("\tfields: {}", fmt::join(fields, ", "));
		throw;
	}
}

bool ResProgress::operator==(ResProgress const &rhs) const
{
	using Fields = std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>;
	Fields const &lhsFields{this->done, this->expected, this->running, this->failed};
	Fields const &rhsFields{rhs.done, rhs.expected, rhs.running, rhs.failed};
	return lhsFields == rhsFields;
}

StdString ResProgress::format() const
{
	StdVec<StdString> numbers;
	StdVec<StdString> labels;

	// Always show the downloaded amount, even if it's zero.
	numbers.emplace_back(humanBytes(this->done));
	labels.emplace_back("DL");

	if (this->expected > 0) {
		numbers.emplace_back(humanBytes(this->expected));
		labels.emplace_back("total");
	}

	if (this->running > 0) {
		numbers.emplace_back(humanBytes(this->running));
		labels.emplace_back("running");
	}

	if (this->failed > 0) {
		numbers.emplace_back(humanBytes(this->failed));
		labels.emplace_back("failed");
	}

	StdStr separator = (numbers.size() > 1) ? "\t" : " ";

	return fmt::format("{}{}{}", fmt::join(numbers, "/"), separator, fmt::join(labels, "/"));
}

//ResSetExpected::ResSetExpected(nix::Logger::Fields const &fields)
//{
//	assert(fields.size() == 1);
//	assert(fields.at(0).type == nix::Logger::Field::tInt);
//	this->expected = fields.at(0).i;
//}
//
//bool ResSetExpected::operator==(ResSetExpected const &rhs) const
//{
//	return this->expected == rhs.expected;
//}

LoggerLabelGuard::~LoggerLabelGuard()
{
	this->logger.preparedUriLabels.pop_front();
}

/**********
  XilLogger
**********/

//void XilLogger::withFetchLabel(StdStr label, std::function<void()> &&callback)
//{
//	// FIXME: this doesn't need to be a string.
//	this->preparedUriLabels.push_front(StdString{label});
//	callback();
//	this->preparedUriLabels.pop_front();
//}

//void XilLogger::prepareFetch(StdString const &&label)
//{
//	this->preparedUriLabels.push_front(label);
//}

bool XilLogger::isVerbose()
{
	eprintln("XilLogger::isVerbose()");
	return true;
}

void XilLogger::log(nix::Verbosity lvl, StdStr msg)
{
	//eprintln("XilLogger::log()");
	// FIXME: make configurable!
	if (lvl <= nix::Verbosity::lvlTalkative) {
		eprintln("{}{}", logLevelToAnsi(lvl), msg);
	}
}

void XilLogger::logEI(nix::ErrorInfo const &ei)
{
	eprintln("XilLogger::logEI()");
	std::stringstream oss;
	// FIXME: showtrace should be configurable.
	showErrorInfo(oss, ei, true);

	this->log(ei.level, oss.str());
}

void XilLogger::warn(StdString const &msg)
{
	this->log(nix::lvlWarn, fmt::format("warning: {}", msg));
}

void XilLogger::result(nix::ActivityId act, nix::ResultType type, Fields const &fields)
{
	auto now = Clock::now();

	if (this->activities.contains(act)) {
		auto const &activity = *this->activities.at(act);
		xlogln("XilLogger::result({} {}, {})", actTypeToString(activity.type), act, resultTypeToString(type));
	} else {
		xlogln("XilLogger::result({}, {})", act, resultTypeToString(type));
	}

	if (type == nix::resBuildLogLine) {
		eprint("resBuildLastLine: ");
		auto lastLine = fields[0].s;
		// printError(lastLine);
		this->log(nix::lvlInfo, lastLine);
	} else if (type == nix::resPostBuildLogLine) {
		eprint("resBuildPostThing: ");
		auto lastLine = fields[0].s;
		// printError("post-build-hook: " + lastLine);
		this->log(
			nix::lvlError,
			fmt::format("post-build-hook: {}", lastLine)
		);
	} else if (type == nix::resProgress) {
		if ((now - this->prevResultUpdate) >= this->resultUpdateThreshold) {

			try {
				ResProgress progress{fields};

				if (progress != this->lastProgress) {
					this->prevResultUpdate = now;
					this->lastProgress = progress;
					StdString label{this->activities.at(act)->format()};
					label += ":";
					//eprint("\r\x1b[2K");
					eprint("\r\x1b[2K");
					//eprint("{:<40} {}", label, progress.format());
					//eprintln("padding: {}, label size: {}", padding, label.size());

					// Cursed.

					//int64_t padding = std::max(static_cast<int64_t>(label.size()) - 40LL, 0LL);
					//auto fmtString = fmt::format("{{}}{{:<{}}} {{:<60}}", padding ? padding : 1);
					//eprint(fmt::runtime(fmtString), logLevelToAnsi(nix::lvlInfo), label, progress.format());

					//auto first = fmt::format("{}{}", logLevelToAnsi(nix::lvlInfo), label);
					auto second = progress.format();
					//xlogln("first: {};; second: {}", label.size(), second.size());
					eprint("{}{}", logLevelToAnsi(nix::lvlInfo), tabulate(label, second, 40 - 3)); // FIXME: no log level


					//eprint("{}{:<40} {:<60}", logLevelToAnsi(nix::lvlInfo), label, progress.format());
				}
			} catch (std::out_of_range &ex) {
				eprintln("error getting fields for progress with fields N={}", fields.size());
			}

			//if (this->lastResultType != nix::resProgress) {
			//	fmt::println("");
			//}

		}
	} else if (type == nix::resSetExpected) {
		// Ignored.
	} else {
		if (this->lastResultType == nix::resProgress) {
			eprintln("");
		}
		eprintln("result: {} with {}",
			resultTypeToString(type),
			fmt::join(fields, ", ")
		);
	}
	this->lastResultType = type;
}

void XilLogger::startActivity(
	nix::ActivityId actId,
	nix::Verbosity lvl,
	nix::ActivityType type,
	StdString const &s,
	Fields const &fields,
	[[maybe_unused]] nix::ActivityId parent
)
{
	xlogln("Starting {} activity {}: {}", actId, actTypeToString(type), s);
	if (!fields.empty()) {
		xlogln("Fields: {}", fmt::join(fields, ", "));
	}
	if (type == nix::actFileTransfer) {

		auto act = std::make_shared<ActFileTransfer>(ActFileTransfer::from(actId, fields));
		std::shared_ptr<LogActivity> logAct = act;

		this->activities.insert(std::pair{actId, logAct});

		StdStr label{act->uri};
		if (this->preparedUriLabels.empty()) {
			throw UnexpectedFetch(actId, act->uri);
		}

		label = this->preparedUriLabels.front();
		act->label = std::make_optional(label);

		this->log(lvl, fmt::format("Starting {} of {} for {}", actTypePretty(type), act->uri, label));

	} else if (type == nix::actCopyPaths) {

		auto act = std::make_shared<ActCopyPaths>(ActCopyPaths::from(actId, fields));
		std::shared_ptr<LogActivity> logAct = act;
		this->activities.insert(std::pair{actId, logAct});

		if (this->preparedUriLabels.empty()) {
			throw UnexpectedFetch(actId, s);
		}
	} else if (type == nix::actCopyPath) {
		auto isPresent = this->activities.contains(actId);
		if (!isPresent) {
			xlogln("Inserting for act ID {}", actId);
			auto act = std::make_shared<ActCopySinglePath>(ActCopySinglePath::from(actId, fields));
			std::shared_ptr<LogActivity> logAct = act;
			this->activities.insert(std::pair{actId, logAct});

			if (this->preparedUriLabels.empty()) {
				throw UnexpectedFetch(actId, s);
			}
			eprintln("");
			this->log(lvl, act->format());
		}
	} else if (!s.empty()) {
		// FIXME: allow level filtering

		this->log(lvl, fmt::format("activity {}: {}...", actTypeToString(type), s));
	}

	auto act = std::make_shared<LogActivity>(actId, type);
	this->activities.insert(std::make_pair(actId, act));
}

void XilLogger::stopActivity(nix::ActivityId act)
{
	try {
		auto activity = this->activities.at(act);
		if (activity->type == nix::actFileTransfer) {
			auto transfer = std::dynamic_pointer_cast<ActFileTransfer>(activity);
			//eprint("\r\x1b[2K");
			eprintln("");


			//auto prefix = fmt::format("{} Finished ", logLevelToAnsi(nix::lvlInfo));
			//int64_t padding = std::max(static_cast<int64_t>(prefix.size()) - 40LL, 0LL);
			//auto fmtString = fmt::format("Finished {{:<{}}} {{:<60}}", padding ? padding : 1);
			//auto formatted = fmt::format(fmt::runtime(fmtString), transfer->format(), this->lastProgress.format());
			//this->log(nix::lvlInfo, formatted);

			auto first = fmt::format("Finished {}", transfer->format());
			auto second = this->lastProgress.format();
			eprintln("{}{}", logLevelToAnsi(nix::lvlInfo), tabulate(first, second, 40 - 3));


			//this->log(nix::lvlInfo,
			//	fmt::format("Finished {:<40} {:<60}\n", transfer->format(), this->lastProgress.format())
			//);


			//eprintln("");
			//eprintln("Finished {}", actTypePretty(activity->type));
		}
		//if (activity->type != nix::actUnknown) {
		//	eprintln("Finished {}", actTypePretty(activity->type));
		//}
		this->activities.erase(act);
	} catch (std::out_of_range const &ex) {
		eprintln("Stopped unknown activity {}", act);
		throw;
	}
}

void XilLogger::setPrintBuildLogs([[maybe_unused]] bool printBuildLogs)
{
	eprintln("XilLogger::setPrintBuildLogs()");
}

StdStr format_as(DerivationOutput const output)
{
	return output.outPath.to_string();
}

