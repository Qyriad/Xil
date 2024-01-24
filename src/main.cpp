// vim: tabstop=4 shiftwidth=4 noexpandtab

#include <functional>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <typeindex>
#include <utility>
#include <unordered_map>

// Nix headers.
#include <config.h>
#include <shared.hh>
#include <command.hh>
#include <eval.hh>
#include <store-api.hh>
#include <input-accessor.hh>
#include <canon-path.hh>
#include <value-to-xml.hh>
#include <print.hh>
#include <eval-settings.hh>
#include <globals.hh>

#include <fmt/core.h>
#include <fmt/format.h>
#include <boost/core/demangle.hpp>
#include <boost/program_options.hpp>
#include <argparse/argparse.hpp>

#include "xil.hpp"

using fmt::print, fmt::println;
using argparse::ArgumentParser;

void addExprArguments(ArgumentParser &parser)
{
	auto &group = parser.add_mutually_exclusive_group(/* required = */ true);

	group.add_argument("--expr", "-E")
		.nargs(1)
		.help("Evaluate an expression given as a command-line argument");
	group.add_argument("--file", "-F")
		.nargs(1)
		.help("Evaluate an expression in the specified file");
	group.add_argument("--flake", "-f")
		.nargs(1)
		.help("Evaluate a flake (unimplemented)");
}

int main(int argc, char *argv[])
{
	// Initialize an args vector from the argv range.
	std::vector<std::string> arguments(&argv[0], &argv[argc]);

	ArgumentParser parser("xil");
	parser.add_argument("--safe")
		.flag()
		.help("catch errors during evaluation")
	;
	parser.add_argument("--short-errors")
		.flag()
		.help("With --safe, elide non-toplevel errors")
	;

	ArgumentParser evalCmd("eval");
	evalCmd.add_description("Evaluate an Nix expression and print what it evaluates to");
	// Takes eval_command by reference.
	parser.add_subparser(evalCmd);
	addExprArguments(evalCmd);

	ArgumentParser printCmd("print");
	printCmd.add_description("Alias for eval --safe --short-errors");
	parser.add_subparser(printCmd);
	addExprArguments(printCmd);

	try {
		parser.parse_args(argc, argv);
	} catch (std::exception &e) {
		//eprintln("{}\n{}", e.what(), parser);
		std::cerr << e.what() << "\n" << parser;
		return 1;
	}

	nix::initNix();
	nix::initGC();

	nix::EvalSettings &settings = nix::evalSettings;
	assert(settings.set("allow-import-from-derivation", "false"));

	// Then get a reference to the Store.
	auto store = nix::openStore();

	// FIXME: allow specifying SearchPath from command line.
	auto state = std::make_shared<nix::EvalState>(nix::SearchPath{}, store, store);

	auto getEvalParser = [&]() -> std::optional<std::reference_wrapper<ArgumentParser>> {
		if (parser.is_subcommand_used(evalCmd)) {
			return std::make_optional<std::reference_wrapper<ArgumentParser>>(std::ref(evalCmd));
		}
		if (parser.is_subcommand_used(printCmd)) {
			return std::make_optional<std::reference_wrapper<ArgumentParser>>(std::ref(printCmd));
		}
		return std::nullopt;
	};

	if (auto evalParser_ = getEvalParser()) {
		auto const &evalParser = evalParser_.value().get();
		auto const safe = parser.get<bool>("--safe") || parser.is_subcommand_used(printCmd);
		auto const shortErrors = parser.get<bool>("--short-errors") || parser.is_subcommand_used(printCmd);


		nix::Expr *expr = nullptr;

		if (auto exprStr = evalParser.present("--expr")) {
			auto str = exprStr.value();
			expr = state->parseExprFromString(str, state->rootPath(nix::CanonPath::fromCwd()));
		} else if (auto exprFile = evalParser.present("file")) {
			auto file = nix::SourcePath(nix::CanonPath(exprFile.value()));
			expr = state->parseExprFromFile(file);
		} else if (auto exprFlake = evalParser.present("flake")) {
			eprintln("flakes not yet implemented");
			return 2;
		} else {
			assert("unreachable" == nullptr);
		}

		nix::Value rootVal;
		try {
			state->eval(expr, rootVal);
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 1;
		}

		Printer printer(state, safe, shortErrors);

		try {
			printer.printValue(rootVal, std::cout, 0, 0);
		} catch (nix::Interrupted &ex) {
			eprintln("Interrupted: {}", ex.msg());
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 3;
		}
		print("\n");
	}

	//auto exprStr = arguments.at(1);

	return 0;
}
