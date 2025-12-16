#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#pragma pack(push, 1)

// Components are either 1 or 3 (Grayscale or YCbCr), but we do not care enough

typedef struct
{
    uint8_t identifier[5];
    uint16_t version;
    uint8_t units;
    uint16_t Xdensity;
    uint16_t Ydensity;
    uint8_t Xthumbnail;
    uint8_t Ythumbnail;
    uint8_t *thumbnailData;
} JFIF_APP0;

typedef struct
{
    uint8_t destination;
    uint8_t table[64];
} JFIF_DQT;

typedef struct
{
    uint16_t bitsPerPixel;
    uint16_t height;
    uint8_t width;
    uint8_t compnum;
    struct
    {
        uint8_t id;
        uint8_t factor; // h/v sampling
        uint8_t table;  // quantization table!!
    } components[3];    // technically should be a pointer and not 3, stfu
} JFIF_SOF;

typedef struct
{
    uint8_t cd;
    uint8_t codeLen[16]; // ?
    uint8_t *codes;
} JFIF_DHT;

typedef struct
{
    uint8_t compnum;
    struct
    {
        uint8_t id;
        uint8_t table; // huffman table
    } components[3];   // technically should be a pointer and not 3, stfu
    uint8_t spectralStart;
    uint8_t spectralEnd;
    uint8_t successive;
} JFIF_SOS;

#pragma pack(pop)

unsigned j_dqt_len = 0;
unsigned j_dht_len = 0;

JFIF_APP0 *j_app0;
JFIF_DQT **j_dqt;
JFIF_SOF *j_sof;
JFIF_DHT **j_dht;
JFIF_SOS *j_sos;
uint8_t *j_entropyData;

uint32_t *j_codelens;
uint16_t **j_codes;
uint16_t **j_lengths;
uint16_t **j_symbols;

int readSegment(FILE *fptr)
{
    uint16_t type;

    fread(&type, 2, 1, fptr);

    uint16_t len;
    uint8_t support;
    fread(&support, 1, 1, fptr);
    len = support << 8;
    fread(&support, 1, 1, fptr);
    len |= support;

    switch (type)
    {
    case 0xE0FF:
    {
        // APP0
        printf("APP0 - %d\n", len);

        JFIF_APP0 *app0 = malloc(sizeof(JFIF_APP0));
        fread(app0, 1, sizeof(JFIF_APP0) - sizeof(uint8_t *), fptr);
        int thumbnailSize = app0->Xthumbnail * app0->Ydensity;

        // totally ignore the thumbnail for now
        fseek(fptr, thumbnailSize * 3, SEEK_CUR);
        j_app0 = app0;
        break;
    }
    case 0xDBFF:
    {
        // DQT
        printf("DQT - %d\n", len);

        JFIF_DQT *dqt = malloc(sizeof(JFIF_DQT));
        fread(dqt, 1, sizeof(JFIF_DQT), fptr);
        j_dqt[j_dqt_len++] = dqt;
        break;
    }
    case 0xC0FF:
    {
        // SOF
        printf("SOF - %d\n", len);

        JFIF_SOF *sof = malloc(sizeof(JFIF_DQT));
        fread(sof, 1, sizeof(JFIF_SOF), fptr);

        j_sof = sof;
        break;
    }
    case 0xC4FF:
    {
        // DHT
        printf("DHT - %d\n", len);

        JFIF_DHT *dht = malloc(sizeof(JFIF_DHT));
        fread(dht, 1, 17, fptr);
        dht->codes = malloc(len - 19);
        fread(dht->codes, 1, len - 19, fptr);

        j_dht[j_dht_len++] = dht;
        break;
    }
    case 0xDAFF:
    {
        // SOS
        printf("SOS - %d\n", len);

        JFIF_SOS *sos = malloc(sizeof(JFIF_SOS));
        fread(sos, 1, sizeof(JFIF_SOS), fptr);

        j_sos = sos;
        return 1;
    }
    }
    return 0;
}

int find_ht(int tc, int th)
{
    for (int i = 0; i < j_dht_len; i++)
    {
        JFIF_DHT *dht = j_dht[i];
        uint8_t itc = dht->cd >> 4;
        uint8_t ith = dht->cd & 0x0F;
        if (itc == tc && ith == th)
            return i;
    }
    return -1;
}

uint8_t *readbit_ptr;
int readbit_offset;
int read_bit()
{
    uint8_t byte = readbit_ptr[0];
    int bit = (byte >> (7 - readbit_offset)) & 1;
    readbit_offset++;
    if (readbit_offset == 8)
    {
        readbit_ptr++;
        readbit_offset = 0;
    }
    return bit;
}

