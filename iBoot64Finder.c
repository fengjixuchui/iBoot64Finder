#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

uint64_t base = 0;
uint32_t version = 0;

// Thank you @b1n4r1b01 and @xerub !

#define bswap32(x) __builtin_bswap32(x)

#define hex_set(vers, hex1, hex2) ((version < vers) ? hex1 : hex2)

uint64_t bof64(const void *buf, uint64_t start, uint64_t where) {
  for (; where >= start; where -= 0x4) {
    uint32_t op = *(uint32_t *)(buf + where);
    
    if ((op & 0xffc003ff) == 0x910003fd) {
      unsigned delta = (op >> 10) & 0xfff;
      
      if ((delta & 0xf) == 0) {
        uint64_t prev = where - ((delta >> 0x4) + 1) * 0x4;

        uint32_t au = *(uint32_t *)(buf + prev);

        if ((au & 0xffc003e0) == 0xa98003e0) return prev;

        while (where > start) {
          where -= 0x4;

          au = *(uint32_t *)(buf + where);

          if ((au & 0xffc003ff) == 0xd10003ff && ((au >> 10) & 0xfff) == delta + 0x10)
            return where;
          
          if ((au & 0xffc003e0) != 0xa90003e0) {
            where += 0x4;
            break;
          }
        }
      }
    }
  }
    
  return 0;
}

void *find_insn_before_ptr(void *ptr, uint32_t search, int size) {
  int ct = 0;

  while (size) {
    uint32_t insn = *(uint32_t *)(ptr - ct);

    if (insn == search) return (ptr - ct + 0x4);

    size -= 0x4;
    ct += 0x4;
  }

  return NULL;
}

void *memdata(void *ibot, int length, uint64_t data, int data_size, void *last_ptr) {
  int loc = length - (ibot - last_ptr);

  for (int i = 0; i < length; i += 0x4) {
    return (void *)memmem(last_ptr + 0x4, loc - 0x4, (const char *)&data, data_size);
  }

  return NULL;
}

uint64_t locate_func(void *ibot, int length, uint32_t insn, uint32_t _insn, char *func) {
  void *find = NULL;
  uint64_t beg = 0, loc = 0;

  find = ibot;

  for (int i = 0; i < length; i += 0x4) {
    find = memdata(ibot, length, bswap32(insn), 0x4, find);

    if (find && find_insn_before_ptr(find, bswap32(_insn), 0x40)) {

      loc = (uint64_t)(((uintptr_t)find - (uintptr_t)ibot) + base);

      beg = bof64(ibot, 0x0, loc - base);

      printf("[%s]: %s = 0x%llx\n", __func__, func, beg);

      return beg;
    }
  }

  return 0;
}

/* iOS 7 | iOS 8 | iOS 9 | iOS 13 | iOS 10 to 12 */
#define insn_set(x, vers1, vers2, vers3, vers4, vers5) \
if      (version == 1940) x = vers1; \
else if (version == 2261) x = vers2; \
else if (version == 2817) x = vers3; \
else if (version == 5540) x = vers4; \
else                      x = vers5;

// https://armconverter.com/ (HEX to ASM)

