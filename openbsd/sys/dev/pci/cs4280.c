/*	$OpenBSD: cs4280.c,v 1.5 2000/09/17 20:31:20 marc Exp $	*/
/*	$NetBSD: cs4280.c,v 1.5 2000/06/26 04:56:23 simonb Exp $	*/

/*
 * Copyright (c) 1999, 2000 Tatoku Ogaito.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Tatoku Ogaito
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Cirrus Logic CS4280 (and maybe CS461x) driver.
 * Data sheets can be found
 * http://www.cirrus.com/ftp/pubs/4280.pdf
 * http://www.cirrus.com/ftp/pubs/4297.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/cirrus/embedded_audio_spec.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/cirrus/embedded_audio_spec.doc
 */

/*
 * TODO
 * Implement MIDI
 * Joystick support
 */

#ifdef CS4280_DEBUG
#ifndef MIDI_READY
#define MIDI_READY
#endif /* ! MIDI_READY */
#endif

#ifdef MIDI_READY
#include "midi.h"
#endif

#if defined(CS4280_DEBUG)
#define DPRINTF(x)	    if (cs4280debug) printf x
#define DPRINTFN(n,x)	    if (cs4280debug>(n)) printf x
int cs4280debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/cs4280reg.h>
#include <dev/pci/cs4280_image.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97.h>

#include <machine/bus.h>

#define CSCC_PCI_BA0 0x10
#define CSCC_PCI_BA1 0x14

struct cs4280_dma {
	bus_dmamap_t map;
	caddr_t addr;		/* real dma buffer */
	caddr_t dum;		/* dummy buffer for audio driver */
	bus_dma_segment_t segs[1];
	int nsegs;
	size_t size;
	struct cs4280_dma *next;
};
#define DMAADDR(p) ((p)->map->dm_segs[0].ds_addr)
#define BUFADDR(p)  ((void *)((p)->dum))
#define KERNADDR(p) ((void *)((p)->addr))

/*
 * Software state
 */
struct cs4280_softc {
	struct device	      sc_dev;

	pci_intr_handle_t *   sc_ih;

	/* I/O (BA0) */
	bus_space_tag_t	      ba0t;
	bus_space_handle_t    ba0h;
	
	/* BA1 */
	bus_space_tag_t	      ba1t;
	bus_space_handle_t    ba1h;
	
	/* DMA */
	bus_dma_tag_t	 sc_dmatag;
	struct cs4280_dma *sc_dmas;

	void	(*sc_pintr)(void *);	/* dma completion intr handler */
	void	*sc_parg;		/* arg for sc_intr() */
	char	*sc_ps, *sc_pe, *sc_pn;
	int	sc_pcount;
	int	sc_pi;
	struct	cs4280_dma *sc_pdma;
	char	*sc_pbuf;
#ifdef DIAGNOSTIC
	char	sc_prun;
#endif

	void	(*sc_rintr)(void *);	/* dma completion intr handler */
	void	*sc_rarg;		/* arg for sc_intr() */
	char	*sc_rs, *sc_re, *sc_rn;
	int	sc_rcount;
	int	sc_ri;
	struct	cs4280_dma *sc_rdma;
	char	*sc_rbuf;
	int	sc_rparam;		/* record format */
#ifdef DIAGNOSTIC
	char	sc_rrun;
#endif

#if NMIDI > 0
	void	(*sc_iintr)(void *, int); /* midi input ready handler */
	void	(*sc_ointr)(void *);	  /* midi output ready handler */
	void	*sc_arg;
#endif

	u_int32_t pctl;
	u_int32_t cctl;

	struct ac97_codec_if *codec_if;
	struct ac97_host_if host_if;	

	char	sc_suspend;
	void   *sc_powerhook;		/* Power Hook */
	u_int16_t  ac97_reg[CS4280_SAVE_REG_MAX + 1];	/* Save ac97 registers */
};

#define BA0READ4(sc, r) bus_space_read_4((sc)->ba0t, (sc)->ba0h, (r))
#define BA0WRITE4(sc, r, x) bus_space_write_4((sc)->ba0t, (sc)->ba0h, (r), (x))
#define BA1READ4(sc, r) bus_space_read_4((sc)->ba1t, (sc)->ba1h, (r))
#define BA1WRITE4(sc, r, x) bus_space_write_4((sc)->ba1t, (sc)->ba1h, (r), (x))

int	cs4280_match  __P((struct device *, void *, void *));
void	cs4280_attach __P((struct device *, struct device *, void *));
int	cs4280_intr __P((void *));
void	cs4280_reset __P((void *));
int	cs4280_download_image __P((struct cs4280_softc *));

int cs4280_download(struct cs4280_softc *, u_int32_t *, u_int32_t, u_int32_t);
int cs4280_allocmem __P((struct cs4280_softc *, size_t, size_t,
			 struct cs4280_dma *));
int cs4280_freemem __P((struct cs4280_softc *, struct cs4280_dma *));

#ifdef CS4280_DEBUG
int	cs4280_check_images   __P((struct cs4280_softc *));
int	cs4280_checkimage(struct cs4280_softc *, u_int32_t *, u_int32_t,
			  u_int32_t);
#endif

struct	cfdriver clcs_cd = {
	NULL, "clcs", DV_DULL
};

struct cfattach clcs_ca = {
	sizeof(struct cs4280_softc), cs4280_match, cs4280_attach
};

int	cs4280_init __P((struct cs4280_softc *, int));
int	cs4280_open __P((void *, int));
void	cs4280_close __P((void *));

int	cs4280_query_encoding __P((void *, struct audio_encoding *));
int	cs4280_set_params __P((void *, int, int, struct audio_params *, struct audio_params *));
int	cs4280_round_blocksize __P((void *, int));

int	cs4280_halt_output __P((void *));
int	cs4280_halt_input __P((void *));

int	cs4280_getdev __P((void *, struct audio_device *));

int	cs4280_mixer_set_port __P((void *, mixer_ctrl_t *));
int	cs4280_mixer_get_port __P((void *, mixer_ctrl_t *));
int	cs4280_query_devinfo __P((void *addr, mixer_devinfo_t *dip));
void   *cs4280_malloc __P((void *, u_long, int, int));
void	cs4280_free __P((void *, void *, int));
u_long	cs4280_round_buffersize __P((void *, u_long));
paddr_t	cs4280_mappage __P((void *, void *, off_t, int));
int	cs4280_get_props __P((void *));
int	cs4280_trigger_output __P((void *, void *, void *, int, void (*)(void *),
	    void *, struct audio_params *));
int	cs4280_trigger_input __P((void *, void *, void *, int, void (*)(void *),
	    void *, struct audio_params *));


void	cs4280_set_dac_rate  __P((struct cs4280_softc *, int ));
void	cs4280_set_adc_rate  __P((struct cs4280_softc *, int ));
int	cs4280_get_portnum_by_name __P((struct cs4280_softc *, char *, char *,
					 char *));
int	cs4280_src_wait	 __P((struct cs4280_softc *));
int	cs4280_attach_codec __P((void *sc, struct ac97_codec_if *));
int	cs4280_read_codec __P((void *sc, u_int8_t a, u_int16_t *d));
int	cs4280_write_codec __P((void *sc, u_int8_t a, u_int16_t d));
void	cs4280_reset_codec __P((void *sc));

