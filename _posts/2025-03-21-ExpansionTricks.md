---
title: C++26 Expansion Tricks
date: 2025-03-21T14:20:29+01:00
categories: [C++,]
tags: [C++, C++26, reflection, tricks, TMP, metaprogramming]
author: Che
---
[P1306](https://wg21.link/p1306) gives us compile time repetition of a statement _for each_ element of a range - what if we instead want the elements as a pack without introducing a new function scope?

In this blog post we'll look at the `expand` helper, expansion statements and how arbitrary ranges can be made decomposable via structured bindings to reduce the need for IILEs.


## Element-wise expansion
### The `expand` pattern
The reflection features introduced in [P2996](https://wg21.link/p2996) by themselves are sufficient to iterate over a compile time range. The paper introduces a helper [`expand`](https://wg21.link/p2996#implementation-status) for this purpose, here's a slightly modified version:

```c++
template <auto... Elts>
struct Replicator {
    template <typename F>
    constexpr decltype(auto) operator>>(F fnc) const {
        (fnc.template operator()<Elts>(), ...);
    }
};

template <auto... Elts>
constexpr inline Replicator<Elts...> replicator{};

template <std::ranges::range R>
consteval auto expand(R const& range) {
    std::vector<std::meta::info> args{};
    for (auto item : range) {
        args.push_back(std::meta::reflect_value(item));
    }
    return substitute(^^replicator, args);
}
```

This allows us to write the following code. Note that `Member` needs to be a constant to be usable in a splice.
```c++
template <typename T>
void print_members(T const& obj) {
    [:expand(nonstatic_data_members_of(^^T)):]
    >> [&]<auto Member>{
        std::println("{}: {}", identifier_of(Member), obj.[:Member:]);
    };
}

struct Test {
    int x;
    char p;
};

int main() { 
    print_members(Test{42, 'y'});
    // prints:
    // x: 42
    // p: y
}
```
[Run on Compiler Explorer](https://godbolt.org/z/3a5WE8TTP)

### Early return
This does not yet match loop semantics closely enough. `continue` can be expressed as `return;` but we cannot yet express `break` or return a value. 

First, let's introduce a way to stop iteration at any point. The short-circuiting property of `&&` and `||` is useful for this, we just need to let the lambda return a bool to indicate whether we should keep iterating.

```c++
template <auto... Elts>
struct Replicator {
    template <typename F>
    constexpr decltype(auto) operator>>(F fnc) const {
        (fnc.template operator()<Elts>() && ...);
    }
};
```

To reuse the example from before, we can now let it stop as soon as some arbitrary condition is met. In the following example we stop as soon as a member named `x` is reached - therefore the second member of `Test` will not be printed.
```c++
template <typename T>
void print_members(T const& obj) {
    [:expand(nonstatic_data_members_of(^^T)):]
    >> [&]<auto Member>{
        std::println("{}: {}", identifier_of(Member), obj.[:Member:]);

        // stop after we've reached a member named "p"
        return identifier_of(Member) == "p";
    };
}

struct Test {
    int x;
    char p;
};

int main() { 
    print_members(Test{42, 'y'});
    // prints:
    // x: 42
}
```
[Run on Compiler Explorer](https://godbolt.org/z/11bs1bed1)

### Returning values

Returning values is a little more difficult. To do this let's circle back a bit - instead of expressing our intent to continue iteration by returning a bool, we stop iterating once the first evaluation of the lambda returns something _other_ than a `void`.

So, let's first define a metafunction to retrieve the first non-void type in a pack of types.
```c++
template <typename...>
struct FirstNonVoid;

template <>
struct FirstNonVoid<> {
    using type = void;
};

template <typename T, typename... Ts>
struct FirstNonVoid<T, Ts...> {
    using type = std::conditional_t<
        std::is_void_v<T>, 
        typename FirstNonVoid<Ts...>::type, 
        T
    >;
};

template <typename... Ts>
using first_non_void = typename FirstNonVoid<Ts...>::type;
```

With this utility we can now tell if any specialization of `fnc`'s `operator()` returned something other than `void`. This unfortunately matters since `void` is not a regular type.

Let's first do the trivial case where no `F::operator()` specialization returns a value. This also means that no early return is going to happen - we can safely fold over `,` instead.
```c++
template <typename F>
constexpr auto operator>>(F fnc) const {
    using ret_t = first_non_void<decltype(fnc.template operator()<Elts>())...>;
    if constexpr (std::is_void_v<ret_t>){
        (fnc.template operator()<Elts>(), ...);
    } else {
      // ...
    }
}
```

Returning a value is a little more involved.

We already know that sooner or later a `F::operator()` specialization will return something other than a `void`, so we can prepare an object of this type to be returned later. To avoid a default-constructibility requirement on the return type, the return object can be wrapped in a union. Note however that this will imply that the return type must be copy-constructible. 

This issue can also be worked around, but the primary point here is to see just how much code is required to _roughly_ emulate expansion statements.

```c++
template <typename F>
constexpr auto operator>>(F fnc) const {
    using ret_t = first_non_void<decltype(fnc.template operator()<Elts>())...>;
    if constexpr (std::is_void_v<ret_t>){
        (fnc.template operator()<Elts>(), ...);
    } else {
        union {
            char dummy;
            ret_t obj;
        } ret {};

        if(!(invoke<Elts>(fnc, &ret.obj) && ...)){
            return ret.obj;
        } else {
            std::unreachable();
        }
    }
}
```
To keep using the short-circuiting property of `&&`, another helper `invoke` must be introduced. If the requested `F::operator()` specialization returned `void`, `invoke` shall return `true`. Otherwise it must return `false` to stop iteration and finally copy construct `ret.obj` from the return value.

```c++
template <auto E, typename F, typename R>
constexpr bool invoke(F fnc, R* result) {
    using return_type = decltype(fnc.template operator()<E>());

    if constexpr (std::is_void_v<return_type>){
        fnc.template operator()<E>();
        return true;
    } else {
        std::construct_at(result, fnc.template operator()<E>());
        return false;
    }
}
```

Finally we can write the following code.
```c++
template <typename T>
auto get_p(T const& obj) {
    return [:expand(nonstatic_data_members_of(^^T)):]
    >> [&]<auto Member>{
        if constexpr (identifier_of(Member) == "p") {
            return obj.[:Member:];
        }
  };
}

struct Test {
    int x;
    char p;
};

int main() { 
    std::print("{}", get_p(Test{42, 'y'}));
    // prints:
    // y
}
```
[Run on Compiler Explorer](https://godbolt.org/z/v4TjxaqbW)

However, note that a `if constexpr` statement must be used to guard the early return.

### Expansion statements
Unfortunately using `expand` implies having to use a lambda expression and therefore introduce a new function scope. While this isn't typically all that problematic, it can for instance cause issues with reflections of function parameters ([P3096](https://wg21.link/P3096)) since they are only splicable within their corresponding function body.


[P1306](https://wg21.link/P1306) `template for` expansion statements allow us to avoid the extra function scope.
```diff
-[:expand(some_range):] >> []<auto Elt>{
-    // ...
-};

+template for (constexpr auto Elt : define_static_array(some_range)) {
+    // ...
+}
```
The `define_static_array` from [P3491](https://wg21.link/P3491) is required because we do not yet have non-transient constexpr allocation. This is unfortunate, but oh well.

Amazingly expansion statements also support `break`, `continue` and early return.

## Transforming ranges to packs
### The `expand` pattern
So, we've established that expansion statements are pretty useful. What if we need the elements as a pack though? We might want to use the elements in a fold expression or expand them into an argument list.

To do this, let's introduce `operator->*` for `Replicator`. Unlike `operator>>` we want to expand all elements into the template argument list of a single call to `F::operator()`.
```c++
template <auto... Elts>
struct Replicator {
    template <typename F>
    constexpr decltype(auto) operator>>(F fnc) const {
        (fnc.template operator()<Elts>(), ...);
    }

    template <typename F>
    constexpr decltype(auto) operator->*(F fnc) const {
        return fnc.template operator()<Elts...>();
    }
};
```

We can now write
```c++
void print_args(auto... args){
    ((std::cout << args << ' '), ...) << '\n';
}

template <typename T>
void print_t(T obj) {
    [:expand(nonstatic_data_members_of(^^T)):]
    ->* [&]<auto... Members>{
        print_args(obj.[:Members:]...);
    };
}

struct Test {
    int x;
    char p;
};

int main() { 
    print_t(Test{42, 'y'});
    // prints
    // 42 y
}
```
[Run on Compiler Explorer](https://godbolt.org/z/66GfsYzTW)

>**Operator choice**
>
>Note that the choice of operator `->*` is mostly arbitrary. It just happens to be a rarely used operator that looks different enough to `>>` to not confuse the two.
>
>You might as well use regular member function templates instead of user-defined operator templates to achieve the following syntax:
>```cpp
>[:expand(some-range):].for_each([]<auto Elt>{
>    // ...
>});
>```
> and respectively
>```cpp
>[:expand(some-range):].into([]<auto... Elts>{
>    // ...
>});
>```
>
{: .prompt-info }

### Structured bindings
Unfortunately this suffers from the same problem as before - we are introducing another function scope.

To get around that, it's possible to use a structured binding to introduce a pack of the elements within the current scope. For this [P1061 Structured Bindings can introduce a Pack](https://wg21.link/P1061) and [P2686 constexpr structured bindings](https://wg21.link/P2686) are essential.

#### Promoting ranges

The simplest way to make an arbitrary range decomposable is to `promote` it to a constexpr C-style array. Unfortunately `define_static_array` from [P3491](https://wg21.link/P3491) gives us a constexpr `span`, not the actual array. The underlying machinery is extremely simple though:
```c++
template <typename T, T... Vs>
constexpr inline T fixed_array[sizeof...(Vs)]{Vs...};

template <std::ranges::input_range R>
consteval auto promote(R&& iterable) {
    std::vector args = {^^std::ranges::range_value_t<R>};
    for (auto element : iterable) {
        args.push_back(std::meta::reflect_value(element));
    }
    return substitute(^^fixed_array, args);
}
```

With `promote` in place, we can now write the following code.
```c++
void foo(int x, char c) {
    constexpr auto [...Param] = [:promote(parameters_of(^^foo)):];
    bar([:Param:]...);
}
```

>**Promoting strings**
>
>In a lot of existing C++(20 and upwards) code you see the following pattern to accept string literals as constant template arguments.
>```cpp
>template <std::size_t N>
>struct fixed_string {
>    constexpr explicit(false) fixed_string(const char (&str)[N]) noexcept {
>        std::ranges::copy(str, str+N, data);
>    }
>
>    char data[N]{};
>};
>
>template <fixed_string S>
>struct Test{};
>```
> P2996's `reflect_value` does not allow reflecting string literals directly (see [P2996](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r9.html#pnum_529)) and the way `define_static_string` is currently specified, it does not help with this either since the generated character array has already decayed to a pointer at that point. Consider the following code:
>
>```cpp
>using a = Test<"foo">; // ok
>
>// error: cannot reflect "foo"
>using b = [:substitute(^^Test, {reflect_value("foo")}):];
>
>// error: cannot deduce N
>using c = Test<define_static_string("foo")>; 
>
>// error: cannot deduce N
>using d = [:substitute(^^Test, {define_static_string("foo")}):]; 
>```
>[Run on Compiler Explorer](https://godbolt.org/z/PvPsMMjqz)
>
>Unfortunately `define_static_array` cannot be used for this either, since the generated array is wrapped in a constexpr span for extraction.
>
>However, with `promote` this is rather easy to solve.
>```cpp
>template <fixed_string S>
>struct Test{};
>
>using e = Test<[:promote("foo"):]>; // ok
>using f = [:substitute(^^Test, {promote("foo")}):]; // ok
>```
>[Run on Compiler Explorer](https://godbolt.org/z/qon8svf4P)
>
{: .prompt-tip }

#### Implementing the tuple protocol
Another way to make something decomposable via structured bindings is to implement the tuple protocol. If you've been paying attention you might have noticed the similarity between `promote` and `expand`. If `Replicator` were to implement the tuple protocol, `expand` would be sufficient.

This is very easy to do:
```c++
template <std::size_t Idx, auto... Elts>
constexpr auto get(Replicator<Elts...> const&){
    return Elts...[Idx];
}

template <auto... Elts>
struct std::tuple_size<Replicator<Elts...>>
    : std::integral_constant<std::size_t, sizeof...(Elts)> {};

template <std::size_t Idx, auto... Elts>
struct std::tuple_element<Idx, Replicator<Elts...>> {
    using type = decltype(Elts...[Idx]);
};
```

Now that `Replicator` is decomposable, we can finally get rid of the lambda expression.
```diff
-[:expand(some_range):] >> []<auto... Elts>{
-    // ...
-};
+constexpr auto [...Elts] = [:expand(some_range):];
```
[Run on Compiler Explorer](https://godbolt.org/z/o86sTYdxq). At the time of writing `constexpr` structured bindings ([P2686](https://wg21.link/p2686)) are not yet implemented in clang, which is why the example does not make use of it.


On a side note, this also means `expand` is usable in expansion statements and can be used to replace `define_static_array`.
```diff
-template for (constexpr auto Elt : define_static_array(some_range)) {
-    // ...
-}

+template for (constexpr auto Elt : [:expand(some_range):]) {
+    // ...
+}
```



## Sequences
While all of the aforementioned examples make use of reflection features, generating a pack of constants is not actually a new concept. 

By far the most common range to expand into a constant template parameter pack is a sequence of integers. In fact, this is common enough for C++14 to have introduced `std::integer_sequence`, `std::index_sequence` and `std::make_index_sequence` for this very purpose.

In a lot of code we see IILEs being used to retrieve the pack. The following pattern is rather popular:
```c++
[]<std::size_t... Idx>(std::index_sequence<Idx...>){
    // ...
}(std::make_index_sequence<Count>());
```

Since we already have the ability to expand arbitrary ranges through the `expand` helper, we can simply make use of C++20's `std::ranges::iota_view` to generate the sequence.
```c++
consteval auto sequence(unsigned maximum) {
    return expand(std::ranges::iota_view{0U, maximum});
}
```

This now allows us to introduce an integer sequence as a pack as follows.
```c++
constexpr auto [...Idx] = [:sequence(Count):];
```

>**Decomposing `integer_sequence`**
>
>Interestingly, reflection features are not actually needed for this. 
>We could instead implement the tuple protocol for `std::integer_sequence` in exactly the same way we've already done for `Replicator`.
>
>
>[P1789](https://wg21.link/P1789) suggests exactly that, which if accepted would allow us to write the following code.
>
>```c++
>constexpr auto [...Idx] = std::make_index_sequence<Count>();
>```
>[Run on Compiler Explorer](https://godbolt.org/z/Gr6GzorYM)
>
{: .prompt-tip }
