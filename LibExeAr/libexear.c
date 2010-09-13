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

struct ExeArFileInfo
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
	struct ExeArFileInfo** files;

	struct ExeArFile* open_file;
};

struct ExeArFile
{
	FILE* f;
	fpos_t data_start;
	uint32_t data_size;

	long bytes_read;
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
	char buf[11];
	uint32_t file_list_size;
	int i;

	ar_info = (struct ExeArInfo*)malloc(sizeof(struct ExeArInfo));
	if (ar_info == NULL)
		goto error;
	ar_info->f = NULL;
	ar_info->files = NULL;
	ar_info->open_file = NULL;

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
	buf[fread(buf, sizeof(char), 10, ar_info->f)] = '\0';
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

	fseek(ar_info->f, -2 -4, SEEK_CUR);
	file_list_size = read_u32(ar_info->f);

	fseek(ar_info->f, -4 -file_list_size -4, SEEK_CUR);
	ar_info->ar_size = read_u32(ar_info->f);
	ar_info->num_files = read_u16(ar_info->f);

	ar_info->files = (struct ExeArFileInfo**)malloc(sizeof(*ar_info->files) * ar_info->num_files);

	for (i = 0; i < ar_info->num_files; ++i)
	{
		struct ExeArFileInfo* file = (struct ExeArFileInfo*)malloc(sizeof(*file));
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

	fseek(ar_info->f, -(int32_t)(ar_info->ar_size + 4 + file_list_size), SEEK_CUR);
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

static uint32_t find_file_offset(struct ExeArInfo* ar_info, const char* path)
{
	int file_low = 0;
	int file_high = ar_info->num_files - 1;

	while (file_low != file_high)
	{
		int file_mid = (file_low + file_high) / 2;
		int cmp = strcmp(path, ar_info->files[file_mid]->name);

		if (cmp < 0)
			file_high = file_mid - 1;
		else if (cmp > 0)
			file_low = file_mid + 1;
		else
			file_low = file_high = file_mid;
	}

	if (strcmp(ar_info->files[file_low]->name, path) == 0)
		return ar_info->files[file_low]->offset;
	else
		return -1;
}

struct ExeArFile* exear_open_file(struct ExeArInfo* ar_info, const char* path)
{
	uint32_t file_off;
	struct ExeArFile* file;

	if (ar_info == NULL || ar_info->open_file != NULL)
		return NULL;

	file_off = find_file_offset(ar_info, path);

	if (file_off == -1)
		return NULL;

	fsetpos(ar_info->f, &ar_info->data_start);
	fseek(ar_info->f, file_off, SEEK_CUR);

	file = (struct ExeArFile*)malloc(sizeof(struct ExeArFile));
	if (file == NULL)
		return NULL;

	file->f = ar_info->f;
	file->data_size = read_u32(ar_info->f);
	fgetpos(file->f, &file->data_start);
	file->bytes_read = 0;

	ar_info->open_file = file;

	return file;
}

void exear_close_file(struct ExeArInfo* ar_info, struct ExeArFile* file)
{
	free(file);

	if (ar_info != NULL)
		ar_info->open_file = NULL;
}

size_t exear_fread(void *ptr, size_t size, size_t nmemb, struct ExeArFile* stream)
{
	size_t i;

	for (i = 0; i < nmemb; ++i)
	{
		if (stream->bytes_read + size > stream->data_size)
			break;

		if (fread(ptr, size, 1, stream->f) != 1)
         break;

		stream->bytes_read += size;
		ptr = (char*)ptr + size;
	}

	return i;
}

int exear_fseek(struct ExeArFile* stream, long offset, int whence)
{
	if (whence == SEEK_CUR)
	{
		if (offset > 0)
		{
			if ((uint32_t)(stream->bytes_read + offset) > stream->data_size)
				offset = stream->data_size - stream->bytes_read;
		}
		else if (offset < 0)
		{
			if (stream->bytes_read + offset < 0)
				offset = -stream->bytes_read;
		}

		stream->bytes_read += offset;
		return fseek(stream->f, offset, SEEK_CUR);
	}
	else if (whence == SEEK_SET)
	{
		int ret;

		if (offset < 0)
			offset = 0;
		else if ((uint32_t)offset > stream->data_size)
			offset = stream->data_size;

		ret = fseek(stream->f, offset - stream->bytes_read, SEEK_CUR);
		stream->bytes_read = offset;
		return ret;
	}
	else if (whence == SEEK_END)
	{
		int ret;

		if (offset > 0)
			offset = 0;
		else if ((uint32_t)(-offset) > stream->data_size)
			offset = -(long)stream->data_size;

		ret = fseek(stream->f, stream->data_size + offset - stream->bytes_read, SEEK_CUR);
		stream->bytes_read = stream->data_size + offset;
		return ret;
	}

	return -1;
}

long exear_ftell(struct ExeArFile* stream)
{
	return stream->bytes_read;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