void	cs4280_power __P((int, void *));

void	cs4280_clear_fifos __P((struct cs4280_softc *));

#if NMIDI > 0
void	cs4280_midi_close __P((void*));
void	cs4280_midi_getinfo __P((void *, struct midi_info *));
int	cs4280_midi_open __P((void *, int, void (*)(void *, int),
			      void (*)(void *), void *));
int	cs4280_midi_output __P((void *, int));
#endif

struct audio_hw_if cs4280_hw_if = {
	cs4280_open,
	cs4280_close,
	NULL,
	cs4280_query_encoding,
	cs4280_set_params,
	cs4280_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	cs4280_halt_output,
	cs4280_halt_input,
	NULL,
	cs4280_getdev,
	NULL,
	cs4280_mixer_set_port,
	cs4280_mixer_get_port,
	cs4280_query_devinfo,
	cs4280_malloc,
	cs4280_free,
	cs4280_round_buffersize,
	0, /* cs4280_mappage, */
	cs4280_get_props,
	cs4280_trigger_output,
	cs4280_trigger_input,
};

#if NMIDI > 0
struct midi_hw_if cs4280_midi_hw_if = {
	cs4280_midi_open,
	cs4280_midi_close,
	cs4280_midi_output,
	cs4280_midi_getinfo,
	0,
};
#endif

	

struct audio_device cs4280_device = {
	"CS4280",
	"",
	"cs4280"
};


int
cs4280_match(parent, ma, aux) 
	struct device *parent;
	void *ma;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_CIRRUS)
		return (0);
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CIRRUS_CS4280 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CIRRUS_CS4610 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CIRRUS_CS4614 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CIRRUS_CS4615) {
		return (1);
	}
	return (0);
}

int
cs4280_read_codec(sc_, add, data)
	void *sc_;
	u_int8_t add;
	u_int16_t *data;
{
	struct cs4280_softc *sc = sc_;
	int n;
	
	DPRINTFN(5,("read_codec: add=0x%02x ", add));
	/* 
	 * Make sure that there is not data sitting around from a preivous
	 * uncompleted access.
	 */
	BA0READ4(sc, CS4280_ACSDA);

	/* Set up AC97 control registers. */
	BA0WRITE4(sc, CS4280_ACCAD, add);
	BA0WRITE4(sc, CS4280_ACCDA, 0);
	BA0WRITE4(sc, CS4280_ACCTL, 
	    ACCTL_RSTN | ACCTL_ESYN | ACCTL_VFRM | ACCTL_CRW  | ACCTL_DCV );

	if (cs4280_src_wait(sc) < 0) {
		printf("%s: AC97 read prob. (DCV!=0) for add=0x%0x\n",
		       sc->sc_dev.dv_xname, add);
		return (1);
	}

	/* wait for valid status bit is active */
	n = 0;
	while (!(BA0READ4(sc, CS4280_ACSTS) & ACSTS_VSTS)) {
		delay(1);
		while (++n > 1000) {
			printf("%s: AC97 read fail (VSTS==0) for add=0x%0x\n", 
			       sc->sc_dev.dv_xname, add);
			return (1);
		}
	}
	*data = BA0READ4(sc, CS4280_ACSDA);
	DPRINTFN(5,("data=0x%04x\n", *data));
	return (0);
}

int
cs4280_write_codec(sc_, add, data)
	void *sc_;
	u_int8_t add;
	u_int16_t data;
{
	struct cs4280_softc *sc = sc_;

	DPRINTFN(5,("write_codec: add=0x%02x  data=0x%04x\n", add, data));
	BA0WRITE4(sc, CS4280_ACCAD, add);
	BA0WRITE4(sc, CS4280_ACCDA, data);
	BA0WRITE4(sc, CS4280_ACCTL, 
	    ACCTL_RSTN | ACCTL_ESYN | ACCTL_VFRM | ACCTL_DCV );

	if (cs4280_src_wait(sc) < 0) {
		printf("%s: AC97 write fail (DCV!=0) for add=0x%02x data="
		       "0x%04x\n", sc->sc_dev.dv_xname, add, data);
		return (1);
	}
	return (0);
}

int
cs4280_src_wait(sc)
	struct cs4280_softc *sc;
{
	int n;
	n = 0;
	while ((BA0READ4(sc, CS4280_ACCTL) & ACCTL_DCV)) {
		delay(1000);
		while (++n > 1000)
			return (-1);
	}
	return (0);
}


void
cs4280_set_adc_rate(sc, rate)
	struct cs4280_softc *sc;
	int rate;
{
	/* calculate capture rate:
	 *
	 * capture_coefficient_increment = -round(rate*128*65536/48000;
	 * capture_phase_increment	 = floor(48000*65536*1024/rate);
	 * cx = round(48000*65536*1024 - capture_phase_increment*rate);
	 * cy = floor(cx/200);
	 * capture_sample_rate_correction = cx - 200*cy;
	 * capture_delay = ceil(24*48000/rate);
	 * capture_num_triplets = floor(65536*rate/24000);
	 * capture_group_length = 24000/GCD(rate, 24000);
	 * where GCD means "Greatest Common Divisor".
	 *
	 * capture_coefficient_increment, capture_phase_increment and
	 * capture_num_triplets are 32-bit signed quantities.
	 * capture_sample_rate_correction and capture_group_length are
	 * 16-bit signed quantities.
	 * capture_delay is a 14-bit unsigned quantity.
	 */
	u_int32_t cci,cpi,cnt,cx,cy,  tmp1;
	u_int16_t csrc, cgl, cdlay;
	
	/* XXX
	 * Even though, embedded_audio_spec says capture rate range 11025 to
	 * 48000, dhwiface.cpp says,
	 *
	 * "We can only decimate by up to a factor of 1/9th the hardware rate.
	 *  Return an error if an attempt is made to stray outside that limit."
	 *
	 * so assume range as 48000/9 to 48000
	 */ 

	if (rate < 8000)
		rate = 8000;
	if (rate > 48000)
		rate = 48000;

	cx = rate << 16;
	cci = cx / 48000;
	cx -= cci * 48000;
	cx <<= 7;
	cci <<= 7;
	cci += cx / 48000;
	cci = - cci;

	cx = 48000 << 16;
	cpi = cx / rate;
	cx -= cpi * rate;
	cx <<= 10;
	cpi <<= 10;
	cy = cx / rate;
	cpi += cy;
	cx -= cy * rate;

	cy   = cx / 200;
	csrc = cx - 200*cy;

	cdlay = ((48000 * 24) + rate - 1) / rate;
#if 0
	cdlay &= 0x3fff; /* make sure cdlay is 14-bit */
#endif

	cnt  = rate << 16;
	cnt  /= 24000;

	cgl = 1;
	for (tmp1 = 2; tmp1 <= 64; tmp1 *= 2) {
		if (((rate / tmp1) * tmp1) != rate)
			cgl *= 2;
	}
	if (((rate / 3) * 3) != rate)
		cgl *= 3;
	for (tmp1 = 5; tmp1 <= 125; tmp1 *= 5) {
		if (((rate / tmp1) * tmp1) != rate) 
			cgl *= 5;
	}
#if 0
	/* XXX what manual says */
	tmp1 = BA1READ4(sc, CS4280_CSRC) & ~CSRC_MASK;
	tmp1 |= csrc<<16;
	BA1WRITE4(sc, CS4280_CSRC, tmp1);
#else
	/* suggested by cs461x.c (ALSA driver) */
	BA1WRITE4(sc, CS4280_CSRC, CS4280_MK_CSRC(csrc, cy));
#endif

#if 0
	/* I am confused.  The sample rate calculation section says
	 * cci *is* 32-bit signed quantity but in the parameter description
	 * section, CCI only assigned 16bit.
	 * I believe size of the variable.
	 */
	tmp1 = BA1READ4(sc, CS4280_CCI) & ~CCI_MASK;
	tmp1 |= cci<<16;
	BA1WRITE4(sc, CS4280_CCI, tmp1);
#else
	BA1WRITE4(sc, CS4280_CCI, cci);
#endif

	tmp1 = BA1READ4(sc, CS4280_CD) & ~CD_MASK;
	tmp1 |= cdlay <<18;
	BA1WRITE4(sc, CS4280_CD, tmp1);
	
	BA1WRITE4(sc, CS4280_CPI, cpi);
	
	tmp1 = BA1READ4(sc, CS4280_CGL) & ~CGL_MASK;
	tmp1 |= cgl;
	BA1WRITE4(sc, CS4280_CGL, tmp1);

	BA1WRITE4(sc, CS4280_CNT, cnt);
	
	tmp1 = BA1READ4(sc, CS4280_CGC) & ~CGC_MASK;
	tmp1 |= cgl;
	BA1WRITE4(sc, CS4280_CGC, tmp1);
}

