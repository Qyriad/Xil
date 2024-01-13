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

#include <fmt/core.h>
#include <fmt/format.h>
#include <boost/core/demangle.hpp>

#include "xil.hpp"

using fmt::print, fmt::println;

bool strIsMultiline(std::string_view sv)
{
	return sv.find("\n") != std::string_view::npos;
}

namespace nix
{
	std::string_view format_as(nix::ValueType const type)
	{
		switch (type) {
			case nix::nThunk:
				return "thunk";
			case nix::nInt:
				return "integer";
			case nix::nFloat:
				return "float";
			case nix::nBool:
				return "bool";
			case nix::nString:
				return "string";
			case nix::nPath:
				return "path";
			case nix::nNull:
				return "null";
			case nix::nAttrs:
				return "attrs";
			case nix::nList:
				return "list";
			case nix::nFunction:
				return "function";
			case nix::nExternal:
				return "external?";
			default:
				return "«unreachable»";
		}
	}
}

void printType(nix::ValueType const &&type, std::ostream &out)
{
	switch (type) {
		case nix::nThunk:
			out << "thunk";
			break;
		case nix::nInt:
			out << "integer";
			break;
		case nix::nFloat:
			out << "float";
			break;
		case nix::nBool:
			out << "bool";
			break;
		case nix::nString:
			out << "string";
			break;
		case nix::nPath:
			out << "path";
			break;
		case nix::nNull:
			out << "null";
			break;
		case nix::nAttrs:
			out << "attrs";
			break;
		case nix::nList:
			out << "list";
			break;
		case nix::nFunction:
			out << "function";
			break;
		case nix::nExternal:
			out << "external?";
			break;
	}
}

std::string printType(nix::ValueType const &&type)
{
	std::stringstream out;
	printType(std::move(type), out);
	return out.str();
}

void printIndent(std::ostream &out, uint32_t indentLevel)
{
	// FIXME: make configurable.
	for (uint32_t i = 0; i < indentLevel * 4; ++i) {
		out << " ";
	}
}

std::string makeIndent(uint32_t level)
{
	std::stringstream ss;
	printIndent(ss, level);
	return ss.str();
}

// Not all expressions have a name.
std::optional<std::string> exprName(nix::Expr *expr, nix::SymbolTable &symbols)
{
	assert(expr != nullptr);

	using OptStr = std::optional<std::string>;

	std::optional<std::string> maybeName = std::visit(overloaded {
		[&](nix::ExprVar *expr) -> OptStr {
			if (expr->name) {
				return std::make_optional(symbols[expr->name]);
			}
			return std::nullopt;
		},
		[&](nix::ExprLambda *expr) -> OptStr {
			if (expr->name) {
				return std::make_optional(symbols[expr->name]);
			}
			return std::nullopt;
		},
		[](nix::ExprCall *) -> OptStr {
			// TODO: should this use the name of the function its calling?
			//return exprName(expr->fun, symbols);
			return std::nullopt;
		},
		[](nix::ExprSelect *) -> OptStr {
			// TODO: should this use the attr path?
			return std::nullopt;
		},
		[](auto *expr) -> OptStr {
			eprintln("exprName(): unhandled expr {}", TYPENAME(*expr));
			return std::nullopt;
		},
	}, ExprT::from(expr));

	// Need to check that the optional has a value and that the Symbol value isn't
	// nothing.
	return maybeName;
}

// Not all values have a name.
std::optional<std::string> valueName(nix::Value &value, nix::SymbolTable &symbols)
{
	switch (value.type()) {
        case nix::nAttrs:
			// TODO: can we do anything here?
			return std::nullopt;
        case nix::nThunk: {
			// TODO: optionally try-force?
			return exprName(value.thunk.expr, symbols);
		}
        case nix::nFunction:
			if (value.isLambda()) {
				return exprName(value.lambda.fun, symbols);
			} else if (value.isPrimOp()) {
				return value.primOp->name;
			} else if (value.isPrimOpApp()) {
				return valueName(*value.primOpApp.left, symbols);
				// FIXME: add primOpApp.right
			}
			return std::nullopt;
		default:
			return std::nullopt;
	}
}

