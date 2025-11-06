#include "c2c_fault.h"

bool c2c_fault::is_under_fault_state()
{
    // For single core, do not need to care whether under fault state
    return false;
}