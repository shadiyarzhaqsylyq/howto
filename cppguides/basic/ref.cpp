#include <iostream>

void setToZero(int& x) {
    x = 0; // Directly modifies the variable passed to it
}

int main() {
    int score = 100;
    setToZero(score);

    std::cout << score; // Outputs 0

    int num = 10;
    int& ref = num; // ref is now an alias for num

    ref = 20; // Changing the reference value

    std::cout << num; // Outputs 20
    return 0;
}
