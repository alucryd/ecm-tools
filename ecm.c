////////////////////////////////////////////////////////////////////////////////
//
#define TITLE "ecm - Encoder/decoder for Error Code Modeler format"
#define COPYR "Copyright (C) 2002-2011 Neill Corlett"
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "banner.h"

////////////////////////////////////////////////////////////////////////////////
//
// Sector types
//
// Mode 1
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 01
// 0010h [---DATA...
// ...
// 0800h                                     ...DATA---]
// 0810h [---EDC---] 00 00 00 00 00 00 00 00 [---ECC...
// ...
// 0920h                                      ...ECC---]
// -----------------------------------------------------
//
// Mode 2 (XA), form 1
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 02
// 0010h [--FLAGS--] [--FLAGS--] [---DATA...
// ...
// 0810h             ...DATA---] [---EDC---] [---ECC...
// ...
// 0920h                                      ...ECC---]
// -----------------------------------------------------
//
// Mode 2 (XA), form 2
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 02
// 0010h [--FLAGS--] [--FLAGS--] [---DATA...
// ...
// 0920h                         ...DATA---] [---EDC---]
// -----------------------------------------------------
//
// ADDR:  Sector address, encoded as minutes:seconds:frames in BCD
// FLAGS: Used in Mode 2 (XA) sectors describing the type of sector; repeated
//        twice for redundancy
// DATA:  Area of the sector which contains the actual data itself
// EDC:   Error Detection Code
// ECC:   Error Correction Code
//

////////////////////////////////////////////////////////////////////////////////

static uint32_t get32lsb(const uint8_t* src) {
    return
        (((uint32_t)(src[0])) <<  0) |
        (((uint32_t)(src[1])) <<  8) |
        (((uint32_t)(src[2])) << 16) |
        (((uint32_t)(src[3])) << 24);
}

static void put32lsb(uint8_t* dest, uint32_t value) {
    dest[0] = (uint8_t)(value      );
    dest[1] = (uint8_t)(value >>  8);
    dest[2] = (uint8_t)(value >> 16);
    dest[3] = (uint8_t)(value >> 24);
}

////////////////////////////////////////////////////////////////////////////////
//
// LUTs used for computing ECC/EDC
//
static uint8_t  ecc_f_lut[256];
static uint8_t  ecc_b_lut[256];
static uint32_t edc_lut  [256];

