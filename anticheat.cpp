#include <cstdlib>
#include <iostream>

#include "lang.h"
#include "visitor.h"

class Length : public Visitor<int> {
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
    return visitExpression(node->expr) + 1;
  }
  int visitSetStatement(SetStatement *node) override {
    return visitExpression(node->value) + 1;
  }
  int visitIfStatement(IfStatement *node) override {
    return visitExpression(node->condition) + visitStatement(node->body) + 1;
  }
  int visitForStatement(ForStatement *node) override {
    return visitStatement(node->body) + visitExpression(node->test) + visitStatement(node->update) + visitStatement(node->body) + 1;
  }
  int visitBlockStatement(BlockStatement *node) override {
    int l = 0;
    for (auto stmt : node->body) {
      l += visitStatement(stmt);
    }
    return l;
  }
  int visitReturnStatement(ReturnStatement *node) override { return 1; }

  int visitIntegerLiteral(IntegerLiteral *node) override { return 1; }
  int visitVariable(Variable *node) override { return 1; }
  int visitCallExpression(CallExpression *node) override {
    int l = 1;
    for (auto expr : node->args) {
      l += visitExpression(expr);
    }
    return l;
  }
};

int getLength(Program *p) {
  return Length().visitProgram(p);
}

int main() {
  int len1 = getLength(scanProgram(std::cin));
  int len2 = getLength(scanProgram(std::cin));
  int diff = abs(len1 - len2);
  if (diff < 20) {
    std::cout << 0.5 + 0.5 * (1.0 - diff / 20.0) << std::endl;
  } else {
    std::cout << 0.5 << std::endl;
  }
  return 0;
}
