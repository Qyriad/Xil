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

#include "xil.hpp"
#include "build.hpp"
#include "settings.hpp"

using fmt::print, fmt::println;
using argparse::ArgumentParser;

nix::Expr *getTargetExpr(ArgumentParser const &parser, nix::EvalState &state)
{
	nix::Expr *expr;

	if (auto exprStr = parser.present("--expr")) {
		auto str = exprStr.value();
		expr = state.parseExprFromString(str, state.rootPath(nix::CanonPath::fromCwd()));
	} else if (auto exprFile = parser.present("file")) {
		auto file = nix::SourcePath(nix::CanonPath(exprFile.value(), nix::CanonPath::fromCwd()));
		expr = state.parseExprFromFile(file);
	} else if (auto exprFlake = parser.present("flake")) {
		eprintln("flakes not yet implemented");
		abort();
	} else {
		assert("unreachable" == nullptr);
	}

	return expr;
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

int main(int argc, char *argv[])
{
	// Initialize an args vector from the argv range.
	std::vector<std::string> arguments(&argv[0], &argv[argc]);

	ArgumentParser parser("xil");

	ArgumentParser evalCmd("eval");
	evalCmd.add_description("Evaluate an Nix expression and print what it evaluates to");
	// Takes eval_command by reference.
	parser.add_subparser(evalCmd);
	addExprArguments(evalCmd);
	addEvalArguments(evalCmd, false);

	ArgumentParser printCmd("print");
	printCmd.add_description("Alias for eval --safe --short-errors --short-derivations=auto");
	parser.add_subparser(printCmd);
	addExprArguments(printCmd);
	addEvalArguments(printCmd, true);

	ArgumentParser buildCmd("build");
	buildCmd.add_description("Build the derivation evaluated from a Nix expression");
	parser.add_subparser(buildCmd);
	addExprArguments(buildCmd);

	try {
		parser.parse_args(argc, argv);
	} catch (std::exception &e) {
		eprintln("{}\n{}", e.what(), parser);
		return 1;
	}

	nix::initNix();
	nix::initGC();

	nix::EvalSettings &settings = nix::evalSettings;
	assert(settings.set("allow-import-from-derivation", "false"));

	// Then get a reference to the Store. FIXME: --store option
	auto store = nix::openStore();

	// FIXME: allow specifying SearchPath from command line.
	auto state = std::make_shared<nix::EvalState>(nix::SearchPath{}, store, store);

	auto getEvalParser = [&]() -> std::optional<std::reference_wrapper<ArgumentParser>> {
		if (parser.is_subcommand_used(evalCmd)) {
			return std::make_optional(std::ref(evalCmd));
		}
		if (parser.is_subcommand_used(printCmd)) {
			return std::make_optional(std::ref(printCmd));
		}
		return std::nullopt;
	};

	if (auto evalParser_ = getEvalParser()) {
		auto const &evalParser = evalParser_.value().get();
		auto const safe = evalParser.get<bool>("--safe") || parser.is_subcommand_used(printCmd);
		auto const shortErrors = evalParser.get<bool>("--short-errors") || parser.is_subcommand_used(printCmd);
		auto shortDrvsOpt = evalParser.get<std::string>("--short-derivations");
		bool shortDrvs = (shortDrvsOpt == "always") || (shortDrvsOpt == "auto");

		nix::Expr *expr = getTargetExpr(evalParser, *state);

		nix::Value rootVal;
		try {
			state->eval(expr, rootVal);
			if (evalParser.get<bool>("--call-package") && rootVal.isLambda()) {
				auto rootPath = state->rootPath(nix::CanonPath::fromCwd());

				// Use the user specified way of "call package".
				auto *callPackageExpr = state->parseExprFromString(CALLPACKAGE_FUN, rootPath);
				auto callPackage = nixEval(*state, *callPackageExpr);
				nix::Value callPackageResult;
				state->callFunction(callPackage, rootVal, callPackageResult, nix::noPos);
				rootVal = callPackageResult;
			}
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 1;
		}

		Printer printer(state, safe, shortErrors, shortDrvs);

		try {
			if (state->isDerivation(rootVal) && shortDrvsOpt == "auto") {
				printer.printAttrs(rootVal.attrs, std::cout, 0, 0);
			} else {
				printer.printValue(rootVal, std::cout, 0, 0);
			}
		} catch (nix::Interrupted &ex) {
			eprintln("Interrupted: {}", ex.msg());
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 3;
		}
		print("\n");
	} else if (parser.is_subcommand_used("build")) {
		nix::Expr *expr = getTargetExpr(buildCmd, *state);
		nix::Value rootVal;
		try {
			state->eval(expr, rootVal);

			// Use the user specified way of "call package".
			auto rootPath = state->rootPath(nix::CanonPath::fromCwd());
			auto *callPackageExpr = state->parseExprFromString(CALLPACKAGE_FUN, rootPath);
			auto callPackage = nixEval(*state, *callPackageExpr);
			nix::Value callPackageResult;
			state->callFunction(callPackage, rootVal, callPackageResult, nix::noPos);
			rootVal = callPackageResult;
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
