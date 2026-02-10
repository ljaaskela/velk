#include "registry.h"

#include "strata_export.h"

STRATA_EXPORT IRegistry &GetRegistry()
{
    static Registry r;
    return *(r.GetInterface<IRegistry>());
}
