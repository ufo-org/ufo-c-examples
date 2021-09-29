#pragma once
//#define DEBUG
#ifdef DEBUG 
#define LOG(...) fprintf(stderr, "DEBUG: " __VA_ARGS__)
#define LOG_SHORT(...) fprintf(stderr,  __VA_ARGS__)
#else
#define LOG(...) 
#define LOG_SHORT(...) 
#endif
#define WARN(...) fprintf(stderr, "WARNING: " __VA_ARGS__)
#define REPORT(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define INFO(...) fprintf(stderr, __VA_ARGS__)