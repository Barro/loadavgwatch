#include "loadavgwatch-impl.h"

loadavgwatch_status loadavgwatch_impl_open(
    const loadavgwatch_state* state, void** out_impl_state)
{
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_impl_close(void* state)
{
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_impl_get_load_average(
    void* state, float* out_loadavg)
{
    *out_loadavg = 1.0;
    return LOADAVGWATCH_OK;
}
