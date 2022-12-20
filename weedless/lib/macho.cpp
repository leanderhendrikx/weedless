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

#include "macho.h"

// mach-o
#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

// c
#include <cstring>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>


// stl
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

// weedless
#include "uleb.h"
#include "config.h"

namespace weedless {
namespace {
template <typename CommandType>
CommandType* getLoadCommand(std::uint32_t index, const struct mach_header_64& machHeader)
{
  struct load_command* currentLc = (struct load_command*)((std::intptr_t)&machHeader + sizeof(struct mach_header_64));
  for (std::uint32_t i = 0; i < machHeader.ncmds; i++) {
    if (i == index) {
      return reinterpret_cast<CommandType*>(currentLc);
    }
    currentLc = (struct load_command*)((std::intptr_t)currentLc + currentLc->cmdsize);
  }
  return nullptr;
}

template <typename CommandType>
void appendLoadCommand(
    const CommandType& command, 
    struct mach_header_64& machHeader)
{
  auto* startNewLc = (uint8_t*)(
        (std::intptr_t)&machHeader + 
        sizeof(struct mach_header_64) +
        machHeader.sizeofcmds);

  for (std::size_t index = 0; index < command.cmdsize; index++) {
    if (startNewLc[index] != 0) {
      throw std::runtime_error("Not enough space to inject load_command!");
    }
  }

  memcpy(startNewLc, &command, command.cmdsize);
  machHeader.sizeofcmds += command.cmdsize;
  machHeader.ncmds += 1;
}

template <typename CommandType>
std::vector<CommandType*> getLoadCommands(std::vector<std::uint32_t> commandTypes, const struct mach_header_64& machHeader)
{
  std::vector<CommandType*> loadCommands;
  struct load_command* currentLc = (struct load_command*)((std::intptr_t)&machHeader + sizeof(struct mach_header_64));
  for (std::uint32_t i = 0; i < machHeader.ncmds; i++) {
    for (const auto commandType: commandTypes) {
      if (currentLc->cmd == commandType) {
        loadCommands.push_back(reinterpret_cast<CommandType*>(currentLc));
      }
    }
    currentLc = (struct load_command*)((std::uintptr_t)currentLc + currentLc->cmdsize);
  }
  return loadCommands;
}

struct segment_command_64* getSegmentCommand(const char* name, const struct mach_header_64& machHeader)
{
  for (auto* segCmd : getLoadCommands<struct segment_command_64>({LC_SEGMENT_64}, machHeader)) {
    if (strcmp(segCmd->segname, name) == 0) {
      return segCmd;
    }
  }
  return nullptr;
}

struct dyld_info_command* getDyldInfoCommand(const struct mach_header_64& machHeader)
{
  auto dyldInfoCmds = getLoadCommands<struct dyld_info_command>({LC_DYLD_INFO_ONLY}, machHeader);
  assert(dyldInfoCmds.size() == 1 && "There should only be 1 such LC!"); 
  return dyldInfoCmds[0];
}

struct std::vector<struct dylib_command*> getLoadDylibCommands(const struct mach_header_64& machHeader)
{
  auto loadDylibCmds = getLoadCommands<struct dylib_command>({LC_LOAD_DYLIB, LC_LOAD_UPWARD_DYLIB, LC_LOAD_WEAK_DYLIB}, machHeader);
  auto loadDylinkerCmd = getLoadCommand<struct dylib_command>(LC_LOAD_DYLINKER, machHeader);
  loadDylibCmds.insert(loadDylibCmds.begin(), loadDylinkerCmd);
  return loadDylibCmds;
}

struct mach_header_64* getMachHeader(void* machoPtr) {
  return (struct mach_header_64*)machoPtr;
}

struct SymbolInfo
{
  const char* symbolName;
  int dylib;
};

struct LazyBindingInfo
{
  uint8_t* symbolNamePtr;
  uint8_t* dylibIndexPtr;

  bool isValid() { return symbolNamePtr && dylibIndexPtr; }

  int dylibBindOpcode;

  const char* getSymbolName() const { return (const char*) symbolNamePtr; }
  uint8_t getDylibIndex() const { 
    if (dylibBindOpcode == BIND_OPCODE_SET_DYLIB_ORDINAL_IMM) {
      return dylibIndexPtr[0] & BIND_IMMEDIATE_MASK; 
    }
   
    if (dylibBindOpcode == BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB) {
      const uint8_t* ptr = dylibIndexPtr+1;
      return read_uleb128(ptr, ptr+16).first; 
    }

    return 0;
  }

