#pragma once

#include "stddef.h"
#include "stdint.h"

struct block_buffer_desc_t;

struct block_driver_desc_t
{
    int (*read_blocks) (struct block_buffer_desc_t* start, size_t count);
    int (*write_blocks) (const struct block_buffer_desc_t* start, size_t count);
};

void block_driver_module_init();
int block_driver_register(struct block_driver_desc_t* driver, size_t num);
int block_driver_read_blocks(struct block_buffer_desc_t* start, size_t count);
int block_driver_write_blocks(const struct block_buffer_desc_t* start, size_t count);
size_t block_driver_get_max_write(uint16_t main_driver);
