#include "hd_driver.h"

#include "stddef.h"

struct hd_subdriver_desc_t
{
    int port;
    int slavebit;
    size_t startlba;
    size_t len;
}

void init_hd_driver_module()
{

}
