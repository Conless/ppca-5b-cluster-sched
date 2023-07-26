#include <cmath>
#include <iostream>
#include <map>
#include <unordered_set>

#include "lang.h"
#include "visitor.h"

const std::unordered_set<std::string> importantBuiltinFunctions = {
  "array.create", "array.get", "array.set", "array.scan", "array.print",
};

double compress(double x) {
    if (x < 0.0001) return 0;
    if (x > 0.9999) return 1;
    return x;
}

double DifferenceMetrics(double a, double b) {
    if (a == 0 && b == 0) {
        return 0;
    }
    return std::abs(a - b) / std::max(a, b);
}

class CFG {
 public:
  CFG(Program* program) : pgm(program) {
    buildCFG();
    dfs(entry);
  }

  double evaluate(const CFG& other) const {
    double simOnBackEdges = 1 - DifferenceMetrics(backEdgeCount, other.backEdgeCount);
    double simOnForwardEdges = 1 - DifferenceMetrics(forwardEdgeCount, other.forwardEdgeCount);
    return std::sqrt(simOnBackEdges * simOnBackEdges);
  }

  struct Node {
    std::vector<Node*> successors;
    std::vector<Node*> predecessors;
  };

 private:
  struct DfsStatus {
    bool in = false;
    bool out = false;
    Node* parent = nullptr;
  };

  void buildCFG() {
    // get the entry
    traverseAllFunctions();
    entry = functionEntries["main"];
  }

  void dfs(Node* node) {
    dfsStatus[node].in = true;
    for (Node* child : node->successors) {
      DfsStatus& status = dfsStatus[child];
      if (!status.in) { // not visited
        dfsStatus[child].parent = node;
        dfs(child);
      } else if (!status.out) { // back edge
        ++backEdgeCount;
      } else { // forward edge
        ++forwardEdgeCount;
      }
    }
    dfsStatus[node].out = true;
  }

  void traverseAllFunctions() {
    for (auto function : pgm->body) {
      functionEntries[function->name] = newNode();
      functionReturns[function->name] = newNode();
    }
    for (auto function : pgm->body) {
      traverseFunction(function);
    }
  }

  Node* traverseFunction(FunctionDeclaration* function) {
    auto* entryNode = functionEntries[function->name];
    auto* returnNode = functionReturns[function->name];
    auto* lastNode = traverseStatement(function->body, entryNode, returnNode);
    if (lastNode != returnNode) {
      connect(lastNode, returnNode);
    }
    return returnNode;
  }

  Node* traverseStatement(Statement* stmt, Node* currentNode, Node* returnNode) {
    if (stmt->is<BlockStatement>()) {
        for (auto s : stmt->as<BlockStatement>()->body) {
          currentNode = traverseStatement(s, currentNode, returnNode);
        }
        return currentNode;
    } else if (stmt->is<IfStatement>()) {
        auto* ifStmt = stmt->as<IfStatement>();
        auto* ifNode = newNode();
        connect(currentNode, ifNode);
        auto* thenNode = traverseStatement(ifStmt->body, ifNode, returnNode);
        auto* endNode = newNode();
        connect(thenNode, endNode);
        connect(currentNode, endNode);
        return endNode;
    } else if (stmt->is<ForStatement>()) {
        auto* forStmt = stmt->as<ForStatement>();
        auto* initNode = traverseStatement(forStmt->init, currentNode, returnNode);
        auto* forNode = newNode();
        auto* bodyNode = newNode();
        auto* endNode = newNode();
        connect(initNode,forNode);
        connect(forNode, endNode);
        connect(forNode, bodyNode);
        bodyNode = traverseStatement(forStmt->body, bodyNode, returnNode);
        auto* stepNode = traverseStatement(forStmt->update, bodyNode, returnNode);
        connect(stepNode, forNode);
        return endNode;
    } else if (stmt->is<ReturnStatement>()) {
        connect(currentNode, returnNode);
        return returnNode;
    } else if (stmt->is<ExpressionStatement>()) {
        return traverseExpression(stmt->as<ExpressionStatement>()->expr, currentNode, returnNode);
    } else {
        return currentNode;
    }
  }

