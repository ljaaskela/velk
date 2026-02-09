#include "registry.h"

#include "ltk_export.h"

LTK_EXPORT IRegistry &GetRegistry()
{
    static Registry r;
    return *(r.GetInterface<IRegistry>());
}
