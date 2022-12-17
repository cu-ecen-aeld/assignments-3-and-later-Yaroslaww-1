#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char* argv[]) {
	if (argc != 3) {
		openlog(NULL, 0, LOG_USER);
		syslog(LOG_ERR, "Wrong number of arguments: %d\n", argc - 1);
		exit(EXIT_FAILURE);
	}

	const char* writeFile = argv[1];
	const char* writeContent = argv[2];

	openlog(NULL, 0, LOG_USER);

	FILE* file = fopen(writeFile, "w");
	if (file == NULL) {
		printf("File %s can`t be opened\n", writeFile);
		perror("perror returned");
		syslog(LOG_ERR, "File %s can`t be opened\n", writeFile);
		exit(EXIT_FAILURE);
	} else {
		syslog(LOG_DEBUG, "Writing %s to %s\n", writeFile, writeContent);
		fprintf(file, "%s\n", writeContent);
		fclose(file);
	}

	exit(EXIT_SUCCESS);
}
