#pragma once

#include <algorithm>
#include <filesystem>
#include <vector>
#include <string>

namespace weedless::config 

{
  struct Dylib {
    std::string name; 
    std::filesystem::path path;
    std::string installName;
  };

  struct Hook {
    std::string symbol;
    std::string dylibName;
  };

  struct Config {
    
    const Dylib* getDylibByName(const std::string& name) const 
    {
      auto it = 
        std::find_if(
            dylibs.cbegin(),
            dylibs.cend(),
            [&name](const auto& dylib)
            {
              return dylib.name == name;
            });
      if (it == dylibs.cend()) {
        return nullptr;
      }
      return &*it;
    }

    std::vector<Dylib> dylibs; 
    std::vector<Hook> hooks; 
    std::filesystem::path target;
  };

  weedless::config::Config read(const std::filesystem::path& path);
}

