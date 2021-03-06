/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: menus.c,v 1.89.2.81 1998/11/24 00:20:45 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"

/* Miscellaneous work routines for menus */
static int
setSrc(dialogMenuItem *self)
{
    Dists |= DIST_SRC;
    SrcDists = DIST_SRC_ALL | DIST_SRC_SMAILCF;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearSrc(dialogMenuItem *self)
{
    Dists &= ~DIST_SRC;
    SrcDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11All(dialogMenuItem *self)
{
    XF86Dists = DIST_XF86_ALL;
    XF86ServerDists = DIST_XF86_SERVER_ALL;
    XF86FontDists = DIST_XF86_FONTS_ALL;
    Dists |= DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11All(dialogMenuItem *self)
{
    XF86Dists = 0;
    XF86ServerDists = 0;
    XF86FontDists = 0;
    Dists &= ~DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11Misc(dialogMenuItem *self)
{
    XF86Dists |= DIST_XF86_MISC_ALL;
    Dists |= DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11Misc(dialogMenuItem *self)
{
    XF86Dists &= ~DIST_XF86_MISC_ALL;
    if (!XF86ServerDists && !XF86FontDists)
	Dists &= ~DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11Servers(dialogMenuItem *self)
{
    XF86Dists |= DIST_XF86_SERVER;
    XF86ServerDists = DIST_XF86_SERVER_ALL;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11Servers(dialogMenuItem *self)
{
    XF86Dists &= ~DIST_XF86_SERVER;
    XF86ServerDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11Fonts(dialogMenuItem *self)
{
    XF86Dists |= DIST_XF86_FONTS;
    XF86FontDists = DIST_XF86_FONTS_ALL;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11Fonts(dialogMenuItem *self)
{
    XF86Dists &= ~DIST_XF86_FONTS;
    XF86FontDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

#define _IS_SET(dist, set) (((dist) & (set)) == (set))

#define IS_DEVELOPER(dist, extra) (_IS_SET(dist, _DIST_DEVELOPER | extra) || \
	_IS_SET(dist, _DIST_DEVELOPER | DIST_DES | extra))

#define IS_USER(dist, extra) (_IS_SET(dist, _DIST_USER | extra) || \
	_IS_SET(dist, _DIST_USER | DIST_DES | extra))

static int
checkDistDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, 0) && _IS_SET(SrcDists, DIST_SRC_ALL);
}

static int
checkDistXDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, DIST_XF86) && _IS_SET(SrcDists, DIST_SRC_ALL);
}

static int
checkDistKernDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, 0) && _IS_SET(SrcDists, DIST_SRC_SYS);
}

static int
checkDistUser(dialogMenuItem *self)
{
    return IS_USER(Dists, 0);
}

static int
checkDistXUser(dialogMenuItem *self)
{
    return IS_USER(Dists, DIST_XF86);
}

static int
checkDistMinimum(dialogMenuItem *self)
{
    return Dists == DIST_BIN;
}

static int
checkDistEverything(dialogMenuItem *self)
{
    return Dists == DIST_ALL && _IS_SET(SrcDists, DIST_SRC_ALL) && \
	_IS_SET(XF86Dists, DIST_XF86_ALL) && \
	_IS_SET(XF86ServerDists, DIST_XF86_SERVER_ALL) && \
	_IS_SET(XF86FontDists, DIST_XF86_FONTS_ALL);
}

static int
DESFlagCheck(dialogMenuItem *item)
{
    return DESDists;
}

static int
srcFlagCheck(dialogMenuItem *item)
{
    return SrcDists;
}

static int
x11FlagCheck(dialogMenuItem *item)
{
    return Dists & DIST_XF86;
}

static int
checkTrue(dialogMenuItem *item)
{
    return TRUE;
}

/* All the system menus go here.
 *
 * Hardcoded things like version number strings will disappear from
 * these menus just as soon as I add the code for doing inline variable
 * expansion.
 */

DMenu MenuIndex = {
    DMENU_NORMAL_TYPE,
    "Glossary of functions",
    "This menu contains an alphabetized index of the top level functions in\n"
    "this program (sysinstall).  Invoke an option by pressing [ENTER].\n"
    "Leave the index page by selecting Cancel [TAB-ENTER].",
    "Use PageUp or PageDown to move through this menu faster!",
    NULL,
    { { "Anon FTP",		"Configure anonymous FTP logins.",	dmenuVarCheck, configAnonFTP, NULL, "anon_ftp" },
      { "Commit",		"Commit any pending actions (dangerous!)", NULL, installCustomCommit },
      { "Console settings",	"Customize system console behavior.",	NULL, dmenuSubmenu, NULL, &MenuSyscons },
      { "Configure",		"The system configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { "Defaults, Load",	"Load default settings.",		NULL, dispatch_load_floppy },
      { "Device, Mouse",	"The mouse configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuMouse },
      { "Disklabel",		"The disk Label editor",		NULL, diskLabelEditor },
      { "Dists, All",		"Root of the distribution tree.",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { "Dists, Basic",		"Basic FreeBSD distribution menu.",	NULL, dmenuSubmenu, NULL, &MenuSubDistributions },
      { "Dists, DES",		"DES distribution menu.",		NULL, dmenuSubmenu, NULL, &MenuDESDistributions },
      { "Dists, Developer",	"Select developer's distribution.",	checkDistDeveloper, distSetDeveloper },
      { "Dists, Src",		"Src distribution menu.",		NULL, dmenuSubmenu, NULL, &MenuSrcDistributions },
      { "Dists, X Developer",	"Select X developer's distribution.",	checkDistXDeveloper, distSetXDeveloper },
      { "Dists, Kern Developer", "Select kernel developer's distribution.", checkDistKernDeveloper, distSetKernDeveloper },
      { "Dists, User",		"Select average user distribution.",	checkDistUser, distSetUser },
      { "Dists, X User",	"Select average X user distribution.",	checkDistXUser, distSetXUser },
      { "Distributions, Adding", "Installing additional distribution sets", NULL, distExtractAll },
      { "Distributions, XFree86","XFree86 distribution menu.",		NULL, distSetXF86 },
      { "Documentation",	"Installation instructions, README, etc.", NULL, dmenuSubmenu, NULL, &MenuDocumentation },
      { "Doc, README",		"The distribution README file.",	NULL, dmenuDisplayFile, NULL, "README" },
      { "Doc, Hardware",	"The distribution hardware guide.",	NULL, dmenuDisplayFile,	NULL, "HARDWARE" },
      { "Doc, Install",		"The distribution installation guide.",	NULL, dmenuDisplayFile,	NULL, "INSTALL" },
      { "Doc, Copyright",	"The distribution copyright notices.",	NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
      { "Doc, Release",		"The distribution release notes.",	NULL, dmenuDisplayFile, NULL, "RELNOTES" },
      { "Doc, HTML",		"The HTML documentation menu.",		NULL, docBrowser },
      { "Emergency shell",	"Start an Emergency Holographic shell.",	NULL, installFixitHoloShell },
      { "Fdisk",		"The disk Partition Editor",		NULL, diskPartitionEditor },
      { "Fixit",		"Repair mode with CDROM or fixit floppy.",	NULL, dmenuSubmenu, NULL, &MenuFixit },
      { "FTP sites",		"The FTP mirror site listing.",		NULL, dmenuSubmenu, NULL, &MenuMediaFTP },
      { "Gateway",		"Set flag to route packets between interfaces.", dmenuVarCheck, dmenuToggleVariable, NULL, "gateway=YES" },
      { "HTML Docs",		"The HTML documentation menu",		NULL, docBrowser },
      { "Install, Novice",	"A novice system installation.",	NULL, installNovice },
      { "Install, Express",	"An express system installation.",	NULL, installExpress },
      { "Install, Custom",	"The custom installation menu",		NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { "Label",		"The disk Label editor",		NULL, diskLabelEditor },
      { "Media",		"Top level media selection menu.",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { "Media, Tape",		"Select tape installation media.",	NULL, mediaSetTape },
      { "Media, NFS",		"Select NFS installation media.",	NULL, mediaSetNFS },
      { "Media, Floppy",	"Select floppy installation media.",	NULL, mediaSetFloppy },
      { "Media, CDROM",		"Select CDROM installation media.",	NULL, mediaSetCDROM },
      { "Media, DOS",		"Select DOS installation media.",	NULL, mediaSetDOS },
      { "Media, UFS",		"Select UFS installation media.",	NULL, mediaSetUFS },
      { "Media, FTP",		"Select FTP installation media.",	NULL, mediaSetFTP },
      { "Media, FTP Passive",	"Select passive FTP installation media.", NULL, mediaSetFTPPassive },
      { "Network Interfaces",	"Configure network interfaces",		NULL, tcpMenuSelect },
      { "Networking Services",	"The network services menu.",		NULL, dmenuSubmenu, NULL, &MenuNetworking },
      { "NFS, client",		"Set NFS client flag.",			dmenuVarCheck, dmenuToggleVariable, NULL, "nfs_client_enable=YES" },
      { "NFS, server",		"Set NFS server flag.",			dmenuVarCheck, configNFSServer, NULL, "nfs_server_enable=YES" },
      { "NTP Menu",		"The NTP configuration menu.",		NULL, dmenuSubmenu, NULL, &MenuNTP },
      { "Options",		"The options editor.",			NULL, optionsEditor },
      { "Packages",		"The packages collection",		NULL, configPackages },
      { "Partition",		"The disk Slice (PC-style partition) Editor",	NULL, diskPartitionEditor },
      { "PCNFSD",		"Run authentication server for PC-NFS.", dmenuVarCheck, configPCNFSD, NULL, "pcnfsd" },
      { "Register",		"Register yourself or company as a FreeBSD user.", dmenuVarCheck, configRegister, NULL, "registered" },
      { "Root Password",	"Set the system manager's password.",   NULL, dmenuSystemCommand, NULL, "passwd root" },
      { "Root Password",	"Set the system manager's password.",   NULL, dmenuSystemCommand, NULL, "passwd root" },
      { "Router",		"Select routing daemon (default: routed)", NULL, configRouter, NULL, "router" },
      { "Syscons",		"The system console configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSyscons },
      { "Syscons, Font",	"The console screen font.",	  NULL, dmenuSubmenu, NULL, &MenuSysconsFont },
      { "Syscons, Keymap",	"The console keymap configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "Syscons, Keyrate",	"The console key rate configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { "Syscons, Saver",	"The console screen saver configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
      { "Syscons, Screenmap",	"The console screenmap configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsScrnmap },
      { "Time Zone",		"Set the system's time zone.",		NULL, dmenuSystemCommand, NULL, "tzsetup" },
      { "Upgrade",		"Upgrade an existing system.",		NULL, installUpgrade },
      { "Usage",		"Quick start - How to use this menu system.",	NULL, dmenuDisplayFile, NULL, "usage" },
      { "User Management",	"Add user and group information.",	NULL, dmenuSubmenu, NULL, &MenuUsermgmt },
      { "XFree86, Fonts",	"XFree86 Font selection menu.",		NULL, dmenuSubmenu, NULL, &MenuXF86SelectFonts },
      { "XFree86, Server",	"XFree86 Server selection menu.",	NULL, dmenuSubmenu, NULL, &MenuXF86SelectServer },
      { "XFree86, PC98 Server",	"XFree86 PC98 Server selection menu.",	NULL, dmenuSubmenu, NULL, &MenuXF86SelectPC98Server },
      { NULL } },
};

/* The initial installation menu */
DMenu MenuInitial = {
    DMENU_NORMAL_TYPE,
    "/stand/sysinstall Main Menu",			/* title */
    "Welcome to the FreeBSD installation and configuration tool.  Please\n" /* prompt */
    "select one of the options below by using the arrow keys or typing the\n"
    "first character of the option name you're interested in.  Invoke an\n"
    "option by pressing [ENTER] or [TAB-ENTER] to exit the installation.", 
    "Press F1 for Installation Guide",			/* help line */
    "install",						/* help file */
    { { "Select" },
      { "Exit Install",	NULL, NULL, dmenuExit },
      { "1 Usage",	"Quick start - How to use this menu system",	NULL, dmenuDisplayFile, NULL, "usage" },
      { "2 Doc",	"Installation instructions, README, etc.",	NULL, dmenuSubmenu, NULL, &MenuDocumentation },
      { "3 Keymap",	"Select keyboard type",				NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "4 Options",	"View/Set various installation options",	NULL, optionsEditor },
      { "5 Novice",	"Begin a novice installation (for beginners)",	NULL, installNovice },
      { "6 Express",	"Begin a quick installation (for the impatient)", NULL, installExpress },
      { "7 Custom",	"Begin a custom installation (for experts)",	NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { "8 Fixit",	"Enter repair mode with CDROM/floppy or start shell",	NULL, dmenuSubmenu, NULL, &MenuFixit },
      { "9 Upgrade",	"Upgrade an existing system",			NULL, installUpgrade },
      { "c Configure",	"Do post-install configuration of FreeBSD",	NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { "l Load Config","Load default install configuration",		NULL, dispatch_load_floppy },
      { "0 Index",	"Glossary of functions",			NULL, dmenuSubmenu, NULL, &MenuIndex },
      { NULL } },
};

/* The main documentation menu */
DMenu MenuDocumentation = {
    DMENU_NORMAL_TYPE,
    "FreeBSD Documentation Menu",
    "If you are at all unsure about the configuration of your hardware\n"
    "or are looking to build a system specifically for FreeBSD, read the\n"
    "Hardware guide!  New users should also read the Install document for\n"
    "a step-by-step tutorial on installing FreeBSD.  For general information,\n"
    "consult the README file.",
    "Confused?  Press F1 for help.",
    "usage",
    { { "1 README",	"A general description of FreeBSD.  Read this!", NULL, dmenuDisplayFile, NULL, "README" },
      { "2 Hardware",	"The FreeBSD survival guide for PC hardware.",	NULL, dmenuDisplayFile,	NULL, "HARDWARE" },
      { "3 Install",	"A step-by-step guide to installing FreeBSD.",	NULL, dmenuDisplayFile,	NULL, "INSTALL" },
      { "4 Copyright",	"The FreeBSD Copyright notices.",		NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
      { "5 Release"	,"The release notes for this version of FreeBSD.", NULL, dmenuDisplayFile, NULL, "RELNOTES" },
      { "6 Shortcuts",	"Creating shortcuts to sysinstall.",		NULL, dmenuDisplayFile, NULL, "shortcuts" },
      { "7 HTML Docs",	"Go to the HTML documentation menu (post-install).", NULL, docBrowser },
      { "0 Exit",	"Exit this menu (returning to previous)",	NULL, dmenuExit },
      { NULL } },
};

DMenu MenuMouseType = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "Select a protocol type for your mouse",
    "If you are not sure, choose \"Auto\".  It should always work for bus\n"
    "and PS/2 style mice.  It may not work for the serial mouse if the mouse\n"
    "does not support the PnP standard.  But, it won't hurt.  Many 2-button\n"
    "serial mice are compatible with \"Microsoft\" or \"MouseMan\".  3-button\n"
    "serial mice may be compatible with \"MouseSystems\" or \"MouseMan\".  If\n"
    "the mouse has a wheel, it may be compatible with \"IntelliMouse\".",
    NULL,
    NULL,
    { { "Auto",		"Bus mouse, PS/2 style mouse or PnP serial mouse",	
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=auto" },
      { "GlidePoint",	"ALPS GlidePoint pad (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=glidepoint" },
      { "Hitachi","Hitachi tablet (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mmhittab" },
      { "IntelliMouse",	"Microsoft IntelliMouse (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=intellimouse" },
      { "Logitech",	"Logitech protocol (old models) (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=logitech" },
      { "Microsoft",	"Microsoft protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=microsoft" },
      { "MM Series","MM Series protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mmseries" },
      { "MouseMan",	"Logitech MouseMan/TrackMan models (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mouseman" },
      { "MouseSystems",	"MouseSystems protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mousesystems" },
      { "ThinkingMouse","Kensington ThinkingMouse (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=thinkingmouse" },
      { NULL } },
};

DMenu MenuMousePort = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "Select your mouse port from the following menu",
    "The built-in pointing device of laptop/notebook computers is usually\n"
    "a PS/2 style device.",
    NULL,
    NULL,
    { { "COM1",	"Serial mouse on COM1 (/dev/cuaa0)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa0" },
      { "COM2",	"Serial mouse on COM2 (/dev/cuaa1)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa1" },
      { "COM3",	"Serial mouse on COM3 (/dev/cuaa2)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa2" },
      { "COM4",	"Serial mouse on COM4 (/dev/cuaa3)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa3" },
      { "BusMouse", "Logitech, ATI or MS bus mouse (/dev/mse0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/mse0" },
      { "PS/2",	"PS/2 style mouse (/dev/psm0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/psm0" },
      { NULL } },
};

DMenu MenuMouse = {
    DMENU_NORMAL_TYPE,
    "Please configure your mouse",
    "You can cut and paste text in the text console by running the mouse\n"
    "daemon.  Specify a port and a protocol type of your mouse and enable\n"
    "the mouse daemon.  If you don't want this feature, select 4 to disable\n"
    "the daemon.\n"
    "Once you've enabled the mouse daemon, you can specify \"/dev/sysmouse\"\n"
    "as your mouse device and \"SysMouse\" or \"MouseSystems\" as mouse\n"
    "protocol when running the X configuration utility (see Configuration\n"
    "menu).",
    NULL,
    NULL,
    { { "1 Type",	"Select mouse protocol type", NULL, dmenuSubmenu, NULL, &MenuMouseType },
      { "2 Port",	"Select mouse port", NULL, dmenuSubmenu, NULL, &MenuMousePort },
      { "3 Enable",	"Test and run the mouse daemon", NULL, mousedTest, NULL, NULL },
      { "4 Disable",	"Disable the mouse daemon", NULL, mousedDisable, NULL, NULL },
      { "0 Exit", "Exit this menu (returning to previous)", NULL, dmenuExit },
      { NULL } },
};

DMenu MenuXF86Config = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select the XFree86 configuration tool you want to use.",
    "The first tool, XF86Setup, is fully graphical and requires the\n"
    "VGA16 server in order to work (should have been selected by\n"
    "default, but if you de-selected it then you won't be able to\n"
    "use this fancy setup tool).  The second tool, xf86config, is\n"
    "a more simplistic shell-script based tool and less friendly to\n"
    "new users, but it may work in situations where the fancier one\n"
    "does not.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
    { { "XF86Setup",	"Fully graphical XFree86 configuration tool.",
	NULL, dmenuSetVariable, NULL, VAR_XF86_CONFIG "=XF86Setup" },
      { "xf86config",	"Shell-script based XFree86 configuration tool.",
	NULL, dmenuSetVariable, NULL, VAR_XF86_CONFIG "=xf86config" },
      { "XF98Setup",	"Fully graphical XFree86 configuration tool (PC98).",
	NULL, dmenuSetVariable, NULL, VAR_XF86_CONFIG "=XF98Setup" },
      { NULL } },
};

DMenu MenuMediaCDROM = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a CDROM type",
    "FreeBSD can be installed directly from a CDROM containing a valid\n"
    "FreeBSD distribution.  If you are seeing this menu it is because\n"
    "more than one CDROM drive was found on your system.  Please select one\n"
    "of the following CDROM drives as your installation drive.",
    "Press F1 to read the installation guide",
    "install",
    { { NULL } },
};

DMenu MenuMediaFloppy = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a Floppy drive",
    "You have more than one floppy drive.  Please chose which drive\n"
    "you would like to use.",
    NULL,
    NULL,
    { { NULL } },
};

DMenu MenuMediaDOS = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a DOS partition",
    "FreeBSD can be installed directly from a DOS partition\n"
    "assuming, of course, that you have copied the relevant\n"
    "distributions into your DOS partition before starting this\n"
    "installation.  If this is not the case then you should reboot\n"
    "DOS at this time and copy the distributions you wish to install\n"
    "into a \"FREEBSD\" subdirectory on one of your DOS partitions.\n"
    "Otherwise, please select the DOS partition containing the FreeBSD\n"
    "distribution files.",
    "Press F1 to read the installation guide",
    "install",
    { { NULL } },
};

DMenu MenuMediaFTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select a FreeBSD FTP distribution site",
    "Please select the site closest to you or \"other\" if you'd like to\n"
    "specify a different choice.  Also note that not every site listed here\n"
    "carries more than the base distribution kits. Only the Primary site is\n"
    "guaranteed to carry the full range of possible distributions.",
    "Select a site that's close!",
    "install",
    { { "KAME (IPv4/IPv6)",    "ftp.kame.net", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.kame.net/pub/FreeBSD/" },
      { "ITNet(IPv4/IPv6)",    "ftp.itjit.ad.jp", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.itjit.ad.jp/pub/FreeBSD/releases/i386/" },
      { "URL", "Specify some other ftp site by URL", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=other" },
      { NULL } }
};

DMenu MenuMediaTape = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a tape drive type",
    "FreeBSD can be installed from tape drive, though this installation\n"
    "method requires a certain amount of temporary storage in addition\n"
    "to the space required by the distribution itself (tape drives make\n"
    "poor random-access devices, so we extract _everything_ on the tape\n"
    "in one pass).  If you have sufficient space for this, then you should\n"
    "select one of the following tape devices detected on your system.",
    "Press F1 to read the installation guide",
    "install",
    { { NULL } },
};

DMenu MenuNetworkDevice = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Network interface information required",
    "If you are using PPP over a serial device, as opposed to a direct\n"
    "ethernet connection, then you may first need to dial your Internet\n"
    "Service Provider using the ppp utility we provide for that purpose.\n"
    "If you're using SLIP over a serial device then the expectation is\n"
    "that you have a HARDWIRED connection.\n\n"
    "You can also install over a parallel port using a special \"laplink\"\n"
    "cable to another machine running a fairly recent (2.0R or later)\n"
    "version of FreeBSD.",
    "Press F1 to read network configuration manual",
    "network_device",
    { { NULL } },
};

/* The media selection menu */
DMenu MenuMedia = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Installation Media",
    "FreeBSD can be installed from a variety of different installation\n"
    "media, ranging from floppies to an Internet FTP server.  If you're\n"
    "installing FreeBSD from a supported CDROM drive then this is generally\n"
    "the best media to use if you have no overriding reason for using other\n"
    "media.",
    "Press F1 for more information on the various media types",
    "media",
    { { "1 CDROM",		"Install from a FreeBSD CDROM",		NULL, mediaSetCDROM },
      { "2 FTP",		"Install from an FTP server",		NULL, mediaSetFTPActive },
      { "3 Options",		"Go to the Options screen",		NULL, optionsEditor },
      { NULL } },
};

/* The distributions menu */
DMenu MenuDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Distributions",
    "As a convenience, we provide several \"canned\" distribution sets.\n"
    "These select what we consider to be the most reasonable defaults for the\n"
    "type of system in question.  If you would prefer to pick and choose the\n"
    "list of distributions yourself, simply select \"Custom\".  You can also\n"
    "pick a canned distribution set and then fine-tune it with the Custom item.\n\n"
    "Choose an item by pressing [SPACE]. When you are finished, chose the Exit\n"
    "item or press [ENTER].",
    "Press F1 for more information on these options.",
    "distributions",
    { { "1 Developer",		"Full sources, binaries and doc but no games", 
	checkDistDeveloper,	distSetDeveloper },
      { "2 X-Developer",	"Same as above, but includes the X Window System",
	checkDistXDeveloper,	distSetXDeveloper },
      { "3 Kern-Developer",	"Full binaries and doc, kernel sources only",
	checkDistKernDeveloper, distSetKernDeveloper },
      { "4 User",		"Average user - binaries and doc only",
	checkDistUser,		distSetUser },
      { "5 X-User",		"Same as above, but includes the X Window System",
	checkDistXUser,		distSetXUser },
      { "6 Minimal",		"The smallest configuration possible",
	checkDistMinimum,	distSetMinimum },
      { "7 Custom",		"Specify your own distribution set",
	NULL,			dmenuSubmenu, NULL, &MenuSubDistributions, '>', '>', '>' },
      { "8 All",		"All sources and binaries (incl X Window System)",
	checkDistEverything,	distSetEverything },
      { "9 Clear",		"Reset selected distribution list to nothing",
	NULL,			distReset, NULL, NULL, ' ', ' ', ' ' },
      { "0 Exit",		"Exit this menu (returning to previous)",
	checkTrue,		dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuSubDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the distributions you wish to install.",
    "Please check off the distributions you wish to install.  At the\n"
    "very minimum, this should be \"bin\".  WARNING:  Do not export the\n"
    "DES distribution out of the U.S.!  It is for U.S. customers only.",
    NULL,
    NULL,
    { { "bin",		"Binary base distribution (required)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_BIN },
      { "compat1x",	"FreeBSD 1.x binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT1X },
      { "compat20",	"FreeBSD 2.0 binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT20 },
      { "compat21",	"FreeBSD 2.1 binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT21 },
      { "DES",		"DES encryption code - NOT FOR EXPORT!",
	DESFlagCheck,	distSetDES },
      { "dict",		"Spelling checker dictionary files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DICT },
      { "doc",		"Miscellaneous FreeBSD online docs",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DOC },
      { "games",	"Games (non-commercial)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_GAMES },
      { "info",		"GNU info files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_INFO },
      { "man",		"System manual pages - recommended",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_MANPAGES },
      { "catman",	"Preformatted system manual pages",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_CATPAGES },
      { "proflibs",	"Profiled versions of the libraries",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_PROFLIBS },
      { "src",		"Sources for everything but DES",
	srcFlagCheck,	distSetSrc },
      { "ports",	"The FreeBSD Ports collection",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_PORTS },
      { "XFree86",	"The XFree86 3.3.2.3 distribution",
	x11FlagCheck,	distSetXF86 },
      { "All",		"All sources, binaries and X Window System binaries",
	NULL, distSetEverything, NULL, NULL, ' ', ' ', ' ' },
      { "Clear",	"Reset all of the above",
	NULL, distReset, NULL, NULL, ' ', ' ', ' ' },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuDESDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the encryption facilities you wish to install.",
    "Please check off any special DES-based encryption distributions\n"
    "you would like to install.  Please note that these services are NOT FOR\n"
    "EXPORT from the United States.  For information on non-U.S. FTP\n"
    "distributions of this software, please consult the release notes.",
    NULL,
    NULL,
    { { "des",		"Basic DES encryption services",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_DES, },
      { "krb",		"Kerberos encryption services",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_KERBEROS },
      { "skerbero",	"Sources for Kerberos IV",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_SKERBEROS },
      { "ssecure",	"Sources for DES",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_SSECURE },
      { "scrypto",	"Export controlled crypto sources",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_SCRYPTO },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuSrcDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS ,
    "Select the sub-components of src you wish to install.",
    "Please check off those portions of the FreeBSD source tree\n"
    "you wish to install.",
    NULL,
    NULL,
    { { "base",		"top-level files in /usr/src",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BASE },
      { "contrib",	"/usr/src/contrib (contributed software)",
	dmenuFlagCheck,	dmenuSetFlag,	NULL, &SrcDists, '[', 'X', ']', DIST_SRC_CONTRIB },
      { "gnu",		"/usr/src/gnu (software from the GNU Project)",
	dmenuFlagCheck,	dmenuSetFlag,	NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GNU },
      { "etc",		"/usr/src/etc (miscellaneous system files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_ETC },
      { "games",	"/usr/src/games (the obvious!)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GAMES },
      { "include",	"/usr/src/include (header files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_INCLUDE },
      { "lib",		"/usr/src/lib (system libraries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIB },
      { "libexec",	"/usr/src/libexec (system programs)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIBEXEC },
      { "lkm",		"/usr/src/lkm (Loadable Kernel Modules)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LKM	},
      { "release",	"/usr/src/release (release-generation tools)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_RELEASE },
      { "bin",		"/usr/src/bin (system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BIN },
      { "sbin",		"/usr/src/sbin (system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SBIN },
      { "share",	"/usr/src/share (documents and shared files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SHARE },
      { "sys",		"/usr/src/sys (FreeBSD kernel)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SYS },
      { "ubin",		"/usr/src/usr.bin (user binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_UBIN },
      { "usbin",	"/usr/src/usr.sbin (aux system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_USBIN },
      { "smailcf",	"/usr/src/usr.sbin (sendmail config macros)",
	dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SMAILCF },
      { "All",		"Select all of the above",
	NULL,		setSrc, NULL, NULL, ' ', ' ', ' ' },
      { "Clear",	"Reset all of the above",
	NULL,		clearSrc, NULL, NULL, ' ', ' ', ' ' },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuXF86Select = {
    DMENU_NORMAL_TYPE,
    "XFree86 3.3.2.3 Distribution",
    "Please select the components you need from the XFree86 3.3.2.3\n"
    "distribution sets.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
    { { "Basic",	"Basic component menu (required)",	NULL, dmenuSubmenu, NULL, &MenuXF86SelectCore },
      { "Server",	"X server menu",			NULL, dmenuSubmenu, NULL, &MenuXF86SelectServer },
      { "Fonts",	"Font set menu",			NULL, dmenuSubmenu, NULL, &MenuXF86SelectFonts },
      { "All",		"Select all XFree86 distribution sets", NULL, setX11All },
      { "Clear",	"Reset XFree86 distribution list",	NULL, clearX11All },
      { "Exit",		"Exit this menu (returning to previous)", checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuXF86SelectCore = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "XFree86 3.3.2.3 base distribution types",
    "Please check off the basic XFree86 components you wish to install.\n"
    "Bin, lib, and set are recommended for a minimum installaion.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
    { { "bin",		"Client applications and shared libs",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_BIN },
      { "cfg",		"Configuration files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_CFG },
      { "doc",		"READMEs and release notes",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_DOC },
      { "html",		"HTML documentation files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_HTML },
      { "lib",		"Data files needed at runtime",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LIB },
      { "lk98",		"Server link kit for PC98 machines",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LKIT98 },
      { "lkit",		"Server link kit for all other machines",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LKIT },
      { "man",		"Manual pages",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_MAN },
      { "prog",		"Programmer's header and library files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_PROG },
      { "set",		"XFree86 Setup Utility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_SET },
      { "9set",		"XFree86 Setup Utility for PC98 machines",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_9SET },
      { "sources",	"XFree86 3.3.2.3 standard sources",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_SRC },
      { "csources",	"XFree86 3.3.2.3 contrib sources",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_CSRC },
      { "All",		"Select all of the above",
	NULL,		setX11Misc, NULL, NULL, ' ', ' ', ' ' },
      { "Clear",	"Reset all of the above",
	NULL,		clearX11Misc, NULL, NULL, ' ', ' ', ' ' },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuXF86SelectFonts = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS ,
    "Font distribution selection.",
    "Please check off the individual font distributions you wish to\n\
install.  At the minimum, you should install the standard\n\
75 DPI and misc fonts if you're also installing a server\n\
(these are selected by default).",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
    { { "fnts",		"Standard 75 DPI and miscellaneous fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_MISC },
      { "f100",		"100 DPI fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_100 },
      { "fcyr",		"Cyrillic Fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_CYR },
      { "fscl",		"Speedo and Type scalable fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_SCALE },
      { "non",		"Japanese, Chinese and other non-english fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_NON },
      { "server",	"Font server",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_SERVER },
      { "All",		"All fonts",
	NULL,		setX11Fonts, NULL, NULL, ' ', ' ', ' ' },
      { "Clear",	"Reset font selections",
	NULL,		clearX11Fonts, NULL, NULL, ' ', ' ', ' ' },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuXF86SelectServer = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "X Server selection.",
    "Please check off the types of X servers you wish to install.\n"
    "If you are unsure as to which server will work for your graphics card,\n"
    "it is recommended that try the SVGA or VGA16 servers or, for PC98\n"
    "machines, the 9EGC or 9840 servers.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
    { { "SVGA",		"Standard VGA or Super VGA card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_SVGA },
      { "VGA16",	"Standard 16 color VGA card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_VGA16 },
      { "Mono",		"Standard Monochrome card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MONO },
      { "8514",		"8-bit (256 color) IBM 8514 or compatible card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_8514 },
      { "AGX",		"8-bit AGX card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_AGX },
      { "I128",		"8, 16 and 24-bit #9 Imagine I128 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_I128 },
      { "Ma8",		"8-bit ATI Mach8 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH8 },
      { "Ma32",		"8 and 16-bit (65K color) ATI Mach32 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH32 },
      { "Ma64",		"8 and 16-bit (65K color) ATI Mach64 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH64 },
      { "P9K",		"8, 16, and 24-bit color Weitek P9000 based boards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_P9000 },
      { "S3",		"8, 16 and 24-bit color S3 based boards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_S3 },
      { "S3V",		"8, 16 and 24-bit color S3 Virge based boards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_S3V },
      { "W32",		"8-bit ET4000/W32, /W32i and /W32p cards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_W32 },
      { "nest",		"A nested server for testing purposes",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_NEST },
      { "PC98",		"Select an X server for a NEC PC98 [Submenu]",
	NULL,		dmenuSubmenu,  NULL, &MenuXF86SelectPC98Server, '>', ' ', '>', 0 },
      { "All",		"Select all of the above",
	NULL,		setX11Servers, NULL, NULL, ' ', ' ', ' ' },
      { "Clear",	"Reset all of the above",
	NULL,		clearX11Servers, NULL, NULL, ' ', ' ', ' ' },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuXF86SelectPC98Server = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "PC98 X Server selection.",
    "Please check off the types of NEC PC98 X servers you wish to install.\n\
If you are unsure as to which server will work for your graphics card,\n\
it is recommended that try the SVGA or VGA16 servers (the VGA16 and\n\
Mono servers are particularly well-suited to most LCD displays).",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
    { { "9480",		"PC98 8-bit (256 color) PEGC-480 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9480 },
      { "9EGC",		"PC98 4-bit (16 color) EGC card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9EGC },
      { "9GA9",		"PC98 GA-968V4/PCI (S3 968) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9GA9 },
      { "9GAN",		"PC98 GANB-WAP (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9GAN },
      { "9LPW",		"PC98 PowerWindowLB (S3) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9LPW },
      { "9MGA",		"PC98 MGA (Matrox) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9MGA },
      { "9NKV",		"PC98 NKV-NEC (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9NKV },
      { "9NS3",		"PC98 NEC (S3) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9NS3 },
      { "9SPW",		"PC98 SKB-PowerWindow (S3) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9SPW },
      { "9SVG",		"PC98 generic SVGA card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9SVG },
      { "9TGU",		"PC98 Cyber9320 and TGUI9680 cards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9TGU },
      { "9WEP",		"PC98 WAB-EP (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9WEP },
      { "9WS",		"PC98 WABS (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9WS },
      { "9WSN",		"PC98 WSN-A2F (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9WSN },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } }
};

DMenu MenuDiskDevices = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select Drive(s)",
    "Please select the drive, or drives, on which you wish to perform\n"
    "this operation.  If you are attempting to install a boot partition\n"
    "on a drive other than the first one or have multiple operating\n"
    "systems on your machine, you will have the option to install a boot\n"
    "manager later.  To select a drive, use the arrow keys to move to it\n"
    "and press [SPACE].  To de-select it, press [SPACE] again.\n\n"
    "Select OK or Cancel to leave this menu.",
    "Press F1 for important information regarding disk geometry!",
    "drives",
    { { NULL } },
};

DMenu MenuHTMLDoc = {
    DMENU_NORMAL_TYPE,
    "Select HTML Documentation pointer",
    "Please select the body of documentation you're interested in, the main\n"
    "ones right now being the FAQ and the Handbook.  You can also chose \"other\"\n"
    "to enter an arbitrary URL for browsing.",
    "Press F1 for more help on what you see here.",
    "html",
    { { "Handbook",	"The FreeBSD Handbook.",				NULL, docShowDocument },
      { "FAQ",		"The Frequently Asked Questions guide.",		NULL, docShowDocument },
      { "Home",		"The Home Pages for the FreeBSD Project (requires net)", NULL, docShowDocument },
      { "Other",	"Enter a URL.",						NULL, docShowDocument },
      { NULL } },
};

/* The main installation menu */
DMenu MenuInstallCustom = {
    DMENU_NORMAL_TYPE,
    "Choose Custom Installation Options",
    "This is the custom installation menu. You may use this menu to specify\n"
    "details on the type of distribution you wish to have, where you wish\n"
    "to install it from and how you wish to allocate disk storage to FreeBSD.",
    "Press F1 to read the installation guide",
    "install",
    { { "1 Options",		"View/Set various installation options", NULL, optionsEditor },
      { "2 Partition",		"Allocate disk space for FreeBSD",	NULL, diskPartitionEditor },
      { "3 Label",		"Label allocated disk partitions",	NULL, diskLabelEditor },
      { "4 Distributions",	"Select distribution(s) to extract",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { "5 Media",		"Choose the installation media type",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { "6 Commit",		"Perform any pending Partition/Label/Extract actions", NULL, installCustomCommit },
      { "0 Exit",		"Exit this menu (returning to previous)", NULL,	dmenuExit },
      { NULL } },
};

/* MBR type menu */
DMenu MenuMBRType = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "overwrite me",		/* will be disk specific label */
    "FreeBSD comes with a boot selector that allows you to easily\n"
    "select between FreeBSD and any other operating systems on your machine\n"
    "at boot time.  If you have more than one drive and want to boot\n"
    "from the second one, the boot selector will also make it possible\n"
    "to do so (limitations in the PC BIOS usually prevent this otherwise).\n"
    "If you do not want a boot selector, or wish to replace an existing\n"
    "one, select \"standard\".  If you would prefer your Master Boot\n"
    "Record to remain untouched then select \"None\".\n\n"
    "  NOTE:  PC-DOS users will almost certainly require \"None\"!",
    "Press F1 to read about drive setup",
    "drives",
    { { "BootMgr",	"Install the FreeBSD Boot Manager (\"Booteasy\")",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr },
      { "Standard",	"Install a standard MBR (no boot manager)",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
      { "None",		"Leave the Master Boot Record untouched",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 2 },
      { NULL } },
};

/* Final configuration menu */
DMenu MenuConfigure = {
    DMENU_NORMAL_TYPE,
    "FreeBSD Configuration Menu",	/* title */
    "If you've already installed FreeBSD, you may use this menu to customize\n"
    "it somewhat to suit your particular configuration.  Most importantly,\n"
    "you can use the Packages utility to load extra \"3rd party\"\n"
    "software not provided in the base distributions.",
    "Press F1 for more information on these options",
    "configure",
    { { "1 User Management",	"Add user and group information",
	NULL, dmenuSubmenu, NULL, &MenuUsermgmt },
      { "2 Console",	"Customize system console behavior",
	NULL,	dmenuSubmenu, NULL, &MenuSyscons },
      { "3 Time Zone",	"Set which time zone you're in",
	NULL,	dmenuSystemCommand, NULL, "tzsetup" },
      { "4 Media",	"Change the installation media type",
	NULL,	dmenuSubmenu, NULL, &MenuMedia	},
      { "5 Mouse",	"Configure your mouse",
	NULL,	dmenuSubmenu, NULL, &MenuMouse, NULL },
      { "6 Networking",	"Configure additional network services",
	NULL,	dmenuSubmenu, NULL, &MenuNetworking },
      { "7 Startup",	"Configure system startup options",
	NULL,	dmenuSubmenu, NULL, &MenuStartup },
      { "8 Options",	"View/Set various installation options",
	NULL, optionsEditor },
      { "9 Distributions", "Install additional distribution sets",
	NULL, distExtractAll },
      { "P Packages",	"Install pre-packaged software for FreeBSD",
	NULL, configPackages },
      { "R Root Password", "Set the system manager's password",
	NULL,	dmenuSystemCommand, NULL, "passwd root" },
      { "D HTML Docs",	"Go to the HTML documentation menu (post-install)",
	NULL, docBrowser },
      { "X XFree86",	"Configure XFree86",
	NULL, configXEnvironment },
      { "K PAO+KAME Kit",	"Install PAO+KAME kit menu",
	NULL,	dmenuSubmenu, NULL, &MenuKameInitial },
      { "L Label",	"The disk Label editor",
	NULL, diskLabelEditor },
      { "F Fdisk",	"The disk Slice (PC-style partition) Editor",
	NULL, diskPartitionEditor },
      { "U Register",	"Register yourself or company as a FreeBSD user.", NULL, configRegister },
      { "E Exit",		"Exit this menu (returning to previous)",
	NULL,	dmenuExit },
      { NULL } },
};

DMenu MenuStartup = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Startup Services Menu",
    "This menu allows you to configure various aspects of your system's\n"
    "startup configuration.  Remember to use SPACE to select items!  The\n"
    "RETURN key will leave this menu (as with all checkbox menus).",
    NULL,
    NULL,
    { { "APM",		"Auto-power management services (typically laptops)",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "apm_enable=YES" },
      { "pccard",	"Enable PCCARD (AKA PCMCIA) services (also laptops)",
	dmenuVarCheck, dmenuToggleVariable, NULL, "pccard_enable=YES" },
      { "pccard mem",	"Set PCCARD memory address (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "pccard_mem" },
      { "pccard ifconfig",	"List of PCCARD ethernet devices to configure",
	dmenuVarCheck, dmenuISetVariable, NULL, "pccard_ifconfig" },
      { " ",		" -- ", NULL,	NULL, NULL, NULL, ' ', ' ', ' ' },
      { "startup dirs",	"Set the list of dirs to look for startup scripts",
	dmenuVarCheck, dmenuISetVariable, NULL, "local_startup" },
      { "named",	"Run a local name server on this host",
	dmenuVarCheck, dmenuToggleVariable, NULL, "named_enable=YES" },
      { "named flags",	"Set default flags to named (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "named_flags" },
      { "nis client",	"This host wishes to be an NIS client.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "nis_client_enable=YES" },
      { "nis server",	"This host wishes to be an NIS server.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "nis_server_enable=YES" },
      { " ",		" -- ", NULL,	NULL, NULL, NULL, ' ', ' ', ' ' },
      { "accounting",	"This host wishes to run process accounting.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "accounting_enable=YES" },
      { "lpd",		"This host has a printer and wants to run lpd.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "lpd_enable=YES" },
      { "linux",	"This host wants to be able to run linux binaries.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "linux_enable=YES" },
      { "quotas",	"This host wishes to check quotas on startup.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "check_quotas=YES" },
      { "SCO",		"This host wants to be able to run IBCS2 binaries.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "ibcs2_enable=YES" },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuNetworking = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Network Services Menu",
    "You may have already configured one network device (and the other\n"
    "various hostname/gateway/name server parameters) in the process\n"
    "of installing FreeBSD.  This menu allows you to configure other\n"
    "aspects of your system's network configuration.",
    NULL,
    NULL,
    { { "Interfaces",	"Configure additional network interfaces",
	NULL, tcpMenuSelect },
      { "NFS client",	"This machine will be an NFS client",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "nfs_client_enable=YES" },
      { "NFS server",	"This machine will be an NFS server",
	dmenuVarCheck, configNFSServer, NULL, "nfs_server_enable=YES" },
      { "AMD",	"This machine wants to run the auto-mounter service",
	dmenuVarCheck, dmenuToggleVariable, NULL, "amd_enable=YES" },
      { "AMD Flags",	"Set flags to AMD service (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "amd_flags" },
      { "TCP Extensions",       "Allow RFC1323 and RFC1644 TCP extensions?",
	dmenuVarCheck, dmenuToggleVariable, NULL, "tcp_extensions=YES" },
      { "Gateway",	"This machine will route packets between interfaces",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "gateway_enable=YES" },
      { "Ntpdate",	"Select a clock-synchronization server",
	dmenuVarCheck,	dmenuSubmenu, NULL, &MenuNTP, '[', 'X', ']', "ntpdate_enable=YES" },
      { "router",	"Select routing daemon (default: routed)",
	dmenuVarCheck, configRouter, NULL, "router" },
      { "Rwhod",	"This machine wants to run the rwho daemon",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "rwhod_enable=YES" },
      { "Anon FTP",	"This machine wishes to allow anonymous FTP.",
	dmenuVarCheck, configAnonFTP, NULL, "anon_ftp" },
      { "PCNFSD",	"Run authentication server for clients with PC-NFS.",
	dmenuVarCheck, configPCNFSD, NULL, "pcnfsd" },
      { "Exit",		"Exit this menu (returning to previous)",
	checkTrue,	dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuNTP = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "NTPDATE Server Selection",
    "There are a number of time synchronization servers available\n"
    "for public use around the Internet.  Please select one reasonably\n"
    "close to you to have your system time synchronized accordingly.",
    "These are the primary open-access NTP servers",
    NULL,
    { { "None",		        "No ntp server",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=NO,ntpdate_flags=none" },
      { "Other",		"Select a site not on this list",
	dmenuVarsCheck, configNTP, NULL, NULL },
      { "Australia",		"ntp.syd.dms.csiro.au (HP 5061 Cesium Beam)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.syd.dms.csiro.au" },
      { "Canada",		"tick.usask.ca (GOES clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=tick.usask.ca" },
      { "France",		"canon.inria.fr (TDF clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=canon.inria.fr" },
      { "Germany",		"ntps1-{0,1,2}.uni-erlangen.de (GPS)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntps1-0.uni-erlangen.de" },
      { "Germany #2",		"ntps1-0.cs.tu-berlin.de (GPS)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntps1-0.cs.tu-berlin.de" },
      { "Japan",		"clock.nc.fukuoka-u.ac.jp (GPS clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=clock.nc.fukuoka-u.ac.jp" },
      { "Japan #2",		"clock.tl.fukuoka-u.ac.jp (GPS clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=clock.tl.fukuoka-u.ac.jp" },
      { "Netherlands",		"ntp0.nl.net (GPS clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp0.nl.net" },
      { "Norway",		"timer.unik.no (NTP clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=timer.unik.no" },
      { "Sweden",		"Time1.Stupi.SE (Cesium/GPS)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=Time1.Stupi.SE" },
      { "Switzerland",		"swisstime.ethz.ch (DCF77 clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=swisstime.ethz.ch" },
      { "U.S. East Coast",	"bitsy.mit.edu (WWV clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=bitsy.mit.edu" },
      { "U.S. East Coast #2",	"otc1.psu.edu (WWV clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=otc1.psu.edu" },
      { "U.S. West Coast",	"apple.com (WWV clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=apple.com" },
      { "U.S. West Coast #2",	"clepsydra.dec.com (GOES clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=clepsydra.dec.com" },
      { "U.S. West Coast #3",	"clock.llnl.gov (WWVB clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=clock.llnl.gov" },
      { "U.S. Midwest",		"ncar.ucar.edu (WWVB clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ncar.ucar.edu" },
      { "U.S. Pacific",		"chantry.hawaii.net (WWV/H clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=chantry.hawaii.net" },
      { "U.S. Southwest",	"shorty.chpc.utexas.edu (WWV clock)",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=shorty.chpc.utexas.edu" },
      { NULL } },
};

DMenu MenuSyscons = {
    DMENU_NORMAL_TYPE,
    "System Console Configuration",
    "The default system console driver for FreeBSD (syscons) has a\n"
    "number of configuration options which may be set according to\n"
    "your preference.\n\n"
    "When you are done setting configuration options, select Cancel.",
    "Configure your system console settings",
    NULL,
    { { "Font",		"Choose an alternate screen font",	NULL, dmenuSubmenu, NULL, &MenuSysconsFont },
      { "Keymap",	"Choose an alternate keyboard map",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "Repeat",	"Set the rate at which keys repeat",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { "Saver",	"Configure the screen saver",		NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
      { "Screenmap",	"Choose an alternate screenmap",	NULL, dmenuSubmenu, NULL, &MenuSysconsScrnmap },
      { "Exit",		"Exit this menu (returning to previous)", NULL, dmenuExit },
      { NULL } },
};

DMenu MenuSysconsKeymap = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keymap",
    "The default system console driver for FreeBSD (syscons) defaults\n"
    "to a standard \"American\" keyboard map.  Users in other countries\n"
    "(or with different keyboard preferences) may wish to choose one of\n"
    "the other keymaps below.\n"
    "Note that sysinstall itself only uses the part of the keyboard map\n"
    "which is required to generate the ANSI character subset, but your\n"
    "choice of keymap will also be saved for later (fuller) use.",
    "Choose a keyboard map",
    NULL,
    { { "Belgian",	"Belgian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=be.iso" },
      { "Brazil CP850",	"Brazil CP850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.cp850" },
      { "Brazil ISO (accent)",	"Brazil ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.iso.acc" },
      { "Brazil ISO",	"Brazil ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.iso" },
      { "Danish CP865",	"Danish Code Page 865 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=danish.cp865" },
      { "Danish ISO",	"Danish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=danish.iso" },
      { "French ISO (accent)", "French ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=fr.iso.acc" },
      { "French ISO",	"French ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=fr.iso" },
      { "German CP850",	"German Code Page 850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=german.cp850"	},
      { "German ISO",	"German ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=german.iso" },
      { "Icelandic (accent)", "Icelandic ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=icelandic.iso.acc" },
      { "Icelandic",	"Icelandic ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=icelandic.iso" },
      { "Italian",	"Italian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=it.iso" },
      { "Latin American", "Latin American ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=lat-amer" },
      { "Japanese 106",	"Japanese 106 keymap",  dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=jp.106" },
      { "Norway ISO",	"Norwegian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=norwegian.iso" },
      { "Russia CP866",	"Russian CP866 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ru.cp866" },
      { "Russia KOI8-R", "Russian KOI8-R keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ru.koi8-r" },
      { "Slovenian", "Slovenian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=si.iso.acc" },
      { "Spanish (accent)", "Spanish ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=spanish.iso.acc" },
      { "Spanish",	"Spanish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=spanish.iso" },
      { "Swedish CP850", "Swedish Code Page 850 keymap", dmenuVarCheck,	dmenuSetKmapVariable, NULL, "keymap=swedish.cp850" },
      { "Swedish ISO",	"Swedish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swedish.iso" },
      { "Swiss German",	"Swiss German ISO keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissgerman.iso" },
      { "U.K. CP850",	"United Kingdom Code Page 850 keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=uk.cp850" },
      { "U.K. ISO",	"United Kingdom ISO keymap", dmenuVarCheck,	dmenuSetKmapVariable, NULL, "keymap=uk.iso" },
      { "U.S. Dvorak",	"United States Dvorak keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.dvorak" },
      { "U.S. ISO",	"United States ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.iso" },
      { NULL } },
};

DMenu MenuSysconsKeyrate = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keyboard Repeat Rate",
    "This menu allows you to set the speed at which keys repeat\n"
    "when held down.",
    "Choose a keyboard repeat rate",
    NULL,
    { { "Slow",	"Slow keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=slow" },
      { "Normal", "\"Normal\" keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=normal" },
      { "Fast",	"Fast keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=fast" },
      { "Default", "Use default keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=NO" },
      { NULL } },
};

DMenu MenuSysconsSaver = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Screen Saver",
    "By default, the console driver will not attempt to do anything\n"
    "special with your screen when it's idle.  If you expect to leave your\n"
    "monitor switched on and idle for long periods of time then you should\n"
    "probably enable one of these screen savers to prevent phosphor burn-in.",
    "Choose a nifty-looking screen saver",
    NULL,
    { { "blank",	"Simply blank the screen",
	dmenuVarCheck, configSaver, NULL, "saver=blank" },
      { "Daemon",	"\"BSD Daemon\" animated screen saver",
	dmenuVarCheck, configSaver, NULL, "saver=daemon" },
      { "Green",	"\"Green\" power saving mode (if supported by monitor)",
	dmenuVarCheck, configSaver, NULL, "saver=green" },
      { "Snake",	"Draw a FreeBSD \"snake\" on your screen",
	dmenuVarCheck, configSaver, NULL, "saver=snake" },
      { "Star",	"A \"twinkling stars\" effect",
	dmenuVarCheck, configSaver, NULL, "saver=star" },
      { "Timeout",	"Set the screen saver timeout interval",
	NULL, configSaverTimeout, NULL, NULL, ' ', ' ', ' ' },
      { NULL } },
};

DMenu MenuSysconsScrnmap = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Screenmap",
    "Unless you load a specific font, most PC hardware defaults to\n"
    "displaying characters in the IBM 437 character set.  However,\n"
    "in the Unix world, this character set is very rarely used.  Most\n"
    "Western European countries, for example, prefer ISO 8859-1.\n"
    "American users won't notice the difference since the bottom half\n"
    "of all these character sets is ANSI anyway.\n"
    "If your hardware is capable of downloading a new display font,\n"
    "you should probably choose that option.  However, for hardware\n"
    "where this is not possible (e.g. monochrome adapters), a screen\n"
    "map will give you the best approximation that your hardware can\n"
    "display at all.",
    "Choose a screen map",
    NULL,
    { { "None",			"No screenmap, use default font", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=NO" },
      { "KOI8-R to IBM866",	"Russian KOI8-R to IBM 866 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=koi8-r2cp866" },
      { "ISO 8859-1 to IBM437",	"W-Europe ISO 8859-1 to IBM 437 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=iso-8859-1_to_cp437" },
      { NULL } },
};

DMenu MenuSysconsFont = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Font",
    "Most PC hardware defaults to displaying characters in the\n"
    "IBM 437 character set.  However, in the Unix world, this\n"
    "character set is very rarely used.  Most Western European\n"
    "countries, for example, prefer ISO 8859-1.\n"
    "American users won't notice the difference since the bottom half\n"
    "of all these charactersets is ANSI anyway.  However, they might\n"
    "want to load a font anyway to use the 30- or 50-line displays.\n"
    "If your hardware is capable of downloading a new display font,\n"
    "you can select the appropriate font below.",
    "Choose a font",
    NULL,
    { { "None", "Use default font",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=NO,font8x14=NO,font8x16=NO" },
      { "IBM 437", "English",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp437-8x8,font8x14=cp437-8x14,font8x16=cp437-8x16" },
      { "IBM 850", "Western Europe, IBM encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp850-8x8,font8x14=cp850-8x14,font8x16=cp850-8x16" },
      { "IBM 865", "Norwegian, IBM encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp865-8x8,font8x14=cp865-8x14,font8x16=cp865-8x16" },
      { "IBM 866", "Russian, IBM encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp866-8x8,font8x14=cp866-8x14,font8x16=cp866-8x16" },
      { "ISO 8859-1", "Western Europe, ISO encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=iso-8x8,font8x14=iso-8x14,font8x16=iso-8x16" },
      { "KOI8-R", "Russian, KOI8-R encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=koi8-r-8x8,font8x14=koi8-r-8x14,font8x16=koi8-r-8x16" },
      { NULL } },
};

DMenu MenuUsermgmt = {
    DMENU_NORMAL_TYPE,
    "User and group management",
    "The submenus here allow to manipulate user groups and\n"
    "login accounts.\n",
    "Configure your user groups and users",
    NULL,
    { { "Add user",	"Add a new user to the system.",	NULL, userAddUser },
      { "Add group",	"Add a new user group to the system.",	NULL, userAddGroup },
      { "Exit",		"Exit this menu (returning to previous)", NULL, dmenuExit },
      { NULL } },
};

DMenu MenuFixit = {
    DMENU_NORMAL_TYPE,
    "Please choose a fixit option",
    "There are three ways of going into \"fixit\" mode:\n"
    "- you can use the 2nd FreeBSD CDROM, in which case there will be\n"
    "  full access to the complete set of FreeBSD commands and utilities,\n"
    "- you can use the more limited (but perhaps customized) fixit floppy,\n"
    "- or you can start an Emergency Holographic Shell now, which is\n"
    "  limited to the subset of commands that is already available right now.",
    "Press F1 for more detailed repair instructions",
    "fixit",
{ { "1 CDROM",	"Use the 2nd \"live\" CDROM from the distribution",	NULL, installFixitCDROM },
  { "2 Floppy",	"Use a floppy generated from the fixit image",		NULL, installFixitFloppy },
  { "3 Shell",	"Start an Emergency Holographic Shell",			NULL, installFixitHoloShell },
  { NULL } },
};

DMenu MenuKameInitial = {
    DMENU_NORMAL_TYPE,
    "PAO+KAME Menu",
    "Welcome to the PAO+KAME installation menu.",
    NULL,
    NULL,
    { { "1 Doc, README",	"Read this",
	NULL, dmenuDisplayFile, NULL, "README.v6" },
      { "2 Doc, Copyright",	"The KAME Copyright notices.",
	NULL, dmenuDisplayFile,	NULL, "COPYRIGHT.v6" },
      { "3 Doc, Install",	"please read INSTALL",
	NULL, dmenuDisplayFile,	NULL, "INSTALL.v6" },
      { "4 Get PAO+KAME Kit",	"Get PAO+KAME kit",
	NULL,	installKameCommit },
      { "5 Compile PAO+KAME Kit",	"Compile and install PAO+KAME",
	NULL,	installKameKit },
      { "E Exit",		"Exit this menu (returning to previous)",
	NULL,	dmenuExit },
      { NULL } },
};

DMenu MenuKameDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS ,
    "PAO+KAME Kit Distribution",
    "Please select the components you need from the KAME kit\n"
    "distribution sets.",
    NULL,
    NULL,
    { { "kernel",	"/kernel.GENERIC.v6 (KAME IPv6 kernel)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &KAMEDists, '[', 'X', ']', DIST_KAME_KERN },
      { "bin",		"/usr/local/v6 (KAME userland tools; size 2.6M)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &KAMEDists, '[', 'X', ']', DIST_KAME_BIN },
      { "src",		"/usr/src/kame (KAME Kit sources; size 9M)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &KAMEDists, '[', 'X', ']', DIST_KAME_SRC },
      {	"PAObin",	"PAO userland",
	dmenuFlagCheck, dmenuSetFlag, NULL, &KAMEDists, '[', 'X', ']', DIST_PAO_BIN },
      { "Exit",		"Exit this menu (returning to previous)", checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { NULL } },
};

DMenu MenuKame = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Installation Media",
    "FreeBSD can be installed from a variety of different installation\n"
    "media, ranging from floppies to an Internet FTP server.  If you're\n"
    "installing FreeBSD from a supported CDROM drive then this is generally\n"
    "the best media to use if you have no overriding reason for using other\n"
    "media.",
    NULL,
    NULL,
    { { "1 FTP",		"Install from an FTP server",		NULL, mediaSetFTPActive },
      { "2 FTP Passive",	"Install from an FTP server through a firewall", NULL, mediaSetFTPPassive },
      { NULL } },
};

DMenu MenuKameFTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select a FreeBSD FTP distribution site or KAME FTP distribution site",
    "Please select the site closest to you or \"other\" if you'd like to\n"
    "specify a different choice.  Also note that not every site listed here\n"
    "carries more than the base distribution kits. Only the Primary site is\n"
    "guaranteed to carry the full range of possible distributions.",
    NULL,
    NULL,
    { { "KAME (IPv4/IPv6)", "ftp.v6.kame.net", NULL, dmenuSetVariable, NULL,
        VAR_KAME_FTP_PATH "=ftp://ftp.v6.kame.net/pub/kame/installer-imasy/FreeBSD-IPv6/"},
      { "ITNet(IPv4/IPv6)", "ftp.itjit.ad.jp", NULL, dmenuSetVariable, NULL,
        VAR_KAME_FTP_PATH "=ftp://ftp.itjit.ad.jp/pub/IPv6/installer/FreeBSD-IPv6/" },
      { "NTTv6(IPv4/IPv6)", "ftp.nttv6.net", NULL, dmenuSetVariable, NULL,
	VAR_KAME_FTP_PATH "=ftp://ftp.nttv6.net/ipv6/IPv6_installer/FreeBSD-IPv6/"},
      { "URL", "Specify some other ftp site by URL", NULL, dmenuSetVariable, NULL,
	VAR_KAME_FTP_PATH "=other" },
      { NULL } }
};
