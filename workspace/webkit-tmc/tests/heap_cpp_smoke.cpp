/* Real freestanding C++20 consumer of exported C allocation symbols.
 * No STL, exceptions, operator-new runtime or WebKit claim. */
#include <stdlib.h>
#include <stdint.h>
class Buffer {
    unsigned char *p;
public:
    explicit Buffer(size_t n):p(static_cast<unsigned char*>(malloc(n))){}
    ~Buffer(){free(p);}
    Buffer(const Buffer&)=delete;
    Buffer&operator=(const Buffer&)=delete;
    unsigned char *data(){return p;}
};
extern "C" int heap_cpp_smoke(void)
{
 Buffer b(4096);if(!b.data())return 1;
 for(unsigned i=0;i<4096;i++)b.data()[i]=static_cast<unsigned char>(i*13);
 for(unsigned i=0;i<4096;i++)if(b.data()[i]!=static_cast<unsigned char>(i*13))return 2;
 void *p=aligned_alloc(4096,8192);if(!p)return 3;
 bool valid=!(reinterpret_cast<uintptr_t>(p)&4095);free(p);return valid?0:4;
}
