/*
 * include/asm-mips/vr41xx.h
 *
 * Primary header for NEC VR41xx processors.
 *
 * Copyright (C) 1999 Michael Klar
 * Copyright (C) 2001 Paul Mundt <lethal@chaoticdreams.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#ifndef __ASM_MIPS_VR41XX_H
#define __ASM_MIPS_VR41XX_H

#include <linux/config.h>

/*
 * Any code being written for a VR41xx device should be including this
 * header.. especially if it's a driver. Directly including a header for a
 * specific member of the family should be avoided, as should any
 * CONFIG_VR41XX checks in drivers when its something as simple as the
 * location of a certain register not being consistent across all members of
 * the family (such as in the case of the DSU).
 *
 * The headers that are included by this header are to have the naming
 * convention of VR41XX_MACRO at all times, and should _never_ under _any_
 * circumstances be referencing VR4181_MACRO (in the case of the vr4181). Any
 * member specific macros need to go somewhere else.
 */
#if defined(CONFIG_VR4111)
  #include <asm/vr4111/vr4111.h>
#elif defined(CONFIG_VR4121)
  #include <asm/vr4121/vr4121.h>
#elif defined(CONFIG_VR4122)
  #include <asm/vr4122/vr4122.h>
#elif defined(CONFIG_VR4181)
  #include <asm/vr4181/vr4181.h>
#endif

#endif /* __ASM_MIPS_VR41XX_H */

