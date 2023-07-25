#include <fstream>
#include <stdio.h>

#include "lang.h"

int main () {
  std::ifstream stdp("input.p");
  std::ifstream userp("output.p");
  std::cout << scanProgram(stdp)->toString() << "endprogram\n";
  std::cout << scanProgram(userp)->toString() << "endprogram\n";
  int c;
  while ((c = getchar()) != EOF) {
    std::cout << (char) c;
  }
  return 0;
}
