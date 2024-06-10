---
title: Implicit main
date: 2024-06-10T23:36:29+00:00
categories: [Python]
tags: [Python, Hackery]
author: Che
bokeh: true
---

In Python the main entrypoint into an application is idiomatically defined as
```py
def main():
  """ main entry point """

if __name__ == "__main__": # entry guard
  main()
```

If we were to call `main()` at module scope without the entry guard and import this module in another script, `main` would get executed regardless at import time. This is likely not desired behavior, especially considering our `main` would receive the command line arguments used for the invocation of the other script. Additionally if we would pickle such a guardless script, unplickling it would then cause an import of the guardless script and therefore invoke `main`.

### But how does the entry guard work?

If you run a Python script as the main program (ie `python script.py`), the special variable `__name__` will be [set to the string `"__main__"`](https://docs.python.org/3/library/__main__.html#name-main). However if our module is imported (as by `import script`) `__name__` at the scope of our script will be set to the [fully qualified name of the module](https://docs.python.org/3/reference/import.html#name__). It is therefore sufficient to compare `__name__` to the hard-coded string `"__main__"`.

## Building an extension system
Projects easily accumulate tons of utility scripts alongside of them. Suppose we extract various commonly used functionality (such as config file parsing, generated command line interface, ...) out into a library that can be imported from our various utility scripts. This still leaves one issue: Even though a command line interface could be generated for us, we still need to write an entry guard and call it. In every script.

We can do better than that. Let's first introduce an `Extension` type for the library. Scripts that import our library shall be made runnable if and only if they contain exactly one class that derives from `Extension`. Additionally our library shall define an entry point of its own - naming one of the `Extension` child classes shall run that extension.

### Minimal AST parsing

### Importing extensions automatically

### Implicit Main: Making extensions directly runnable

To inject this into our module as soon as it imports our utility library, we can instead insert the following into our library's package-level `__init__.py`
```py
from pathlib import Path

def main(command):
  """ Generated main """
  # TODO run command

def check_direct_run():
  import __main__
  if not (importer := getattr(__main__, '__file__', None)):
    # if `__main__.__file__` is not set we cannot possibly check its suffix
    # => assume we weren't directly ran
    return

  importer = Path(importer)
  if importer.suffix == '.py':

    main(importer.name)

# always executed at import time
check_direct_run()
```

