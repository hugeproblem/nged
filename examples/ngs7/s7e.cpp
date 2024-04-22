// s7 extended
// here to run exported scheme file from visual scheme editor

#include "s7.h"
#include "s7-extensions.h"

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
  if (argc!=2) {
    std::stringstream ss;
    ss << "usage:\n"
       << "      " << argv[0] << " <scheme file>\n"
       << "   or:\n"
       << "      " << argv[0] << " --rep\n"
       << "\n"
       << "  \"--rep\" means Read, Evaluate, Print (no loop)\n";
    fputs(ss.str().c_str(), stderr);
    return 1;
  }

  FILE* inputfile = nullptr;
  bool const useStdin = strncmp(argv[1], "--rep", 6) == 0;

  if (useStdin)
    inputfile = stdin;
  else {
    inputfile = fopen(argv[1], "rb");
  }
  if (!inputfile) {
    fprintf(stderr, "cannot open file %s as source\n", argv[1]);
    return 1;
  }
  std::string source;
  char buf[1024];
  size_t bytesread = 0;
  do {
    bytesread = fread(buf, 1, sizeof(buf), inputfile);
    source.append(buf, bytesread);
  } while (bytesread!=0);

  s7_scheme* runtime = s7_init();
  addS7Extenstions(runtime);
  s7_eval_c_string(runtime, source.c_str());
  s7_free(runtime);
  return 0;
}

