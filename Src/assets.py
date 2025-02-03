#!/bin/python3
import sys
import re

output_file = "resources.cpp"
with open(output_file, "w") as f:
    f.write("#include <unordered_map>\n#include <string>\n#include <tuple>\n\nextern \"C\" {\n")

    for symbol in sys.argv[1:]:
        var_name = "_binary_" + re.sub('[^a-zA-Z0-9]', '_', symbol)
        f.write(f"\textern const char {var_name}_start[];\n\textern const char {var_name}_end[];\n")

    f.write("}")

    f.write("\n\nstd::unordered_map<std::string, std::tuple<const char*, const char*>> _binary_assets_symbols = {\n")

    for symbol in sys.argv[1:]:
        var_name = "_binary_" + re.sub('[^a-zA-Z0-9]', '_', symbol)
        f.write(f"\t{{\"{symbol}\", {{(const char*) &{var_name}_start, (const char*) &{var_name}_end}}}},\n")

    f.write("};\n")

print(f"File {output_file} is generated.")