static void eccedc_init(void) {
    size_t i;
    for(i = 0; i < 256; i++) {
        uint32_t edc = i;
        size_t j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
        ecc_f_lut[i] = j;
        ecc_b_lut[i ^ j] = i;
        for(j = 0; j < 8; j++) {
            edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
        }
        edc_lut[i] = edc;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Compute EDC for a block
//
static uint32_t edc_compute(
    uint32_t edc,
    const uint8_t* src,
    size_t size
) {
    for(; size; size--) {
        edc = (edc >> 8) ^ edc_lut[(edc ^ (*src++)) & 0xFF];
    }
    return edc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Check ECC block (either P or Q)
// Returns true if the ECC data is an exact match
//
static int8_t ecc_checkpq(
    const uint8_t* address,
    const uint8_t* data,
    size_t major_count,
    size_t minor_count,
    size_t major_mult,
    size_t minor_inc,
    const uint8_t* ecc
) {
    size_t size = major_count * minor_count;
    size_t major;
    for(major = 0; major < major_count; major++) {
        size_t index = (major >> 1) * major_mult + (major & 1);
        uint8_t ecc_a = 0;
        uint8_t ecc_b = 0;
        size_t minor;
        for(minor = 0; minor < minor_count; minor++) {
            uint8_t temp;
            if(index < 4) {
                temp = address[index];
            } else {
                temp = data[index - 4];
            }
            index += minor_inc;
            if(index >= size) { index -= size; }
            ecc_a ^= temp;
            ecc_b ^= temp;
            ecc_a = ecc_f_lut[ecc_a];
        }
        ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
        if(
            ecc[major              ] != (ecc_a        ) ||
            ecc[major + major_count] != (ecc_a ^ ecc_b)
        ) {
            return 0;
        }
    }
    return 1;
}

//
// Write ECC block (either P or Q)
//
static void ecc_writepq(
    const uint8_t* address,
    const uint8_t* data,
    size_t major_count,
    size_t minor_count,
    size_t major_mult,
    size_t minor_inc,
    uint8_t* ecc
) {
    size_t size = major_count * minor_count;
    size_t major;
    for(major = 0; major < major_count; major++) {
        size_t index = (major >> 1) * major_mult + (major & 1);
        uint8_t ecc_a = 0;
        uint8_t ecc_b = 0;
        size_t minor;
        for(minor = 0; minor < minor_count; minor++) {
            uint8_t temp;
            if(index < 4) {
                temp = address[index];
            } else {
                temp = data[index - 4];
            }
            index += minor_inc;
            if(index >= size) { index -= size; }
            ecc_a ^= temp;
            ecc_b ^= temp;
            ecc_a = ecc_f_lut[ecc_a];
        }
        ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
        ecc[major              ] = (ecc_a        );
        ecc[major + major_count] = (ecc_a ^ ecc_b);
    }
}

//
// Check ECC P and Q codes for a sector
// Returns true if the ECC data is an exact match
//
static int8_t ecc_checksector(
    const uint8_t *address,
    const uint8_t *data,
    const uint8_t *ecc
) {
    return
        ecc_checkpq(address, data, 86, 24,  2, 86, ecc) &&      // P
        ecc_checkpq(address, data, 52, 43, 86, 88, ecc + 0xAC); // Q
}

//
// Write ECC P and Q codes for a sector
//
static void ecc_writesector(
    const uint8_t *address,
    const uint8_t *data,
    uint8_t *ecc
) {
    ecc_writepq(address, data, 86, 24,  2, 86, ecc);        // P
    ecc_writepq(address, data, 52, 43, 86, 88, ecc + 0xAC); // Q
}

////////////////////////////////////////////////////////////////////////////////

static const uint8_t zeroaddress[4] = {0, 0, 0, 0};

////////////////////////////////////////////////////////////////////////////////
//
// Check if this is a sector we can compress
//
// Sector types:
//   0: Literal bytes (not a sector)
//   1: 2352 mode 1         predict sync, mode, reserved, edc, ecc
//   2: 2336 mode 2 form 1  predict redundant flags, edc, ecc
//   3: 2336 mode 2 form 2  predict redundant flags, edc
//
static int8_t detect_sector(const uint8_t* sector, size_t size_available) {
    if(
        size_available >= 2352 &&
        sector[0x000] == 0x00 && // sync (12 bytes)
        sector[0x001] == 0xFF &&
        sector[0x002] == 0xFF &&
        sector[0x003] == 0xFF &&
        sector[0x004] == 0xFF &&
        sector[0x005] == 0xFF &&
        sector[0x006] == 0xFF &&
        sector[0x007] == 0xFF &&
        sector[0x008] == 0xFF &&
        sector[0x009] == 0xFF &&
        sector[0x00A] == 0xFF &&
        sector[0x00B] == 0x00 &&
        sector[0x00F] == 0x01 && // mode (1 byte)
        sector[0x814] == 0x00 && // reserved (8 bytes)
        sector[0x815] == 0x00 &&
        sector[0x816] == 0x00 &&
        sector[0x817] == 0x00 &&
        sector[0x818] == 0x00 &&
        sector[0x819] == 0x00 &&
        sector[0x81A] == 0x00 &&
        sector[0x81B] == 0x00
    ) {
        //
        // Might be Mode 1
        //
        if(
            ecc_checksector(
                sector + 0xC,
                sector + 0x10,
                sector + 0x81C
            ) &&
            edc_compute(0, sector, 0x810) == get32lsb(sector + 0x810)
        ) {
            return 1; // Mode 1
        }

    } else if(
        size_available >= 2336 &&
        sector[0] == sector[4] && // flags (4 bytes)
        sector[1] == sector[5] && //   versus redundant copy
        sector[2] == sector[6] &&
        sector[3] == sector[7]
    ) {
        //
        // Might be Mode 2, Form 1 or 2
        //
        if(
            ecc_checksector(
                zeroaddress,
                sector,
                sector + 0x80C
            ) &&
            edc_compute(0, sector, 0x808) == get32lsb(sector + 0x808)
        ) {
            return 2; // Mode 2, Form 1
        }
        //
        // Might be Mode 2, Form 2
        //
        if(
            edc_compute(0, sector, 0x91C) == get32lsb(sector + 0x91C)
        ) {
            return 3; // Mode 2, Form 2
        }
    }

    //
    // Nothing
    //
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Reconstruct a sector based on type
//
static void reconstruct_sector(
    uint8_t* sector, // must point to a full 2352-byte sector
    int8_t type
) {
    //
    // Sync
    //
    sector[0x000] = 0x00;
    sector[0x001] = 0xFF;
    sector[0x002] = 0xFF;
    sector[0x003] = 0xFF;
    sector[0x004] = 0xFF;
    sector[0x005] = 0xFF;
    sector[0x006] = 0xFF;
    sector[0x007] = 0xFF;
    sector[0x008] = 0xFF;
    sector[0x009] = 0xFF;
    sector[0x00A] = 0xFF;
    sector[0x00B] = 0x00;

    switch(type) {
    case 1:
        //
        // Mode
        //
        sector[0x00F] = 0x01;
        //
        // Reserved
        //
        sector[0x814] = 0x00;
        sector[0x815] = 0x00;
        sector[0x816] = 0x00;
        sector[0x817] = 0x00;
        sector[0x818] = 0x00;
        sector[0x819] = 0x00;
        sector[0x81A] = 0x00;
        sector[0x81B] = 0x00;
        break;
    case 2:
    case 3:
        //
        // Mode
        //
        sector[0x00F] = 0x02;
        //
        // Flags
        //
        sector[0x010] = sector[0x014];
        sector[0x011] = sector[0x015];
        sector[0x012] = sector[0x016];
        sector[0x013] = sector[0x017];
        break;
    }

    //
    // Compute EDC
    //
    switch(type) {
    case 1: put32lsb(sector+0x810, edc_compute(0, sector     , 0x810)); break;
    case 2: put32lsb(sector+0x818, edc_compute(0, sector+0x10, 0x808)); break;
    case 3: put32lsb(sector+0x92C, edc_compute(0, sector+0x10, 0x91C)); break;
    }

    //
    // Compute ECC
    //
    switch(type) {
    case 1: ecc_writesector(sector+0xC , sector+0x10, sector+0x81C); break;
    case 2: ecc_writesector(zeroaddress, sector+0x10, sector+0x81C); break;
    }

    //
    // Done
    //
}

////////////////////////////////////////////////////////////////////////////////
//
// Encode a type/count combo
//
// Returns nonzero on error
//
static int8_t write_type_count(
    const char* outfilename,
    FILE *out,
    int8_t type,
    uint32_t count
) {
    int8_t returncode = 0;

    count--;
    if(fputc(((count >= 32) << 7) | ((count & 31) << 2) | type, out) == EOF) {
        goto error_out;
    }
    count >>= 5;
    while(count) {
        if(fputc(((count >= 128) << 7) | (count & 127), out) == EOF) {
            goto error_out;
        }
        count >>= 7;
    }
    //
    // Success
    //
    returncode = 0;
    goto done;

error_out:
    printfileerror(out, outfilename);
    goto error;

error:
    returncode = 1;
    goto done;

done:
    return returncode;
}

////////////////////////////////////////////////////////////////////////////////

static uint8_t sector_buffer[2352];

////////////////////////////////////////////////////////////////////////////////

static off_t mycounter_analyze = (off_t)-1;
static off_t mycounter_encode  = (off_t)-1;
static off_t mycounter_decode  = (off_t)-1;
static off_t mycounter_total   = 0;

static void resetcounter(off_t total) {
    mycounter_analyze = (off_t)-1;
    mycounter_encode  = (off_t)-1;
    mycounter_decode  = (off_t)-1;
    mycounter_total   = total;
}

static void encode_progress(void) {
    off_t a = (mycounter_analyze + 64) / 128;
    off_t e = (mycounter_encode  + 64) / 128;
    off_t t = (mycounter_total   + 64) / 128;
    if(!t) { t = 1; }
    fprintf(stderr,
        "Analyze(%02u%%) Encode(%02u%%)\r",
        (unsigned)((((off_t)100) * a) / t),
        (unsigned)((((off_t)100) * e) / t)
    );
}

static void decode_progress(void) {
    off_t d = (mycounter_decode  + 64) / 128;
    off_t t = (mycounter_total   + 64) / 128;
    if(!t) { t = 1; }
    fprintf(stderr,
        "Decode(%02u%%)\r",
        (unsigned)((((off_t)100) * d) / t)
    );
}

static void setcounter_analyze(off_t n) {
    int8_t p = ((n >> 20) != (mycounter_analyze >> 20));
    mycounter_analyze = n;
    if(p) { encode_progress(); }
}

static void setcounter_encode(off_t n) {
    int8_t p = ((n >> 20) != (mycounter_encode >> 20));
    mycounter_encode = n;
    if(p) { encode_progress(); }
}

static void setcounter_decode(off_t n) {
    int8_t p = ((n >> 20) != (mycounter_decode >> 20));
    mycounter_decode = n;
    if(p) { decode_progress(); }
}

////////////////////////////////////////////////////////////////////////////////
//
// Encode a run of sectors/literals of the same type
//
// Returns nonzero on error
//
static int8_t write_sectors(
    int8_t type,
    uint32_t count,
    const char* infilename,
    const char* outfilename,
    FILE* in,
    FILE* out
) {
    int8_t returncode = 0;

    if(write_type_count(outfilename, out, type, count)) { goto error; }

    if(type == 0) {
        while(count) {
            uint32_t b = count;
            if(b > sizeof(sector_buffer)) { b = sizeof(sector_buffer); }
            if(fread(sector_buffer, 1, b, in) != b) { goto error_in; }
            if(fwrite(sector_buffer, 1, b, out) != b) { goto error_out; }
            count -= b;
            setcounter_encode(ftello(in));
        }
        return 0;
    }
    for(; count; count--) {
        switch(type) {
        case 1:
            if(fread(sector_buffer, 1, 2352, in) != 2352) { goto error_in; }
            if(fwrite(sector_buffer + 0x00C, 1, 0x003, out) != 0x003) { goto error_out; }
            if(fwrite(sector_buffer + 0x010, 1, 0x800, out) != 0x800) { goto error_out; }
            break;
        case 2:
            if(fread(sector_buffer, 1, 2336, in) != 2336) { goto error_in; }
            if(fwrite(sector_buffer + 0x004, 1, 0x804, out) != 0x804) { goto error_out; }
            break;
        case 3:
            if(fread(sector_buffer, 1, 2336, in) != 2336) { goto error_in; }
            if(fwrite(sector_buffer + 0x004, 1, 0x918, out) != 0x918) { goto error_out; }
            break;
        }
        setcounter_encode(ftello(in));
    }
    //
    // Success
    //
    returncode = 0;
    goto done;

error_in:
    printfileerror(in, infilename);
    goto error;

error_out:
    printfileerror(out, outfilename);
    goto error;

error:
    returncode = 1;
    goto done;

done:
    return returncode;
}

////////////////////////////////////////////////////////////////////////////////
//
// Returns nonzero on error
//
static int8_t ecmify(
    const char* infilename,
    const char* outfilename
) {
    int8_t returncode = 0;

    FILE* in  = NULL;
    FILE* out = NULL;

    uint8_t* queue = NULL;
    size_t queue_start_ofs = 0;
    size_t queue_bytes_available = 0;

    uint32_t input_edc = 0;

    //
    // Current sector type (run)
    //
    int8_t   curtype = -1; // not a valid type
    uint32_t curtype_count = 0;
    off_t    curtype_in_start = 0;

    uint32_t literal_skip = 0;

    off_t input_file_length;
    off_t input_bytes_checked = 0;
    off_t input_bytes_queued  = 0;

    off_t typetally[4] = {0,0,0,0};

    static const size_t sectorsize[4] = {
        1,
        2352,
        2336,
        2336
    };

    size_t queue_size = ((size_t)(-1)) - 4095;
    if((unsigned long)queue_size > 0x40000lu) {
        queue_size = (size_t)0x40000lu;
    }

    //
    // Allocate space for queue
    //
    queue = malloc(queue_size);
    if(!queue) {
        printf("Out of memory\n");
        goto error;
    }

    //
    // Ensure the output file doesn't already exist
    //
    out = fopen(outfilename, "rb");
    if(out) {
        printf("Error: %s exists; refusing to overwrite\n", outfilename);
        goto error;
    }

    //
    // Open both files
    //
    in = fopen(infilename, "rb");
    if(!in) { goto error_in; }

    out = fopen(outfilename, "wb");
    if(!out) { goto error_out; }

    printf("Encoding %s to %s...\n", infilename, outfilename);

    //
    // Get the length of the input file
    //
    if(fseeko(in, 0, SEEK_END) != 0) { goto error_in; }
    input_file_length = ftello(in);
    if(input_file_length < 0) { goto error_in; }

    resetcounter(input_file_length);

    //
    // Magic identifier
    //
    if(fputc('E' , out) == EOF) { goto error_out; }
    if(fputc('C' , out) == EOF) { goto error_out; }
    if(fputc('M' , out) == EOF) { goto error_out; }
    if(fputc(0x00, out) == EOF) { goto error_out; }

    for(;;) {
        int8_t detecttype;

        //
        // Refill queue if necessary
        //
        if(
            (queue_bytes_available < 2352) &&
            (((off_t)queue_bytes_available) < (input_file_length - input_bytes_queued))
        ) {
            //
            // We need to read more data
            //
            off_t willread = input_file_length - input_bytes_queued;
            off_t maxread = queue_size - queue_bytes_available;
            if(willread > maxread) {
                willread = maxread;
            }

            if(queue_start_ofs > 0) {
                memmove(queue, queue + queue_start_ofs, queue_bytes_available);
                queue_start_ofs = 0;
            }
            if(willread) {
                setcounter_analyze(input_bytes_queued);

                if(fseeko(in, input_bytes_queued, SEEK_SET) != 0) {
                    goto error_in;
                }
                if(fread(queue + queue_bytes_available, 1, willread, in) != (size_t)willread) {
                    goto error_in;
                }

                input_edc = edc_compute(
                    input_edc,
                    queue + queue_bytes_available,
                    willread
                );

                input_bytes_queued    += willread;
                queue_bytes_available += willread;
            }
        }

        if(queue_bytes_available == 0) {
            //
            // No data left to read -> quit
            //
            detecttype = -1;

        } else if(literal_skip > 0) {
            //
            // Skipping through literal bytes
            //
            literal_skip--;
            detecttype = 0;

        } else {
            //
            // Heuristic to skip past CD sync after a mode 2 sector
            //
            if(
                curtype >= 2 &&
                queue_bytes_available >= 0x10 &&
                queue[queue_start_ofs + 0x0] == 0x00 &&
                queue[queue_start_ofs + 0x1] == 0xFF &&
                queue[queue_start_ofs + 0x2] == 0xFF &&
                queue[queue_start_ofs + 0x3] == 0xFF &&
                queue[queue_start_ofs + 0x4] == 0xFF &&
                queue[queue_start_ofs + 0x5] == 0xFF &&
                queue[queue_start_ofs + 0x6] == 0xFF &&
                queue[queue_start_ofs + 0x7] == 0xFF &&
                queue[queue_start_ofs + 0x8] == 0xFF &&
                queue[queue_start_ofs + 0x9] == 0xFF &&
                queue[queue_start_ofs + 0xA] == 0xFF &&
                queue[queue_start_ofs + 0xB] == 0x00 &&
                queue[queue_start_ofs + 0xF] == 0x02
            ) {
                // Treat this byte as a literal...
                detecttype = 0;
                // ...and skip the next 15
                literal_skip = 15;
            } else {
                //
                // Detect the sector type at the current offset
                //
                detecttype = detect_sector(queue + queue_start_ofs, queue_bytes_available);
            }
        }

        if(
            (detecttype == curtype) &&
            (curtype_count <= 0x7FFFFFFF) // avoid overflow
        ) {
            //
            // Same type as last sector
            //
            curtype_count++;

        } else {
            //
            // Changing types: Flush the input
            //
            if(curtype_count > 0) {
                if(fseeko(in, curtype_in_start, SEEK_SET) != 0) { goto error_in; }
                typetally[curtype] += curtype_count;
                if(write_sectors(
                    curtype,
                    curtype_count,
                    infilename,
                    outfilename,
                    in,
                    out
                )) { goto error; }
            }
            curtype = detecttype;
            curtype_in_start = input_bytes_checked;
            curtype_count = 1;

        }

        //
        // Current type is negative ==> quit
        //
        if(curtype < 0) { break; }

        //
        // Advance to the next sector
        //
        input_bytes_checked   += sectorsize[curtype];
        queue_start_ofs       += sectorsize[curtype];
        queue_bytes_available -= sectorsize[curtype];

    }

    //
    // Store the end-of-records indicator
    //
    if(write_type_count(outfilename, out, 0, 0)) { goto error; }

    //
    // Store the EDC of the input file
    //
    put32lsb(sector_buffer, input_edc);
    if(fwrite(sector_buffer, 1, 4, out) != 4) { goto error_out; }

    //
    // Show report
    //
    printf("Literal bytes........... "); fprintdec(stdout, typetally[0]); printf("\n");
    printf("Mode 1 sectors.......... "); fprintdec(stdout, typetally[1]); printf("\n");
    printf("Mode 2 form 1 sectors... "); fprintdec(stdout, typetally[2]); printf("\n");
    printf("Mode 2 form 2 sectors... "); fprintdec(stdout, typetally[3]); printf("\n");
    printf("Encoded ");
    fprintdec(stdout, input_file_length);
    printf(" bytes -> ");
    fprintdec(stdout, ftello(out));
    printf(" bytes\n");

    //
    // Success
    //
    printf("Done\n");
    returncode = 0;
    goto done;

error_in:
    printfileerror(in, infilename);
    goto error;

error_out:
    printfileerror(out, outfilename);
    goto error;

error:
    returncode = 1;
    goto done;

done:
    if(queue != NULL) { free(queue); }
    if(in    != NULL) { fclose(in ); }
    if(out   != NULL) { fclose(out); }

    return returncode;
}

////////////////////////////////////////////////////////////////////////////////
//
// Returns nonzero on error
//
static int8_t unecmify(
    const char* infilename,
    const char* outfilename
) {
    int8_t returncode = 0;

    FILE* in  = NULL;
    FILE* out = NULL;

    off_t input_file_length;

    uint32_t output_edc = 0;
    int8_t type;
    uint32_t num;

    //
    // Ensure the output file doesn't already exist
    //
    out = fopen(outfilename, "rb");
    if(out) {
        printf("Error: %s exists; refusing to overwrite\n", outfilename);
        goto error;
    }

    //
    // Open both files
    //
    in = fopen(infilename, "rb");
    if(!in) { goto error_in; }

    //
    // Get the length of the input file
    //
    if(fseeko(in, 0, SEEK_END) != 0) { goto error_in; }
    input_file_length = ftello(in);
    if(input_file_length < 0) { goto error_in; }

    resetcounter(input_file_length);

    if(fseeko(in, 0, SEEK_SET) != 0) { goto error_in; }

    //
    // Magic header
    //
    if(
        (fgetc(in) != 'E') ||
        (fgetc(in) != 'C') ||
        (fgetc(in) != 'M') ||
        (fgetc(in) != 0x00)
    ) {
        printf("Header missing; does not appear to be an ECM file\n");
        goto error;
    }

    //
    // Open output file
    //
    out = fopen(outfilename, "wb");
    if(!out) { goto error_out; }

    printf("Decoding %s to %s...\n", infilename, outfilename);

    for(;;) {
        int c = fgetc(in);
        int bits = 5;
        if(c == EOF) { goto error_in; }
        type = c & 3;
        num = (c >> 2) & 0x1F;
        while(c & 0x80) {
            c = fgetc(in);
            if(c == EOF) { goto error_in; }
            if(
                (bits > 31) ||
                ((uint32_t)(c & 0x7F)) >= (((uint32_t)0x80000000LU) >> (bits-1))
            ) {
                printf("Corrupt ECM file; invalid sector count\n");
                goto error;
            }
            num |= ((uint32_t)(c & 0x7F)) << bits;
            bits += 7;
        }
        if(num == 0xFFFFFFFF) {
            // End indicator
            break;
        }
        num++;
        if(type == 0) {
            while(num) {
                uint32_t b = num;
                if(b > sizeof(sector_buffer)) { b = sizeof(sector_buffer); }
                if(fread(sector_buffer, 1, b, in) != b) {
                    goto error_in;
                }
                output_edc = edc_compute(output_edc, sector_buffer, b);
                if(fwrite(sector_buffer, 1, b, out) != b) {
                    goto error_out;
                }
                num -= b;
                setcounter_decode(ftello(in));
            }
        } else {
            for(; num; num--) {
                switch(type) {
                case 1:
                    if(fread(sector_buffer + 0x00C, 1, 0x003, in) != 0x003) { goto error_in; }
                    if(fread(sector_buffer + 0x010, 1, 0x800, in) != 0x800) { goto error_in; }
                    reconstruct_sector(sector_buffer, 1);
                    output_edc = edc_compute(output_edc, sector_buffer, 2352);
                    if(fwrite(sector_buffer, 1, 2352, out) != 2352) { goto error_out; }
                    break;
                case 2:
                    if(fread(sector_buffer + 0x014, 1, 0x804, in) != 0x804) { goto error_in; }
                    reconstruct_sector(sector_buffer, 2);
                    output_edc = edc_compute(output_edc, sector_buffer + 0x10, 2336);
                    if(fwrite(sector_buffer + 0x10, 1, 2336, out) != 2336) { goto error_out; }
                    break;
                case 3:
                    if(fread(sector_buffer + 0x014, 1, 0x918, in) != 0x918) { goto error_in; }
                    reconstruct_sector(sector_buffer, 3);
                    output_edc = edc_compute(output_edc, sector_buffer + 0x10, 2336);
                    if(fwrite(sector_buffer + 0x10, 1, 2336, out) != 2336) { goto error_out; }
                    break;
                }
                setcounter_decode(ftello(in));
            }
        }
    }
    //
    // Verify the EDC of the entire output file
    //
    if(fread(sector_buffer, 1, 4, in) != 4) { goto error_in; }

    printf("Decoded ");
    fprintdec(stdout, ftello(in));
    printf(" bytes -> ");
    fprintdec(stdout, ftello(out));
    printf(" bytes\n");

    if(get32lsb(sector_buffer) != output_edc) {
        printf("Checksum error (0x%08lX, should be 0x%08lX)\n",
            (unsigned long)output_edc,
            (unsigned long)get32lsb(sector_buffer)
        );
        goto error;
    }

    //
    // Success
    //
    printf("Done\n");
    returncode = 0;
    goto done;

error_in:
    printfileerror(in, infilename);
    goto error;

error_out:
    printfileerror(out, outfilename);
    goto error;

error:
    returncode = 1;
    goto done;

done:
    if(in    != NULL) { fclose(in ); }
    if(out   != NULL) { fclose(out); }

    return returncode;
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    int returncode = 0;
    int8_t encode = 0;
    char* infilename  = NULL;
    char* outfilename = NULL;
    char* tempfilename = NULL;

    normalize_argv0(argv[0]);

    //
    // Check command line
    //
    switch(argc) {
    case 2:
        //
        // bin2ecm source
        // ecm2bin source
        //
        encode = (strcmp(argv[0], "ecm2bin") != 0);
        infilename  = argv[1];

        tempfilename = malloc(strlen(infilename) + 7);
        if(!tempfilename) {
            printf("Out of memory\n");
            goto error;
        }

        strcpy(tempfilename, infilename);

        if(encode) {
            //
            // Append ".ecm" to the input filename
            //
            strcat(tempfilename, ".ecm");
        } else {
            //
            // Remove ".ecm" from the input filename
            //
            size_t l = strlen(tempfilename);
            if(
                (l > 4) &&
                tempfilename[l - 4] == '.' &&
                tolower(tempfilename[l - 3]) == 'e' &&
                tolower(tempfilename[l - 2]) == 'c' &&
                tolower(tempfilename[l - 1]) == 'm'
            ) {
                tempfilename[l - 4] = 0;
            } else {
                //
                // If that fails, append ".unecm" to the input filename
                //
                strcat(tempfilename, ".unecm");
            }
        }
        outfilename = tempfilename;
        break;

    case 3:
        //
        // bin2ecm source dest
        // ecm2bin source dest
        //
        encode = (strcmp(argv[0], "ecm2bin") != 0);
        infilename  = argv[1];
        outfilename = argv[2];
        break;

    default:
        goto usage;
    }

    //
    // Initialize the ECC/EDC tables
    //
    eccedc_init();

    //
    // Go!
    //
    if(encode) {
        if(ecmify(infilename, outfilename)) { goto error; }
    } else {
        if(unecmify(infilename, outfilename)) { goto error; }
    }

    //
    // Success
    //
    returncode = 0;
    goto done;

usage:
    banner();
    printf(
        "Usage:\n"
        "\n"
        "To encode:\n"
        "    bin2ecm cdimagefile\n"
        "    bin2ecm cdimagefile ecmfile\n"
        "\n"
        "To decode:\n"
        "    ecm2bin ecmfile\n"
        "    ecm2bin ecmfile cdimagefile\n"
    );

error:
    returncode = 1;
    goto done;

done:
    if(tempfilename) { free(tempfilename); }
    return returncode;
}

////////////////////////////////////////////////////////////////////////////////
