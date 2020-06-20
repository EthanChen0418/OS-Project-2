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
#define MMAP_SIZE (4096 * 64)
#define BUF_SIZE 512
#define SLAVE_CREATE_SOCKET 0x12345677
#define SLAVE_MMAP 0x12345678
#define SLAVE_DISCONNECT 0x12345679
int main (int argc, char* argv[])
{
	char buf[BUF_SIZE];
	int file_num;
	char method[20];
	char ip[20];

	int dev_fd, file_fd;// the fd for the device and the fd for the input file
	char file_name[50];
	struct timeval start;
	struct timeval end;
	double trans_time; //calulate the time between the device is opened and it is closed
	char *kernel_address, *file_address;

	file_num = atoi(argv[1]);
	strcpy(method, argv[2+file_num]);
	strcpy(ip, argv[3+file_num]);

	for (int i=0; i<file_num; ++i)
	{

		gettimeofday(&start ,NULL);
		size_t ret = 0, file_size = 0, data_size = 0;
		strcpy(file_name, argv[2+i]);
	
		if( (dev_fd = open("/dev/slave_device", O_RDWR)) < 0)//should be O_RDWR for PROT_WRITE when mmap()
		{
			perror("failed to open /dev/slave_device\n");
			return 1;
		}
		if( (file_fd = open (file_name, O_RDWR | O_CREAT | O_TRUNC)) < 0)
		{
			perror("failed to open input file\n");
			return 1;
		}

		if(ioctl(dev_fd, SLAVE_CREATE_SOCKET, ip) == -1)	//SLAVE_CREATE_SOCKET : connect to master in the device
		{
			perror("ioclt create slave socket error\n");
			return 1;
		}	    
	
	    	write(1, "ioctl success\n", 14);

		switch(method[0])
		{
			case 'f'://fcntl : read()/write()
				do
				{
					ret = read(dev_fd, buf, sizeof(buf)); // read from the the device
					write(file_fd, buf, ret); //write to the input file
					file_size += ret;
				}while(ret > 0);
				break;
			case 'm'://mmap : mmap(), memcpy()
				//printf("enter!\n");
				do{
					//sleep(1);
					//printf("pass!\n");
					//printf("fffirst_ret = %ld\n", ret);
					//printf("pass!\n");
					ret = ioctl(dev_fd, SLAVE_MMAP);
					printf("first_ret = %ld", ret);
					if(ret > 0){
						int deg;
						if((deg = posix_fallocate(file_fd, file_size, ret)) != 0){
							printf("There is something wrong during \" posix_fallocate \" of slave.c !\n");
							//fprintf(stderr, "deg = %d data_size = %ull ret = %ld", deg, data_size, ret);
							exit(-1);
						}
						printf("file_fd: %d file_size: %ld\n", file_fd, file_size);
						if((file_address = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, file_fd, file_size)) == MAP_FAILED){
							perror("slave file mmap: ");
							exit(-1);
						}
						if((kernel_address = mmap(NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, dev_fd, 0)) == MAP_FAILED){
							perror("slave device mmap: ");
							exit(-1);
						}
						file_size += ret;
						printf("data_size: %ld, ret: %ld\n", file_size, ret);
						memcpy(file_address, kernel_address, ret);
						munmap(file_address, MMAP_SIZE);
						munmap(kernel_address, MMAP_SIZE);		

					}
					else if(ret < 0){
						perror(" There is something wrong during \" mmap \" of slave.c !\n");
						exit(-1);
					}
				}while(ret > 0);
				break;
			default:
				fprintf(stderr, "please use an available method !");
				exit(-1);
		}
	
	
	
		if(ioctl(dev_fd, SLAVE_DISCONNECT) == -1)// end receiving data, close the connection
		{
			perror("ioclt client exits error\n");
			return 1;
		}
		gettimeofday(&end, NULL);
		trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
		printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, file_size / 8);


		close(file_fd);
		close(dev_fd);
	}
	return 0;
}

