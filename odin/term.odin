package main

import "core:fmt"
import "core:os"
import "core:sys/posix"

main :: proc() {
    // 1. Check if stdin is a terminal
    if !posix.isatty(posix.STDIN_FILENO) {
        fmt.eprintln("stdin is not a tty")
        return
    }

    // 2. Retrieve current terminal attributes
    tio: posix.termios
    if posix.tcgetattr(posix.STDIN_FILENO, &tio) != .OK {
        fmt.eprintln("Failed to get terminal attributes")
        return
    }

    // 3. Keep a backup to restore terminal state on exit
    orig_tio := tio
    defer posix.tcsetattr(posix.STDIN_FILENO, .TCSANOW, &orig_tio)

    // 4. Modify flags (Odin uses bit_sets for c_lflag, c_iflag, etc.)
    // Disable canonical mode and echoing
    tio.c_lflag -= {.ICANON, .ECHO}

    // Minimum number of characters to read and timeout (0.1s units)
    tio.c_cc[.VMIN]  = 1
    tio.c_cc[.VTIME] = 0

    // Apply the new terminal settings
    if posix.tcsetattr(posix.STDIN_FILENO, .TCSANOW, &tio) != .OK {
        fmt.eprintln("Failed to set terminal attributes")
        return
    }

    fmt.println("Press any key to continue...")

    buf: [1]byte
    bytes_read, _ := os.read(os.stdin, buf[:])
    if bytes_read > 0 {
        fmt.printfln("You pressed: '%c' (ASCII %d)", buf[0], buf[0])
    }
}