void
cs4280_set_dac_rate(sc, rate)
	struct cs4280_softc *sc;
	int rate;
{
	/*
	 * playback rate may range from 8000Hz to 48000Hz
	 *
	 * play_phase_increment = floor(rate*65536*1024/48000)
	 * px = round(rate*65536*1024 - play_phase_incremnt*48000)
	 * py=floor(px/200)
	 * play_sample_rate_correction = px - 200*py
	 *
	 * play_phase_increment is a 32bit signed quantity.
	 * play_sample_rate_correction is a 16bit signed quantity.
	 */
	int32_t ppi;
	int16_t psrc;
	u_int32_t px, py;
	
	if (rate < 8000)
		rate = 8000;
	if (rate > 48000)
		rate = 48000;
	px = rate << 16;
	ppi = px/48000;
	px -= ppi*48000;
	ppi <<= 10;
	px  <<= 10;
	py  = px / 48000;
	ppi += py;
	px -= py*48000;
	py  = px/200;
	px -= py*200;
	psrc = px;
#if 0
	/* what manual says */
	px = BA1READ4(sc, CS4280_PSRC) & ~PSRC_MASK;
	BA1WRITE4(sc, CS4280_PSRC,
			  ( ((psrc<<16) & PSRC_MASK) | px ));
#else	
	/* suggested by cs461x.c (ALSA driver) */
	BA1WRITE4(sc, CS4280_PSRC, CS4280_MK_PSRC(psrc,py));
#endif
	BA1WRITE4(sc, CS4280_PPI, ppi);
}

void
cs4280_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct cs4280_softc *sc = (struct cs4280_softc *) self;
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	char const *intrstr;
	pci_intr_handle_t ih;
	mixer_ctrl_t ctl;
	u_int32_t mem;
    
	/* Map I/O register */
	if (pci_mapreg_map(pa, CSCC_PCI_BA0, 
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->ba0t, &sc->ba0h, NULL, NULL)) {
		printf(": can't map BA0 space\n");
		return;
	}
	if (pci_mapreg_map(pa, CSCC_PCI_BA1,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->ba1t, &sc->ba1h, NULL, NULL)) {
		printf(": can't map BA1 space\n");
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;

	/* Enable the device (set bus master flag) */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	   pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	   PCI_COMMAND_MASTER_ENABLE);

	/* LATENCY_TIMER setting */
	mem = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	if ( PCI_LATTIMER(mem) < 32 ) {
		mem &= 0xffff00ff;
		mem |= 0x00002000;
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, mem);
	}
	
	/* Map and establish the interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);

	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, cs4280_intr, sc,
				       sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(" %s\n", intrstr);

	/* Initialization */
	if(cs4280_init(sc, 1) != 0)
		return;

	/* AC 97 attachement */
	sc->host_if.arg = sc;
	sc->host_if.attach = cs4280_attach_codec;
	sc->host_if.read   = cs4280_read_codec;
	sc->host_if.write  = cs4280_write_codec;
	sc->host_if.reset  = cs4280_reset_codec;

	if (ac97_attach(&sc->host_if) != 0) {
		printf("%s: ac97_attach failed\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Turn mute off of DAC, CD and master volumes by default */
	ctl.type = AUDIO_MIXER_ENUM;
	ctl.un.ord = 0;	 /* off */

	ctl.dev = cs4280_get_portnum_by_name(sc, AudioCoutputs,
					     AudioNmaster, AudioNmute);
	cs4280_mixer_set_port(sc, &ctl);

	ctl.dev = cs4280_get_portnum_by_name(sc, AudioCinputs,
					     AudioNdac, AudioNmute);
	cs4280_mixer_set_port(sc, &ctl);

	ctl.dev = cs4280_get_portnum_by_name(sc, AudioCinputs,
					     AudioNcd, AudioNmute);
	cs4280_mixer_set_port(sc, &ctl);
	
	audio_attach_mi(&cs4280_hw_if, sc, &sc->sc_dev);

#if NMIDI > 0
	midi_attach_mi(&cs4280_midi_hw_if, sc, &sc->sc_dev);
#endif
	sc->sc_suspend = PWR_RESUME;
	sc->sc_powerhook = powerhook_establish(cs4280_power, sc);
}

int
cs4280_intr(p)
	void *p;
{
	/*
	 * XXX
	 *
	 * Since CS4280 has only 4kB dma buffer and
	 * interrupt occurs every 2kB block, I create dummy buffer
	 * which returns to audio driver and actual dma buffer
	 * using in DMA transfer.
	 *
	 *
	 *  ring buffer in audio.c is pointed by BUFADDR
	 *	 <------ ring buffer size == 64kB ------>
	 *	 <-----> blksize == 2048*(sc->sc_[pr]count) kB 
	 *	|= = = =|= = = =|= = = =|= = = =|= = = =|
	 *	|	|	|	|	|	| <- call audio_intp every
	 *						     sc->sc_[pr]_count time.
	 *
	 *  actual dma buffer is pointed by KERNADDR
	 *	 <-> dma buffer size = 4kB
	 *	|= =|
	 *
	 *
	 */
	struct cs4280_softc *sc = p;
	u_int32_t intr, mem;
	char * empty_dma;
	int handled = 0;

	/* grab interrupt register then clear it */
	intr = BA0READ4(sc, CS4280_HISR);
	BA0WRITE4(sc, CS4280_HICR, HICR_CHGM | HICR_IEV);

	/* Playback Interrupt */
	if (intr & HISR_PINT) {
		handled = 1;
		mem = BA1READ4(sc, CS4280_PFIE);
		BA1WRITE4(sc, CS4280_PFIE, (mem & ~PFIE_PI_MASK) | PFIE_PI_DISABLE);
		if (sc->sc_pintr) {
			if ((sc->sc_pi%sc->sc_pcount) == 0)
				sc->sc_pintr(sc->sc_parg);
		} else {
			printf("unexpected play intr\n");
		}
		/* copy buffer */
		++sc->sc_pi;
		empty_dma = sc->sc_pdma->addr;
		if (sc->sc_pi&1)
			empty_dma += CS4280_ICHUNK;
		memcpy(empty_dma, sc->sc_pn, CS4280_ICHUNK);
		sc->sc_pn += CS4280_ICHUNK;
		if (sc->sc_pn >= sc->sc_pe)
			sc->sc_pn = sc->sc_ps;
		BA1WRITE4(sc, CS4280_PFIE, mem);
	}
	/* Capture Interrupt */
	if (intr & HISR_CINT) {
		int  i;
		int16_t rdata;

		handled = 1;
		mem = BA1READ4(sc, CS4280_CIE);
		BA1WRITE4(sc, CS4280_CIE, (mem & ~CIE_CI_MASK) | CIE_CI_DISABLE);
		++sc->sc_ri;
		empty_dma = sc->sc_rdma->addr;
		if ((sc->sc_ri&1) == 0)
			empty_dma += CS4280_ICHUNK;

		/*
		 * XXX
		 * I think this audio data conversion should be
		 * happend in upper layer, but I put this here
		 * since there is no conversion function available.
		 */
		switch(sc->sc_rparam) {
		case CF_16BIT_STEREO:
			/* just copy it */
			memcpy(sc->sc_rn, empty_dma, CS4280_ICHUNK);
			sc->sc_rn += CS4280_ICHUNK;
			break;
		case CF_16BIT_MONO:
			for (i = 0; i < 512; i++) {
				rdata  = *((int16_t *)empty_dma)++>>1;
				rdata += *((int16_t *)empty_dma)++>>1;
				*((int16_t *)sc->sc_rn)++ = rdata;
			}
			break;
		case CF_8BIT_STEREO:
			for (i = 0; i < 512; i++) {
				rdata = *((int16_t*)empty_dma)++;
				*sc->sc_rn++ = rdata >> 8;
				rdata = *((int16_t*)empty_dma)++;
				*sc->sc_rn++ = rdata >> 8;
			}
			break;
		case CF_8BIT_MONO:
			for (i = 0; i < 512; i++) {
				rdata =	 *((int16_t*)empty_dma)++ >>1;
				rdata += *((int16_t*)empty_dma)++ >>1;
				*sc->sc_rn++ = rdata >>8;
			}
			break;
		default:
			/* Should not reach here */
			printf("unknown sc->sc_rparam: %d\n", sc->sc_rparam);
		}
		if (sc->sc_rn >= sc->sc_re)
			sc->sc_rn = sc->sc_rs;
		BA1WRITE4(sc, CS4280_CIE, mem);
		if (sc->sc_rintr) {
			if ((sc->sc_ri%(sc->sc_rcount)) == 0)
				sc->sc_rintr(sc->sc_rarg);
		} else {
			printf("unexpected record intr\n");
		}
	}

#if NMIDI > 0
	/* Midi port Interrupt */
	if (intr & HISR_MIDI) {
		int data;

		handled = 1;
		DPRINTF(("i: %d: ", 
			 BA0READ4(sc, CS4280_MIDSR)));
		/* Read the received data */
		while ((sc->sc_iintr != NULL) &&
		       ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_RBE) == 0)) {
			data = BA0READ4(sc, CS4280_MIDRP) & MIDRP_MASK;
			DPRINTF(("r:%x\n",data));
			sc->sc_iintr(sc->sc_arg, data);
		}
		
		/* Write the data */
#if 1
		/* XXX:
		 * It seems "Transmit Buffer Full" never activate until EOI
		 * is deliverd.  Shall I throw EOI top of this routine ?
		 */
		if ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_TBF) == 0) {
			DPRINTF(("w: "));
			if (sc->sc_ointr != NULL)
				sc->sc_ointr(sc->sc_arg);
		}