  Node* traverseExpression(Expression* expr, Node* currentNode, Node* returnNode) {
    if (expr->is<CallExpression>()) {
        CallExpression* callExpr = expr->as<CallExpression>();
        for (auto e : callExpr->args) {
          currentNode = traverseExpression(e, currentNode, returnNode);
        }
        if (builtinFunctions.count(callExpr->func) > 0) {
            return currentNode;
        }
        connect(currentNode, functionEntries[callExpr->func]);
        return functionReturns[callExpr->func];
    } else {
        return currentNode;
    }
  }

  void connect(Node* from, Node* to) {
    from->successors.push_back(to);
    to->predecessors.push_back(from);
  }

  Node* newNode() {
    auto* node = new Node;
    dfsStatus[node] = DfsStatus{false, false, nullptr};
    return node;
  }

  Program* pgm;
  Node* entry = nullptr;
  std::map<Node*, DfsStatus> dfsStatus;
  std::map<std::string, Node*> functionEntries;
  std::map<std::string, Node*> functionReturns;
  int backEdgeCount = 0;
  int forwardEdgeCount = 0;
};

struct Count {
  int arrayCreate = 0;
  int arrayGet = 0;
  int arraySet = 0;
  int arrayScan = 0;
  int arrayPrint = 0;

  Count& operator+=(const Count& other) {
    arrayCreate += other.arrayCreate;
    arrayGet += other.arrayGet;
    arraySet += other.arraySet;
    arrayScan += other.arrayScan;
    arrayPrint += other.arrayPrint;
    return *this;
  }
  Count operator+(const Count& other) const {
    Count result = *this;
    result += other;
    return result;
  }
  double evaluate(const Count& other) const {
    double simOnArrayCreate = 1 - DifferenceMetrics(arrayCreate, other.arrayCreate);
    double simOnArrayGet = 1 - DifferenceMetrics(arrayGet, other.arrayGet);
    double simOnArraySet = 1 - DifferenceMetrics(arraySet, other.arraySet);
    double simOnArrayScan = 1 - DifferenceMetrics(arrayScan, other.arrayScan);
    double simOnArrayPrint = 1 - DifferenceMetrics(arrayPrint, other.arrayPrint);
    return std::pow(simOnArrayCreate * simOnArrayGet * simOnArraySet * simOnArrayScan * simOnArrayPrint, 1.0 / 5);
  }
};

class ImportantFunctionsCount : public Visitor<Count> {
 public:
  Count visitProgram(Program *node) override {
    Count l;
    for (auto func : node->body) {
      l += visitFunctionDeclaration(func);
    }
    return l;
  }
  Count visitFunctionDeclaration(FunctionDeclaration *node) override {
    return visitStatement(node->body);
  }
  Count visitExpressionStatement(ExpressionStatement *node) override {
    return visitExpression(node->expr);
  }
  Count visitSetStatement(SetStatement *node) override {
    return visitExpression(node->value);
  }
  Count visitIfStatement(IfStatement *node) override {
    return visitExpression(node->condition) + visitStatement(node->body);
  }
  Count visitForStatement(ForStatement *node) override {
    return visitStatement(node->body) + visitExpression(node->test) + visitStatement(node->update) + visitStatement(node->body);
  }
  Count visitBlockStatement(BlockStatement *node) override {
    Count l;
    for (auto stmt : node->body) {
      l += visitStatement(stmt);
    }
    return l;
  }
  Count visitReturnStatement(ReturnStatement *node) override { return Count(); }

  Count visitIntegerLiteral(IntegerLiteral *node) override { return Count(); }
  Count visitVariable(Variable *node) override { return Count(); }
  Count visitCallExpression(CallExpression *node) override {
    Count l;
    for (auto expr : node->args) {
      l += visitExpression(expr);
    }
    if (node->func == "array.create") {
      ++l.arrayCreate;
    } else if (node->func == "array.get") {
      ++l.arrayGet;
    } else if (node->func == "array.set") {
      ++l.arraySet;
    } else if (node->func == "array.scan") {
      ++l.arrayScan;
    } else if (node->func == "array.print") {
      ++l.arrayPrint;
    }
    return l;
  }
};

