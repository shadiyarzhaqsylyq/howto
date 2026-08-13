#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
// open(), pwrite(), pread()
int main() {
    // 1. Open the file to get a file descriptor (int)
    int fd = open("example.txt", O_RDWR | O_CREAT, 0644); // can use O_EXCL - 
    // O_RDWR(read\write) |(or) O_CREAT(If it does not exit). 
    // 0644 - 6(owner/me) can Read(4) + Write(2) = 6. 0 tells that its octal number
    // 44 - 4(Group), 4(Everyone else)
    // 0644 tells that only I can read and write, everyone else can only read.
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    // 2. Write "HELLO" at an exact offset of 10 bytes
    // This does NOT move the system's file offset pointer
    const char *text = "HELLO";
    ssize_t bytes_written = pwrite(fd, text, 5, 10); 

    // 3. Read 5 bytes from offset 10 into a buffer
    char buffer[6] = {0};
    ssize_t bytes_read = pread(fd, buffer, 5, 10);

    printf("Read from offset 10: %s\n", buffer);

    // 4. Check the current file offset using lseek
    // It will still be 0, because pread/pwrite don't move it!
    off_t current_offset = lseek(fd, 0, SEEK_CUR);
    printf("Current file pointer position: %ld\n", (long)current_offset);

    close(fd);
    return 0;
}