struct Seen
{
	using ValueIndex = uint64_t;
	std::unordered_map<void *, std::optional<std::string>> inner;
	uint64_t maxIndex;

	bool insert(void *value, std::optional<std::string> valueName)
	{
		if (this->inner.contains(value)) {
			return false;
		}
		auto [iterator, inserted] = this->inner.insert(std::make_pair(value, valueName));
		assert(inserted);
		this->maxIndex += 1;
		return inserted;
	}

	bool contains(void *value)
	{
		return this->inner.contains(value);
	}
};

void printValue(nix::Value &value, nix::EvalState &state, std::ostream &out, Seen &seen, uint32_t indentLevel = 0)
{

	// FIXME: arbitrary hardcoded limit
	if (indentLevel > 100) {
		out << "«too deep»";
		return;
	}

	if (value.type() == nix::nThunk) {

		try {
			state.forceValue(value, nix::noPos);
			auto maybeName = valueName(value, state.symbols);
			std::string nameStr = maybeName.has_value() ? fmt::format("{} ", maybeName.value()) : "";
			//print("working on {}{} level {}\n", nameStr, printType(value.type()), indentLevel);
		} catch (nix::ThrownError &ex) {
			out << "«throws»";
			return;
		} catch (nix::EvalError &ex) {
			out << "«eval error»";
			return;
		}
	} else {
		//print("working on {} level {}\n", printType(value.type()), indentLevel);
	}
	switch (value.type()) {
		case nix::nThunk:
			out << "«thunk»";
			break;
		case nix::nInt:
			out << value.integer;
			break;
		case nix::nFloat:
			out << value.fpoint;
			break;
		case nix::nBool:
			nix::printLiteralBool(out, value.boolean);
			break;
		case nix::nString: {
			auto valueSv = value.str();
			bool multiline = valueSv.find("\n") != decltype(valueSv)::npos;
			if (multiline) {
				out << "''\n";
				printIndent(out, indentLevel + 1);
				for (auto charIt = valueSv.begin(); charIt != valueSv.end(); ++charIt) {
					auto ch = *charIt;
					switch (ch) {
						case '\\':
							out << "\\";
							break;
						case '\n': {
							out << "\n";
							uint32_t endIndent = charIt + 1 != valueSv.end() ? indentLevel + 1 : indentLevel;
							printIndent(out, endIndent);
							break;
						}
						case '\r':
							out << "\\r";
							break;
						// FIXME: allow custom size for \t
						default:
							out << ch;
							break;
					}
				}
				out << "''";
			} else {
				nix::printLiteralString(out, valueSv);
			}
			break;
		}
		case nix::nPath:
			out << value.path().to_string();
			break;
		case nix::nNull:
			out << "null";
			break;
		case nix::nAttrs: {
			if (state.isDerivation(value)) {
				out << "«derivation ";
				nix::Bindings::iterator it = value.attrs->find(state.sDrvPath);
				try {
					if (it == value.attrs->end()) {
						out << "«No drvPath»";
					} else {
						state.forceValue(*it->value, nix::noPos);
						assert(it->value->type() == nix::nString);
						out << it->value->str();
					}
				} catch (nix::ThrownError &ex) {
					out << "«THROWS» ";
				}
				out << "»";
			} else {
				nix::Bindings::iterator it = value.attrs->find(state.symbols.create("_type"));
				if (it != value.attrs->end()) {
					if (it->value->type() == nix::nString) {
						if (std::string_view(it->value->string.s) == "pkgs") {
							if (indentLevel > 0) {
								out << "«too deep»";
								return;
							}
						}
					}
				}
				bool empty = value.attrs->size() == 0;
				if (empty) {
					out << "{ }";
					break;
				}

				if (seen.contains(value.attrs) != 0) {
					out << "«attrs repeated»";
					break;
				}
				auto findResult = seen.inner.find(value.attrs);
				if (findResult != seen.inner.end()) {
					auto [_value, name] = *findResult;
					if (name != std::nullopt) {
						out << "«repeated " << name.value() << "»";
					} else {
						out << "«repeated»";
					}
					break;
				}
				seen.insert(value.attrs, valueName(value, state.symbols));

				auto &ssAttrs = out;

				ssAttrs << "{\n";

				for (auto &attrPair : value.attrs->lexicographicOrder(state.symbols)) {
					printIndent(ssAttrs, indentLevel + 1);
					auto attrName = state.symbols[attrPair->name];
					//println("<{}>", static_cast<std::string>(attrName));
					ssAttrs << attrName << " = ";
					if (attrPair->value->type() == nix::nAttrs && attrPair->value->attrs->size() != 0) {
						// FIXME: hardcodes pkgs recursion
						nix::Bindings::iterator it = value.attrs->find(state.symbols.create("_type"));
						if (it != value.attrs->end() && it->value->type() == nix::nString) {
							if (std::string_view(it->value->string.s) == "pkgs") {
								if (indentLevel > 0) {
									out << "«too deep»";
									return;
								}
							}
						}

						// Don't store seen derivations.
						if (!state.isDerivation(*attrPair->value)) {
							// This does nothing if the value is already in the seen set.
							seen.insert(attrPair->value->attrs, attrName);
						}
					}
					try {
						std::flush(out);
						printValue(*attrPair->value, state, ssAttrs, seen, indentLevel + 1);
					} catch (nix::EvalError &ex) {
						// FIXME: more useful message
						ssAttrs << "«eval error»";
					}
					ssAttrs << ";\n";
				}

				printIndent(ssAttrs, indentLevel);
				ssAttrs << "}";

			}
			break;
		}
		case nix::nList: {
			// FIXME: better handling for "short" lists.
			bool empty = value.listSize() == 0;
			if (empty) {
				out << "[ ]";
				break;
			}

			out << "[\n";

			for (auto &listItem : value.listItems()) {
				printIndent(out, indentLevel + 1);
				std::flush(out);
				printValue(*listItem, state, out, seen, indentLevel + 1);
				out << "\n";
			}

			printIndent(out, indentLevel);
			out << "]";

			break;
		}
		case nix::nFunction:
			if (value.isLambda()) {
				out << "«lambda";
				if (value.lambda.fun) {
					if (value.lambda.fun->name) {
						out << " ";
						out << state.symbols[value.lambda.fun->name];
						if (!value.lambda.fun->arg) {
							out << ": … ";
						}
					}
					if (value.lambda.fun->arg) {
						out << ": ";
						out << state.symbols[value.lambda.fun->arg];
					}
				}
				out << "»";
			} else if (value.isPrimOp()) {
				out << "«primop " << value.primOp->name << "»";
			} else if (value.isPrimOpApp()) {
				out << "«primopApp ";
				auto lhsName = valueName(*value.primOpApp.left, state.symbols);
				if (lhsName.has_value()) {
					out << lhsName.value();
				}
				auto rhsName = valueName(*value.primOpApp.right, state.symbols);
				if (rhsName.has_value()) {
					if (lhsName) {
						out << " ";
					}
					out << rhsName.value();
				}
				out << "»";
			} else {
				assert("unreachable" == nullptr);
			}
			break;
		case nix::nExternal:
			out << "«external?»";
			break;
	}
}

int main(int argc, char *argv[])
{
	// Initialize an args vector from the argv range.
	std::vector<std::string> arguments(&argv[0], &argv[argc]);

	nix::initNix();
	nix::initGC();

	nix::EvalSettings &settings = nix::evalSettings;
	auto success = settings.set("allow-import-from-derivation", "false");
	assert(success);

	// Then get a reference to the Store.
	auto store = nix::openStore();

	// FIXME: allow specifying SearchPath from command line.
	//nix::EvalState state(nix::SearchPath{}, store, store);
	auto state = std::make_shared<nix::EvalState>(nix::SearchPath{}, store, store);

	auto exprStr = arguments.at(1);

	nix::Expr *fileExpr = state->parseExprFromString(exprStr, state->rootPath(nix::CanonPath::fromCwd()));
	nix::Value rootVal;
	state->eval(fileExpr, rootVal);

	Seen seenSet;

	Printer printer(state);

	try {
		//printValue(rootVal, state, std::cout, seenSet);
		printer.printValue(rootVal, std::cout, 0);
	} catch (nix::Interrupted &ex) {
		eprintln("Interrupted: {}", ex.msg());
	}
	print("\n");

	return 0;
}
