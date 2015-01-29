/*-------------------------------------------------------------------------------------------------------------------------------------------
 * remote-ir.h - declaration of remote IR routines using IRMP
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef REMOTE_IR_H
#define REMOTE_IR_H

extern uint_fast8_t remote_ir_get_cmd (void);
extern uint_fast8_t remote_ir_learn (void);
extern uint_fast8_t remote_ir_read_codes_from_eeprom (void);
extern uint_fast8_t remote_ir_write_codes_to_eeprom (void);

#endif
