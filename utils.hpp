#ifndef UTILS_HPP
#define UTILS_HPP

#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

/*
 * макрос которым мерил время сортировки да и вообще общее время
 * в макрос вставляется любой код и по окончанию работы этого куска кода
 * в стандартный поток вывода выводится значение времени в наносекундах
*/
#ifndef M_TIME
#define M_TIME(...)  \
            {                                                                               \
                auto start = std::chrono::steady_clock::now();                              \
                __VA_ARGS__                                                                 \
                auto end = std::chrono::steady_clock::now();                                \
                auto diff = end - start;                                                    \
                auto diff_sec = std::chrono::duration_cast<std::chrono::nanoseconds>(diff); \
                std::cout << "\nnanoseconds :"<<diff_sec.count() << std::endl;              \
            }
#endif

/*
 * функтор использующийся для заполнения вектора рандомными значениями
*/
template<typename T = int>
struct fill_random {

    /*
     * перегруженный оператор
     * @param vec - входной вектор который нужно заполнить
     * @param min - минимальное значение
     * @param max - максимальное значение
    */
    void operator()(std::vector<T>& vec, T min, T max) {
        std::mt19937 mt;
        std::uniform_int_distribution<T> d(min, max);
        std::generate(std::begin(vec), std::end(vec), [&] { return d(mt); });
    }
};

/*
 * шаблонная функция сливающая несколько векторов в один выходной
 * @param in - входной вектор отсортированных векторов
 * @param out - выходной отсортированный вектор
*/
template<typename T>
void merge_n_vectors(const std::vector<std::vector<T>>& in,
                     std::vector<T>& out) {

    auto end = std::end(in);
    std::vector<T> temp;
    for (auto it = std::begin(in); it != end; ++it) {
        temp = std::move(out);
        std::merge(std::begin(*it), std::end(*it),
                   std::begin(temp), std::end(temp),
                   std::back_inserter(out));
    }
}

#endif // UTILS_HPP
