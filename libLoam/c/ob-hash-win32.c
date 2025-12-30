#include "libLoam/c/ob-hash.h"

/* This is the assembly implementation of ob_city_hash64() for Windows.
 *
 * Since Visual Studio doesn't support .asm files, we do the whole thing
 * as a .c file containing nothing but inline assembly.
 *
 * This assembly was produced as follows:
 *
 * - Compile ob_city_hash64() from ob-hash-city.c on 32-bit Mac OS X
 *   using "clang version 3.0 (trunk 135431)" (the whole point of this
 *   exercise being that clang optimizes ob_city_hash64 much, much
 *   better than Visual C++ does)
 *
 * - Convert AT&T-style assembly produced by Clang into Intel-assembly
 *   using the somewhat scary and unreliable program:
 *   http://www.bumba.net/~hmaon/a2i/
 *
 * - Do a fair amount of manual fixup, since previously-mentioned
 *   program is indeed scary and unreliable.
 *
 * - Paste the whole thing into an __asm block in a "naked" C function.
 *
 * And scarily, this actually works, and produces much faster code!
 *
 * Reproducing Google's copyright below, since this was derived from
 * their code (by first manually converting the C++ to C, and then
 * following all the steps above to get assembly), even though it
 * obviously doesn't resemble their source file at all anymore.
 */

// Copyright (c) 2011 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// CityHash, by Geoff Pike and Jyrki Alakuijala
//
// This file provides CityHash64() and related functions.
//
// It's probably possible to create even faster hash functions by
// writing a program that systematically explores some of the space of
// possible hash functions, by using SIMD instructions, or by
// compromising on hash quality.

