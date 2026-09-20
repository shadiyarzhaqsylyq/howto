//  Recommended
auto w = std::make_unique<Widget>();

// Avoid 
std::unique_ptr<Widget> w(new Widget());

// Using with move
std::vector<std::unique_ptr<Widget>> widgets;
auto w = std::make_unique<Widget>();

widgets.push_back(std::move(w)); // 'w' is now empty; vector owns it

// Do not use smart pointers with int, double and self managing objects std::string, vector keep them on the stack
