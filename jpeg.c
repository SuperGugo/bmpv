#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#pragma pack(push, 1)

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
    uint16_t precision;
    uint16_t lineNb;
    uint8_t samplesPerLine;
    uint8_t compnum;
    struct {
        uint8_t id;
        uint8_t factor;
        uint8_t table;
    } components[3]; // technically should be a pointer and not 3, stfu
} JFIF_SOF;

typedef struct
{
    uint8_t cd;
    uint8_t codeLen[16]; // ?
    uint8_t *codes;
} JFIF_DHT;

#pragma pack(pop)

typedef struct
{
    enum
    {
        APP0,
        DQT,
        SOF,
        DHT,
        SOS,
        ECS,
        EOI
    } type;
} segment;

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
        break;
    }
    case 0xDBFF:
    {
        // DQT
        printf("DQT - %d", len);

        JFIF_DQT *dqt = malloc(sizeof(JFIF_DQT));
        fread(dqt, 1, sizeof(JFIF_DQT), fptr);
        printf(" dst - %d\n", dqt->destination);
        break;
    }
    case 0xC0FF:
    {
        // SOF
        printf("SOF - %d", len);

        JFIF_SOF *sof = malloc(sizeof(JFIF_DQT));
        fread(sof, 1, sizeof(JFIF_SOF), fptr);
        printf(" prec - %d\n", sof->precision);
        break;
    }
    case 0xC4FF:
    {
        // DHT
        printf("DHT - %d", len);
        JFIF_DHT *dht = malloc(sizeof(JFIF_DHT));
        fread(dht, 1, 17, fptr);
        dht->codes = malloc(len-19);
        fread(dht->codes, 1, len-19, fptr);
        printf(" cd - %d\n", dht->cd);
        break;
    }
    case 0xDAFF:
    {
        // SOS
        printf("SOS - %d", len);
        
        break;
    }
    case 0xD9FF:
    {
        // EOF
        return 1;
    }
    default:
        fseek(fptr, len-2, SEEK_CUR);
        printf("%d - %d\n", type, len);
    }
    return 0;
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

    while (!readSegment(fptr));

    fclose(fptr);
    return 0;
}
