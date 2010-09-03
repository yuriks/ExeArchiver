/*
 * ExeArchiver: An archiver for embedding data into programs.
 * Copyright (C) 2010  Yuri K. Schlesner
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <cstdint>

uint32_t get_file_size(std::ifstream& f)
{
	std::ifstream::pos_type old_pos = f.tellg();

	f.seekg(0, std::ios::end);
	uint32_t size = static_cast<uint32_t>(f.tellg());

	f.seekg(old_pos);

	return size;
}

std::ostream& write_u16(std::ostream& f, uint32_t val)
{
	f.put((val >> 8)  & 0xFF);
	f.put((val >> 0)  & 0xFF);

	return f;
}

std::ostream& write_u32(std::ostream& f, uint32_t val)
{
	f.put((val >> 24) & 0xFF);
	f.put((val >> 16) & 0xFF);
	f.put((val >> 8)  & 0xFF);
	f.put((val >> 0)  & 0xFF);

	return f;
}

std::ostream& copy_file_contents(std::istream& infile, std::ostream& outfile)
{
	static const int buf_size = 16 * 1024;
	char buf[buf_size];

	infile.seekg(0);

	do
	{
		infile.read(buf, buf_size);
		outfile.write(buf, infile.gcount());
	} while (infile.gcount() > 0);

	return outfile;
}

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cerr << "Usage:\n  " << argv[0] << " outfile filelist" << std::endl;
		return 1;
	}

	std::ifstream filelist_file(argv[2]);
	if (!filelist_file)
	{
		std::cerr << "Error opening " << argv[2] << std::endl;
		return 2;
	}

	std::ofstream outfile(argv[1], std::ios::out | std::ios::binary);
	if (!outfile)
	{
		std::cerr << "Error opening " << argv[1] << std::endl;
		return 2;
	}

	// nome / offset
	typedef std::vector<std::pair<std::string, std::uint32_t>> FileList;
	FileList file_list;

	uint32_t cur_offset = 0;

	std::string fname;
	while (std::getline(filelist_file, fname))
	{
		if (fname.empty())
			continue;

		std::ifstream infile(fname, std::ios::in | std::ios::binary);
		if (!infile)
		{
			std::cerr << "Error opening " << fname << " - Skipped" << std::endl;
			continue;
		}

		uint32_t size = get_file_size(infile);

		std::cerr << "Processing \"" << fname << "\" Size: " << size << std::endl;

		write_u32(outfile, size);
		copy_file_contents(infile, outfile);

		file_list.push_back(make_pair(fname, cur_offset));
		cur_offset += size + 4; // +4 is uint32_t size
	}

	filelist_file.close();

	write_u32(outfile, cur_offset); // Contents size
	cur_offset = 0;

	std::sort(file_list.begin(), file_list.end());

	write_u16(outfile, file_list.size());
	cur_offset += 2;
	for (FileList::const_iterator i = file_list.begin(); i != file_list.end(); ++i)
	{
		write_u16(outfile, i->first.length());
		outfile << i->first;
		write_u32(outfile, i->second);

		cur_offset += 2 + i->first.length() + 4;
	}
	write_u16(outfile, 0xFFFF); // End of file list
	cur_offset += 2;

	write_u32(outfile, cur_offset); // File list size

	write_u16(outfile, 0); // Version
	write_u16(outfile, 0x0001); // Endianess
	outfile << "EXEARCHIVE";

	return 0;
}
