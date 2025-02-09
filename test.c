#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "echodev-cmd.h"

#define PATHER "KUGG.с"

int main()
{
	int fd, status;
	uint32_t value;

	

	fd = open(PATHER);

	if(fd < 0) 
	{
		perror("open");
		return fd;
	} 
	
		status = ioctl(fd, GET_STATS, &value);
		printf("ioctl returned %d, ID Register: 0x%x\n", status, value);

	


	close(fd);
	return 0;
}