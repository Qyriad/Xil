// vim: tabstop=4 shiftwidth=4 noexpandtab

#include <algorithm>
#include <cassert>
#include <error.hh>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>

// Nix headers.
#include <nix/config.h> // IWYU pragma: keep
#include <nix/attrs.hh>
#include <nix/attr-path.hh>
#include <nix/attr-set.hh>
#include <nix/canon-path.hh>
#include <nix/eval.hh>
#include <nix/eval-cache.hh>
#include <nix/eval-settings.hh>
#include <nix/flake/flake.hh>
#include <nix/flake/flakeref.hh>
// LockedNode
#include <nix/flake/lockfile.hh>
#include <nix/input-accessor.hh>
#include <nix/installable-flake.hh>
#include <nix/loggers.hh>
#include <nix/nixexpr.hh>
#include <nix/outputs-spec.hh>
#include <nix/path.hh>
#include <nix/search-path.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/util.hh>
#include <nix/value.hh>

#include <argparse/argparse.hpp>
#include <boost/stacktrace.hpp>
#include <fmt/core.h>
#include <cppitertools/itertools.hpp>

#include "nixcompat.h" // IWYU pragma: keep
#include "attriter.hpp"
#include "build.hpp"
#include "flake.hpp"
#include "logging.hh"
#include "xil.hpp"
#include "settings.hpp"

namespace nix::fetchers
{
	extern std::vector<std::shared_ptr<nix::fetchers::InputScheme>> inputSchemes;
}

using fmt::print, fmt::println;
using argparse::ArgumentParser;

nix::Value callPackage(nix::EvalState &state, nix::Value &targetValue)
{
	auto rootPath = state.rootPath(nix::CanonPath::fromCwd());

	// Use the user-specified expression for "call package".
	nix::Expr *callPackageExpr = state.parseExprFromString(CALLPACKAGE_FUN, rootPath);
	nix::Value callPackage = nixEval(state, *callPackageExpr);

	return nixCallFunction(state, callPackage, targetValue);
}

nix::InstallableFlake parseInstallable(
	std::shared_ptr<nix::EvalState> state,
	std::string const &installableSpec,
	InstallableMode installableMode = InstallableMode::BUILD
)
{
	auto [fragmentedFlakeRef, outputsSpec] = nix::ExtendedOutputsSpec::parse(installableSpec);

	auto [flakeRef, fragment] = nix::parseFlakeRefWithFragment(
		std::string(fragmentedFlakeRef),
		nix::CanonPath::fromCwd().c_str()
	);

	auto const thisSystem = nix::settings.thisSystem.get();

	auto defaultFlakeAttrPaths = installableMode.defaultFlakeAttrPaths(thisSystem);
	auto defaultFlakeAttrPrefixes = installableMode.defaultFlakeAttrPrefixes(thisSystem);


	nix::flake::LockFlags flags;
	flags.updateLockFile = false;
	flags.writeLockFile = false;


	auto installableFlake = nix::InstallableFlake(
		nullptr,
		nix::ref{state},
		std::move(flakeRef),
		fragment,
		outputsSpec,
		defaultFlakeAttrPaths,
		defaultFlakeAttrPrefixes,
		flags
	);


	return installableFlake;
}


// FIXME: refactor
void addEvalArguments(ArgumentParser &parser, bool isPrint)
{
	// FIXME: make invertable
	parser.add_argument("--safe")
		.flag()
		.default_value(isPrint)
		.help("catch errors during evaluation");
	parser.add_argument("--short-errors")
		.flag()
		.default_value(isPrint)
		.help("With --safe, elide non-toplevel errors");
	parser.add_argument("--no-autocall")
		.flag()
		.default_value(false)
		.help("Automatically call functions with default arguments");
	parser.add_argument("--short-derivations")
		.choices("always", "never", "auto")
		.default_value(isPrint ? "auto" : "always")
		.nargs(1)
		.help("Print derivations as their drvPaths instead of as attrsets, or only at top-level for auto");
}

void addExprArguments(ArgumentParser &parser)
{
	auto &group = parser.add_mutually_exclusive_group(/* required = */ true);

	group.add_argument("--expr", "-E")
		.nargs(1)
		.metavar("EXPR")
		.help("Evaluate an expression given as a command-line argument");
	group.add_argument("--file", "-F")
		.nargs(1)
		.metavar("FILE")
		.help("Evaluate an expression in the specified file");
	group.add_argument("--flake", "-f")
		.nargs(1)
		.metavar("FLAKEREF")
		.help("Evaluate a flake");

	parser.add_argument("--call-package", "-C")
		.flag()
		.help(fmt::format("Use `{}` to call the target expression", CALLPACKAGE_FUN));

	parser.add_argument("--installable-mode")
		.nargs(1)
		.metavar("MODE")
		.choices("all", "none", "build", "devshell", "app", "check")
		.help("Sets the default attribute prefixes for flake installable fragments");
}

