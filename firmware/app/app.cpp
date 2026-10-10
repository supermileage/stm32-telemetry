#include "app.h"

static_assert(__cplusplus >= 201703L, "Application code requires C++17");

extern "C" void app_init(void)
{
    // Called before the scheduler. Add application task startup here later.
}
