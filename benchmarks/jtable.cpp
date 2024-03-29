#include <utility>

template <std::size_t I> int handler(){return I;}


template <std::size_t ... Is>
int f2(int i, std::index_sequence<Is...>)
{
    int ret = -1;
    bool found = ((i == Is ? (ret = handler<Is> ()), true : false) || ...);
    return ret;
}

int run_f2(int x) {
    return f2(x, std::make_index_sequence<6>());
}

int run_manual (int x) {
    switch (x) {
       case 0: return handler<0>();
       case 1: return handler<1>();
       case 2: return handler<2>();
       case 3: return handler<3>();
       case 4: return handler<4>();
       case 5: return handler<5>();
    }
    return -1;
}

int main() {}