unt64 __declspec (naked) ob_city_hash64 (const void *key, size_t len)
{
  __asm {
    push    ebp
    push    ebx
    push    edi
    push    esi
    sub     esp,    104
    mov     ebx,    DWORD PTR [128+esp]
    mov     edi,    DWORD PTR [124+esp]
    cmp     ebx,    32
    ja      LBB2_10

    cmp     ebx,    16
    ja      LBB2_9

    cmp     ebx,    9
    jb      LBB2_4

    mov     esi,    DWORD PTR [-8+ebx+edi]
    mov     DWORD PTR [92+esp],     esi
    mov     ebp,    DWORD PTR [-4+ebx+edi]
    mov     DWORD PTR [96+esp],     ebp
    add     esi,    ebx
    adc     ebp,    0
    mov     cl,     bl
    mov     edx,    esi
    shrd    edx,    ebp,    cl
    mov     cl,     bl
    mov     edi,    ebp
    shr     edi,    cl
    xor     eax,    eax
    test    bl,     32
    cmovne  edx,    edi
    cmovne  edi,    eax
    mov     eax,    64
    sub     eax,    ebx
    mov     cl,     al
    shld    ebp,    esi,    cl
    mov     cl,     al
    shl     esi,    cl
    test    al,     32
    cmovne  ebp,    esi
    mov     eax,    0
    cmovne  esi,    eax
    or      esi,    edx
    mov     ebx,    DWORD PTR [124+esp]
    mov     ecx,    DWORD PTR [ebx]
    xor     ecx,    esi
    mov     edx,    -348639895
    mov     eax,    ecx
    mul     edx
    xor     eax,    esi
    imul    ecx,    ecx,    -1646269944
    add     ecx,    edx
    or      ebp,    edi
    mov     edx,    DWORD PTR [4+ebx]
    xor     edx,    ebp
    imul    esi,    edx,    -348639895
    add     esi,    ecx
    mov     ecx,    esi
    shr     ecx,    15
    xor     ecx,    eax
    mov     eax,    ecx
    mov     edi,    -348639895
    mul     edi
    imul    ecx,    ecx,    -1646269944
    add     ecx,    edx
    xor     esi,    ebp
    imul    esi,    esi,    -348639895
    add     esi,    ecx
    mov     ecx,    esi
    shr     ecx,    15
    xor     ecx,    eax
    mov     eax,    ecx
    mul     edi
    imul    ecx,    ecx,    -1646269944
    add     ecx,    edx
    imul    edx,    esi,    -348639895
    add     edx,    ecx
    xor     edx,    DWORD PTR [96+esp]
    xor     eax,    DWORD PTR [92+esp]
    jmp     LBB2_18
  LBB2_4:
    cmp     ebx,    4
    jb      LBB2_6

    mov     esi,    DWORD PTR [edi]
    mov     ebp,    esi
    shr     ebp,    29
    shl     esi,    3
    add     esi,    ebx
    adc     ebp,    0
    mov     ebx,    DWORD PTR [-4+ebx+edi]
    xor     esi,    ebx
    mov     edi,    -348639895
    mov     eax,    esi
    mul     edi
    mov     ecx,    eax
    imul    eax,    esi,    -1646269944
    add     eax,    edx
    imul    esi,    ebp,    -348639895
    add     esi,    eax
    mov     eax,    esi
    shr     eax,    15
    xor     ecx,    ebx
    xor     ecx,    eax
    mov     eax,    ecx
    mul     edi
    imul    ecx,    ecx,    -1646269944
    add     ecx,    edx
    imul    esi,    esi,    -348639895
    add     esi,    ecx
    mov     ecx,    esi
    shr     ecx,    15
    xor     ecx,    eax
    mov     eax,    ecx
    mul     edi
    jmp     LBB2_16
  LBB2_6:
    test    ebx,    ebx
    jne     LBB2_8

    mov     eax,    797982799
    mov     edx,    -1696503237
    jmp     LBB2_18
  LBB2_8:
    movzx   eax,    BYTE PTR [-1+ebx+edi]
    lea     esi,    [ebx+eax*4]
    mov     ecx,    1352557911
    mov     eax,    esi
    mul     ecx
    mov     ecx,    eax
    imul    ebp,    esi,    -917907513
    add     ebp,    edx
    shr     ebx,    1
    movzx   eax,    BYTE PTR [edi+ebx]
    shl     eax,    8
    movzx   edi,    BYTE PTR [edi]
    or      edi,    eax
    mov     ebx,    797982799
    mov     eax,    edi
    mul     ebx
    mov     esi,    eax
    imul    edi,    edi,    -1696503237
    add     edi,    edx
    xor     edi,    ebp
    mov     eax,    edi
    shr     eax,    15
    xor     esi,    ecx
    xor     esi,    eax
    mov     eax,    esi
    mul     ebx
    imul    ecx,    esi,    -1696503237
    add     ecx,    edx
    imul    edx,    edi,    797982799
    jmp     LBB2_17
  LBB2_9:
    mov     eax,    DWORD PTR [-16+ebx+edi]
    mov     DWORD PTR [84+esp],     eax
    mov     esi,    DWORD PTR [-8+ebx+edi]
    mov     DWORD PTR [72+esp],     esi
    mov     ecx,    797982799
    mov     eax,    esi
    mul     ecx
    mov     DWORD PTR [88+esp],     eax
    imul    eax,    esi,    -1696503237
    add     eax,    edx
    imul    ecx,    DWORD PTR [-4+ebx+edi], 797982799
    add     ecx,    eax
    mov     esi,    DWORD PTR [edi]
    mov     eax,    edi
    mov     edx,    DWORD PTR [8+eax]
    mov     DWORD PTR [96+esp],     edx
    mov     edi,    eax
    mov     edx,    -1097272717
    mov     eax,    esi
    mul     edx
    mov     DWORD PTR [92+esp],     eax
    imul    eax,    esi,    -1265453457
    add     eax,    edx
    imul    ebp,    DWORD PTR [4+edi],      -1097272717
    add     ebp,    eax
    mov     edx,    DWORD PTR [92+esp]
    add     edx,    ebx
    mov     eax,    ebp
    adc     eax,    0
    sub     edx,    DWORD PTR [88+esp]
    sbb     eax,    ecx
    mov     DWORD PTR [80+esp],     eax
    mov     esi,    DWORD PTR [12+edi]
    mov     eax,    DWORD PTR [96+esp]
    sub     DWORD PTR [92+esp],     eax
    sbb     ebp,    esi
    xor     eax,    1352557911
    mov     DWORD PTR [96+esp],     eax
    xor     esi,    -917907513
    mov     edi,    DWORD PTR [96+esp]
    shld    edi,    esi,    12
    mov     eax,    esi
    mov     esi,    DWORD PTR [96+esp]
    shld    eax,    esi,    12
    add     eax,    edx
    mov     DWORD PTR [76+esp],     eax
    adc     edi,    DWORD PTR [80+esp]
    mov     DWORD PTR [96+esp],     edi
    mov     edx,    -1748291289
    mov     edi,    DWORD PTR [84+esp]
    mov     eax,    edi
    mul     edx
    imul    edi,    edi,    -1012545444
    add     edi,    edx
    mov     edx,    DWORD PTR [124+esp]
    imul    edx,    DWORD PTR [-12+ebx+edx],        -1748291289
    add     edx,    edi
    mov     ebx,    ebp
    mov     edi,    DWORD PTR [92+esp]
    shld    ebx,    edi,    21
    shrd    ebp,    edi,    11
    add     ebp,    eax
    adc     ebx,    edx
    shrd    DWORD PTR [88+esp],     ecx,    30
    xor     edx,    edx
    mov     esi,    DWORD PTR [72+esp]
    mov     eax,    esi
    mul     edx
    mov     edi,    eax
    or      edi,    DWORD PTR [88+esp]
    imul    eax,    esi,    -1103036100
    add     eax,    edx
    shr     ecx,    30
    or      ecx,    eax
    add     edi,    ebp
    adc     ecx,    ebx
    mov     ebx,    DWORD PTR [96+esp]
    xor     ecx,    ebx
    imul    ebp,    ecx,    -348639895
    mov     eax,    DWORD PTR [76+esp]
    xor     edi,    eax
    mov     esi,    eax
    mov     ecx,    -348639895
    mov     eax,    edi
    mul     ecx
    mov     ecx,    eax
    imul    edi,    edi,    -1646269944
    add     edi,    edx
    add     edi,    ebp
    mov     eax,    edi
    shr     eax,    15
    xor     ecx,    esi
    xor     ecx,    eax
    mov     eax,    ecx
    mov     edx,    -348639895
    mul     edx
    mov     ebp,    -348639895
    imul    ecx,    ecx,    -1646269944
    add     ecx,    edx
    xor     edi,    ebx
    imul    esi,    edi,    -348639895
    add     esi,    ecx
    jmp     LBB2_15
  LBB2_10:
    cmp     ebx,    64
    ja      LBB2_12

    mov     esi,    DWORD PTR [-12+ebx+edi]
    mov     DWORD PTR [68+esp],     esi
    mov     ecx,    DWORD PTR [-16+ebx+edi]
    mov     DWORD PTR [76+esp],     ecx
    add     ecx,    ebx
    adc     esi,    0
    mov     edx,    -1748291289
    mov     eax,    ecx
    mul     edx
    mov     ebp,    edi
    imul    edi,    ecx,    -1012545444
    add     edi,    edx
    imul    ecx,    esi,    -1748291289
    add     ecx,    edi
    add     eax,    DWORD PTR [ebp]
    adc     ecx,    DWORD PTR [4+ebp]
    mov     edx,    ecx
    shld    edx,    eax,    27
    mov     DWORD PTR [88+esp],     edx
    mov     esi,    DWORD PTR [8+ebp]
    add     esi,    eax
    mov     DWORD PTR [92+esp],     esi
    mov     edi,    DWORD PTR [12+ebp]
    adc     edi,    ecx
    mov     DWORD PTR [96+esp],     edi
    mov     edx,    esi
    shld    edx,    edi,    25
    mov     ebp,    eax
    shld    ebp,    ecx,    27
    shld    edi,    esi,    25
    add     edi,    ebp
    adc     edx,    DWORD PTR [88+esp]
    mov     ebp,    DWORD PTR [124+esp]
    mov     esi,    DWORD PTR [28+ebp]
    mov     DWORD PTR [88+esp],     esi
    mov     esi,    DWORD PTR [24+ebp]
    mov     DWORD PTR [48+esp],     esi
    add     eax,    esi
    adc     ecx,    DWORD PTR [88+esp]
    mov     esi,    ecx
    shld    esi,    eax,    12
    shrd    ecx,    eax,    20
    add     ecx,    edi
    adc     esi,    edx
    mov     eax,    ebp
    mov     ebp,    DWORD PTR [16+eax]
    mov     eax,    DWORD PTR [20+eax]
    mov     DWORD PTR [72+esp],     eax
    mov     edi,    DWORD PTR [92+esp]
    add     edi,    ebp
    mov     DWORD PTR [92+esp],     edi
    mov     edx,    DWORD PTR [96+esp]
    adc     edx,    eax
    mov     DWORD PTR [96+esp],     edx
    mov     eax,    edi
    shld    eax,    edx,    1
    shld    edx,    edi,    1
    add     edx,    ecx
    mov     DWORD PTR [84+esp],     edx
    adc     eax,    esi
    mov     DWORD PTR [80+esp],     eax
    mov     eax,    DWORD PTR [124+esp]
    mov     ecx,    DWORD PTR [-28+ebx+eax]
    mov     esi,    DWORD PTR [-20+ebx+eax]
    add     ebp,    DWORD PTR [-32+ebx+eax]
    mov     edx,    eax
    adc     ecx,    DWORD PTR [72+esp]
    mov     eax,    ecx
    shld    eax,    ebp,    27
    mov     DWORD PTR [64+esp],     eax
    mov     eax,    ebp
    shld    eax,    ecx,    27
    mov     DWORD PTR [72+esp],     eax
    mov     edi,    DWORD PTR [-24+ebx+edx]
    add     edi,    ebp
    adc     esi,    ecx
    mov     eax,    edi
    shld    eax,    esi,    25
    mov     DWORD PTR [52+esp],     eax
    mov     eax,    esi
    shld    eax,    edi,    25
    mov     DWORD PTR [56+esp],     eax
    add     edi,    DWORD PTR [76+esp]
    adc     esi,    DWORD PTR [68+esp]
    mov     eax,    edi
    shld    eax,    esi,    1
    mov     DWORD PTR [60+esp],     eax
    mov     eax,    esi
    shld    eax,    edi,    1
    mov     DWORD PTR [68+esp],     eax
    mov     eax,    DWORD PTR [-4+ebx+edx]
    mov     DWORD PTR [76+esp],     eax
    mov     ebx,    DWORD PTR [-8+ebx+edx]
    add     edi,    ebx
    adc     esi,    eax
    add     edi,    DWORD PTR [84+esp]
    adc     esi,    DWORD PTR [80+esp]
    mov     eax,    edi
    mov     edx,    -1748291289
    mul     edx
    mov     DWORD PTR [100+esp],    eax
    imul    eax,    edi,    -1012545444
    add     eax,    edx
    imul    esi,    esi,    -1748291289
    add     esi,    eax
    mov     edx,    DWORD PTR [92+esp]
    add     edx,    DWORD PTR [48+esp]
    mov     eax,    DWORD PTR [96+esp]
    adc     eax,    DWORD PTR [88+esp]
    add     edx,    DWORD PTR [72+esp]
    adc     eax,    DWORD PTR [64+esp]
    add     ebp,    ebx
    adc     ecx,    DWORD PTR [76+esp]
    mov     ebx,    ecx
    shld    ebx,    ebp,    12
    shrd    ecx,    ebp,    20
    add     ecx,    edx
    adc     ebx,    eax
    add     ecx,    DWORD PTR [56+esp]
    adc     ebx,    DWORD PTR [52+esp]
    add     ecx,    DWORD PTR [68+esp]
    adc     ebx,    DWORD PTR [60+esp]
    mov     edi,    797982799
    mov     eax,    ecx
    mul     edi
    imul    ecx,    ecx,    -1696503237
    add     ecx,    edx
    imul    ebx,    ebx,    797982799
    add     ebx,    ecx
    add     eax,    DWORD PTR [100+esp]
    adc     ebx,    esi
    mov     ecx,    ebx
    shr     ecx,    15
    xor     ecx,    eax
    mov     eax,    ecx
    mov     edx,    -1748291289
    mul     edx
    imul    ecx,    ecx,    -1012545444
    add     ecx,    edx
    imul    esi,    ebx,    -1748291289
    add     esi,    ecx
    add     eax,    DWORD PTR [84+esp]
    adc     esi,    DWORD PTR [80+esp]
    mov     ecx,    esi
    shr     ecx,    15
    xor     ecx,    eax
    mov     eax,    ecx
    mul     edi
    imul    ecx,    ecx,    -1696503237
    add     ecx,    edx
    imul    edx,    esi,    797982799
    jmp     LBB2_17
  LBB2_12:
    mov     eax,    DWORD PTR [-16+ebx+edi]
    mov     DWORD PTR [92+esp],     eax
    mov     ecx,    DWORD PTR [-12+ebx+edi]
    mov     DWORD PTR [84+esp],     ecx
    mov     ebp,    DWORD PTR [-52+ebx+edi]
    mov     DWORD PTR [44+esp],     ebp
    mov     edx,    DWORD PTR [-60+ebx+edi]
    mov     esi,    DWORD PTR [-56+ebx+edi]
    mov     DWORD PTR [96+esp],     esi
    mov     eax,    DWORD PTR [-64+ebx+edi]
    add     eax,    ebx
    adc     edx,    0
    add     esi,    eax
    adc     ebp,    edx
    add     esi,    DWORD PTR [-48+ebx+edi]
    mov     DWORD PTR [76+esp],     esi
    adc     ebp,    DWORD PTR [-44+ebx+edi]
    mov     DWORD PTR [80+esp],     ebp
    mov     ebx,    edi
    mov     DWORD PTR [100+esp],    ebx
    mov     edi,    ebp
    shld    edi,    esi,    20
    shld    esi,    ebp,    20
    add     esi,    eax
    adc     edi,    edx
    mov     DWORD PTR [68+esp],     edi
    xor     ecx,    -1265453457
    mov     DWORD PTR [40+esp],     ecx
    mov     edi,    DWORD PTR [92+esp]
    xor     edi,    -1097272717
    mov     DWORD PTR [36+esp],     edi
    add     eax,    edi
    adc     edx,    ecx
    mov     edi,    DWORD PTR [128+esp]
    mov     ebp,    DWORD PTR [-36+edi+ebx]
    mov     DWORD PTR [72+esp],     ebp
    mov     ecx,    DWORD PTR [-40+edi+ebx]
    mov     DWORD PTR [88+esp],     ecx
    add     eax,    ecx
    adc     edx,    ebp
    mov     ecx,    eax
    shld    ecx,    edx,    11
    shld    edx,    eax,    11
    add     edx,    esi
    mov     DWORD PTR [64+esp],     edx
    adc     ecx,    DWORD PTR [68+esp]
    mov     DWORD PTR [68+esp],     ecx
    mov     esi,    ecx
    shr     esi,    15
    xor     esi,    edx
    mov     edx,    -1097272717
    mov     eax,    esi
    mul     edx
    imul    esi,    esi,    -1265453457
    add     esi,    edx
    imul    ecx,    ecx,    -1097272717
    add     ecx,    esi
    mov     edx,    DWORD PTR [44+esp]
    xor     edx,    -1012545444
    mov     esi,    DWORD PTR [96+esp]
    xor     esi,    -1748291289
    add     esi,    eax
    mov     DWORD PTR [96+esp],     esi
    adc     edx,    ecx
    mov     DWORD PTR [44+esp],     edx
    mov     ecx,    DWORD PTR [ebx]
    add     ecx,    esi
    mov     ebp,    DWORD PTR [4+ebx]
    adc     ebp,    edx
    mov     esi,    ecx
    shld    esi,    ebp,    25
    mov     eax,    esi
    mov     edx,    -1097272717
    mul     edx
    mov     DWORD PTR [20+esp],     eax
    imul    eax,    esi,    -1265453457
    add     eax,    edx
    shld    ebp,    ecx,    25
    imul    ecx,    ebp,    -1097272717
    add     ecx,    eax
    mov     DWORD PTR [28+esp],     ecx
    mov     ebp,    edi
    mov     esi,    DWORD PTR [-28+ebp+ebx]
    mov     eax,    ebp
    mov     ecx,    -1097272717
    mul     ecx
    mov     edi,    eax
    imul    ecx,    ebp,    -1265453457
    add     ecx,    edx
    mov     eax,    ebx
    add     edi,    DWORD PTR [-32+ebp+eax]
    adc     ecx,    esi
    mov     edx,    DWORD PTR [-20+ebp+eax]
    mov     ebx,    DWORD PTR [-24+ebp+eax]
    mov     esi,    eax
    add     ebx,    edi
    adc     edx,    ecx
    add     ebx,    DWORD PTR [92+esp]
    adc     edx,    DWORD PTR [84+esp]
    mov     DWORD PTR [84+esp],     edx
    mov     eax,    edx
    shld    eax,    ebx,    20
    mov     ebp,    ebx
    shld    ebp,    edx,    20
    add     ebp,    edi
    adc     eax,    ecx
    mov     DWORD PTR [56+esp],     eax
    mov     edx,    DWORD PTR [128+esp]
    mov     eax,    DWORD PTR [-4+edx+esi]
    mov     DWORD PTR [92+esp],     eax
    mov     eax,    DWORD PTR [-8+edx+esi]
    mov     DWORD PTR [60+esp],     eax
    add     edi,    eax
    adc     ecx,    DWORD PTR [92+esp]
    add     edi,    -1748291289
    adc     ecx,    -1012545444
    mov     eax,    edi
    shld    eax,    ecx,    11
    shld    ecx,    edi,    11
    add     ecx,    ebp
    mov     DWORD PTR [48+esp],     ecx
    adc     eax,    DWORD PTR [56+esp]
    mov     DWORD PTR [56+esp],     eax
    mov     eax,    DWORD PTR [76+esp]
    add     eax,    DWORD PTR [88+esp]
    mov     DWORD PTR [76+esp],     eax
    mov     eax,    DWORD PTR [80+esp]
    adc     eax,    DWORD PTR [72+esp]
    mov     DWORD PTR [80+esp],     eax
    add     ebx,    DWORD PTR [60+esp]
    mov     DWORD PTR [52+esp],     ebx
    mov     eax,    DWORD PTR [84+esp]
    adc     eax,    DWORD PTR [92+esp]
    mov     DWORD PTR [84+esp],     eax
    mov     edi,    DWORD PTR [36+esp]
    mov     esi,    edi
    mov     ecx,    DWORD PTR [40+esp]
    shld    esi,    ecx,    31
    mov     eax,    esi
    mov     edx,    -1097272717
    mul     edx
    mov     DWORD PTR [60+esp],     eax
    imul    eax,    esi,    -1265453457
    add     eax,    edx
    mov     edx,    edi
    shrd    edx,    ecx,    1
    imul    edi,    edx,    -1097272717
    add     edi,    eax
    mov     eax,    DWORD PTR [128+esp]
    add     eax,    -65
    and     eax,    -64
    add     eax,    64
    add     DWORD PTR [100+esp],    56
    mov     edx,    eax

    align 16
  LBB2_13:

    mov     DWORD PTR [4+esp],      edx
    mov     edx,    DWORD PTR [96+esp]
    xor     edx,    DWORD PTR [52+esp]
    mov     eax,    DWORD PTR [44+esp]
    xor     eax,    DWORD PTR [84+esp]
    mov     ecx,    eax
    shld    ecx,    edx,    31
    mov     DWORD PTR [32+esp],     ecx
    mov     ebx,    eax
    shrd    ebx,    edx,    1
    mov     DWORD PTR [36+esp],     ebx
    add     ebx,    DWORD PTR [48+esp]
    adc     ecx,    DWORD PTR [56+esp]
    mov     edx,    DWORD PTR [100+esp]
    add     ebx,    DWORD PTR [-24+edx]
    adc     ecx,    DWORD PTR [-20+edx]
    mov     ebp,    DWORD PTR [-12+edx]
    mov     eax,    DWORD PTR [-16+edx]
    add     eax,    ebx
    adc     ebp,    ecx
    mov     esi,    DWORD PTR [-4+edx]
    mov     DWORD PTR [96+esp],     esi
    mov     esi,    DWORD PTR [-8+edx]
    mov     DWORD PTR [92+esp],     esi
    mov     edx,    DWORD PTR [92+esp]
    add     eax,    edx
    mov     DWORD PTR [88+esp],     eax
    adc     ebp,    DWORD PTR [96+esp]
    mov     DWORD PTR [72+esp],     ebp
    mov     esi,    ebp
    shld    esi,    eax,    20
    shld    eax,    ebp,    20
    add     eax,    ebx
    mov     DWORD PTR [44+esp],     eax
    adc     esi,    ecx
    mov     DWORD PTR [40+esp],     esi
    mov     eax,    DWORD PTR [60+esp]
    add     eax,    DWORD PTR [64+esp]
    mov     esi,    edi
    adc     esi,    DWORD PTR [68+esp]
    add     eax,    edx
    adc     esi,    DWORD PTR [96+esp]
    mov     ebp,    esi
    shld    ebp,    eax,    22
    shrd    esi,    eax,    10
    mov     eax,    esi
    mov     edx,    -1097272717
    mul     edx
    mov     DWORD PTR [92+esp],     eax
    imul    eax,    esi,    -1265453457
    add     eax,    edx
    imul    edx,    ebp,    -1097272717
    add     edx,    eax
    xor     edx,    DWORD PTR [80+esp]
    mov     DWORD PTR [24+esp],     edx
    mov     eax,    DWORD PTR [92+esp]
    xor     eax,    DWORD PTR [76+esp]
    mov     DWORD PTR [92+esp],     eax
    add     ebx,    eax
    adc     ecx,    edx
    mov     ebp,    DWORD PTR [100+esp]
    mov     edx,    DWORD PTR [4+ebp]
    mov     DWORD PTR [8+esp],      edx
    mov     eax,    DWORD PTR [ebp]
    mov     DWORD PTR [12+esp],     eax
    add     ebx,    eax
    adc     ecx,    edx
    mov     eax,    ebx
    shld    eax,    ecx,    11
    shld    ecx,    ebx,    11
    add     ecx,    DWORD PTR [44+esp]
    adc     eax,    DWORD PTR [40+esp]
    mov     DWORD PTR [40+esp],     eax
    mov     esi,    DWORD PTR [64+esp]
    mov     eax,    esi
    mov     edx,    -1097272717
    mul     edx
    imul    esi,    esi,    -1265453457
    add     esi,    edx
    imul    edx,    DWORD PTR [68+esp],     -1097272717

    add     edx,    esi
    mov     esi,    DWORD PTR [-52+ebp]
    mov     ebx,    DWORD PTR [-40+ebp]
    mov     DWORD PTR [96+esp],     ebx
    add     eax,    DWORD PTR [-56+ebp]
    mov     DWORD PTR [68+esp],     eax
    adc     esi,    edx
    mov     edx,    DWORD PTR [-36+ebp]
    mov     DWORD PTR [44+esp],     edx
    add     eax,    ebx
    mov     ebp,    esi
    adc     ebp,    edx
    mov     edx,    DWORD PTR [100+esp]
    add     eax,    DWORD PTR [-48+edx]
    mov     DWORD PTR [esp],        eax
    adc     ebp,    DWORD PTR [-44+edx]
    mov     ebx,    ebp
    shld    ebx,    eax,    20
    mov     edx,    eax
    mov     eax,    ebx
    mov     ebx,    edx
    shld    ebx,    ebp,    20
    add     ebx,    DWORD PTR [68+esp]
    mov     DWORD PTR [64+esp],     ebx
    adc     eax,    esi
    mov     DWORD PTR [16+esp],     eax
    mov     eax,    DWORD PTR [60+esp]
    add     eax,    DWORD PTR [76+esp]
    adc     edi,    DWORD PTR [80+esp]
    add     eax,    DWORD PTR [20+esp]
    adc     edi,    DWORD PTR [28+esp]
    add     eax,    DWORD PTR [96+esp]
    mov     ebx,    eax
    adc     edi,    DWORD PTR [44+esp]
    mov     eax,    edi
    shld    eax,    ebx,    27
    mov     DWORD PTR [80+esp],     eax
    shrd    edi,    ebx,    5
    mov     eax,    edi
    mov     edx,    -1097272717
    mul     edx
    mov     DWORD PTR [96+esp],     eax
    imul    eax,    edi,    -1265453457
    add     eax,    edx
    imul    ebx,    DWORD PTR [80+esp],     -1097272717

    add     ebx,    eax
    xor     ebx,    DWORD PTR [56+esp]
    mov     DWORD PTR [44+esp],     ebx
    mov     edi,    DWORD PTR [68+esp]
    add     edi,    DWORD PTR [52+esp]
    adc     esi,    DWORD PTR [84+esp]
    mov     edx,    DWORD PTR [100+esp]
    mov     eax,    DWORD PTR [-28+edx]
    mov     DWORD PTR [84+esp],     eax
    mov     eax,    DWORD PTR [-32+edx]
    mov     DWORD PTR [80+esp],     eax
    mov     edx,    DWORD PTR [80+esp]
    add     edi,    edx
    adc     esi,    DWORD PTR [84+esp]
    mov     eax,    DWORD PTR [96+esp]
    xor     eax,    DWORD PTR [48+esp]
    mov     DWORD PTR [96+esp],     eax
    add     edi,    eax
    adc     esi,    ebx
    mov     ebx,    edi
    shld    ebx,    esi,    11
    shld    esi,    edi,    11
    add     esi,    DWORD PTR [64+esp]
    mov     DWORD PTR [64+esp],     esi
    mov     esi,    DWORD PTR [esp]
    adc     ebx,    DWORD PTR [16+esp]
    mov     DWORD PTR [68+esp],     ebx
    mov     edi,    DWORD PTR [88+esp]
    add     edi,    DWORD PTR [12+esp]
    mov     DWORD PTR [88+esp],     edi
    mov     edi,    DWORD PTR [72+esp]
    adc     edi,    DWORD PTR [8+esp]
    mov     DWORD PTR [72+esp],     edi
    add     esi,    edx
    mov     edx,    DWORD PTR [4+esp]
    adc     ebp,    DWORD PTR [84+esp]
    add     DWORD PTR [100+esp],    64
    add     edx,    -64
    mov     DWORD PTR [48+esp],     ecx
    mov     eax,    DWORD PTR [40+esp]
    mov     DWORD PTR [56+esp],     eax
    mov     ebx,    eax
    mov     eax,    DWORD PTR [88+esp]
    mov     DWORD PTR [52+esp],     eax
    mov     DWORD PTR [84+esp],     edi
    mov     DWORD PTR [76+esp],     esi
    mov     DWORD PTR [80+esp],     ebp
    mov     eax,    DWORD PTR [92+esp]
    mov     DWORD PTR [60+esp],     eax
    mov     edi,    DWORD PTR [24+esp]
    mov     eax,    DWORD PTR [36+esp]
    mov     DWORD PTR [20+esp],     eax
    mov     eax,    DWORD PTR [32+esp]
    mov     DWORD PTR [28+esp],     eax
    jne     LBB2_13

    mov     edi,    DWORD PTR [64+esp]
    xor     edi,    ecx
    mov     edx,    -348639895
    mov     eax,    edi
    mul     edx
    mov     DWORD PTR [100+esp],    eax
    imul    eax,    edi,    -1646269944
    add     eax,    edx
    mov     edx,    DWORD PTR [68+esp]
    xor     edx,    ebx
    imul    edi,    edx,    -348639895
    add     edi,    eax
    mov     eax,    edi
    shr     eax,    15
    mov     edx,    DWORD PTR [100+esp]
    xor     edx,    ecx
    xor     edx,    eax
    mov     eax,    edx
    mov     ecx,    edx
    mov     edx,    -348639895
    mul     edx
    imul    ebx,    ecx,    -1646269944
    add     ebx,    edx
    xor     edi,    DWORD PTR [40+esp]
    imul    ecx,    edi,    -348639895
    add     ecx,    ebx
    mov     edi,    ecx
    shr     edi,    15
    xor     edi,    eax
    mov     eax,    edi
    mov     ebx,    -348639895
    mul     ebx
    imul    edi,    edi,    -1646269944
    add     edi,    edx
    imul    ecx,    ecx,    -348639895
    add     ecx,    edi
    add     eax,    DWORD PTR [36+esp]
    mov     DWORD PTR [100+esp],    eax
    adc     ecx,    DWORD PTR [32+esp]
    mov     DWORD PTR [84+esp],     ecx
    mov     ecx,    DWORD PTR [88+esp]
    xor     esi,    ecx
    mov     eax,    esi
    mul     ebx
    mov     ebx,    eax
    imul    eax,    esi,    -1646269944
    add     eax,    edx
    xor     ebp,    DWORD PTR [72+esp]
    imul    edi,    ebp,    -348639895
    add     edi,    eax
    mov     eax,    edi
    shr     eax,    15
    xor     ebx,    ecx
    xor     ebx,    eax
    mov     eax,    ebx
    mov     ebp,    -348639895
    mul     ebp
    imul    esi,    ebx,    -1646269944
    add     esi,    edx
    xor     edi,    DWORD PTR [72+esp]
    imul    ebx,    edi,    -348639895
    add     ebx,    esi
    mov     edi,    ebx
    shr     edi,    15
    xor     edi,    eax
    mov     eax,    edi
    mul     ebp
    mov     esi,    eax
    imul    eax,    edi,    -1646269944
    add     eax,    edx
    imul    ebp,    ebx,    -348639895
    add     ebp,    eax
    mov     ecx,    DWORD PTR [24+esp]
    mov     ebx,    ecx
    shr     ebx,    15
    xor     ebx,    DWORD PTR [92+esp]
    mov     edx,    -1097272717
    mov     eax,    ebx
    mul     edx
    mov     edi,    eax
    imul    eax,    ebx,    -1265453457
    add     eax,    edx
    imul    ebx,    ecx,    -1097272717
    add     ebx,    eax
    add     edi,    DWORD PTR [96+esp]
    adc     ebx,    DWORD PTR [44+esp]
    add     edi,    esi
    adc     ebx,    ebp
    mov     ebp,    DWORD PTR [100+esp]
    xor     edi,    ebp
    mov     eax,    edi
    mov     edx,    -348639895
    mul     edx
    mov     esi,    eax
    imul    eax,    edi,    -1646269944
    add     eax,    edx
    mov     ecx,    DWORD PTR [84+esp]
    xor     ebx,    ecx
    imul    edi,    ebx,    -348639895
    add     edi,    eax
    mov     eax,    edi
    shr     eax,    15
    xor     esi,    ebp
    xor     esi,    eax
    mov     eax,    esi
    mov     ebp,    -348639895
    mul     ebp
    imul    ebx,    esi,    -1646269944
    add     ebx,    edx
    xor     edi,    ecx
    imul    esi,    edi,    -348639895
    add     esi,    ebx
  LBB2_15:
    mov     ecx,    esi
    shr     ecx,    15
    xor     ecx,    eax
    mov     eax,    ecx
    mul     ebp
  LBB2_16:
    imul    ecx,    ecx,    -1646269944
    add     ecx,    edx
    imul    edx,    esi,    -348639895
  LBB2_17:
    add     edx,    ecx
  LBB2_18:
    add     esp,    104
    pop     esi
    pop     edi
    pop     ebx
    pop     ebp
    ret
  }
}
