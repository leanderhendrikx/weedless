#pragma once

// stl
#include <string>
#include <vector>


namespace weedless {

  namespace config {
    struct Config;
  };

void installDylibs(const config::Config& config);
}
