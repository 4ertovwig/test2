#ifndef NAIVE_EXTERNAL_SORT_HPP
#define NAIVE_EXTERNAL_SORT_HPP

#include <algorithm>
#include <future>
#include <vector>
#include <iostream>
#include <fstream>
#include <iterator>

#include <utils.hpp>

#ifndef USE_LOG
#   define USE_LOG
#endif

const unsigned kilo = 1024;
const unsigned mega = 1024 * 1024;
const unsigned empiric_constant = 2;    //константа найденная эмпирически

/*
 * политика соответствующая выполнению кода в разных режимах
 * parallel - то, что реально выполняется параллельно
 * synchronous - сортировка с помощью std::sort
*/
enum policy_ {
    parallel,
    synchronous
};

/*
 * класс реализующий сортировку больших файлов
 * @param Compare - функтор для сравнения элементов
 * по умолчанию стоит сортировка по возрастанию
 * @param policy  - политика сортировки
*/
template<typename Compare = std::less<int>,
         enum policy_ policy = parallel>
class naive_external_sort {

public:
    /*
     * конструктор
     * @param size - максимальное использованное количество памяти
     * @param input - имя файла который нужно сортировать
    */
    naive_external_sort(std::size_t size)
        : count(std::thread::hardware_concurrency()-1),
          size(size),
          files()
    {
    }

    /*
     * конструктор
     * @param size - максимальное использованное количество памяти
     * @param n - максимальное использованное количество потоков
     * @param input - имя файла который нужно сортировать
    */
    naive_external_sort(std::size_t size,
                        std::size_t n)
        : count(n),
          size(size),
          files()
    {
    }

    /*
     * деструктор
    */
    ~naive_external_sort() throw() = default;

    /*
     * функция возвращающая размер файла в байтах
     * @param str - название файла
     * @return - размер файла в байтах
    */
    long size_file(char* str) {
        std::ifstream is(str);
        is.seekg (0, is.end);
        long length = is.tellg();
        is.seekg (0, is.beg);
        return length;
    }

    /*
     * внутренняя структура для следования по файлу
     * с целью слияния файлов
    */
    struct stream_reader {
        // значение считанное из стрима определенного файла
        int value;
        // указателья на входной стрим временного файла
        // без указателя обойтись нельзя т.к. класс not assignable
        std::ifstream* in;
    };

    /*
     * непосредственно, сама функция сортировки
     * @param in  - имя входного файла
     * @param out - имя выходного файла
    */
    void sort(const std::string& input, const std::string& out) {

        // создаем отсортированные временные файлы
        try {
            create_temp_files(input);
        }
        catch(std::runtime_error& err)
        {
            std::cerr<<"\ncurrent file cannot be open";
        }
        catch(...)
        {
            std::cerr << "\nprobably error in multithreading synchronization";
        }

        // сливаем эти отсортированные временные файлы
        merge_temp_files(out);
    }

