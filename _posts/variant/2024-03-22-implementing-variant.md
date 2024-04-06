---
title: Implementing Variant
date: 2024-03-17T23:36:29+00:00
categories: [C++]
tags: [C++, Metaprogramming]
author: Che
bokeh: true
---

In this blog post we will take a closer look at unions and how variant-like types can be implemented. While most of the example code will work with earlier C++ versions (albeit with slight modifications) most of the example code will assume at least C++23.

## Back to basics
You can think of a type as the set of all values the type can take on. A [union type](https://en.wikipedia.org/wiki/Union_type) is therefore a mathematical union of types, meaning it can take on all values of its member types. In other words:
> To put it bluntly, unions are a hack to subvert the C++ type system
>
> [N2248 - Toward a More Perfect Union](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2248.html)

In C++ we can introduce such types with the `union` keyword.

<br/>
As per [[class.union]/1](https://eel.is/c++draft/class.union#general-1) unions in C++ are classes. This means they can have constructors, destructors and non-virtual member functions - however they may not have base classes or be used as base classes ([[class.union]/4](https://eel.is/c++draft/class.union#general-4)). Since unions may only have one _active_ non-static data member at a time and an active member is one whose lifetime has begun and not yet ended ([[class.union]/2](https://eel.is/c++draft/class.union#general-2)), we cannot access inactive union members ([[basic.life]/7](https://eel.is/c++draft/basic.life#7) with one notable exception, more on that later).

> Note that accessing inactive union members is well defined in C.
> > If the member used to read the contents of a union object is not the same as the member last used to store a value in the object the appropriate part of the object representation of the value is reinterpreted as an object representation in the new type as described in 6.2.6 (a process sometimes called type punning). This might be a non-value representation.
> >
> > Footnote 107 of the [C standard](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf)
> 
> GCC also explicitly allows type-punning via inactive union member access
> > A member of a union object is accessed using a member of a different type (C90 6.3.2.3). The relevant bytes of the representation of the object are treated as an object of the type used for the access. See Type-punning. This may be a trap representation.
> > 
> > [4.9 Structures, Unions, Enumerations, and Bit-Fields](https://gcc.gnu.org/onlinedocs/gcc/Structures-unions-enumerations-and-bit-fields-implementation.html)
>
> While this documentation is part of the section about [**C** Implementation-Defined Behavior](https://gcc.gnu.org/onlinedocs/gcc/C-Implementation.html), the section about [**C++** Implementation-Defined Behavior](https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Implementation.html) explicitly mentions that 
> > Some choices are documented in the corresponding document for the C language.
>
> Hence it is safe to _assume_ that GCC allows type-punning via unions in C++ mode as well.
>
> MSVC however does not allow this ([Improper Access to a Union](https://learn.microsoft.com/en-us/cpp/c-language/improper-access-to-a-union)).
{: .prompt-info }


Anyway, what we can always do in C++ is switch the active member. [[class.union]/6](https://eel.is/c++draft/class.union#general-6) tells us one safe way to do so is
```c++
union U{
  M m;
  N n;
} u;

u.m.~M();
new (&u.n) N;
```
This allows us to destruct the previous member even if it has a non-trivial destructor and hence "deactivate" this member by ending its lifetime ([[basic.life]/1.4](https://eel.is/c++draft/basic.life#1.4)). Since C++20 we can use [`std::destroy_at`](https://en.cppreference.com/w/cpp/memory/destroy_at) instead.

Subsequently this also allows us to activate a new member using a placement new-expression ([[expr.new]/19](https://eel.is/c++draft/expr.new#19)). C++20 also provides a more pleasant way to write this: [`std::construct_at`](https://en.cppreference.com/w/cpp/memory/construct_at).

## Safely accessing unions
### Naive implementation

Since we cannot access a union's inactive members safely we need to track whichever member is active. For this purpose we need to wrap the union type in a record (a non-union class type) and use another non-static data member as tag. This is what's called a [tagged union](https://en.wikipedia.org/wiki/Tagged_union), sum type, discriminated union, disjoint union, choice type or simply variant.

```c++
struct TaggedUnion {
  union Union {
    int integer;
    char character;
    float floating;
  };

  enum Tag {
    integer,
    character,
    floating
  };

  Union data;
  Tag tag;
};
```
So, how do we keep track of which member is active?

First we want to provide constructors for all alternatives. In every one of them we want to set the tag to an appropriate value - later on we will reuse this tag to retrieve whichever member was active last.
```c++
explicit TaggedUnion(int integer_) 
  : data{.integer = integer_}
  , tag{integer} {}

explicit TaggedUnion(char character_) 
  : data{.character = character_}
  , tag{character} {}

explicit TaggedUnion(float float_) 
  : data{.floating = float_}
  , tag{floating} {}
```
Additionally we should define `operator=` overloads to actually switch the union's active type later on. That'd look something like
```cpp
  TaggedUnion& operator=(int integer_) {
    data.integer = integer_;
    tag = integer;
    return *this;
  }
```
Just like the constructors this needs to be repeated for every alternative.

Retrieving the active member is a little more involved. Essentially we need to find the associated tag for the requested member type and fail in some way if it isn't the active one. Let's add another member function `get` to our `TaggedUnion` type.

```cpp
template <typename T>
T get() {
  if (tag == integer) {
    if constexpr (std::same_as<T, int>) {
      return data.integer;
    }
  }
  else if (tag == character) {
    if constexpr (std::same_as<T, char>) {
      return data.character;
    }
  }
  else if (tag == floating) {
    if constexpr (std::same_as<T, float>) {
      return data.floating;
    }
  }

  // If this is reached something went wrong.
  // Either the tag had an invalid value or T did not match the associated type.
  std::unreachable();
}
```

At some point you will have to decide what to do if the requested member could not be retrieved. It is possible and sometimes desired to throw an exception here - this is what the standard library implementations typically do. For the scope of this blog post it does not really matter how we do error handling - for this reason let's use `std::unreachable` instead and let it cause UB if something went wrong. That's reasonably easy to debug and the generated assembly will be significantly easier to read.

### Accessor

As you probably have noticed by now: this is rather verbose and only gets worse if we add more alternative types.

Since we can always activate a union member by using a placement new expression and forming a pointer to an object whose lifetime has not yet begun or already ended is fine [[basic.life]/6](https://eel.is/c++draft/basic.life#6). Additionally all union members have the same address ([[class.union]/3 Note 2](http://eel.is/c++draft/class.union#general-note-2)) (TODO relevance? checking member ptr class type + is_function is sufficient).

TODO code example

## Detour: Friend injection
By now you are probably thinking "_I came here for weird shit and all I got was ~~this lousy shirt~~ an accessor object?_". Fear not, there are other more clever solutions to this. 

Since as we all know "_clever_" is just an euphemism for "_abusing obscure language features that probably shouldn't be a thing_" let's take a moment to talk about everyone's second favorite language feature ADL and friend injection.

> Should you? Probably not.
> > **2118. Stateful metaprogramming via friend injection**
> >
> > Defining a friend function in a template, then referencing that function later provides a means of capturing and retrieving metaprogramming state. This technique is arcane and should be made ill-formed.
> >
> > **Notes from the May, 2015 meeting:**
> >
> > CWG agreed that such techniques should be ill-formed, although the mechanism for prohibiting them is as yet undetermined.
> 
> [C++ Standard Core Language Active Issues](https://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#2118)
{: .prompt-warning }

(Pro Tip: The [C++ Standard Core Language Active Issues](https://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html) list is a great resource to learn about extremely obscure C++ "features")

## ADL based safe union access

## Visitation

One major obstacle is retrieving the appropriate union member and use it as argument to a function invocation. This pattern is called the [visitor pattern](https://en.wikipedia.org/wiki/Visitor_pattern), the equivalent standard library function for use with [`std::variant`](https://en.cppreference.com/w/cpp/utility/variant) is [`std::visit`](https://en.cppreference.com/w/cpp/utility/variant/visit).

For our implementation we will conveniently ignore the fact that `std::visit` is supposed to work with multiple variants. While implementing that is a very interesting problem in itself, let's focus on visiting _one_ variant instead.


## Detour: Generating jump tables

A lot of programming languages provide some way of generating what's called a jump table or [branch table](https://en.wikipedia.org/wiki/Branch_table). While [stmt.switch](https://eel.is/c++draft/stmt.switch) does not mandate compilers to turn switch statements into jump tables, most compilers will do if the number of cases is sufficient.

With GCC's [computed goto](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html) we can demonstrate what a switch statement will be desugared to. Consider a simple switch such as
```cpp
int mersenne(int index){
  switch (index) {
    case 0: return 2;
    case 1: return 3;
    case 2: return 5;
    case 3: return 7;
    case 4: return 13;
    case 5: return 17;
    default: return -1;
  }
}
```
this would roughly be equivalent to
```cpp
int mersenne(int index){
  constexpr static void* jtable[] = { 
    &&CASE_0, 
    &&CASE_1, 
    &&CASE_2, 
    &&CASE_3, 
    &&CASE_4, 
    &&CASE_5
  };  

  if (index >= (sizeof jtable / sizeof* jtable) || index < 0) {
    // boundary check
    goto DEFAULT;
  }
  goto* jtable[index];

CASE_0:  return 2;
CASE_1:  return 3;
CASE_2:  return 5;
CASE_3:  return 7;
CASE_4:  return 13;
CASE_5:  return 17;
DEFAULT: return -1;
}
```

However note that the element type of the jump table in the second example is wider.

<br/>

For simplicity let's not care about a generic visitor just yet, let's instead always call an undefined template function `h`:
```cpp
template <std::size_t>
int h();
```

Since it is not defined compilers cannot possibly inline calls to it. This should come in handy once we look at the generated assembly (and optimization passes) to see if compilers were able to understand our code. Also it greatly simplifies our `visit` since the return type is known for now.

### Recursive switch

Okay so let's start with a simple recursive approach.

```c++
template <std::size_t Offset, std::size_t Limit>
[[clang::always_inline]] inline int visit(std::size_t index) {
  switch (index - Offset) {
    case 0:
      if constexpr (Offset < Limit) {
        return h<Offset>();
      }
    default:
      if constexpr (Offset + 1 < Limit) {
        return visit<Offset + 1, Limit>(index);
      }
  }

  std::unreachable();
}
```

While the `inline` keyword's more interesting use is to collapse multiple functions or variables of multiple translation units into one entity with one address ([dcl.inline/6](https://eel.is/c++draft/dcl.inline#6)) and therefore sidestep what would otherwise be ODR violations ([basic.def.odr](https://eel.is/c++draft/basic.def.odr)), most compilers actually honor `inline` as an optimization hint ([dcl.inline/2](https://eel.is/c++draft/dcl.inline#2)). For every function call the cost of inlining will be compared against a threshold, `inline` functions have their threshold slightly increased.

Unfortunately it is not possible to force inlining in a standard way. However most compilers have special keywords or attributes for this very purpose.

| Compiler | Keyword | Attribute |
|---|---|---|
| GCC |  | `[[gnu::always_inline]]` |
| Clang | `__forceinline` | `[[clang::always_inline]]` or `[[gnu::always_inline]]` |
| MSVC | `__forceinline` | `[[msvc::forceinline]]` |

We can use [Compiler Explorer's clang Opt Pipeline Viewer](https://clang.godbolt.org/z/5MP61jTvr) feature to verify all calls to `visit` were inlined. After the `SimplifyCFG` optimization pass the chained comparisons are now combined into a switch. If we do not force inlining, switches may already be generated during the earlier `InlinerPass` pass and increase the calculated inlining cost beyond the threshold, resulting in multiple jump tables.
```llvm
; int visit<0ul, 3ul>(unsigned long)
define weak_odr dso_local noundef i32 @_Z5visitILm0ELm3EEim(i64 noundef %index) local_unnamed_addr #0 comdat {
entry:
  switch i64 %index, label %sw.epilog.i [
    i64 0, label %sw.bb
    i64 1, label %sw.bb.i
    i64 2, label %_Z5visitILm2ELm3EEim.exit
  ]
  ; ...
}
```
However if we look at the generated assembly, we will notice the lack of a jump table:
```cpp
int visit<0ul, 3ul>(unsigned long):
        cmp     rdi, 2
        je      int h<2ul>()
        cmp     rdi, 1
        jne     int h<0ul>()
        jmp     int h<1ul>()
```

Turns out clang decided it's cheaper to to not generate a jump table for so few branches with x86 as target assembler. This might happen, but at least for x86 we get a jump table with 4 or more cases. This can be verified by looking at the X86 DAG->DAG Instruction Selection pass.

As before after the SimplifyCFG pass it will have combined all inlined comparisons into one switch.
```llvm
; int visit<0ul, 4ul>(unsigned long)
define weak_odr dso_local noundef i32 @_Z5visitILm0ELm4EEim(i64 noundef %index) local_unnamed_addr #0 comdat {
  entry:
    switch i64 %index, label %sw.epilog.i [
      i64 0, label %sw.bb
      i64 1, label %sw.bb.i
      i64 2, label %sw.bb.i7
      i64 3, label %_Z5visitILm0ELm4EEim.exit
    ]
  ; ...
}
```
This time we can see this lovely note in the result of the X86 DAG->DAG Instruction Selection pass:
```cpp
# Machine code for function 
# int visit<0ul, 4ul>(unsigned long): IsSSA, TracksLiveness
Jump Tables:
%jump-table.0: %bb.1 %bb.2 %bb.3 %bb.5
```
Yay, a jump table! Let's verify the generated assembly
```cpp
int visit<0ul, 4ul>(unsigned long): 
        lea     rax, [rip + .LJTI0_0]
        movsxd  rcx, dword ptr [rax + 4*rdi]
        add     rcx, rax
        jmp     rcx
.LBB0_1:                           // label %sw.bb
        jmp     int handler<0ul>() // TAILCALL
.LBB0_3:                           // label %sw.bb.i7
        jmp     int handler<2ul>() // TAILCALL
.LBB0_4:                           // label %_Z5visitILm0ELm4EEim.exit
        jmp     int handler<3ul>() // TAILCALL
.LBB0_2:                           // label %sw.bb.i
        jmp     int handler<1ul>() // TAILCALL
.LJTI0_0:
        .long   .LBB0_1-.LJTI0_0
        .long   .LBB0_2-.LJTI0_0
        .long   .LBB0_3-.LJTI0_0
        .long   .LBB0_4-.LJTI0_0
```
`.LJTI0_0` there it is. Magnificent.

Okay, so is this approach sufficient? Unfortunately not. Currently only clang is able to look through this and generate one big jump table.

### Macroify it

Before we switch strategy just yet there is one more thing we could do: Generate bigger switch tables using macros. 

Let's define a macro for this purpose.
```c++
#define CASE(Idx)                        \
    case Offset + Idx:                   \
    if constexpr(Offset + Idx < Limit){  \
        return handler<Offset + Idx>();  \
    }

template <std::size_t Offset, std::size_t Limit>
FORCE_INLINE inline int visit(std::size_t index) {
    constexpr static auto step = 10;
    switch(index - Offset){
        CASE(0)
        CASE(1)
        CASE(2)
        CASE(3)
        CASE(4)
        CASE(5)
        CASE(6)
        CASE(7)
        CASE(8)
        CASE(9)
        default:
        if constexpr(Offset + step < Limit){
            return visit<Offset + step, Limit>(index);
        }
    }

    std::unreachable();
}
```
Unfortunately this will generate multiple jump tables for large enough variants. 
TODO MSVC STL reference, select strategy based on size, pattern match via specializations, generate huge tables via macro

### Dispatch table

This is nice, however you have been promised C++ metaprogramming magic, not boring macros. Let's explore another approach - a dispatch table. In essence a dispatch table is a degenerated jump table - instead of relative jumps we now have to do function calls.

TODO code example

### Variadic switch

In a perfect world syntax like

```c++
template <typename Visitor, typename Variant, std::size_t ... Is>
decltype(auto) visit(Visitor visitor, Variant&& variant, std::index_sequence<Is...>) {
    switch (variant.index()) { 
      (case Is: return visitor(get<Is>(variant));)... 
    }
}
```
would be valid. Unfortunately it is not.

One attempt some of you may know is forcing the compiler to create a initializer list of an arbitrary type for us - that'd look something like this:
```c++
template <std::size_t... Is>
int visit(std::size_t index) {
  int retval;
  std::initializer_list<int>({(index == Is ? (retval = h<Is>()), 0 : 0)...});
  return retval;
}
```

Clang is able to see through this, unfortunately GCC and MSVC cannot. To explain why this is the case we need to dig into what GCC does under the hood. 

First let's rewrite the initializer list part to a fold over `,`. Clang can still see through this, GCC still cannot.

```c++
template <std::size_t... Is>
int visit(std::size_t index) {
  int retval;
  ((index == Is ? (retval = h<Is>()),0 : 0), ...);
  return retval;
}
```
Assuming `Is` is the index sequence [0, 5] we can now see that this is expands to
```cpp
int visit(std::size_t index) {
  int retval;
  if (index == 0) { // case 0
    retval = h<0>();
  }
  if (index == 1) { // case 1
    retval = h<1>();
  }
  if (index == 2) { // case 2
    retval = h<2>();
  }
  if (index == 3) { // case 3
    retval = h<3>();
  }
  if (index == 4) { // case 4
    retval = h<4>();
  }
  if (index == 5) { // case 5
    retval = h<5>();
  }
  return retval;
}
```
Let's look at the first two "cases" just before GCC's `iftoswitch` optimization pass. For brevity the following snippet has some pass details removed and identifiers changed to match the code snippet. Additionally there's a little white lie hidden here - it does not actually write directly to `retval` just yet, for now it would use a temporary.

```c++
//   basic block 2, loop depth 0, maybe hot
//    pred:       ENTRY (FALLTHRU,EXECUTABLE)
  if (index == 0)
    goto <bb 3>;
  else
    goto <bb 4>;
//    succ:       3 (TRUE_VALUE,EXECUTABLE)
//                4 (FALSE_VALUE,EXECUTABLE)

//   basic block 3, loop depth 0, maybe hot
//    pred:       2 (TRUE_VALUE,EXECUTABLE)
  retval = h<0>();
//    succ:       4 (FALLTHRU,EXECUTABLE)

//   basic block 4, loop depth 0, maybe hot
//    pred:       2 (FALSE_VALUE,EXECUTABLE)
//                3 (FALLTHRU,EXECUTABLE)
  if (index == 1)
    goto <bb 5>;
  else
    goto <bb 6>;
//    succ:       5 (TRUE_VALUE,EXECUTABLE)
//                6 (FALSE_VALUE,EXECUTABLE)

//   basic block 5, loop depth 0, maybe hot
//    pred:       4 (TRUE_VALUE,EXECUTABLE)
  retval = h<1>();
```
So unfortunately GCC cannot assume that only one of the "case" branches will be taken at this point and decides against turning this into a switch. Let's help GCC out and turn this into

```cpp
int visit(std::size_t index) {
  int retval;
  if (index == 0) {      // case 0
    retval = h<0>();
  }
  else if (index == 1) { // case 1
    retval = h<1>();
  }
  else if (index == 2) { // case 2
    retval = h<2>();
  }
  else if (index == 3) { // case 3
    retval = h<3>();
  }
  else if (index == 4) { // case 4
    retval = h<4>();
  }
  else if (index == 5) { // case 5
    retval = h<5>();
  }
  return retval;
}
```
instead. We can [manually confirm](https://gcc.godbolt.org/z/G3YhP4Pq8) that this works. Great - so how do we do this generically?

We already had a pack of indices before, so let's use a fold expression again. What we're looking for is an operator capable of [short circuiting](https://en.wikipedia.org/wiki/Short-circuit_evaluation). C++ has two such operators - logical AND (`&&`) [expr.log.and/1](https://eel.is/c++draft/expr.log.and#1) and logical OR (`||`) [expr.log.or/1](https://eel.is/c++draft/expr.log.or#1). So let's fold over `||` instead.

```c++
template <std::size_t... Is>
int visit(std::size_t index) {
  int value;
  (void)((index == Is ? value = h<Is>(), true : false) || ...);
  return value;
}
```
As [Compiler Explorer](https://gcc.godbolt.org/z/jGsxKPcxz) can confirm for us: this will convince GCC to generate a jump table. Awesome.

## Generic visitation

As demonstrated in the previous chapter generating jump tables is possible. So far there has been quite a lot of assumptions - namely that we know the visitor and its return value ahead of time. To implement a generic visitor we will have to drop that assumption. Additionally let's introduce a new function template `get_n<std::size_t I, typename U>(U const&)` that gives us the `I`-th alternative of our union - we can worry about its implementation later.

Naively we could do something like
```c++
template <typename U, typename F, std::size_t... Is>
decltype(auto) visit(U const& storage, std::size_t index, F visitor) {
   using return_type = std::common_type_t< // common type of
    std::invoke_result_t<        // result type of
      F,                         // the visitor
      std::invoke_result_t<      // when called with an object of the result type of
        decltype(get_n<Is, U>),  // get_n<Is>
        U                        // called with a U object
      >
    >...                         // for every value in the Is pack
  >;
  
  return_type value;
  (void)((index == Is ? value = visitor(get_n<Is>(storage)), true : false) || ...);
  return value;
}
```

This however still won't work if `return_type` is not default-constructible. One way around this using `std::optional<return_type>` instead. If the optional contains a value after performing the fold over `||` we know the visitation was successful and can unwrap the optional.

`std::optional` is essentially a record type containing a union and a boolean tag that tells us whether it has a value or not. Since the fold expression already tells us whether the index we got at runtime was actually valid or not, we can instead simply use a union.

To get around the problem of `return_type` possibly not being default-constructible we can wrap it in a union of `return_type` and _some_ default-constructible type. The smallest fundamental type whose actual object representation is not implementation-defined ([[basic.fundamental]/10](https://eel.is/c++draft/basic.fundamental#10)) is `char` ([basic.fundamental/3](https://eel.is/c++draft/basic.fundamental#3), [basic.fundamental/7](https://eel.is/c++draft/basic.fundamental#7)). Additionally narrow character types have the weakest alignment requirements ([basic.align/6](https://eel.is/c++draft/basic.align#6)).
```c++
union {
  char dummy;
  return_type value;
} ret {};
```

As mentioned in the introduction of this blog post - placement new or `std::construct_at` can be used to switch the active member of a union. So let's use this in combination with the above union. We should now have something like
```c++
template <typename U, typename F, std::size_t... Is>
decltype(auto) visit(U const& storage, std::size_t index, F visitor) {
  using return_type = std::common_type_t<
    decltype(callback(get_n<Idx, union_type>(storage)))...>;
  union {
    char dummy;
    return_type value;
  } ret{};

  bool const success = (
    (index == Idx 
    ? std::construct_at(&ret.value, callback(get_n<Idx, union_type>(storage))), true 
    : false) 
  || ...);

  if (success) {
    return ret.value;
  }
  // alternatively we could throw an exception if this is reached
  std::unreachable();
}
```

## Generating unions

So far we have explored ways to retrieve the active member of an existing union type safely both at compile time and runtime, but we haven't yet done any work to generate the underlying union type. 

Essentially we want to be able to write `Variant<int, float, double, char>` and have it generate something like
```c++
union U {
  int    alternative_0;
  float  alternative_1;
  double alternative_2;
  char   alternative_3;
}
```

Unfortunately syntax such as
```c++
template <typename... Alternatives>
union U {
  Alternatives... alternatives;
};
```
is not valid. In C++26 there is one potential way to get around this by abusing the fact that [P2662 Pack Indexing](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2662r3.pdf) allows us to index lambda init-capture packs. For example something like
```c++
template <typename... Ts>
constexpr auto tuple(Ts... values) {
    return [...members = values]<std::size_t Idx>() {
        return members...[Idx];
    };
}
```
could be used as a rather inconvenient but potentially very low overhead tuple. However, since lambda closure types are never union types ([expr.prim.lambda.closure/1](https://eel.is/c++draft/expr.prim.lambda.closure#1)) this approach is not useable to generate the underlying union type. We might however be able to use this trick to get the active union member back more easily.

Admittedly this is _still_ a lot to type. What we really want is be able to write `Variant<int, float, double, char>` and have it generate something like 
```c++
union U {
  int    alternative_0;
  float  alternative_1;
  double alternative_2;
  char   alternative_3;
}
```
with a bunch of generic helpers for us under the hood.

Unfortunately syntax such as
```c++
template <typename... Alternatives>
union U {
  Alternatives... alternatives;
};
```
is not valid. In C++26 there is one potential way to get around this by abusing the fact that [P2662 Pack Indexing](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2662r3.pdf) allows us to index lambda init-capture packs. For example something like
```c++
template <typename... Ts>
constexpr auto tuple(Ts... values) {
    return [...members = values]<std::size_t Idx>() {
        return members...[Idx];
    };
}
```
could be used as a rather inconvenient but potentially very low overhead tuple. However, since lambda closure types are never union types ([expr.prim.lambda.closure/1](https://eel.is/c++draft/expr.prim.lambda.closure#1)) this approach is not useable for our variant hackery. We might however be able to use this trick to get the active union member back more easily. 

The accessor introduced in the first few chapters of this post can be rewritten as
```c++
//TODO
template <typename... Ts>
constexpr auto generate_accessor(Ts... values) {
    return [...members = values]<std::size_t Idx>() {
        return members...[Idx];
    };
}

union U {
  int    alternative_0;
  float  alternative_1;
  double alternative_2;
  char   alternative_3;
  constexpr static auto accessor = generate_accessor(
      &U::alternative_0,
      &U::alternative_1,
      &U::alternative_2,
      &U::alternative_3);
}
```

### Recursive unions

Recursive types to the rescue! Unfortunately unions cannot have base classes nor be used as base classes ([[class.union]/4](https://eel.is/c++draft/class.union#general-4)), so we need another approach.

All union members are allocated as if they were the sole member of the union, meaning they all have the same address ([[class.union]/3](https://eel.is/c++draft/class.union#general-3)). Since a union member itself can be a union, this guarantee extends to union union members. Therefore 

```c++
union A {
  int foo;
  char bar;
  float baz;
};
```
has the same guarantees as
```c++
union A {
  float baz;
  union {
    char bar;
    union {
      float baz;
    } tail;
  } tail;
};
```
This pattern can now easily be generalized by using template specializations
```c++
template <typename... Ts>
union RecursiveUnion;

template <typename T>
union RecursiveUnion<T> {
  T value;
};

template <typename T, typename... Ts>
union RecursiveUnion<T, Ts...>{
  T value;
  RecursiveUnion<Ts...> tail;
};
```

Unfortunately initializing unions requires the initializer list to have at most one element ([[dcl.init.aggr]/20](https://eel.is/c++draft/dcl.init.aggr#20)), hence we must name which member we want to initialize _unless_ we want to initialize the first one ([[dcl.init.aggr]/3.2](https://eel.is/c++draft/dcl.init.aggr#3.2)). Since designators must name a _direct_ member of the class under construction ([[dcl.init.aggr]/3.1](https://eel.is/c++draft/dcl.init.aggr#3.1)) this gets rather awkward quickly, so let's instead define a recursive constructor.

Let's make an assumption: Alternatives of a variant type must be unique. A variant cannot have the same alternative type more than once. This means we can now unambiguously determine the index of a specific type within all alternative types of the variant, which will also come in handy for the tag.

Finding the index of a type within a pack can be done by counting up _until_ the type at the current position within the pack matches the searched-for type. To avoid recursion we can fold over the pack. Doing something _until_ a predicate matches in the context of fold expressions always means we want to use a short-circuiting operator, so let's do just that.
```c++
template <typename T, typename... Ts>
consteval std::size_t get_type_index(){
    std::size_t index = 0;
    (void)((!std::same_as<T, Ts> ? ++index, false : true) || ...);
    return index;
}

template <typename T, typename... Ts>
constexpr inline std::size_t type_index = get_type_index<T, Ts...>();
```

We can now add some constructors to our recursive union.

```c++
template <std::size_t N>
using size_constant = std::integral_constant<std::size_t, N>;

template <typename... Ts>
union RecursiveUnion;

template <typename T>
union RecursiveUnion<T> {
  T value;
  
  constexpr RecursiveUnion(size_constant<0>, T&& obj) 
    : value{std::forward<T>(obj)} {
    // cannot delegate anymore, construct `value` member
  }
};

template <typename T, typename... Ts>
union RecursiveUnion<T, Ts...> {
  T value;
  RecursiveUnion<Ts...> tail;

  constexpr RecursiveUnion(size_constant<0>, T&& obj)
    : value{std::forward<T>(obj)} {
    // current index is 0, construct `value` member
  }

  template <std::size_t N, typename U>
  constexpr RecursiveUnion(size_constant<N>, U&& obj)
    : tail{size_constant<N-1>{}, std::forward<U>(obj)} {
    // decrement N by one and delegate to the next constructor
  }
};
```

To get everything started, the variant type's constructor must pass the constructor of its `RecursiveUnion` member the desired type index.
```c++
template <typename... Ts>
struct Variant {
  RecursiveUnion<Ts...> value;
  std::size_t           index;

  template<typename T>
    requires ( std::same_as<T, Ts> || ... )
  explicit Variant(T&& obj)
    : index{type_index<T, Ts...>}
    , value{size_constant<type_index<T, Ts...>>{}, std::forward<T>(t)}
  { }
};
```

The same recursive approach can be reused to finally implement `get_n`.
```c++
template <std::size_t Idx, typename U>
constexpr decltype(auto) get_n(U&& union_) {
  if constexpr (Idx == 0) {
    return std::forward<U>(union_).value;
  }
  else {
    return get_n<Idx - 1>(std::forward<U>(union_).tail);
  }
}
```

### Reducing recursion depth
TODO explain why, use flamegraphs

To reduce the recursion depth of `get_n` it is advisable to handle more than one alternative at a time. This is what most standard libraries do (link libstdc++ and MSVC STL). Let's update `get_n` with this optimization.

```c++
template <std::size_t Idx, typename U>
constexpr decltype(auto) get_n(U&& union_) {
  if constexpr (Idx == 0) {
    return std::forward<U>(union_).value;
  }
  else if constexpr (Idx == 1) {
    return std::forward<U>(union_).tail.value;
  }
  else if constexpr (Idx == 2) {
    return std::forward<U>(union_).tail.tail.value;
  }
  else {
    return get_n<Idx - 3>(std::forward<U>(union_).tail.tail.tail);
  }
}
```

To go one step further we can also reduce recursion depth of the delegating constructors. To do so we need to add specializations of `RecursiveUnion`. 

Since this makes implementing `get_n` a lot nastier (we can no longer assume to have a `value` member!) let's implement `get_n` as non-static member function instead. This implies it needs to be implemented as non-static member function for all other specializations of `RecursiveUnion` as well.

```cpp
template <typename T1, typename T2, typename T3, typename T4, typename... Ts>
union RecursiveUnion<T1, T2, T3, T4, Ts...> {
  T1 alternative_1;
  T2 alternative_2;
  T3 alternative_3;
  T4 alternative_4;
  RecursiveUnion<Ts...> tail;

  constexpr RecursiveUnion(size_constant<0>, T&& obj)
    : value{std::forward<T>(obj)} {}
  
  constexpr RecursiveUnion(size_constant<1>, T&& obj)
    : value{std::forward<T>(obj)} {}
  
  constexpr RecursiveUnion(size_constant<2>, T&& obj)
    : value{std::forward<T>(obj)} {}
  
  constexpr RecursiveUnion(size_constant<3>, T&& obj)
    : value{std::forward<T>(obj)} {}

  template <std::size_t N, typename U>
  constexpr RecursiveUnion(size_constant<N>, U&& obj)
    : tail{size_constant<N-4>{}, std::forward<U>(obj)} {}

  template <std::size_t N>
  constexpr decltype(auto) get_n() {
    if constexpr (N == 0) {
      return alternative_1;
    } else if constexpr (N == 1) {
      return alternative_2;
    } else if constexpr (N == 2) {
      return alternative_3;
    } else if constexpr (N == 3) {
      return alternative_4;
    } else {
      return tail.get_n<N-4>();
    }
  }
};
```

With `get_n` being a non-static member function we unfortunately must now duplicate it with all possible combinations of [cv-qualifiers](https://eel.is/c++draft/dcl.decl.general#nt:cv-qualifier) to handle all possible `this` pointer types ([[class.mfct.non.static]/3](https://eel.is/c++draft/class.mfct.non.static#3)). Fortunately C++23 gave us explicit object member functions ([[dcl.fct]/6](https://eel.is/c++draft/dcl.fct#6)), so we can simply rewrite the `get_n` member function as
```cpp
template <std::size_t N, typename Self>
constexpr decltype(auto) get_n(this Self&& self) {
  if constexpr (N == 0) {
    return std::forward<Self>(self).alternative_1;
  } else if constexpr (N == 1) {
    return std::forward<Self>(self).alternative_2;
  } else if constexpr (N == 2) {
    return std::forward<Self>(self).alternative_3;
  } else if constexpr (N == 3) {
    return std::forward<Self>(self).alternative_4;
  } else {
    return std::forward<Self>(self).tail.template get_n<N - 4>();
  }
}
```

The implementations of `get_n` for the other specializations is analogous to this.

### Union trees

Another way of reducing recursion depth is by changing the structure of the underlying union. While the recursion depth of the simple `RecursiveUnion` type grows lineary, the maximum depth of a balanced binary tree only grows logarithmically.


The real challenge is generating such a tree. Unfortunately actually balancing it is rather expensive, so let's instead recursively merge adjacent leafs or subtrees until there is only one node left over. This should gives us a somewhat balanced tree, benchmarking will tell us if that's good enough.

TODO code example

{% include_relative depth_plot.html %}


Since union trees can actually be more expensive for small unions, it is advisable to only switch to this strategy for large unions. The final implementation can be found at [TODO](https://github.com/tsche/variant).

TODO push final implementation to github

## Inverted variant

Used to minimize padding. TODO

```c++
union U{
    struct{
        unsigned char tag;
        char buf[7];
    }a;
    struct{
        unsigned char tag;
        float f;
    }b;
};
struct S{
    unsigned char tag;
    union{
        char buf[7];
        float f;
    };
};
static_assert(sizeof(U)==8);
static_assert(sizeof(S)==12);
```
https://github.com/Halalaluyafail3 
Usage example: https://github.com/Halalaluyafail3/LuaPreprocessor/blob/main/src/LuaPreprocessor.c#L154-L179 used to avoid as much padding as possible.
