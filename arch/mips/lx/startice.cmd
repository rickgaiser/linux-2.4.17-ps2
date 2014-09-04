+q  /* put mon in quiet mode, don't display commands read in */
/*----------------------------------------------------------------------*/
/*                                                                      */
/* startice.cmd:  MAJIC startup command file for Lexra LX4180           */
/* ===================================================================  */
/*                                                                      */
/* This file will be searched for automatically when MONICE starts      */
/* running.  It is a handy place to do standard startup commands        */
/* like defining aliases to run useful utility command files.  The      */
/* source level debuggers read cdb.rc instead, but it then reads this   */
/* file.                                                                */
/*                                                                      */
/* As distributed by EPI, this file configures the MAJIC emulator and   */
/* debugger as appropriate for the reference platform and demonstration */
/* software.  It also serves as a template for creating a custom        */
/* ustrtice.cmd file for your target system and software environment.   */
/*                                                                      */
/* It is intended that this file, possibly modified to add more     	*/
/* commands of your choosing, and any command and trace spec files  	*/
/* it references, be copied to the .../bin directory so it will be	*/
/* loaded no matter what working directory the debugger is run from.	*/
/*                                                                      */
/* User specific startup commands should be stored in a file called     */
/* ustrtice.cmd in the working directory (typically, the directory in   */
/* which the executable program exists).                                */
/*                                                                      */
/* Note: This file will be read from the source level debugger (in MON  */
/*       mode) as well as MONICE, so only those commands supported by   */
/*       both should be used here.                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/
//
//........................................................................
:STANDARD_SECTION
// standard command sequence aliases
//
ea dr fr c dr		/* Display Registers */
ea lq l -no tldbs	/* Load just the initialized registers */
ea ld l -o db		/* Load just the data sections */
ea la l -o tldbs	/* Load all sections including symbols */
//
// aliases for displaying the trace control settings (DTS), to enable
// display of the EXCEPT and TPOINT "signals" in formated trace display
// (ETF_*), and kill the trace control settings (KTS).
//
ea dts  do ice_trig*; do trace*
ea etf_exc etf except m=f e=y ib=location bb=1 ba=1
ea etf_tp  etf tpoint m=f e=y ib=location bb=1 ba=1
ea kts eo tt=auto; eo tta=stop; eo tg=none; eo trt=on
//
// aliases for setting hardware breakpoints in MONICE
//
ea bcim fr c ej_bcim     /* Break Clear (Inst Match) */
ea bsim fr c ej_bsim     /* Break Set   (Inst Match) */
//
// goto REFERENCE_PLATFORM
//
//........................................................................
:CUSTOMIZATION_SECTION
//
// Edit the contents of this section to describe your hardware and
// software environment, as described in the MAJIC User Manual.
//
eo ice_jtag_clock_freq = 20      // JTAG clock rate
eo ice_jtag_use_trst   = On      // Use TRST* to reset JTAG controller
eo ice_power_sense     = RST     // Sense target voltage level on RST* pin
eo load_osboot         = off     // auto-load osboot.sys along with program
eo freestanding_mode   = on      // freestanding_mode inhibits EPI-OS support
eo reset_address       = bfc00000
// eo ice_reset_output = on		/* Assert RST* signal when resetting */
// ea RT rt; fr c setupmem.cmd		/* Alias for target reset, reinit */
//
mc *:P, inv			/* start off with everything invalid */
mc 0:P FFFFFF:P, dma, pwe		/* Adjust for your RAM area */
// mc 100000:P 1FbFFFF:P, inv
mc 1FC00000:P 1FC7FFFF:P, dma, pwd	/* Adjust for your ROM area */
// mc range, atrb, atrb			/* Add more for other areas */
mc FF300000:P FF3FFFFF:P, jam, pwd      /* required by MAJIC */
mc *:P, jam                  /* force jam */
//
goto FINAL_SECTION
//
//........................................................................
:REFERENCE_PLATFORM
//
// This section configures a MAJIC and debugger for the LXB4080 evaluation
// board with LX4180 PLD daughter card, and EPI sample software.  It can
// be removed from the copy of startice.cmd in your working directory,
// once the "goto REFERENCE_PLATFORM" at the top of this file has also
// been removed.
//
eo ice_jtag_clock_freq = 20
eo ice_jtag_use_trst   = On
eo ice_power_sense     = RST
eo reset_address       = bfc00000
eo load_osboot         = off
eo freestanding_mode   = on
//
mc 0:P FFFFFF:P, dma, pwe
mc 1FC00000:P 1FC7FFFF:P, dma, pwd
mc FF300000:P FF3FFFFF:P, jam, pwd
//
goto FINAL_SECTION
//
//........................................................................
:FINAL_SECTION
//
// Finally read in the user's custom startup command file.  If
// there isn't one, we'll get the dummy version that contains no
// commands.
//
fr c ustrtice.cmd
//
dv "The emulator is ready for use\n"; -q        // <eof>