    /*
     * дружественная функция которая сливает вектор
     * отсортированных векторов в один выходной вектор
    */
    template<typename T>
    friend void merge_n_vectors(const std::vector<std::vector<T>>& in,
                                std::vector<T>& out);

private:
    /*
     * параллельная функция сортировки
     * на самом деле она синхронная т.е. нету никакого смысла
     * в std::future т.к. все равно ждем завершения асинхронного потока
     * см. в README там описано почему так
     * @param vec - входной вектор который загружен в память из файла
     * фактически это вектор in
    */
    void naive_parallel_mergesort(std::vector<int>& vec) {

        if(vec.empty())
            return;

        // размер одной части которая будет сортироваться параллельно
        int chunk = vec.size() / count;

        // вектор векторов полученных из разных потоков
        // из-за этого дополнительная память увеличивается вдвое
        std::vector<std::vector<int>> results;

        // сортируем каждую часть (псевдо)параллельно
        // скорее всего будет inline лямбды и сработает RVO
        // нет гарантии что std::future вообще будет выполняться асинхронно
        for(uint i=0; i< count; ++i) {
            if( i< count - 1 ) {
               std::future<std::vector<int>>&& res =
                       std::async(std::launch::async,
                                  [&]{ std::sort(std::begin(vec) + i*chunk,
                                                 std::begin(vec) + (i+1)*chunk,
                                                 Compare());
                                     return std::vector<int> (std::begin(vec) + i*chunk,
                                                           std::begin(vec) + (i+1)*chunk);
                                  });
               auto&& s = res.get();    //вот это убивает всю параллельность
               results.push_back(std::move(s));
            }
            else {
               std::future<std::vector<int>>&& res =
                       std::async(std::launch::async,
                                  [&]{ std::sort(std::begin(vec) + i*chunk,
                                                 std::end(vec),
                                                 Compare());

                                     return std::vector<int> (std::begin(vec) + i*chunk,
                                                              std::end(vec));
                                  });
               auto&& s = res.get();
               results.push_back(std::move(s));
            }
        }

        // очищаем входной вектор чтобы можно было сливать туда полученные векторы
        vec.clear();

        // сливаем все векторы в один выходной
        merge_n_vectors(results, vec);
    }

    /*
     * функция для проверки на true всего вектора
     * вроде такой в алгоритмах std нету
     * @param vec - входной вектор для проверки
     * @return - true если все члены вектора true
    */
    template<typename T>
    bool guard(std::vector<T>& vec) {
        for(auto& el : vec) {
            if(!el)
                return false;
        }
        return true;
    }

    /*
     * параллельная функция сортировки
     * выигрывает у std::sort по скорости
     * @param vec - входной вектор который загружен в память из файла
     * фактически это вектор in
    */
    void true_parallel_mergesort(std::vector<int>& vec) {

        if(vec.empty())
            return;

        std::mutex m_barrier;           // мьютекс для захвата при ожидании начала выполнения слияния
        std::mutex m_vec;               // мьютекс для защиты векторов
        std::condition_variable var;    // условная переменная с помощью которой и ждем начало слияния

        // размер одной части которая будет сортироваться параллельно
        int chunk = vec.size() / count;

        // вектор векторов полученных из разных потоков
        // из-за этого дополнительная память увеличивается вдвое
        std::vector<std::vector<int>> results;

	// вектор потоков в каждом из которых по отдельности сортируем части входного вектора
        std::vector<std::thread> vec_th;

        // сортируем каждую часть параллельно
        // скорее всего будет inline лямбды и сработает RVO
        // индекс обязательно захватываем по копии а не по значению
        for(uint i=0; i< count; ++i) {
            if( i< count - 1 ) {
                vec_th.emplace_back([i, chunk, &m_vec, &vec, &var, &results]{
                    std::sort(std::begin(vec) + i*chunk,
                              std::begin(vec) + (i+1)*chunk);

                    // блокируем векторы для добавления элементов
                    std::unique_lock<std::mutex> lk(m_vec);
                    results.emplace_back(std::begin(vec) + i*chunk,
                                         std::begin(vec) + (i+1)*chunk);
                });
            } else {
                vec_th.emplace_back([i, chunk, &m_vec, &vec, &var, &results]{
                    std::sort(std::begin(vec) + i*chunk,
                              std::end(vec));

                    std::unique_lock<std::mutex> lk(m_vec);
                    results.emplace_back(std::begin(vec) + i*chunk,
                                         std::end(vec));
                });
            }
        }

        for(auto& el : vec_th)
            el.join();

        // очищаем входной вектор чтобы можно было сливать туда полученные векторы
        vec.clear();

        // сливаем все векторы в один выходной
        merge_n_vectors(results, vec);
    }

