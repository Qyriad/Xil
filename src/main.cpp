// vim: tabstop=4 shiftwidth=4 noexpandtab

#include <string>
#include <sstream>
#include <utility>

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

#include <fmt/core.h>

using std::string;

using fmt::print, fmt::println;

using nix::SearchPath, nix::EvalState, nix::Expr;

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
		case nix::nFunction:
			out << "function";
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

void printValue(nix::Value &value, nix::EvalState &state, std::ostream &out, uint32_t indentLevel = 0)
{
	//print("working on {} level {}\n", printType(value.type()), indentLevel);
	if (value.type() == nix::nThunk) {
		state.forceValue(value, nix::noPos);
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
			//nix::printLiteralString(out, value.str());
			auto valueSv = value.str();
			//bool hasNewLines = std::find(value_sv.begin(), value_sv.end(), std::string_view("\n"));
			bool multiline = valueSv.find("\n") != decltype(valueSv)::npos;
			if (multiline) {
				out << "''\n";
				printIndent(out, indentLevel + 1);
				for (auto charIt = valueSv.begin(); charIt != valueSv.end(); ++charIt) {
					auto ch = *charIt;
					switch (ch) {
						//case '\"':
						//	[[fallthrough]];
						case '\\':
							out << "\\" << ch;
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
				//printIndent(out, indentLevel == 0 ? indentLevel : indentLevel - 1);
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
				auto it = value.attrs->find(state.sDrvPath);
				state.forceValue(*it->value, nix::noPos);
				printValue(*it->value, state, out);
				//printType(it->value->type(), out);
				out << "»";
			} else {
				bool shortDisplay = value.attrs->size() > 4 ? false : true;
				if (shortDisplay) {
					out << "{ ";
					for (auto &attrPair : value.attrs->lexicographicOrder(state.symbols)) {
						out << state.symbols[attrPair->name] << " = ";
						printValue(*attrPair->value, state, out);
						out << "; ";
					}
					out << "}";
				} else {
					out << "{\n";
					for (auto &attrPair : value.attrs->lexicographicOrder(state.symbols)) {
						// FIXME: make configurable
						printIndent(out, indentLevel + 1);
						out << state.symbols[attrPair->name] << " = ";
						// FIXME
						if (indentLevel > 2) {
							out << "«CODE»";
						} else {
							printValue(*attrPair->value, state, out, indentLevel + 1);
						}
						out << ";\n";
					}
					printIndent(out, indentLevel);
					out << "}";
				}
				//out << "«" << value.attrs->size() << " attrs»";
			}
			break;
		}
		case nix::nList: {
			if (!value.isList()) {
				println("not list but {}", printType(value.type()));
			}
			bool shortDisplay = value.listSize() > 1 ? false : true;
			if (shortDisplay) {
				out << "[ ";
				for (auto *listItem : value.listItems()) {
					printValue(*listItem, state, out);
					out << " ";
				}
				out << "]";
			} else {
				out << "[\n";
				for (auto *listItem : value.listItems()) {
					printIndent(out, indentLevel + 1);
					printValue(*listItem, state, out);
					out << "\n";
				}
				printIndent(out, indentLevel);
				out << "]";
			}
			break;
		}
		case nix::nFunction:
		case nix::nExternal:
			break;
	}
}

int main(int argc, char *argv[])
{
	//print("Hello, world!\n");

	nix::initNix();
	nix::initGC();

	// Then get a reference to the Store.
	auto store = nix::openStore();

	// FIXME: allow specifying SearchPath from command line.
	EvalState state(SearchPath{}, store, store);

	//string exprStr = fmt::fmt("let pkgs = import <nixpkgs> {{ }}; in pkgs.cmake.drvAttrs");
	string exprStr = std::string(argv[1]);

	Expr *fileExpr = state.parseExprFromString(exprStr, state.rootPath(nix::CanonPath::fromCwd()));
	nix::Value rootVal;
	state.eval(fileExpr, rootVal);

	std::stringstream ss;

	printValue(rootVal, state, ss);


	nix::NixStringContext context;

	print("{}\n", ss.str());

	return 0;
}
