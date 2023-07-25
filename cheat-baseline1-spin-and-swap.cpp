#include <iostream>

#include "lang.h"
#include "transform.h"

int a = 0;

class Cheat : public Transform {
 public:
  Expression* transformIntegerLiteral(IntegerLiteral* node) override {
    return new CallExpression(
      "+",
      {new IntegerLiteral(1), new IntegerLiteral(node->value - 1)}
    );
  }

  Expression* transformCallExpression(CallExpression* node) override {
    if (node->func == "+") {
      ++a;
      if (a % 4 == 0) {
        return new CallExpression(
          "+",
          {node->args[1], node->args[2]}
        );
      } else {
        return new CallExpression(
          "-",
          {
            node->args[0],
            new CallExpression(
              "-", {new IntegerLiteral(0), node->args[1]}
            )
          }
        );
      }
    } else if (node->func == "-") {
      return new CallExpression(
        "+",
        {
          node->args[0],
          new CallExpression(
            "-", {new IntegerLiteral(0), node->args[1]}
          )
        }
      );
    } else {
      return Transform::transformCallExpression(node);
    }
  }
};

int main() {
  auto code = scanProgram(std::cin);
  auto cheat = Cheat().transformProgram(code);
  std::cout << cheat->toString();
  return 0;
}
