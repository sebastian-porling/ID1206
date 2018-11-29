void test();
void test_constant(int count, int size);
void *balloc(size_t size);
void bfree(void *memory);
int get_size(void *memory);
int getNumberOfPages();
struct head *magic(void *memory);