void find_image(void *ibot, int length) {
  uint64_t insn = 0, _insn = 0;

  locate_func(ibot, length, 
    hex_set(4076, hex_set(3406, 0x89e68c72, 0xC0008072), 0x6000a872), 
    hex_set(4076, hex_set(3406, 0x080C40B9, 0x6000a852), 0xC0008052), "_image_load");

  locate_func(ibot, length, 
    0x09090253, hex_set(2817, 0x087c40d3, 0xe80300aa), "_image4_load");

  locate_func(ibot, length, 
    0x6082813c, hex_set(2817, 0x61220591, 0xc0c20091), "_Img4DecodeInit");

  locate_func(ibot, length,
    hex_set(4013, hex_set(3406, 0xe20318aa, 0x48af8d72), 0x2410487a), 
    hex_set(4013, hex_set(3406, 0x810240f9, 0x010B40b9), 0x48af8d52),
    "_image_load_file");

  insn_set(insn, 
    0x2a5d1053, 0x0a5d1053, 0x0a5d1053, 0x2b5d1053, 0x2b5d1053);
  locate_func(ibot, length, 0xe00313aa, insn, "_image4_dump_list");

  locate_func(ibot, length, 
    hex_set(5540, 0x1f000871, 0xa81640f9), 
    hex_set(5540, 0x00013fd6, 0x02408052), "_image_search_bdev");

  locate_func(ibot, length, 
    hex_set(3406, 0xe20307aa, 0xf40307aa), 
    hex_set(3406, 0xf30306aa, 0xfd030191), "_image_load_memory");

  insn_set(insn, 
    0x0100e4d2, 0x0100e4d2, 0x0100e4d2, 0xf5030091, 0xf5030091);
  locate_func(ibot, length, insn, 0xf30302aa, "_image4_get_partial");

  // iOS 11+ (bof64 is buggy here for iOS 10 and less so I removed the offsets).
  if (version >= 4076)
    locate_func(ibot, length, 0x8082c93c, 0xe00314aa, "_Img4DecodeGetPayload");

  locate_func(ibot, length,
    hex_set(3406, 0x08cc0129, 0x084c0129), 
    hex_set(4076, 0x48af8d72, 0xa8aca972), "_image_create_from_memory");

  insn_set(insn,
    0x0841298b, 0x6931899a, 0x6931899a, 0xc8038052, 0x20013fd6);
  insn_set(_insn,
    0xea279f1a, 0x2b0840b9, 0x2b0840b9, 0xa80e40f9, 0xc91640f9);
  locate_func(ibot, length, insn, _insn, "_image4_process_superblock");
}

void find_libc(void *ibot, int length) {
  uint64_t insn = 0, _insn = 0;

  insn_set(insn,
    0x2a3140a9, 0x2a3140a9, 0xb81a088b, 0x29195a8b, 0x2a0908cb);
  locate_func(ibot, length, 
    hex_set(2817, 0x4ae57a92, 0x2a0540b3), insn, "_memalign");

  insn_set(insn, 
    0x09fd46d3, 0x09fd46d3, 0x087c40d3, 0xc81240f9, 0xc81240f9);
  insn_set(_insn, 
    0xf30300aa, 0xf30300aa, 0xf30300aa, 0xff7e00f1, 0xe00314aa);
  locate_func(ibot, length, insn, _insn, "_malloc");

  /* [NOTE]: _bcopy is translated to _memcpy (or _memmove). */
  locate_func(ibot, length, 0x420400f1, 0x422000b1, "_memcpy");

  locate_func(ibot, length, 0x217c039b, 0x211c4092, "_memset");

  locate_func(ibot, length, 0x6300028b, 0x23740bd5, "_bzero");

  locate_func(ibot, length, 
    hex_set(2817, 0x6be57a92, 0x29e57a92), 
    hex_set(2817, 0x49e57ad3, 0x28e57ad3), "_free");
}

void find_usb(void *ibot, int length) {
  uint64_t insn = 0;

  insn_set(insn,
    0x600a00f9, 0x600a00f9, 0x600600f9, 0x800200f9, 0x800600f9);
  locate_func(ibot, length, hex_set(3406, 0x60820091, 
    hex_set(5540, 0x80a20091, 0x34008052)), insn, "_usb_serial_early_init");

  insn_set(insn, 
    0x900d40f9, 0x900d40f9, 0x4b711d53, 0x4b711d53, 0x4b711d53);
  locate_func(ibot, length, 
    insn, hex_set(2817, 0x8c3140f9, 0x6a2140b9), "_usb_core_start");

  insn_set(insn, 
    0xa0c20191, 0xa0c20191, 0x881a40f9, 0x6100a052, 0xa01e40f9);
  locate_func(ibot, length, 0x020080d2, insn, "_usb_core_init");
}

