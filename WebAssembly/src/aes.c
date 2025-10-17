#include "../tiny-AES-c/aes.h"
//#include <stdio.h>
#include <stdlib.h>

#define ECB 1

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
    uint8_t text[] = "Hello, WASM World. I'm so happy!";

    text[0] = inp;
    int checksum = 0;

    uint32_t textlen = len(text);

    if(textlen % 16 != 0){
        //printf("Inappropriate text length of %d! Aborting.\n",textlen);
        return 0xdeadbeef;
    }

    struct AES_ctx ctx;
    /*printf("Plaintext: \"%s\"\n",text);
    printf("Text in utf8:\n");
    print_blocks(text);
    printf("\n");*/
    AES_init_ctx(&ctx, key);
    for(int i = 0; i<textlen;i+=16)
        AES_ECB_encrypt(&ctx, text+i);
    
    
    for (int i = 0; i < textlen; i+=4)
    checksum ^= (text[i]) + 
                (text[i+1] << 8) + 
                (text[i+2] << 16) +
                (text[i+3] << 24);

    for(int i = 0; i<textlen;i+=16)
       AES_ECB_decrypt(&ctx, text + i);
    /*printf("Plaintext: \"%s\"\n",text);
    printf("Text in utf8:\n");
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
