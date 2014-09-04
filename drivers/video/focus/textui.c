//	textui.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements a text-based user interface.

#include <stdio.h>
#include <string.h>

#include "FS460.h"
#include "ver_460.h"
#include "regs.h"


// ==========================================================================
//
// This function returns a description of a TV standard index.

static char *str_TVStandard(unsigned long dwTVStandard)
{
	switch (dwTVStandard)
	{
		case FS460_TV_STANDARD_NTSC_M:
		return "NTSC";

		case FS460_TV_STANDARD_NTSC_M_J:
		return "NTSC_EIAJ";

		case FS460_TV_STANDARD_PAL_B:
		return "PAL_B";

		case FS460_TV_STANDARD_PAL_D:
		return "PAL_D";

		case FS460_TV_STANDARD_PAL_G:
		return "PAL_G";

		case FS460_TV_STANDARD_PAL_H:
		return "PAL_H";

		case FS460_TV_STANDARD_PAL_I:
		return "PAL_I";

		case FS460_TV_STANDARD_PAL_M:
		return "PAL_M";

		case FS460_TV_STANDARD_PAL_N:
		return "PAL_N";

		default:
		return "???";
	}
}

// ==========================================================================
//
// This function returns a description of a VGA mode index.

static char *str_VgaMode(unsigned long dwVgaMode)
{
	switch (dwVgaMode)
	{
		case FS460_VGA_MODE_640X480:
		return "640x480";

		case FS460_VGA_MODE_720X487:
		return "720x487";

		case FS460_VGA_MODE_720X576:
		return "720x576";

		case FS460_VGA_MODE_800X600:
		return "800x600";

		case FS460_VGA_MODE_1024X768:
		return "1024x768";

		default:
		return "???";
	}
}


// ==========================================================================
//
// This function reads and displays a register set, or a single register.

static void dump_registers(char *cmd)
{
	S_FS460_REG_INFO r;
	const S_SET_DESCRIP *set;
	const S_REGISTER_DESCRIP *reg;
	int err, lcnt;

	set = 0;
	if (strcmp(cmd,"g") == 0)
		set = regs_gcc();
	else if (strcmp(cmd,"ss") == 0)
		set = regs_scaler();
	else if (strcmp(cmd,"sv") == 0)
		set = regs_vgatv();
	else if (strcmp(cmd,"se") == 0)
		set = regs_encoder();

#ifdef FS460_MACROVISION
	else if (strcmp(cmd,"sm") == 0)
		set = regs_macrovision();
#endif

	else if (strcmp(cmd,"vc") == 0)
		set = regs_vp_config();
	else if (strcmp(cmd,"vl") == 0)
		set = regs_vp_layer();
	else if (strcmp(cmd,"vm") == 0)
		set = regs_vp_move();
	else if (strcmp(cmd,"l") == 0)
		set = regs_lpc();

	if (set)
	{
		r.source = set->source;
		reg = set->registers;
		lcnt = 0;
		while ((reg->name) && ((reg - set->registers) < MAX_REGISTERS))
		{
			if (reg->bit_length)
			{
				r.size = reg->bit_length / 8;
				r.offset = reg->offset;
				r.value = 0;
				
				err = FS460_read_register(&r);
				if (err)
					printf("  %-12.12s(0x%02lx) = <err %08x>", reg->name, r.offset, err);
				else if (reg->bit_length == 32)
					printf("  %-12.12s(0x%02lx) = %08lx     ", reg->name, r.offset, r.value);
				else if (reg->bit_length == 16)
					printf("  %-12.12s(0x%02lx) = %04lx (%5ld) ", reg->name, r.offset, r.value, r.value);
				else
					printf("  %-12.12s(0x%02lx) = %02lx   (%5ld) ", reg->name, r.offset, r.value, r.value);
				if ((++lcnt % 2 == 0))
					printf("\n");
			}

			reg++;
		}
		if ((lcnt % 2) != 0)
			printf("\n");

		return;
	}

	if (cmd[1] == ' ')
	{
		r.source = -1;

		if (cmd[0] == 'g')
			r.source = FS460_SOURCE_GCC;
		else if (cmd[0] == 's')
			r.source = FS460_SOURCE_SIO;
		else if (cmd[0] == 'v')
			r.source = FS460_SOURCE_BLENDER;
		else if (cmd[0] == 'l')
			r.source = FS460_SOURCE_LPC;

		if (-1 != r.source)
		{
			if (2 == sscanf(cmd + 2,"%lx %u",&r.offset,&r.size))
			{
				err = FS460_read_register(&r);
				if (err)
					printf("error %u reading register.\n",err);
				else
					printf("offset=0x%04lx, value=0x%04lx, size=%u\n",r.offset,r.value,r.size);

				return;
			}
		}
	}

	printf("bad command.\n");
}