#else
		while ((sc->sc_ointr != NULL) && 
		       ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_TBF) == 0)) {
			DPRINTF(("w: "));
			sc->sc_ointr(sc->sc_arg);
		}
#endif
		DPRINTF(("\n"));
	}
#endif

	return handled;
}


/* Download Proceessor Code and Data image */

int
cs4280_download(sc, src, offset, len)
	struct cs4280_softc *sc;
	u_int32_t *src;
	u_int32_t offset, len;
{
	u_int32_t ctr;

#ifdef CS4280_DEBUG
	u_int32_t con, data;
	u_int8_t c0,c1,c2,c3;
#endif
	if ((offset&3) || (len&3))
		return (-1);

	len /= sizeof(u_int32_t);
	for (ctr = 0; ctr < len; ctr++) {
		/* XXX:
		 * I cannot confirm this is the right thing or not
		 * on BIG-ENDIAN machines.
		 */
		BA1WRITE4(sc, offset+ctr*4, htole32(*(src+ctr)));
#ifdef CS4280_DEBUG
		data = htole32(*(src+ctr));
		c0 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+0);
		c1 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+1);
		c2 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+2);
		c3 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+3);
		con = ( (c3<<24) | (c2<<16) | (c1<<8) | c0 );
		if (data != con ) {
			printf("0x%06x: write=0x%08x read=0x%08x\n",
			       offset+ctr*4, data, con);
			return (-1);
		}
#endif
	}
	return (0);
}

int
cs4280_download_image(sc)
	struct cs4280_softc *sc;
{
	int idx, err;
	u_int32_t offset = 0;

	err = 0;

	for (idx = 0; idx < BA1_MEMORY_COUNT; ++idx) {
		err = cs4280_download(sc, &BA1Struct.map[offset],
				  BA1Struct.memory[idx].offset,
				  BA1Struct.memory[idx].size);
		if (err != 0) {
			printf("%s: load_image failed at %d\n",
			       sc->sc_dev.dv_xname, idx);
			return (-1);
		}
		offset += BA1Struct.memory[idx].size / sizeof(u_int32_t);
	}
	return (err);
}

#ifdef CS4280_DEBUG
int
cs4280_checkimage(sc, src, offset, len)
	struct cs4280_softc *sc;
	u_int32_t *src;
	u_int32_t offset, len;
{
	u_int32_t ctr, data;
	int err = 0;

	if ((offset&3) || (len&3))
		return -1;

	len /= sizeof(u_int32_t);
	for (ctr = 0; ctr < len; ctr++) {
		/* I cannot confirm this is the right thing
		 * on BIG-ENDIAN machines
		 */
		data = BA1READ4(sc, offset+ctr*4);
		if (data != htole32(*(src+ctr))) {
			printf("0x%06x: 0x%08x(0x%08x)\n",
			       offset+ctr*4, data, *(src+ctr));
			*(src+ctr) = data;
			++err;
		}
	}
	return (err);
}

int
cs4280_check_images(sc)
	struct cs4280_softc *sc;
{
	int idx, err;
	u_int32_t offset = 0;

