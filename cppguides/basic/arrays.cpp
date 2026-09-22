#include <iostream>
#include <iterator>
#include <array>
void print_nums(const std::array<int, 3u> & numbers){

    for(const auto number: numbers) 
    {	
		std::cout << std::noshowpos;
        std::cout << number << '\n';
    }

}

int main() 
{
    std::array<int, 3u> v = {3, 1, 4};
    auto vi = std::begin(v);
    std::cout << std::showpos << *vi << '\n'; 
    
    int a[] = {-5, 10, 15};
    auto ai = std::begin(a);
    std::cout << *ai << '\n';
	print_nums(v);
return 0;
}
