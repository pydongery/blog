// #include <iostream>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

// template <typename T> auto strtype() {
//   auto name = std::string_view{__PRETTY_FUNCTION__ + 20};
//   name.remove_suffix(1);
//   return name;
// }


template <typename T, typename... Ts> consteval std::size_t get_type_index() {
  std::size_t index = 0;
  (void)((!std::same_as<T, Ts> ? ++index, false : true) || ...);
  return index;
}

template <typename T, typename... Ts>
constexpr inline std::size_t type_index = get_type_index<T, Ts...>();

template <typename... Ts> struct TypeList {
  template <typename T>
  constexpr static bool contains = (std::same_as<T, Ts> || ...);
};
template <typename T, typename V> struct ExtendImpl;

template <typename... Ts, typename... Us>
struct ExtendImpl<TypeList<Ts...>, TypeList<Us...>> {
  using type = TypeList<Ts..., Us...>;
};

template <std::size_t N>
using size_constant = std::integral_constant<std::size_t, N>;

template <typename... Ts> union TreeUnion;

template <typename T> struct TreeTraits {
  static constexpr bool is_leaf = true;
  static constexpr std::size_t size = 1;
  // using type = T;
  // using types = TypeList<T>;

  // template <typename U> static constexpr bool contains = std::same_as<T, U>;

  // template <typename V>
  // static constexpr std::size_t index =
  //     !std::same_as<V, T>; // one past end if not found
};

template <typename T> struct TreeTraits<TreeUnion<T>> {
  static constexpr bool is_leaf = true;
  static constexpr std::size_t size = 1;
  // using type = T;
  // using types = TypeList<T>;

  // template <typename U> static constexpr bool contains = std::same_as<T, U>;

  // template <typename V>
  // static constexpr std::size_t index = !std::same_as<V, T>;
};

template <typename... Ts> struct TreeTraits<TreeUnion<Ts...>> {
  static constexpr bool is_leaf = false;
  static constexpr std::size_t size = (0 + ... + TreeTraits<Ts>::size);
  // using first_type = T;
  // using second_type = U;

  // using types = ExtendImpl<typename TreeTraits<T>::types, typename TreeTraits<U>::types>::type;

  // template <typename V>
  // static constexpr bool contains = types::template contains<V>;

  // template <typename V>
  // static constexpr bool contains =  TreeTraits<T>::template contains<V> ||
                                    // TreeTraits<U>::template contains<V>;

  // template <typename V>
  // static constexpr std::size_t index =
  //     TreeTraits<T>::template contains<V>
  //         ? TreeTraits<T>::template index<V>
  //         : TreeTraits<T>::size + TreeTraits<U>::template index<V>;
};

// template <typename T1, typename T2, typename T3, typename T4>
// struct TreeTraits<TreeUnion<T1, T2, T3, T4>> {
//   static constexpr bool is_leaf = false;
//   static constexpr std::size_t size =
//       TreeTraits<T1>::size + TreeTraits<T2>::size + TreeTraits<T3>::size +
//       TreeTraits<T4>::size;
//   using first_type = T1;
//   using second_type = T2;
//   using third_type = T3;
//   using fourth_type = T4;

//   using types = ExtendImpl<
//       typename TreeTraits<T1>::types,
//       typename ExtendImpl<typename TreeTraits<T2>::types,
//                           typename ExtendImpl<typename TreeTraits<T3>::types,
//                                               typename TreeTraits<T4>::types>::
//                               type>::type>::type;


//   // template <typename V>
//   // static constexpr bool contains = types::template contains<V>;

//   template <typename V>
//   static constexpr bool contains = TreeTraits<T1>::template contains<V> ||
//                                    TreeTraits<T2>::template contains<V> ||
//                                    TreeTraits<T3>::template contains<V> ||
//                                    TreeTraits<T4>::template contains<V>;

