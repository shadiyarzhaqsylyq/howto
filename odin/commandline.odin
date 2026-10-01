package simple_repl

import "core:fmt"
import "core:os"
import "core:bufio"
import "core:strings"

main :: proc () {
    reader: bufio.Reader
    
    in_stream := os.to_stream(os.stdin)
    bufio.reader_init(&reader, in_stream)
    for {
        fmt.print(" REPL > ")
        err := os.flush(os.stdin)
        text, _ := bufio.reader_read_string(&reader, '\n')

        if strings.compare("exit", strings.trim_space(text)) == 0 {
            break
        } else {
            fmt.println("command: ", text)
        }
    }
}
