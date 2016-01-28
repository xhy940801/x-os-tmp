#include "block_driver.h"

#include "string.h"
#include "block_buffer.h"

static struct block_driver_desc_t* block_drivers[32];

void block_driver_module_init()
{
    _memset(block_drivers, 0, sizeof(block_drivers));
}

int block_driver_register(struct block_driver_desc_t* driver, uint16_t main_driver)
{
    if(block_drivers[main_driver] != NULL)
        return -1;
    block_drivers[main_driver] = driver;
    return 0;
}

int block_driver_read_blocks(struct block_buffer_desc_t* block_buffers[], size_t count, int timeout)
{
    if(block_drivers[block_buffers[0]->main_driver] == NULL)
    {
        return -1;
    }
    if(block_drivers[block_buffers[0]->main_driver]->read_blocks == NULL)
    {
        return -1;
    }
    return block_drivers[block_buffers[0]->main_driver]->read_blocks(block_buffers, count, timeout);
}

int block_driver_write_blocks(struct block_buffer_desc_t* block_buffers[], size_t count, int timeout)
{
    if(block_drivers[block_buffers[0]->main_driver] == NULL)
    {
        return -1;
    }
    if(block_drivers[block_buffers[0]->main_driver]->write_blocks == NULL)
    {
        return -1;
    }
    return block_drivers[block_buffers[0]->main_driver]->write_blocks(block_buffers, count, timeout);
}

int block_driver_get_driver_info(uint16_t main_driver, uint16_t sub_driver, struct block_driver_info_t* driver_info)
{
    if(block_drivers[main_driver] == NULL)
    {
        return -1;
    }
    if(block_drivers[main_driver]->get_driver_info == NULL)
    {
        return -1;
    }
    return block_drivers[main_driver]->get_driver_info(sub_driver, driver_info);
}

size_t block_driver_get_max_read(uint16_t main_driver)
{
    if(block_drivers[main_driver] == NULL)
        return 0;
    return block_drivers[main_driver]->max_read;
}

size_t block_driver_get_max_write(uint16_t main_driver)
{
    if(block_drivers[main_driver] == NULL)
        return 0;
    return block_drivers[main_driver]->max_write;
}