//   template <typename V>
//   static constexpr std::size_t index =
//       TreeTraits<T1>::template contains<V>
//           ? TreeTraits<T1>::template index<V>
//           : TreeTraits<T1>::size +
//                 (TreeTraits<T2>::template contains<V>
//                      ? TreeTraits<T2>::template index<V>
//                      : TreeTraits<T2>::size +
//                            (TreeTraits<T3>::template contains<V>
//                                 ? TreeTraits<T3>::template index<V>
//                                 : TreeTraits<T3>::size +
//                                       (TreeTraits<T4>::template contains<V>
//                                            ? TreeTraits<T4>::template index<V>
//                                            : TreeTraits<T4>::size)));
// };

template <typename T>
concept is_leaf = TreeTraits<T>::is_leaf;

template <typename V, typename T>

#ifdef LIST_CONTAINS
concept contains = TreeTraits<T>::types::template contains<V>;
#else
concept contains = TreeTraits<T>::template contains<V>;
#endif
// if left::size == right::size push node

template <typename V, typename Alt>
constexpr V do_get(Alt const &alternative) {
  if constexpr (TreeTraits<Alt>::is_leaf) {
    return alternative;
  } else {
    return alternative.template get<V>();
  }
}

template <std::size_t Idx, typename Alt>
constexpr auto do_get_n(Alt const &alternative) {
  if constexpr (TreeTraits<Alt>::is_leaf) {
    return alternative;
  } else {
    return alternative.template get_n<Idx>();
  }
}


template <typename T, typename U> union TreeUnion<T, U> {
  T left;
  U right;

  TreeUnion() = default;

  // template <contains<T> V> constexpr TreeUnion(V value) : left{value} {}
  // template <contains<U> V> constexpr TreeUnion(V value) : right{value} {}


  template <typename V>
  requires (is_leaf<T>)
  constexpr TreeUnion(size_constant<0>, V value) : left{value} {}

    template <std::size_t Idx, typename V>
  requires (!is_leaf<T> && Idx < TreeTraits<T>::size)
  constexpr TreeUnion(size_constant<Idx>, V value) : left{size_constant<Idx>{}, value} {}

  template <std::size_t Idx, typename V>
  requires (is_leaf<U> && Idx == TreeTraits<T>::size)
  constexpr TreeUnion(size_constant<Idx>, V value) : right{value} {}

  template <std::size_t Idx, typename V>
  requires (!is_leaf<U> && Idx >= TreeTraits<T>::size)
  constexpr TreeUnion(size_constant<Idx>, V value) : right{size_constant<Idx - TreeTraits<T>::size>{}, value} {}



  // template <typename V> constexpr V get() const {
  //   if constexpr (contains<V, T>) {
  //     return do_get<V>(left);
  //   } else if constexpr (contains<V, U>) {
  //     return do_get<V>(right);
  //   }
  //   std::unreachable();
  // }

  template <std::size_t Idx>
  constexpr auto get_n() const {
    if constexpr (Idx < TreeTraits<T>::size){
      if constexpr (is_leaf<T>) {
        return left;
      } else {
        return left.template get_n<Idx>();
      }
    } else if constexpr (Idx - TreeTraits<T>::size < TreeTraits<U>::size) {
      constexpr auto idx = Idx - TreeTraits<T>::size;
      if constexpr (is_leaf<U>) {
        return right;
      } else {
        return right.template get_n<idx>();
      }
    }
    std::unreachable();
  }
};

// template <typename T1, typename T2, typename T3, typename T4>
// union TreeUnion<T1, T2, T3, T4> {
//   T1 alt1;
//   T2 alt2;
//   T3 alt3;
//   T4 alt4;

//   TreeUnion() = default;

//   template <contains<T1> V> constexpr TreeUnion(V value) : alt1{value} {}
//   template <contains<T2> V> constexpr TreeUnion(V value) : alt2{value} {}
//   template <contains<T3> V> constexpr TreeUnion(V value) : alt3{value} {}
//   template <contains<T4> V> constexpr TreeUnion(V value) : alt4{value} {}

//   template <typename V> constexpr V get() const {
//     if constexpr (contains<V, T1>) {
//       return do_get<V>(alt1);
//     } else if constexpr (contains<V, T2>) {
//       return do_get<V>(alt2);
//     } else if constexpr (contains<V, T3>) {
//       return do_get<V>(alt3);
//     } else if constexpr (contains<V, T4>) {
//       return do_get<V>(alt4);
//     }
//     std::unreachable();
//   }
// };

