#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DT_FRAME_START 0x00
#define DT_FRAME_END   0x01
#define DT_LINE_START  0x02
#define DT_LINE_END    0x03
#define DT_RAW10       0x2B

#define VC0            0x00

typedef struct {
    uint8_t data_id;
    uint8_t word_count_lsb;
    uint8_t word_count_msb;
    uint8_t ecc;
} Csi2Header;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} ByteBuffer;

static const char *k_output_bin = "out_mipi_csi2_frame_raw10.bin";
static const char *k_output_txt = "out_mipi_csi2_frame_raw10_annotated.txt";
static const char *k_sensor_like_output_bin = "out_mipi_csi2_sensor_like_raw10.bin";
static const char *k_sensor_like_output_txt = "out_mipi_csi2_sensor_like_raw10_annotated.txt";

static void bb_init(ByteBuffer *bb, size_t capacity)
{
    bb->data = (uint8_t *)malloc(capacity);
    bb->size = 0;
    bb->capacity = capacity;
}

static void bb_free(ByteBuffer *bb)
{
    free(bb->data);
    bb->data = NULL;
    bb->size = 0;
    bb->capacity = 0;
}

static void bb_push(ByteBuffer *bb, uint8_t byte)
{
    if (bb->size >= bb->capacity) {
        size_t new_capacity = bb->capacity ? bb->capacity * 2 : 256;
        uint8_t *new_data = (uint8_t *)realloc(bb->data, new_capacity);
        if (!new_data) {
            fprintf(stderr, "realloc failed\n");
            exit(1);
        }
        bb->data = new_data;
        bb->capacity = new_capacity;
    }
    bb->data[bb->size++] = byte;
}

static void bb_append(ByteBuffer *bb, const uint8_t *src, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        bb_push(bb, src[i]);
}

/*
 * Real CSI-2 ECC is a Hamming code over the 24 header bits.
 * This demo uses a tiny placeholder so the byte layout still looks complete.
 */
static uint8_t demo_header_ecc(uint8_t b0, uint8_t b1, uint8_t b2)
{
    return (uint8_t)(b0 ^ b1 ^ b2);
}

static void emit_short_packet(ByteBuffer *bb, uint8_t data_type, uint16_t short_data)
{
    Csi2Header h;
    h.data_id = (uint8_t)((VC0 << 6) | data_type);
    h.word_count_lsb = (uint8_t)(short_data & 0xFF);
    h.word_count_msb = (uint8_t)((short_data >> 8) & 0xFF);
    h.ecc = demo_header_ecc(h.data_id, h.word_count_lsb, h.word_count_msb);

    bb_append(bb, (const uint8_t *)&h, sizeof(h));
}

static void emit_long_packet(ByteBuffer *bb, uint8_t data_type, const uint8_t *payload, uint16_t payload_len)
{
    Csi2Header h;
    uint16_t crc_demo = 0;

    h.data_id = (uint8_t)((VC0 << 6) | data_type);
    h.word_count_lsb = (uint8_t)(payload_len & 0xFF);
    h.word_count_msb = (uint8_t)((payload_len >> 8) & 0xFF);
    h.ecc = demo_header_ecc(h.data_id, h.word_count_lsb, h.word_count_msb);

    bb_append(bb, (const uint8_t *)&h, sizeof(h));
    bb_append(bb, payload, payload_len);

    for (uint16_t i = 0; i < payload_len; ++i)
        crc_demo = (uint16_t)(crc_demo + payload[i]);

    bb_push(bb, (uint8_t)(crc_demo & 0xFF));
    bb_push(bb, (uint8_t)((crc_demo >> 8) & 0xFF));
}

/*
 * RAW10 packing: 4 pixels -> 5 bytes
 * B0 = P0[7:0]
 * B1 = P1[7:0]
 * B2 = P2[7:0]
 * B3 = P3[7:0]
 * B4 = P0[9:8] | P1[9:8]<<2 | P2[9:8]<<4 | P3[9:8]<<6
 */
static size_t pack_raw10_group(const uint16_t *pixels4, uint8_t out5[5])
{
    out5[0] = (uint8_t)(pixels4[0] & 0xFF);
    out5[1] = (uint8_t)(pixels4[1] & 0xFF);
    out5[2] = (uint8_t)(pixels4[2] & 0xFF);
    out5[3] = (uint8_t)(pixels4[3] & 0xFF);
    out5[4] = (uint8_t)(((pixels4[0] >> 8) & 0x03) |
                        (((pixels4[1] >> 8) & 0x03) << 2) |
                        (((pixels4[2] >> 8) & 0x03) << 4) |
                        (((pixels4[3] >> 8) & 0x03) << 6));
    return 5;
}