    /*
     * функция сортировки входного вектора
     * в зависимости от политики может быть синхронной или асинхронной
     * @param vec - входной вектор, получающийся из кусочно-разбитого
     * входного файла
    */
    template<typename Comp = Compare>
    void sort_temp_vector(std::vector<int>& vec)
    {
        if(policy == parallel)
            true_parallel_mergesort(vec);
        else
            std::sort(std::begin(vec), std::end(vec), Compare());
    }

    /*
     * далеко не самая быстрая реализация слияния файлов
     * @param out - имя выходного файла
    */
    void merge_temp_files(const std::string& out) {

    #ifdef USE_LOG
        std::cout<<"\nstart merging";
    #endif

        // вектор содержащий позицию и стрим временного файла
        std::vector<stream_reader> pos_streams;

        // вектор стримов открывающих временные файлы
        std::vector<std::ifstream> streams;
        for(auto& el : files)
            streams.push_back(std::move(std::ifstream { el }));

        //добавляем точки входа в каждый стрим
        int temp = 0;
        for(auto& el : streams) {
            el >> temp;
            pos_streams.push_back({ temp, &el });
        }

        std::ofstream of(out);
        for( ; pos_streams.size(); )
        {
            // находим минимальный элемент из двух файлов
            // и так для каждого файла и выбранного элемента
            auto g = std::min_element(std::begin(pos_streams), std::end(pos_streams),
                                        [](stream_reader& el1, stream_reader& el2) {
                                            Compare cmp; return cmp(el1.value, el2.value);
                                        });
            of << g->value << "\n";
            if (!((*g->in) >> g->value))
            {
                pos_streams.erase(g);
            }
        }
    #ifdef USE_LOG
        std::cout<<"\nend merging";
    #endif
    }

    /*
     * функция создающая временные файлы из внешнего файла
     * и сортирующая их согласно сортирующему функтору
     * @param1 - имя входного файла
    */
    void create_temp_files(const std::string& str) {

        // считаем количество строк входного файла
        //std::istream in(str);
        //auto& is = in.get();
        std::ifstream is(str);
        long long numLines = 0;
        std::string dummy;
        while ( std::getline(is, dummy) )
            ++numLines;

        // максимальное количество элементов вектора в который можно
        // записывать данные из входного файла
        // делим на эмпирическую константу которая соответствует
        // дополнительной занимаемой памяти
        long s_files = this->size * kilo / ( empiric_constant * sizeof(int) );

        // сколько временных файлов нужно создать
        unsigned count = numLines / s_files;

        // итератор на начало файла
        is.clear();
        is.seekg(0, std::ios::beg);
        //auto it = std::istream_iterator<int>(is);

        for( uint i=0; i < count; ++i ) {

            std::vector<int> temp/*(s_files)*/;

            // сдвигаем курсор на размер чуть меньше которого должна занимать программа
         //   std::advance(it, i * s_files);
            is.seekg( i* s_files );

            //копируем
         //   std::copy_n(it, s_files, std::begin(temp));
            int value, pos = 0;
            if( i < count - 1) {
                while(is >> value && pos++ < s_files) {
                    temp.push_back(value);
                }
            } else {
                long len = numLines - i * s_files;
                while(is >> value && pos++ < len) {
                    temp.push_back(value);
                }
            }

            //сортируем куски
            sort_temp_vector<Compare>(temp);

            //имя временного файла
            std::string name = std::to_string(i) + ".dat";

            // копируем содержимое буфера во временный файл
            std::ofstream ofs(name, std::ofstream::out);
            std::copy(std::begin(temp), std::end(temp),
                      std::ostream_iterator<int>{ ofs, "\n"});

            //добавляем имя временного файла в список временных файлов
            files.push_back(name);

        #ifdef USE_LOG
            std::cout<<"create file "<<name<<"\n";
        #endif
        }
    }


    // количество потоков
    std::size_t count;
    // количество занимаемой памяти в мегабайтах
    std::size_t size;
    // список имен временных файлов
    std::vector<std::string> files;
};

#endif // NAIVE_EXTERNAL_SORT_HPP
