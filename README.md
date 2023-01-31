# jp-watch

Minimalist app to recursively watch a directory for changes.

Written for ```jp-sync``` because ```fswatch``` and ```inotify-tools``` didn't produce consistent results including directories identified across platforms.

## Installation

MacOS
```
$ cd ~
$ git clone https://github.com/jdspugh/jp-watch.git
$ cd jp-watch
$ gcc -O3 -framework CoreServices -o jp-watch jp-watch-fsevents.c
$ sudo mv jp-watch /usr/local/bin
```

Linux (Ubuntu)
```
$ cd ~
$ git clone https://github.com/jdspugh/jp-watch.git
$ cd jp-watch
$ gcc -O3 -o jp-watch jp-watch-fanotify.c
$ sudo mv jp-watch /usr/local/bin
```

## Useage

```
$ jp-watch <directory to watch>
```

## Design Descisions

### MacOS

```FsEvents``` on MacOS work very well and as expected. The implementation also easily picks up new files added to recursively watched directories.

### Linux (Ubutnu)

I tried ```inotify``` but it doesn't pick up on new files/folders added to recursively watched directories. This was a show stopper for me. Then I discovered fanotify, which inotify actually uses internally. ```fanotify``` is available on all modern Linux operating systems (kernels versions > 2.6.37).

I also had a quick look at ```kqueue``` but I felt this tool was just adding overheads and was not low level enough.

### Rust vs C

After coding these tools in Rust, I decided to go for pure C. The main reason being that the binary sizes are smaller. C has no runtime to include in the binary unlike Rust and C++. This may not be a huge deal but my intention is that this software becomes a small command line tool / library of operating systems and as such it should avoid bloat as much as possible.

Getting setup with C and the overall development flow was ultra smooth and fast. With Rust I had to install Cargo and it had to install some packages. Not a big deal but it was a smoother and faster experience with C.

Performance is debatable between C,C++ and Rust but C seems to have an edge much like driving a manual car vs automatic.