static void unpack_raw10_group(const uint8_t in5[5], uint16_t *pixels4)
{
    pixels4[0] = (uint16_t)(in5[0] | ((in5[4] & 0x03) << 8));
    pixels4[1] = (uint16_t)(in5[1] | (((in5[4] >> 2) & 0x03) << 8));
    pixels4[2] = (uint16_t)(in5[2] | (((in5[4] >> 4) & 0x03) << 8));
    pixels4[3] = (uint16_t)(in5[3] | (((in5[4] >> 6) & 0x03) << 8));
}

static size_t unpack_raw10_payload(const uint8_t *payload, size_t payload_len, uint16_t *pixels, size_t max_pixels)
{
    size_t groups = payload_len / 5;
    size_t pixel_count = groups * 4;

    if (pixel_count > max_pixels)
        pixel_count = max_pixels;

    for (size_t g = 0; g < groups && g * 4 + 3 < pixel_count; ++g)
        unpack_raw10_group(payload + g * 5, pixels + g * 4);

    return pixel_count;
}

static void dump_bytes(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (len % 16 != 0)
        printf("\n");
}

static void dump_bytes_to_file(FILE *fp, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        fprintf(fp, "%02X ", data[i]);
        if ((i + 1) % 16 == 0)
            fprintf(fp, "\n");
    }
    if (len % 16 != 0)
        fprintf(fp, "\n");
}

static const char *packet_name(uint8_t data_type)
{
    switch (data_type) {
    case DT_FRAME_START: return "Frame Start";
    case DT_FRAME_END:   return "Frame End";
    case DT_LINE_START:  return "Line Start";
    case DT_LINE_END:    return "Line End";
    case DT_RAW10:       return "RAW10 Long Packet";
    default:             return "Unknown";
    }
}

static void print_packet(const uint8_t *buf, size_t offset, size_t payload_len)
{
    const Csi2Header *h = (const Csi2Header *)(buf + offset);
    uint8_t data_type = (uint8_t)(h->data_id & 0x3F);
    uint16_t wc = (uint16_t)(h->word_count_lsb | (h->word_count_msb << 8));

    printf("[%04lu] %s, DT=0x%02X, VC=%u, WC/ShortData=%u, ECC=0x%02X\n",
           (unsigned long)offset, packet_name(data_type), data_type,
           (unsigned)(h->data_id >> 6), (unsigned)wc, h->ecc);

    if (data_type == DT_RAW10) {
        const uint8_t *payload = buf + offset + sizeof(Csi2Header);
        const uint8_t *crc = payload + payload_len;
        printf("        payload bytes: ");
        dump_bytes(payload, payload_len);
        printf("        demo crc: %02X %02X\n", crc[0], crc[1]);
    }
}

static void build_demo_frame(ByteBuffer *stream)
{
    /*
     * Demo frame:
     * frame 0
     * line 0 pixels: 0x001 0x155 0x2AA 0x3FF
     * line 1 pixels: 0x012 0x123 0x234 0x345
     */
    const uint16_t line0[4] = { 0x001, 0x155, 0x2AA, 0x3FF };
    const uint16_t line1[4] = { 0x012, 0x123, 0x234, 0x345 };
    uint8_t payload[5];

    emit_short_packet(stream, DT_FRAME_START, 0);

    emit_short_packet(stream, DT_LINE_START, 0);
    pack_raw10_group(line0, payload);
    emit_long_packet(stream, DT_RAW10, payload, (uint16_t)sizeof(payload));
    emit_short_packet(stream, DT_LINE_END, 0);

    emit_short_packet(stream, DT_LINE_START, 1);
    pack_raw10_group(line1, payload);
    emit_long_packet(stream, DT_RAW10, payload, (uint16_t)sizeof(payload));
    emit_short_packet(stream, DT_LINE_END, 1);

    emit_short_packet(stream, DT_FRAME_END, 0);
}

static uint16_t clamp10(int value)
{
    if (value < 0)
        return 0;
    if (value > 1023)
        return 1023;
    return (uint16_t)value;
}

