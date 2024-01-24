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

#include "xil.hpp"

using fmt::print, fmt::println;

int main(int argc, char *argv[])
{
	// Initialize an args vector from the argv range.
	std::vector<std::string> arguments(&argv[0], &argv[argc]);

	nix::initNix();
	nix::initGC();

	nix::EvalSettings &settings = nix::evalSettings;
	assert(settings.set("allow-import-from-derivation", "false"));

	// Then get a reference to the Store.
	auto store = nix::openStore();

	// FIXME: allow specifying SearchPath from command line.
	auto state = std::make_shared<nix::EvalState>(nix::SearchPath{}, store, store);

	auto exprStr = arguments.at(1);

	nix::Expr *fileExpr = state->parseExprFromString(exprStr, state->rootPath(nix::CanonPath::fromCwd()));
	nix::Value rootVal;
	try {
		state->eval(fileExpr, rootVal);
	} catch (nix::EvalError &e) {
		eprintln("{}", e.msg());
		return 1;
	}

	Printer printer(state);

	try {
		printer.printValue(rootVal, std::cout, 0, 0);
	} catch (nix::Interrupted &ex) {
		eprintln("Interrupted: {}", ex.msg());
	}
	print("\n");

	return 0;
}
