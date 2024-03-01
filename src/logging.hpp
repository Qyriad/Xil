#pragma once

#include <memory>

#include <nix/config.h>
#include <nix/error.hh>
#include <nix/logging.hh>

#include <fmt/format.h>

#include "std/map.hpp"
#include "std/optional.hpp"
#include "std/string.hpp"
#include "std/string_view.hpp"

/// Formats a number of bytes to a human-friendly string.
StdString humanBytes(uint64_t const bytes);

/// Returns a string with the necessary ANSI terminal escape codes
/// to set the foreground color to that log level's color.
StdString logLevelToAnsiColor(nix::Verbosity level);

/// Human-readable name for a nix::Verbosity.
StdString logLevelName(nix::Verbosity level);

/// nix::Verbosity to human-readable name with its color.
StdString logLevelToAnsi(nix::Verbosity level);

StdStr const actTypeToString(nix::ActivityType activityType);
StdStr const actTypePretty(nix::ActivityType activityType);
StdStr const resultTypeToString(nix::ResultType resultType);

namespace nix
{
	std::string format_as(nix::Logger::Field const field) noexcept;
}

struct LogActivity
{
	nix::ActivityId id{0};
	nix::ActivityType type{nix::actUnknown};

	constexpr LogActivity(nix::ActivityId id, nix::ActivityType type) : id(id), type(type)
	{ }
	constexpr virtual ~LogActivity() = default;

	virtual StdString format() const;
};

struct ActFileTransfer : public LogActivity
{
	using Self = ActFileTransfer;

	static constexpr nix::ActivityType const TYPE{nix::actFileTransfer};

	//nix::ActivityId id;
	StdString uri;
	StdOpt<StdString> label{std::nullopt};

	static Self from(nix::ActivityId id, nix::Logger::Fields const &fields);

	ActFileTransfer() = delete;
	virtual ~ActFileTransfer() = default;

	StdString format() const override;
	//virtual nix::ActivityType type() const override;

protected:
	// This is constructor is private so I can do input validation before setting fields,
	// without calling each field's default constructor in the process.
	ActFileTransfer(nix::ActivityId id, StdString &&uri) :
		LogActivity(id, Self::TYPE), uri(uri)
	{ }
};

// CopyPath -> ActCopySinglePath
struct ActCopyPaths : public LogActivity
{
	using Self = ActCopyPaths;

	static constexpr nix::ActivityType const TYPE{nix::actCopyPaths};

	StdOpt<StdString> label{std::nullopt};

	static Self from(nix::ActivityId id, nix::Logger::Fields const &fields);

	ActCopyPaths() = delete;
	virtual ~ActCopyPaths() = default;

	StdString format() const override;

protected:
	// This is constructor is private so I can do input validation before setting fields,
	// without calling each field's default constructor in the process.
	ActCopyPaths(nix::ActivityId id) : LogActivity(id, Self::TYPE)
	{ }
};

struct ActCopySinglePath : public LogActivity
{
	using Self = ActCopySinglePath;

	static constexpr nix::ActivityType const TYPE{nix::actCopyPath};

	StdString sourceUri;
	StdString destUri;
	// This is the path *of the Nix store*, not the StorePath being copied.
	StdString storePath;
	StdOpt<StdString> label{std::nullopt};

	static Self from(nix::ActivityId, nix::Logger::Fields const &fields);

	ActCopySinglePath() = delete;
	virtual ~ActCopySinglePath() = default;

	StdString format() const override;

protected:
	ActCopySinglePath(
		nix::ActivityId id,
		StdString sourceUri,
		StdString destUri,
		StdString storePath
	) : LogActivity(id, Self::TYPE), sourceUri(sourceUri), destUri(destUri), storePath(storePath)
	{ }
};

struct LogResult
{
	static constexpr nix::ResultType const TYPE{static_cast<nix::ResultType>(0)};

	virtual StdString format() const;
};

struct ResProgress : public LogResult
{
	using Self = ResProgress;

	static constexpr nix::ResultType const TYPE{nix::resProgress};

	uint64_t done{0};
	uint64_t expected{0};
	uint64_t running{0};
	uint64_t failed{0};

	ResProgress() = default;

	explicit ResProgress(nix::Logger::Fields const &fields);

	bool operator==(Self const &rhs) const;

	StdString format() const override;
};

//struct ResSetExpected : public LogResult
//{
//	using Self = ResSetExpected;
//
//	static constexpr nix::ResultType const TYPE{nix::resSetExpected};
//
//	uint64_t expected{0};
//
//	ResSetExpected() = delete;
//
//	explicit ResSetExpected(nix::Logger::Fields const &fields);
//
//	bool operator==(Self const &rhs) const;
//
//	StdString format() const override;
//};

struct UnexpectedFetch : public std::logic_error
{
	nix::ActivityId act;
	StdString uri;

	UnexpectedFetch() = delete;

	UnexpectedFetch(nix::ActivityId act, StdString uri) :
		std::logic_error(fmt::format("Unexpected fetch of URI {}", uri)),
		act(act),
		uri(uri)
	{ }
};

struct XilLogger;

struct LoggerLabelGuard
{
	XilLogger &logger;

	LoggerLabelGuard(XilLogger &logger) : logger(logger) { }
	~LoggerLabelGuard();
};

struct XilLogger : public nix::Logger
{
#pragma region Types
	using Clock = std::chrono::system_clock;
	using Time = std::chrono::time_point<Clock>;
	using Duration = std::chrono::duration<double>;

#pragma region Statics
	static constexpr Duration const resultUpdateThreshold{std::chrono::milliseconds{1}};

#pragma region Fields
	std::list<StdStr> preparedUriLabels;
	StdMap<nix::ActivityId, std::shared_ptr<LogActivity>> activities;
	Time prevResultUpdate{Clock::now()};
	nix::ResultType lastResultType{static_cast<nix::ResultType>(0)};
	ResProgress lastProgress{};

#pragma region Methods
	template <typename CallableT>
	auto withFetchLabel(StdStr label, CallableT &&callable) -> decltype(callable())
	{
		this->preparedUriLabels.push_front(label);
		LoggerLabelGuard guard{*this};
		return callable();
	}

#pragma region Overrides

	//void stop() override;
	//void pause() override;
	//void resume() override;
	bool isVerbose() override;

	void log(nix::Verbosity lvl, std::string_view msg) override;

	void logEI(nix::ErrorInfo const &ei) override;

	void warn(std::string const &msg) override;

	void startActivity(
		nix::ActivityId act,
		nix::Verbosity lvl,
		nix::ActivityType type,
		std::string const &s,
		Fields const &fields,
		nix::ActivityId parent
	) override;

	void stopActivity(nix::ActivityId act) override;

	void result(nix::ActivityId act, nix::ResultType type, Fields const &fields) override;

	//void writeToStdout(std::string_view s) override;

	//std::optional<char> ask(std::string_view s) override;

	void setPrintBuildLogs(bool printBuildLogs) override;
};
