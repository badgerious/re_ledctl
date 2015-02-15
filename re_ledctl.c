#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/io/iodev.h>
#include <machine/iodev.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#define SPEED_10   0x1
#define SPEED_100  0x2
#define SPEED_1000 0x4
#define BLINK      0x8

u_int read_io_port(int offset, int width)
{
    int devio = open("/dev/io", O_RDONLY);
    if (devio == -1)
    {
        perror("open /dev/io");
        exit(1);
    }

    struct iodev_pio_req r;
    r.access = IODEV_PIO_READ;
    r.port = offset;
    r.width = width;

    if (ioctl(devio, IODEV_PIO, &r) == -1)
    {
        perror("IODEV_PIO_READ failed");
        exit(1);
    }

    close(devio);

    return r.val;
}

void write_io_port(int offset, u_int val, int width)
{
    int devio = open("/dev/io", O_RDWR);
    if (devio == -1)
    {
        perror("open /dev/io");
        exit(1);
    }
    struct iodev_pio_req r;
    r.access = IODEV_PIO_WRITE;
    r.port = offset;
    r.width = width;
    r.val = val;

    if (ioctl(devio, IODEV_PIO, &r) == -1)
    {
        perror("IODEV_PIO_WRITE failed");
        exit(1);
    }

    close(devio);
}

void usage()
{
    printf("Usage: re_ledctl [-s SPEED] [-b] INTERFACE LEDNUM\n");
    printf("\n");
    printf("EXAMPLE: re_ledctl -s 1000 -b re0 0\n");
}

/*
 * Return base address of I/O port space allocated to device.
 * 'interface' is probably something like 're0'.
 */
int parse_interface(char *interface)
{
    char driver_name[PCI_MAXNAMELEN + 1];
    int unit_num;
    char fmt_string[128];
    snprintf(fmt_string, sizeof(fmt_string), "%%%d[a-z]%%d", PCI_MAXNAMELEN);
    if (sscanf(interface, fmt_string, driver_name, &unit_num) != 2)
    {
        fprintf(stderr, "Couldn't parse interface name '%s'\n", interface);
        exit(1);
    }

    int devpci;
    if ((devpci = open("/dev/pci", O_RDONLY)) == -1)
    {
        perror("Failed to open /dev/pci");
        exit(1);
    }

    struct pci_match_conf search_patterns[1];
    bzero(&search_patterns[0], sizeof(search_patterns[0]));
    strlcpy(search_patterns[0].pd_name, driver_name, sizeof(search_patterns[0].pd_name));
    search_patterns[0].pd_unit = unit_num;
    search_patterns[0].flags = (pci_getconf_flags)(PCI_GETCONF_MATCH_NAME | PCI_GETCONF_MATCH_UNIT);

    struct pci_conf matches[1];

    struct pci_conf_io pci_query;
    bzero(&pci_query, sizeof(pci_query));
    pci_query.patterns = search_patterns;
    pci_query.num_patterns = 1;
    pci_query.pat_buf_len = sizeof(search_patterns);
    pci_query.match_buf_len = sizeof(matches);
    pci_query.matches = matches;

    if (ioctl(devpci, PCIOCGETCONF, &pci_query) == -1)
    {
        perror("PCIOCGETCONF failed");
        exit(1);
    }

    if (pci_query.num_matches != 1)
    {
        fprintf(stderr, "Failed to find device for interface %s\n", interface);
        exit(1);
    }

    struct pci_conf *device = &pci_query.matches[0];

    if ((device->pc_hdr & PCIM_HDRTYPE) != PCIM_HDRTYPE_NORMAL)
    {
        fprintf(stderr, "Interface's pc_hdr isn't PCIM_HDRTYPE_NORMAL\n");
        exit(1);
    }

    struct pci_bar_io bar;
    int base = -1;
    for (int i = 0; i < PCIR_MAX_BAR_0; ++i)
    {
        bar.pbi_sel = device->pc_sel;
        bar.pbi_reg = PCIR_BAR(i);
        if (ioctl(devpci, PCIOCGETBAR, &bar) == -1)
        {
            continue;
        }
        if (PCI_BAR_IO(bar.pbi_base))
        {
            base = bar.pbi_base & PCIM_BAR_IO_BASE;
            break;
        }
    }

    if (base == -1)
    {
        fprintf(stderr, "Failed to find I/O Port BAR for interface %s", interface);
        exit(1);
    }

    close(devpci);

    return base;
}

/*
 * Return shift amount within LED control register
 * for the selected LED. Each LED gets 4 bits.
 *
 * LED0 is the green LED on the left. It is bits [3:0] of 16 bit register 0x18.
 *
 * LED1 is the amber LED on the right. It is bits [7:4] of 16 bit register 0x18.
 *
 * LED3 (bits [15:12]) is not implemented on APU1D. Bits [11:8] are reserved.
 */
int parse_lednum(char *lednum)
{
    if (strcmp("0", lednum) == 0)
    {
        return 0;
    }
    else if (strcmp("1", lednum) == 0)
    {
        return 4;
    }
    else
    {
        usage();
        printf("\nLEDNUM must be '0' or '1'\n");
        exit(1);
    }
}

int parse_speed(char *speed)
{
    if (strcmp(speed, "10") == 0)
    {
        return SPEED_10;
    }
    else if (strcmp(speed, "100") == 0)
    {
        return SPEED_100;
    }
    else if (strcmp(speed, "1000") == 0)
    {
        return SPEED_1000;
    }
    else
    {
        usage();
        printf("\nSPEED must be '10', '100', or '1000'\n");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    int ch;
    uint8_t new_flags = 0;
    while ((ch = getopt(argc, argv, "bs:")) != -1)
    {
        switch(ch)
        {
            case 'b':
                new_flags |= BLINK;
                break;
            case 's':
                new_flags |= parse_speed(optarg);
                break;
            case '?':
            default:
                usage();
                return 1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 2)
    {
        usage();
        return 1;
    }

    int io_bar    = parse_interface(argv[0]);
    int flagshift = parse_lednum(argv[1]);
    new_flags = new_flags << flagshift;

    // Set bits [7:6] in 0x50 to disable register protection (must do FIRST).
    write_io_port(io_bar + 0x50, 0xC0 | read_io_port(io_bar + 0x50, 1), 1);
    // Set bit 6 in 0x55 to enable custom LED function.
    write_io_port(io_bar + 0x55, 0x40 | read_io_port(io_bar + 0x55, 1), 1);

    // Now write the bits to set the LEDs up like user asked.
    uint16_t flags = read_io_port(io_bar + 0x18, 2);
    flags &= ~(0xF << flagshift);
    flags |= new_flags;
    write_io_port(io_bar + 0x18, flags, 2);

    return 0;
}