template <typename T> union TreeUnion<T> {
  using type = T;

  T value;

  template <std::same_as<T> V> constexpr TreeUnion(V value) : value{value} {
    // std::cout << "leaf -> " << strtype<V>() << '\n';
  }

  template <typename V> using append = TreeUnion<T, V>;

  template <std::same_as<T> V> constexpr V get() { return value; }
  template <std::size_t Idx> constexpr auto get_n() {
    static_assert(Idx == 0);
    return value;
  }
};

template <std::size_t> struct Constant {};


template <typename, typename> struct Tree;

template <typename T, typename U>
struct Tree<TypeList<>, TypeList<TreeUnion<T, U>>>{
  // output accumulator only contains one node, done
  using type = TreeUnion<T, U>;
};

template <typename... Out>
struct Tree<TypeList<>, TypeList<Out...>> {
  // more than one node in output accumulator
  // => not yet done, recurse!
  using type = Tree<TypeList<Out...>, TypeList<>>::type;
};

template <typename T, typename... Out>
struct Tree<TypeList<T>, TypeList<Out...>>{
  // odd number of elements but it wasn't a node
  // => not yet done, recurse!
  using type = Tree<TypeList<Out..., T>, TypeList<>>::type;
};

template <typename T1, typename T2, typename... In, typename... Out>
struct Tree<TypeList<T1, T2, In...>, TypeList<Out...>>{
  // consume two types from the input list, append them wrapped in a node to the output list
  // => not yet done, recurse
  using type = Tree<TypeList<In...>, TypeList<Out..., TreeUnion<T1, T2>>>::type;
};

// template <typename... Ts>
// struct Tree<TypeList<>, TypeList<TreeUnion<Ts...>>> {
//   using type = TreeUnion<Ts...>;
// };

// template <typename... Prev> struct Tree<TypeList<Prev...>, TypeList<>> {
//   using type = Tree<TypeList<>, TypeList<Prev...>>::type;
// };

// template <typename... Prev, typename T>
// struct Tree<TypeList<Prev...>, TypeList<T>> {
//   using type = Tree<TypeList<>, TypeList<Prev..., T>>::type;
// };

// template <typename... Prev, typename T1, typename T2, typename... Ts>
// struct Tree<TypeList<Prev...>, TypeList<T1, T2, Ts...>> {
//   using type =
//       Tree<TypeList<Prev..., TreeUnion<T1, T2>>, TypeList<Ts...>>::type;
// };

// template <typename T1, typename T2, typename T3, typename T4,
//           typename... In, typename... Out>
// struct Tree<TypeList<T1, T2, T3, T4, In...>, TypeList<Out...>> {
//   using type =
//       Tree<TypeList<In...>, TypeList<Out..., TreeUnion<TreeUnion<T1, T2>, TreeUnion<T3, T4>>>>::type;
// };


// template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8,
//           typename... In, typename... Out>
// struct Tree<TypeList<T1, T2, T3, T4, T5, T6, T7, T8, In...>, TypeList<Out...>> {
//   using type =
//     Tree<
//       TypeList<In...>,
//       TypeList<Out...,
//         TreeUnion<
//           TreeUnion<
//             TreeUnion<T1, T2>,
//             TreeUnion<T3, T4>
//           >,
//           TreeUnion<
//             TreeUnion<T5, T6>,
//             TreeUnion<T7, T8>
//           >
//         >
//       >
//     >::type;
// };