class RemoveDeadCodeForFunction {
 public:
  RemoveDeadCodeForFunction(FunctionDeclaration* function) : func(function) {
  }

  FunctionDeclaration* eliminateDeadCode() {
    bool ok = false;
    do {
      ok = !tryEliminate();
    } while (!ok);
    return func;
  }

 private:
  struct SymbolStatus {
    bool constant = true;
    int value = 0;
  };

  bool tryEliminate() {
    symbolStatus.clear();
    hasChanged = false;
    for (auto param : func->params) {
      symbolStatus[param->name].constant = false;
    }
    checkSymbolConstantStatement(func->body);
    func->body = replaceAndEliminate(func->body);
    if (func->body == nullptr) {
      func->body = new BlockStatement({});
    }
    return hasChanged;
  }
  Statement* newEmptyStmt() {
    return new ExpressionStatement(new CallExpression(
      "==",
      {new IntegerLiteral(0), new IntegerLiteral(0)}
    ));
  }

  Statement* replaceAndEliminate(Statement* stmt) {
    if (stmt->is<ExpressionStatement>()) {
      return new ExpressionStatement(
        replaceAndEliminate(stmt->as<ExpressionStatement>()->expr)
      );
    } else if (stmt->is<SetStatement>()) {
      return new SetStatement(
        stmt->as<SetStatement>()->name, replaceAndEliminate(stmt->as<SetStatement>()->value));
    } else if (stmt->is<IfStatement>()) {
      IfStatement* ifStmt = stmt->as<IfStatement>();
      Expression* condition = replaceAndEliminate(ifStmt->condition);
      if (condition->is<IntegerLiteral>()) {
        hasChanged = true;
        if (condition->as<IntegerLiteral>()->value == 0) {
          return nullptr;
        } else {
          return replaceAndEliminate(ifStmt->body);
        }
      } else {
        Statement* body = replaceAndEliminate(ifStmt->body);
        if (body == nullptr) {
          return nullptr;
        } else {
          return new IfStatement(condition, body);
        }
      }
    } else if (stmt->is<ForStatement>()) {
      ForStatement* forStmt = stmt->as<ForStatement>();
      Statement* init = replaceAndEliminate(forStmt->init);
      Expression* test = replaceAndEliminate(forStmt->test);
      Statement* update = replaceAndEliminate(forStmt->update);
      Statement* body = replaceAndEliminate(forStmt->body);
      if (init == nullptr) init = newEmptyStmt();
      if (update == nullptr) update = newEmptyStmt();
      if (body == nullptr) body = newEmptyStmt();
      return new ForStatement(init, test, update, body);
    } else if (stmt->is<BlockStatement>()) {
      BlockStatement* blockStmt = stmt->as<BlockStatement>();
      std::vector<Statement*> newBody;
      for (auto s : blockStmt->body) {
        Statement* newStmt = replaceAndEliminate(s);
        if (newStmt != nullptr) {
          newBody.push_back(newStmt);
        }
      }
      return new BlockStatement(newBody);
    } else {
      return stmt;
    }
  }

