#include "../tiny-AES-c/aes.h"
#include <stdio.h>
#include <stdlib.h>


#define ECB 1
#define REPS 10


uint32_t len(uint8_t *text){
    uint32_t length = 0;
    while(text[length] != '\0')
        length++;
    return length;
}

/*void print_blocks(uint8_t *text){
    for(int i = 0; text[i] != '\0';){
        printf("%.2x",text[i]);
        if(!(++i%16))
            printf("\n");
    }
    printf("\n");
}*/


int func(uint8_t inp){
    uint8_t key[] =  "MyV3ryS3CrEtKeY!";
    #include "important_script.c"
    //uint8_t text[] = "Hello, WASM World. I'm so happy!";
    uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

    text[0] = inp;
    int checksum = 0;

    uint32_t textlen = len(text);

    if(textlen % 16 != 0){
        return textlen % 16;
    }

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    for(int j = 0; j<REPS; j++)
        AES_CBC_encrypt_buffer(&ctx, text, textlen);
    
    
    for (int i = 0; i < textlen; i+=4)
    checksum ^= (text[i]) + 
                (text[i+1] << 8) + 
                (text[i+2] << 16) +
                (text[i+3] << 24);
    for(int j = 0; j<REPS; j++)
        AES_CBC_decrypt_buffer(&ctx, text, textlen);
    //printf("Plaintext: \"%s\"\n",text);
    /*printf("Text in utf8:\n");
    print_blocks(text);
    printf("\n"); */


    //Just to ensure that compiler makes sure that encryption and decryption happens.
    for (int i = 0; i < textlen; i+=4)
        checksum ^= (text[i]) + 
                    (text[i+1] << 8) + 
                    (text[i+2] << 16) +
                    (text[i+3] << 24);
    //printf("%d\n",checksum);
    return checksum;
}