template struct TypeList<
    Constant<0>, Constant<1>, Constant<2>, Constant<3>, Constant<4>,
    Constant<5>, Constant<6>, Constant<7>, Constant<8>, Constant<9>,
    Constant<10>, Constant<11>, Constant<12>, Constant<13>, Constant<14>,
    Constant<15>, Constant<16>, Constant<17>, Constant<18>, Constant<19>,
    Constant<20>, Constant<21>, Constant<22>, Constant<23>, Constant<24>,
    Constant<25>, Constant<26>, Constant<27>, Constant<28>, Constant<29>,
    Constant<30>, Constant<31>, Constant<32>, Constant<33>, Constant<34>,
    Constant<35>, Constant<36>, Constant<37>, Constant<38>, Constant<39>,
    Constant<40>, Constant<41>, Constant<42>, Constant<43>, Constant<44>,
    Constant<45>, Constant<46>, Constant<47>, Constant<48>, Constant<49>,
    Constant<50>, Constant<51>, Constant<52>, Constant<53>, Constant<54>,
    Constant<55>, Constant<56>, Constant<57>, Constant<58>, Constant<59>,
    Constant<60>, Constant<61>, Constant<62>, Constant<63>, Constant<64>,
    Constant<65>, Constant<66>, Constant<67>, Constant<68>, Constant<69>,
    Constant<70>, Constant<71>, Constant<72>, Constant<73>, Constant<74>,
    Constant<75>, Constant<76>, Constant<77>, Constant<78>, Constant<79>,
    Constant<80>, Constant<81>, Constant<82>, Constant<83>, Constant<84>,
    Constant<85>, Constant<86>, Constant<87>, Constant<88>, Constant<89>,
    Constant<90>, Constant<91>, Constant<92>, Constant<93>, Constant<94>,
    Constant<95>, Constant<96>, Constant<97>, Constant<98>, Constant<99>,
    Constant<100>, Constant<101>, Constant<102>, Constant<103>, Constant<104>,
    Constant<105>, Constant<106>, Constant<107>, Constant<108>, Constant<109>,
    Constant<110>, Constant<111>, Constant<112>, Constant<113>, Constant<114>,
    Constant<115>, Constant<116>, Constant<117>, Constant<118>, Constant<119>,
    Constant<120>, Constant<121>, Constant<122>, Constant<123>, Constant<124>,
    Constant<125>, Constant<126>, Constant<127>, Constant<128>, Constant<129>,
    Constant<130>, Constant<131>, Constant<132>, Constant<133>, Constant<134>,
    Constant<135>, Constant<136>, Constant<137>, Constant<138>, Constant<139>,
    Constant<140>, Constant<141>, Constant<142>, Constant<143>, Constant<144>,
    Constant<145>, Constant<146>, Constant<147>, Constant<148>, Constant<149>,
    Constant<150>, Constant<151>, Constant<152>, Constant<153>, Constant<154>,
    Constant<155>, Constant<156>, Constant<157>, Constant<158>, Constant<159>,
    Constant<160>, Constant<161>, Constant<162>, Constant<163>, Constant<164>,
    Constant<165>, Constant<166>, Constant<167>, Constant<168>, Constant<169>,
    Constant<170>, Constant<171>, Constant<172>, Constant<173>, Constant<174>,
    Constant<175>, Constant<176>, Constant<177>, Constant<178>, Constant<179>,
    Constant<180>, Constant<181>, Constant<182>, Constant<183>, Constant<184>,
    Constant<185>, Constant<186>, Constant<187>, Constant<188>, Constant<189>,
    Constant<190>, Constant<191>, Constant<192>, Constant<193>, Constant<194>,
    Constant<195>, Constant<196>, Constant<197>, Constant<198>, Constant<199>>;

template <std::size_t... Idx>
auto generate_tree(std::index_sequence<Idx...>) {
  using tree = Tree<TypeList<Constant<Idx>...>, TypeList<>>::type;
  return tree{};
}

template <typename T>
auto get_size(){
  return TreeTraits<T>::size;
}

// #include <iostream>
// template <typename t>
// void print(){
//   std::cout << __PRETTY_FUNCTION__ << '\n';
// }

int main() {
  using tree = decltype(generate_tree(std::make_index_sequence<20>()));
  auto test2 = tree{size_constant<10>{}, Constant<10>{}};
  auto size = get_size<tree>();
  auto foo = test2.get_n<0>();
  // auto foo2 = test2.get_n<100>();
  // auto foo3 = test2.get_n<199>();
}