void *find_funcs(void *ibot, int length, int extra) {
  uint64_t insn = 0, _insn = 0;

  find_image(ibot, length);

  if (extra) {
    // [NOTE]: platform_bootprep_darwin() setup trustzones and "Lock the TZ0 region"...

    locate_func(ibot, length, 0x60024039, 0xe10313aa, "_platform_late_init");

    insn_set(insn,
      0x0880a0f2, 0x0880a0f2, 0x0800a2f2, 0x0101c0f2, hex_set(4076, 0x1500a2f2, 0x1501c0f2));
    locate_func(ibot, length, insn, hex_set(2817, 0xf40300aa, 0xf40302aa), "_load_kernelcache_file");

    insn_set(insn, 
      0x0880a0f2, 0x0880a0f2, 0x0800a2f2, 0x0201c0f2, hex_set(4076, 0x1500a2f2, 0x1501c0f2));
    locate_func(ibot, length, 
      hex_set(5540, hex_set(3406, 0x060080d2, 0x070080d2), 0x40008012), insn, "_load_kernelcache");

    insn_set(insn,
     0x60023fd6, 0xa0023fd6, 0x80023fd6, 0x680d8052, 0x80023fd6);
    locate_func(ibot, length, insn, 0x03008052, "_prepare_and_jump");

    insn_set(insn, 
      0x010140f9, 0x010140f9, 0x010140f9, 0xc100a052, hex_set(4076, 0x00e08472, 0x0040a072));
    insn_set(_insn, 
      0xfd030091, 0xfd430091, 0xe10313aa, 0x0040a072, 0x02008052);
    locate_func(ibot, length, insn, _insn, "_load_fs_firmware");

    locate_func(ibot, length, 0xe0039f5a, 0xe30316aa, "_aes_crypto_cmd");
  }

  find_usb(ibot, length);

  insn_set(insn, 0x480100f9, 0x880300f9, 0x0801138b, 0x080300f9, 0x0801138b);
  locate_func(ibot, length, 0x010080d2, insn, "_macho_load");

  find_libc(ibot, length);

  return ibot;
}

void usage(char *owo[]) {
  char *ibot = NULL;
  ibot = strrchr(owo[0], '/');
  printf("usage: %s [-f] <iboot> [-e]\n", (ibot ? ibot + 1 : owo[0]));
  printf("\t-f, --find\tfind functions of a decrypted iBoot64.\n");
  printf("\t-e, --extra\ttry to find more iBoot functions.\n");
}

int main(int argc, char *argv[]) {
  int length = 0;
  FILE *fd = NULL;
  void *ibot = NULL;

  int find = 0, extra = 0;

  if (argc < 3) goto usagend;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--find")) {
      find = 1;

      if (argv[i+2] != NULL) {
        if (!strcmp(argv[i+2], "-e") || !strcmp(argv[i+2], "--extra")) {
          extra = 1;
          break;
        } else {
          printf("warning: unrecognized argument: %s\n", argv[i+2]);
          goto usagend;
        }
      }

      break;
    } else {
      printf("warning: unrecognized argument: %s\n", argv[i]);
      goto usagend;
    }
  }

  if (find) {
    printf("[%s]: starting..\n", __func__);
    
    fd = fopen(argv[2], "rb");

    if (!fd) {
      printf("[%s]: unable to open %s.\n", __func__, argv[2]);
      return -1;
    }

    fseek(fd, 0x0, SEEK_END);
    
    length = ftell(fd);
    
    fseek(fd, 0x0, SEEK_SET);
    
    ibot = (void *)malloc(length);
    
    fread(ibot, 1, length, fd);
    
    fflush(fd);
    fclose(fd);

    if (*(uint32_t *)ibot == 0x496d6733) {
      printf("[%s]: IMG3 files are not supported.\n", __func__);
      goto end;
    }

    if (*(uint32_t *)ibot != 0xea00000e && *(uint32_t *)ibot != 0x90000000) {
      printf("[%s]: this is not a valid iBoot64 image.\n", __func__);
      goto end;
    }

    printf("[%s]: detected iBoot-%s!\n", __func__, ibot + 0x286);

    version = atoi(ibot + 0x286);

    if (version >= 6603) {
      printf("[%s]: iOS 14 is not supported yet, come back soon!\n", __func__);
      return -1;
    }

    if (strcmp(ibot + 0x240, "ROMRELEASE") == 0) {
      printf("[%s]: SecureROM is not supported, only iBoot bootloaders are.\n", __func__);
      return -1;
    }

    base = *(uint64_t *)(ibot + 0x318);

    printf("[%s]: base_addr = 0x%llx\n", __func__, base);

    if (!find_funcs(ibot, length, extra)) return -1;

    printf("[%s]: done!\n", __func__);

    return 0;
  }

  usagend:
  usage(argv);
  return -1;

  end:
  free(ibot);
  return -1;
}