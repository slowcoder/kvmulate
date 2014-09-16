#pragma once

#include "system.h"

int irq_init(sys_t *pInCtx);
int irq_set(int irq,int level);

