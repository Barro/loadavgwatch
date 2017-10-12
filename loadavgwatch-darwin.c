#include "loadavgwatch-impl.h"
#include <sys/sysctl.h>

loadavgwatch_status loadavgwatch_impl_open(
    const loadavgwatch_state* state, void** out_impl_state)
{
    float load;
    loadavgwatch_status status = loadavgwatch_impl_get_load_average(
        NULL, &load);
    if (status != LOADAVGWATCH_OK) {
        PRINT_LOG_MESSAGE(state->log_error, "Initial load reading failed!");
        return status;
    }
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
    int mib[2] = {CTL_VM, VM_LOADAVG};
    size_t len = sizeof(mib) / sizeof(mib[0]);
    struct loadavg load;
    size_t load_size = sizeof(load);
    int read_result = sysctl(mib, len, &load, &load_size, NULL, 0);
    if (read_result == ENOMEM) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }
    if (read_result != 0) {
        return LOADAVGWATCH_ERR_READ;
    }
    *out_loadavg = (float)load.ldavg[0] / load.fscale;
    return LOADAVGWATCH_OK;
}
