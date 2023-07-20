#include <fstream>
#include <iostream>

#include "lang.h"

int main (int argc, char **argv) {
  try {
    auto code = std::ifstream(argv[1]);
    auto *p = scanProgram(code);
    std::cout << p->toString();
    p->eval();
  } catch (const EvalError &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
