// Line 3
void function_30(void) {
    function_1(); // ERROR: Compiler doesn't know what function_1 is yet
}

// ... 50,000 lines of code ...

// Line 50,000
void function_1(void) {
    // Implementation
}

Correct version
// Line 1: Tell the compiler the signature exists ahead of time
void function_1(void); 

// Line 3
void function_30(void) {
    function_1(); // WORKS: Compiler knows the return type and parameter types
}

// ... 50,000 lines of code ...

// Line 50,000
void function_1(void) {
    // Implementation
}


/*
Public function
Declare in .h and define .c
no static

Private helper
Define and declar in .c only
uses static only

Static Inline
Use static inline for tiny functions. Declare it only in header file
*/