// ==========================================================================
//
// This function writes a register value.

static void write_register(char *cmd)
{
	int err;
	S_FS460_REG_INFO r;

	r.source = -1;

	if (cmd[1] == ' ')
	{
		if (cmd[0] == 'g')
			r.source = FS460_SOURCE_GCC;
		else if (cmd[0] == 's')
			r.source = FS460_SOURCE_SIO;
		else if (cmd[0] == 'v')
			r.source = FS460_SOURCE_BLENDER;
		else if (cmd[0] == 'l')
			r.source = FS460_SOURCE_LPC;

		if (-1 != r.source)
		{		
			if (3 == sscanf(cmd + 2,"%lx %lx %u",&r.offset,&r.value,&r.size))
			{
				err = FS460_write_register(&r);
				if (err)
					printf("error %u writing register.\n",err);
				else
					printf("offset=0x%04lx, value=0x%04lx, size=%u\n",r.offset,r.value,r.size);

				return;
			}
		}
	}

	printf("bad command.\n");
}

// ==========================================================================
//
// This function reads and displays device settings.

static void get_settings(void)
{
	int err;
	unsigned long mode;
	int setting;
	int left, top, width, height;
	unsigned int yc_filter;
#ifdef FS460_MACROVISION
	unsigned int trigger_bits;
#endif
	
	printf("Videoparameters:\n");

	// output and setup.
	printf("  output:\n");

	err = FS460_get_tv_standard(&mode);
	if (err)
		printf("    std=<err %u>, ",err);
	else
		printf("    std=%s, ",str_TVStandard(mode));
	err = FS460_get_available_tv_standards(&mode);
	if (err)
		printf("avail stds=<err %u>\n",err);
	else
		printf("avail stds=0x%08lx\n",mode);

	err = FS460_get_vga_mode(&mode);
	if (err)
		printf("    vgamode=<err %u>, ",err);
	else
		printf("    vgamode=%s, ",str_VgaMode(mode));
	err = FS460_get_available_vga_modes(&mode);
	if (err)
		printf("avail vgamodes=<err %u>\n",err);
	else
		printf("avail vgamodes=0x%04lx\n",mode);
	
	err = FS460_get_tvout_mode(&mode);
	if (err)
		printf("    tvout mode=<err %u>\n",err);
	else
	{
		switch (mode)
		{
			case FS460_TVOUT_MODE_CVBS_YC:
				printf("    tvout mode=CVBS + Y/C\n");
			break;
			case FS460_TVOUT_MODE_RGB:
				printf("    tvout mode=RGB\n");
			break;
			default:
				printf("    tvout mode=0x%04lx\n",mode);
			break;
		}
	}

	// scaling position.
	printf("  VGA position:\n");
	err = FS460_get_vga_position(&left, &top, &width, &height);
	if (err)
		printf("  VGA position=<err %u>\n",err);
	else
		printf("  VGA position=(%d,%d,%d,%d)\n", left, top, width, height);

	// filters.
	printf("  filters:\n");
	err = FS460_get_sharpness(&setting);
	if (err)
		printf("    sharpness=<err %u> ",err);
	else
		printf("    sharpness=%d ",setting);
	err = FS460_get_flicker_filter(&setting);
	if (err)
		printf("flicker=<err %u>\n",err);
	else
		printf("flicker=%d\n", setting);

	err = FS460_get_yc_filter(&yc_filter);
	if (err)
		printf("    yc_filter <err %u>\n",err);
	else
	{
		if (FS460_LUMA_FILTER & yc_filter)
			printf("    notch=on ");
		else
			printf("    notch=off");
		if (FS460_CHROMA_FILTER & yc_filter)
			printf(" chroma=on\n");
		else
			printf(" chroma=off\n");
	}

	// encoder.
	printf("  encoder:\n");
	err = FS460_get_color(&setting);
	if (err)
		printf("    color=<err %u>, ",err);
	else
		printf("    color=%d, ",setting);
	err = FS460_get_brightness(&setting);
	if (err)
		printf("brightness=<err %u>, ",err);
	else
		printf("brightness=%d, ",setting);
	err = FS460_get_contrast(&setting);
	if (err)
		printf("contrast=<err %u>\n",err);
	else
		printf("contrast=%d\n",setting);

#ifdef FS460_MACROVISION
	printf("  copy protect:\n");
	err = FS460_get_aps_trigger_bits(&trigger_bits);
	if (err)
		printf("    trigger_bits=<err %u>, ",err);
	else
	{
		printf("    trigger_bits=");
		if (trigger_bits & 2)
			printf("1");
		else
			printf("0");
		if (trigger_bits & 1)
			printf("1\n");
		else
			printf("0\n");
	}
#endif
}

