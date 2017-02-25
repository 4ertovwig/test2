#include <naive_external_sort.hpp>


int main(int argc, char *argv[])
{
	// заполняем вектор для примера
	// и создаем файл для примера
	//std::vector<int> vec(10000000);
	//fill_random<> rnd; rnd(vec, 0, 255);
	//std::ofstream fs("test1.txt");
	//std::copy(std::begin(vec), std::end(vec),
	//	    std::ostream_iterator<int>{fs, "\n"});
	
	// делаем параллельный режим сортировки по возрастанию
	naive_external_sort<std::greater<int>, parallel> sorter(atoi(argv[2]) * kilo, atoi(argv[4]));
	sorter.sort(argv[6],"ooo.txt");

	// проверка на правильность сортировки файла
	// надо быть аккуратнее т.к. файл может быть здоровый
	//std::vector<int> vv;
	//std::ifstream in("ooo.txt");
	//std::copy(std::istream_iterator<int>{in},
	//	    std::istream_iterator<int>{},
	//          std::back_inserter(vv));
	//std::cout<<"ooo.txt is sorted :"<<std::is_sorted(std::begin(vv), std::begin(vv), std::greater<int>())<<std::endl;
}
