#include "papi_sde_interface.h"

#pragma weak papi_sde_init
#pragma weak papi_sde_register_counter
#pragma weak papi_sde_describe_counter


papi_handle_t 
__attribute__((weak)) 
papi_sde_init(char *name_of_library, int event_count)
{
    printf("weak papi_sde_init called from %s\n", __FILE__);
    void * ptr = NULL;
    return ptr;
}

void 
__attribute__((weak)) 
papi_sde_register_counter(papi_handle_t handle, char *event_name, long long int *counter)
{
    printf("weak papi_sde_register_counter called from %s\n", __FILE__);
}

void 
__attribute__((weak)) 
papi_sde_describe_counter(papi_handle_t handle, char *event_name, char *event_description)
{
    printf("weak papi_sde_describe_counter called from %s\n", __FILE__);
}
