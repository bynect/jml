#include <jml_module.h>


#if JML_PLATFORM == 1 || JML_PLATFORM == 2

#include <dlfcn.h>
#include <unistd.h>
/*TODO*/

#elif JML_PLATFORM == 3

#include <windows.h>
/*TODO*/

#else

/*TODO*/

#endif
