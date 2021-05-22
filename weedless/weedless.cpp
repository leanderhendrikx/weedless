// weedless
#include "macho.h"
#include "config.h"
#include "install.h"

// sys
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

// stl
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "No hook file provided." << std::endl;
    return 1;
  }

  auto config = weedless::config::read(argv[1]);

  weedless::installDylibs(config); 
  weedless::patchMachO(config); 
} 