/** Base class for arguments that evaluate Nix expressions in some way. */
struct XilEvaluatorArgs
{
	ArgumentParser &root;
	// The subcommand parser used for this evaluator.
	ArgumentParser &evalParser;

	XilEvaluatorArgs(ArgumentParser &root, ArgumentParser &evalParser) :
		root(root), evalParser(evalParser) { }

	/** Gets the nix::Value from whichever of --expr, --file, and --flake was used.
	Accordingly, this function may throw if the expression fails to evaluate.
	*/
	nix::Value getTargetValue(std::shared_ptr<nix::EvalState> state, InstallableMode installableMode = InstallableMode::ALL) const
	{
		nix::Expr *expr = nullptr;
		nix::Value outValue;

		if (auto const &exprStr = this->evalParser.present("--expr")) {
			std::string const &str = exprStr.value();
			expr = state->parseExprFromString(str, state->rootPath(nix::CanonPath::fromCwd()));
			state->eval(expr, outValue);

		} else if (auto const &exprFile = this->evalParser.present("--file")) {
			auto const canonExprFilePath = nix::CanonPath(exprFile.value(), nix::CanonPath::fromCwd());
			auto const file = state->rootPath(canonExprFilePath);
			expr = state->parseExprFromFile(file);
			state->eval(expr, outValue);

		} else if (auto const &flakeSpec = this->evalParser.present("--flake")) {

			// If we have a flake, then we'll be getting a Value directly, not a nix::Expr.
			nix::InstallableFlake instFlake = parseInstallable(
				state,
				flakeSpec.value(),
				installableMode
			);

			LazyFlake lazyFlake{state, *dynamic_cast<XilLogger *>(nix::logger), std::move(instFlake)};
			outValue = lazyFlake.installableValue(installableMode);

		} else {
			assert("unreachable" == nullptr);
		}

		return outValue;
	}
};

/** Struct for arguments that end up printing the result of a Nix evaluation. */
struct XilPrinterArgs : public XilEvaluatorArgs
{
	ArgumentParser &printCmd;
	ArgumentParser &evalCmd;

	XilPrinterArgs(
		ArgumentParser &rootParser,
		ArgumentParser &evalParser,
		ArgumentParser &printCmd,
		ArgumentParser &evalCmd
	) : XilEvaluatorArgs(rootParser, evalParser), printCmd(printCmd), evalCmd(evalCmd)
	{ }

	bool isPrint() const noexcept
	{
		return this->root.is_subcommand_used(this->printCmd);
	}

	bool safe() const noexcept
	{
		return this->evalParser.get<bool>("--safe") || this->isPrint();
	}

	bool shortErrors() const noexcept
	{
		return this->evalParser.get<bool>("--short-errors") || this->isPrint();
	}
};

struct XilArgs
{
	ArgumentParser parser;

	ArgumentParser evalCmd;
	ArgumentParser printCmd;
	ArgumentParser buildCmd;
	ArgumentParser fetchCmd;

	XilArgs(int argc, char *argv[]) :
		parser(ArgumentParser{"xil"}),
		evalCmd(ArgumentParser{"eval"}),
		printCmd(ArgumentParser{"print"}),
		buildCmd(ArgumentParser{"build"}),
		fetchCmd(ArgumentParser{"fetch"})
	{
		this->parser.add_subparser(this->evalCmd);
		this->evalCmd.add_description("Evaluate a Nix expression and print what it evaluates to");
		addExprArguments(this->evalCmd);
		addEvalArguments(this->evalCmd, false);

		this->parser.add_subparser(this->printCmd);
		this->printCmd.add_description("Alias for eval --safe --short-errors --short-derivations=auto");
		addExprArguments(this->printCmd);
		addEvalArguments(this->printCmd, true);

		this->parser.add_subparser(this->buildCmd);
		this->buildCmd.add_description("Build the derivation evaluated from a Nix expression");
		addExprArguments(this->buildCmd);

		this->parser.add_subparser(this->fetchCmd);
		this->fetchCmd.add_description("Fetch the specified flake");
		this->fetchCmd.add_argument("installable");

		this->parser.parse_args(argc, argv);
	}

