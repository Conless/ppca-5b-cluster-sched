#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>

#include "lang.h"

const int timeLimit = 1000000;

std::string exec (const char *prog, const std::string &infile) {
  std::ifstream program(prog);
  std::istringstream iss(infile);
  std::ostringstream oss;
  scanProgram(program)->eval(timeLimit, iss, oss);
  return oss.str();
}

int main (int argc, char **argv) {
  // FILE *input   = fopen(argv[1], "r"); // 题目的输入文件
  // FILE *output  = fopen(argv[2], "r"); // 用户输出
  FILE *answer  = fopen(argv[3], "r"); // 题目的答案
  FILE *score   = fopen(argv[4], "w"); // 把评测的分数输出到这里
  // FILE *message = fopen(argv[5], "w"); // 这里输出错误/提示信息

  std::string infile;
  int c;
  while ((c = fgetc(answer)) != EOF) {
    infile += c;
  }

  std::string ans = exec(argv[1], infile);
  std::string userAns = exec(argv[2], infile);

  if (ans == userAns) {
    fputs("1", score);
  } else {
    fputs("0", score);
  }

  return 0;
}
