#include <iostream>
#include <graphics.h>

struct Picture {
	const char *path;
	int size;
} pics[] = {
	{"wah.bmp"               , 1024},	
	{"burning-ship.bmp"      , 256 },	
	{"scavengers-reign-1.bmp", 512 },	
	{"scavengers-reign-2.bmp", 1024},
};

// leaves the flag 00
uint col_div_4(Uint32 c) {
	return (((c & 0xFF0000) >> 2) & 0xFF0000)  // r
	     | (((c &   0xFF00) >> 2) &   0xFF00)  // g
	     | (((c &     0xFF) >> 2) &     0xFF); // b
}

bool get_flag(uint col) {return (col >> 24)>0;}

void set_flag(uint &col, bool flag) {col = (col & 0x00FFFFFF) | ((uint)flag<<24);}

uint fill(uint **buffers, int buffers_len, uint *img_buffer, int level, int px, int py, int index) {
	//std::cout << "fill " << level << " " << index << " " << px << " " << py << std::endl;
	if (level == buffers_len - 1) {
		return buffers[level][index] = img_buffer[py*(1<<(buffers_len-1))+px];
	}

	return buffers[level][index] = 
		col_div_4(fill(buffers, buffers_len, img_buffer, level+1, px*2  , py*2  , index*4  ))+
		col_div_4(fill(buffers, buffers_len, img_buffer, level+1, px*2+1, py*2  , index*4+1))+
		col_div_4(fill(buffers, buffers_len, img_buffer, level+1, px*2  , py*2+1, index*4+2))+
		col_div_4(fill(buffers, buffers_len, img_buffer, level+1, px*2+1, py*2+1, index*4+3));
}

void find(uint **buffers, int buffers_len, int px, int py, int &level, int &i, int &j, int &index) {
	level = i = j = index = 0;
	while (!(level+1 == buffers_len || get_flag(buffers[level][index]))) {
		int mask = 1 << (buffers_len - 2 - level);
		
		bool halfx = (px & mask)>0; i = i*2 + halfx;
		bool halfy = (py & mask)>0; j = j*2 + halfy;
		index = index * 4 + halfx + 2 * halfy;
		level++;
		//std::cout << "level=" <<  level << " index=" << index << " i=" << i << " j=" << j << " mask=" << mask  << '\n';
	}
}

void paint(uint **buffers, int buffers_len, int level, int i, int j, int index) {
	if (level+1 == buffers_len || get_flag(buffers[level][index])) {
		uint col = buffers[level][index] | 0xFF000000;
		if (level+1 == buffers_len) {
			putpixel(i, j, COLOR32(col));
		} else {
			//std::cout << "fill " << level << " " << index << " " << i << " " << j << " " << std::hex << col  << std::dec << std::endl;
			int diff = buffers_len - 1 - level;
			int minx = i << diff, miny = j << diff, maxx = i, maxy = j;
			for (int k=0;k<diff;k++) maxx=maxx*2+1,maxy=maxy*2+1;
			for (int i=minx;i<=maxx;i++) 
				for(int j=miny; j<=maxy; j++)
					putpixel(i, j, COLOR32(col));
		}
		return;
	}

	paint(buffers, buffers_len, level+1, i*2  , j*2  , index*4  );
	paint(buffers, buffers_len, level+1, i*2+1, j*2  , index*4+1);
	paint(buffers, buffers_len, level+1, i*2  , j*2+1, index*4+2);
	paint(buffers, buffers_len, level+1, i*2+1, j*2+1, index*4+3);
}

void recursive_set_flag(uint **buffers, int buffers_len, int level, int index, bool value) {
	if (level+1 == buffers_len) return;
	set_flag(buffers[level][index],value);

	recursive_set_flag(buffers, buffers_len, level+1, index*4  , value);
	recursive_set_flag(buffers, buffers_len, level+1, index*4+1, value);
	recursive_set_flag(buffers, buffers_len, level+1, index*4+2, value);
	recursive_set_flag(buffers, buffers_len, level+1, index*4+3, value);
}

