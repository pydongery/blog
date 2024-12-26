---
title: Macro Python
date: 2024-11-05T04:20:00+02:00
categories: [Python]
tags: [Python, shenanigans, preprocessing, language extensions, procedural macros]
author: Che
---

In this blog post we will explore implementing a macro system for Python by abusing source encodings.

As I've mentioned in my [last blog post](/posts/PythonsPreprocessor) - source encodings are awesome and allow us to easily extend Python by preprocessing modules _before_ they are compiled and executed. Python already comes with a mechanism to opt into such preprocessors through [encoding declarations](https://peps.python.org/pep-0263/#defining-the-encoding). Since encodings can also be specified when opening a file, this means the macro preprocessor can also be used in a standalone fashion.

To avoid having to re-write a bunch of boilerplate, we will build upon the [`magic_codec`](https://github.com/Tsche/magic_codec) codec from the last post. If there is interest in this sort of thing, this might be the first post in a series of posts about `magic_codec`-based preprocessors for various Python language extensions.

## What is a macro anyway?
Macros are essentially rules that tell us how to transform some input into some output, or in other words they are code-to-code transformations. Since code can be represented at various levels of abstraction, macros also come in many flavors.

### Text-substitution macros
The simplest kind of macros are **text-substitution macros**. In their most primitive form they treat source code as just text and therefore map sequences of characters (input) to new sequences of characters (output). No understanding of the code's syntax or semantics is required whatsoever, making this approach very straightforward but severely limited.

For example, defining new macros within the file being processed requires further lexical analysis. At the very least the source code must be tokenized in some way that allows us to later find and parse macro definitions and all macro expansion sites. Note that we still do not need a deep understanding of the language's grammar.

At this point we can perform the substitutions at the token, rather than the character level. This approach is particularly advantageous in whitespace-sensitive languages like Python, where proper indentation is crucial to prevent syntax errors.

Since simple replacements are not a very powerful tool, many macro systems implement **parameterized macros**. The perhaps most popular example of parameterized macros would be function-like macros in C or C++. Consider the following example:
```c++
#define PI 3         // parameterless (object-like) macro
#define INC(x) x + 1 // parameterized (function-like) macro

// simple expansion
PI // -> 3

// parameterized expansion - looks like a function call
INC(5) // -> 5 + 1
```
Even though function-like macros in C/C++ are severely limited (ie. they cannot recurse) and are fundamentally still text-substitution macros, this allows for [absolutely crazy stuff](https://github.com/rofl0r/chaos-pp) and is a [mind-blowing rabbit hole](https://jadlevesque.github.io/PPMP-Iceberg/) in itself. However, this kind of macro allows us to extend Python's grammar as we wish - the macro's input does not need to be valid Python.

### Syntactic macros
More sophisticated macro systems go yet another step further and operate on the **abstract syntax tree (AST)** of the code rather than raw text or tokens. These are called **syntactic macros** and have the nice property of preserving the lexical structure of the original program. Once again this helps to greatly reduce the risk of producing invalid code after macro expansion. Interestingly, this approach has already been proposed for Python in [PEP 638](https://peps.python.org/pep-0638/).

As a Python developer you might already be familiar with a very similar concept - **decorators**. Decorators are runtime constructs that transform functions or classes. Consider the following example:
```py
def foo(target: callable) -> callable:
  def replacement(param: int) -> int:
    return target(42)
  return replacement

@foo
def bar(param: int) -> int:
  return param * 10
```
The decorator syntax is just syntactic sugar, and would be equivalent to writing `bar = foo(bar)` directly after the function definition of `bar`. Syntactic macros operate similarly, but transform AST nodes instead of runtime objects.

By reusing the [ast](https://docs.python.org/3/library/ast.html) standard library module, we could therefore write a syntactic Python macro like this:
```py
from ast import *

def foo(target: AST) -> AST:
  # note that this currently does not use the input
  return FunctionDef(name='bar', 
                     args=arguments(posonlyargs=[], 
                                    args=[arg(arg='param', 
                                              annotation=Name(id='int'))], 
                                    kwonlyargs=[], 
                                    kw_defaults=[], 
                                    defaults=[]), 
                     body=[Return(value=BinOp(left=Constant(value=42), 
                                  op=Mult(), 
                                  right=Constant(value=10)))], 
                     decorator_list=[], 
                     returns=Name(id='int'))

# hypothetical use
@foo 
def bar(param: int) -> int:
  return param * 10
```
Since macro decorators transform code before execution rather than during, this allows for zero runtime overhead decorators. However it also necessitates some new syntax to distinguish macros from regular decorators.

### Procedural and declarative macros
In the example above, Python is used as macro language. This allows us to use the [`ast`](https://docs.python.org/3/library/ast.html) and [`tokenize`](https://docs.python.org/3/library/tokenize.html) modules in macros. Additionally there is no need to learn _yet another_ programming language just to use macros.

Since Python is a procedural language, this approach gives us **procedural macros**. The Rustaceans among you might already be familiar with [procedural macros](https://doc.rust-lang.org/reference/procedural-macros.html), so let's talk about Rust's [_other kind_ of macros](https://doc.rust-lang.org/reference/macros-by-example.html) as well - **declarative macros**.

Declarative macros are defined by rules, consisting of a matcher and a transcriber. A matcher describes syntax the rule will match on, while the transcriber describes the syntax that will replace a successful match. During macro invocation all rules are then tried in order and the first match will be transcribed. For example:
```rust
macro_rules! foo {
    (7) => { 2 };
    ($arg: literal) => { 42 * $arg };
}

fn main() {
    println!("x: {}", foo!(5) * foo!(7));
}
```
The order of rules in declarative macros is significant. In this example `foo!(5) * foo!(7)` would expand to `42 * 5 * 2` ([CE](https://godbolt.org/z/4cvPdrxxr)). However, if we were to switch the macro rules and instead define `foo` as
```rust
macro_rules! foo {
    ($arg: literal) => { 42 * $arg };
    (7) => { 2 };
}
```
Then `foo!(5) * foo!(7)` would then expand to `42 * 5 * (42 * 7)` ([CE](https://godbolt.org/z/zhGo4bjPE)), since the first rule already matches all literals - including `7`.


Some macro systems even allow macros to define other macros, a feature most commonly associated with Lisp. If our Python macro preprocessor allowed macros to process inputs other than valid ASTs, this meta-macro behavior could be used to implement Rust-like declarative macros atop our regular procedural macros. More on this later.

## Design considerations
This leaves us with three main questions:
- How are macros defined?
- How are macros used?
- How do macros interact with the import system?

### Defining macros
To make our life a little easier and avoid scoping issues later on, an important first design decision is to only allow macro definitions at module (or file)-scope. Furthermore let's establish that macros can be one of three entities: functions, classes or variables. 

Macros shall only be valid in the context of the macro preprocessor - they must not be leaked into the final expanded code. This implies that compiling a module that uses the `magic.macro` codec would result in the same Python bytecode as manually preprocessing and compiling the result would.

#### `macro` keyword
Similarly to `async` a keyword could be used to mark a function, class or variable as macro.
```py
macro def foo(): ...

macro class Bar: ...

async macro bar(): ...
# same as
macro async bar(): ...

macro X = "234"
```
This works fairly well to introduce macros and syntax highlighters aren't too unhappy about it. Unfortunately this doesn't help us much to differentiate macro invocations from regular function calls and adds considerable parsing complexity.

#### Bang names
Another way to differentiate macros from other code is to expect another token after the name which can never appear there in valid Python. A good candidate is `!` - the only time it can ever appear after an identifier is in f-strings (ie `f"{foo!s}`).

```
replacement_field ::=  "{" f_expression ["="] ["!" conversion] [":" format_spec] "}"
conversion        ::=  "s" | "r" | "a"
```

Unfortunately `f"{foo!s} bar"` would be ambiguous - it could either mean `f"{str(foo)} bar"` or `f"{foo! s} bar"` which might be valid if `foo!` expands to `3 +` or similar. If you'd really mean to do that, you could however simply write `f"{(foo! s)} bar` to get rid of the ambiguity. Therefore we can prioritize the vanilla Python meaning by scanning ahead for a conversion specifier followed by either `:` or `}`.


##### Mangling
Since `!` cannot occur in a valid Python identifier, bang names must be transformed in some way. The easiest solution would be to mangle bang names (ie. `foo!` -> `__macro__foo`) before the macro interpreter runs that code. 
For example consider this code snippet:
```py
def cube!(x: int):
    return x ** 3

x = 3 + cube!(12)
assert x == 1731
```

Before executing the definition of the `cube` macro, its name must be mangled. The resulting code will look something like this:
```py
def __macro__cube(x: int):
    return x ** 3
```

Next `__macro__cube(12)` is evaluated to replace `cube!(12)` with `1728`. The final preprocessing result should then be:
```py
x = 3 + 1728
assert x == 1731
```

While this introduces noise in possibly thrown exceptions, we can use this techique to allow macros to use (soft-)keywords as name. Since there is only 39 keywords including soft keywords at the time of writing, we can get away with doing this only if we have to.

##### Disambiguation
Alternatively `!` can be used as disambiguator to force execution in the context of the macro processor. In this case it would be sufficient to simply drop the `!` token rather than mangling the name (unless required).

This also means that `!` can be used to invoke a macro regardless of how it was introduced.
```py
macro def foo():
  return 1

x = foo!() # 1

@macro
def foo():
  # redefinition of `foo`
  return 2

x = foo!() # 2

def foo!():
  # redefinition of `foo`
  return 3

x = foo!() # 3
```

#### `@macro` decorator
```py
@macro
def foo():
    ...

@macro
class Bar:
    ...
```
The downside of this is that we can only decorate functions and classes, which should be fine for our purposes. Unlike the other proposed ways to introduce macros, we can easily make this valid Python without any preprocessing by simply defining a `macro` decorator beforehand.

A magic `macro` decorator could of course accept arguments to tell our preprocessor more about the macro. This makes this approach useful enough to be the best choice thus far. Interestingly [PEP 638](https://peps.python.org/pep-0638/) opted for this choice as well - it proposes a decorator `macro_processor` in the `macros` module to mark a function as a macro.

### Using macros
#### Imports
In many contexts we need to name some imported type or object before having the opportunity to do a local import first, so we need a mechanism to invoke import statements in the macro processor. For this we can use the `macro` soft keyword. [PEP 638](https://peps.python.org/pep-0638/) chose to reuse the concept of bang names and introduce two new keywords `import!` and `from!`.
```py
macro import token
import! token

macro from functools import partial
from! functools import partial
# optionally we could do
from functools import! partial
# or even a combination of both
from! functools import! partial
```

#### Evaluating as expressions
The first possible use that will come in handy is evaluating a macro as an expression. For example consider:
```py
@macro
def foo(bar):
    return bar * 2

print(foo(7)) # preprocessed to print(14)
```
Since `foo` is valid in the context of the macro processor, it can be evaluated. Note that all arguments to the `foo` macro must be constants or be visible in the context of the macro processor.

Unfortunately this can easily cause conflicts. In the following example `Optional`, `cache` and `foo` are valid within the macro processor, but are also used in regular code. Even if macros are registered/tagged in some way, `foo(None)` would still be ambiguous. To rectify this issue, usage of the macro disambiguator `!` can be enforced.

```py
from typing import Optional
from functools import cache

from! typing import Optional # macro context
from! functools import cache # macro context

@cache # must be applied in macro context
@macro
def foo(x: Optional[int]):
    return x or 42

@cache
def foo(x: Optional[int]):
    return x or 12

bar = foo!(None) # macro invocation, preprocesses to 42
baz = foo(None) # not a macro invocation, evaluates to 12
```

If we require the disambiguator to be used **outside** of macro context, we can lift the requirement of having macro arguments be actual constants. Instead we can pass the actual list of tokens of the macro invocation's argument list to the macro. This allows us to write macros such as `rpn!(+ 3 4)`.

This has the nice side-effect of doubling as a shorthand for `rpn(tokenize("+ 3 4"))` **within** macro context, while not precluding direct invocation of other funtions defined in macro context. 

#### Decorating functions or classes
To quickly recap: Decorators allow us to wrap functions or classes. Decorator expressions must return a callable, which is invoked when the function is defined. So essentially
```py
@some_decorator
@another_decorator(with_args=True)
def foo():
    print("bar")
```
is equivalent to writing
```py
def foo():
    print("bar")

foo = some_decorator(another_decorator(with_args=True)(foo))
```
If you are interested in the design considerations behind decorators, please refer to [PEP 318](https://peps.python.org/pep-0318/). 


Unfortunately this introduces one more indirection. By allowing macros to be used in decorator expressions, we can implement zero-overhead decorators. Since runtime functions/classes do not exist at preprocessing time, we need our macros to receive the function/class's code as list of tokens, AST or source string instead.

This allows us to write code like
```py
@flombigate!
@bonkle!(123, some_arg=None)
@cache
def foo(x: int):
    print(f"bar {x}")
```

[PEP 614](https://peps.python.org/pep-0614/) significantly reduced the restrictions on what can appear in a decorator expression in Python 3.9. To keep things simple we will not attempt to look for macro invocations in more complex decorator expressions.

#### Statement macros

So far we're able to preprocess expressions, functions and classes. To make our macros even more powerful, we could also pass them suites - that is either an indented block of code or a (possibly semicolon-separated collection of) statement(s) followed by a newline.
```py
main!:
    print("foo")
```
This raises the question whether sister-macros should be allowed. A sister-macro would consume the next suite and allow implementing multi-block statements like
```py
try!:
    foo()
finally!: print("bar")
```

The `@macro` decorator can be used to not only control the kind of macro, but also express sister-macros. For example:
```py
@macro(kind="statement")
def try!(tokens):
    yield from tokens

@macro(kind="statement", after=try!)
def finally!(tokens):
    yield from tokens
```
Whenever we find a `try!` macro use, we must scan for `finally!` before returning to normal operation. `finally!` is otherwise not considered for macro invocations. By using the bang names in the macro definition, we can effectively use keywords as macro names.

Unfortunately this implies additional semantic analysis, which in turn increases the complexity of our preprocessor significantly. [PEP 638](https://peps.python.org/pep-0638/) proposes this, but it is not implemented in the example implementation of the ideas discussed in this blog post.

### Import behavior
To avoid having to rewrite macros in every module, we need some way to interact with the import system. As discussed before, `import!` and `from!` can be used to import entities in the context of the macro preprocessor. This allows us to implement macro logic as regular code and use it as macro later on, for example:
```py
from! macro_logic import some_macro

@some_macro!
def transform_this():
    print(123)
```

So far so good. Since we already established that macros only exist in the context of the preprocessor, things get a lot more hairy once we attempt to import actual macros. To get this to work we need to compile and cache the macro code separately from the rest of the module.


## Parsing utilities
Since we introduce new syntax _and_ want macros to be able to operate on raw token streams we need to do a fair bit of parsing. Note that we do not have to actually parse all of Python - we can get away with doing the least amount of work required.

Just like most classic parsers, we first need to tokenize the source code. Fortunately Python's builtin [`tokenize`](https://docs.python.org/3/library/tokenize.html) already provides us with a tokenizer that's sufficient for our purposes. `tokenize.tokenize` is a generator that yields one token at a time. To make our life a little easier we want to be able to peek ahead and roll back to a previous position in the token stream if so desired. Unfortunately neither is directly supported by the `tokenize` API - so let's wrap it.

Similarly to the `Tokenizer` introduced in Guido's excellent series of [PEG parsing posts](https://medium.com/@gvanrossum_83706/peg-parsing-series-de5d41b2ed60) (specifically part 2), we need three functions:
* `next()` yields the next token and caches it
* `mark()` returns the current position within the token stream
* `reset(pos: int)` sets the current position within the token stream to `pos`

While this is already sufficient for generated parsers, it doesn't exactly lead to nice code. To avoid having to remember positions at multiple levels to backtrack to the correct one as soon as an alternative isn't matched, we can instead use context managers. For this to work we need to track the position before the context manager was entered and conditionally reset to it _if_ parsing was unsuccessful, which can be expressed by throwing an exception.

```py
class Cancellation(Exception):
    """ This is thrown whenever parsing some alternative was not successful """

class TokenStream(PeekableStream):
    # assumes `TokenStream._cache` is used to cache already seen items
    # and `TokenStream.cursor` holds the current location within the token stream

    @property
    def alt(parent):
        # pylint: disable=E0213
        class Guard:
            def __init__(self, cursor: int):
                self.cursor = cursor

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback) -> bool:
                # note that this _must_ be annotated as returning a `bool` to tell
                # static analyzers that this will **not** swallow all exceptions
                if exc_type is Cancellation:
                    parent.reset(self.cursor)
                    return True
                return False
        return Guard(parent.cursor)
```

Additionally `peek()` to peek the next token without advancing the cursor and multi-token variants `peek_n(amount: int)` and `next(amount: int)` are provided. The implementation can be found in [`util.py`](https://github.com/Tsche/magic_codec/blob/develop/src/magic_codec/util.py).


To further simplify the actual parser code, we can implement more helpers such as `consume_line()`, `consume_while(predicate)`, `consume_until(predicate)`. Additionally two more utilities `expect(predicate)` and `maybe(predicate)` can be introduced to reduce the need of having to write code such as
```py
if tokens.peek() == some_token:
  tokens.next() # advance if the expected token was next
```
`expect` is only expected to appear within a `TokenStream.alt`, therefore it signifies that an unexpected token was found by throwing a `Cancellation` exception.

Now, we don't actually want to have to write the predicate as a lambda every time. This would result in code looking like
```py
with tokens.alt:
    tokens.maybe(lambda token: token.type == NAME and token.string == "async")
    tokens.expect(lambda token: token.type == NAME and token.string in ["class", "def"])
    name = tokens.expect(lambda token: token.type == NAME).string
    # ...
```

Instead we could write something along the lines of
```py
with tokens.alt:
    tokens.maybe((NAME, "async"))
    tokens.expect((NAME, ["class", "def"]))
    name = tokens.expect((NAME, ...)).string
    # ...
```

This is implemented as `TokenQuery` in [`util.py`](https://github.com/Tsche/magic_codec/blob/develop/src/magic_codec/util.py). The rules are fairly straight forward:
* `TokenQuery(NAME, "async")` exactly matches the token `Token(NAME, "async")`
* `TokenQuery(NAME, ["class", "def"])` matches `Token(NAME, "class")` and `Token(NAME, "def")`. Similarly `TokenQuery([STRING, NUMBER], "4")` would match both `Token(STRING, "4")` and `Token(NUMBER, "4")`. 
* `TokenQuery(NAME, ...)` matches any token whose type is `NAME`. Similarly `TokenQuery(..., ...)` would match any token.


## Synthesizing code

## Isolating the macro environment
To ensure anything executed by the macro processor does not poison the host environment, we can isolate by spawning another Python interpreter via `multiprocessing` or better yet the new [`interpreters`](https://peps.python.org/pep-0734/) module.

## Declarative macros
