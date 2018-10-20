#!/bin/bash

set -e

if [ "x$#" != x6 ]
then
  echo 'Illegal number of parameters' 2>&1
  exit 2
fi

embedder_py="$1"
luac="$2"
plainname="$3"
input_path="$4"
output_c_path="$5"
output_h_path="$6"

"$luac" -s -o "$output_c_path.luac" -- "$input_path"

exec -- "$embedder_py" "$plainname" "$output_c_path.luac" "$output_c_path" "$output_h_path"

# vi:ts=2:sw=2:et
