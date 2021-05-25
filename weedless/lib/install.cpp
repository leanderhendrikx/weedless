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

#include "install.h"

// weedless
#include "config.h"

// stl
#include <filesystem>


namespace weedless {

std::filesystem::path 
GetFullPathFromInstallName(
    std::string installName, 
    const std::filesystem::path& executablePath)
{
  static const std::string executable_path_const = "@executable_path";

  std::size_t epIndex = installName.find(executable_path_const);
  if (epIndex != std::string::npos) {
    installName.replace(
        epIndex, executable_path_const.size(), executablePath.string());      
  }
  
  return std::filesystem::path(installName);
}


void installDylibs(const config::Config& config)
{
  std::filesystem::path path(config.target);
  for (const auto& dylib: config.dylibs) {
    const auto fullPath = 
      GetFullPathFromInstallName(
          dylib.installName, 
          config.target.parent_path()); 
    if (dylib.path != fullPath) {
      std::filesystem::copy(
          dylib.path, 
          fullPath, 
          std::filesystem::copy_options::update_existing);
    }
  }
}

}
