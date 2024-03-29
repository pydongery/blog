#include <bits/utility.h>
#include <memory>
#include <utility>
#include <type_traits>
#include <concepts>

template <typename T, typename... Ts>
consteval std::size_t get_type_index(){
    std::size_t index = 0;
    (void)((!std::same_as<T, Ts> ? ++index, false : true) || ...);
    return index;
}

template <typename T, typename... Ts>
constexpr inline std::size_t type_index = get_type_index<T, Ts...>();

template <std::size_t N>
using size_constant = std::integral_constant<std::size_t, N>;

template <typename... Ts>
union RecursiveUnion;

template <typename T, typename... Ts>
union RecursiveUnion<T, Ts...> {
    T value;
    RecursiveUnion<Ts...> tail;

    // constexpr RecursiveUnion() = default;

    // constexpr RecursiveUnion(size_constant<0>, T&& obj)
    //     : value{std::forward<T>(obj)} {}
    // template <std::size_t N, typename U>
    // constexpr RecursiveUnion(size_constant<N>, U&& obj)
    //     : tail{size_constant<N - 1>{}, std::forward<U>(obj)} {}
};

template <typename T1, typename T2, typename T3, typename T4, typename... Ts>
union RecursiveUnion<T1, T2, T3, T4, Ts...> {
    T1 value1;
    T2 value2;
    RecursiveUnion<Ts...> tail;

    // constexpr RecursiveUnion(size_constant<0>, T1&& obj)
    //     : value1{std::forward<T1>(obj)} {}

    // constexpr RecursiveUnion(size_constant<1>, T2&& obj)
    //     : value2{std::forward<T2>(obj)} {}

    // template <std::size_t N, typename U>
    // constexpr RecursiveUnion(size_constant<N>, U&& obj)
    //     : tail{size_constant<N - 2>{}, std::forward<U>(obj)} {}
};


template <typename T>
union RecursiveUnion<T> {
    T value;

    constexpr RecursiveUnion() = default;
    constexpr RecursiveUnion(size_constant<0>, T&& obj)
        : value{std::forward<T>(obj)} {}
};

// template <std::size_t Idx, typename U>
// constexpr decltype(auto) get_n(U const& union_) {
//     // TODO union_ should be U&& not U const&
//     if constexpr (Idx == 0) {
//         return union_.value;
//     } else if constexpr (Idx == 1) {
//         return union_.tail.value;
//     } else if constexpr (Idx == 2) {
//         return union_.tail.tail.value;
//     }
//     // else if constexpr (Idx >= 8) {
//     //     if constexpr(requires{union_.tail.tail.tail.tail.tail.tail.tail.tail;}){
//     //         return get_n<Idx - 8>(union_.tail.tail.tail.tail.tail.tail.tail.tail);
//     //     }
//     // }
//     else {
//         if constexpr(requires{union_.tail.tail.tail;}){
//             return get_n<Idx - 3>(union_.tail.tail.tail);
//         }
//     }
//     std::unreachable();
// }

// template <typename T, typename V>
// constexpr decltype(auto) get(V const& variant_) {
//     return get_n<V::template index_of<T>>(variant_.storage);
// }

template <typename... Ts>
struct Variant {
private:
    using union_type = RecursiveUnion<Ts...>;
    union_type storage;
    std::size_t index;



    template <typename T, typename V>
    friend constexpr decltype(auto) get(V const&);

    // template <typename F, std::size_t... Is>
    // using return_type = std::common_type_t<
    //         std::invoke_result_t<
    //             F,
    //             std::invoke_result_t<
    //                 decltype(get_n<Is, union_type>),
    //                 union_type
    //             >
    //         >...
    //     >;
public:
    Variant() = default;
    template <typename T>
    constexpr static auto index_of = type_index<T, Ts...>;

    template <typename T>
        requires(std::same_as<T, Ts> || ...)
    Variant(T&& t)
        : index(type_index<T, Ts...>),
          storage(size_constant<type_index<T, Ts...>>{},
                  std::forward<T>(t)) {}

    //TODO
    Variant(const Variant&) = delete;
    Variant& operator=(const Variant&) = delete;
    Variant(Variant&&) = delete;
    Variant& operator=(Variant&&) = delete;

