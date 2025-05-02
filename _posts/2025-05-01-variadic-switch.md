---
title: Variadic Switch
date: 2025-05-01T00:36:29+00:00
categories: [C++]
tags: [C++, Metaprogramming]
author: Che
bokeh: true
---
Way back in 2017 I found an interesting question on [Reddit](https://www.reddit.com/r/cpp/comments/6vyqra/variadic_switch_case/). Essentially the author asks why there is no way to expand a pack into a sequence of case labels followed by a statement. It was illustrated with the following imaginary syntax:
```c++
template <class Visitor, class Variant, std::size_t ... Is>
auto visit_impl(Visitor visitor, Variant && variant, std::index_sequence<Is...>) {
    auto i = variant.index();
    switch (i) { (case Is: return visitor(std::get<Is>(variant));)... }
}

template <class Visitor, class Variant>
auto visit(Visitor visitor, Variant&& variant) {
    return visit_impl(visitor, variant, 
                      std::make_index_sequence<std::variant_size_v<Variant>>{});
}
```

This poses an interesting challenge - how close can we get to generically generating something that optimizes to equivalently good assembly as a hand-written switch would?

And perhaps even more interestingly: Can C++26 features help with this?

## Jump Tables

Some programming languages provide a direct or indirect way of generating what's called a jump table or [branch table](https://en.wikipedia.org/wiki/Branch_table). While C++'s `switch` statements do not require compilers to turn them into jump tables, most compilers will do so if the number of cases is large enough and optimization is enabled.


>For simplicity let's not care about a generic visitor just yet, let's instead always call an undefined template function `h`:
>```cpp
>template <int>
>int h();
>```
>Since it is not defined compilers cannot possibly inline calls to it. This should come in handy once we look at the generated assembly (and optimization passes) to see if compilers were able to understand our code.
>
{: .prompt-info }

Consider a simple switch such as:
```cpp
int mersenne(unsigned index){
  switch (index) {
    case 0: return h<2>();
    case 1: return h<3>();
    case 2: return h<5>();
    case 3: return h<7>();
    case 4: return h<13>();
    default: return -1;
  }
}
```

With optimizations enabled, GCC turns this into the following x86 assembly:
```c++
mersenne(unsigned int):
        cmp     edi, 4
        ja      .L2
        mov     edi, edi
        jmp     [QWORD PTR .L4[0+rdi*8]]
.L4:
        .quad   .L8
        .quad   .L7
        .quad   .L6
        .quad   .L5
        .quad   .L3
.L5:
        jmp     int h<7>()
.L3:
        jmp     int h<13>()
.L8:
        jmp     int h<2>()
.L7:
        jmp     int h<3>()
.L6:
        jmp     int h<5>()
.L2:
        mov     eax, -1
        ret
```

While it won't help with solving the stated problem, we can approximately work backwards from the generated assembly by using GCC's [computed goto](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html) extension.
```cpp
template <auto> int h();

int mersenne(unsigned index){
  constexpr static void* jtable[] = 
  {                    //.L4:
    &&CASE_0,          //    .quad  .L8
    &&CASE_1,          //    .quad  .L7
    &&CASE_2,          //    .quad  .L6
    &&CASE_3,          //    .quad  .L5
    &&CASE_4,          //    .quad  .L3
  };

  if (index > 4)       // cmp  edi, 4
    goto DEFAULT;      // ja   .L2
  
  goto* jtable[index]; // jmp  [QWORD PTR .L4[0+rdi*8]]

CASE_0:                //.L8:
  return h<2>();       //    jmp  int h<2>()
CASE_1:                //.L7:
  return h<3>();       //    jmp  int h<3>()
CASE_2:                //.L6:
  return h<5>();       //    jmp  int h<5>()
CASE_3:                //.L5:
  return h<7>();       //    jmp  int h<7>()
CASE_4:                //.L3:
  return h<13>();      //    jmp  int h<13>()
DEFAULT:               //.L2:
  return -1;           //    mov  eax, -1
                       //    ret
}
```
[Run on Compiler Explorer](https://godbolt.org/z/n3sjnTenq)

This leads us to the first naive implementation strategy.

## Dispatch Tables
To turn this back into valid C++, we need to create an array of function pointers instead of label addresses. Conveniently, grabbing a pointer to every specialization of `h` that we wish to handle is trivial.

This gets us what's called a [dispatch table](https://en.wikipedia.org/wiki/Dispatch_table). Our example from before can be rewritten as follows.
```cpp
int mersenne(unsigned index){
  using F = int();
  constexpr static F* table[] = { 
    &h<2>,
    &h<3>,
    &h<5>,
    &h<7>,
    &h<13>,
    &h<17>
  };  

  if (index > 4) {
    // boundary check
    return -1;
  }
  return table[index]();
}
```
More generically, we can expand packs to generate the table.
```cpp
template <int... Is>
int visit(unsigned index) {
  using F = int();
  constexpr static F* table[] = {&h<Is>...};  

  if (index >= (sizeof table / sizeof* table)) {
    // boundary check
    return -1;
  }
  return table[index]();
}
```
[Run on Compiler Explorer](https://godbolt.org/z/MP1xYcsjh)

Note that while this still produces good assembly for this trivial example, this quickly degenerates and emits a call instead of a jump.

### Generic visit
However, this can already be used to implement single-variant `visit`. Since `visit` is a great usage example, we will look at a simplified implementation for each strategy.
>In the `visit` example implementations for all strategies, `get` is used for clarity. However, in the compiler explorer examples you will see `__unchecked_get` instead, which (for libc++) is essentially the same as `get` minus boundary checks.
>
>This is mostly done to make the emitted assembly easier to read.
{: .prompt-info }

```c++
template <typename F, typename V>
decltype(auto) visit(F&& visitor, V&& variant) {
  constexpr static auto max_index = std::variant_size_v<std::remove_cvref_t<V>>;
  constexpr static auto table = []<std::size_t... Idx>(std::index_sequence<Idx...>) {
    return std::array{+[](F&& visitor, V&& variant) {
      return std::invoke(std::forward<F>(visitor), 
                         __unchecked_get<Idx>(std::forward<V>(variant)));
    }...};
  }(std::make_index_sequence<max_index>());

  const auto index = variant.index();
  if (index >= table.size()) {
    // boundary check
    std::unreachable();
  }
  return table[index](std::forward<F>(visitor), std::forward<V>(variant));
}
```
[Run on Compiler Explorer](https://godbolt.org/z/77TWhc6ce)

## Macros
Surely the closest thing to a switch is.. a switch. Trusty old macros might not spark joy, but by cleverly combining them with C++ features we can try something else: Generating a lot of cases and then discarding all whose whose index exceeds the amount of cases we want to handle.

To do this we first need some way to emit cases with an increasing index. A set of macros like the following can help:
```c++
#define STAMP4(Offset, Fnc)  \
    Fnc(Offset)              \
    Fnc((Offset) + 1)        \
    Fnc((Offset) + 2)        \
    Fnc((Offset) + 3)

#define STAMP16(Offset, Fnc)  \
    STAMP4(Offset, Fnc)       \
    STAMP4((Offset) + 4, Fnc) \
    STAMP4((Offset) + 8, Fnc) \
    STAMP4((Offset) + 12, Fnc)

#define STAMP64(Offset, Fnc)    \
    STAMP16(Offset, Fnc)        \
    STAMP16((Offset) + 16, Fnc) \
    STAMP16((Offset) + 32, Fnc) \
    STAMP16((Offset) + 48, Fnc)

#define STAMP256(Offset, Fnc)    \
    STAMP64(Offset, Fnc)         \
    STAMP64((Offset) + 64, Fnc)  \
    STAMP64((Offset) + 128, Fnc) \
    STAMP64((Offset) + 192, Fnc)

#define STAMP(Count) STAMP##Count
```

With the `STAMP` macro in place, we can now define a macro to generate a `case`.
```c++
#define GENERATE_CASE(Idx)                                             \
  case (Idx):                                                          \
    if constexpr ((Idx) < max_index) {                                 \
      return std::invoke(std::forward<F>(visitor),                     \
                         get<Idx>(std::forward<Vs>(variant)));         \
    }                                                                  \
    std::unreachable();
```

To reduce the amount of branches that need to be discarded as much as possible, we can generate multiple template classes that handle variants up to a specific size limit. The appropriately sized one can later be selected and instantiated in `visit`.
```c++ 
#define GENERATE_STRATEGY(Idx, Stamper)                                 \
  template <>                                                           \
  struct VisitStrategy<Idx> {                                           \
    template <typename F, typename V>                                   \
    static constexpr decltype(auto) visit(F&& visitor, V&& variant) {   \
      switch (variant.index()) {                                        \
        Stamper(0, GENERATE_CASE);                                      \
        default:                                                        \
          std::unreachable();                                           \
      }                                                                 \
    }                                                                   \
  }

GENERATE_STRATEGY(1, STAMP(4));    // 4^1 potential states
GENERATE_STRATEGY(2, STAMP(16));   // 4^2 potential states
GENERATE_STRATEGY(3, STAMP(64));   // 4^3 potential states
GENERATE_STRATEGY(4, STAMP(256));  // 4^4 potential states
```

At this point we only need to select the appropriate specialization of `VisitStrategy`. Note that this can only handle variants with up to 256 alternatives - if you need to support more than that, you have to fall back to another strategy (ie. dispatch table).
```c++
template <typename F, typename V>
constexpr decltype(auto) visit(F&& visitor, V&& variant) {
  constexpr static auto max_index = std::variant_size_v<std::remove_cvref_t<V>>;
  using visit_helper              = VisitStrategy<max_index <= 4     ? 1
                                                  : max_index <= 16  ? 2
                                                  : max_index <= 64  ? 3
                                                                     : 4;

  return visit_helper::template visit(std::forward<F>(visitor),
                                      std::forward<Vs>(variant));
}
```

## Recursive switch
This is nice, however you have been promised C++ metaprogramming magic, not boring old macros. Let's explore another approach. 

```c++
template <int Offset, int Limit>
[[clang::always_inline]] inline int visit(unsigned index) {
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

While the `inline` keyword's more interesting use is to collapse multiple functions or variables of multiple translation units into one entity with one address ([dcl.inline/6](https://standards.pydong.org/c++/dcl.inline#6)) and therefore sidestep what would otherwise be ODR violations ([basic.def.odr](https://standards.pydong.org/c++/basic.def.odr)), some compilers actually honor `inline` as an ignorable optimization hint ([dcl.inline/2](https://standards.pydong.org/c++/dcl.inline#2)). For example Clang raises the inlining threshold slightly.

Unfortunately it is not possible to force inlining in a portable way. However, most compilers have special keywords or attributes for this very purpose.

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

Okay, so is this approach sufficient? Unfortunately not. Currently only clang is able to see through this and generate one big jump table.


## Fold Tricks
One commenter in the reddit thread I mentioned in the introduction of this blog post proposes the following rather arcane code:
```c++
template <int... Is>
int visit(unsigned index) {
  int retval;
  std::initializer_list<int>({(index == Is ? (retval = h<Is>()), 0 : 0)...});
  return retval;
}
```
Clang is able to see through this, unfortunately GCC and MSVC cannot ([Run on Compiler Explorer](https://godbolt.org/z/joh7e5b1b)). To explain why this is the case we need to dig into what GCC does under the hood. 

First let's rewrite the initializer list part to a fold over `,`. Clang can still see through this, GCC still cannot.

```c++
template <int... Is>
int visit(unsigned index) {
  int retval;
  ((index == Is ? (retval = h<Is>()),0 : 0), ...);
  return retval;
}
```

Assuming `Is` is the index sequence [0, 5] this expands to the following code ([cppinsights](https://cppinsights.io/s/4207098f)):
```cpp
template<>
int visit<0, 1, 2, 3, 4, 5>(unsigned index) {
  int retval;
  (index == 0 ? (retval = h<0>()) , 0 : 0),
    ((index == 1 ? (retval = h<1>()) , 0 : 0), 
      ((index == 2 ? (retval = h<2>()) , 0 : 0) , 
        ((index == 3 ? (retval = h<3>()) , 0 : 0) , 
          ((index == 4 ? (retval = h<4>()) , 0 : 0) , 
            (index == 5 ? (retval = h<5>()) , 0 : 0)))));
  return static_cast<int &&>(retval);
}
```

We can rewrite this to be a little easier to read:
```cpp
int visit(unsigned index) {
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
int visit(unsigned index) {
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

### Short-circuiting folds
We already had a pack of indices before, so let's use a fold expression again. What we're looking for is an operator capable of [short circuiting](https://en.wikipedia.org/wiki/Short-circuit_evaluation). C++ has two such operators - logical AND (`&&`) [expr.log.and/1](https://standards.pydong.org/c++/expr.log.and#1) and logical OR (`||`) [expr.log.or/1](https://standards.pydong.org/c++/expr.log.or#1). So let's fold over `||` instead.

```c++
template <int... Is>
int visit(unsigned index) {
  int value;
  (void)((index == Is ? value = h<Is>(), true : false) || ...);
  return value;
}
```

According to [cppinsights](https://cppinsights.io/s/827e0ae9) this desugars to:
```cpp
template<>
int visit<0, 1, 2, 3, 4, 5>(unsigned index)
{
  int value;
  static_cast<void>(
    (index == 0 ? (value = h<0>()) , true : false) || 
      ((index == 1 ? (value = h<1>()) , true : false) || 
        ((index == 2 ? (value = h<2>()) , true : false) || 
          ((index == 3 ? (value = h<3>()) , true : false) || 
            ((index == 4 ? (value = h<4>()) , true : false) || 
              (index == 5 ? (value = h<5>()) , true : false))))));
  return static_cast<int &&>(value);
}
```
As [Compiler Explorer](https://gcc.godbolt.org/z/jGsxKPcxz) can confirm for us: this will convince GCC to generate a jump table. Awesome.

### Generic visit
Unfortunately making this generic is a little more difficult. 

First of all let's make sure we do not introduce a default-constructibility requirement on the return type of the visitor. The idiomatic way to do so is by wrapping it in a `std::optional`. However, the fold expression already tells us whether a visitor was found (and therefore if the `optional` is engaged), we don't really need `optional` to begin with.

```c++
template <int... Is>
int visit(unsigned index) {
  union {
    int value;
  };
  if (((index == Is ? std::construct_at(&value, h<Is>()), true : false) || ...)) {
    return value;
  }
  std::unreachable(); // no visitor found
}
```

## Expansion statements
So, can we do any better in C++26? Are there any C++26 features that could help with this?

While it's not yet approved for inclusion in C++26 at the time of writing, one interesting candidate is expansion statements from [P1306](https://wg21.link/p1306). I've mentioned expansion statements in [C++26 Expansion Tricks](https://pydong.org/posts/ExpansionTricks/) before, but originally missed its usefulness for this particular problem.


In the spirit of [Duff's device](https://en.wikipedia.org/wiki/Duff%27s_device), the obvious approach is to interleave `switch` and expansion statements.
```c++
int visit(int x) {
    switch (x) {
        template for (constexpr auto I : {0, 1, 2, 3, 4}) {
            case I:
                return h<I>();
        }
    }
    return -1;
}
```
While this compiles with the clang-p2996 fork at the time of writing, this is unfortunately **not intended to work**. 

> 2. The contained statement of an _expansion-statement_ is a _control-flow-limited statement_ ([[stmt.label]](https://standards.pydong.org/c++/stmt.label#3)).

[stmt.expand]/2 from [P1306](https://wg21.link/p1306r3) (Thanks to Dan Katz for clarifying this!)

### Optimizer to the rescue

Bummer. However, as we've already proven with the fold tricks, clang (and gcc for that matter) are perfectly capable of optimizing multiple branches into a jump table for us. 

With the fold tricks from before we needed to prepare a to-be-returned object ahead of time. This is because we cannot `return` out of a fold expression directly. However, with expansion statements this is not an issue.

This makes it possible to simplify to the following code:
```c++
int visit(int x) {
    template for (constexpr auto I : {0, 1, 2, 3, 4}) {
        if (x == I) {
            return h<I>();
        }
    }
    return -1;
}
```
We can manually verify with [Compiler Explorer](tbd) that this optimizes as intended.

> At the time of writing "Opt Viewer" for clang-p2996 is broken due to a missing Compiler Explorer-specific patch.
>
> To illustrate how this gets optimized, we can instead look at what the expansion statement above would ultimately expand to:
>```c++
>int visit(int x) {
>    if (x == 0) { return h<0>(); }
>    if (x == 1) { return h<1>(); }
>    if (x == 2) { return h<2>(); }
>    if (x == 3) { return h<3>(); }
>    if (x == 4) { return h<4>(); }
>    if (x == 5) { return h<5>(); }
>    return -1;
>}
>```
>[View on Compiler Explorer](tbd)
>
{: .prompt-info }


### Generic visit
Implementing a generic single-variant `visit` is now rather trivial:

```c++
template <typename F, typename V>
decltype(auto) visit(F&& fnc, V&& variant) {
  static constexpr std::size_t size = std::variant_size_v<std::remove_cvref_t<V>>;
  auto const index = variant.index();
  if (index == std::variant_npos) {
    // TODO error handling
    std::unreachable();
  }

  template for (constexpr auto I : std::views::iota{0uz, size}) {
    if (index == I) {
      return std::invoke(std::forward<F>(fnc),
                         get<I>(std::forward<V>(variant)));
    }
  }
  std::unreachable();
}
```
[Run on Compiler Explorer](https://godbolt.org/z/GTTGrTb4M)

>If [P1789](https://wg21.link/p1789) gets accepted, we can write this in an even nicer way:
>```diff
>  template for (constexpr auto I :
>-                std::views::iota{0uz, size}) {
>+                std::make_index_sequence<size>()) {
>    if (index == I) {
>      return std::invoke(std::forward<F>(fnc),
>                         get<I>(std::forward<V>(variant)));
>    }
>  }
>```
>
{: .prompt-tip }