	err = 0;
	/*for (idx=0; idx < BA1_MEMORY_COUNT; ++idx) { */
	for (idx = 0; idx < 1; ++idx) {
		err = cs4280_checkimage(sc, &BA1Struct.map[offset],
				      BA1Struct.memory[idx].offset,
				      BA1Struct.memory[idx].size);
		if (err != 0) {
			printf("%s: check_image failed at %d\n",
			       sc->sc_dev.dv_xname, idx);
		}
		offset += BA1Struct.memory[idx].size / sizeof(u_int32_t);
	}
	return (err);
}

#endif

int
cs4280_attach_codec(sc_, codec_if)
	void *sc_;
	struct ac97_codec_if *codec_if;
{
	struct cs4280_softc *sc = sc_;

	sc->codec_if = codec_if;
	return (0);
}

void
cs4280_reset_codec(sc_)
	void *sc_;
{
	struct cs4280_softc *sc = sc_;
	int n;

	/* Reset codec */
	BA0WRITE4(sc, CS4280_ACCTL, 0);
	delay(100);    /* delay 100us */
	BA0WRITE4(sc, CS4280_ACCTL, ACCTL_RSTN);

	/* 
	 * It looks like we do the following procedure, too
	 */

	/* Enable AC-link sync generation */
	BA0WRITE4(sc, CS4280_ACCTL, ACCTL_ESYN | ACCTL_RSTN);
	delay(50*1000); /* XXX delay 50ms */
	
	/* Assert valid frame signal */
	BA0WRITE4(sc, CS4280_ACCTL, ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);

	/* Wait for valid AC97 input slot */
	n = 0;
	while (BA0READ4(sc, CS4280_ACISV) != (ACISV_ISV3 | ACISV_ISV4)) {
		delay(1000);
		if (++n > 1000) {
			printf("reset_codec: AC97 inputs slot ready timeout\n");
			return;
		}
	}
}


/* Processor Soft Reset */
void
cs4280_reset(sc_)
	void *sc_;
{
	struct cs4280_softc *sc = sc_;

	/* Set RSTSP bit in SPCR (also clear RUN, RUNFR, and DRQEN) */
	BA1WRITE4(sc, CS4280_SPCR, SPCR_RSTSP);
	delay(100);
	/* Clear RSTSP bit in SPCR */
	BA1WRITE4(sc, CS4280_SPCR, 0);
	/* enable DMA reqest */
	BA1WRITE4(sc, CS4280_SPCR, SPCR_DRQEN);
}

int
cs4280_open(addr, flags)
	void *addr;
	int flags;
{
	return (0);
}

void
cs4280_close(addr)
	void *addr;
{
	struct cs4280_softc *sc = addr;
	
	cs4280_halt_output(sc);
	cs4280_halt_input(sc);
	
	sc->sc_pintr = 0;
	sc->sc_rintr = 0;
}

int
cs4280_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

int
cs4280_set_params(addr, setmode, usemode, play, rec)
	void *addr;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct cs4280_softc *sc = addr;
	struct audio_params *p;
	int mode;

	for (mode = AUMODE_RECORD; mode != -1;
	    mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1 ) {
		if ((setmode & mode) == 0)
			continue;
		
		p = mode == AUMODE_PLAY ? play : rec;
		
		if (p == play) {
			DPRINTFN(5,("play: sample=%ld precision=%d channels=%d\n",
				p->sample_rate, p->precision, p->channels));
			/* play back data format may be 8- or 16-bit and
			 * either stereo or mono.
			 * playback rate may range from 8000Hz to 48000Hz 
			 */
			if (p->sample_rate < 8000 || p->sample_rate > 48000 ||
			    (p->precision != 8 && p->precision != 16) ||
			    (p->channels != 1  && p->channels != 2) ) {
				return (EINVAL);
			}
		} else {
			DPRINTFN(5,("rec: sample=%ld precision=%d channels=%d\n",
				p->sample_rate, p->precision, p->channels));
			/* capture data format must be 16bit stereo
			 * and sample rate range from 11025Hz to 48000Hz.
			 *
			 * XXX: it looks like to work with 8000Hz,
			 *	although data sheets say lower limit is
			 *	11025 Hz.
			 */

			if (p->sample_rate < 8000 || p->sample_rate > 48000 ||
			    (p->precision != 8 && p->precision != 16) ||
			    (p->channels  != 1 && p->channels  != 2) ) {
				return (EINVAL);
			}
		}
		p->factor  = 1;
		p->sw_code = 0;

		/* capturing data is slinear */
		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_BE:
			if (mode == AUMODE_RECORD) {
				if (p->precision == 16)
					p->sw_code = swap_bytes;
			}
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (mode == AUMODE_RECORD) {
				if (p->precision == 16)
					p->sw_code = change_sign16_swap_bytes;
				else
					p->sw_code = change_sign8;
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (mode == AUMODE_RECORD) {
				if (p->precision == 16)
					p->sw_code = change_sign16;
				else
					p->sw_code = change_sign8;
			}
			break;
		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_slinear16;
			} else {
				p->sw_code = slinear8_to_mulaw;
			}
			break;
		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_slinear16;
			} else {
				p->sw_code = slinear8_to_alaw;
			}
			break;
		default:
			return (EINVAL);
		}
	}

	/* set sample rate */
	cs4280_set_dac_rate(sc, play->sample_rate);
	cs4280_set_adc_rate(sc, rec->sample_rate);
	return (0);
}

int
cs4280_round_blocksize(hdl, blk)
	void *hdl;
	int blk;
{
	return (blk < CS4280_ICHUNK ? CS4280_ICHUNK : blk & -CS4280_ICHUNK);
}

u_long
cs4280_round_buffersize(addr, size)
	void *addr;
	u_long size;
{
	/* although real dma buffer size is 4KB, 
	 * let the audio.c driver use a larger buffer.
	 * ( suggested by Lennart Augustsson. )
	 */
	return (size);
}

int
cs4280_get_props(hdl)
	void *hdl;
{
	return (AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX); 
#ifdef notyet
	/* XXX 
	 * How can I mmap ?
	 */
		AUDIO_PROP_MMAP 
#endif
	    
}

int
cs4280_mixer_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct cs4280_softc *sc = addr;

	return (sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp));
}

paddr_t
cs4280_mappage(addr, mem, off, prot)
	void *addr;
	void *mem;
	off_t off;
	int prot;
{
	struct cs4280_softc *sc = addr;
	struct cs4280_dma *p;

	if (off < 0)
		return (-1);
	for (p = sc->sc_dmas; p && BUFADDR(p) != mem; p = p->next)
		;
	if (!p) {
		DPRINTF(("cs4280_mappage: bad buffer address\n"));
		return (-1);
	}
	return (bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs, 
				off, prot, BUS_DMA_WAITOK));
}


int
cs4280_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	struct cs4280_softc *sc = addr;

	return (sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip));
}

int
cs4280_get_portnum_by_name(sc, class, device, qualifier)
	struct cs4280_softc *sc;
	char *class, *device, *qualifier;
{
	return (sc->codec_if->vtbl->get_portnum_by_name(sc->codec_if, class,
	     device, qualifier));
}

int
cs4280_halt_output(addr)
	void *addr;
{
	struct cs4280_softc *sc = addr;
	u_int32_t mem;
	
	mem = BA1READ4(sc, CS4280_PCTL);
	BA1WRITE4(sc, CS4280_PCTL, mem & ~PCTL_MASK);
#ifdef DIAGNOSTIC
	sc->sc_prun = 0;
#endif
	return (0);
}

