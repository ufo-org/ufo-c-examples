#include "nycpp.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "seq.h"

#ifdef __cplusplus
}
#endif

#include <iostream>

// void *nypp_seq_creation(Arguments *config, AnySystem system) {
//     //return (void *) seq_normil_from_length(1, config->size, 2);

//     //seq_normil_from_length(1, config->size, 2);
// }

// void nypp_seq_cleanup(Arguments *config, AnySystem system, AnyObject object) {
//     seq_normil_free(object);
// }

// void nypp_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next) {
//     int64_t *data = (int64_t *) object;    
//     int64_t sum = 0;
//     SequenceResult result;
//     while (true) {
//         result = next(config, sequence);        
//         if (result.end) {
//             break;
//         }        
//         if (result.write) {
//             data[result.current] = random_int(1000);
//         } else {
//             sum += data[result.current];
//         }
//     };
// }


int main(int argc, char *argv[]) {

    // Wrapper arr(10);

    // arr[1] = 1;

    // // int32_t i = arr[3];
    // std::cout << arr[1] << "\n";
    // std::cout << arr[0] << "\n";

    return 0;
}

