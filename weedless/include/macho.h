#pragma once

namespace weedless {

  namespace config {
    struct Config;
  };

  void patchMachO(const config::Config& config);
}

