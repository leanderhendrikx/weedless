// weedless
#include "macho.h"
#include "config.h"
#include "install.h"

// stl
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "No hook file provided." << std::endl;
    return 1;
  }

  auto config = weedless::config::read(argv[1]);

  weedless::installDylibs(config); 
  weedless::patchMachO(config); 
} 
