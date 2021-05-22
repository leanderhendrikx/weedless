# Weedless

A function hooking tool for the Mach-O format based on dylib redirection (for educational use only).

## How does it work?
Weedless parses and modifies the dynamic linker info stored inside a Mach-O. 

### Step 1: gathering information
A Mach-O has a set of opcodes encoded inside that tells the dynamic linker in which libraries to look for all dynamically loaded symbols. Weedless interprets a limited set of those opcodes to build an overview of all dynamically loaded symbols and the libraries they come from. For each symbol, the following information is encoded: `symbol name`, `dylib index`.
Example: 
```
_strlen   | dylib index 1 
_puts     | dylib index 1 
_getValue | dylib index 2 
```
A good explanation on Mach-O and those opcodes can be found on: https://adrummond.net/posts/macho

### Step 2: injecting dylibs
Next, weedless will inject the dylibs that contain the replacement functions (hooks). It does that by adding a load dylib command (`LC_LOAD_DYLIB`) and copying the dylib to the configured location (based on the install name). The new load commands are placed after all pre-existing load dylib commands such that the dylib order doesn't change.

### Step 3: modifying opcodes
Lastly, weedless modifies the dylib index (parsed in step 1) for hooked symbols such that it now points to one of the injected libraries.
Example: 
```
_strlen   | dylib index 1 
_puts     | dylib index 1 
_getValue | dylib index 3 <- changed from 2 to 3. 
```

## Limitations
There are two ways that a symbol can be encoded in an opcode (IMM vs ULEB). If the symbol points to a very low dylib index (<16) it's encoded using an immediate in the opcode (IMM). Symbols that come from a dylib with a bigger index (>= 16) are encoded using an extra ULEB opcode. Weedless does not yet support conversion from IMM to ULEB opcodes. 

## Building
```
mkdir build && cd build
cmake .. 
cmake --build .
```

## Usage
```
weedless hooks.json
```

## Configuration
Weedless uses JSON configuration files for each binary that needs to be patched. 
Each configuration file defines what the target is, which dylibs to inject and which symbols to hook.
Example:
```
{
  "config":
  {
    "dylibs": [
      { 
        "name": "libhooks",                                 ## name for referencing in `hooks`.
        "path": "build/example/libhooks.dylib",             ## path to the hooks dylib.
        "install_name": "@executable_path/libhooks.dylib"   ## install name of the hooks dylib. (only @executable_path is supported)
      }
    ],
    "hooks": [ 
      { "symbol": "_strlen",   "dylib_name": "libhooks" },  ## hook `_strlen` with the implementation provided in `libhooks`.
      { "symbol": "_GetValue", "dylib_name": "libhooks" },
      { "symbol": "_ptrace",   "dylib_name": "libhooks" }
    ],
    "target": "build/example/example_target"                ## binary to modify.
  }
}
```

## Code signing
Any modification to a code-signed application will cause that application to crash during startup. Future version might have an option to remove the code signature from the binary. For now, there are plenty of tools available that can do that.

## Running the example
The example directory contains the following:
- `value.h/c` compiles to `libvalue.dylib`
- `example.c` compiles to `example_target` and links against `libvalue.dylib`
- `hooks.c` compile to `libhooks.dylib`
- `hooks.json` weedless config file

```
# Run the clean example binary.
./build/example/example_target

# Patch the binary using `example/hooks.json`
./build/weedless example/hooks.json

# Run the patched example binary. (Output should be different because of hooks.)
./build/example/example_target
```

## Support my work
Like this project? Buy me a coffee: BTC 3F7R4WQa5pZKewHTC7RWFL2YtKRX1AVb43
