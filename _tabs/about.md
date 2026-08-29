---
# the default layout is 'page'
icon: fas fa-info-circle
order: 6
---

This blog's main focus is on rather arcane things you can do in Python and C++. Occasionally you might find posts about other things I'm working on.

## About the author
As you might have gathered by now, my name is Matthias Wippich and I tend to go by Che or Tsche online. I'm a member of the C++ committee (formally ISO/IEC JTC1/SC22/WG21) and IEEE. In my little free time I sporadically contribute to the LLVM project/clang and do a lot of gardening.

I used to study games engineering at [TUM](https://www.tum.de/) and am currently pursuing a degree in computational linguistics at [LMU](https://www.lmu.de/). To finance all this, I'm also working in the embedded firmware team of a life sciences and lab automation company.

My main interests are in metaprogramming, embedded and compilers. I also do quite enjoy messing around with Python.

### Papers
Here's a (possibly incomplete) list of ISO papers I've written:
- [P1789: Library Support for Expansion Statements](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p1789r3.pdf) accepted into C++26
- [P4234: $identifiers](https://wg21.link/p4234)

### Projects
Some of my recent projects include:
- **[rsl](https://github.com/fuquery/rsl)**: Experimental reflective reimplementations of various standard library facilities. Also adds a few experimental extensions.
- **[autoload](https://github.com/Tsche/autoload)**: Automatic symbol loading from dynamically loaded libraries.
- **[repr](https://github.com/Tsche/repr) / [repr.wtf](https://repr.wtf)**: A C++ static reflection library that mimics Python's `repr()`.
- **[magic_codec](https://github.com/Tsche/magic_codec) / [Python's Preprocessor](/posts/PythonsPreprocessor)**: A modular Python preprocessing utility that uses a custom codec.
- **[slo::variant](https://github.com/Tsche/variant/tree/develop)**: Mostly compliant `std::variant` implementation that does some experimental optimizations, including rearranging the underlying storage as a tree to reduce maximum recursion depth for alternative retrieval
- **[crashbench](https://github.com/Tsche/crashbench)**: Compile time benchmarking and diagnostic/death test utility. This uses a custom DSL to hide configuration in valid C++ attributes.
- **[diagtest](https://github.com/Tsche/diagtest)**: Diagnostics/death testing utility. Abandoned and replaced by crashbench.


### Discord
Aside from that I own the [Better C++ Discord community](https://discord.gg/byZvFu7d94). Feel free to come say hello - I go by @tsche there :)
