#include <stdio.h>
#include <stdlib.h>
#include "logger.h"
typedef void (*test_process_frame_t)(const void *buffer, int size);

FILE *wfp;

void write_frame(const void *buffer, int size)
{
	/* write to file */
    // LOG_INFO("%s", buffer);

	fwrite(buffer, size, 1, wfp);
	fflush(wfp);
}

int main (int argc, char* argv[])
{
    // printf("SRT DEMO APP\n");
    FILE *fp = NULL;
    fp = fopen("test.txt", "rb");
    wfp = fopen ("result.txt", "ab");
    char *buffer;
    buffer = (char*)calloc(10000, 1);
    int size;
    while(!feof(fp))
    {
        size = fread(buffer, sizeof(char), 1000, fp);
        int N = size/2;
		int j = size%2;
		LOG_INFO("N: %d, j: %d", N, j);
		// write_frame(buffer, size);
		for(int i = 0; i < N; ++i)
		{
			write_frame((buffer + 2*i), 2);
		}
		if(j != 0)
		{
			write_frame(buffer + 2*N, j);
		}
    }
    free(buffer);
    fclose(fp);
    fclose(wfp);
    return 0;
}