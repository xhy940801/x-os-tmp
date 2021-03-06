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

int block_driver_read_blocks(struct block_buffer_desc_t* start, size_t count, int timeout)
{
    if(block_drivers[start->main_driver] == NULL)
    {
        return -1;
    }
    if(block_drivers[start->main_driver]->read_blocks == NULL)
    {
        return -1;
    }
    return block_drivers[start->main_driver]->read_blocks(start, count, timeout);
}

int block_driver_write_blocks(const struct block_buffer_desc_t* start, size_t count, int timeout)
{
    if(block_drivers[start->main_driver] == NULL)
    {
        return -1;
    }
    if(block_drivers[start->main_driver]->write_blocks == NULL)
    {
        return -1;
    }
    return block_drivers[start->main_driver]->write_blocks(start, count, timeout);
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