int main()
{
	int pic_index = 0;

	std::cout << "use number keys to switch images;\n"
	<< "use left click, to subdivide nodes and right click to merge nodes;\n"
	<< "the scroll wheel also be used and scroll click will subdivide nodes recursively;\n"
	<< "\'f\' will merge all nodes and \'c\' will subdivide all nodes;\n"
	<< "---------------------\n"; 

	while (pic_index != -1) {
		// init window with picture
		int img_size = pics[pic_index].size;
		char *path = (char*)pics[pic_index].path;
		initwindow(img_size, img_size);
		setwintitle(getcurrentwindow(), path);
		sdlbgifast();	
		sdlbgiauto();
		
		// allocating memory for quad tree
		// for levels [0, levels-2] the most significant bits indicate a flag
		int buffers_len = log2(img_size) + 1;
		uint **buffers = new uint*[buffers_len];
		for (int i=0,s=1;i<buffers_len;i++,s*=2) {
			buffers[i] = new uint[s*s];
		}

		// read image
		readimagefile(path, 0, 0, 0, 0);

		// populate tree with values		
		uint *img_buffer = new uint[img_size * img_size];
		getbuffer(img_buffer);
		uint avg_col = fill(buffers, buffers_len, img_buffer, 0, 0, 0 ,0);
		std::cout << "the average color of the image is " << std::hex << avg_col << std::dec << '\n';	

		// process events
		while (int code = getevent()) {
			if ('1'<= code && code <='4') {
				pic_index = code - '1';
				std::cout << "switching to " << pics[pic_index].path  << '\n';
				break;
			}
			else if (code == 'q' || code == SDL_QUIT) {
				std::cout << "closing...\n";
				pic_index = -1;
				break;
			}
			else if (code == 1 || code == 4) {
				int px = mousex(), py = mousey(), level, i, j, index;
				find(buffers, buffers_len, px, py, level, i, j, index);
				if (level != buffers_len - 1) {
					set_flag(buffers[level][index], false);
					paint(buffers, buffers_len, level, i, j, index);
				}
				
				std::cout << "subdivide level=" << level << " px=" << px << " py=" << py << " i=" << i << " j=" << j << " index=" << index <<"\n";
			}
			else if (code == 2) {
				int px = mousex(), py = mousey(), level, i, j, index;
				find(buffers, buffers_len, px, py, level, i, j, index);
				recursive_set_flag(buffers, buffers_len, level, index, false);
				paint(buffers, buffers_len, level, i, j, index);
				std::cout << "subdivide recursively level=" << level << " px=" << px << " py=" << py << " i=" << i << " j=" << j << " index=" << index <<"\n";
			}
			else if (code == 3 || code == 5) {
				int px = mousex(), py = mousey(), level, i, j, index;
				find(buffers, buffers_len, px, py, level, i, j, index);

				std::cout << "merge level=" << level << " px=" << px << " py=" << py << " i=" << i << " j=" << j << " index=" << index <<"\n";
				std::cout << std::hex << buffers[buffers_len-1][index] << std::dec << '\n';
				if (level > 0) {
					set_flag(buffers[level-1][index/4], true);
					paint(buffers, buffers_len, level-1, i/2, j/2, index/4);
				}
			}
			else if (code == 'f') {
				for (int i=0,s=1;i<buffers_len-1;i++,s*=2)
					for (int j=0;j<s*s;j++)
						set_flag(buffers[i][j],true);
				paint(buffers, buffers_len, 0, 0, 0, 0);
			}
			else if (code == 'c') {
				for (int i=0,s=1;i<buffers_len-1;i++,s*=2)
					for (int j=0;j<s*s;j++)
						set_flag(buffers[i][j],false);
				paint(buffers, buffers_len, 0, 0, 0, 0);
			}
			else std::cout << "unknown event code:" << code << '\n';

			refresh();
		}

		// free window and memory
		for (int i=0;i<buffers_len;i++)
			delete buffers[i];;
		delete buffers;
		delete img_buffer;
		closewindow(getcurrentwindow());
	}

	return 0;
}
