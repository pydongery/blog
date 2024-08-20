---
title: Python's Preprocessor
date: 2024-08-19T04:20:00+02:00
categories: [Python]
tags: [Python, shenanigans, preprocessing, language extensions]
author: Che
---

Every now and the you hear outrageous claims such as "Python has no preprocessor". 

This is simply not true. In fact, Python has the **best preprocessor** of all languages - it quite literally allows us to do whatever we want, and a lot more. It's just a little tricky to (ab)use.

## Python source code encodings
Thanks to [PEP-0263](https://peps.python.org/pep-0263/#defining-the-encoding) it is possible to define a _source code encoding_ by placing a magic comment in one of the first 2 lines. 

All of the following lines would instruct the Python interpreter to decode the rest of the file using the `utf8` codec:
```py
# coding=utf8
# -*- coding: utf8 -*-
# vim: set fileencoding=utf8 :
```

To be precise, the line must match the regular expression `^[ \t\f]*#.*?coding[:=][ \t]*([-_.a-zA-Z0-9]+)`. Naturally we can use our own encodings, but their names must match `[-_.a-zA-Z0-9]+`. As you might have guessed by now - our own codec will do a whole lot more than just decode the source file.

## Path configuration files (.pth)

Unless the Python interpreter was started with the [`-S`](https://docs.python.org/3.12/using/cmdline.html#cmdoption-S) option, it will automatically load the [`site`](https://docs.python.org/3/library/site.html) package during initialization. This is done to append _site-specific_ paths to the module search path. 

One way to do so is by placing a _path configuration file_ (with `.pth` suffix) in the [`site-packages`](https://docs.python.org/3/library/site.html#index-1) folder of your target Python installation. Every line (except lines starting in `#` and blank lines) in it will be added to the module search path.

Interestingly the [Python Docs](https://docs.python.org/3.12/library/site.html#index-2) also mention the following:
> Lines starting with import (followed by space or tab) are executed.

Which gives us a nice opportunity to always execute arbitrary code during initialization of the Python interpreter. This can be used to load the custom codec - to do so create a file `packagename.pth` in [`site-packages`](https://docs.python.org/3/library/site.html#index-1) with content matching
```py
import packagename.register_codec
```
This will import the `register_codec` module from the `packagename` package. Importing this module must register the codec, which is done by registering a search function by calling [`codecs.register`](https://docs.python.org/3/library/codecs.html#codecs.register). For example:

```py
import codecs
from typing import Optional

def search_function(encoding) -> Optional[codecs.CodecInfo]:
    if encoding == "codec_name":
        return codecs.CodecInfo(
          name=encoding,
          encode=codecs.utf_8_encode,
          decode=your_decoder,
          incrementaldecoder=your_incremental_decoder
      )

codecs.register(search_function)
```
Since importing modules only executes them once, this is sufficient to register our codec's search function exactly once. This leaves one thing to do: the actual decoder.

## Defining custom codecs
Essentially we need two things to make the Python interpreter happy:
- a decode function `decode(data: bytes) -> tuple[str, int]`
- an incremental decoder class

Let's do the decode function first. `codecs.utf_8_decode` can be used for the actual decoding - this will return a tuple of the decoded content of the source file and how many bytes were consumed. The resulting string can be passed on to our actual preprocessor.

> Uncaught exceptions will not be printed with traceback to the termminal as you would expect. Instead the interpreter will simply yield
> `SyntaxError: encoding problem: your_codec` with no helpful extra information as to why there was a problem with your codec.
>
> It is therefore advisable to catch exceptions coming from your preprocessor and explicitly print them before reraising.
{: .prompt-warning }

```py
import codecs
import traceback

def preprocessor(data: str) -> str:
    # do actual preprocessing here
    return data

def decode(data: bytes) -> tuple[str, int]:
    decoded, consumed = codecs.utf_8_decode(data, errors='strict', final=True)
    try:
        # run the preprocessor
        processed = preprocessor(decoded)
    except Exception:
        # print the traceback
        print(traceback.format_exc())
        raise
    return processed, consumed
```

To get things to work nicely we also need to provide an incremental decoder. Since we don't want to actually preprocess the file incrementally, we can instead collect it into a buffer and preprocess the entire thing once the final decode call happened. For this purpose we can inherit from `codecs.BufferedIncrementalDecoder` (or [`codecs.IncrementalDecoder`](https://docs.python.org/3/library/codecs.html#codecs.IncrementalDecoder), since we will override `decode`, which provides the primary machinery, anyway). This will look something like this:

```py
import codecs

class Decoder(codecs.BufferedIncrementalDecoder):
    def _buffer_decode(self, input, errors, final):  """not used"""

    def decode(self, data, final=False) -> str:
        self.buffer += data

        if self.buffer and final:
            buffer = self.buffer
            self.reset()
            # call our decode function, return only the result string
            return decode(buffer)[0]

        return ""
```

The search function from earlier can now be updated to use the decode function and the incremental decoder class.
```py
def search_function(encoding) -> Optional[codecs.CodecInfo]:
    if encoding == "codec_name":
        return codecs.CodecInfo(
          name=encoding,
          encode=codecs.utf_8_encode,
          decode=decode, # our decode function
          incrementaldecoder=Decoder # our incremental decoder
      )
```

It does not matter if or how the source file's content is used, you can also return completely arbitrary code. However note that **the first line will be dropped** (since it is expected to contain the magic line) and it **must** be valid Python.


## Extending Python
Fortunately extending Python is rather easy since Python's standard library contains tools to tokenize and parse Python. While regular expressions may be sufficient for simple language extensions, this often tends to be rather error prone. 

If your language extension uses only valid Python tokens, it is possible to use the [`tokenize`](https://docs.python.org/3/library/tokenize.html) module to retrieve the file's token stream, modify it as required and [`untokenize`](https://docs.python.org/3/library/tokenize.html#tokenize.untokenize) the result.

If your language extension transforms syntactically valid Python, it is possible to use the [`ast`](https://docs.python.org/3/library/ast.html) module to [`parse`](https://docs.python.org/3/library/ast.html#ast.parse) the source file, modify the resulting abstract syntax tree and finally [`unparse`](https://docs.python.org/3/library/ast.html#ast.unparse) it.

### Unary increment and decrement
Unlike [many other languages](https://en.wikipedia.org/wiki/Increment_and_decrement_operators#Supporting_languages) Python is unfortunately lacking unary increment and decrement operators.

In case you're not familiar with the concept, here's a quick refresher:
* Pre-increment and pre-decrement operators modify their operand by 1 and return the value **after** doing so
* Post-increment and post-decrement operators modify their operand by 1 and return the value **before** doing so

In Python "post-increment" `x++` and "post-decrement"`x--` are syntactically not valid. 

"Pre-increment" `++x` and "pre-decrement" `--x` however are syntactically valid, but would result in a call `x.__pos__().__pos__()` or `x.__neg__().__neg__()` respectively. Keep in mind that breaking these up with extra parentheses like `+(+x)` or `-(-x)` would still result in that call.


Essentially we want to replace every occurrence of these invalid unary increment and decrement expressions into a Python expression that has the same semantics. 

One possible way to do this is to form a tuple of the `x` before mutating it and `x` after mutation. This can be used for both prefix and postfix notation - we can simply pick out whichever value we need using the tuple's subscript operator. Thanks to [PEP-0572](https://peps.python.org/pep-0572/) Python has assignment _expressions_ (also known as the walrus operator), which allow mutation of `x` but also return the resulting value.
<br>

Here's the list of replacements:

|------------------|-------------------------------------|----------------------|
| Unary expression |          Token sequence             |   Python equivalent  |
|------------------|-------------------------------------|----------------------|
|      `x++`       | `(NAME, 'x'), (OP, '+'), (OP, '+')` | `(x, x := x + 1)[0]` |
|      `x--`       | `(NAME, 'x'), (OP, '-'), (OP, '-')` | `(x, x := x - 1)[0]` |
|      `++x`       | `(OP, '+'), (OP, '+'), (NAME, 'x')` | `(x, x := x - 1)[1]` |
|      `--x`       | `(OP, '-'), (OP, '-'), (NAME, 'x')` | `(x, x := x - 1)[1]` |
|------------------|-------------------------------------|----------------------|

Simply replacing these token sequences in the token stream is striclty speaking not sufficient, since it will fail for expressions such as `x++ - -y`, however this can easily be disambiguated with extra parenthesis: `x++ - (-y)`.

[incdec.py](https://github.com/dankeyy/incdec.py), the Python project that inspired this blog post, uses regular expressions to do the replacements. While it does try to prevent replacements inside string literals, it is still rather brittle. You can find a reimplementation that directly modifies the token stream at [magic.incdec](https://github.com/Tsche/magic_codec/blob/master/src/magic_codec/builtin/incdec.py).

#### Example

An input file `incdec.py`
```py
# coding: magic.incdec
i = 6

assert i-- == 6
assert i == 5
assert ++i == 6
assert --i == 5
assert i++ == 5
assert i == 6
assert (++i, 'i++') == (7, 'i++')
print("PASSED")

```
would be transformed to

```py
i = 6

assert ((i, i := i - 1)[0]) == 6
assert i == 5
assert ((i, i := i + 1)[1]) == 6
assert ((i, i := i - 1)[1]) == 5
assert ((i, i := i + 1)[0]) == 5
assert i == 6
assert (((i, i :=i + 1)[1]),'i++') == (7, 'i++')
print ("PASSED")

```

To verify that it actually works, try running `python tests/incdec/incdec.py` in the [`magic_codec repository`](https://github.com/Tsche/magic_codec) after installing [`magic_codec`](https://pypi.org/project/magic_codec/). It should print
```
$ python tests/incdec/incdec.by
PASSED
```

### Python with braces (Bython)
Another thing C/C++ programmers usually find rather off-putting about Python is its use of indentation for scoping purposes. Unfortunately the Python developers have strong opinions on using braces for scoping, which can be confirmed by importing `braces` from `__future__`:
```py
>>> from __future__ import braces
  File "<stdin>", line 1
SyntaxError: not a chance
```

Let's do it anyway.

<br>

As with the incdec example, we can directly modify the token stream. To do so get the tokens from the source file using [`tokenize.generate_tokens`](https://docs.python.org/3/library/tokenize.html#tokenize.generate_tokens). Unfortunately `generate_tokens` expects a callable that yields one line at a time. We can get one by wrapping our string in a [`StringIO`](https://docs.python.org/3/library/io.html#io.StringIO) object and use its bound `readline` method.

<br>

Since whitespace does not matter in the input, all tokens of the types `INDENT` and `DEDENT` can be dropped. 

Tokens of the type `OP` are interesting for primary required machinery - if the token's string representation matches `{`, the indentation level needs to be increased and a `:` emitted. Likewise if the token's string representation matches `}`, the indentation level must be decreased. 

Finally to fix indentation every token of type `NL` must be followed by a token of type `INDENT` with an appropriate amount of whitespace for the current indentation level as content.

Since Python uses curly braces for dictionaries, this can be slightly improved upon by only adjusting the indentation level only if the `{` token is followed by a newline and respectively the `}` token preceeded by a newline. Limiting dictionaries with the curly brace syntax to a single line might seem rather limiting, but remember that 
```py
dictionary = { \
    'a': 420,  \
    'b': 10    \
}
```
contains no newline tokens within the curly braces.

#### Example

An input file `test.by`
```py
# coding: magic.braces
def print_message(num_of_times) {
    for i in range(num_of_times) {  
   print("braces ftw")
  print({'x': 3})
 }
}

x = {        \
  'foo': 42, \
  'bar': 5   \
}

if __name__ == "__main__" {
print_message(2)
    print({k:v for k, v in x.items()})
}
```
would be transformed to

```py
# coding: magic.braces
def print_message(num_of_times):
    for i in range(num_of_times):
        print("braces ftw")
        print({'x': 3})

x = {        \
  'foo': 42, \
  'bar': 5   \
}

if __name__ == "__main__":
    print_message(2)
    print({k:v for k, v in x.items()})

```

You can verify this by running `python tests/braces/test.by` in the [`magic_codec repository`](https://github.com/Tsche/magic_codec) after installing [`magic_codec`](https://pypi.org/project/magic_codec/). It should print
```
$ python tests/braces/test.by
braces ftw
{'x': 3}
braces ftw
{'x': 3}
{'foo': 42, 'bar': 5}
```

## Interpreting other languages

Instead of expanding Python, why not teach the Python interpreter itself a few more tricks? After all there's all kinds of cool languages it could interpret!

Some languages (ie. shell script, CMake script, PHP or Ruby) use `#` for comments, notably every language that supports [shebangs](https://en.wikipedia.org/wiki/Shebang_(Unix)) - this can be abused to set the encoding directly.

### C and C++
For C and C++ we have no such luck. Comments use `/* comment */` or `// comment` syntax, neither of which is usable. It is however possible to satisfy the source encoding pattern by using preprocessor directives, which happen to start with a `#`.

The regular expression for magic lines `^[ \t\f]*#.*?coding[:=][ \t]*([-_.a-zA-Z0-9]+)` matches, if a line contains:
- any amount of spaces, tabs or form feeds
- the `#` character
- any amount of any characters
- the word `coding`
- either `:` or `=`
- any amount of spaces or tabs
- an identifier matching `[-_.a-zA-Z0-9]+`

One preprocessor directive in C++ that can be used for this is `#define`. What we want to do is define a macro and let its value match `.*?coding[:=][ \t]*([-_.a-zA-Z0-9]+)`. For example
```c
#define CODEC "coding:magic.cpp"
```
would match. 

Great, we can now trigger the `magic.cpp` decoder with a valid C or C++ source file. To actually get the Python interpreter to interpret this C or C++ code for us, we can use the excellent package [`cppyy`](https://pypi.org/project/cppyy/). In essence [`cppyy`](https://cppyy.readthedocs.io/en/latest/) uses [`cling`](https://root.cern/cling/) under the hood to interpret our code and generates Python bindings for us to use it. 

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

Now we can run `python foo.cpp` if `foo.cpp` begins with the magic line `#define CODEC "coding:magic.cpp"`. One example implementation of this can be found at [magic.cpp](https://github.com/Tsche/magic_codec/blob/master/src/magic_codec/builtin/cpp.py).

#### Example

An input file `test.cpp`
```c++
#define CODEC "coding:magic.cpp"
#include <cstdio>

int main() {
    puts("Hello World");
}
```
would be transformed to

```py
import cppyy

cppyy.cppdef(r"""
#define CODEC "coding:magic.cpp"
#include <cstdio>

int main() {
    puts("Hello World");
}
""")
from cppyy.gbl import main

if __name__ == "__main__":
    main()

```

You can try this by running `python tests/cpp/test.cpp` in the [`magic_codec repository`](https://github.com/Tsche/magic_codec) after installing [`magic_codec`](https://pypi.org/project/magic_codec/) and [`cppyy`](https://pypi.org/project/cppyy/). It should print
```
$ python tests/cpp/test.cpp
Hello World
```


## Validating data

One data interchange format that does allow comments and uses `#` to introduce them is [TOML](https://toml.io/en/). This allows us to set an encoding and let the Python interpreter act as a validation tool instead. [jsonschema](https://pypi.org/project/jsonschema/) which is a Python implementation of [JSON Schema](https://json-schema.org/) can be used to do the actual validation.

This one is rather straight forward, a `preprocess` function could look like this:
```py
def preprocess(data: str):
    return """
import argparse
import json
import sys
import tomllib
from pathlib import Path
from jsonschema import ValidationError, validate

def main():
    parser = argparse.ArgumentParser(
                    prog='magic.toml',
                    description='Verify toml data against json schemas')
    parser.add_argument('-s', '--schema', type=Path, required=True)
    args = parser.parse_args()

    data = tomllib.loads(Path(sys.argv[0]).read_text(encoding="utf-8"))
    schema = json.loads(args.schema.read_text(encoding="utf-8"))
    try:
        validate(data, schema)
    except ValidationError as exc:
        print(exc)
    else:
        print("Successfully validated.")

if __name__ == "__main__":
    main()
"""
```

A slightly different example implementation can be found at [magic.toml](https://github.com/Tsche/magic_codec/blob/master/src/magic_codec/builtin/toml.py).

### Example

With a schema `schema.json`
```py
{
    "type": "object",
    "properties": {
        "name": {"type": "string"},
        "age": {"type": "number"},
        "scores": {
            "type": "array",
            "items": {"type": "number"}
        },
        "address": {"$ref": "#/$defs/address"}
    },
    "required": ["name"],
    "$defs": {
        "address": {
            "type": "object",
            "properties": {
                "street": {"type": "string"},
                "postcode": {"type": "number"}
            },
            "required": ["street"]
        }
    }
}
```

and an input file `data_valid.toml`
```toml
# coding: magic.toml
name = "John Doe"
age = 42
scores = [40, 20, 80, 90]

[address]
street = "Grove St. 4"
postcode = 19201
```
the expected output is
```
$ python tests/toml/data_valid.toml -s tests/toml/schema.json
Successfully validated.
```

<br>

However, for an input file `data_invalid.toml`
```py
# coding: magic.toml
name = "John Doe"
age = 42
scores = [40, "20", 80, 90]

[address]
street = "Grove St. 4"
postcode = 19201
```
the expected output will be
```
$ python tests/toml/data_invalid.toml -s tests/toml/schema.json
'20' is not of type 'number'

Failed validating 'type' in schema['properties']['scores']['items']:
    {'type': 'number'}

On instance['scores'][1]:
    '20'
```

## Conclusion

Custom codecs in conjunction with path configuration files can drastically change the behavior of the Python interpreter. While most of the examples here are written purely for entertainment purposes, there are definitely valid uses for this technique. One notable example is [pythonql](https://github.com/pythonql/pythonql), which is a query language extension for Python.

If you want to play around with your own preprocessors but do not wish to mess with `site-packages` directly, introduce path configuration files and write all the boilerplate yourself, you can instead use [`magic_codec`](https://github.com/Tsche/magic_codec). 

To extend [`magic_codec`](https://github.com/Tsche/magic_codec) with your own preprocessors, you can create another Python package whose name is prefixed with `magic_`. Setting the codec of any file to `magic_foo` would load the `magic_foo` package and check if it has a function `preprocess`.

The expected signature of `preprocess` is as follows:
```py
def preprocess(data: str) -> str:
    raise NotImplementedError
```

You can find an example extension in [example/](https://github.com/Tsche/magic_codec/tree/master/example).