int decode_huffman(int table)
{
    uint16_t code = 0;
    uint16_t symbol = -1;
    for (int i = 0; i < 16; ++i)
    {
        code = (code << 1) | read_bit();
        for (int j = 0; j < j_codelens[table]; ++j)
        {
            if (j_lengths[table][j] > i + 1)
                break;
            if (j_lengths[table][j] == i + 1)
            {
                if (code == j_codes[table][j])
                {
                    symbol = j_symbols[table][j];
                    break;
                }
            }
        }
    }
    return symbol;
}

int main(int argc, char **argv)
{

    FILE *fptr = fopen("chair.jpeg", "rb");
    if (!fptr)
    {
        printf("file not found");
        return 1;
    }

    fseek(fptr, 2, SEEK_SET);

    j_dqt = malloc(sizeof(JFIF_DQT *) * 16); // arbitrary numbers for now
    j_dht = malloc(sizeof(JFIF_DHT *) * 16);

    while (!readSegment(fptr))
        ;

    int len = 8;
    int out = 0;
    j_entropyData = malloc(len);
    uint8_t rb;

    while (1)
    {
        uint8_t oldrb = rb;
        fread(&rb, 1, 1, fptr);
        if (oldrb == 0xFF && rb != 0x00)
        {
            --out;
            break;
        }
        j_entropyData[out] = rb;

        ++out;
        if (out >= len)
        {
            j_entropyData = realloc(j_entropyData, len *= 2);
        }
    }

    /*
    for (int i = 0; i < out; ++i)
    {
        printf("%d ", j_entropyData[i]);
    }*/

    printf("\n");
    printf("%d\n", j_sof->bitsPerPixel);
    printf("%d\n", j_sof->height);
    printf("%d\n", j_sof->width);
    printf("%d\n", j_sof->components[2].table);

    j_codelens = malloc(j_dht_len * sizeof(uint32_t *));
    j_codes = malloc(j_dht_len * sizeof(uint16_t *));
    j_lengths = malloc(j_dht_len * sizeof(uint16_t *));
    j_symbols = malloc(j_dht_len * sizeof(uint16_t *));

    for (int i = 0; i < j_dht_len; i++)
    {
        JFIF_DHT *dht = j_dht[i];

        int codesum = 0;
        for (int j = 0; j < 16; ++j)
            codesum += dht->codeLen[j];

        uint16_t *codes = malloc(codesum * sizeof(uint16_t));
        uint16_t *lengths = malloc(codesum * sizeof(uint16_t));
        uint16_t *symbols = malloc(codesum * sizeof(uint16_t));

        uint16_t code = 0;
        int k = 0;
        for (int j = 0; j < 16; ++j)
        {
            for (int w = 0; w < dht->codeLen[j]; ++w)
            {
                lengths[k] = j + 1;
                codes[k] = code;
                symbols[k] = dht->codes[k];
                ++code;
                ++k;
            }
            code <<= 1;
        }
        j_codelens[i] = codesum;
        j_codes[i] = codes;
        j_lengths[i] = lengths;
        j_symbols[i] = symbols;
    }

    printf("\n\n");

    printf("\nsof %d", j_sof->components[0].factor);
    printf("\nsof %d", j_sof->components[1].factor);
    printf("\nsof %d", j_sof->components[2].factor);

    printf("\nsos %d", j_sos->components[0].table);
    printf("\nsos %d", j_sos->components[1].table);
    printf("\nsos %d\n", j_sos->components[2].table);

    for (int i = 0; i < 1; i++)
    {
        int blocks = (j_sof->components[i].factor >> 4) * (j_sof->components[i].factor & 0x0F);
        int dc = find_ht(0, j_sos->components[i].table >> 4);
        int ac = find_ht(1, j_sos->components[i].table & 0x0F);

        printf("\n");
        for (int j = 0; j < j_codelens[dc]; ++j)
        {
            printf("%d-%d-%d ", j_codes[dc][j], j_lengths[dc][j], j_symbols[dc][j]);
        }
        printf("\n");

        uint32_t dcoffset = 0;
        readbit_offset = 0;
        readbit_ptr = j_entropyData;
        int symbol = decode_huffman(dc);
        for (int i = 0; i < symbol; ++i)
        {
            dcoffset = (dcoffset << 1) | read_bit();
        }
        if (symbol > 0 && (dcoffset & (1 << (symbol - 1))) == 0)
        dcoffset -= (1 << symbol) - 1;
        printf("symbol: %d, dcoffset: %d\n", symbol, dcoffset);

        symbol = decode_huffman(ac);
        int rl = symbol >> 4;
        int eb = symbol & 0xF;
        uint32_t acoffset1 = 0;
        for (int i = 0; i < eb; ++i)
        {
            acoffset1 = (acoffset1 << 1) | read_bit();
        }
        if (eb > 0 && (acoffset1 & (1 << (eb-1))) == 0)
        acoffset1 -= (1 << eb) - 1;

        printf("acoffset1: %d", acoffset1);

        // printf("%d ", j_entropyData[i]);
    }

    fclose(fptr);
    return 0;
}