// ==========================================================================
//
// This function resets device defaults.

static void reset_defaults(void)
{
	int err;
	int lines;

	FS460_get_tv_active_lines(&lines);
	err = FS460_set_vga_position(20,20,680,lines-40);
	
	err |= FS460_set_flicker_filter(1000);
	err |= FS460_set_sharpness(200);
	err |= FS460_set_color(50);
	err |= FS460_set_brightness(50);
	err |= FS460_set_contrast(60);

	if (err)
		printf("Error writing defaults.\n");
}


// ==========================================================================
//
// This function increments or decrements a standard register.

#define CMD_SHARPNESS 0
#define CMD_FLICKER 1
#define CMD_BRIGHTNESS 2
#define CMD_CONTRAST 3
#define CMD_COLOR 4

typedef int (*GET_FUNC)(int *p_setting);
typedef int (*SET_FUNC)(int setting);

struct
{
	char *name;
	GET_FUNC get_func;
	SET_FUNC set_func;
	int step_size;
} inc_dec_cmd_def[] =
{
	{ "sharpness", FS460_get_sharpness, FS460_set_sharpness, 100},
	{ "flicker", FS460_get_flicker_filter, FS460_set_flicker_filter, 100},
	{ "brightness", FS460_get_brightness, FS460_set_brightness, 10},
	{ "contrast", FS460_get_contrast, FS460_set_contrast, 10},
	{ "color", FS460_get_color, FS460_set_color, 10}
};

static void inc_dec_cmd(int cmd, char *mod)
{
	int err;
	int setting;
	
	if (0 == inc_dec_cmd_def[cmd].get_func(&setting))
	{
		if (0 == strcmp(mod,"-"))
			setting -= inc_dec_cmd_def[cmd].step_size;
		else if (0 == strcmp(mod,"+"))
			setting += inc_dec_cmd_def[cmd].step_size;
		else if (0 == strcmp(mod,"--"))
			setting = -10000;
		else if (0 == strcmp(mod,"++"))
			setting = 10000;

		err = inc_dec_cmd_def[cmd].set_func(setting);
		if (err)
			printf("Error setting %s.\n",inc_dec_cmd_def[cmd].name);
		else
		{
			err = inc_dec_cmd_def[cmd].get_func(&setting);
			if (err)
				printf("Error getting %s.\n",inc_dec_cmd_def[cmd].name);
			else
				printf("%s=%d\n",inc_dec_cmd_def[cmd].name,setting);
		}
	}
}

// ==========================================================================
//
// This function adjusts a position/size register.

#define CMD_POS_LEFT 0
#define CMD_POS_TOP 1
#define CMD_POS_WIDTH 2
#define CMD_POS_HEIGHT 3