static uint16_t sensor_like_bayer_pixel(size_t row, size_t col)
{
    int gradient = (int)(row * 36 + col * 10);
    int is_even_row = (row % 2 == 0);
    int is_even_col = (col % 2 == 0);

    if (is_even_row && is_even_col)
        return clamp10(720 + gradient / 3); /* R */
    if (is_even_row && !is_even_col)
        return clamp10(520 + gradient / 2); /* G on R row */
    if (!is_even_row && is_even_col)
        return clamp10(500 + gradient / 2); /* G on B row */
    return clamp10(320 + gradient / 4);     /* B */
}

static void build_sensor_like_frame(ByteBuffer *stream, size_t width, size_t height)
{
    uint16_t short_data_frame = 1;

    emit_short_packet(stream, DT_FRAME_START, short_data_frame);

    for (size_t row = 0; row < height; ++row) {
        size_t groups = width / 4;
        uint16_t group_pixels[4];
        uint8_t packed[5];
        ByteBuffer payload;

        emit_short_packet(stream, DT_LINE_START, (uint16_t)row);
        bb_init(&payload, groups * sizeof(packed));

        for (size_t g = 0; g < groups; ++g) {
            size_t base_col = g * 4;
            group_pixels[0] = sensor_like_bayer_pixel(row, base_col + 0);
            group_pixels[1] = sensor_like_bayer_pixel(row, base_col + 1);
            group_pixels[2] = sensor_like_bayer_pixel(row, base_col + 2);
            group_pixels[3] = sensor_like_bayer_pixel(row, base_col + 3);
            pack_raw10_group(group_pixels, packed);
            bb_append(&payload, packed, sizeof(packed));
        }

        emit_long_packet(stream, DT_RAW10, payload.data, (uint16_t)payload.size);
        bb_free(&payload);

        emit_short_packet(stream, DT_LINE_END, (uint16_t)row);
    }

    emit_short_packet(stream, DT_FRAME_END, short_data_frame);
}

static void parse_demo_stream(const uint8_t *buf, size_t size)
{
    size_t off = 0;

    printf("=== Parse simplified CSI-2 stream ===\n");
    while (off + sizeof(Csi2Header) <= size) {
        const Csi2Header *h = (const Csi2Header *)(buf + off);
        uint8_t data_type = (uint8_t)(h->data_id & 0x3F);
        uint16_t wc = (uint16_t)(h->word_count_lsb | (h->word_count_msb << 8));

        if (data_type == DT_RAW10) {
            if (off + sizeof(Csi2Header) + wc + 2 > size) {
                printf("broken stream at offset %lu\n", (unsigned long)off);
                return;
            }

            print_packet(buf, off, wc);

            if (wc == 5) {
                uint16_t pixels[4];
                unpack_raw10_group(buf + off + sizeof(Csi2Header), pixels);
                printf("        unpacked RAW10 pixels: %u %u %u %u\n",
                       pixels[0], pixels[1], pixels[2], pixels[3]);
            }

            off += sizeof(Csi2Header) + wc + 2;
        } else {
            print_packet(buf, off, 0);
            off += sizeof(Csi2Header);
        }
    }
}

static void parse_named_stream(const char *title, const uint8_t *buf, size_t size)
{
    printf("=== %s ===\n", title);
    parse_demo_stream(buf, size);
}

static void write_binary_file(const char *path, const uint8_t *buf, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "failed to open binary output: %s\n", path);
        exit(1);
    }

    fwrite(buf, 1, size, fp);
    fclose(fp);
}

