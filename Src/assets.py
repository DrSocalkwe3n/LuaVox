#!/bin/python3
import sys
import re

if len(sys.argv) < 3:
    print("Usage: assets.py <output_cpp> <file1> [file2 ...]")
    sys.exit(1)

output_cpp = sys.argv[1]
symbols = sys.argv[2:]

with open(output_cpp, "w") as f:
    f.write("#include <unordered_map>\n#include <string>\n#include <tuple>\n\nextern \"C\" {\n")

    for symbol in symbols:
        var_name = "_binary_" + re.sub('[^a-zA-Z0-9]', '_', symbol)
        f.write(f"\textern const char {var_name}_start[];\n\textern const char {var_name}_end[];\n")

    f.write("}\n\n")

    f.write("std::unordered_map<std::string, std::tuple<const char*, const char*>> _binary_assets_symbols = {\n")

    for symbol in symbols:
        var_name = "_binary_" + re.sub('[^a-zA-Z0-9]', '_', symbol)
        f.write(f"\t{{\"{symbol}\", {{(const char*) &{var_name}_start, (const char*) &{var_name}_end}}}},\n")

    f.write("};\n")

print(f"File {output_cpp} is generated.")