static void xy_cmd(int cmd, char *mod)
{
	int err;
	int left, top, width, height, *p;
	
	switch(cmd)
	{
		case CMD_POS_LEFT:
			printf("vga left");
			p = &left;
		break;

		case CMD_POS_TOP:
			printf("vga top");
			p = &top;
		break;

		case CMD_POS_WIDTH:
			printf("vga width");
			p = &width;
		break;

		case CMD_POS_HEIGHT:
			printf("vga height");
			p = &height;
		break;
		
		default:
		return;
	}
	
	err = FS460_get_vga_position(&left, &top, &width, &height);
	if (err)
	{
		printf(": error %u getting value.\n",err);
		return;
	}

	if (0 == strcmp(mod,"+"))
		*p += 1;
	else if (0 == strcmp(mod,"-"))
		*p -= 1;
	else if (0 == strcmp(mod,"0"))
		*p = 0;
	else if ('\0' != mod[0])
	{
			printf(": invalid switch \"%s\".\n",mod);
			return;
	}

	err = FS460_set_vga_position(left, top, width, height);

	if (err)
		printf(": error %u writing value.\n",err);
	else
	{
		err = FS460_get_vga_position(&left, &top, &width, &height);
	}
		
	if (err)
		printf(": error %u reading value after write.\n",err);
	else
		printf(" = %d\n",*p);
}

// ==========================================================================
//
// This function increments or decrements the FFO_LAT register.

static void inc_dec_ffolat(int dir)
{
	S_FS460_REG_INFO r;
	int err;

	r.source = FS460_SOURCE_SIO;
	r.size = 2;
	r.offset = SIO_FFO_LAT;
	err = FS460_read_register(&r);
	if (err)
	{
		printf("Failed, error %u.\n",err);
		return;
	}

	r.value += dir;

	err = FS460_write_register(&r);
	if (err)
	{
		printf("Failed, error %u.\n",err);
		return;
	}
	printf("  ffolat=%04lx\n", r.value);
}



// ==========================================================================
//
// This function rotates the TV standard to the next available standard.

static void tv_standard(char *mod)
{
	int err;
	unsigned long tv_standard,available_tv_standards;

	// get the current and available TV standards
	err = FS460_get_tv_standard(&tv_standard);
	if (err)
	{
		printf("error %u getting tv standard.\n",err);
		return;
	}
	err = FS460_get_available_tv_standards(&available_tv_standards);
	if (err)
	{
		printf("error %u getting available tv standards.\n",err);
		return;
	}

	// if there are any available standards...
	if (available_tv_standards)
	{
		if (0 == strcmp(mod,"+"))
		{
			// increment the current standard to the next available standard
			do
			{
				tv_standard <<= 1;
				if (!tv_standard)
					tv_standard = 1;
			}
			while (!(tv_standard & available_tv_standards));
		}
		else if (0 == strcmp(mod,"-"))
		{
			// decrement the current standard to the previous available standard
			do
			{
				tv_standard >>= 1;
				if (!tv_standard)
					tv_standard = 0x80000000;
			}
			while (!(tv_standard & available_tv_standards));
		}
		else if (0 != strcmp(mod,""))
		{
			printf("Invalid command.\n");
			return;
		}
			
		// set it
		err = FS460_set_tv_standard(tv_standard);
		if (err)
			printf("error %u setting TV standard.\n",err);
		else
		{
			// get it again and display it's name
			err = FS460_get_tv_standard(&tv_standard);
			if (err)
				printf("error %u getting TV standard after set.\n",err);
			else
				printf("  TV Standard=%s\n",str_TVStandard(tv_standard));
		}
	}
}


// ==========================================================================
//
// This function rotates the VGA mode to the next available mode.

static void vga_mode(char *mod)
{
	int err;
	unsigned long vga_mode,available_vga_modes;

	// get the current and available VGA modes
	err = FS460_get_vga_mode(&vga_mode);
	if (err)
	{
		printf("error %u getting vga mode.\n",err);
		return;
	}
	err = FS460_get_available_vga_modes(&available_vga_modes);
	if (err)
	{
		printf("error %u getting available vga modes.\n",err);
		return;
	}

	// if there are any available modes...
	if (available_vga_modes)
	{
		if (0 == strcmp(mod,"+"))
		{
			// increment the current mode to the next available mode
			do
			{
				vga_mode <<= 1;
				if (!vga_mode)
					vga_mode = 1;
			}
			while (!(vga_mode & available_vga_modes));
		}
		else if (0 == strcmp(mod,"-"))
		{
			// decrement the current mode to the previous available mode
			do
			{
				vga_mode >>= 1;
				if (!vga_mode)
					vga_mode = 0x80000000;
			}
			while (!(vga_mode & available_vga_modes));
		}
		else if (0 == strcmp(mod,""))
		{
		}
		else
		{
			printf("Invalid command.\n");
			return;
		}

		// set it                                            
		err = FS460_set_vga_mode(vga_mode);
		if (err)
			printf("error %u setting vga mode.\n",err);
		else
		{
			// get it again and display it's name
			err = FS460_get_vga_mode(&vga_mode);
			if (err)
				printf("error %u getting vga mode after set.\n",err);
			else
				printf("  vga mode=%s\n",str_VgaMode(vga_mode));
		}
	}
}

