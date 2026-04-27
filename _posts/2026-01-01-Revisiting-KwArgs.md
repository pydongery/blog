---
title: Revisiting Keyword Arguments
date: 2026-01-01T04:20:29+01:00
categories: [C++,]
tags: [C++, C++26, reflection, experiments]
author: Che
---

A lot has happened since I first wrote about reflection on this blog. Reflection has been voted into C++26 at the Sofia meeting in June 2025, implementations are starting to make massive progress, life is good. However, having multiple implementations also means that we must now verify previous shenanigans against multiple implementations.

Why am I bringing this up? Well, as it turns out some assumptions made for the original [keyword arguments](/KwArgs) no longer hold across all implementations. 

The lambda trick felt kind of dirty to begin with - technically implementations are free to reorder closures in the closure type however they want ([[expr.prim.lambda.capture]/10](https://standards.pydong.org/c++23/expr.prim.lambda.capture#10)), so we might accidentally mess up the mapping. While S. Davis Herring's paper [P3847](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3847r0.html) addresses this issue, we're still hitting a brick wall with the lambda attempt for another reason:
[[meta.reflection.member.queries]/2](https://eel.is/c++draft/meta.reflection.member.queries#2) states
> It is implementation-defined whether declarations of other members of a closure type _Q_ are _Q_-members-of-eligible.

This gives implementations the freedom to simply not allow us to find members corresponding to captures in the closure type. So, let's check:
```cpp
#include <meta>
#include <print>

consteval std::size_t member_count(std::meta::info R,
                                   std::meta::access_context ctx) {
  return nonstatic_data_members_of(R, ctx).size();
}

int main() {
  auto lambda = [x = 1]<typename T>(this T obj) {
    std::print("xobj: {}, ",
                 member_count(^^T, std::meta::access_context::current()));
  };
  lambda();
  std::println("ext: {}", member_count(^^decltype(lambda),
                                       std::meta::access_context::unchecked()));
}
```
[Run on Compiler Explorer](https://godbolt.org/z/364jh1Mq6). Clang prints `xobj: 1, ext: 1`, but GCC prints `xobj: 0, ext: 0` :(


Let's take a step back. Why did we use lambdas here in the first place?

Essentially the keyword argument trick consists of two parts - first, we need to parse a stringified version of the arguments; secondly, we need to make the argument list valid C++. For the latter part capture-lists of lambdas seemed interesting, but if we change syntax slightly we can do better.

## Designated initializers
If we prefix every name with a `.`, we can use designated initializers instead of lambda captures. For this to work, we only need to inject an aggregate with members of corresponding names ahead of time. Since we already have a parser from the lambda approach, let's just reuse that.
```c++
struct NameParser : _impl::Parser {
  using _impl::Parser::Parser;

  std::vector<std::string_view> names;

  constexpr bool parse() {
    cursor = 0;

    while (is_valid()) {
      skip_whitespace();

      // require dotted names
      if (current() != '.') {
        return false;
      }
      ++cursor;
      skip_whitespace();

      std::size_t start = cursor;

      // find '=', ',' or whitespace
      skip_to('=', ',', ' ', '\n', '\r', '\t');
      if (cursor - start == 0) {
        // default capture or invalid name
        return false;
      }

      names.emplace_back(data.substr(start, cursor - start));

      // skip ahead to next capture
      // if the current character is already ',', this will not move the cursor
      skip_to(',');
      ++cursor;

      skip_whitespace();
    }
    return true;
  }
};

```
{: data-line="12-15" .line-numbers }

With this out of the way we need to decide on some type for the non-static data members. Since we do not yet know the types of our arguments, whatever we choose must be constructible from anything. `std::any` comes to mind.
```cpp
template <string_constant... Names>
struct Inject {
  struct kwargs_impl;
  consteval {
    std::vector<std::meta::info> args;
    for (auto name : {std::string_view(Names)...}){
      args.push_back(data_member_spec(^^std::any, {.name = name}));
    }
    define_aggregate(^^kwargs_impl, args);
  };

  static_assert(is_complete_type(^^kwargs_impl), "Invalid keyword arguments");  
};

consteval std::meta::info make(std::string_view Names) {
  auto parser = NameParser(Names);
  if (!parser.parse()) {
    throw "parsing failed";
  }
  std::vector<std::meta::info> args;

  for (auto name : parser.names) {
    args.push_back(std::meta::reflect_constant_string(name));
  }

  return extract<std::meta::info (*)()>(substitute(^^inject, args))();
}
```

With this in place we can now make `kwargs(.x=1, .y="123")` legal again - all we have to do is
```cpp
#define kwargs(...) typename[:make(#__VA_ARGS__):]{{ __VA_ARGS__ }}
```

This expands `.x=1, .y="123"` to `typename[:make(".x=1, .y=123"):]{{ .x=1, .y=123 }}`.

## Strongly typed keyword argument containers
While this is better than nothing, it isn't exactly easy to use. `any_cast` does not spark joy. As it turns out it only requires minor mental gymnastics to capture the actual types of the arguments.

In the [last blog post on the topic](KwArgs/#helper-objects) we briefly talked about some pre-26 keyword argument approach via helper objects. The primary problem with this approach is that we actually have to create these helper objects somehow. This is either a syntactic burden or quite some boilerplate for the user to type.

As it turns out, with reflection and a little bit of preprocessor macro wizardry this doesn't have to be a syntactic burden. For all this to work, we need 3 parts:
- an injected aggregate with appropriate member names for detection
- a macro to expand the argument list to something that's valid C++
- an injected aggregate with appropriate member names and types

### Detection
For detection we can essentially reuse the code from above. We just have to swap out `std::any` for our detector type
```diff
+template <string_constant Name>
+struct Arg {
+  template <typename T>
+  TypedArg<T, Name> operator=(T&& value) const{
+    return {std::forward<T>(value)};
+  }
+};

template <string_constant... Names>
consteval std::meta::info inject() {
  struct kwargs_impl;
  consteval {
    std::vector<std::meta::info> args;
    for (auto name : {std::string_view(Names)...}){
-      args.push_back(data_member_spec(^^std::any, {.name = name}));
+      auto detector_type = substitute(^^Arg, {std::meta::reflect_constant_string(name)});
+      args.push_back(data_member_spec(detector_type, {.name = name}));
    }
    define_aggregate(^^kwargs_impl, args);
  };

  static_assert(is_complete_type(^^kwargs_impl), "Invalid keyword arguments");
  return ^^kwargs_t<kwargs_impl>;
}
```
We can now write a variable template that gives us an object of a type with correctly named data members.
```cpp
template <string_constant Expr>
constexpr auto detector = typename[:make_detector_type(Expr):]();
```

### Preprocessor macro
The next step is to expand the argument list to a bunch of assignments to members of `detector`. Essentially we want to do the following transformation
```cpp
kwargs(.x=1, .y="foo")
// detector.x=1, detector.y="foo"
```

For this we need recursive macros - thanks to C++20's `__VA_OPT__` this has become a lot easier ([Recursove macros with C++20 __VA_OPT__](https://www.scs.stanford.edu/~dm/blog/va-opt.html)). We just need to make one slight change - we must also comma-delimit the arguments:
```c++
#define PARENS ()
#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...) __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) \
  macro(a1) __VA_OPT__(, FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER
``` 

Now we can write our `kwargs` macro as follows:
```cpp
template <typename... Ts>
auto make_kwarg_container(Ts&&...);

#define IDENTITY(x) x
#define EXPAND_ONE(d) detector<d> . IDENTITY
#define kwargs(...) make_kwarg_container(FOR_EACH(EXPAND_ONE(#__VA_ARGS__), __VA_ARGS__))
```

### Injecting the final kwargs container
Now we only have to implement `make_kwarg_container`.