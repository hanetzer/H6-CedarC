/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : h265_sha.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2017/04/13
*   Comment :
*
*
*/

#include   <stdio.h>
#include   <string.h>

typedef   struct {

    unsigned   int  state[5];

    unsigned   int   count[2];

    unsigned   char   buffer[64];

}   SHA1_CTX;


void   SHA1Final(unsigned   char   digest[20],   SHA1_CTX*   context)   ;
void   SHA1Update(SHA1_CTX*   context,   unsigned   char*   data,   unsigned   int   len) ;
void   SHA1Init(SHA1_CTX*   context) ;
void   SHA1Transform(unsigned int   state[5],   unsigned   char   buffer[64])  ;
