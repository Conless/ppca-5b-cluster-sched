#include <cmath>
#include <iostream>
#include <map>
#include <unordered_set>

#include "lang.h"
#include "visitor.h"

const std::unordered_set<std::string> importantBuiltinFunctions = {
  "array.create", "array.get", "array.set", "array.scan", "array.print",
};

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

  double evaluate(CFG& other) {
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

class ImportantFunctionsCount : public Visitor<int> {
 public:
  int visitProgram(Program *node) override {
    int l = 0;
    for (auto func : node->body) {
      l += visitFunctionDeclaration(func);
    }
    return l;
  }
  int visitFunctionDeclaration(FunctionDeclaration *node) override {
    return visitStatement(node->body);
  }
  int visitExpressionStatement(ExpressionStatement *node) override {
    return visitExpression(node->expr);
  }
  int visitSetStatement(SetStatement *node) override {
    return visitExpression(node->value);
  }
  int visitIfStatement(IfStatement *node) override {
    return visitExpression(node->condition) + visitStatement(node->body);
  }
  int visitForStatement(ForStatement *node) override {
    return visitStatement(node->body) + visitExpression(node->test) + visitStatement(node->update) + visitStatement(node->body);
  }
  int visitBlockStatement(BlockStatement *node) override {
    int l = 0;
    for (auto stmt : node->body) {
      l += visitStatement(stmt);
    }
    return l;
  }
  int visitReturnStatement(ReturnStatement *node) override { return 0; }

  int visitIntegerLiteral(IntegerLiteral *node) override { return 0; }
  int visitVariable(Variable *node) override { return 0; }
  int visitCallExpression(CallExpression *node) override {
    int l = 0;
    for (auto expr : node->args) {
      l += visitExpression(expr);
    }
    if (importantBuiltinFunctions.count(node->func) > 0) {
      l++;
    }
    return l;
  }
};

int main() {
  auto* pgm1 = scanProgram(std::cin);
  auto* pgm2 = scanProgram(std::cin);
  CFG cfg1(pgm1);
  CFG cfg2(pgm2);
  ImportantFunctionsCount ifc1;
  auto m1 = cfg1.evaluate(cfg2);
  std::cout << m1 * m1 << std::endl;
  return 0;
}
