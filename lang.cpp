#include "lang.h"

#include <cctype>
#include <iostream>
#include <stack>
#include <unordered_set>

const int kIdMaxLength = 255;
const std::vector<std::string> keywords = {
    "set", "if", "for", "block", "return", "function", "global",
};
const std::vector<std::string> builtinFunctions = {
    "+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "||", "&&", "!", "scan",
    "print", "array.create", "array.get", "array.set",
};

static bool isTruthy(Value *value) {
  auto *iv = dynamic_cast<IntValue *>(value);
  return iv != nullptr && iv->value != 0;
}

ArrayValue::ArrayValue(int length) {
  if (length > 1000000) throw RuntimeError("Out of memory");
  contents = new int[length];
}

Value *VariableSet::getOrThrow(const std::string &name) {
  if (values.count(name) == 0) {
    throw RuntimeError("Use of undefined variable: " + name);
  }
  if (values[name] == nullptr) {
    throw RuntimeError("Use of uninitialized variable: " + name);
  }
  return values[name];
}

struct Context {
  VariableSet globalVars;
  std::stack<VariableSet> callStack;
  Program *program;

  VariableSet &currentFrame() { return callStack.top(); }
  Value *getOrThrow(const std::string &name) {
    if (globalVars.values.count(name) > 0) {
      return globalVars.values[name];
    }
    return currentFrame().getOrThrow(name);
  }
  void set(const std::string &name, Value *value) {
    if (globalVars.values.count(name) > 0) {
      globalVars.values[name] = value;
    } else {
      currentFrame().values[name] = value;
    }
  }
};


std::string IntegerLiteral::toString() const { return std::to_string(value); }
Value *IntegerLiteral::eval(Context &ctx) const { return new IntValue(value); }

std::string Variable::toString() const { return name; }
Value *Variable::eval(Context &ctx) const { return ctx.getOrThrow(name); }

struct ReturnFromCall {
  Value *value;
};
std::string CallExpression::toString() const {
  std::string str = std::string("(") + func;
  for (const auto &arg : args) {
    str += " ";
    str += arg->toString();
  }
  str += ")";
  return str;
}
Value *CallExpression::eval(Context &ctx) const {
  std::vector<Value *> argValues;
  for (const auto &arg : args) {
    argValues.push_back(arg->eval(ctx));
  }

  auto requireArity = [&](int arity) {
    if (args.size() != arity) {
      throw RuntimeError("Function arity mismatch at " + func);
    }
  };
  auto readInt = [&](int ix) {
    auto *iv = dynamic_cast<IntValue *>(argValues[ix]);
    if (!iv) throw RuntimeError("Type error: int expected");
    return iv->value;
  };

  if (func == "+") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x + y);
  } else if (func == "-") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x - y);
  } else if (func == "*") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x * y);
  } else if (func == "/") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    if (y == 0) {
      throw RuntimeError("Divide by zero");
    }
    return new IntValue(x / y);
  } else if (func == "%") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    if (y == 0) {
      throw RuntimeError("Mod by zero");
    }
    return new IntValue(x % y);
  } else if (func == "<") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x < y);
  } else if (func == ">") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x > y);
  } else if (func == "<=") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x <= y);
  } else if (func == ">=") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x >= y);
  } else if (func == "==") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x == y);
  } else if (func == "!=") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x != y);
  } else if (func == "||") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x || y);
  } else if (func == "&&") {
    requireArity(2);
    int x = readInt(0), y = readInt(1);
    return new IntValue(x && y);
  } else if (func == "!") {
    requireArity(1);
    int x = readInt(0);
    return new IntValue(!x);
  } else if (func == "scan") {
    requireArity(0);
    int x;
    std::cin >> x;
    return new IntValue(x);
  } else if (func == "print") {
    requireArity(1);
    std::cout << readInt(0) << '\n';
    return new IntValue(0);
  } else if (func == "array.create") {
    requireArity(1);
    return new ArrayValue(readInt(0));
  } else if (func == "array.get") {
    requireArity(2);
    auto *array = dynamic_cast<ArrayValue *>(argValues[0]);
    int index = readInt(1);
    if (!array) throw RuntimeError("Type error at array.get: array expected");
    if (index >= array->length || index < 0) {
      throw RuntimeError("Index out of bounds at array.get");
    }
    return new IntValue(array->contents[index]);
  } else if (func == "array.set") {
    requireArity(3);
    auto *array = dynamic_cast<ArrayValue *>(argValues[0]);
    int index = readInt(1);
    int value = readInt(2);
    if (!array) throw RuntimeError("Type error at array.set: array expected");
    if (index >= array->length || index < 0) {
      throw RuntimeError("Index out of bounds at array.set");
    }
    array->contents[index] = value;
    return new IntValue(0);
  }

  auto *maybeFunc = ctx.program->index[func];
  if (!maybeFunc || !maybeFunc->is<FunctionDeclaration>())
    throw RuntimeError("No such function: " + func);
  auto *funcObject = maybeFunc->as<FunctionDeclaration>();
  requireArity(funcObject->params.size());

  ctx.callStack.push({});
  for (int i = 0; i < args.size(); ++i) {
    const auto &name = funcObject->params[i];
    if (ctx.program->index.count(name) > 0) {
      throw RuntimeError("Function parameter name is global identifier: " +
                         name);
    }
    ctx.set(name, argValues[i]);
  }

  try {
    funcObject->body->eval(ctx);
  } catch (ReturnFromCall r) {
    ctx.callStack.pop();
    return r.value;
  }

  ctx.callStack.pop();
  return new IntValue(0);
}


