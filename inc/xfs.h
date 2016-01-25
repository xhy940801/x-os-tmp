#pragma once

#include "stdint.h"

struct xfs_super_block_desc_t
{
    uint32_t flags;
    uint32_t block_num;
    uint32_t block_size;
    int32_t inode_head;
    int32_t free_bitmap;
};

struct xfs_hd_inode_clause_desc_t
{
    uint32_t inode_count;
    int32_t start_offset;
    uint32_t size;
};

struct xfs_hd_inode_desc_t
{
    uint32_t flags;
    int16_t sub_driver_offset;
    int16_t main_driver_offset;
    int32_t pos_offset;
    int32_t owner_id;
    int32_t group_id;
    uint32_t size;
    struct xfs_hd_inode_clause_desc_t[0];
};
