#ifndef KERNEL_DEV_FS_H
#define KERNEL_DEV_FS_H

using Character_Device_Descriptor_Send = bool(*)(void* user, char ch);
using Character_Device_Descriptor_Recv = bool(*)(void* user, char* ch);

struct Character_Device_Descriptor {
    const char* Name;
    Character_Device_Descriptor_Send Send;
    Character_Device_Descriptor_Recv Recv;
};

int CharDev_Init();
int CharDev_Register(void* user, Character_Device_Descriptor*);

#endif /* KERNEL_DEV_FS_H */
