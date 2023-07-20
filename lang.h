#include <exception>
#include <istream>
#include <string>
#include <unordered_map>
#include <vector>


// +------------------------------------+
// |            Exceptions              |
// +------------------------------------+

class EvalError : public std::exception {};
class SyntaxError : public EvalError {
 public:
  std::string reason;
  SyntaxError(const std::string &reason) : reason("Syntax error: " + reason) {}
  const char *what() const noexcept override { return reason.c_str(); }
};
class RuntimeError : public EvalError {
 public:
  std::string reason;
  RuntimeError(const std::string &reason)
      : reason("Runtime error: " + reason) {}
  const char *what() const noexcept override { return reason.c_str(); }
};


// +------------------------------------+
// |              Values                |
// +------------------------------------+

class Value {
 public:
  virtual ~Value() {}
};

class IntValue : public Value {
 public:
  int value;
  IntValue(int value) : value(value) {}
};

class ArrayValue : public Value {
 public:
  int length;
  int *contents;
  ArrayValue(int length);
};

struct VariableSet {
  std::unordered_map<std::string, Value *> values;
  Value *getOrThrow(const std::string &name);
};


// +------------------------------------+
// |        Program Structures          |
// +------------------------------------+

class Program;
struct Context;

class Construct {
 public:
  virtual ~Construct() {}
  virtual std::string toString() const = 0;

  template <typename T>
  bool is() const {
    return dynamic_cast<const T *>(this) != nullptr;
  }
  template <typename T>
  T *as() {
    return dynamic_cast<T *>(this);
  }
};


// +------------------------------------+
// |           Expressions              |
// +------------------------------------+

class Expression : public Construct {
 public:
  virtual Value *eval(Context &ctx) const = 0;
};

class IntegerLiteral : public Expression {
 public:
  int value;

  IntegerLiteral(int value) : value(value) {}
  std::string toString() const override;
  Value *eval(Context &ctx) const override;
};

class Variable : public Expression {
 public:
  std::string name;

  Variable(const std::string &name) : name(name) {}
  std::string toString() const override;
  Value *eval(Context &ctx) const override;
};

class CallExpression : public Expression {
 public:
  std::string func;
  std::vector<Expression *> args;

  CallExpression(const std::string &func, const std::vector<Expression *> &args)
      : func(func), args(args) {}
  std::string toString() const override;
  Value *eval(Context &ctx) const override;
};


// +------------------------------------+
// |           Statements               |
// +------------------------------------+

class Statement : public Construct {
 public:
  virtual void eval(Context &ctx) const = 0;
};

class SetStatement : public Statement {
 public:
  std::string name;
  Expression *value;

  SetStatement(const std::string &name, Expression *value)
      : name(name), value(value) {}
  std::string toString() const override;
  void eval(Context &ctx) const override;
};

class IfStatement : public Statement {
 public:
  Expression *condition;
  Statement *body;

  IfStatement(Expression *condition, Statement *body)
      : condition(condition), body(body) {}
  std::string toString() const override;
  void eval(Context &ctx) const override;
};

class ForStatement : public Statement {
 public:
  Statement *init;
  Expression *test;
  Statement *update;
  Statement *body;

  ForStatement(Statement *init, Expression *test, Statement *update,
               Statement *body)
      : init(init), test(test), update(update), body(body) {}
  std::string toString() const override;
  void eval(Context &ctx) const override;
};

class BlockStatement : public Statement {
 public:
  std::vector<Statement *> body;

  BlockStatement(const std::vector<Statement *> &body) : body(body) {}
  std::string toString() const override;
  void eval(Context &ctx) const override;
};

class ReturnStatement : public Statement {
 public:
  Expression *value;

  ReturnStatement(Expression *value) : value(value) {}
  std::string toString() const override;
  void eval(Context &ctx) const override;
};


// +------------------------------------+
// |        Global Constructs           |
// +------------------------------------+

class ProgramElement : public Construct {};

class FunctionDeclaration : public ProgramElement {
 public:
  std::string name;
  std::vector<std::string> params;
  Statement *body;

  FunctionDeclaration(const std::string &name,
                      const std::vector<std::string> &params, Statement *body)
      : name(name), params(params), body(body) {}
  std::string toString() const override;
};

class GlobalVariable : public ProgramElement {
 public:
  std::string name;

  GlobalVariable(const std::string &name) : name(name) {}
  std::string toString() const override;
};

class Program : public Construct {
 public:
  std::vector<ProgramElement *> body;
  std::unordered_map<std::string, ProgramElement *> index;

  Program(const std::vector<ProgramElement *> &body);
  std::string toString() const override;
  void eval();
};


// +------------------------------------+
// |               Parser               |
// +------------------------------------+

Construct *scan(std::istream &is);
Program *scanProgram(std::istream &is);
