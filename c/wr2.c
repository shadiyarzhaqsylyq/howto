//pwritev2()
// 1. Define the count of pages we are writing
int iovcnt = 3; 

// 2. Allocate the array of vectors
struct iovec iov[3];

// 3. Point each vector to a scattered buffer pool page
iov[0].iov_base = buffer_pool_page_A;
iov[0].iov_len  = 4096;

iov[1].iov_base = buffer_pool_page_B;
iov[1].iov_len  = 4096;

iov[2].iov_base = buffer_pool_page_C;
iov[2].iov_len  = 4096;

// 4. Issue the write (this gathers all 3 pages into 1 contiguous disk write)
pwritev2(fd, iov, iovcnt, file_offset, 0);
