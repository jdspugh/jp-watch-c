<div align="center">
  <img style="height:80px" src="https://jdspugh.github.io/image/jp-watch/jp-watch-logo.png" />
  <h1>jp-watch</h1>
</div>

# About

Lightweight, cross platform app to efficiently watch a set of files and/or directories recursively for changes on Linux and MacOS.

File and directory creation, deletion and attribute changes are reported. Output is one absolute path per line e.g.:
```
/home/user/new-file
/home/user/new-dir/
/tmp/file5349872
```

# Installation

MacOS
```
$ cd ~
$ git clone https://github.com/jdspugh/jp-watch-c.git
$ cd jp-watch-c
$ clang -O3 -framework CoreServices -o jp-watch jp-watch.c
$ sudo mv jp-watch /usr/local/bin
$ jp-watch --help
```

Linux
```
$ cd ~
$ git clone https://github.com/jdspugh/jp-watch-c.git
$ cd jp-watch-c
$ gcc -O3 -o jp-watch jp-watch.c
$ sudo mv jp-watch /usr/local/bin
$ jp-watch --help
```

# Examples

Watch the local directory
```
$ jp-watch .
```

Watch the whole drive
```
$ jp-watch /
```

Watch a few different files and directories with a combination of relative and absolute paths
```
$ jp-watch README.md jp-watch.c /var/www /tmp
```

# Limitations

`jp-watch` is not designed to watch a large number (hundreds or thousands) of different directories. The more files and directories you pass as parameters, the more cpu and memory the app will consume with a difficulty of O(1). It would be better to watch the containing directories of the files and directories you want to watch instead.

# Articles

Read my [blog post](https://jdspugh.github.io/2023/02/23/jp-watch-c.html) for the full details on this tools, it's design and implementation.

# Special Thanks to

[Code Review Stack Exchange](https://codereview.stackexchange.com/questions/283521/minimalist-recursive-file-watcher-macos-linux)
