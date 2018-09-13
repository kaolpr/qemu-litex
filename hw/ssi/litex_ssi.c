/*
 * LiteX Bit-Banging SSI controller
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Copyright (c) 2012 Oskar Andero <oskar.andero@gmail.com>
 *
 * This file is derived from hw/realview.c by Paul Brook
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "bitbang_ssi.h"
#include "qemu/log.h"

#include "hw/ssi/litex_ssi.h"

enum {
    R_SPIFLASH_BITBANG,
    R_SPIFLASH_MISO,
    R_SPIFLASH_BITBANG_EN,
    /* LITEX_SSI_R_MAX */
};

#define SSI_MOSI    0x01
#define SSI_SCLK    0x02
#define SSI_CS_N    0x04
//#define SSI_SCLK    0x08

#define SSI_MISO    0x01

static uint64_t litex_ssi_read(void *opaque, hwaddr addr, unsigned size)
{
    LiteXSSIState *s = (LiteXSSIState *)opaque;
    addr >>= 2;
    if (addr >= LITEX_SSI_R_MAX) {
        printf("Tried to access invalid address %08x\n", (unsigned int)addr);
        return 0;
    }

    uint64_t r = s->regs[addr];
    return r;
}

static void litex_ssi_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    LiteXSSIState *s = (LiteXSSIState *)opaque;
    addr >>= 2;

    if (addr >= LITEX_SSI_R_MAX) {
        printf("Tried to access invalid address %08x\n", (unsigned int)addr);
        return;
    }

    s->regs[addr] = value & 0xff;
    switch(addr) {
    case R_SPIFLASH_BITBANG: {
        int ssi_mosi, ssi_cs_n, ssi_sclk, ssi_miso;

        if (!s->regs[R_SPIFLASH_BITBANG_EN]) {
            printf("Bit banging not enabled!\n");
            return;
        }

        ssi_mosi = (value & SSI_MOSI) ? 1 : 0;
        ssi_cs_n = (value & SSI_CS_N) ? 1 : 0;
        ssi_sclk = (value & SSI_SCLK) ? 1 : 0;

        qemu_set_irq(s->cs_n, ssi_cs_n);
        bitbang_ssi_set(s->bitbang, BITBANG_SSI_MOSI, ssi_mosi);
        ssi_miso = bitbang_ssi_set(s->bitbang, BITBANG_SSI_SCLK, ssi_sclk);
        s->regs[R_SPIFLASH_BITBANG] = ssi_miso;
    }
    }
}

static const MemoryRegionOps litex_ssi_ops = {
    .read = litex_ssi_read,
    .write = litex_ssi_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void litex_ssi_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    LiteXSSIState *s = LITEX_SSI(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    SSIBus *bus;

    bus = ssi_create_bus(dev, "ssi");
    s->bitbang = bitbang_ssi_init(bus, BITBANG_SSI_CPOL0, BITBANG_SSI_CPHA0, 8);
    memory_region_init_io(&s->iomem, OBJECT(dev), &litex_ssi_ops, s, "litex_ssi",
                          LITEX_SSI_R_MAX * 4);
    sysbus_init_mmio(sbd, &s->iomem);

    qdev_init_gpio_out_named(dev, &(s->cs_n), SSI_GPIO_CS, 1);
}

static const VMStateDescription vmstate_litex_ssi = {
    .name = "litex_ssi",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, LiteXSSIState, LITEX_SSI_R_MAX),
        VMSTATE_END_OF_LIST()
    }
};

static Property litex_ssi_properties[] = {
    DEFINE_PROP_STRING("spiflash", LiteXSSIState, spiflash),
    DEFINE_PROP_END_OF_LIST(),
};


static void litex_ssi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = litex_ssi_properties;
    dc->vmsd = &vmstate_litex_ssi;
}

static const TypeInfo litex_ssi_info = {
    .name          = TYPE_LITEX_SSI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .class_init    = litex_ssi_class_init,
    .instance_size = sizeof(LiteXSSIState),
    .instance_init = litex_ssi_init,
};

static void litex_ssi_register_types(void)
{
    type_register_static(&litex_ssi_info);
}

type_init(litex_ssi_register_types)
