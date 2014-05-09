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

static char* filename;
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

	gzFile file = gzopen(filename, "r");

	if (file==NULL) return -EBADF;

	fi->fh = (uint64_t)file;

	return 0;
}

static int vdifs_release(const char * path, struct fuse_file_info *fi)
{
	if(strcmp(path, dd_path) != 0)
		return -ENOENT;

	gzFile file = (gzFile)fi->fh;

	gzclose(file);

	fi->fh=(uint64_t)NULL;

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
	.read	= vdifs_read,
	.release = vdifs_release
};

void usage( char *argv[])
{
	printf("Usage:\n");
	printf("%s <filename> <directory>\n", argv[0]);
}

//this function leaks the return callee must clean it up
char *get_full_path(char *filename)
{
	char *linkname;
	char link[32];
	struct stat sb;
	FILE* f;
	ssize_t r;

	//open file to create /proc/self/fd/###
	f = fopen(filename, "r");

	//create a string "/proc/self/fd/###"
	snprintf(link, 32, "/proc/self/fd/%i", fileno(f));

	//find out how long the filename that /proc/self/fd/### points to is
	if (lstat(link, &sb) == -1) 
	{
		perror("lstat");
		exit(EXIT_FAILURE);
    	}
	
	//create space for the filename
	linkname = malloc(sb.st_size + 1);

	//make sure the space was created
	if (linkname == NULL) {
		fprintf(stderr, "insufficient memory\n");
		exit(EXIT_FAILURE);
	}

	//read the filename
	r = readlink(link, linkname, sb.st_size + 1);

	//did we really read the file name?
	if (r < 0) {
	        perror("lstat");
	        exit(EXIT_FAILURE);
    	}

	//make sure the file didn't change under us.... why should this ever happen we own the fd?
	if (r > sb.st_size) {
		fprintf(stderr, "symlink increased in size "
		"between lstat() and readlink()\n");
		exit(EXIT_FAILURE);
	}

	//make sure to null terminate
	linkname[sb.st_size] = '\0';

	//close the fd since we have the file name already
	fclose(f);

	return linkname;
}

int main(int argc, char *argv[])
{
	if (argc!=3) 
	{
		usage(argv);
		return 1;
	}

	// dd_path = "/" + argv[1].substring(-3)
	//go in reverse to find the last instance of /
	{
		int len = strlen(argv[1]);

		int start=0;
		int end=len;
		int i;
		for(i=0; i<len; i++)
		{
			if (argv[1][i]=='/') start=i+1;
			else if (argv[1][i]=='.') end=i;
		}
		
		int len2=end-start+2;
		dd_path = malloc(len2);
		bzero(dd_path, len2);
		
		snprintf(dd_path, len2, "/%s", argv[1]+start);
	}
	//printf("dd_path= %s\n", dd_path);

	gzFile vdi_file;
	vdi_file = gzopen(argv[1], "r");

	if (vdi_file==NULL)
	{
		printf("Error(%i) opening: %s for gzip\n", errno, argv[1]);
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

	//printf("size = %lu \n", file_size);

	filename = get_full_path(argv[1]);
	//printf("filename = %s \n", filename);

	return fuse_main(argc-1, argv+1, &vdifs_op);
}