static void write_annotated_file(const char *path, const char *bin_name, const uint8_t *buf, size_t size)
{
    FILE *fp = fopen(path, "w");
    size_t off = 0;

    if (!fp) {
        fprintf(stderr, "failed to open text output: %s\n", path);
        exit(1);
    }

    fprintf(fp, "Simplified MIPI CSI-2 RAW10 frame stream\n");
    fprintf(fp, "Binary file: %s\n\n", bin_name);
    fprintf(fp, "Whole stream hex dump:\n");
    dump_bytes_to_file(fp, buf, size);
    fprintf(fp, "\n");

    fprintf(fp, "Packet-by-packet layout:\n");
    while (off + sizeof(Csi2Header) <= size) {
        const Csi2Header *h = (const Csi2Header *)(buf + off);
        uint8_t data_type = (uint8_t)(h->data_id & 0x3F);
        uint16_t wc = (uint16_t)(h->word_count_lsb | (h->word_count_msb << 8));

        fprintf(fp, "[offset %04lu] %s\n", (unsigned long)off, packet_name(data_type));
        fprintf(fp, "  header:\n");
        fprintf(fp, "    data_id         = 0x%02X\n", h->data_id);
        fprintf(fp, "    word_count_lsb  = 0x%02X\n", h->word_count_lsb);
        fprintf(fp, "    word_count_msb  = 0x%02X\n", h->word_count_msb);
        fprintf(fp, "    ecc             = 0x%02X\n", h->ecc);

        if (data_type == DT_RAW10) {
            const uint8_t *payload = buf + off + sizeof(Csi2Header);
            const uint8_t *crc = payload + wc;
            uint16_t pixels[256];
            size_t pixel_count = unpack_raw10_payload(payload, wc, pixels, 256);

            fprintf(fp, "  payload (%u bytes):\n", (unsigned)wc);
            dump_bytes_to_file(fp, payload, wc);
            fprintf(fp, "  footer crc (demo): %02X %02X\n", crc[0], crc[1]);
            fprintf(fp, "  unpacked raw10 pixels (%lu):", (unsigned long)pixel_count);
            for (size_t i = 0; i < pixel_count; ++i) {
                if (i % 16 == 0)
                    fprintf(fp, "\n    ");
                fprintf(fp, "%4u ", pixels[i]);
            }
            fprintf(fp, "\n");

            off += sizeof(Csi2Header) + wc + 2;
        } else {
            fprintf(fp, "  short_data/value  = %u\n", (unsigned)wc);
            off += sizeof(Csi2Header);
        }

        fprintf(fp, "\n");
    }

    fprintf(fp, "What the stream contains:\n");
    fprintf(fp, "1. Frame Start short packet\n");
    fprintf(fp, "2. Line Start short packet\n");
    fprintf(fp, "3. RAW10 long packet carrying pixel payload\n");
    fprintf(fp, "4. Line End short packet\n");
    fprintf(fp, "5. Frame End short packet\n\n");
    fprintf(fp, "In real hardware, LP/HS switching and lane-level serialization happen before the receiver sees this byte-stream form.\n");

    fclose(fp);
}

int main(void)
{
    ByteBuffer simple_stream;
    ByteBuffer sensor_like_stream;

    bb_init(&simple_stream, 256);
    build_demo_frame(&simple_stream);

    printf("=== Simplified MIPI CSI-2 byte stream ===\n");
    dump_bytes(simple_stream.data, simple_stream.size);
    printf("\n");
    parse_named_stream("Parse simplified CSI-2 stream", simple_stream.data, simple_stream.size);
    write_binary_file(k_output_bin, simple_stream.data, simple_stream.size);
    write_annotated_file(k_output_txt, k_output_bin, simple_stream.data, simple_stream.size);

    bb_init(&sensor_like_stream, 1024);
    build_sensor_like_frame(&sensor_like_stream, 16, 6);

    printf("\n=== Sensor-like multi-line RAW10 CSI-2 byte stream ===\n");
    dump_bytes(sensor_like_stream.data, sensor_like_stream.size);
    printf("\n");
    parse_named_stream("Parse sensor-like multi-line CSI-2 stream", sensor_like_stream.data, sensor_like_stream.size);
    write_binary_file(k_sensor_like_output_bin, sensor_like_stream.data, sensor_like_stream.size);
    write_annotated_file(k_sensor_like_output_txt, k_sensor_like_output_bin,
                         sensor_like_stream.data, sensor_like_stream.size);

    printf("\n=== Notes ===\n");
    printf("1. Real MIPI CSI-2 has proper ECC/CRC, this demo uses placeholders for readability.\n");
    printf("2. Real hardware also involves LP/HS state switching and lane-level serialization.\n");
    printf("3. The receiver first sees packet flow, then reconstructs image lines from long packets.\n");
    printf("4. Generated files:\n");
    printf("   - %s\n", k_output_bin);
    printf("   - %s\n", k_output_txt);
    printf("   - %s\n", k_sensor_like_output_bin);
    printf("   - %s\n", k_sensor_like_output_txt);

    bb_free(&simple_stream);
    bb_free(&sensor_like_stream);
    return 0;
}
