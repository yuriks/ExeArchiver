/*
 * The MIT License
 * 
 * Copyright (c) 2010 Yuri K. Schlesner
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef EXEARCHIVER_LIBEXEAR_H
#define EXEARCHIVER_LIBEXEAR_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ExeArInfo;

/** Opens the executable and reads archive info from it.
 *
 * @param fname The path to the executable, if NULL it'll be auto-detected.
 */
struct ExeArInfo* exear_open(const char* fname);

/** Closes handlers and frees all data of ar_info. ar_info shall not be used
 * after calling this function.
 *
 * @param ar_info The ExeArInfo struct to free.
 */
void exear_close(struct ExeArInfo* ar_info);

/** Returns a FILE and the size of the data corresponding to the specified
 * path. The FILE should not be closed.
 *
 * @param ar_info The ExeArInfo struct.
 * @param path The path of the file to open.
 * @param size Pointer to integer that will hold the size of the file.
 *             Unchanged if path isn't found.
 *
 * @return A FILE object seeked to the correct position for reading, or NULL
 *         if path wasn't found.
 */
FILE* exear_open_file(struct ExeArInfo* ar_info, const char* path, uint32_t* size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EXEARCHIVER_LIBEXEAR_H */
