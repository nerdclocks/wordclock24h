/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * http.h - simple HTTP server via ESP8266
 *
 * Copyright (c) 2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef HTTP_H
#define HTTP_H

#include "listener.h"

extern uint16_t     htoi (char *, uint8_t);
extern uint_fast8_t http_server (char *, char *, LISTENER_DATA * ld);

#endif