    // constexpr decltype(auto) visit(auto callback) const {
    //     // return dispatch(std::make_index_sequence<sizeof...(Ts)>{}, F);
    //     return dispatch_recursive(callback);
    // }
};

// #include <variant>
#include <cstdio>


template <std::size_t> struct Constant{};

using tree = Variant<Constant<0>, Constant<1>, Constant<2>, Constant<3>, Constant<4>, Constant<5>, Constant<6>, Constant<7>, Constant<8>, Constant<9>, Constant<10>, Constant<11>, Constant<12>, Constant<13>, Constant<14>, Constant<15>, Constant<16>, Constant<17>, Constant<18>, Constant<19>, Constant<20>, Constant<21>, Constant<22>, Constant<23>, Constant<24>, Constant<25>, Constant<26>, Constant<27>, Constant<28>, Constant<29>, Constant<30>, Constant<31>, Constant<32>, Constant<33>, Constant<34>, Constant<35>, Constant<36>, Constant<37>, Constant<38>, Constant<39>, Constant<40>, Constant<41>, Constant<42>, Constant<43>, Constant<44>, Constant<45>, Constant<46>, Constant<47>, Constant<48>, Constant<49>, Constant<50>, Constant<51>, Constant<52>, Constant<53>, Constant<54>, Constant<55>, Constant<56>, Constant<57>, Constant<58>, Constant<59>, Constant<60>, Constant<61>, Constant<62>, Constant<63>, Constant<64>, Constant<65>, Constant<66>, Constant<67>, Constant<68>, Constant<69>, Constant<70>, Constant<71>, Constant<72>, Constant<73>, Constant<74>, Constant<75>, Constant<76>, Constant<77>, Constant<78>, Constant<79>, Constant<80>, Constant<81>, Constant<82>, Constant<83>, Constant<84>, Constant<85>, Constant<86>, Constant<87>, Constant<88>, Constant<89>, Constant<90>, Constant<91>, Constant<92>, Constant<93>, Constant<94>, Constant<95>, Constant<96>, Constant<97>, Constant<98>, Constant<99>, Constant<100>, Constant<101>, Constant<102>, Constant<103>, Constant<104>, Constant<105>, Constant<106>, Constant<107>, Constant<108>, Constant<109>, Constant<110>, Constant<111>, Constant<112>, Constant<113>, Constant<114>, Constant<115>, Constant<116>, Constant<117>, Constant<118>, Constant<119>, Constant<120>, Constant<121>, Constant<122>, Constant<123>, Constant<124>, Constant<125>, Constant<126>, Constant<127>, Constant<128>, Constant<129>, Constant<130>, Constant<131>, Constant<132>, Constant<133>, Constant<134>, Constant<135>, Constant<136>, Constant<137>, Constant<138>, Constant<139>, Constant<140>, Constant<141>, Constant<142>, Constant<143>, Constant<144>, Constant<145>, Constant<146>, Constant<147>, Constant<148>, Constant<149>, Constant<150>, Constant<151>, Constant<152>, Constant<153>, Constant<154>, Constant<155>, Constant<156>, Constant<157>, Constant<158>, Constant<159>, Constant<160>, Constant<161>, Constant<162>, Constant<163>, Constant<164>, Constant<165>, Constant<166>, Constant<167>, Constant<168>, Constant<169>, Constant<170>, Constant<171>, Constant<172>, Constant<173>, Constant<174>, Constant<175>, Constant<176>, Constant<177>, Constant<178>, Constant<179>, Constant<180>, Constant<181>, Constant<182>, Constant<183>, Constant<184>, Constant<185>, Constant<186>, Constant<187>, Constant<188>, Constant<189>, Constant<190>, Constant<191>, Constant<192>, Constant<193>, Constant<194>, Constant<195>, Constant<196>, Constant<197>, Constant<198>, Constant<199>>;

int main(){
  auto test2 = tree{Constant<10>{}};
  auto foo = get<Constant<0>>(test2);
  // auto foo2 = get<Constant<100>>(test2);
  // auto foo3 = get<Constant<199>>(test2);
}
