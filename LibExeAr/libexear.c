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
#include "libexear.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ExeArFile
{
	char* name;
	uint32_t offset;
};

struct ExeArInfo
{
	unsigned int version;

	FILE* f;
	uint32_t ar_size;
	fpos_t data_start;

	int num_files;
	struct ExeArFile** files;
};

#ifdef _WIN32
	#include <Windows.h>
	static FILE* exear_detect_fname()
	{
		TCHAR buf[512];

		DWORD size = GetModuleFileName(NULL, buf, 512);

		if (size == 0)
			return NULL;
		else
	#ifdef UNICODE
			return _wfopen(buf, L"rb");
	#else
			return fopen(buf, "rb");
	#endif
	}
#elif linux
	static FILE* exear_detect_fname()
	{
		return fopen("/proc/self/exe", "rb");
	}
#else
	#include <sys/param.h>

	#ifdef BSD
		static FILE* exear_detect_fname()
		{
			return fopen("/proc/curproc/file", "rb");
		}
	#else
		static FILE* exear_detect_fname()
		{
			return NULL;
		}
	#endif
#endif

static uint16_t read_u16(FILE* f)
{
	uint16_t val = 0;
	val |= (fgetc(f) & 0xFF) << 8;
	val |= (fgetc(f) & 0xFF) << 0;

	return (feof(f) || ferror(f)) ? 0 : val;
}

static uint32_t read_u32(FILE* f)
{
	uint32_t val = 0;
	val |= (fgetc(f) & 0xFF) << 24;
	val |= (fgetc(f) & 0xFF) << 16;
	val |= (fgetc(f) & 0xFF) << 8;
	val |= (fgetc(f) & 0xFF) << 0;

	return (feof(f) || ferror(f)) ? 0 : val;
}

struct ExeArInfo* exear_open(const char* fname)
{
	struct ExeArInfo* ar_info = NULL;
	char buf[10];
	uint32_t file_list_size;
	int i;

	ar_info = (struct ExeArInfo*)malloc(sizeof(struct ExeArInfo));
	if (ar_info == NULL)
		goto error;
	ar_info->f = NULL;
	ar_info->files = NULL;

	if (fname == NULL) {
		ar_info->f = exear_detect_fname();
		if (ar_info->f == NULL)
			goto error;
	}
	else
	{
		ar_info->f = fopen(fname, "r");
	}

	fseek(ar_info->f, -10, SEEK_END);
	fread(buf, sizeof(char), 10, ar_info->f);
	if (strcmp(buf, "EXEARCHIVE") != 0)
	{
		goto error;
	}

	fseek(ar_info->f, -10 -2, SEEK_CUR);
	if (read_u16(ar_info->f) != 0x0001) /* Endianess */
	{
		goto error;
	}

	fseek(ar_info->f, -2 -2, SEEK_CUR);
	if ((ar_info->version = read_u16(ar_info->f)) != 0) /* Version */
	{
		goto error;
	}

	fseek(ar_info->f, -2 -4 -4, SEEK_CUR);
	ar_info->ar_size = read_u32(ar_info->f);
	file_list_size = read_u32(ar_info->f);

	fseek(ar_info->f, -4 -file_list_size, SEEK_CUR);
	ar_info->num_files = read_u16(ar_info->f);

	ar_info->files = (struct ExeArFile**)malloc(sizeof(*ar_info->files) * ar_info->num_files);

	for (i = 0; i < ar_info->num_files; ++i)
	{
		struct ExeArFile* file = (struct ExeArFile*)malloc(sizeof(*file));
		int fname_size = read_u16(ar_info->f);

		ar_info->files[i] = file;

		file->name = (char*)malloc(sizeof(char) * (fname_size + 1));
		file->name[fread(file->name, sizeof(char), fname_size, ar_info->f)] = '\0';
		file->offset = read_u32(ar_info->f);
	}

	if (read_u16(ar_info->f) != 0xFFFF) /* File list terminator */
	{
		goto error;
	}

	fseek(ar_info->f, -(int32_t)(ar_info->ar_size + file_list_size), SEEK_CUR);
	fgetpos(ar_info->f, &ar_info->data_start);

	return ar_info;

error:
	exear_close(ar_info);
	return NULL;
}

void exear_close(struct ExeArInfo* ar_info)
{
	if (ar_info != NULL)
	{
		if (ar_info->f != NULL)
			fclose(ar_info->f);

		if (ar_info->files != NULL)
		{
			int i;
			for (i = 0; i < ar_info->num_files; ++i)
			{
				free(ar_info->files[i]->name);
				free(ar_info->files[i]);
			}
			free(ar_info->files);
		}
	}
	free(ar_info);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
