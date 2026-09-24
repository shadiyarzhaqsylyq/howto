// Do not use this
void function_30(void) {
    function_1(); // ERROR: Compiler doesn't know what function_1 is yet
}

// ... 50,000 lines of code ...

// Line 50,000
void function_1(void) {
    // Implementation
}

// Correct technique
void function_1(void) {
    // Implementation
}
void function_30(void) {
    function_1(); // Correct. Will compile
}


/*
Public function
Declare in .h and define .c
no static

Private helper
Define and declare in .c only
uses static only

Static Inline
Use static inline for tiny functions. Declare it only in header file
*/
