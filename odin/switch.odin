package main

import "core:fmt"

main :: proc() {
	program := "a b c d e"

	for token in program {
		switch token {
		case 'a': fmt.printf("a\n")
		case 'b': fmt.printf("b\n")
		case 'c': fmt.printf("c\n")
		case 'd': fmt.printf("d\n")
		case 'e': fmt.printf("e\n")
		case ' ': fmt.printf("space\n")
        case: //everything else
		}
	}

	fmt.printf("The program \"%s\" ",
	           program,)
}
