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
