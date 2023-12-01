#ifndef _KETNEL_DEBUG_
#define _KETNEL_DEBUG_
void panic_spin(char* file, int line, char* func, char* condition);
#define PANIC_SPIN(...)(__FILE__, __LINE__, __func__, __VA_ARGS__) 
//如果不执行debug就将整个宏定义为空，以减小程序的体积
#ifdef NDEBUG
    #define ASSERT(CONDITION)((void)0) 
#else
    #define ASSERT(CONDITION)\
    if(CONDITION){              \
                                \
    }else{                      \
        PANIC_SPIN(#CONDITION); \
    }                           
#endif
#endif