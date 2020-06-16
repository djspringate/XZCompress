// XZCompress.cpp
// 16/06/20
// David Springate
// david@liquidbridge.net

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void printHeader()
{
	printf("XZCompress\n");
}

void printUsage()
{
	printf("Usage:\n");
	printf("xzcompress *input_filename* *chunk_size* *output_compressed_data_filename* *output_metadata_json_filename*\n");
}

long int getFileSize(FILE* fh)
{
	int returnVal = fseek(fh, 0, SEEK_END);
	if (returnVal != 0)
	{
		printf("An error occurred during seek.");
		return 0;
	}

	return ftell(fh);
}

void seekToBeginning(FILE* fh)
{
	int returnVal = fseek(fh, 0, SEEK_SET);
	if (returnVal != 0)
	{
		printf("An error occurred during seek.");
	}
}

void seekForwardBy(FILE* fh, long int offset)
{
	int returnVal = fseek(fh, offset, SEEK_CUR);
	if (returnVal != 0)
	{
		printf("An error occurred during seek.");
	}

}

FILE* openFileForReading(const char* inputFilePath)
{
	FILE* inputFileHandle = nullptr;
	{
		errno_t fopenRetVal = fopen_s(&inputFileHandle, inputFilePath, "r+b");
		if (fopenRetVal != 0)
		{
			printf("Error opening %s for reading.", inputFilePath);
		}
	}
	return inputFileHandle;
}

FILE* openFileForWriting(const char* outputFilePath)
{
	FILE* outputFileHandle = nullptr;
	errno_t fopenRetVal = fopen_s(&outputFileHandle, outputFilePath, "w+b");
	if (fopenRetVal != 0)
	{
		printf("Error opening %s for writing.", outputFilePath);
	}
	return outputFileHandle;
}

int main(int argc, char** argv)
{
	printHeader();

	if (argc < 5)
	{
		printUsage();
		return 0;
	}

	char** command_line_arguments = (char**)malloc(sizeof(char*) * argc);

	// Process command line arguments: (I just find this nicer)
	for (int i = 0; i < argc; ++i)
	{
		command_line_arguments[i] = *(argv + i);
	}

	// Now, ignoring the application executable path, what are our arguments:
	char* inputFilePath = command_line_arguments[1];

	char* requestedChunkSizeString = command_line_arguments[2];
	unsigned int requestedChunkSize = atoi(requestedChunkSizeString);

	char* outputFilePath = command_line_arguments[3];
	char* outputMetaDataFilePath = command_line_arguments[4];

	printf("Opening %s\n", inputFilePath);
	printf("Using chunk size of %d\n", requestedChunkSize);
	printf("Writing to %s\n", outputFilePath);
	printf("Writing metadata to %s\n", outputMetaDataFilePath);

	// Open input file:
	FILE* inputFileHandle = openFileForReading(inputFilePath);
	if (!inputFileHandle)
	{
		return 1;
	}

	// Open output file:
	FILE* outputFileHandle = openFileForWriting(outputFilePath);
	if (!outputFileHandle)
	{
		return 1;
	}

	// How big is our input file?
	long int inputFileSize = getFileSize(inputFileHandle);

	// Assuming a chunk size of N, how many chunks is this file going to be?
	unsigned int chunkSize = requestedChunkSize;
	unsigned int numberOfWholeChunks = inputFileSize / chunkSize;
	unsigned int lastChunkSize = inputFileSize % chunkSize;
	unsigned int numberOfChunks = numberOfWholeChunks + (lastChunkSize > 0);

	// So, we now know our chunk sizes, let's compress and write out to disk!
	seekToBeginning(inputFileHandle);
	seekToBeginning(outputFileHandle);

	for (unsigned int i = 0; i < numberOfChunks; ++i)
	{
		const unsigned int currentChunkSize = (i < numberOfWholeChunks) ? chunkSize : lastChunkSize;

		unsigned char* chunkData = (unsigned char*)malloc(currentChunkSize);
		if (!chunkData)
		{
			printf("Failed to allocate memory for read.\n");
			return 1;
		}
		memset(chunkData, 0, currentChunkSize);

		//printf("Current file position: %d\n", ftell(inputFileHandle));

		size_t readSize = fread(chunkData, currentChunkSize, 1, inputFileHandle);
		if (ferror(inputFileHandle))
		{
			printf("A read error occurred.\n");
			return 1;
		}
		if (feof(inputFileHandle))
		{
			printf("Unexpectedly reached end of file.\n");
			return 1;
		}

		// Compress the chunk:

		// Write out some meta data to describe this chunk to JSON:

		// Write out to disk:
		size_t elementsWritten = fwrite(chunkData, currentChunkSize, 1, outputFileHandle);
		if (elementsWritten != 1)
		{
			printf("A write error occurred.\n");
			return 1;
		}

		free(chunkData);
	}

	fclose(outputFileHandle);
	fclose(inputFileHandle);

	return 0;
}
