// XZCompress.cpp
// 16/06/20
// David Springate
// david@liquidbridge.net

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fstream> // For Json parsers :/

// External libraries:
#include <zlib.h>
#include <json/json.h>

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
		printf("An error occurred during seek.\n");
		return 0;
	}

	return ftell(fh);
}

void seekToBeginning(FILE* fh)
{
	int returnVal = fseek(fh, 0, SEEK_SET);
	if (returnVal != 0)
	{
		printf("An error occurred during seek.\n");
	}
}

void seekForwardBy(FILE* fh, long int offset)
{
	int returnVal = fseek(fh, offset, SEEK_CUR);
	if (returnVal != 0)
	{
		printf("An error occurred during seek.\n");
	}

}

FILE* openFileForReadingBinary(const char* inputFilePath)
{
	FILE* inputFileHandle = nullptr;
	{
		errno_t fopenRetVal = fopen_s(&inputFileHandle, inputFilePath, "r+b");
		if (fopenRetVal != 0)
		{
			printf("Error opening %s for reading.\n", inputFilePath);
		}
	}
	return inputFileHandle;
}

FILE* openFileForReadingText(const char* inputFilePath)
{
	FILE* inputFileHandle = nullptr;
	{
		errno_t fopenRetVal = fopen_s(&inputFileHandle, inputFilePath, "r");
		if (fopenRetVal != 0)
		{
			printf("Error opening %s for reading.\n", inputFilePath);
		}
	}
	return inputFileHandle;
}

FILE* openFileForWritingBinary(const char* outputFilePath)
{
	FILE* outputFileHandle = nullptr;
	errno_t fopenRetVal = fopen_s(&outputFileHandle, outputFilePath, "w+b");
	if (fopenRetVal != 0)
	{
		printf("Error opening %s for writing.\n", outputFilePath);
	}
	return outputFileHandle;
}


FILE* openFileForWritingText(const char* outputFilePath)
{
	FILE* outputFileHandle = nullptr;
	errno_t fopenRetVal = fopen_s(&outputFileHandle, outputFilePath, "w");
	if (fopenRetVal != 0)
	{
		printf("Error opening %s for writing.\n", outputFilePath);
	}
	return outputFileHandle;
}

