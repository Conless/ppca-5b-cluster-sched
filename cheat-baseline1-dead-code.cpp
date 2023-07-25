#include <iostream>

#include "lang.h"
#include "transform.h"

int a = 0;

class Cheat : public Transform {
 public:
  Statement *transformSetStatement(SetStatement *node) override {
    return ifForSure(new SetStatement(
      transformVariable(node->name),
      transformExpression(node->value)
    ));
  }

  Statement* transformIfStatement(IfStatement* node) override {
    return new IfStatement(
      transformExpression(node->condition),
      ifForSure(transformStatement(node->body))
    );
  }
 
  Statement* transformForStatement(ForStatement *node) override {
    return new ForStatement(
      transformStatement(node->init),
      transformExpression(node->test),
      transformStatement(node->update),
      ifForSure(transformStatement(node->body))
    );
  }

  Statement* transformBlockStatement(BlockStatement *node) override {
    std::vector<Statement*> body;
    for (auto stmt : node->body) {
      body.push_back(transformStatement(stmt));
    }
    return ifForSure(new BlockStatement(body));
  }

  Statement* transformExpressionStatement(ExpressionStatement *node) override {
    return ifForSure(new ExpressionStatement(transformExpression(node->expr)));
  }

  FunctionDeclaration* transformFunctionDeclaration(FunctionDeclaration *node) override {
    std::vector<Variable *> params;
    for (auto param : node->params) {
      params.push_back(transformVariable(param));
    }
    Statement* body = new BlockStatement({
      new SetStatement(new Variable("ppcappcappcappcappca"), new IntegerLiteral(0)),
      new SetStatement(new Variable("ppcappcappcappcappcb"), new IntegerLiteral(0)),
      transformStatement(node->body)
    });
    return new FunctionDeclaration(node->name, params, body);
  }

 private:
  Statement* ifForSure(Statement* node) {
    return new IfStatement(
      new CallExpression(
        "==",
        {new Variable("ppcappcappcappcappca"), new Variable("ppcappcappcappcappcb")}
      ),
      node
    );
  }
};

int main() {
  auto code = scanProgram(std::cin);
  auto cheat = Cheat().transformProgram(code);
  std::cout << cheat->toString();
  return 0;
}