	/** Gets the XilArgs, if any, for `eval` or `print`, whichever is used. */
	std::optional<XilPrinterArgs> getPrinterArgs() noexcept
	{
		if (this->parser.is_subcommand_used(this->evalCmd)) {
			return XilPrinterArgs{
				this->parser, // root
				this->evalCmd, // evalParser
				this->printCmd, // printCmd
				this->evalCmd // evalCmd
			};
		} else if (this->parser.is_subcommand_used(this->printCmd)) {
			return XilPrinterArgs{
				this->parser, // root
				this->printCmd, // evalParser
				this->printCmd, //printCmd
				this->evalCmd // evalCmd
			};
		}

		return std::nullopt;
	}

	/** Gets the XilEvaluatorArgs, if any, for `eval`, `print`, or `build`, whichever is used. */
	std::optional<XilEvaluatorArgs> getEvalArgs() noexcept
	{
		if (this->parser.is_subcommand_used(this->evalCmd) || this->parser.is_subcommand_used(this->printCmd)) {
			return this->getPrinterArgs();
		} else if (this->parser.is_subcommand_used(this->buildCmd)) {
			return XilEvaluatorArgs{
				this->parser, // root
				this->buildCmd // evalParsr
			};
		}

		return std::nullopt;
	}
};

int main(int argc, char *argv[])
{
	LOGFILE = std::make_shared<std::fstream>();
	LOGFILE->exceptions(std::ios_base::badbit | std::ios_base::failbit);
	LOGFILE->open("/home/qyriad/.local/var/log/xil.log", std::fstream::out | std::fstream::trunc);

	XilArgs args(argc, argv);

	nix::initNix();
	nix::initGC();

	auto xilLogger = new XilLogger{};
	nix::logger = xilLogger;

	//nix::EvalSettings &settings = nix::evalSettings;
	// FIXME: log IFDs, rather than disallowing them.
	//assert(settings.set("allow-import-from-derivation", "false"));

	// FIXME: --store option
	auto store = nix::openStore();

	// FIXME: allow specifying SearchPath from command line.
	auto xilEl = nix::SearchPath::Elem::parse(fmt::format("xil={}", XILLIB_DIR));
	nix::SearchPath searchPath;
	if (args.parser.is_subcommand_used(args.printCmd)) {
		searchPath = nix::SearchPath{std::list<nix::SearchPath::Elem>{xilEl}};
	} else {
		searchPath = nix::SearchPath{};
	}
	auto state = std::make_shared<nix::EvalState>(searchPath, store, store);

	// Handle eval and print commands.
	if (auto evalArgs_ = args.getPrinterArgs()) {
		// Unwrap the optional.
		auto const &evalArgs = evalArgs_.value();
		auto const &evalParser = evalArgs.evalParser;

		nix::Value rootVal;

		try {
			rootVal = evalArgs.getTargetValue(state);
			if (evalParser.get<bool>("--call-package") && rootVal.isLambda()) {
				rootVal = callPackage(*state, rootVal);
			}
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 1;
		}

		auto const &shortDrvsOpt = evalParser.get<std::string>("--short-derivations");
		// As far as `Printer` is concerned, "auto" is equivalent to "always".
		// If it's "auto", then we'll manually check for a top-level derivation attrset.
		bool const shortDrvs = (shortDrvsOpt == "always") || (shortDrvsOpt == "auto");

		Printer printer(state, evalArgs.safe(), evalArgs.shortErrors(), shortDrvs);

		try {
			if (state->isDerivation(rootVal) && shortDrvsOpt == "auto") {
				// If we're printing this derivation "not-short", then run the attr printer manually.
				printer.printAttrs(rootVal.attrs, std::cout, 0, 0);
			} else {
				// Otherwise print as normal.
				printer.printValue(rootVal, std::cout, 0, 0);
			}
			// Add a trailing newline.
			println("");
		} catch (nix::Interrupted &e) {
			eprintln("Interrupted: {}\n", e.msg());
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 2;
		}
	} else if (args.parser.is_subcommand_used("build")) {
		auto evalArgs = args.getEvalArgs().value();
		try {
			nix::Value rootVal = evalArgs.getTargetValue(state, InstallableMode::BUILD);

			if (args.buildCmd.get<bool>("--call-package")) {
				rootVal = callPackage(*state, rootVal);
			}
			// Use the user specified way of "call package".
			state->forceValue(rootVal, nix::noPos);
			assert(state->isDerivation(rootVal));

			DrvBuilder builder(state, store, rootVal);
			eprintln("Expression evaluated to derivation {}", builder.meta.drvPath);
			builder.realizeDerivations();
		} catch (nix::Interrupted &ex) {
			eprintln("Interrupted: {}", ex.msg());
		} catch (nix::EvalError &ex) {
			eprintln("{}", ex.msg());
			return 3;
		}
	} else if (args.parser.is_subcommand_used("fetch")) {
		auto installableSpec = args.fetchCmd.get<std::string>("installable");
		//eprintln("installable is {}", installableSpec);

		//using nix::flake::LockedFlake, nix::flake::LockFlags, nix::flake::LockedNode;
		//using nix::fetchers::Input, nix::fetchers::InputScheme;

		//nix::InstallableFlake instFlake = parseInstallable(
		//	state,
		//	installableSpec,
		//	InstallableMode::BUILD
		//);
		//LazyFlake lazyFlake{state, *xilLogger, std::move(instFlake)};
		LazyFlake lazyFlake{
			state,
			*xilLogger,
			parseInstallable(state, installableSpec, InstallableMode::BUILD),
		};

		nix::flake::Node &root = *lazyFlake.locked().lockFile.root;
		StdVec<nix::flake::LockedNode> allLockedNodes{};
		//lockedFlake->flake.lockedRef.fetchTree(state->store);
		collectFlakeNodes(allLockedNodes, root);
		eprintln("The following flake store paths will be fetched for {}:", lazyFlake.fullStorePath());
		for (auto const &node : allLockedNodes) {
			auto const &rawStorePath = node.lockedRef.input.computeStorePath(*state->store);
			auto const &storePath = state->store->printStorePath(rawStorePath);
			eprintln("\t{} for {}", storePath, node.lockedRef.to_string());
		}

		for (auto const &node : allLockedNodes) {
			xilLogger->withFetchLabel(node.originalRef.to_string(), [&] {
				node.lockedRef.fetchTree(state->store);
			});
		}

		//StdString flakeLabel{instFlake.flakeRef.to_string()};
		//
		//StdString resolveLabel{"Flake Registry"};
		//nix::FlakeRef resolved = xilLogger->withFetchLabel(resolveLabel, [&]() -> nix::FlakeRef {
		//	return instFlake.flakeRef.resolve(state->store);
		//});
		//
		//LockFlags lockFlags{};
		//lockFlags.updateLockFile = false;
		//lockFlags.writeLockFile = false;
		//
		//Input &input = resolved.input;
		//InputScheme &scheme = *input.scheme;
		//xilLogger->withFetchLabel(flakeLabel, [&]() {
		//	scheme.fetch(state->store, input);
		//});
		//
		//auto lockedFlake = std::make_shared<LockedFlake>(
		//	nix::flake::lockFlake(*state, instFlake.flakeRef, lockFlags)
		//);
		//nix::FlakeRef &lockedRef = lockedFlake->flake.lockedRef;
		//
		//xilLogger->log(nix::lvlInfo,
		//	fmt::format("Locked flake as {}", lockedRef.to_string())
		//);
		//
		//nix::flake::Node &root = *lockedFlake->lockFile.root;
		//lockedFlake->flake.lockedRef.fetchTree(state->store);
		//
		//std::vector<LockedNode> allLockedNodes{};
		//collectFlakeNodes(allLockedNodes, root);
		//
		//eprintln("The following flake store paths will be fetched:");
		//for (auto const &node : allLockedNodes) {
		//	auto rawStorePath = node.lockedRef.input.computeStorePath(*state->store);
		//	auto storePath = state->store->printStorePath(rawStorePath);
		//	auto label = node.lockedRef.to_string();
		//	eprintln("\t{} for {}", storePath, label);
		//}
		//
		//for (auto const &node : allLockedNodes) {
		//	auto rawStorePath = node.lockedRef.input.computeStorePath(*state->store);
		//	auto storePath = state->store->printStorePath(rawStorePath);
		//	auto original = node.originalRef.to_string();
		//	auto locked = node.lockedRef.to_string();
		//	//eprintln("fetching: {} for {} ({})", storePath, original, locked);
		//	//node.lockedRef.input.fetch(state->store);
		//	xilLogger->withFetchLabel(node.originalRef.to_string(), [&]() {
		//		node.lockedRef.fetchTree(state->store);
		//		//input.fetch(state->store);
		//	});
		//}
	}

	return 0;
}