static std::string indent(const std::string &s) {
  std::string res = "  ";
  for (char ch : s) {
    res += ch;
    if (ch == '\n') {
      res += "  ";
    }
  }
  return res;
}

std::string SetStatement::toString() const {
  return std::string("(set ") + name + " " + value->toString() + ")";
}
void SetStatement::eval(Context &ctx) const { ctx.set(name, value->eval(ctx)); }

std::string IfStatement::toString() const {
  return std::string("(if ") + condition->toString() + "\n" +
         indent(body->toString()) + ")";
}
void IfStatement::eval(Context &ctx) const {
  bool ok = isTruthy(condition->eval(ctx));
  if (ok) {
    body->eval(ctx);
  }
}

std::string ForStatement::toString() const {
  return std::string("(for\n") + indent(init->toString()) + "\n" +
         indent(test->toString()) + "\n" + indent(update->toString()) + "\n" +
         indent(body->toString()) + ")";
}
void ForStatement::eval(Context &ctx) const {
  for (init->eval(ctx); isTruthy(test->eval(ctx)); update->eval(ctx)) {
    body->eval(ctx);
  }
}

std::string BlockStatement::toString() const {
  std::string str = "(block";
  for (const auto &stmt : body) {
    str += "\n";
    str += indent(stmt->toString());
  }
  str += ")";
  return str;
}
void BlockStatement::eval(Context &ctx) const {
  for (auto stmt : body) {
    stmt->eval(ctx);
  }
}

std::string ReturnStatement::toString() const {
  return std::string("(return ") + value->toString() + ")";
}
void ReturnStatement::eval(Context &ctx) const {
  throw ReturnFromCall{value->eval(ctx)};
}

std::string FunctionDeclaration::toString() const {
  std::string str = "(function (";
  str += name;
  for (const auto &param : params) {
    str += " ";
    str += param;
  }
  str += ")\n";
  str += indent(body->toString());
  str += ")";
  return str;
}

std::string GlobalVariable::toString() const {
  return std::string("(global ") + name + ")";
}

Program::Program(const std::vector<ProgramElement *> &body) : body(body) {
  for (auto el : body) {
    std::string name;
    if (el->is<FunctionDeclaration>()) {
      name = el->as<FunctionDeclaration>()->name;
    } else {
      name = el->as<GlobalVariable>()->name;
    }

    for (const auto &f : builtinFunctions) {
      if (name == f) {
        throw SyntaxError("Redefining built-in construct: " + name);
      }
    }
    if (index.count(name) > 0) {
      throw SyntaxError("Duplicate program element: " + name);
    }
    index[name] = el;
  }
}
std::string Program::toString() const {
  std::string str;
  for (auto el : body) {
    str += el->toString();
    str += "\n\n";
  }
  return str;
}
void Program::eval() {
  VariableSet globalVars;
  for (const auto &el : body) {
    if (el->is<GlobalVariable>()) {
      globalVars.values[el->as<GlobalVariable>()->name] = nullptr;
    }
  }
  Context ctx{
      .globalVars = std::move(globalVars),
      .callStack = {},
      .program = this,
  };
  CallExpression("main", {}).eval(ctx);
}

static bool isValidIdentifier(const std::string &name) {
  if (name.length() > kIdMaxLength) return false;
  if (name.empty()) return false;
  if (isdigit(name[0])) return false;
  for (char ch : name) {
    if (ch == ')' || ch == '(' || ch == ';') return false;
    if (!isgraph(ch)) return false;
  }
  for (const auto &kw : keywords) {
    if (name == kw) return false;
  }
  return true;
}

