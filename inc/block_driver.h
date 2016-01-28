#pragma once

#include "stddef.h"
#include "stdint.h"

struct block_buffer_desc_t;

struct block_driver_info_t
{
	size_t driver_block_size;
	size_t driver_block_count;
};

struct block_driver_desc_t
{
    int (*read_blocks) (struct block_buffer_desc_t* block_buffers[], size_t count, int timeout);
    int (*write_blocks) (struct block_buffer_desc_t* block_buffers[], size_t count, int timeout);
    int (*get_driver_info) (uint16_t sub_driver, struct block_driver_info_t* driver_info);
    size_t max_read;
    size_t max_write;
};

void block_driver_module_init();
int block_driver_register(struct block_driver_desc_t* driver, uint16_t main_driver);
ssize_t block_driver_read_blocks(struct block_buffer_desc_t* block_buffers[], size_t count, int timeout);
ssize_t block_driver_write_blocks(struct block_buffer_desc_t* block_buffers[], size_t count, int timeout);
int block_driver_get_driver_info(uint16_t main_driver, uint16_t sub_driver, struct block_driver_info_t* driver_info);
size_t block_driver_get_max_read(uint16_t main_driver);
size_t block_driver_get_max_write(uint16_t main_driver);
