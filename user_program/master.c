#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define PAGE_SIZE 4096
#define BUF_SIZE 512
#define MMAP_SIZE (PAGE_SIZE * 64)
#define IOCTL_CREATESOCK 0x12345677
#define IOCTL_MMAP 0x12345678
#define IOCTL_EXIT 0x12345679
#define IOCTL_DEFAULT 0x12349487
size_t get_filesize(const char* filename);//get the size of the input file


int main (int argc, char* argv[])
{
	char buf[BUF_SIZE];
	int file_num = atoi(argv[1]);;
	char method[20];
	strcpy(method, argv[2 + file_num]);

	for (int i=0; i<file_num; ++i)
	{
		int dev_fd, file_fd;// the fd for the device and the fd for the input file
		size_t ret, file_size, offset = 0, tmp;
		char file_name[50];
		char *kernel_address = NULL, *file_address = NULL;
		double trans_time; //calulate the time between the device is opened and it is closed
	
		strcpy(file_name, argv[2+i]);

		struct timeval start, end;
	
		if( (dev_fd = open("/dev/master_device", O_RDWR)) < 0)
		{
			perror("failed to open /dev/master_device\n");
			return 1;
		}
		
		if( (file_fd = open (file_name, O_RDWR)) < 0 )
		{
			perror("failed to open input file\n");
			return 1;
		}
	
		if( (file_size = get_filesize(file_name)) < 0)
		{
			perror("failed to get filesize\n");
			return 1;
		}
	
	
		if(ioctl(dev_fd, IOCTL_CREATESOCK) == -1) //0x12345677 : create socket and accept the connection from the slave
		{
			perror("ioclt server create socket error\n");
			return 1;
		}
	
		gettimeofday(&start ,NULL);
	
		switch(method[0])
		{
			case 'f': //fcntl : read()/write()
				do
				{
					ret = read(file_fd, buf, sizeof(buf)); // read from the input file
					write(dev_fd, buf, ret);//write to the the device
				}while(ret > 0);
				break;

			case 'm':
				while(offset < file_size){
					size_t file_left = file_size - offset;
					size_t mmap_size = file_left < MMAP_SIZE? file_left: MMAP_SIZE;

					if((file_address = mmap(NULL, mmap_size, PROT_READ, MAP_SHARED, file_fd, offset)) == MAP_FAILED){
						perror("master file mmap: ");
						return 1;
					}

					if((kernel_address = mmap(NULL, mmap_size, PROT_WRITE, MAP_SHARED, dev_fd, offset)) == MAP_FAILED){
						perror("master device mmap: ");
						return 1;
					}

					memcpy(kernel_address, file_address, mmap_size);

					if(ioctl(dev_fd, IOCTL_MMAP, mmap_size) == -1){
						perror("master ioctl mmap: ");
						return 1;
					}

					offset += mmap_size;
				}

				if(ioctl(dev_fd, IOCTL_DEFAULT, kernel_address) == -1){
					perror("master ioctl default: ");
					return 1;
				}

				break;
		}
	
		if(ioctl(dev_fd, 0x12345679) == -1) // end sending data, close the connection
		{
			perror("ioclt server exits error\n");
			return 1;
		}

		gettimeofday(&end, NULL);
		trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.001;
		printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, file_size);
	
		close(file_fd);
		close(dev_fd);
	}

	
	return 0;
}

size_t get_filesize(const char* filename)
{
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}