  void setDylibIndex(uint64_t index)
  {
    if (dylibBindOpcode == BIND_OPCODE_SET_DYLIB_ORDINAL_IMM) {
      if (index > 15) {
        throw std::runtime_error("Dylib ordinal does not fit in imm opcode (not supported)");
      }
      dylibIndexPtr[0] = BIND_OPCODE_SET_DYLIB_ORDINAL_IMM + (index & BIND_IMMEDIATE_MASK);
    }
       
    if (dylibBindOpcode == BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB) {
      write_uleb128(dylibIndexPtr+1, index, 1); 
    }
  }

};

std::vector<LazyBindingInfo> getLazyBindingInfo(const struct mach_header_64& machHeader) {

  const auto* dyldInfoCmd = getDyldInfoCommand(machHeader);
  if (!dyldInfoCmd) { 
    throw std::runtime_error("Could not get dyld_info_command!"); 
  }
  
  uint8_t* lazyBindInfo = (uint8_t*)((intptr_t)&machHeader + dyldInfoCmd->lazy_bind_off);
 
  std::vector<LazyBindingInfo> lazyBindingInfos;
  LazyBindingInfo currentInfo;
  for (int i = 0; i < dyldInfoCmd->lazy_bind_size; i++) {
    switch(lazyBindInfo[i] & BIND_OPCODE_MASK)
    {
      case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM: {
        currentInfo.symbolNamePtr = lazyBindInfo+i+1; 
        i += strlen(currentInfo.getSymbolName())+1;
        break;
      }
      case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM: {
        currentInfo.dylibIndexPtr = lazyBindInfo + i;
        currentInfo.dylibBindOpcode = BIND_OPCODE_SET_DYLIB_ORDINAL_IMM;
        break;
      }
      case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB: {
        currentInfo.dylibIndexPtr = lazyBindInfo + i;
        currentInfo.dylibBindOpcode = BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB;
        
        const uint8_t* ptr = lazyBindInfo+i;
        auto [data, size] = read_uleb128(ptr, lazyBindInfo+dyldInfoCmd->lazy_bind_size); 
        i += ceil((float)size/8); 
        break;
      }
      case BIND_OPCODE_DONE: {
        if (!currentInfo.isValid()) { break; }
        lazyBindingInfos.push_back(currentInfo);
        memset(&currentInfo, 0, sizeof(SymbolInfo));
        break;
      }
      case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
      case BIND_OPCODE_SET_TYPE_IMM:
      case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
      case BIND_OPCODE_THREADED: {
        break;
      }
      
      case BIND_OPCODE_SET_ADDEND_SLEB:
      case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
      case BIND_OPCODE_ADD_ADDR_ULEB:
      case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
      case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:{
        const uint8_t* ptr = lazyBindInfo+i;
        auto [data, size] = read_uleb128(ptr, lazyBindInfo+dyldInfoCmd->lazy_bind_size); 
        i += ceil((float)size/8); 
      }
    }
  }

  return lazyBindingInfos;
}

void injectDylib(const std::string& dylibPath, void* machoPtr) {
  auto* machHeader = getMachHeader(machoPtr);
  if (!machHeader) { 
    throw std::runtime_error("Could not get mach_header."); 
  }
 
  std::size_t cmdSize = sizeof(struct dylib_command) + dylibPath.length() +1;
  auto * loadDylibCmd = (struct dylib_command*)malloc(cmdSize);

  loadDylibCmd->cmd = LC_LOAD_DYLIB;
  loadDylibCmd->cmdsize = cmdSize;
  loadDylibCmd->dylib.compatibility_version = 1;
  loadDylibCmd->dylib.current_version = 1;
  loadDylibCmd->dylib.name.offset = sizeof(struct dylib_command);
  loadDylibCmd->dylib.timestamp = 2;

  void* dylibNameStart = (void*)((intptr_t)loadDylibCmd + loadDylibCmd->dylib.name.offset); 
  memcpy(dylibNameStart, dylibPath.c_str(), dylibPath.length());

  appendLoadCommand(*loadDylibCmd, *machHeader);

  free (loadDylibCmd);
}

std::optional<std::size_t>
getDylibLoadCmdIndexByName(const char* dylibName, const mach_header_64& machHeader)
{
  auto dylibLoadCmds = getLoadDylibCommands(machHeader);
  for (size_t dylibIndex = 0; dylibIndex < dylibLoadCmds.size(); dylibIndex++) {
    const auto* dlc = dylibLoadCmds[dylibIndex];
    const char* curDylibName = (const char*)((intptr_t)dlc + dlc->dylib.name.offset); 
    if (strcmp(dylibName, curDylibName) == 0) {
      return dylibIndex; 
    }
  }
  return std::nullopt;
}

void patchMachOImpl(void* machoPtr, const config::Config& config)
{
  const auto* machHeader = getMachHeader(machoPtr);
  if (!machHeader) { 
    throw std::runtime_error("Could not get mach_header."); 
  }

  for (const auto& dylib: config.dylibs)
  {
    auto hookDylibIndex = 
      getDylibLoadCmdIndexByName(dylib.installName.c_str(), *machHeader);

    if (!hookDylibIndex.has_value()) {
      injectDylib(dylib.installName, machoPtr);
    }
  }

  auto lazyBindingInfos = getLazyBindingInfo(*machHeader);

  for (const auto& hook : config.hooks) {
    const auto* hookDylib = config.getDylibByName(hook.dylibName); 
    if (hookDylib == nullptr) {
      continue;
    }
    auto hookDylibIndex = 
      getDylibLoadCmdIndexByName(hookDylib->installName.c_str(), *machHeader);
    if (!hookDylibIndex.has_value()) {
      throw std::runtime_error("Can't find dylib index!");
    }

    for (auto& lbi: lazyBindingInfos) {
      if (strcmp(lbi.getSymbolName(), hook.symbol.c_str()) == 0) {
        lbi.setDylibIndex(*hookDylibIndex);            
      }
    }
  }
}

template <typename ProcessFn, typename... Args>
void processMachO(
    const std::filesystem::path &path, 
    ProcessFn fn, 
    Args... args)
{
  int fd;

  if ((fd = open(path.c_str(), O_RDWR)) < 0) {
    throw std::runtime_error("Could not read input file.");
  }

  lseek(fd, 0, SEEK_SET);
  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    throw std::runtime_error("Could not get file info.");
  }

  void* machoPtr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (machoPtr == MAP_FAILED) {
    throw std::runtime_error("Could not map file.");
  }
  
  fn(machoPtr, args...);
  
  if (msync(machoPtr, st.st_size, MS_SYNC) == -1) {
    close(fd); 
    throw std::runtime_error("Unable to sync file to disk.");
  }

  if (munmap(machoPtr, st.st_size) == -1) {
    close(fd); 
    throw std::runtime_error("Unable to unmap file..");
  }

  close(fd);
}

}

void patchMachO(const config::Config& config) {
  processMachO<>(config.target, patchMachOImpl, config);
}
}
