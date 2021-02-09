#!@BASH@

set -ex

sphinx_bin="$1"
sphinx_opts="$2"
lang="$3"
format="$4"
meson_cur_src_dir="$5"
out_dir="$6"
out_file="$7"

$sphinx_bin $sphinx_build_opts \
  -D language=$lang \
  -b $format $meson_cur_src_dir \
  $out_dir
touch $out_file