int
cs4280_halt_input(addr)
	void *addr;
{
	struct cs4280_softc *sc = addr;
	u_int32_t mem;

	mem = BA1READ4(sc, CS4280_CCTL);
	BA1WRITE4(sc, CS4280_CCTL, mem & ~CCTL_MASK);
#ifdef DIAGNOSTIC
	sc->sc_rrun = 0;
#endif
	return (0);
}

int
cs4280_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = cs4280_device;
	return (0);
}

int
cs4280_mixer_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct cs4280_softc *sc = addr;
	int val;

	val = sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
	DPRINTFN(3,("mixer_set_port: val=%d\n", val));
	return (val);
}


int
cs4280_freemem(sc, p)
	struct cs4280_softc *sc;
	struct cs4280_dma *p;
{
	bus_dmamap_unload(sc->sc_dmatag, p->map);
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return (0);
}

int
cs4280_allocmem(sc, size, align, p)
	struct cs4280_softc *sc;
	size_t size;
	size_t align;
	struct cs4280_dma *p;
{
	int error;

	/* XXX */
	p->size = size;
	error = bus_dmamem_alloc(sc->sc_dmatag, p->size, align, 0,
				 p->segs, sizeof(p->segs)/sizeof(p->segs[0]),
				 &p->nsegs, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to allocate dma, error=%d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}
	
	error = bus_dmamem_map(sc->sc_dmatag, p->segs, p->nsegs, p->size, 
			       &p->addr, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		printf("%s: unable to map dma, error=%d\n",
		       sc->sc_dev.dv_xname, error);
		goto free;
	}

	error = bus_dmamap_create(sc->sc_dmatag, p->size, 1, p->size,
				  0, BUS_DMA_NOWAIT, &p->map);
	if (error) {
		printf("%s: unable to create dma map, error=%d\n",
		       sc->sc_dev.dv_xname, error);
		goto unmap;
	}

	error = bus_dmamap_load(sc->sc_dmatag, p->map, p->addr, p->size, NULL, 
				BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to load dma map, error=%d\n",
		       sc->sc_dev.dv_xname, error);
		goto destroy;
	}
	return (0);

destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return (error);
}


void *
cs4280_malloc(addr, size, pool, flags)
	void *addr;
	u_long size;
	int pool, flags;
{
	struct cs4280_softc *sc = addr;
	struct cs4280_dma *p;
	caddr_t q;
	int error;
	
	DPRINTFN(5,("cs4280_malloc: size=%d pool=%d flags=%d\n", size, pool, flags));
	q = malloc(size, pool, flags);
	if (!q) 
		return (0);
	p = malloc(sizeof(*p), pool, flags);
	if (!p) {
		free(q,pool);
		return (0);
	}
	/* 
	 * cs4280 has fixed 4kB buffer
	 */
	error = cs4280_allocmem(sc, CS4280_DCHUNK, CS4280_DALIGN, p);

	if (error) {
		free(q, pool);
		free(p, pool);
		return (0);
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	p->dum = q; /* return to audio driver */

	return (p->dum);
}

void
cs4280_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	struct cs4280_softc *sc = addr;
	struct cs4280_dma **pp, *p;
	
	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (BUFADDR(p) == ptr) {
			cs4280_freemem(sc, p);
			*pp = p->next;
			free(p->dum, pool);
			free(p, pool);
			return;
		}
	}
}

int
cs4280_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct cs4280_softc *sc = addr;
	u_int32_t pfie, pctl, mem, pdtc;
	struct cs4280_dma *p;
	
#ifdef DIAGNOSTIC
	if (sc->sc_prun)
		printf("cs4280_trigger_output: already running\n");
	sc->sc_prun = 1;
