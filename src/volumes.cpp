#include "common.h"
#include "volumes.h"
#include "disk.h"
#include "utils.h"
#include "logging.h"

#define MAX_VOLUMES (128)

struct Volume_State {
    Volume_Descriptor desc;
    void* filesystem;
};

static Volume_State gaVolumes[MAX_VOLUMES];
static u32 giVolumesLastIndex = 0;

Volume_Handle Volume_Register(const Volume_Descriptor* vol) {
    Volume_Handle ret;
    ASSERT(giVolumesLastIndex < MAX_VOLUMES);

    auto& S = gaVolumes[giVolumesLastIndex];
    S.desc = *vol;
    S.filesystem = NULL;

    logprintf("New volume registered: disk %d start %x size %x\n", vol->disk, vol->offset, vol->length);

    giVolumesLastIndex++;

    return ret;
}

Filesystem Volume_Get_Filesystem(Volume_Handle vol) {
    Filesystem ret = NULL;

    if(vol < giVolumesLastIndex) {
        ret = gaVolumes[vol].filesystem;
    }

    return ret;
}