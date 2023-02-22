<div align="center">
  <img style="height:80px" src="https://jdspugh.github.io/jp-watch.png" />
  <h1>jp-watch</h1>
</div>

# About

Lightweight, cross platform app to efficiently watch a set of files and/or directories recursively for changes on Linux and MacOS.

# Installation

MacOS
```
$ cd ~
$ git clone https://github.com/jdspugh/jp-watch.git
$ cd jp-watch
$ clang -O3 -framework CoreServices -o jp-watch jp-watch.c
$ sudo mv jp-watch /usr/local/bin
$ jp-watch --help
```

Linux
```
$ cd ~
$ git clone https://github.com/jdspugh/jp-watch.git
$ cd jp-watch
$ gcc -O3 -o jp-watch jp-watch.c
$ sudo mv jp-watch /usr/local/bin
$ sudo jp-watch --help
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

# Why?

* ```inotify-tools``` is not available on MacOS
* ```fswatch``` on Linux can't produce updates events using the ```--event Updated``` option when using the ```inotify_monitor``` monitor type

# Design Descisions

## Rust vs C

After coding these tools in Rust, I decided to go for pure C. The main reason being that the binary sizes are smaller. C has no runtime to include in the binary unlike Rust and C++. This may not be a huge deal but my intention is that this software becomes a small command line tool / library to be included in operating systems and as such it should avoid bloat as much as possible.

Getting setup with C and the overall development flow was ultra smooth and fast. With Rust I had to install Cargo and it had to install some packages. Not a big deal but it was a smoother and faster experience with C.

Performance is debatable between C,C++ and Rust but C seems to have an edge and it feels much like driving a manual car vs automatic.
