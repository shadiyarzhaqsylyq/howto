#include <iostream>
#include <iterator>
#include <vector>
#include <array>
#include <ranges>
#include <algorithm>


int main() 
{
   std::vector<double> cnt{1.01, 2.02, 3.03, -5.04};
    auto lambda = [](double value){ return value < 0.0; };
    auto erased = std::erase_if(cnt, lambda);
	bool invalid = lambda(-67);
	std::cout << invalid << '\n';
    std::cout <<  erased << " prices below zero\n";
return 0;
}
