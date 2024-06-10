---
title: Python's Preprocessor
date: 2024-06-10T23:36:29+00:00
categories: [Python]
tags: [Python, Hackery]
author: Che
bokeh: true
---

Every now and the you hear outrageous claims such as "Python has no preprocessor". This is simply not true. In fact, Python has the best preprocessor of all languages - it quite literally allows us to do whatever we want, and a lot more. It's just a little tricky to (ab)use.

# Python Source Code Encoding
According to [PEP-0263](https://peps.python.org/pep-0263/#defining-the-encoding) it is possible to define a source code encoding by placing a magic comment in one of the first 2 lines. The following lines would all be interpreted as setting the encoding to `utf8`:
```py
# coding=utf8
# -*- coding: utf8 -*-
# vim: set fileencoding=utf8 :
```

To be precise, the line must match the regular expression `^[ \t\f]*#.*?coding[:=][ \t]*([-_.a-zA-Z0-9]+)`. Naturally we can use our own encodings, but their names must match `[-_.a-zA-Z0-9]+`.

# Defining Custom Codecs
https://docs.python.org/3/library/codecs.html

# Extending Python
## Unary increment and decrement
https://github.com/dankeyy/incdec.py

In Python the postfix operators `x++` can be written as `((x, x := x+1)[0])`, `x--` is therefore `((x, x := x-1)[0])`. Using the same expression we can write the prefix operators `++x` as `((x, x := x+1)[1])` and `--x` as `((x, x := x-1)[1])` respectively. This expression works by pulling out the respective element from a tuple of itself and itself updated.

Other than that it's simply text replacement. 

# Polyglotting

Instead of expanding Python, why not teach the Python interpreter a few more tricks? After all there's all kinds of cool languages it could interpret!

## C and C++
The easiest way to smuggle the magic line into C and C++ sources is by defining a macro like
```c
#define CODEC "coding:pydong"
```
Great, we can now trigger the `pydong` decoder with a valid C or C++ source file. To actually get the Python interpreter to interpret this C or C++ code for us, we can use the excellent package `cppyy`. In essence `cppyy` uses `cling` under the hood to interpret our code and generates Python bindings for us to use it. 

After our decoder is done with the input file, the output should look something like
```py
import cppyy

# interpret the input source code
cppyy.cppdef("<input source file content>")

# find the main function
from cppyy.gbl import main

if __name__ == "__main__":
  # call C/C++ main
  main()
```

Now we can run `python foo.cpp` if `foo.cpp` begins with the magic line `#define CODEC "coding:pydong"` or similar.

## Shell script
Shell script comments start with `#`, hence we don't need to do anything special for the magic line.

## CMake
Just like Shell scripts, CMake uses `#` for comments.

## PHP
PHP allows `#` comments, for example for the shebang. 

## Ruby
Ruby uses `#` for single-line comments. This means just like PHP, we can simply use a comment for the magic line.
