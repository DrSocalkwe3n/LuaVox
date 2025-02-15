#!/bin/sh

function compile_shaders() {
  local source_dir="$1"

  for item in "$source_dir"/*; do
    filename=$(basename "$item")
    if [ -d "$item" ]; then
        compile_shaders "$source_dir"/"$filename"

    elif [ -f "$item" ] && [ $item -nt $item.bin ] && ([[ $filename = *'.frag' ]] || [[ $filename = *'.vert' ]] || [[ $filename = *'.geom' ]]); then
      echo $filename
      glslc $item -o $item.bin --target-env=vulkan1.2
    fi
  done
}

compile_shaders .
