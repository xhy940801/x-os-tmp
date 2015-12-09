#pragma once

#include "stddef.h"
#include "stdint.h"

void setup_intr_desc(size_t id, void* callback, uint16_t dpl);

void setup_trap_desc(size_t id, void* callback, uint16_t dpl);