bool deflate_with_strategy(z_stream& myZStream, const unsigned int strategy, unsigned int currentChunkSize, unsigned char* chunkData, unsigned char* compressedDataBuffer)
{
	myZStream.zalloc = Z_NULL;
	myZStream.zfree = Z_NULL;
	myZStream.opaque = Z_NULL;
	myZStream.avail_in = currentChunkSize;
	myZStream.next_in = chunkData;
	myZStream.avail_out = currentChunkSize;
	myZStream.next_out = compressedDataBuffer;
	myZStream.data_type = Z_BINARY;

	int windowBits = 15;
	int deflateInitReturnVal = deflateInit2(&myZStream, Z_BEST_COMPRESSION, Z_DEFLATED, windowBits, 9, strategy);

	int deflateReturnVal = deflate(&myZStream, Z_FINISH);
	if ((deflateReturnVal != Z_STREAM_END) && (deflateReturnVal != Z_OK))
	{
		printf("An error occurred during deflate.\n");
		return false;
	}

	if (myZStream.total_out >= myZStream.total_in)
	{
		printf("Compressed data size is >= input data size.");
		return false;
	}

	return true;
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
	FILE* inputFileHandle = openFileForReadingBinary(inputFilePath);
	if (!inputFileHandle)
	{
		return 1;
	}

	// Open output file:
	FILE* outputFileHandle = openFileForWritingBinary(outputFilePath);
	if (!outputFileHandle)
	{
		return 1;
	}

	Json::Value rootJsonValue;

	// Does the metadata file already exist?
	std::ifstream ifs(outputMetaDataFilePath);
	if (ifs.is_open())
	{
		// If it already exists, populate our JSON data:
		Json::CharReaderBuilder builder;
		std::string errs;
		bool parsingReturnValue = Json::parseFromStream(builder, ifs, &rootJsonValue, &errs);
		if (!parsingReturnValue)
		{
			printf("An existing Json file was found but an error occurred during parsing.\n");
			return 1;
		}
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

	Json::Value newJsonValue;
	//newJsonValue["filename"] = inputFilePath;
	newJsonValue["requested_chunk_size"] = requestedChunkSize;
	newJsonValue["number_of_chunks"] = numberOfChunks;

	// Before we start, let's initialise Zlib:
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
		unsigned char* compressedDataBuffer = (unsigned char*)malloc(currentChunkSize);

		// We're going to run compression 5 times, in order to get the best
		// compression for smallest size.
		size_t smallestCompressedDataSize = currentChunkSize;
		char bestCompressionMethod = 0;

		for (unsigned int i = 0; i < 5; ++i)
		{
			// Success isn't guaranteed and that's okay..
			z_stream myZStream = {};
			deflate_with_strategy(myZStream, i, currentChunkSize, chunkData, compressedDataBuffer);

			if (myZStream.total_out < smallestCompressedDataSize)
			{
				smallestCompressedDataSize = myZStream.total_out;
				bestCompressionMethod = i;
			}
		}

		// Now run it again so we get the actual data:
		{
			z_stream myZStream = {};
			const bool deflateResult = deflate_with_strategy(myZStream, bestCompressionMethod, currentChunkSize, chunkData, compressedDataBuffer);
			if (deflateResult == false)
			{
				return 1;
			}

			if (myZStream.total_out >= myZStream.total_in)
			{
				printf("Compressed data size is >= input data size.");
				return 1;
			}
		}

		z_stream myZStream = {};
		myZStream.zalloc = Z_NULL;
		myZStream.zfree = Z_NULL;
		myZStream.opaque = Z_NULL;
		myZStream.avail_in = currentChunkSize;
		myZStream.next_in = chunkData;
		myZStream.avail_out = currentChunkSize;
		myZStream.next_out = compressedDataBuffer;
		myZStream.data_type = Z_BINARY;

		int windowBits = 15;
		int deflateInitReturnVal = deflateInit2(&myZStream, Z_BEST_COMPRESSION, Z_DEFLATED, windowBits, 9, Z_DEFAULT_STRATEGY);

		int deflateReturnVal = deflate(&myZStream, Z_FINISH);
		if ((deflateReturnVal != Z_STREAM_END) && (deflateReturnVal != Z_OK))
		{
			printf("An error occurred during deflate.\n");
			return 1;
		}

		if (myZStream.total_out >= myZStream.total_in)
		{
			printf("Compressed data size is >= input data size.");
			return 1;
		}

		// Write out some meta data to describe this chunk to JSON:
		printf("Compressed %d chunk to %d bytes.\n", currentChunkSize, myZStream.total_out);
		newJsonValue["chunks"][i]["chunk_size_uncompressed"] = currentChunkSize;
		newJsonValue["chunks"][i]["chunk_size_compressed"] = (unsigned int)myZStream.total_out;
		//newJsonValue["chunks"][i]["chunk_size_saved_difference"] = (unsigned int)(currentChunkSize - myZStream.total_out);
		//newJsonValue["chunks"][i]["percent_saved"] = 100 - (((float)myZStream.total_out / (float)currentChunkSize) * 100);

		// Write out compressed data:
		size_t elementsWritten = fwrite(compressedDataBuffer, myZStream.total_out, 1, outputFileHandle);
		if (elementsWritten != 1)
		{
			printf("A write error occurred.\n");
			return 1;
		}

		// Close Zlib handle:
		int deflateEndReturnVal = deflateEnd(&myZStream);
		if (deflateEndReturnVal != Z_OK)
		{
			printf("An error occurred calling deflateEnd().\n");
			return 1;
		}

		free(compressedDataBuffer);
		free(chunkData);
	}

	// Check for duplicate entries, remove them first:
	if (rootJsonValue.isMember(inputFilePath))
	{
		rootJsonValue.removeMember(inputFilePath);
	}
	rootJsonValue[inputFilePath].append(newJsonValue);

	// Write out Json data:
	Json::StreamWriterBuilder builder;
	builder["commentStyle"] = "None";
	builder["indentation"] = "   ";

	std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
	std::ofstream outputFileStream(outputMetaDataFilePath, std::ofstream::out);
	writer->write(rootJsonValue, &outputFileStream);

	fclose(outputFileHandle);
	fclose(inputFileHandle);

	return 0;
}
	