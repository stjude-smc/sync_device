/**
 * @file triggers.h
 * @author Roman Kiselev (roman.kiselev@stjude.org)
 * @brief Functions to read/write IO ports and generate trigger signals
 * @version 0.3
 * @date 2023-01-10
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once

#include "sys_globals.h"

void init_IO();

void set_lasers(uint8_t laser);
void lasers_off();
uint8_t next_laser();

void reset_lasers();
uint8_t get_first_laser();