// vim: tabstop=4 shiftwidth=4 noexpandtab

#include <cassert>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Nix headers.
#include <nix/config.h> // IWYU pragma: keep
#include <nix/canon-path.hh>
#include <nix/eval.hh>
#include <nix/eval-settings.hh>
#include <nix/input-accessor.hh>
#include <nix/nixexpr.hh>
#include <nix/path.hh>
#include <nix/search-path.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/util.hh>
#include <nix/value.hh>

#include <fmt/core.h>
#include <argparse/argparse.hpp>

#include "nixcompat.h"
#include "xil.hpp"
#include "build.hpp"
#include "settings.hpp"

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
		.default_value(isPrint ? "auto" : "never")
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
		.help("Evaluate a flake (unimplemented)");

	parser.add_argument("--call-package", "-C")
		.flag()
		.help(fmt::format("Use `{}` to call the target expression", CALLPACKAGE_FUN));
}

struct XilEvalArgs
{
	ArgumentParser &root;
	ArgumentParser &evalParser;
	ArgumentParser &printCmd;
	ArgumentParser &evalCmd;

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

	/** Gets the nix::Expr from whichever of --expr, --file, and --flake was used. */
	nix::Expr *getTargetExpr(nix::EvalState &state) const
	{
		nix::Expr *expr;

		if (auto const &exprStr = this->evalParser.present("--expr")) {
			std::string const &str = exprStr.value();
			expr = state.parseExprFromString(str, state.rootPath(nix::CanonPath::fromCwd()));
		} else if (auto const &exprFile = this->evalParser.present("--file")) {
			auto const canonExprFilePath = nix::CanonPath(exprFile.value(), nix::CanonPath::fromCwd());
			auto const file = state.rootPath(canonExprFilePath);
			expr = state.parseExprFromFile(file);
		} else if (auto const &exprFlake = this->evalParser.present("--flake")) {
			eprintln("flakes not yet implemented");
			abort();
		} else {
			assert("unreachable" == nullptr);
		}

		return expr;
	}
};

struct XilArgs
{
	ArgumentParser parser;

	ArgumentParser evalCmd;
	ArgumentParser printCmd;
	ArgumentParser buildCmd;

	XilArgs(int argc, char *argv[]) :
		parser(ArgumentParser{"xil"}),
		evalCmd(ArgumentParser{"eval"}),
		printCmd(ArgumentParser{"print"}),
		buildCmd(ArgumentParser{"build"})
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

		this->parser.parse_args(argc, argv);
	}

	/** Gets the XilArgs, if any, for `eval` or `print`, whichever is used. */
	std::optional<XilEvalArgs> getEvalArgs() noexcept
	{
		if (this->parser.is_subcommand_used(this->evalCmd)) {
			return XilEvalArgs{
				.root = this->parser,
				.evalParser = this->evalCmd,
				.printCmd = this->printCmd,
				.evalCmd = this->evalCmd,
			};
		} else if (this->parser.is_subcommand_used(this->printCmd)) {
			return XilEvalArgs{
				.root = this->parser,
				.evalParser = this->printCmd,
				.printCmd = this->printCmd,
				.evalCmd = this->evalCmd,
			};
		}

		return std::nullopt;
	}

	/** Gets the nix::Expr from whichever of --expr, --file, and --flake was used. */
	nix::Expr *getTargetExpr(ArgumentParser const &evalParser, nix::EvalState &state) const
	{
		nix::Expr *expr;

		if (auto const &exprStr = evalParser.present("--expr")) {
			std::string const &str = exprStr.value();
			expr = state.parseExprFromString(str, state.rootPath(nix::CanonPath::fromCwd()));
		} else if (auto const &exprFile = evalParser.present("--file")) {
			auto const canonExprFilePath = nix::CanonPath(exprFile.value(), nix::CanonPath::fromCwd());
			auto const file = state.rootPath(canonExprFilePath);
			expr = state.parseExprFromFile(file);
		} else if (auto const &exprFlake = evalParser.present("--flake")) {
			eprintln("flakes not yet implemented");
			abort();
		} else {
			assert("unreachable" == nullptr);
		}

		return expr;
	}
};

int main(int argc, char *argv[])
{
	XilArgs args(argc, argv);

	nix::initNix();
	nix::initGC();

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
	if (auto evalArgs_ = args.getEvalArgs()) {
		// Unwrap the optional.
		auto const &evalArgs = evalArgs_.value();
		auto const &evalParser = evalArgs.evalParser;

		nix::Expr *expr = evalArgs.getTargetExpr(*state);

		nix::Value rootVal;
		try {
			state->eval(expr, rootVal);
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
			eprintln("Interrupted: {}", e.msg());
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 2;
		}
	} else if (args.parser.is_subcommand_used("build")) {
		nix::Expr *expr = args.getTargetExpr(args.buildCmd, *state);
		nix::Value rootVal;
		try {
			state->eval(expr, rootVal);

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
	}

	return 0;
}
