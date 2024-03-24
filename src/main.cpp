// vim: tabstop=4 shiftwidth=4 noexpandtab

#include <cassert>
#include <iostream>
#include <memory>

// Nix headers.
#include <nix/config.h> // IWYU pragma: keep
#include <nix/attr-path.hh>
#include <nix/attr-set.hh>
#include <nix/canon-path.hh>
#include <nix/eval.hh>
#include <nix/eval-cache.hh>
#include <nix/eval-settings.hh>
#include <nix/flake/flake.hh>
#include <nix/flake/flakeref.hh>
#include <nix/input-accessor.hh>
#include <nix/installable-flake.hh>
#include <nix/nixexpr.hh>
#include <nix/outputs-spec.hh>
#include <nix/path.hh>
#include <nix/search-path.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/util.hh>
#include <nix/value.hh>

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <cppitertools/itertools.hpp>

#include "std/list.hpp"
#include "std/optional.hpp"
#include "std/string.hpp"
#include "std/string_view.hpp"
#include "std/vector.hpp"

#include "nixcompat.h" // IWYU pragma: keep
#include "attriter.hpp"
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

nix::InstallableFlake parseInstallable(
	nix::EvalState &state,
	StdString const &installableSpec,
	InstallableMode installableMode = InstallableMode::BUILD
)
{
	auto [fragmentedFlakeRef, outputsSpec] = nix::ExtendedOutputsSpec::parse(installableSpec);

	auto [flakeRef, fragment] = nix::parseFlakeRefWithFragment(
		StdString(fragmentedFlakeRef),
		nix::CanonPath::fromCwd().c_str()
	);

	auto const thisSystem = nix::settings.thisSystem.get();

	auto defaultFlakeAttrPaths = installableMode.defaultFlakeAttrPaths(thisSystem);
	auto defaultFlakeAttrPrefixes = installableMode.defaultFlakeAttrPrefixes(thisSystem);


	nix::flake::LockFlags flags;
	flags.updateLockFile = false;
	flags.writeLockFile = false;

	// We need to make a new nix::EvalState for this to be at all sound >.>
	auto statePtr = nix::ref(std::make_shared<nix::EvalState>(
		state.getSearchPath(),
		state.store
	));

	auto installableFlake = nix::InstallableFlake(
		nullptr,
		statePtr,
		std::move(flakeRef),
		fragment,
		outputsSpec,
		defaultFlakeAttrPaths,
		defaultFlakeAttrPrefixes,
		flags
	);

	eprintln("Returning installable {}", installableFlake.what());

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
	nix::Value getTargetValue(nix::EvalState &state, InstallableMode installableMode = InstallableMode::ALL) const
	{
		nix::Expr *expr = nullptr;
		nix::Value outValue;

		if (auto const &exprStr = this->evalParser.present("--expr")) {
			StdString const &str = exprStr.value();
			expr = state.parseExprFromString(str, state.rootPath(nix::CanonPath::fromCwd()));
			state.eval(expr, outValue);

		} else if (auto const &exprFile = this->evalParser.present("--file")) {
			auto const canonExprFilePath = nix::CanonPath(exprFile.value(), nix::CanonPath::fromCwd());
			auto const file = state.rootPath(canonExprFilePath);
			expr = state.parseExprFromFile(file);
			state.eval(expr, outValue);

		} else if (auto const &flakeSpec = this->evalParser.present("--flake")) {

			// If we have a flake, then we'll be getting a Value directly, not a nix::Expr.
			nix::InstallableFlake instFlake = parseInstallable(
				state,
				flakeSpec.value(),
				installableMode
			);

			using nix::flake::LockedFlake;
			using nix::flake::LockFlags;

			// First we need to lock the flake, or Nix will complain.
			auto const lockedFlake = std::make_shared<LockedFlake>(
				// FIXME: CLI does not allow changing lock flags.
				nix::flake::lockFlake(state, instFlake.flakeRef, LockFlags{})
			);

			// We also can only do most things through the eval cache, so let's open that.
			auto evalCache = nix::openEvalCache(state, lockedFlake);
			auto attrCursor = evalCache->getRoot();
			auto asValue = attrCursor->forceValue();

			// Now let's work on the installable fragment part.
			// For each possible attrpath the fragment could refer to,
			// we'll check if it actually exists, and use it if it does.
			bool found = false;
			StdVec<StdString> const requestedAttrPaths = instFlake.getActualAttrPaths();
			for (StdString const &requestedPath : requestedAttrPaths) {

				// This is a bit of a hack.
				if (requestedPath.empty()) {
					found = true;
					outValue = asValue;
					break;
				}

				// Get the requested attr path from the installable fragment as Symbols,
				// and then convert them to string_views.
				auto const symToSv = [&](nix::Symbol const &sym) {
					return static_cast<StdStr>(state.symbols[sym]);
				};
				StdVec<nix::Symbol> const parsedAttrPath = nix::parseAttrPath(state, requestedPath);
				auto const range = std::ranges::transform_view(parsedAttrPath, symToSv);
				StdVec<StdStr> attrPathParts{range.begin(), range.end()};

				assert(asValue.type() == nix::nAttrs);

				AttrIterable currentAttrs{asValue.attrs, state.symbols};
				OptionalRef<nix::Attr> result = currentAttrs.find_by_nested_key(state, attrPathParts);
				if (result.has_value()) {
					found = true;
					outValue = *result->get().value;
				}
			}

			if (!found) {
				auto msg = fmt::format(
					"flake '{}' does not provide any of {}",
					instFlake.what(),
					fmt::join(requestedAttrPaths, ", ")
				);
				throw nix::EvalError(msg);
			}
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
	ArgumentParser posCmd;
	ArgumentParser buildCmd;

	XilArgs(int argc, char *argv[]) :
		parser(ArgumentParser{"xil"}),
		evalCmd(ArgumentParser{"eval"}),
		printCmd(ArgumentParser{"print"}),
		posCmd(ArgumentParser{"pos"}),
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

		this->parser.add_subparser(this->posCmd);
		this->printCmd.add_description("Print the source position a function or derivation is defined at");
		addExprArguments(this->posCmd);
		addEvalArguments(this->posCmd, false);		


		this->parser.add_subparser(this->buildCmd);
		this->buildCmd.add_description("Build the derivation evaluated from a Nix expression");
		addExprArguments(this->buildCmd);

		this->parser.parse_args(argc, argv);
	}

	/** Gets the XilArgs, if any, for `eval` or `print`, whichever is used. */
	StdOpt<XilPrinterArgs> getPrinterArgs() noexcept
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
		} else if (this->parser.is_subcommand_used(this->posCmd)) {
			return XilPrinterArgs{
				this->parser, // root
				this->posCmd, // evalParser
				this->printCmd, //printCmd
				this->evalCmd // evalCmd
			};
		}

		return std::nullopt;
	}

	/** Gets the XilEvaluatorArgs, if any, for `eval`, `print`, or `build`, whichever is used. */
	StdOpt<XilEvaluatorArgs> getEvalArgs() noexcept
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

