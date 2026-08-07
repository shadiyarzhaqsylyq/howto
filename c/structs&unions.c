#include <stdio.h>

// Define an enum for the active data type
typedef enum { TYPE_INT, TYPE_FLOAT } DataType;

typedef struct {
    DataType type; // The "tag" tracking active data
    union {
        int intValue;
        float floatValue;
    } data; // Nested anonymous union
} Variant;

int main() {
    Variant v;
    
    // Storing an integer
    v.type = TYPE_INT;
    v.data.intValue = 42;
    
    // Safely reading the data based on the tag
    if (v.type == TYPE_INT) {
        printf("Integer: %d\n", v.data.intValue);
    }
    return 0;
}

#include <stdio.h>

typedef union {
    // Layout 1: View data as individual point coordinates
    struct {
        int x;
        int y;
    } point;

    // Layout 2: View the exact same data as a 2-element array
    int raw_array[2];
} Vector2D;

int main() {
    Vector2D vec;

    // Modify via the struct layout
    vec.point.x = 10;
    vec.point.y = 20;

    // Access via the array layout (shares the same memory)
    printf("Array index 0: %d\n", vec.raw_array[0]); // Outputs 10
    printf("Array index 1: %d\n", vec.raw_array[1]); // Outputs 20

    return 0;
}
/*
### Structs ### 
#include

struct Node{
int data;
struct Node *next;
};

int main(){
struct Node first;
struct Node second;

first.data = 10;
second.data = 20;

first.next = &second;
second.next = NULL;

printf("Fist node data: %data\n", first.data);
printf("Seccond node data via pointer: %d\n ", first.next->data);
}

### Structs






*/