#ifdef FS460_MACROVISION
void set_cp_protect(char *cmd)
{
	unsigned int trigger_bits;
	int errc;

	if (strlen(cmd) != 5)
	{
		printf("invalid command\n");
		return;
	}

	trigger_bits = 0;
	if (cmd[3] == '1')
		trigger_bits += 2;
	if (cmd[4] == '1')
		trigger_bits += 1;

	errc = FS460_set_aps_trigger_bits(trigger_bits);
	if (errc != 0)
		printf("FS460_set_aps_trigger_bits() returned %d\n", errc);
}
#endif


// ==========================================================================
//
// This function enables TV out.

static void enable_tvout(unsigned int on)
{
	int err;
	
	err = FS460_set_tv_on(on);
	if (err)
		printf("Failed, error %u.\n",err);
}


// ==========================================================================
//
// This function prints the selected user help screen.

static void help(int level)
{
	switch(level)
	{
		case 0:
		{
			printf("Standard commands:\n");
			printf("  a                  = again (repeat last command)\n");
			printf("  m-,m,m+            = vga mode\n");
			printf("  s-,s,s+            = tv standard\n");
			printf("  e                  = enable tvout\n");
			printf("  d                  = disable tvout\n");
			printf("  b,b--,b-,b+,b++    = brightness\n");
			printf("  c,c--,c-,c+,c++    = contrast\n");
			printf("  o,o--,o-,o+,o++    = color\n");
			printf("  f,f--,f-,f+,f++    = flicker\n");
			printf("  n,n--,n-,n+,n++    = sharpness\n");
			printf("  l,l-,l0,l+         = left position\n");
			printf("  t,t-,t0,t+         = top position\n");
			printf("  w,w-,w0,w+         = width\n");
			printf("  h,h-,h0,h+         = height\n");
			printf("  g                  = get video parameters\n");
			printf("  def                = default settings\n");
			printf("  q                  = quit\n");
			printf("  ?                  = standard commands (display this message)\n");
			printf("  ??                 = advanced commands\n");
		}
		break;

		case 1:
		{
			printf("Advanced commands:\n");
#ifdef FS460_MACROVISION
			printf("  cp [00|01|10|11]        = set aps trigger bits\n");
#endif
			printf("  rg                      = register dump: graphics controller\n");
			printf("  rg [reg] [size]         = read Graphics controller register\n");
			printf("  Rg [reg] [value] [size] = write Graphics controller register\n");
			printf("  rs                      = register dump: SIO\n");
			printf("  rs [reg] [size]         = read SIO register\n");
			printf("  Rs [reg] [value] [size] = write SIO register\n");
#ifdef FS460_MACROVISION
			printf("  rsv, rss, rse, rsm      = register dump: VGA-TV, scaler, encoder, macrovision\n");
#else
			printf("  rsv, rss, rse           = register dump: VGA-TV, scaler, encoder\n");
#endif
			printf("  rvc,rvl,rvm             = register dump: VP config, VP layer, VP move\n");
			printf("  rv [reg] [size]         = read VP register\n");
			printf("  Rv [reg] [value] [size] = write VP register\n");
			printf("  rl                      = register dump: LPC\n");
			printf("  rl [reg] [size]         = read LPC register\n");
			printf("  Rl [reg] [value] [size] = write LPC register\n");
			printf("  <,>                     = dec, inc FFO_LAT\n");
			printf("  ?                       = standard commands\n");
			printf("  ??                      = advanced commands (display this message)\n");
		}
		break;
	}
}