void describeLambdaPos(std::shared_ptr<nix::EvalState> state, nix::Value & lambdaVal)
{
	if (lambdaVal.isLambda()) {
		state->forceValue(lambdaVal, nix::noPos);
		auto lambdaData = lambdaVal.lambda.fun;
		auto lambdaPos = state->positions[lambdaData->pos];
		auto lambdaName = state->symbols[lambdaData->name];
		// it seems that "<<" is the only interface Pos provides
		// for printing itself.
		std::cout << "lambda '" << lambdaName << "' defined at: " << lambdaPos << std::endl;
	} else {
		eprintln("not a lambda: {}", lambdaVal.type());
	}
}

void describePos(std::shared_ptr<nix::EvalState> state, nix::Value & rootVal)
{
	using nix::Value;
	if (state->isDerivation(rootVal)) {
		eprintln("is derivation");
		// EvalState contains a few constant symbols for easy access,
		// "meta" is one of them
		auto metaAttr = rootVal.attrs->get(state->sMeta);
		if (metaAttr != NULL) {
			eprintln("derivation has meta");
			auto metaVal = metaAttr->value;
			// no constant symbol for position, so we have to make
			// it manually.
			auto sPosition = state->symbols.create("position");
			//eprintln("key is: {}", sPosition);
			state->forceValue(*metaVal, nix::noPos);
			if (metaVal->type() == nix::nAttrs) {
				auto posAttr = metaVal->attrs->get(sPosition);
				if (posAttr != NULL) {
					auto posVal = posAttr->value;
					state->forceValue(*posVal, nix::noPos);
					if (posVal->type() == nix::ValueType::nString) {
						println("package defined at: {}", posVal->str());
					}
				} else {
					eprintln("meta does not have position");
				}
			} else {
				eprintln("meta is not attrs: {}", metaVal->type());
			}
		}
	}
	describeLambdaPos(state, rootVal);
	if (rootVal.type() == nix::ValueType::nAttrs) {
		auto functorAttr = rootVal.attrs->get(state->sFunctor);
		if (functorAttr != NULL) {
			eprintln("has functor");
			auto functorVal = functorAttr->value;
			describeLambdaPos(state, *functorVal);
		}
	}
}

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
	nix::SearchPath searchPath{std::list<nix::SearchPath::Elem>{xilEl}};
	auto state = std::make_shared<nix::EvalState>(searchPath, store, store);

	// Handle eval and print commands.
	if (auto evalArgs_ = args.getPrinterArgs()) {
		// Unwrap the optional.
		auto const &evalArgs = evalArgs_.value();
		auto const &evalParser = evalArgs.evalParser;

		nix::Value rootVal;

		try {
			rootVal = evalArgs.getTargetValue(*state);
			if (evalParser.get<bool>("--call-package") && rootVal.isLambda()) {
				rootVal = callPackage(*state, rootVal);
			}
		} catch (nix::EvalError &e) {
			eprintln("{}", e.msg());
			return 1;
		}

		auto const &shortDrvsOpt = evalParser.get<StdString>("--short-derivations");
		// As far as `Printer` is concerned, "auto" is equivalent to "always".
		// If it's "auto", then we'll manually check for a top-level derivation attrset.
		bool const shortDrvs = (shortDrvsOpt == "always") || (shortDrvsOpt == "auto");

		Printer printer(state, evalArgs.safe(), evalArgs.shortErrors(), shortDrvs);

		try {
			if (args.parser.is_subcommand_used(args.posCmd)) {
				describePos(state, rootVal);
			} else if (state->isDerivation(rootVal) && shortDrvsOpt == "auto") {
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
			nix::Value rootVal = evalArgs.getTargetValue(*state, InstallableMode::BUILD);

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
