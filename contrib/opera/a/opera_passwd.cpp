// sna@reteam.org  - 6th of April 2005


#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>

#include "openssl/md5.h"
#include "openssl/des.h"

const unsigned char opera_salt[11] =
{
	0x83, 0x7D, 0xFC, 0x0F, 0x8E, 0xB3, 0xE8, 0x69, 0x73, 0xAF, 0xFF
};

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cout << "Usage: unwand <opera wand file>" << std::endl;
		return 1;
	}

	FILE *fdWand = fopen(argv[1], "rb");

	if (!fdWand) {
		perror("Failed to open file");
		return 1;
	}

	fseek(fdWand, 0, SEEK_END);
	unsigned long fileSize = ftell(fdWand);

	unsigned char *wandData = (unsigned char *)malloc(fileSize);

	if(NULL == wandData)
	{
		fclose(fdWand);
		perror("Memory allocation failed");
		return 1;
	}

	rewind(fdWand);
	fread(wandData, fileSize, 1, fdWand);
	fclose(fdWand);

	unsigned long wandOffset = 0;

	//
	// main loop, find and process encrypted blocks
	//

	while(wandOffset < fileSize)
	{
		// find key length field at start of block
		unsigned char *wandKey = (unsigned char *)
			memchr(wandData + wandOffset, DES_KEY_SZ, fileSize - wandOffset);

		if(NULL == wandKey)
		{
			break;
		}

		wandOffset = ++wandKey - wandData;

		// create pointers to length fields
		unsigned char *blockLengthPtr = wandKey - 8;
		unsigned char *dataLengthPtr = wandKey + DES_KEY_SZ;

		if(blockLengthPtr < wandData || dataLengthPtr > wandData + fileSize)
		{
			continue;
		}

		// convert big-endian numbers to native
		unsigned long
			blockLength  = *blockLengthPtr++ << 24;
			blockLength |= *blockLengthPtr++ << 16;
			blockLength |= *blockLengthPtr++ <<  8;
			blockLength |= *blockLengthPtr;

		unsigned long
			dataLength  = *dataLengthPtr++ << 24;
			dataLength |= *dataLengthPtr++ << 16;
			dataLength |= *dataLengthPtr++ <<  8;
			dataLength |= *dataLengthPtr;

		// as discussed in the article
		if(blockLength != dataLength + DES_KEY_SZ + 4 + 4)
		{
			continue;
		}

		// perform basic sanity checks on data length
		if(dataLength > fileSize - (wandOffset + DES_KEY_SZ + 4)
			|| dataLength < 8 || dataLength % 8 != 0)
		{
			continue;
		}

		unsigned char
			hashSignature1[MD5_DIGEST_LENGTH],
			hashSignature2[MD5_DIGEST_LENGTH],
			tmpBuffer[256];

		//
		// hashing of (salt, key), (hash, salt, key)
		//

		memcpy(tmpBuffer, opera_salt, sizeof(opera_salt));
		memcpy(tmpBuffer + sizeof(opera_salt), wandKey, DES_KEY_SZ);

		MD5(tmpBuffer, sizeof(opera_salt) + DES_KEY_SZ, hashSignature1);

		memcpy(tmpBuffer, hashSignature1, sizeof(hashSignature1));
		memcpy(tmpBuffer + sizeof(hashSignature1),
			opera_salt, sizeof(opera_salt));

		memcpy(tmpBuffer + sizeof(hashSignature1) +
			sizeof(opera_salt), wandKey, DES_KEY_SZ);

		MD5(tmpBuffer, sizeof(hashSignature1) +
			sizeof(opera_salt) + DES_KEY_SZ, hashSignature2);

		//
		// schedule keys. key material from hashes
		//

		DES_key_schedule key_schedule1, key_schedule2, key_schedule3;

		DES_set_key_unchecked((const_DES_cblock *)&hashSignature1[0],
			&key_schedule1);

		DES_set_key_unchecked((const_DES_cblock *)&hashSignature1[8],
			&key_schedule2);

		DES_set_key_unchecked((const_DES_cblock *)&hashSignature2[0],
			&key_schedule3);

		DES_cblock iVector;
		memcpy(iVector, &hashSignature2[8], sizeof(DES_cblock));

		unsigned char *cryptoData = wandKey + DES_KEY_SZ + 4;

		//
		// decrypt wand data in place using 3DES-CBC
		//

		DES_ede3_cbc_encrypt(cryptoData, cryptoData, dataLength,
			&key_schedule1, &key_schedule2, &key_schedule3, &iVector, 0);

		if(0x00 == *cryptoData || 0x08 == *cryptoData)
		{
			std::wcout << L"<null>" << std::endl;
		}
		else
		{
			// remove padding (data padded up to next block)
			unsigned char *padding = cryptoData + dataLength - 1;
			memset(padding - (*padding - 1), 0x00, *padding);

            unsigned char * c;
            //for (c = cryptoData; c< padding; c++) printf( "%02x ", *c);
            //printf("\n");
            for (c = cryptoData; c< padding; c++)
               if (*c) printf( "%c", *c);
            printf("\n");

            //std::wcout << (wchar_t *)cryptoData << std::endl;
		}

		wandOffset = wandOffset + DES_KEY_SZ + 4 + dataLength;
	}

	free(wandData);
	return 0;
}