static void removeWhitespaces(std::istream &is) {
  while (is && isspace(is.peek())) is.get();
  if (is.peek() == ';') {
    int ch;
    do {
      ch = is.get();
    } while (ch != EOF && ch != '\n');
    removeWhitespaces(is);
  }
}

static void expectClosingParens(std::istream &is) {
  removeWhitespaces(is);
  int ch = is.get();
  if (ch != ')') {
    throw SyntaxError(std::string("Closing parenthesis expected, got ") +
                      char(ch));
  }
}

static std::string scanToken(std::istream &is) {
  removeWhitespaces(is);
  std::string token;
  for (int next = is.peek(); !isspace(next) && next != ')' && next != ';';
       next = is.peek()) {
    token += is.get();
  }
  return token;
}

static std::string scanIdentifier(std::istream &is) {
  auto name = scanToken(is);
  if (!isValidIdentifier(name))
    throw SyntaxError("Invalid identifier: " + name);
  return name;
}

template <typename T>
static T *scanT(std::istream &is) {
  auto *construct = scan(is);
  if (construct == nullptr) throw SyntaxError("Unexpected EOF");
  if (!construct->is<T>()) {
    throw SyntaxError(std::string("Wrong construct type; ") +
                      typeid(*construct).name() + " found, " +
                      typeid(T).name() + " expected");
  }
  return construct->as<T>();
}

Construct *scan(std::istream &is) {
  // ignore whitespaces
  removeWhitespaces(is);
  if (!is || is.peek() == EOF) return nullptr;
  if (is.peek() != '(') {
    // variable or literal
    auto name = scanToken(is);
    if (name.empty()) return nullptr;
    if (isdigit(name[0])) {
      for (char ch : name) {
        if (!isdigit(ch)) throw SyntaxError("Invalid literal: " + name);
      }
      int value = std::stoi(name);
      return new IntegerLiteral(value);
    }
    if (isValidIdentifier(name)) {
      return new Variable(name);
    }
    throw SyntaxError("Invalid identifier name: " + name);
  }
  is.get();

  auto type = scanToken(is);
  if (type == "set") {
    auto name = scanIdentifier(is);
    auto *value = scanT<Expression>(is);
    expectClosingParens(is);
    return new SetStatement(name, value->as<Expression>());
  } else if (type == "if") {
    auto *cond = scanT<Expression>(is);
    auto *body = scanT<Statement>(is);
    expectClosingParens(is);
    return new IfStatement(cond, body);
  } else if (type == "for") {
    auto *init = scanT<Statement>(is);
    auto *test = scanT<Expression>(is);
    auto *update = scanT<Statement>(is);
    auto *body = scanT<Statement>(is);
    expectClosingParens(is);
    return new ForStatement(init, test, update, body);
  } else if (type == "block") {
    std::vector<Statement *> body;
    removeWhitespaces(is);
    while (is.peek() != ')') {
      body.push_back(scanT<Statement>(is));
      removeWhitespaces(is);
    }
    expectClosingParens(is);
    return new BlockStatement(body);
  } else if (type == "return") {
    auto *value = scanT<Expression>(is);
    expectClosingParens(is);
    return new ReturnStatement(value);
  } else if (type == "function") {
    removeWhitespaces(is);
    if (is.get() != '(') {
      throw SyntaxError("Opening parenthesis expected");
    }
    auto name = scanIdentifier(is);
    std::vector<std::string> params;
    removeWhitespaces(is);
    while (is.peek() != ')') {
      params.push_back(scanIdentifier(is));
      removeWhitespaces(is);
    }
    expectClosingParens(is);
    auto *body = scanT<Statement>(is);
    expectClosingParens(is);
    return new FunctionDeclaration(name, params, body);
  } else if (type == "global") {
    auto name = scanIdentifier(is);
    expectClosingParens(is);
    return new GlobalVariable(name);
  } else {
    // call expression
    auto &name = type;
    if (!isValidIdentifier(name))
      throw SyntaxError("Invalid identifier: " + name);
    std::vector<Expression *> args;
    removeWhitespaces(is);
    while (is.peek() != ')') {
      args.push_back(scanT<Expression>(is));
      removeWhitespaces(is);
    }
    expectClosingParens(is);
    return new CallExpression(name, args);
  }
}

Program *scanProgram(std::istream &is) {
  std::vector<ProgramElement *> body;
  while (true) {
    auto *el = scan(is);
    if (el == nullptr) break;
    if (!el->is<ProgramElement>()) {
      throw SyntaxError("Invalid program element");
    }
    body.push_back(el->as<ProgramElement>());
  }
  return new Program(body);
}
