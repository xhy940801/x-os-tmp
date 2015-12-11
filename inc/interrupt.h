#pragma once

#include "stddef.h"
#include "stdint.h"

void setup_intr_desc(size_t id, void* callback, uint16_t dpl);

void setup_trap_desc(size_t id, void* callback, uint16_t dpl);

void set_8259_mask(uint8_t port);

void clear_8259_mask(uint8_t port);
