/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2005  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

//#define FUSE_USE_VERSION 21
#define FUSE_USE_VERSION 25

#include <fuse.h>
#include <zlib.h>

static char *file_name;
static char *dd_path; // = "/dd";
static unsigned long file_size;

static int vdifs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(path, "/") == 0) 
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if(strcmp(path, dd_path) == 0) 
	{
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = file_size; //header_desc.disksize;
	}
	else
	res = -ENOENT;

	return res;
}

static int vdifs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, dd_path + 1, NULL, 0);

	return 0;
}

static int vdifs_open(const char *path, struct fuse_file_info *fi)
{
	if(strcmp(path, dd_path) != 0) return -ENOENT;

	if((fi->flags & 3) != O_RDONLY) return -EACCES;

	gzFile file = gzopen(file_name, "r");

	if (file==NULL) return -EBADF;

	fi->fh = (uint64_t)file;

	return 0;
}

static int vdifs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
	if(strcmp(path, dd_path) != 0)
		return -ENOENT;

//    gzseek(vdi_file, offset, SEEK_SET);
	return gzread((gzFile)fi->fh, buf, size);
}

static struct fuse_operations vdifs_op = {
	.getattr	= vdifs_getattr,
	.readdir	= vdifs_readdir,
	.open	= vdifs_open,
	.read	= vdifs_read
};

void usage( char *argv[])
{
	printf("Usage:\n");
	printf("%s <filename> <directory>\n", argv[0]);
}


int main(int argc, char *argv[])
{
	if (argc!=3) 
	{
		usage(argv);
		return 1;
	}

	// dd_path = argv[1].substring(-3)
	//go in reverse to find the last instance of /
	{
		int len = strlen(argv[1]);

		int start;
		for(start=len; start>=0; start--)
		{
			if (argv[1][start]=='/') break;
		}

		if (argv[1][start]=='/') start++;

		len-=start;
	

		dd_path = malloc(len);
		bzero(dd_path, len);
		dd_path[0]='/';
		strncpy(dd_path+1, argv[1]+start, len-3);
	}
	printf("dd_path= %s\n", dd_path);

	// file_name = argv[1]
	int len = strlen(argv[1]);
	file_name=malloc(len+1);
	file_name[len]=0;
	strncpy(file_name, argv[1], len);
	printf("file_name= %s\n", file_name);



	gzFile vdi_file;
	vdi_file = gzopen(argv[1], "r");

	if (vdi_file==NULL)
	{
		printf("Error opening: %s\n", argv[1]);
		return -1;
	}

	file_size=0;
	int read=0;
	unsigned char *buf=(unsigned char *)malloc(4096);
	do{
		read=gzread(vdi_file, buf, 4096);
		file_size+=read;
	}while(gzeof(vdi_file)==0);

	gzclose(vdi_file);

	vdi_file=NULL;

	printf("size = %lu \n", file_size);	

	return fuse_main(argc-1, argv+1, &vdifs_op);
}






