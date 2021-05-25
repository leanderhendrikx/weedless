// MIT License
// 
// Copyright (c) 2021 Leander Hendrikx
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "config.h"
#include "nlohmann/json.hpp"

// sys
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

// stl
#include <string>
#include <vector>

namespace nlohmann 
{
  template <typename T>
  void read(T& t, const std::string& key, const json& j)
  {
    auto it = j.find(key);
    if (it != j.end()) {
      t = it->get<T>();
    }
  }


  template <>
  struct adl_serializer<weedless::config::Dylib> {
    static void from_json(const json& j, weedless::config::Dylib& obj) 
    {
      read(obj.name, "name", j);
      read(obj.installName, "install_name", j);
      
      std::filesystem::path path;
      read(path, "path", j);
      obj.path = std::filesystem::absolute(path);
    }
  };

  template <>
  struct adl_serializer<weedless::config::Hook> {
    static void from_json(const json& j, weedless::config::Hook& obj) 
    {
      read(obj.dylibName, "dylib_name", j);
      read(obj.symbol, "symbol", j);
    }
  };
  
  template <>
  struct adl_serializer<weedless::config::Config> {
    static void from_json(const json& j, weedless::config::Config& obj) 
    {
      read(obj.dylibs, "dylibs", j);
      read(obj.hooks, "hooks", j);
      
      std::filesystem::path target;
      read(target, "target", j);
      obj.target = std::filesystem::absolute(target);
    }
  };
}

namespace weedless::config {

bool verifyDylibs(const Config& config)
{
  for (const auto& dylib: config.dylibs) {
    // The dylib has to exist!
    if (!std::filesystem::exists(dylib.path)) { 
      return false; 
    }

    // The install name can't contain @rpath since we can't
    // translate it to a unique path.
    if (dylib.installName.find("@rpath") != std::string::npos) { 
      return false; 
    }
    
    // The install name can't contain @loader_path since we can't
    // translate it to a unique path without knowing where we inject it.
    if (dylib.installName.find("@loader_path") != std::string::npos) { 
      return false; 
    }

    // The string can only contain @executable_path at the start.
    // npos (-1) or 0 are fine.
    if (dylib.installName.find("@executable_path") > 0) {
      return false;
    }
    
    // The string can only contain @loader_path at the start.
    // npos (-1) or 0 are fine.
  }
  return true;
}

bool verifyHooks(const Config& config)
{
  for (const auto& hook: config.hooks) {
    const auto matchesDylib = std::any_of(
        config.dylibs.begin(), 
        config.dylibs.end(), 
        [&hook](const auto& dylib) {
          return hook.dylibName == dylib.name;
        });
    if (!matchesDylib) { return false; }
  }
  return true;
}

Config read(const std::filesystem::path& path)
{
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Config file does not exist!");
  }

  std::ifstream cfgStr (path);
  if (!cfgStr.is_open()) {
    throw std::runtime_error("Can't open config file!");
  }

  nlohmann::json j;
  cfgStr >> j;
  Config config;
  nlohmann::read(config, "config", j);

  if (!verifyDylibs(config)) {
    throw std::runtime_error("Dylibs config verification failed!");
  }

  if (!verifyHooks(config)) {
    throw std::runtime_error("Hooks config verification failed!");
  }

  if (!std::filesystem::exists(config.target)) {
    throw std::runtime_error("Target config path does not exist!");
  }

  return config;
}
}
