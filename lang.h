#include <exception>
#include <istream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// +------------------------------------+
// |              Values                |
// +------------------------------------+

class Value {
 public:
  virtual ~Value() {}
};
using ValuePtr = std::shared_ptr<Value>;

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
  ~ArrayValue() override;
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
  virtual ValuePtr eval(Context &ctx) const = 0;
};

class IntegerLiteral : public Expression {
 public:
  int value;

  IntegerLiteral(int value) : value(value) {}
  std::string toString() const override;
  ValuePtr eval(Context &ctx) const override;
};

class Variable : public Expression {
 public:
  std::string name;

  Variable(std::string name) : name(std::move(name)) {}
  std::string toString() const override;
  ValuePtr eval(Context &ctx) const override;
};

class CallExpression : public Expression {
 public:
  std::string func;
  std::vector<Expression *> args;

  CallExpression(std::string func, std::vector<Expression *> args)
      : func(std::move(func)), args(std::move(args)) {}
  std::string toString() const override;
  ValuePtr eval(Context &ctx) const override;
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

  SetStatement(std::string name, Expression *value)
      : name(std::move(name)), value(value) {}
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

  BlockStatement(std::vector<Statement *> body) : body(std::move(body)) {}
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

class FunctionDeclaration : public Construct {
 public:
  std::string name;
  std::vector<std::string> params;
  Statement *body;

  FunctionDeclaration(std::string name, std::vector<std::string> params,
                      Statement *body)
      : name(std::move(name)), params(std::move(params)), body(body) {}
  std::string toString() const override;
};

class Program : public Construct {
 public:
  std::vector<FunctionDeclaration *> body;
  std::unordered_map<std::string, FunctionDeclaration *> index;

  Program(std::vector<FunctionDeclaration *> body);
  std::string toString() const override;
  void eval();
};


// +------------------------------------+
// |            Exceptions              |
// +------------------------------------+

class EvalError : public std::exception {
 public:
  const Construct *location;
  std::string reason;

  EvalError(const Construct *location, const std::string &reason_)
      : location(location) {
    if (location == nullptr) {
      reason = reason_;
      return;
    }
    reason = "At " + location->toString() + ":\n" + reason_;
  }
  const char *what() const noexcept override { return reason.c_str(); }
};
class SyntaxError : public EvalError {
 public:
  SyntaxError(const Construct *location, const std::string &reason)
      : EvalError(location, "Syntax error: " + reason) {}
};
class RuntimeError : public EvalError {
 public:
  RuntimeError(const Construct *location, const std::string &reason)
      : EvalError(location, "Runtime error: " + reason) {}
};


// +------------------------------------+
// |               Parser               |
// +------------------------------------+

Construct *scan(std::istream &is);
Program *scanProgram(std::istream &is);