  Expression* replaceAndEliminate(Expression* expr) {
    if (expr->is<CallExpression>()) {
      CallExpression* callExpr = expr->as<CallExpression>();
      std::vector<Expression*> newArgs;
      bool allConstant = true;
      for (auto e : callExpr->args) {
        Expression* newExpr = replaceAndEliminate(e);
        newArgs.push_back(newExpr);
        if (!newExpr->is<IntegerLiteral>()) {
          allConstant = false;
        }
      }
      if (allConstant) {
        if (callExpr->func == "+") {
          int result = newArgs[0]->as<IntegerLiteral>()->value + newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "-") {
          int result = newArgs[0]->as<IntegerLiteral>()->value - newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "*") {
          int result = newArgs[0]->as<IntegerLiteral>()->value * newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "/") {
          int result = newArgs[0]->as<IntegerLiteral>()->value / newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "%") {
          int result = newArgs[0]->as<IntegerLiteral>()->value % newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "==") {
          int result = newArgs[0]->as<IntegerLiteral>()->value == newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "!=") {
          int result = newArgs[0]->as<IntegerLiteral>()->value != newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "<") {
          int result = newArgs[0]->as<IntegerLiteral>()->value < newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "<=") {
          int result = newArgs[0]->as<IntegerLiteral>()->value <= newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == ">") {
          int result = newArgs[0]->as<IntegerLiteral>()->value > newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == ">=") {
          int result = newArgs[0]->as<IntegerLiteral>()->value >= newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "!") {
          int result = !(newArgs[0]->as<IntegerLiteral>()->value);
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "&&") {
          int result = newArgs[0]->as<IntegerLiteral>()->value && newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else if (callExpr->func == "||") {
          int result = newArgs[0]->as<IntegerLiteral>()->value || newArgs[1]->as<IntegerLiteral>()->value;
          hasChanged = true;
          return new IntegerLiteral(result);
        } else {
          return new CallExpression(callExpr->func, newArgs);
        }
      }
      return new CallExpression(callExpr->func, newArgs);
    } else if (expr->is<Variable>()) {
      Variable* var = expr->as<Variable>();
      if (symbolStatus.count(var->name) == 0) {
        return expr;
      } else {
        if (symbolStatus[var->name].constant) {
          hasChanged = true;
          return new IntegerLiteral(symbolStatus[var->name].value);
        } else {
          return expr;
        }
      }
    } else {
      return expr;
    }
  }

  void checkSymbolConstantStatement(Statement* stmt) {
    if (stmt->is<BlockStatement>()) {
      for (auto s : stmt->as<BlockStatement>()->body) {
        checkSymbolConstantStatement(s);
      }
    } else if (stmt->is<IfStatement>()) {
      auto* ifStmt = stmt->as<IfStatement>();
      checkSymbolConstantStatement(ifStmt->body);
    } else if (stmt->is<ForStatement>()) {
      auto* forStmt = stmt->as<ForStatement>();
      checkSymbolConstantStatement(forStmt->init);
      checkSymbolConstantStatement(forStmt->update);
      checkSymbolConstantStatement(forStmt->body);
    } else if (stmt->is<SetStatement>()) {
      SetStatement* setStmt = stmt->as<SetStatement>();
      if (!setStmt->value->is<IntegerLiteral>()) {
        symbolStatus[setStmt->name->name].constant = false;
      } else {
        if (symbolStatus.count(setStmt->name->name) == 0) {
          symbolStatus[setStmt->name->name].constant = true;
          symbolStatus[setStmt->name->name].value = setStmt->value->as<IntegerLiteral>()->value;
        } else {
          if (!symbolStatus[setStmt->name->name].constant ||
            symbolStatus[setStmt->name->name].value != setStmt->value->as<IntegerLiteral>()->value) {
            symbolStatus[setStmt->name->name].constant = false;
          }
        }
      }
    }
  }

  FunctionDeclaration* func;
  std::map<std::string, SymbolStatus> symbolStatus;
  bool hasChanged = false;
};

Program* eliminateDeadCode(Program* origin) {
  std::vector<FunctionDeclaration *> body;
  for (auto func : origin->body) {
    RemoveDeadCodeForFunction removeDeadCode(func);
    body.push_back(removeDeadCode.eliminateDeadCode());
  }
  return new Program(body);
}

int main() {
  auto* pgm1 = eliminateDeadCode(scanProgram(std::cin));
  auto* pgm2 = eliminateDeadCode(scanProgram(std::cin));
  CFG cfg1(pgm1);
  CFG cfg2(pgm2);
  ImportantFunctionsCount ifc;
  Count count1 = ifc.visitProgram(pgm1);
  Count count2 = ifc.visitProgram(pgm2);
  auto m1 = cfg1.evaluate(cfg2);
  auto m2 = count1.evaluate(count2);
  std::cout << compress(1.5 * m1 * m1 + 0.4 * m2 - 0.3) << std::endl;
  return 0;
}
