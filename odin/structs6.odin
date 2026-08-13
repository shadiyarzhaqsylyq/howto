package main
import "core:fmt"

// 1. Define the union type
VectorValue :: union {
    f32,
    [3]f32,
}

// 2. Place it inside the struct
Entity :: struct {
    name:  string,
    id:    u32,
    pos:   VectorValue, // Field holding the union
}

main :: proc() {
    // Initialization
    e1 := Entity{
        name = "Player",
        id   = 1,
        pos  = f32(5.5), // Assigning a single float variant
    }

    e2 := Entity{
        name = "Enemy",
        id   = 2,
        pos  = [3]f32{10.0, 20.0, 30.0}, // Assigning an array variant
    }

    // Accessing via a type switch
    switch v in e1.pos {
    case f32:    fmt.println(e1.name, "is at 1D scalar:", v)
    case [3]f32: fmt.println(e1.name, "is at 3D vector:", v)
    }
}