#endif

	DPRINTF(("cs4280_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc->sc_pintr = intr;
	sc->sc_parg  = arg;

	/* stop playback DMA */
	mem = BA1READ4(sc, CS4280_PCTL);
	BA1WRITE4(sc, CS4280_PCTL, mem & ~PCTL_MASK);

	/* setup PDTC */
	pdtc = BA1READ4(sc, CS4280_PDTC);
	pdtc &= ~PDTC_MASK;
	pdtc |= CS4280_MK_PDTC(param->precision * param->channels);
	BA1WRITE4(sc, CS4280_PDTC, pdtc);
	
	DPRINTF(("param: precision=%d  factor=%d channels=%d encoding=%d\n",
	       param->precision, param->factor, param->channels,
	       param->encoding));
	for (p = sc->sc_dmas; p != NULL && BUFADDR(p) != start; p = p->next)
		;
	if (p == NULL) {
		printf("cs4280_trigger_output: bad addr %p\n", start);
		return (EINVAL);
	}
	if (DMAADDR(p) % CS4280_DALIGN != 0 ) {
		printf("cs4280_trigger_output: DMAADDR(p)=0x%lx does not start"
		       "4kB align\n", DMAADDR(p));
		return (EINVAL);
	}

	sc->sc_pcount = blksize / CS4280_ICHUNK; /* CS4280_ICHUNK is fixed hardware blksize*/
	sc->sc_ps = (char *)start;
	sc->sc_pe = (char *)end;
	sc->sc_pdma = p;
	sc->sc_pbuf = KERNADDR(p);
	sc->sc_pi = 0;
	sc->sc_pn = sc->sc_ps;
	if (blksize >= CS4280_DCHUNK) {
		sc->sc_pn = sc->sc_ps + CS4280_DCHUNK;
		memcpy(sc->sc_pbuf, start, CS4280_DCHUNK);
		++sc->sc_pi;
	} else {
		sc->sc_pn = sc->sc_ps + CS4280_ICHUNK;
		memcpy(sc->sc_pbuf, start, CS4280_ICHUNK);
	}

	/* initiate playback dma */
	BA1WRITE4(sc, CS4280_PBA, DMAADDR(p));

	/* set PFIE */
	pfie = BA1READ4(sc, CS4280_PFIE) & ~PFIE_MASK;

	if (param->precision * param->factor == 8)
		pfie |= PFIE_8BIT;
	if (param->channels == 1)
		pfie |= PFIE_MONO;

	if (param->encoding == AUDIO_ENCODING_ULINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_BE)
		pfie |= PFIE_SWAPPED;
	if (param->encoding == AUDIO_ENCODING_ULINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_ULINEAR_LE)
		pfie |= PFIE_UNSIGNED;

	BA1WRITE4(sc, CS4280_PFIE, pfie | PFIE_PI_ENABLE);

	cs4280_set_dac_rate(sc, param->sample_rate);

	pctl = BA1READ4(sc, CS4280_PCTL) & ~PCTL_MASK;
	pctl |= sc->pctl;
	BA1WRITE4(sc, CS4280_PCTL, pctl);
	return (0);
}

int
cs4280_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct cs4280_softc *sc = addr;
	u_int32_t cctl, cie;
	struct cs4280_dma *p;
	
#ifdef DIAGNOSTIC
	if (sc->sc_rrun)
		printf("cs4280_trigger_input: already running\n");
	sc->sc_rrun = 1;
#endif
	DPRINTF(("cs4280_trigger_input: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc->sc_rintr = intr;
	sc->sc_rarg  = arg;

	sc->sc_ri = 0;
	sc->sc_rcount = blksize / CS4280_ICHUNK; /* CS4280_ICHUNK is fixed hardware blksize*/
	sc->sc_rs = (char *)start;
	sc->sc_re = (char *)end;
	sc->sc_rn = sc->sc_rs;

	/* setup format information for internal converter */
	sc->sc_rparam = 0;
	if (param->precision == 8) {
		sc->sc_rparam += CF_8BIT;
		sc->sc_rcount <<= 1;
	}
	if (param->channels  == 1) {
		sc->sc_rparam += CF_MONO;
		sc->sc_rcount <<= 1;
	}

	/* stop capture DMA */
	cctl = BA1READ4(sc, CS4280_CCTL) & ~CCTL_MASK;
	BA1WRITE4(sc, CS4280_CCTL, cctl);
	
	for (p = sc->sc_dmas; p && BUFADDR(p) != start; p = p->next)
		;
	if (!p) {
		printf("cs4280_trigger_input: bad addr %p\n", start);
		return (EINVAL);
	}
	if (DMAADDR(p) % CS4280_DALIGN != 0) {
		printf("cs4280_trigger_input: DMAADDR(p)=0x%lx does not start"
		       "4kB align\n", DMAADDR(p));
		return (EINVAL);
	}
	sc->sc_rdma = p;
	sc->sc_rbuf = KERNADDR(p);
	
	/* initiate capture dma */
	BA1WRITE4(sc, CS4280_CBA, DMAADDR(p));

	/* set CIE */
	cie = BA1READ4(sc, CS4280_CIE) & ~CIE_CI_MASK;
	BA1WRITE4(sc, CS4280_CIE, cie | CIE_CI_ENABLE);

	cs4280_set_adc_rate(sc, param->sample_rate);

	cctl = BA1READ4(sc, CS4280_CCTL) & ~CCTL_MASK;
	cctl |= sc->cctl;
	BA1WRITE4(sc, CS4280_CCTL, cctl);
	return (0);
}


int
cs4280_init(sc, init)
	struct cs4280_softc *sc;
	int init;
{
	int n;
	u_int32_t mem;

	/* Start PLL out in known state */
	BA0WRITE4(sc, CS4280_CLKCR1, 0);
	/* Start serial ports out in known state */
	BA0WRITE4(sc, CS4280_SERMC1, 0);

	/* Specify type of CODEC */
/* XXX should no be here */
#define SERACC_CODEC_TYPE_1_03
#ifdef	SERACC_CODEC_TYPE_1_03
	BA0WRITE4(sc, CS4280_SERACC, SERACC_HSP | SERACC_CTYPE_1_03); /* AC 97 1.03 */
#else
	BA0WRITE4(sc, CS4280_SERACC, SERACC_HSP | SERACC_CTYPE_2_0);  /* AC 97 2.0 */
#endif

	/* Reset codec */
	BA0WRITE4(sc, CS4280_ACCTL, 0);
	delay(100);    /* delay 100us */
	BA0WRITE4(sc, CS4280_ACCTL, ACCTL_RSTN);
	
	/* Enable AC-link sync generation */
	BA0WRITE4(sc, CS4280_ACCTL, ACCTL_ESYN | ACCTL_RSTN);
	delay(50*1000); /* delay 50ms */

	/* Set the serial port timing configuration */
	BA0WRITE4(sc, CS4280_SERMC1, SERMC1_PTC_AC97);
	
	/* Setup clock control */
	BA0WRITE4(sc, CS4280_PLLCC, PLLCC_CDR_STATE|PLLCC_LPF_STATE);
	BA0WRITE4(sc, CS4280_PLLM, PLLM_STATE);
	BA0WRITE4(sc, CS4280_CLKCR2, CLKCR2_PDIVS_8);
	
	/* Power up the PLL */
	BA0WRITE4(sc, CS4280_CLKCR1, CLKCR1_PLLP);
	delay(50*1000); /* delay 50ms */
	
	/* Turn on clock */
	mem = BA0READ4(sc, CS4280_CLKCR1) | CLKCR1_SWCE;
	BA0WRITE4(sc, CS4280_CLKCR1, mem);
	
	/* Set the serial port FIFO pointer to the
	 * first sample in FIFO. (not documented) */
	cs4280_clear_fifos(sc);

#if 0
	/* Set the serial port FIFO pointer to the first sample in the FIFO */
	BA0WRITE4(sc, CS4280_SERBSP, 0);
#endif
	
	/* Configure the serial port */
	BA0WRITE4(sc, CS4280_SERC1,  SERC1_SO1EN | SERC1_SO1F_AC97);
	BA0WRITE4(sc, CS4280_SERC2,  SERC2_SI1EN | SERC2_SI1F_AC97);
	BA0WRITE4(sc, CS4280_SERMC1, SERMC1_MSPE | SERMC1_PTC_AC97);
	
	/* Wait for CODEC ready */
	n = 0;
	while ((BA0READ4(sc, CS4280_ACSTS) & ACSTS_CRDY) == 0) {
		delay(125);
		if (++n > 1000) {
			printf("%s: codec ready timeout\n",
			       sc->sc_dev.dv_xname);
			return(1);
		}
	}

	/* Assert valid frame signal */
	BA0WRITE4(sc, CS4280_ACCTL, ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);

	/* Wait for valid AC97 input slot */
	n = 0;
	while ((BA0READ4(sc, CS4280_ACISV) & (ACISV_ISV3 | ACISV_ISV4)) !=
	       (ACISV_ISV3 | ACISV_ISV4)) {
		delay(1000);
		if (++n > 1000) {
			printf("AC97 inputs slot ready timeout\n");
			return(1);
		}
	}
	
	/* Set AC97 output slot valid signals */
	BA0WRITE4(sc, CS4280_ACOSV, ACOSV_SLV3 | ACOSV_SLV4);

	/* reset the processor */
	cs4280_reset(sc);

	/* Download the image to the processor */
	if (cs4280_download_image(sc) != 0) {
		printf("%s: image download error\n", sc->sc_dev.dv_xname);
		return(1);
	}

	/* Save playback parameter and then write zero.
	 * this ensures that DMA doesn't immediately occur upon
	 * starting the processor core 
	 */
	mem = BA1READ4(sc, CS4280_PCTL);
	sc->pctl = mem & PCTL_MASK; /* save startup value */
	cs4280_halt_output(sc);
	
	/* Save capture parameter and then write zero.
	 * this ensures that DMA doesn't immediately occur upon
	 * starting the processor core 
	 */
	mem = BA1READ4(sc, CS4280_CCTL);
	sc->cctl = mem & CCTL_MASK; /* save startup value */
	cs4280_halt_input(sc);

	/* MSH: need to power up ADC and DAC? */

	/* Processor Startup Procedure */
	BA1WRITE4(sc, CS4280_FRMT, FRMT_FTV);
	BA1WRITE4(sc, CS4280_SPCR, SPCR_RUN | SPCR_RUNFR | SPCR_DRQEN);

	/* Monitor RUNFR bit in SPCR for 1 to 0 transition */
	n = 0;
	while (BA1READ4(sc, CS4280_SPCR) & SPCR_RUNFR) {
		delay(10);
		if (++n > 1000) {
			printf("SPCR 1->0 transition timeout\n");
			return(1);
		}
	}
	
	n = 0;
	while (!(BA1READ4(sc, CS4280_SPCS) & SPCS_SPRUN)) {
		delay(10);
		if (++n > 1000) {
			printf("SPCS 0->1 transition timeout\n");
			return(1);
		}
	}
	/* Processor is now running !!! */

	/* Setup  volume */
	BA1WRITE4(sc, CS4280_PVOL, 0x80008000);
	BA1WRITE4(sc, CS4280_CVOL, 0x80008000);

	/* Interrupt enable */
	BA0WRITE4(sc, CS4280_HICR, HICR_IEV|HICR_CHGM);

	/* playback interrupt enable */
	mem = BA1READ4(sc, CS4280_PFIE) & ~PFIE_PI_MASK;
	mem |= PFIE_PI_ENABLE;
	BA1WRITE4(sc, CS4280_PFIE, mem);
	/* capture interrupt enable */
	mem = BA1READ4(sc, CS4280_CIE) & ~CIE_CI_MASK;
	mem |= CIE_CI_ENABLE;
	BA1WRITE4(sc, CS4280_CIE, mem);

#if NMIDI > 0
	/* Reset midi port */
	mem = BA0READ4(sc, CS4280_MIDCR) & ~MIDCR_MASK;
	BA0WRITE4(sc, CS4280_MIDCR, mem | MIDCR_MRST);
	DPRINTF(("midi reset: 0x%x\n", BA0READ4(sc, CS4280_MIDCR)));
	/* midi interrupt enable */
	mem |= MIDCR_TXE | MIDCR_RXE | MIDCR_RIE | MIDCR_TIE;
	BA0WRITE4(sc, CS4280_MIDCR, mem);
#endif
	return(0);
}

void
cs4280_power(why, v)
	int why;
	void *v;
{
	struct cs4280_softc *sc = (struct cs4280_softc *)v;
	int i;

	DPRINTF(("%s: cs4280_power why=%d\n",
	       sc->sc_dev.dv_xname, why));
	if (why != PWR_RESUME) {
		sc->sc_suspend = why;

		cs4280_halt_output(sc);
		cs4280_halt_input(sc);
		/* Save AC97 registers */
		for(i = 1; i <= CS4280_SAVE_REG_MAX; i++) {
			if(i == 0x04) /* AC97_REG_MASTER_TONE */
				continue;
			cs4280_read_codec(sc, 2*i, &sc->ac97_reg[i>>1]);
		}
		/* should I powerdown here ? */
		cs4280_write_codec(sc, AC97_REG_POWER, CS4280_POWER_DOWN_ALL);
	} else {
		if (sc->sc_suspend == PWR_RESUME) {
			printf("cs4280_power: odd, resume without suspend.\n");
			sc->sc_suspend = why;
			return;
		}
		sc->sc_suspend = why;
		cs4280_init(sc, 0);
		cs4280_reset_codec(sc);

		/* restore ac97 registers */
		for(i = 1; i <= CS4280_SAVE_REG_MAX; i++) {
			if(i == 0x04) /* AC97_REG_MASTER_TONE */
				continue;
			cs4280_write_codec(sc, 2*i, sc->ac97_reg[i>>1]);
		}
	}
}

void
cs4280_clear_fifos(sc)
	struct cs4280_softc *sc;
{
	int pd = 0, cnt, n;
	u_int32_t mem;
	
	/* 
	 * If device power down, power up the device and keep power down
	 * state.
	 */
	mem = BA0READ4(sc, CS4280_CLKCR1);
	if (!(mem & CLKCR1_SWCE)) {
		printf("cs4280_clear_fifo: power down found.\n");
		BA0WRITE4(sc, CS4280_CLKCR1, mem | CLKCR1_SWCE);
		pd = 1;
	}
	BA0WRITE4(sc, CS4280_SERBWP, 0);
	for (cnt = 0; cnt < 256; cnt++) {
		n = 0;
		while (BA0READ4(sc, CS4280_SERBST) & SERBST_WBSY) {
			delay(1000);
			if (++n > 1000) {
				printf("clear_fifo: fist timeout cnt=%d\n", cnt);
				break;
			}
		}
		BA0WRITE4(sc, CS4280_SERBAD, cnt);
		BA0WRITE4(sc, CS4280_SERBCM, SERBCM_WRC);
	}
	if (pd)
		BA0WRITE4(sc, CS4280_CLKCR1, mem);
}

#if NMIDI > 0
int
cs4280_midi_open(addr, flags, iintr, ointr, arg)
	void *addr;
	int flags;
	void (*iintr)__P((void *, int));
	void (*ointr)__P((void *));
	void *arg;
{
	struct cs4280_softc *sc = addr;
	u_int32_t mem;

	DPRINTF(("midi_open\n"));
	sc->sc_iintr = iintr;
	sc->sc_ointr = ointr;
	sc->sc_arg = arg;

	/* midi interrupt enable */
	mem = BA0READ4(sc, CS4280_MIDCR) & ~MIDCR_MASK;
	mem |= MIDCR_TXE | MIDCR_RXE | MIDCR_RIE | MIDCR_TIE | MIDCR_MLB;
	BA0WRITE4(sc, CS4280_MIDCR, mem);
#ifdef CS4280_DEBUG
	if (mem != BA0READ4(sc, CS4280_MIDCR)) {
		DPRINTF(("midi_open: MIDCR=%d\n", BA0READ4(sc, CS4280_MIDCR)));
		return(EINVAL);
	}
	DPRINTF(("MIDCR=0x%x\n", BA0READ4(sc, CS4280_MIDCR)));
#endif
	return (0);
}

void
cs4280_midi_close(addr)
	void *addr;
{
	struct cs4280_softc *sc = addr;
	u_int32_t mem;
	
	DPRINTF(("midi_close\n"));
	mem = BA0READ4(sc, CS4280_MIDCR);
	mem &= ~MIDCR_MASK;
	BA0WRITE4(sc, CS4280_MIDCR, mem);

	sc->sc_iintr = 0;
	sc->sc_ointr = 0;
}

int
cs4280_midi_output(addr, d)
	void *addr;
	int d;
{
	struct cs4280_softc *sc = addr;
	u_int32_t mem;
	int x;

	for (x = 0; x != MIDI_BUSY_WAIT; x++) {
		if ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_TBF) == 0) {
			mem = BA0READ4(sc, CS4280_MIDWP) & ~MIDWP_MASK;
			mem |= d & MIDWP_MASK;
			DPRINTFN(5,("midi_output d=0x%08x",d));
			BA0WRITE4(sc, CS4280_MIDWP, mem);
			if (mem != BA0READ4(sc, CS4280_MIDWP)) {
				DPRINTF(("Bad write data: %d %d",
					 mem, BA0READ4(sc, CS4280_MIDWP)));
				return(EIO);
			}
			return (0);
		}
		delay(MIDI_BUSY_DELAY);
	}
	return (EIO);
}

void
cs4280_midi_getinfo(addr, mi)
	void *addr;
	struct midi_info *mi;
{
	mi->name = "CS4280 MIDI UART";
	mi->props = MIDI_PROP_CAN_INPUT | MIDI_PROP_OUT_INTR;
}

#endif
