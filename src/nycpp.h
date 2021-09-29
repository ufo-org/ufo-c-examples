#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "bench.h"

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <memory>

template <class T>
class NYCpp {
    public:
    NYCpp(size_t n): inner(std::unique_ptr<T[]>(new int32_t[n])), size(n) {}
    ~NYCpp() {}
    T operator [](int i) const {
        return inner[i];
    }
    T& operator [](int i) {
        return inner[i];
    }
    private:
    std::unique_ptr<T[]> inner;
    size_t size;
};

#endif

void *nypp_seq_creation(Arguments *config, AnySystem system);
void nypp_seq_cleanup(Arguments *config, AnySystem system, AnyObject object);
void nypp_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next);