// ==========================================================================
//
// This function executes a user command.

static void execute_cmd(char *in)
{
	if (*in == 'b') inc_dec_cmd(CMD_BRIGHTNESS, in + 1);
#ifdef FS460_MACROVISION
	else if (strncmp(in, "cp", 2) == 0) set_cp_protect(in);
#endif
	else if (*in == 'c') inc_dec_cmd(CMD_CONTRAST, in + 1);
	else if (strcmp(in, "d") == 0) enable_tvout(0);
	else if (strcmp(in, "def") == 0) reset_defaults();
	else if (strcmp(in, "e") == 0) enable_tvout(1);
	else if (*in == 'f') inc_dec_cmd(CMD_FLICKER, in + 1);
	else if (strcmp(in, "g") == 0) get_settings();
	else if (*in == 'h') xy_cmd(CMD_POS_HEIGHT, in + 1);
	else if (*in == 'l') xy_cmd(CMD_POS_LEFT, in + 1);
	else if (*in == 'm') vga_mode(in + 1);
	else if (*in == 'o') inc_dec_cmd(CMD_COLOR, in + 1);
	else if (*in == 'r') dump_registers(in + 1);
	else if (*in == 'R') write_register(in + 1);
	else if (*in == 'n') inc_dec_cmd(CMD_SHARPNESS, in + 1);
	else if (*in == 's') tv_standard(in + 1);
	else if (*in == 't') xy_cmd(CMD_POS_TOP, in + 1);
	else if (*in == 'w') xy_cmd(CMD_POS_WIDTH, in + 1);
	else if (strcmp(in, "?") == 0) help(0);
	else if (strcmp(in, "??") == 0) help(1);
	else if (strcmp(in, ">") == 0) inc_dec_ffolat(1);
	else if (strcmp(in, "<") == 0) inc_dec_ffolat(-1);
	else
		printf("unknown command %s\n", in);
}

// ==========================================================================
//
// This function checks the program arguments for a command to execute on
// startup.

static void get_args(char *command, int maxlen, int argc, char **argv)
{
	while (--argc > 0)
	{
		++argv;
		if (strcmp("-e", *argv) == 0)
		{
			if (--argc > 0)
			{
				++argv;
				strncpy(command, *argv, maxlen);
			}
		}
	}
}


// ==========================================================================
//
// This function gets a one-line command from the user.

static int getline(char *line, int maxlen)
{
	int l = 0;
	char c;

	printf("cmd> ");

	c = getchar();
    while ((c != '\n') && (c != EOF))
	{
		*line++ = c;
		//could not get stdin not to linebuffer.
		//if (c == '!' || ++l == maxlen)	// escape
		if (l == maxlen)
			break;
		c = getchar();
	}
	*line = (char) NULL;
	return strlen(line);
}


// ==========================================================================
//
// This function implements the text-based user interface.
//
// argc, argv: the count and array or parameters passed to the text-mode
// program.

#define INLEN 60

void textui(int argc, char **argv)
{
	char in[INLEN];
	char last_in[INLEN];

	// print program info
	printf("FS460 Test Application\n");
	printf("Copyright 1999-2001 Focus Enhancements Inc.\n");
	printf("version " VERSION_MAJOR_STR "." VERSION_MINOR_STR "." VERSION_BUILD_STR "\n\n");
	
	last_in[0] = '\0';

#if (0)
	// vga setup.
	set_vga_mode(FS460_VGA_MODE_640X480);

	// tv standard setup.
	set_tv_standard(FS460_TV_STANDARD_NTSC_M);

	// notch filter off.
	set_yc_filter(0);
#endif

	in[0] = 0;
	get_args(in, sizeof(in), argc, argv);
	if (in[0])
		execute_cmd(in);
	else
	{
		printf("Type '?' for a list of commands\n\n");

		getline(in, sizeof(in) - 1);
		while (strcmp(in, "q") != 0)
		{
			if (strlen(in) > 0)
			{
				if (strcmp(in, "a") == 0)
				{
					strcpy(in, last_in);
					last_in[0] = (char) NULL;
				}
			}
			execute_cmd(in);
			strcpy(last_in, in);
			
			getline(in, INLEN-1);
		}
	}
}
