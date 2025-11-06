#include "pdk-spdm-app-res-version-plat.h"

namespace pdk::spdm::platforms::res::version {

void get_versions(bool& support_spdm_1_1, bool& support_spdm_1_2, bool& support_spdm_1_3)
{
    support_spdm_1_1 = false;
    support_spdm_1_2 = true;
    support_spdm_1_3 = false;
    return;
}

}  // namespace pdk::spdm::platforms::res::version